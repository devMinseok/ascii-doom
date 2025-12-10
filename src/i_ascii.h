#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 문자 그리드 해상도 (웹/JS와 반드시 일치)
#ifndef ASCII_WIDTH
#define ASCII_WIDTH  240
#endif
#ifndef ASCII_HEIGHT
#define ASCII_HEIGHT 80
#endif

typedef struct {
    char    character;
    uint8_t r, g, b;
} AsciiCell;

// 초기화/종료
void I_InitASCII(void);
void I_ShutdownASCII(void);

// RGBA32(0xAARRGGBB) → ASCII 셀 버퍼 변환
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer,
                          int src_width, int src_height,
                          void *output_buffer,
                          int ascii_width, int ascii_height);

// 버퍼 포인터/크기
const void* I_GetASCIIBuffer(void);

// 런타임 토글/상태
void ascii_set_simd(int enabled);
int  ascii_get_simd(void);
int  ascii_simd_supported(void);

// (선택) 엔진 FPS/지연 측정용 카운터 getter
uint32_t ascii_get_frame_id(void);
double   ascii_get_last_ms(void);

#ifdef __cplusplus
}
#endif