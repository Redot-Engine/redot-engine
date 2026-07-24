/**************************************************************************/
/*  Slice.h                                                               */
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

#include <cassert>
#include <cstddef>
#include <cstdint>

struct Slice {
	void *data;
	size_t length;

	static Slice nil;

	constexpr static inline void *get(
			Slice self,
			size_t index,
			size_t size) noexcept;

	constexpr static inline bool subslice(
			Slice *dst,
			Slice src,
			size_t begin,
			size_t count) noexcept;

	static inline bool set(Slice dst, Slice src, size_t index) noexcept;
	static void copy(Slice dst, Slice src) noexcept;
	static void set(Slice dst, uint8_t n) noexcept;
};

constexpr inline void *Slice::get(
		Slice self,
		size_t index,
		size_t size) noexcept {
	void *ret = nullptr;
	if ((size * (index + 1)) < self.length) {
		ret = ((uint8_t *)self.data) + (size * index);
	}
	return ret;
}

constexpr inline bool Slice::subslice(
		Slice *dst,
		Slice src,
		size_t begin,
		size_t count) noexcept {
	bool ret = !(src.length <= (begin + count));
	assert(dst);
	if (ret) {
		dst->data = &(((uint8_t *)src.data)[begin]);
		dst->length = count;
	}
	return ret;
}

inline bool Slice::set(Slice dst, Slice src, size_t index) noexcept {
	Slice tmp = {};
	if (!subslice(&tmp, dst, index, src.length)) {
		return false;
	}
	Slice::copy(tmp, src);
	return true;
}

#define sliceAt(slice, t, i) ((t *)Slice::get(slice, i, sizeof(t)))
#define subslice_t(dst, src, t, begin, count) subslice( \
		(dst),                                          \
		(src),                                          \
		sizeof(t) * (begin),                            \
		sizeof(t) * (count))
#define sliceCount(self, t) ((self).length / sizeof(t))
