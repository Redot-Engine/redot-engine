/**************************************************************************/
/*  ustring_simd.h                                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// SSE4.1 loops for String. They live in their own translation unit on purpose: having them in
// ustring.cpp changes how the compiler optimizes the unrelated UTF-8 code there, which then runs
// measurably slower on non-ASCII text.

#include "core/typedefs.h"

// Same condition as GODOT_SIMD_SSE41 in core/math/simd.h, without pulling the intrinsics in.
#if !defined(GODOT_SIMD_DISABLE) && (defined(__SSE4_1__) || (defined(_MSC_VER) && defined(_M_X64)))
#define GODOT_STRING_SIMD

namespace StringSIMD {

struct HashBlocks {
	uint32_t hash;
	int64_t count;
};

/// Continues the String::hash djb2 over leading blocks of 4 characters that contain no terminator.
/// Returns the hash so far and how many characters were consumed; equals the sequential computation.
HashBlocks hash_blocks(const char32_t *p_chr, int64_t p_length, uint32_t p_hash);

/// Compares blocks of 4 characters of p_str32 with the Latin-1 string p_str, widening `char` with
/// the platform's signedness like `(char32_t)p_str[i]`. Returns the index where the scalar
/// comparison must continue, or -1 if a difference was found.
int64_t latin1_equal_blocks(const char32_t *p_str32, const char *p_str, int64_t p_length);

/// Converts leading blocks of 4 ASCII characters to upper or lower case, stopping at the first block
/// holding NUL or a character outside ASCII. Returns how many characters were converted.
int64_t ascii_case_blocks(bool p_upper, const char32_t *p_src, char32_t *p_dst, int64_t p_length);

struct DecodedBlocks {
	int64_t read;
	int64_t written;
};

/// Decodes leading blocks of well-formed UTF-8 with no NUL, skipping '\r' when p_skip_cr is set, exactly
/// like String::append_utf8. Stops before the first block that needs the scalar loop (an error, NUL or
/// fewer than 32 bytes left). Returns how many bytes were read and characters written.
DecodedBlocks decode_utf8_blocks(const uint8_t *p_src, const uint8_t *p_limit, char32_t *p_dst, bool p_skip_cr);

// Everything is passed by value on purpose: taking the address of the caller's locals changes
// how the compiler allocates registers in the caller's loops.

} // namespace StringSIMD

#endif
