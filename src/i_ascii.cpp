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
#include <algorithm>
#include <string>
#include <vector>
#include <emscripten.h>

#if defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif

#include "i_ascii.h"
#include "i_video.h"

#define CELL_BUFFER_SIZE (ASCII_WIDTH * ASCII_HEIGHT)
static AsciiCell cell_buffer[CELL_BUFFER_SIZE];
static boolean ascii_initialized = false;

extern "C" {

void I_InitASCII(void)
{
    if (ascii_initialized)
        return;
    
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
    return cell_buffer;
}

// C++ implementation of RGBA to AsciiCell data conversion
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                          int src_width, int src_height,
                          void *output_buffer,
                          int ascii_width, int ascii_height)
{
    const std::string ascii_chars = " .:-=+*#%@";
    AsciiCell* out_cells = static_cast<AsciiCell*>(output_buffer);

    for (int y = 0; y < ascii_height; ++y) {
        for (int x = 0; x < ascii_width; ++x) {
            // Calculate source region
            int src_x_start = (x * src_width) / ascii_width;
            int src_y_start = (y * src_height) / ascii_height;
            int src_x_end = ((x + 1) * src_width) / ascii_width;
            int src_y_end = ((y + 1) * src_height) / ascii_height;
            
            // Average color and brightness in region
            long long total_r = 0, total_g = 0, total_b = 0;
            int count = 0;

            for (int sy = src_y_start; sy < src_y_end; ++sy) {
                const uint32_t* row_ptr = &rgba_buffer[sy * src_width + src_x_start];
                int region_width = src_x_end - src_x_start;
                count += region_width;

#if defined(__wasm_simd128__)
                v128_t sums = wasm_i32x4_const(0, 0, 0, 0); // Accumulator for BGRA sums
                int sx = 0;
                for (; sx <= region_width - 4; sx += 4) {
                    v128_t pixels = wasm_v128_load(row_ptr + sx); // Load 4 pixels (BGRA)
                    
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
                    uint32_t pixel = row_ptr[sx];
                    total_r += (pixel >> 16) & 0xFF;
                    total_g += (pixel >> 8) & 0xFF;
                    total_b += pixel & 0xFF;
                }
#else 
                for (int sx = src_x_start; sx < src_x_end; ++sx) {
                    uint32_t pixel = rgba_buffer[sy * src_width + sx];
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
            
            int avg_r = total_r / count;
            int avg_g = total_g / count;
            int avg_b = total_b / count;
            
            int brightness = (avg_r * 299 + avg_g * 587 + avg_b * 114) / 1000;
            
            int char_idx = (brightness * (ascii_chars.length() - 1)) / 255;

            cell.character = ascii_chars[char_idx];
            cell.r = avg_r;
            cell.g = avg_g;
            cell.b = avg_b;
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

