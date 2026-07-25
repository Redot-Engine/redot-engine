/**************************************************************************/
/*  test_slice.h                                                          */
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

#include "core/templates/Slice.h"
#include "tests/test_macros.h"

namespace TestSlice {

TEST_CASE("[Slice] at") {
	uint8_t value[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF };
	Slice slice = {
		.data = &value,
		.length = 8
	};
	CHECK_EQ(*sliceAt(slice, uint8_t, 0), 0xDE);
	CHECK_EQ(*sliceAt(slice, uint8_t, 1), 0xAD);
	CHECK_EQ(*sliceAt(slice, uint8_t, 2), 0xBE);
	CHECK_EQ(*sliceAt(slice, uint8_t, 3), 0xEF);
	CHECK_EQ(*sliceAt(slice, uint8_t, 4), 0xDE);
	CHECK_EQ(*sliceAt(slice, uint8_t, 5), 0xAD);
	CHECK_EQ(*sliceAt(slice, uint8_t, 6), 0xBE);
	CHECK_EQ(*sliceAt(slice, uint8_t, 7), 0xEF);
	CHECK_EQ(sliceAt(slice, uint8_t, 8), nullptr);
	CHECK_EQ(*sliceAt(slice, uint16_t, 0), 0xADDE);
	CHECK_EQ(*sliceAt(slice, uint16_t, 1), 0xEFBE);
	CHECK_EQ(*sliceAt(slice, uint16_t, 2), 0xADDE);
	CHECK_EQ(*sliceAt(slice, uint16_t, 3), 0xEFBE);
	CHECK_EQ(sliceAt(slice, uint16_t, 4), nullptr);
	CHECK_EQ(*sliceAt(slice, uint32_t, 0), 0xEFBEADDE);
	CHECK_EQ(*sliceAt(slice, uint32_t, 1), 0xEFBEADDE);
	CHECK_EQ(sliceAt(slice, uint32_t, 2), nullptr);
	CHECK_EQ(*sliceAt(slice, uint64_t, 0), 0xEFBEADDEEFBEADDE);
	CHECK_EQ(sliceAt(slice, uint64_t, 1), nullptr);
}

TEST_CASE("[Slice] subslice") {
	uint8_t value[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF };
	Slice slice = {
		.data = &value,
		.length = 8
	};
	Slice sub = Slice::nil;
	CHECK(Slice::subslice(&sub, slice, 2, 4));
	CHECK_EQ(sub.length, 4);
	CHECK_EQ(*sliceAt(sub, uint8_t, 0), 0xBE);
	CHECK_EQ(*sliceAt(sub, uint8_t, 1), 0xEF);
	CHECK_EQ(*sliceAt(sub, uint8_t, 2), 0xDE);
	CHECK_EQ(*sliceAt(sub, uint8_t, 3), 0xAD);
}

TEST_CASE("[Slice] subslice end") {
	uint8_t value[8] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD, 0xBE, 0xEF };
	Slice slice = {
		.data = &value,
		.length = 8
	};
	Slice sub = Slice::nil;
	CHECK(Slice::subslice(&sub, slice, 4, 4));
	CHECK_EQ(sub.length, 4);
	CHECK_EQ(*sliceAt(sub, uint8_t, 0), 0xDE);
	CHECK_EQ(*sliceAt(sub, uint8_t, 1), 0xAD);
	CHECK_EQ(*sliceAt(sub, uint8_t, 2), 0xBE);
	CHECK_EQ(*sliceAt(sub, uint8_t, 3), 0xEF);
}

TEST_CASE("[Slice] set byte") {
	uint8_t value[64] = {};
	size_t i = 0;
	size_t j = 0;
	Slice slice = {
		.data = &value,
		.length = 64
	};
	Slice sub = Slice::nil;
	for (i = 0; i < 64; i += 1) {
		Slice::subslice(&sub, slice, 0, i);
		Slice::set(sub, i);
		for (j = 0; j < i; j += 1) {
			CHECK_EQ(*sliceAt(slice, uint8_t, j), i);
		}
		for (; j < 64; j += 1) {
			CHECK_EQ(*sliceAt(slice, uint8_t, j), 0);
		}
	}
}

TEST_CASE("[Slice] copy") {
	uint8_t value[256] = {};
	uint8_t window[128] = {};
	uint8_t bOrig[128] = {};
	Slice slice = {
		.data = &value,
		.length = 256
	};
	Slice windowSlice = {
		.data = &window,
		.length = 128
	};
	Slice bOrigSlice = {
		.data = &bOrig,
		.length = 128
	};
	Slice a = {
		.data = &(value[64]),
		.length = 64
	};
	Slice b = Slice::nil;
	for (size_t i = 0; i < sliceCount(slice, uint8_t); i += 1) {
		*sliceAt(slice, uint8_t, i) = i;
	}
	for (size_t i = 0; i < 128; i += 1) {
		Slice::subslice(&b, slice, i, i);
		Slice::copy(windowSlice, b);
		for (size_t j = 0; j < sliceCount(b, uint8_t); j += 1) {
			CHECK_EQ(
					*sliceAt(windowSlice, uint8_t, j),
					*sliceAt(b, uint8_t, j));
		}
	}
	for (size_t i = 0; i < 128; i += 1) {
		Slice::subslice(&b, slice, i, i);
		Slice::copy(windowSlice, a);
		Slice::copy(bOrigSlice, b);
		Slice::copy(a, b);
		for (size_t j = 0; j < MIN(i, 64U); j += 1) {
			CHECK_EQ(*sliceAt(a, uint8_t, j), *sliceAt(bOrigSlice, uint8_t, j));
		}
		Slice::copy(a, windowSlice);
	}
}

} //namespace TestSlice
