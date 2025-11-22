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
#include "i_ascii.h"
#include "i_video.h"

// New buffer for HTML output. Size is estimated for worst-case scenario:
// ASCII_WIDTH * ASCII_HEIGHT * (sizeof(<span>...</span>) + safety margin)
// Approx: 120 * 40 * 50 = 240,000. Let's use 250,000.
#define HTML_BUFFER_SIZE 250000
static char html_buffer[HTML_BUFFER_SIZE];
static boolean ascii_initialized = false;

extern "C" {

void I_InitASCII(void)
{
    if (ascii_initialized)
        return;
    
    html_buffer[0] = '\0';
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
    return html_buffer;
}

// C++ implementation of RGBA to colored HTML ASCII conversion
extern "C" void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                                     int src_width, int src_height,
                                     char *output_buffer, // This will be our html_buffer
                                     int ascii_width, int ascii_height)
{
    const std::string ascii_chars = " .:-=+*#%@";
    char* buffer_ptr = output_buffer;
    int remaining_size = HTML_BUFFER_SIZE;

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
                for (int sx = src_x_start; sx < src_x_end; ++sx) {
                    uint32_t pixel = rgba_buffer[sy * src_width + sx];
                    // Assuming RGBA8888, where alpha is the highest byte
                    total_r += (pixel >> 16) & 0xFF;
                    total_g += (pixel >> 8) & 0xFF;
                    total_b += pixel & 0xFF;
                    ++count;
                }
            }
            
            if (count == 0) continue;
            
            int avg_r = total_r / count;
            int avg_g = total_g / count;
            int avg_b = total_b / count;
            
            // Convert to brightness (0-255) using luminance formula
            int brightness = (avg_r * 299 + avg_g * 587 + avg_b * 114) / 1000;
            
            // Map to ASCII character
            int char_idx = (brightness * (ascii_chars.length() - 1)) / 255;
            char ascii_char = ascii_chars[char_idx];
            
            // Prevent special HTML characters from breaking the output
            char final_char = ascii_char;
            if (ascii_char == '<') final_char = '&lt;';
            if (ascii_char == '>') final_char = '&gt;';
            if (ascii_char == '&') final_char = '&amp;';

            // Write colored HTML span to buffer
            int written = snprintf(buffer_ptr, remaining_size,
                                   "<span style=\"color:rgb(%d,%d,%d);\">%c</span>",
                                   avg_r, avg_g, avg_b, final_char);
            
            if (written > 0) {
                buffer_ptr += written;
                remaining_size -= written;
            }
        }
        // Add line break
        int written = snprintf(buffer_ptr, remaining_size, "<br/>");
        if (written > 0) {
            buffer_ptr += written;
            remaining_size -= written;
        }
    }
    *buffer_ptr = '\0'; // Null-terminate the string
}

} // extern "C"

// Emscripten export: Get ASCII buffer pointer
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
    return HTML_BUFFER_SIZE;
}
} // extern "C"

#endif // __EMSCRIPTEN__

