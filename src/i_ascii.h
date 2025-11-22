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
//	ASCII art rendering for Emscripten web port.
//

#ifndef I_ASCII_H
#define I_ASCII_H

#include "doomtype.h"

#ifdef __EMSCRIPTEN__

#ifdef __cplusplus
extern "C" {
#endif

// ASCII output dimensions (can be configured)
#define ASCII_WIDTH  120
#define ASCII_HEIGHT 40

// Convert RGBA buffer to ASCII string
// Input: rgba_buffer - pointer to ARGB8888 pixel data (width x height x 4 bytes)
//        src_width, src_height - source image dimensions
//        ascii_buffer - output buffer for ASCII string (must be at least ASCII_WIDTH * ASCII_HEIGHT + 1 bytes)
//        ascii_width, ascii_height - output ASCII dimensions
// Note: This function is implemented in C++ (i_ascii.cpp)
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                          int src_width, int src_height,
                          char *ascii_buffer,
                          int ascii_width, int ascii_height);

// Get pointer to the current ASCII buffer (for Emscripten export)
// Returns a null-terminated string
const char* I_GetASCIIBuffer(void);

// Initialize ASCII rendering
void I_InitASCII(void);

// Cleanup ASCII rendering
void I_ShutdownASCII(void);

#ifdef __cplusplus
}
#endif

#endif // __EMSCRIPTEN__

#endif // I_ASCII_H

