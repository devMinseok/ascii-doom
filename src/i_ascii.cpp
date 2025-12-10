#include "config.h"
#ifdef __EMSCRIPTEN__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <emscripten.h>

#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

#include "i_ascii.h"
#include "i_video.h"

// ===== 설정/상수 =====
static constexpr char  ASCII_CHARS[]   = " .:-=+*#%@";
static constexpr int   ASCII_CHARS_LEN = sizeof(ASCII_CHARS) - 1;
static constexpr float GAMMA_VALUE     = 0.35f;  // 더 밝게
// ====================

#define CELL_BUFFER_SIZE (ASCII_WIDTH * ASCII_HEIGHT)
static AsciiCell cell_buffer[CELL_BUFFER_SIZE];

// 플래그/통계
static bool ascii_initialized = false;
static bool use_simd = true;

// 벤치마크 모드
static bool benchmark_mode = false;

// 벤치마크 통계 구조체
struct BenchmarkStats {
    uint32_t frame_count;
    uint32_t warmup_count;  // 워밍업 프레임 수
    double total_time_ms;
    double min_time_ms;
    double max_time_ms;
    double avg_time_ms;
};

static constexpr uint32_t BENCHMARK_WARMUP_FRAMES = 3;  // 워밍업 3프레임

static BenchmarkStats stats_simd_on = {0, 0, 0.0, 1e9, 0.0, 0.0};
static BenchmarkStats stats_simd_off = {0, 0, 0.0, 1e9, 0.0, 0.0};

// LUT
static uint8_t gamma_table[256];  // 채널별 감마 보정
static uint8_t idxLUT[256];       // 밝기(0..255) → 문자 인덱스
static bool lut_initialized = false;

// (선택) 엔진 FPS/지연 추적
static uint32_t g_ascii_frame_id = 0;
static double   g_ascii_last_ms  = 0.0;

// ===== 적분영상/경계 버퍼 =====
static uint32_t *I_R = nullptr, *I_G = nullptr, *I_B = nullptr; // (src_w+1)*(src_h+1)
static int I_W = 0, I_H = 0; // integral dims

static int *X0 = nullptr, *X1 = nullptr, *Y0 = nullptr, *Y1 = nullptr;
static int *COUNT_X = nullptr, *COUNT_Y = nullptr;
static uint32_t *INV_COUNT = nullptr;  // 역수 LUT: (1<<16) / cnt
static int B_W = 0, B_H = 0; // ascii dims

// 임시 RGB 버퍼 (SIMD용 연속 메모리)
static uint16_t *temp_r = nullptr, *temp_g = nullptr, *temp_b = nullptr;
static int temp_size = 0;
// ==============================

extern "C" {

// ---------- LUT 초기화 ----------
static void init_luts_once(void) {
    if (lut_initialized) return;
    // 감마 LUT
    for (int i = 0; i < 256; ++i) {
        float normalized = i / 255.0f;
        float corrected  = std::pow(normalized, GAMMA_VALUE);
        float v = corrected * 255.0f;
        if (v < 0.0f) v = 0.0f; else if (v > 255.0f) v = 255.0f;
        gamma_table[i] = (uint8_t)(v + 0.5f);
    }
    // 밝기 → 문자 인덱스 LUT ( /255 제거: >>8 라운딩)
    for (int b = 0; b < 256; ++b) {
        int idx = (b * (ASCII_CHARS_LEN - 1) + 127) >> 8;
        if (idx < 0) idx = 0;
        else if (idx >= ASCII_CHARS_LEN) idx = ASCII_CHARS_LEN - 1;
        idxLUT[b] = (uint8_t)idx;
    }
    lut_initialized = true;
}

// ---------- API ----------
void I_InitASCII(void) {
    if (ascii_initialized) return;
    init_luts_once();
    std::memset(cell_buffer, 0, sizeof(cell_buffer));
    ascii_initialized = true;
}

void I_ShutdownASCII(void) {
    if (!ascii_initialized) return;
    ascii_initialized = false;

    free(I_R); free(I_G); free(I_B); I_R=I_G=I_B=nullptr; I_W=I_H=0;
    free(X0); free(X1); free(Y0); free(Y1);
    free(COUNT_X); free(COUNT_Y); free(INV_COUNT);
    X0=X1=Y0=Y1=COUNT_X=COUNT_Y=nullptr; INV_COUNT=nullptr; B_W=B_H=0;
    free(temp_r); free(temp_g); free(temp_b);
    temp_r=temp_g=temp_b=nullptr; temp_size=0;
}

const void* I_GetASCIIBuffer(void) {
    return cell_buffer;
}

// ---------- 경계/카운트 준비 ----------
static void ensure_bounds(int src_w, int src_h, int ascii_w, int ascii_h) {
    if (B_W == ascii_w && B_H == ascii_h && X0 && X1 && Y0 && Y1) return;

    free(X0); free(X1); free(Y0); free(Y1); free(COUNT_X); free(COUNT_Y); free(INV_COUNT);
    X0=(int*)malloc(sizeof(int)*ascii_w);
    X1=(int*)malloc(sizeof(int)*ascii_w);
    Y0=(int*)malloc(sizeof(int)*ascii_h);
    Y1=(int*)malloc(sizeof(int)*ascii_h);
    COUNT_X=(int*)malloc(sizeof(int)*ascii_w);
    COUNT_Y=(int*)malloc(sizeof(int)*ascii_h);
    INV_COUNT=(uint32_t*)malloc(sizeof(uint32_t)*ascii_w*ascii_h);

    for (int x = 0; x < ascii_w; ++x) {
        int x0 = (int)((int64_t)x     * src_w / ascii_w);
        int x1 = (int)((int64_t)(x+1) * src_w / ascii_w);
        if (x1 <= x0) x1 = x0 + 1;
        if (x0 < 0) x0 = 0;
        if (x1 > src_w) x1 = src_w;
        X0[x]=x0; X1[x]=x1; COUNT_X[x]=x1-x0;
    }
    for (int y = 0; y < ascii_h; ++y) {
        int y0 = (int)((int64_t)y     * src_h / ascii_h);
        int y1 = (int)((int64_t)(y+1) * src_h / ascii_h);
        if (y1 <= y0) y1 = y0 + 1;
        if (y0 < 0) y0 = 0;
        if (y1 > src_h) y1 = src_h;
        Y0[y]=y0; Y1[y]=y1; COUNT_Y[y]=y1-y0;
    }

    // 역수 LUT 계산: (1<<16) / cnt → 곱셈+시프트로 나눗셈 대체
    for (int y = 0; y < ascii_h; ++y) {
        for (int x = 0; x < ascii_w; ++x) {
            int cnt = COUNT_Y[y] * COUNT_X[x];
            INV_COUNT[y*ascii_w + x] = (cnt > 0) ? ((1u << 16) / cnt) : 0;
        }
    }

    B_W = ascii_w; B_H = ascii_h;
}

// ---------- 적분영상 준비 ----------
static void ensure_integral_capacity(int src_w, int src_h) {
    const int W = src_w + 1;
    const int H = src_h + 1;
    if (I_W == W && I_H == H && I_R && I_G && I_B) return;

    free(I_R); free(I_G); free(I_B);
    I_R = (uint32_t*)malloc(sizeof(uint32_t)*W*H);
    I_G = (uint32_t*)malloc(sizeof(uint32_t)*W*H);
    I_B = (uint32_t*)malloc(sizeof(uint32_t)*W*H);
    I_W = W; I_H = H;
}

static inline int IIX(int x, int y, int W) { return y*W + x; }

// RGBA32 → R/G/B 적분영상
static void build_integral_images(const uint32_t *rgba, int w, int h) {
    ensure_integral_capacity(w, h);
    const int W = I_W; // w+1

    // 첫 행/열 0 클리어
    std::memset(I_R, 0, sizeof(uint32_t)*W);
    std::memset(I_G, 0, sizeof(uint32_t)*W);
    std::memset(I_B, 0, sizeof(uint32_t)*W);
    for (int y = 1; y <= h; ++y) {
        I_R[IIX(0,y,W)] = 0;
        I_G[IIX(0,y,W)] = 0;
        I_B[IIX(0,y,W)] = 0;
    }

    // 행+열 누적 통합 (이전 행 결과를 바로 더함)
    for (int y = 0; y < h; ++y) {
        uint32_t rsum = 0, gsum = 0, bsum = 0;
        const uint32_t* srcRow = rgba + y*w;
        uint32_t* dstR = I_R + IIX(1, y+1, W);
        uint32_t* dstG = I_G + IIX(1, y+1, W);
        uint32_t* dstB = I_B + IIX(1, y+1, W);
        uint32_t* prevR = I_R + IIX(1, y, W);
        uint32_t* prevG = I_G + IIX(1, y, W);
        uint32_t* prevB = I_B + IIX(1, y, W);

        for (int x = 0; x < w; ++x) {
            uint32_t s = srcRow[x];
            rsum += (s>>16)&0xFF;
            gsum += (s>> 8)&0xFF;
            bsum += (s    )&0xFF;
            dstR[x] = rsum + prevR[x];
            dstG[x] = gsum + prevG[x];
            dstB[x] = bsum + prevB[x];
        }
    }
}

static void ensure_temp_buffer(int size) {
    if (temp_size >= size) return;
    free(temp_r); free(temp_g); free(temp_b);
    // 16바이트 정렬 (SIMD)
    temp_r = (uint16_t*)aligned_alloc(16, sizeof(uint16_t) * size);
    temp_g = (uint16_t*)aligned_alloc(16, sizeof(uint16_t) * size);
    temp_b = (uint16_t*)aligned_alloc(16, sizeof(uint16_t) * size);
    temp_size = size;
}

static inline uint8_t clamp_to_byte(int v) {
    return static_cast<uint8_t>(std::min(255, std::max(0, v)));
}

// ---------- 메인 변환 ----------
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer,
                          int src_width, int src_height,
                          void *output_buffer,
                          int ascii_width, int ascii_height)
{
    AsciiCell* out = (AsciiCell*)output_buffer;
    if (!rgba_buffer || src_width<=0 || src_height<=0 ||
        ascii_width<=0 || ascii_height<=0) return;

    // 초기화 작업 (벤치마크에서 제외)
    init_luts_once();
    ensure_bounds(src_width, src_height, ascii_width, ascii_height);
    
    // 벤치마크 모드일 때 시간 측정 시작 (실제 변환 작업만 측정)
    double start_time = 0.0;
    if (benchmark_mode) {
        start_time = emscripten_get_now();
    }
    
    build_integral_images(rgba_buffer, src_width, src_height);

    const int total_cells = ascii_width * ascii_height;
    ensure_temp_buffer(total_cells);

    // 패스1: 적분영상에서 RGB 평균 추출 → 임시 버퍼 (나눗셈 → 곱셈+시프트)
    const int W = I_W;
    for (int y = 0; y < ascii_height; ++y) {
        const int y0 = Y0[y], y1 = Y1[y];
        const int row_offset = y * ascii_width;

        for (int x = 0; x < ascii_width; ++x) {
            const int x0 = X0[x], x1 = X1[x];
            const uint32_t inv = INV_COUNT[row_offset + x];

            const uint32_t rsum = I_R[IIX(x1,y1,W)] - I_R[IIX(x0,y1,W)] - I_R[IIX(x1,y0,W)] + I_R[IIX(x0,y0,W)];
            const uint32_t gsum = I_G[IIX(x1,y1,W)] - I_G[IIX(x0,y1,W)] - I_G[IIX(x1,y0,W)] + I_G[IIX(x0,y0,W)];
            const uint32_t bsum = I_B[IIX(x1,y1,W)] - I_B[IIX(x0,y1,W)] - I_B[IIX(x1,y0,W)] + I_B[IIX(x0,y0,W)];

            // 곱셈+시프트로 나눗셈 대체: (sum * inv) >> 16 ≈ sum / cnt
            const int idx = row_offset + x;
            temp_r[idx] = (uint16_t)((rsum * inv) >> 16);
            temp_g[idx] = (uint16_t)((gsum * inv) >> 16);
            temp_b[idx] = (uint16_t)((bsum * inv) >> 16);
        }
    }

    // 패스2: 밝기 계산 + 감마 + 문자 결정 (SIMD)
#if defined(__wasm_simd128__)
    if (use_simd) {
        const v128_t coef_r = wasm_i32x4_splat(299);
        const v128_t coef_g = wasm_i32x4_splat(587);
        const v128_t coef_b = wasm_i32x4_splat(114);
        const v128_t v255 = wasm_i32x4_splat(255);
        const v128_t v0 = wasm_i32x4_splat(0);

        int i = 0;
        for (; i <= total_cells - 4; i += 4) {
            // 4셀 RGB 로드 (uint16 → uint32 확장)
            v128_t r16 = wasm_v128_load64_zero(&temp_r[i]);
            v128_t g16 = wasm_v128_load64_zero(&temp_g[i]);
            v128_t b16 = wasm_v128_load64_zero(&temp_b[i]);

            // uint16 → uint32 직접 확장
            v128_t r = wasm_u32x4_extend_low_u16x8(r16);
            v128_t g = wasm_u32x4_extend_low_u16x8(g16);
            v128_t b = wasm_u32x4_extend_low_u16x8(b16);

            // 밝기 = (r*299 + g*587 + b*114) >> 10
            v128_t lum = wasm_i32x4_add(
                wasm_i32x4_add(wasm_i32x4_mul(r, coef_r), wasm_i32x4_mul(g, coef_g)),
                wasm_i32x4_mul(b, coef_b)
            );
            lum = wasm_i32x4_shr(lum, 10);

            // clamp 0-255
            lum = wasm_i32x4_max(lum, v0);
            lum = wasm_i32x4_min(lum, v255);
            r = wasm_i32x4_min(wasm_i32x4_max(r, v0), v255);
            g = wasm_i32x4_min(wasm_i32x4_max(g, v0), v255);
            b = wasm_i32x4_min(wasm_i32x4_max(b, v0), v255);

            // 레인 추출 후 LUT/감마 적용 (컴파일 타임 상수 필요)
            #define EXTRACT_AND_STORE(lane) do { \
                int lv = wasm_i32x4_extract_lane(lum, lane); \
                int rv = wasm_i32x4_extract_lane(r, lane); \
                int gv = wasm_i32x4_extract_lane(g, lane); \
                int bv = wasm_i32x4_extract_lane(b, lane); \
                AsciiCell& c = out[i + lane]; \
                c.character = ASCII_CHARS[idxLUT[lv]]; \
                c.r = gamma_table[rv]; \
                c.g = gamma_table[gv]; \
                c.b = gamma_table[bv]; \
            } while(0)

            EXTRACT_AND_STORE(0);
            EXTRACT_AND_STORE(1);
            EXTRACT_AND_STORE(2);
            EXTRACT_AND_STORE(3);
            #undef EXTRACT_AND_STORE
        }

        // 나머지 스칼라
        for (; i < total_cells; ++i) {
            const uint8_t rv = clamp_to_byte(temp_r[i]);
            const uint8_t gv = clamp_to_byte(temp_g[i]);
            const uint8_t bv = clamp_to_byte(temp_b[i]);
            const uint8_t lum = clamp_to_byte((rv*299 + gv*587 + bv*114) >> 10);

            out[i].character = ASCII_CHARS[idxLUT[lum]];
            out[i].r = gamma_table[rv];
            out[i].g = gamma_table[gv];
            out[i].b = gamma_table[bv];
        }
    } else
#endif
    {
        // 스칼라 fallback
        for (int i = 0; i < total_cells; ++i) {
            const uint8_t rv = clamp_to_byte(temp_r[i]);
            const uint8_t gv = clamp_to_byte(temp_g[i]);
            const uint8_t bv = clamp_to_byte(temp_b[i]);
            const uint8_t lum = clamp_to_byte((rv*299 + gv*587 + bv*114) >> 10);

            out[i].character = ASCII_CHARS[idxLUT[lum]];
            out[i].r = gamma_table[rv];
            out[i].g = gamma_table[gv];
            out[i].b = gamma_table[bv];
        }
    }

    // 벤치마크 모드일 때 시간 측정 및 통계 업데이트
    if (benchmark_mode) {
        double elapsed_ms = emscripten_get_now() - start_time;
        BenchmarkStats* stats = use_simd ? &stats_simd_on : &stats_simd_off;
        
        stats->frame_count++;
        
        // 워밍업 프레임은 통계에서 제외
        if (stats->warmup_count < BENCHMARK_WARMUP_FRAMES) {
            stats->warmup_count++;
        } else {
            // 실제 측정 시작 (워밍업 이후)
            stats->total_time_ms += elapsed_ms;
            uint32_t measured_frames = stats->frame_count - BENCHMARK_WARMUP_FRAMES;
            if (measured_frames > 0) {
                if (elapsed_ms < stats->min_time_ms) stats->min_time_ms = elapsed_ms;
                if (elapsed_ms > stats->max_time_ms) stats->max_time_ms = elapsed_ms;
                stats->avg_time_ms = stats->total_time_ms / measured_frames;
            }
        }
    }

    g_ascii_frame_id++;
    g_ascii_last_ms = emscripten_get_now();
}

} // extern "C"

// ---------- Emscripten exports ----------
extern "C" {

EMSCRIPTEN_KEEPALIVE
const void* ascii_get_buffer(void) {
    return I_GetASCIIBuffer();
}

EMSCRIPTEN_KEEPALIVE
int ascii_get_buffer_size(void) {
    return CELL_BUFFER_SIZE * (int)sizeof(AsciiCell);
}

EMSCRIPTEN_KEEPALIVE
void ascii_set_simd(int enabled) {
    use_simd = (enabled != 0);
}

EMSCRIPTEN_KEEPALIVE
int ascii_get_simd(void) {
#if defined(__wasm_simd128__)
    return use_simd ? 1 : 0;
#else
    return -1; // 빌드가 SIMD 미지원
#endif
}

EMSCRIPTEN_KEEPALIVE
int ascii_simd_supported(void) {
#if defined(__wasm_simd128__)
    return 1;
#else
    return 0;
#endif
}

EMSCRIPTEN_KEEPALIVE
uint32_t ascii_get_frame_id(void) { return g_ascii_frame_id; }

EMSCRIPTEN_KEEPALIVE
double ascii_get_last_ms(void) { return g_ascii_last_ms; }

EMSCRIPTEN_KEEPALIVE
void ascii_set_benchmark_mode(int enabled) {
    benchmark_mode = (enabled != 0);
    if (benchmark_mode) {
        // 통계 리셋 (워밍업 포함)
        stats_simd_on = {0, 0, 0.0, 1e9, 0.0, 0.0};
        stats_simd_off = {0, 0, 0.0, 1e9, 0.0, 0.0};
    }
}

EMSCRIPTEN_KEEPALIVE
int ascii_get_benchmark_mode(void) {
    return benchmark_mode ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
void ascii_reset_benchmark_stats(void) {
    stats_simd_on = {0, 0, 0.0, 1e9, 0.0, 0.0};
    stats_simd_off = {0, 0, 0.0, 1e9, 0.0, 0.0};
}

EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_frame_count_simd_on(void) { return (double)stats_simd_on.frame_count; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_total_time_simd_on(void) { return stats_simd_on.total_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_min_time_simd_on(void) { return stats_simd_on.min_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_max_time_simd_on(void) { return stats_simd_on.max_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_avg_time_simd_on(void) { return stats_simd_on.avg_time_ms; }

EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_frame_count_simd_off(void) { return (double)stats_simd_off.frame_count; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_total_time_simd_off(void) { return stats_simd_off.total_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_min_time_simd_off(void) { return stats_simd_off.min_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_max_time_simd_off(void) { return stats_simd_off.max_time_ms; }
EMSCRIPTEN_KEEPALIVE
double ascii_get_benchmark_avg_time_simd_off(void) { return stats_simd_off.avg_time_ms; }

} // extern "C"

#endif // __EMSCRIPTEN__