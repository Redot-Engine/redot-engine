/**************************************************************************/
/*  io.cpp                                                                */
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

#include "io.h"

using namespace IO;

struct SliceState {
	Slice buffer;
	size_t offset;
	bool closed;
};

IO::Error sliceRead(SliceState *state, size_t *read, Slice buffer) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice tmp = Slice::nil;
	size_t count = buffer.length;
	if (state->closed) {
		ret = IO::Error::Closed;
	} else {
		if ((state->buffer.length - state->offset) < buffer.length) {
			count = (state->buffer.length - state->offset);
			ret = IO::Error::Eof;
		}
		// the following will not error, and therefore, safe to ignore
		Slice::subslice(&tmp, state->buffer, state->offset, count);
		Slice::copy(buffer, tmp);
		state->offset += count;
		if (read) {
			*read = count;
		}
	}
	return ret;
}

IO::Error sliceWrite(SliceState *state, size_t *written, Slice buffer) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice tmp = Slice::nil;
	size_t count = buffer.length;
	if (state->closed) {
		return IO::Error::Closed;
	}

	if ((state->buffer.length - state->offset) < buffer.length) {
		count = (state->buffer.length - state->offset);
		ret = IO::Error::Eof;
	}
	// the following will not error, and therefore, safe to ignore
	Slice::subslice(&tmp, state->buffer, state->offset, count);
	Slice::copy(tmp, buffer);
	state->offset += count;
	if (written) {
		*written = count;
	}

	return ret;
}

IO::Error sliceSeek(
		SliceState *state,
		ssize_t offset,
		size_t *where,
		Whence whence) noexcept {
	size_t newOffsets[WHENCE_END + 1] = {
		0,
		state->offset,
		state->buffer.length,
	};
	if (state->closed) {
		return IO::Error::Closed;
	}
	if (whence > WHENCE_END) {
		return IO::Error::InvalidWhence;
	}
	if (((size_t)(newOffsets[whence] + offset)) >= state->buffer.length) {
		return IO::Error::InvalidOffset;
	}
	state->offset = ((size_t)(newOffsets[whence] + offset));
	if (where) {
		*where = ((size_t)(newOffsets[whence] + offset));
	}
	return IO::Error::Okay;
}

IO::Error sliceClose(SliceState *state) noexcept {
	IO::Error ret = state->closed ? IO::Error::Closed : IO::Error::Okay;
	state->closed = true;
	return ret;
}

IO::Error sliceSize(SliceState *state, size_t *dst) noexcept {
	IO::Error ret = IO::Error::Okay;
	if (state->closed) {
		return IO::Error::Closed;
	}
	if (dst) {
		*dst = state->buffer.length;
	}
	return ret;
}

IO::Error sliceFlush(SliceState *state) noexcept {
	IO::Error ret = state->closed ? IO::Error::Closed : IO::Error::Okay;
	return ret;
}

void sliceDestroy(SliceState *state) noexcept {
	free(state);
}

static Reader::VTbl SLICE_READER_VTABLE = {
	.read = (ReadProc)sliceRead,
	.seek = (SeekProc)sliceSeek,
	.close = (CloseProc)sliceClose,
	.size = (SizeProc)sliceSize,
	.destroy = (DestroyProc)sliceDestroy,
};

static Writer::VTbl SLICE_WRITER_VTABLE = {
	.write = (WriteProc)sliceWrite,
	.seek = (SeekProc)sliceSeek,
	.close = (CloseProc)sliceClose,
	.flush = (FlushProc)sliceFlush,
	.destroy = (DestroyProc)sliceDestroy,
};

IO::Error Reader::make(Reader *dst, Slice slice) noexcept {
	SliceState *state = (SliceState *)malloc(sizeof(SliceState));
	IO::Error ret = IO::Error::Okay;
	assert(dst);
	if (!state) {
		ret = IO::Error::OutOfMemory;
	} else {
		state->buffer = slice;
		state->offset = 0;
		dst->vtbl = &SLICE_READER_VTABLE;
		dst->state = (void *)state;
	}
	return ret;
}

IO::Error Writer::make(Writer *dst, Slice slice) noexcept {
	SliceState *state = (SliceState *)malloc(sizeof(SliceState));
	IO::Error ret = IO::Error::Okay;
	assert(dst);
	if (!state) {
		ret = IO::Error::OutOfMemory;
	} else {
		state->buffer = slice;
		state->offset = 0;
		dst->vtbl = &SLICE_WRITER_VTABLE;
		dst->state = (void *)state;
	}
	return ret;
}
