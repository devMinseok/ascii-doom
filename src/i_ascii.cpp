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

#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <string>
#include <emscripten.h>
#include "i_ascii.h"
#include "i_video.h"

// Static buffer for ASCII output (includes newlines: (width + 1) * height + 1)
static char ascii_buffer[(ASCII_WIDTH + 1) * ASCII_HEIGHT + 1];
static boolean ascii_initialized = false;

extern "C" {

void I_InitASCII(void)
{
    if (ascii_initialized)
        return;
    
    std::memset(ascii_buffer, 0, sizeof(ascii_buffer));
    ascii_initialized = true;
}

void I_ShutdownASCII(void)
{
    if (!ascii_initialized)
        return;
    
    ascii_initialized = false;
}

const char* I_GetASCIIBuffer(void)
{
    return ascii_buffer;
}

// C++ implementation of ASCII conversion (called from C code)
extern "C" void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                                     int src_width, int src_height,
                                     char *ascii_buffer,
                                     int ascii_width, int ascii_height)
{
    const std::string ascii_chars = " .:-=+*#%@";
    
    for (int y = 0; y < ascii_height; ++y) {
        for (int x = 0; x < ascii_width; ++x) {
            // Calculate source region
            int src_x = (x * src_width) / ascii_width;
            int src_y = (y * src_height) / ascii_height;
            int src_x_end = ((x + 1) * src_width) / ascii_width;
            int src_y_end = ((y + 1) * src_height) / ascii_height;
            
            // Average brightness in region
            int total_r = 0, total_g = 0, total_b = 0;
            int count = 0;
            for (int sy = src_y; sy < src_y_end; ++sy) {
                for (int sx = src_x; sx < src_x_end; ++sx) {
                    uint32_t pixel = rgba_buffer[sy * src_width + sx];
                    total_r += (pixel >> 16) & 0xFF;
                    total_g += (pixel >> 8) & 0xFF;
                    total_b += pixel & 0xFF;
                    ++count;
                }
            }
            
            // Avoid division by zero
            if (count == 0) {
                ascii_buffer[y * (ascii_width + 1) + x] = ' ';
                continue;
            }
            
            int avg_r = total_r / count;
            int avg_g = total_g / count;
            int avg_b = total_b / count;
            
            // Convert to brightness (0-255) using luminance formula
            int brightness = (avg_r * 299 + avg_g * 587 + avg_b * 114) / 1000;
            
            // Map to ASCII character
            int char_count = static_cast<int>(ascii_chars.length());
            int char_idx = (brightness * (char_count - 1)) / 255;
            ascii_buffer[y * (ascii_width + 1) + x] = ascii_chars[char_idx];
        }
        ascii_buffer[y * (ascii_width + 1) + ascii_width] = '\n';
    }
    ascii_buffer[ascii_height * (ascii_width + 1)] = '\0';
}

} // extern "C"

// Emscripten export: Get ASCII buffer pointer
// Must be outside extern "C" block for EMSCRIPTEN_KEEPALIVE to work properly
extern "C" {
EMSCRIPTEN_KEEPALIVE
const char* ascii_get_buffer(void)
{
    return I_GetASCIIBuffer();
}

// Emscripten export: Get ASCII buffer size
EMSCRIPTEN_KEEPALIVE
int ascii_get_buffer_size(void)
{
    return (ASCII_WIDTH + 1) * ASCII_HEIGHT + 1;
}
} // extern "C"

#endif // __EMSCRIPTEN__

