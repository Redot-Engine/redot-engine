/**************************************************************************/
/*  image_rw.h                                                            */
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

#include "core/io/image.h"
#include "io.h"

/*
# Image Readers and Writers

Image readers and writers operate on 4x4 blocks of pixels due to it being a
convenient size for SIMD and the compressed formats supported (namely 4x4
block compression and 4x4 or 8x8 adaptive scalable texture compression)
*/

namespace IO::Image {
using ReadProc = Error (*)(
		IO::Reader reader,
		void *state,
		ColorRGBAF32x16 *colors) noexcept;
using WriteProc = Error (*)(
		IO::Writer writer,
		void *state,
		ColorRGBAF32x16 *colors) noexcept;
using FlushProc = Error (*)(IO::Writer writer, void *state) noexcept;
struct Reader {
	struct VTbl {
		ReadProc read;
		DestroyProc destroy;
	};
	IO::Reader reader;
	void *state;
	VTbl *vtbl;

	static IO::Error make(
			Reader *dst,
			IO::Reader r,
			::Image::Format format,
			uint32_t width,
			uint32_t height) noexcept;

	constexpr static inline IO::Error read(
			Reader reader,
			ColorRGBAF32x16 *colors) noexcept {
		return reader.vtbl->read(reader.reader, reader.state, colors);
	}

	constexpr static inline IO::Error close(Reader reader) noexcept {
		return IO::Reader::close(reader.reader);
	}

	constexpr static inline IO::Error size(
			Reader reader,
			size_t *dst) noexcept {
		return IO::Reader::size(reader.reader, dst);
	}

	static inline void destroy(Reader *reader) noexcept {
		if (reader->vtbl->destroy) {
			reader->vtbl->destroy(reader->state);
		}
		*reader = {};
	}
};
struct Writer {
	struct VTbl {
		FlushProc flush;
		WriteProc write;
		DestroyProc destroy;
	};
	IO::Writer writer;
	void *state;
	const VTbl *vtbl;

	static IO::Error make(
			Writer *dst,
			IO::Writer w,
			::Image::Format format,
			uint32_t width,
			uint32_t height) noexcept;

	constexpr static inline IO::Error write(
			Writer writer,
			ColorRGBAF32x16 *colors) noexcept {
		return writer.vtbl->write(writer.writer, writer.state, colors);
	}

	constexpr static inline IO::Error close(Writer writer) noexcept {
		return IO::Writer::close(writer.writer);
	}

	constexpr static inline IO::Error flush(Writer writer) noexcept {
		IO::Error err = writer.vtbl->flush(writer.writer, writer.state);
		if (err == IO::Error::Okay) {
			err = IO::Writer::flush(writer.writer);
		}
		return err;
	}

	static inline void destroy(Writer *writer) noexcept {
		if (writer->vtbl->destroy) {
			writer->vtbl->destroy(writer->state);
		}
		*writer = {};
	}
};
} //namespace IO::Image
