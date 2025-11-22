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
#define ASCII_WIDTH  240
#define ASCII_HEIGHT 80

// Struct to hold data for one character cell
typedef struct {
    char character;
    uint8_t r;
    uint8_t g;
    uint8_t b;
} AsciiCell;


// Convert RGBA buffer to an array of AsciiCell data
void I_ConvertRGBAtoASCII(const uint32_t *rgba_buffer, 
                          int src_width, int src_height,
                          void *cell_buffer,
                          int ascii_width, int ascii_height);

// Get pointer to the current AsciiCell buffer (for Emscripten export)
const void* I_GetASCIIBuffer(void);

// Initialize ASCII rendering
void I_InitASCII(void);

// Cleanup ASCII rendering
void I_ShutdownASCII(void);

#ifdef __cplusplus
}
#endif

#endif // __EMSCRIPTEN__

#endif // I_ASCII_H

