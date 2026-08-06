/**************************************************************************/
/*  Slice.cpp                                                             */
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

#include "core/templates/Slice.h"
#include "core/typedefs.h"

Slice Slice::nil = {};

void Slice::set(Slice dst, uint8_t n) noexcept {
	uint16_t n16 = 0x0101 * n;
	uint32_t n32 = 0x01010101 * n;
	uint64_t n64 = 0x0101010101010101 * n;
	size_t i = 0;
	while (((((uintptr_t)dst.data) + i) & 7) && (i < dst.length)) {
		*((uint8_t *)get(dst, i, 1)) = n;
		i += 1;
	}
	while (i < dst.length) {
		switch (dst.length - i) {
			case 4:
				*((uint32_t *)get(dst, i, 1)) = n32;
				i += 4;
				break;
			case 7:
			case 3:
				*((uint8_t *)get(dst, i, 1)) = n;
				i += 1;
				[[fallthrough]];
			case 6:
			case 2:
				*((uint16_t *)get(dst, i, 1)) = n16;
				i += 2;
				break;
			case 5:
			case 1:
				*((uint8_t *)get(dst, i, 1)) = n;
				i += 1;
				break;
			default:
				*((uint64_t *)get(dst, i, 1)) = n64;
				i += 8;
				[[fallthrough]];
			case 24:
			case 25:
			case 26:
			case 27:
			case 28:
			case 29:
			case 30:
			case 31:
				*((uint64_t *)get(dst, i, 1)) = n64;
				i += 8;
				[[fallthrough]];
			case 16:
			case 17:
			case 18:
			case 19:
			case 20:
			case 21:
			case 22:
			case 23:
				*((uint64_t *)get(dst, i, 1)) = n64;
				i += 8;
				[[fallthrough]];
			case 8:
			case 9:
			case 10:
			case 11:
			case 12:
			case 13:
			case 14:
			case 15:
				*((uint64_t *)get(dst, i, 1)) = n64;
				i += 8;
				break;
		}
	}
}
