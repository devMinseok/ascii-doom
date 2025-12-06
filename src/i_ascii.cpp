//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	ASCII art rendering for Emscripten web port (C++ implementation).
//

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

#define CELL_BUFFER_SIZE (ASCII_WIDTH * ASCII_HEIGHT)
static AsciiCell cell_buffer[CELL_BUFFER_SIZE];

// ASCII 렌더링이 초기화되었는지 여부를 나타내는 플래그
static boolean ascii_initialized = false;

// 감마 보정 룩업 테이블 (pow() 호출 제거로 성능 향상)
static uint8_t gamma_table[256];
static boolean gamma_table_initialized = false;

// ASCII 문자 팔레트 (constexpr 배열로 최적화)
static constexpr char ASCII_CHARS[] = " .:-=+*#%@";
static constexpr int ASCII_CHARS_LEN = sizeof(ASCII_CHARS) - 1;  // null terminator 제외

extern "C" {

// 감마 테이블 초기화 (gamma = 0.35, 밝기 2배 증가)
static void init_gamma_table(void)
{
    if (gamma_table_initialized)
        return;
    
    // 감마 0.35 = 더 밝은 출력 (기존 0.5에서 약 2배 밝기)
    constexpr float gamma = 0.35f;
    for (int i = 0; i < 256; ++i) {
        float normalized = i / 255.0f;
        float corrected = std::pow(normalized, gamma);
        gamma_table[i] = static_cast<uint8_t>(std::min(255.0f, corrected * 255.0f));
    }
    gamma_table_initialized = true;
}

void I_InitASCII(void)
{
    if (ascii_initialized)
        return;
    
    // 감마 테이블 초기화 (최초 1회만)
    init_gamma_table();
    
    // 전체 AsciiCell 버퍼를 0으로 초기화
    //  → 처음에는 화면 전체가 '빈 문자/검은색' 상태가 됨
    std::memset(cell_buffer, 0, sizeof(cell_buffer));
    ascii_initialized = true;
}

void I_ShutdownASCII(void)
{
    if (!ascii_initialized)
        return;
    
    ascii_initialized = false;
}

const void* I_GetASCIIBuffer(void)
{
    // JS 쪽에서 이 포인터를 받아서 Uint8Array 로 캐스팅해 사용한다.
    // (ascii_get_buffer() 를 통해 export 됨)
    return cell_buffer;
}

// C++ implementation of RGBA to AsciiCell data conversion (optimized)
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                          int src_width, int src_height,
                          void *output_buffer,
                          int ascii_width, int ascii_height)
{
    AsciiCell* out_cells = static_cast<AsciiCell*>(output_buffer);

    // 스케일 비율을 미리 계산 (나눗셈 최소화)
    const int scale_x = (src_width << 8) / ascii_width;   // 8비트 고정소수점
    const int scale_y = (src_height << 8) / ascii_height;

    for (int y = 0; y < ascii_height; ++y) {
        const int src_y_start = (y * scale_y) >> 8;
        const int src_y_end = ((y + 1) * scale_y) >> 8;
        
        for (int x = 0; x < ascii_width; ++x) {
            const int src_x_start = (x * scale_x) >> 8;
            const int src_x_end = ((x + 1) * scale_x) >> 8;
            
            // 해당 영역의 RGB 값을 합산
            int total_r = 0, total_g = 0, total_b = 0;
            int count = 0;

            for (int sy = src_y_start; sy < src_y_end; ++sy) {
                const uint32_t* row_ptr = &rgba_buffer[sy * src_width + src_x_start];
                const int region_width = src_x_end - src_x_start;
                count += region_width;

#if defined(__wasm_simd128__)
                v128_t sums = wasm_i32x4_const(0, 0, 0, 0);
                int sx = 0;
                for (; sx <= region_width - 4; sx += 4) {
                    v128_t pixels = wasm_v128_load(row_ptr + sx);
                    
                    v128_t wide_low = wasm_u16x8_extend_low_u8x16(pixels);
                    v128_t wide_high = wasm_u16x8_extend_high_u8x16(pixels);
                    
                    sums = wasm_i32x4_add(sums, wasm_u32x4_extend_low_u16x8(wide_low));
                    sums = wasm_i32x4_add(sums, wasm_u32x4_extend_high_u16x8(wide_low));
                    sums = wasm_i32x4_add(sums, wasm_u32x4_extend_low_u16x8(wide_high));
                    sums = wasm_i32x4_add(sums, wasm_u32x4_extend_high_u16x8(wide_high));
                }

                total_b += wasm_i32x4_extract_lane(sums, 0);
                total_g += wasm_i32x4_extract_lane(sums, 1);
                total_r += wasm_i32x4_extract_lane(sums, 2);

                for (; sx < region_width; ++sx) {
                    const uint32_t pixel = row_ptr[sx];
                    total_r += (pixel >> 16) & 0xFF;
                    total_g += (pixel >> 8) & 0xFF;
                    total_b += pixel & 0xFF;
                }
#else 
                for (int sx = 0; sx < region_width; ++sx) {
                    const uint32_t pixel = row_ptr[sx];
                    total_r += (pixel >> 16) & 0xFF;
                    total_g += (pixel >> 8) & 0xFF;
                    total_b += pixel & 0xFF;
                }
#endif
            }
            
            AsciiCell& cell = out_cells[y * ascii_width + x];

            if (count == 0) {
                cell.character = ' ';
                cell.r = 0;
                cell.g = 0;
                cell.b = 0;
                continue;
            }
            
            // 평균 계산
            const int avg_r = total_r / count;
            const int avg_g = total_g / count;
            const int avg_b = total_b / count;
            
            // 밝기 계산 (정수 연산)
            const int brightness = (avg_r * 299 + avg_g * 587 + avg_b * 114) / 1000;
            
            // ASCII 문자 인덱스 계산 (constexpr 배열 사용)
            const int char_idx = (brightness * (ASCII_CHARS_LEN - 1)) / 255;

            // 감마 룩업 테이블 사용 (pow() 호출 제거)
            cell.character = ASCII_CHARS[char_idx];
            cell.r = gamma_table[avg_r];
            cell.g = gamma_table[avg_g];
            cell.b = gamma_table[avg_b];
        }
    }
}

} // extern "C"

// Emscripten export: Get ASCII buffer pointer
extern "C" {
EMSCRIPTEN_KEEPALIVE
const void* ascii_get_buffer(void)
{
    return I_GetASCIIBuffer();
}

// Emscripten export: Get ASCII buffer size
EMSCRIPTEN_KEEPALIVE
int ascii_get_buffer_size(void)
{
    return CELL_BUFFER_SIZE * sizeof(AsciiCell);
}
} // extern "C"

#endif // __EMSCRIPTEN__

