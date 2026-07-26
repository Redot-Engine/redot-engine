/**************************************************************************/
/*  io.h                                                                  */
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
#include <cstddef>

namespace IO {
enum class Error {
	Okay,
	OutOfMemory,
	Eof,
	InvalidWhence,
	InvalidOffset,
	NotImplemented,
	Closed,
};

enum Whence : uint32_t {
	WHENCE_SET,
	WHENCE_CURRENT,
	WHENCE_END,
};

using ReadProc = Error (*)(
		void *state,
		size_t *read,
		Slice buffer) noexcept;
using WriteProc = Error (*)(
		void *state,
		size_t *written,
		Slice buffer) noexcept;
using SeekProc = Error (*)(
		void *state,
		ptrdiff_t offset,
		size_t *where,
		Whence whence) noexcept;
using CloseProc = Error (*)(void *state) noexcept;
using SizeProc = Error (*)(void *state, size_t *dst) noexcept;
using FlushProc = Error (*)(void *state) noexcept;
// because C++ and this codebase's heavy use of RAII
using DestroyProc = void (*)(void *state) noexcept;

struct Reader {
	struct VTbl {
		ReadProc read;
		SeekProc seek;
		CloseProc close;
		SizeProc size;
		DestroyProc destroy;
	};
	void *state;
	const VTbl *vtbl;

	static Error make(Reader *dst, Slice slice) noexcept;
	// static Error make(Reader* dst, Ref<FileAccess> file) noexcept;
	static void destroy(Reader *reader) noexcept {
		if (reader->vtbl && reader->vtbl->destroy) {
			reader->vtbl->destroy(reader->state);
		}
		*reader = {};
	}

	constexpr static inline Error read(
			Reader reader,
			size_t *read,
			Slice buffer) noexcept {
		return reader.vtbl->read(reader.state, read, buffer);
	}

	constexpr static inline Error seek(
			Reader reader,
			ptrdiff_t offset,
			size_t *where,
			Whence whence) noexcept {
		return reader.vtbl->seek(reader.state, offset, where, whence);
	}

	constexpr static inline Error close(Reader reader) noexcept {
		return reader.vtbl->close(reader.state);
	}

	constexpr static inline Error size(Reader reader, size_t *dst) noexcept {
		return reader.vtbl->size(reader.state, dst);
	}
};

struct Writer {
	struct VTbl {
		WriteProc write;
		SeekProc seek;
		CloseProc close;
		FlushProc flush;
		DestroyProc destroy;
	};
	void *state;
	const VTbl *vtbl;

	static Error make(Writer *dst, Slice slice) noexcept;
	// static Error make(Writer* dst, Ref<FileAccess> file) noexcept;
	static void destroy(Writer *writer) noexcept {
		if (writer->vtbl && writer->vtbl->destroy) {
			writer->vtbl->destroy(writer->state);
		}
		*writer = {};
	}

	constexpr static inline Error write(
			Writer writer,
			size_t *written,
			Slice buffer) noexcept {
		return writer.vtbl->write(writer.state, written, buffer);
	}

	constexpr static inline Error seek(
			Writer writer,
			ptrdiff_t offset,
			size_t *where,
			Whence whence) noexcept {
		return writer.vtbl->seek(writer.state, offset, where, whence);
	}

	constexpr static inline Error close(Writer writer) noexcept {
		return writer.vtbl->close(writer.state);
	}

	constexpr static inline Error flush(Writer writer) noexcept {
		return writer.vtbl->flush(writer.state);
	}
};
} //namespace IO
