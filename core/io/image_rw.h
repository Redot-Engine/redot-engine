#pragma once

#include "core/io/image.h"
#include "io.h"

/*
# Image Readers and Writers

Image readers and writers operate on 4x4 blocks of pixels due to it being a
convenient size for SIMD and the compressed formats supported (namely 4x4
block compression and 4x4 or 8x8 adaptive scalable texture compression)
*/

namespace IO::Image
{
	using ReadProc = Error (*)(
		IO::Reader reader,
		void *state,
		ColorRGBAF32x16 *colors
	) noexcept;
	using WriteProc = Error (*)(
		IO::Writer writer,
		void *state,
		ColorRGBAF32x16 *colors
	) noexcept;
	using FlushProc = Error (*)(IO::Writer writer, void *state) noexcept;
	struct Reader
	{
		struct VTbl
		{
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
			uint32_t height
		) noexcept;

		constexpr static inline IO::Error read(
			Reader reader,
			ColorRGBAF32x16 *colors
		) noexcept
		{
			return reader.vtbl->read(reader.reader, reader.state, colors);
		}

		constexpr static inline IO::Error close(Reader reader) noexcept
		{
			return IO::Reader::close(reader.reader);
		}

		constexpr static inline IO::Error size(
			Reader reader,
			size_t *dst
		) noexcept
		{
			return IO::Reader::size(reader.reader, dst);
		}

		static inline void destroy(Reader *reader) noexcept
		{
			if (reader->vtbl->destroy)
			{
				reader->vtbl->destroy(reader->state);
			}
			*reader = {};
		}
	};
	struct Writer
	{
		struct VTbl
		{
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
			uint32_t height
		) noexcept;

		constexpr static inline IO::Error write(
			Writer writer,
			ColorRGBAF32x16 *colors
		) noexcept
		{
			return writer.vtbl->write(writer.writer, writer.state, colors);
		}

		constexpr static inline IO::Error close(Writer writer) noexcept
		{
			return IO::Writer::close(writer.writer);
		}

		constexpr static inline IO::Error flush(Writer writer) noexcept
		{
			IO::Error err = writer.vtbl->flush(writer.writer, writer.state);
			if (err == IO::Error::Okay)
			{
				err = IO::Writer::flush(writer.writer);
			}
			return err;
		}

		static inline void destroy(Writer *writer) noexcept
		{
			if (writer->vtbl->destroy)
			{
				writer->vtbl->destroy(writer->state);
			}
			*writer = {};
		}
	};
}
