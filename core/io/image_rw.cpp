/**************************************************************************/
/*  image_rw.cpp                                                          */
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

#include "image_rw.h"
#include "core/io/image.h"

using namespace IO::Image;

struct UncompressedImageState {
	uint32_t currentBlock;
	uint32_t res[2];
	Slice data;
	::Image::Format format;
	ColorRGBAF32x16 blocks4x4[];
};

constexpr size_t CONSTANT_FACTORS[::Image::FORMAT_MAX] = {
	1, // FORMAT_L8
	2, // FORMAT_LA8
	1, // FORMAT_R8
	2, // FORMAT_RG8
	3, // FORMAT_RGB8
	4, // FORMAT_RGBA8
	2, // FORMAT_RGBA4444
	2, // FORMAT_RGB565
	4, // FORMAT_RF
	8, // FORMAT_RGF
	12, // FORMAT_RGBF
	16, // FORMAT_RGBAF
	2, // FORMAT_RH
	4, // FORMAT_RGH
	6, // FORMAT_RGBH
	8, // FORMAT_RGBAH
	4, // FORMAT_RGBE9995
	1, // FORMAT_DXT1
	1, // FORMAT_DXT3
	1, // FORMAT_DXT5
	1, // FORMAT_RGTC_R
	1, // FORMAT_RGTC_RG
	1, // FORMAT_BPTC_RGBA
	1, // FORMAT_BPTC_RGBF
	1, // FORMAT_BPTC_RGBFU
	1, // FORMAT_ETC
	1, // FORMAT_ETC2_R11
	1, // FORMAT_ETC2_R11S
	1, // FORMAT_ETC2_RG11
	1, // FORMAT_ETC2_RG11S
	1, // FORMAT_ETC2_RGB8
	1, // FORMAT_ETC2_RGBA8
	1, // FORMAT_ETC2_RGB8A1
	1, // FORMAT_ETC2_RA_AS_RG
	1, // FORMAT_DXT5_RA_AS_RG
	1, // FORMAT_ASTC_4x4
	1, // FORMAT_ASTC_4x4_HDR
	1, // FORMAT_ASTC_8x8
	1, // FORMAT_ASTC_8x8_HDR
};

static IO::Error constantFactorWriterStateCtor(
		UncompressedImageState **state,
		uint32_t width,
		uint32_t height,
		::Image::Format format) noexcept {
	IO::Error ret = IO::Error::Okay;
	// ensure we have enough 4x4 blocks to cover the width of the image
	*state = (UncompressedImageState *)Memory::alloc_aligned_static(
			sizeof(UncompressedImageState) +
					((((width << 2) * CONSTANT_FACTORS[format]) + ((size_t)0xFF)) & ~((size_t)0xFF)),
			alignof(UncompressedImageState));
	if (*state) {
		(*state)->data.data = (*state)->blocks4x4;
		(*state)->data.length = (width << 2) * CONSTANT_FACTORS[format];
		(*state)->res[0] = width;
		(*state)->res[1] = height;
		(*state)->format = format;
		(*state)->currentBlock = 0;
	} else {
		ret = IO::Error::OutOfMemory;
	}
	return ret;
}

static IO::Error constantFactorReaderStateCtor(
		UncompressedImageState **state,
		uint32_t width,
		uint32_t height,
		::Image::Format format) noexcept {
	IO::Error ret = IO::Error::Okay;
	// ensure we have enough 4x4 blocks to cover the width of the image
	*state = (UncompressedImageState *)Memory::alloc_aligned_static(
			sizeof(UncompressedImageState) +
					((((width << 2) * CONSTANT_FACTORS[format]) + ((size_t)0xFF)) & ~((size_t)0xFF)),
			alignof(UncompressedImageState));
	if (*state) {
		(*state)->data.data = (*state)->blocks4x4;
		(*state)->data.length = (width << 2) * CONSTANT_FACTORS[format];
		(*state)->res[0] = width;
		(*state)->res[1] = height;
		(*state)->format = format;
		(*state)->currentBlock = (width + 3) >> 2;
	} else {
		ret = IO::Error::OutOfMemory;
	}
	return ret;
}

static IO::Error l8ReadScalar(
		IO::Reader reader,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t read = 0;
	uint32_t width = state->res[0];
	size_t block;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		if (!data.data) {
			return IO::Error::OutOfMemory;
		}
		ret = reader.read(reader, &read, data);
		if (read) {
			state->currentBlock = 0;
		}
	}
	block = state->currentBlock;
	if (ret == IO::Error::Okay) {
		for (size_t c = 0; c < 3; c += 1) {
			for (size_t i = 0; i < 4; i += 1) {
				for (size_t j = 0; j < 4; j += 1) {
					if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
						colors->c[c][(i << 2) + j] = (*sliceAt(
								data,
								uint8_t,
								(((block << 2) + (width * i)) + j)));
					} else {
						colors->c[c][(i << 2) + j] = 0;
					}
					colors->c[c][(i << 2) + j] /= 255;
				}
			}
		}
		state->currentBlock += 1;
	}
	for (size_t i = 0; i < 16; i += 1) {
		colors->a[i] = 1;
	}
	return ret;
}

static IO::Error l8WriteScalar(
		IO::Writer writer,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	uint32_t width = state->res[0];
	size_t block = state->currentBlock;
	for (size_t i = 0; i < 4; i += 1) {
		for (size_t j = 0; j < 4; j += 1) {
			if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
				(*sliceAt(
						data,
						uint8_t,
						(((block << 2) + (width * i)) + j))) = (13938U * ((uint64_t)(colors->r[(i << 2) + j] * 255)) +
																	   46869U * ((uint64_t)(colors->g[(i << 2) + j] * 255)) +
																	   4729U * ((uint64_t)(colors->b[(i << 2) + j] * 255)) +
																	   32768U) >>
						16U;
			}
		}
	}
	state->currentBlock += 1;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		ret = writer.write(writer, nullptr, state->data);
		state->currentBlock = 0;
	}
	return ret;
}

static IO::Error la8ReadScalar(
		IO::Reader reader,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t read;
	uint32_t width = state->res[0];
	size_t block;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		if (!data.data) {
			return IO::Error::OutOfMemory;
		}
		ret = reader.read(reader, &read, data);
		if (read) {
			state->currentBlock = 0;
		}
	}
	block = state->currentBlock;
	if (ret == IO::Error::Okay) {
		for (size_t c = 0; c < 3; c += 1) {
			for (size_t i = 0; i < 4; i += 1) {
				for (size_t j = 0; j < 4; j += 1) {
					if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
						colors->c[c][(i << 2) + j] = (*sliceAt(
								data,
								uint8_t,
								(((block << 2) + (width * i)) + (j << 1))));
					} else {
						colors->c[c][(i << 2) + j] = 0;
					}
					colors->c[c][(i << 2) + j] /= 255;
				}
			}
		}
		for (size_t i = 0; i < 4; i += 1) {
			for (size_t j = 0; j < 4; j += 1) {
				if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
					colors->a[(i << 2) + j] = (*sliceAt(
							data,
							uint8_t,
							(((block << 2) + (width * i)) + ((j << 1) + 1))));
				} else {
					colors->a[(i << 2) + j] = 0;
				}
				colors->a[(i << 2) + j] /= 255;
			}
		}
		state->currentBlock += 1;
	}
	return ret;
}

static IO::Error la8WriteScalar(
		IO::Writer writer,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	uint32_t width = state->res[0];
	size_t block = state->currentBlock;
	for (size_t i = 0; i < 4; i += 1) {
		for (size_t j = 0; j < 4; j += 1) {
			if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
				(*sliceAt(
						data,
						uint8_t,
						((((block << 3) + (width * i)) + j) << 1))) = (13938U * ((uint64_t)(colors->r[(i << 2) + j] * 255)) +
																			  46869U * ((uint64_t)(colors->g[(i << 2) + j] * 255)) +
																			  4729U * ((uint64_t)(colors->b[(i << 2) + j] * 255)) +
																			  32768U) >>
						16U;
				(*sliceAt(
						data,
						uint8_t,
						(((((block << 3) + (width * i)) + j) << 1) + 1))) = (uint8_t)(colors->a[(i << 2) + j] * 255);
			}
		}
	}
	state->currentBlock += 1;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		ret = writer.write(writer, nullptr, state->data);
		state->currentBlock = 0;
	}
	return ret;
}

constexpr size_t COMPONENT_COUNT[::Image::FORMAT_MAX] = {
	1, // FORMAT_L8
	2, // FORMAT_LA8
	1, // FORMAT_R8
	2, // FORMAT_RG8
	3, // FORMAT_RGB8
	4, // FORMAT_RGBA8
	4, // FORMAT_RGBA4444
	3, // FORMAT_RGB565
	1, // FORMAT_RF
	2, // FORMAT_RGF
	3, // FORMAT_RGBF
	4, // FORMAT_RGBAF
	1, // FORMAT_RH
	2, // FORMAT_RGH
	3, // FORMAT_RGBH
	4, // FORMAT_RGBAH
	4, // FORMAT_RGBE9995
	1, // FORMAT_DXT1
	1, // FORMAT_DXT3
	1, // FORMAT_DXT5
	1, // FORMAT_RGTC_R
	1, // FORMAT_RGTC_RG
	1, // FORMAT_BPTC_RGBA
	1, // FORMAT_BPTC_RGBF
	1, // FORMAT_BPTC_RGBFU
	1, // FORMAT_ETC
	1, // FORMAT_ETC2_R11
	1, // FORMAT_ETC2_R11S
	1, // FORMAT_ETC2_RG11
	1, // FORMAT_ETC2_RG11S
	1, // FORMAT_ETC2_RGB8
	1, // FORMAT_ETC2_RGBA8
	1, // FORMAT_ETC2_RGB8A1
	1, // FORMAT_ETC2_RA_AS_RG
	1, // FORMAT_DXT5_RA_AS_RG
	1, // FORMAT_ASTC_4x4
	1, // FORMAT_ASTC_4x4_HDR
	1, // FORMAT_ASTC_8x8
	1, // FORMAT_ASTC_8x8_HDR
};

static IO::Error rgba8ReadScalar(
		IO::Reader reader,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t read;
	size_t block;
	::Image::Format format = state->format;
	uint32_t width = state->res[0];
	size_t c;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		if (!data.data) {
			return IO::Error::OutOfMemory;
		}
		ret = reader.read(reader, &read, data);
		if (read) {
			state->currentBlock = 0;
		}
	}
	block = state->currentBlock;
	if (ret == IO::Error::Okay) {
		for (c = 0; c < COMPONENT_COUNT[format]; c += 1) {
			for (size_t i = 0; i < 4; i += 1) {
				for (size_t j = 0; j < 4; j += 1) {
					if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
						colors->c[c][(i << 2) + j] = (*sliceAt(
								data,
								uint8_t,
								(((block << 2) + (width * i) + j) * COMPONENT_COUNT[format]) + c));
					} else {
						colors->c[c][(i << 2) + j] = 0;
					}
					colors->c[c][(i << 2) + j] /= 255;
				}
			}
		}
		for (; c < 3; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 0;
			}
		}
		for (; c < 4; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 1;
			}
		}
		state->currentBlock += 1;
	}
	return ret;
}

static IO::Error rgba8WriteScalar(
		IO::Writer writer,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t block = state->currentBlock;
	::Image::Format format = state->format;
	uint32_t width = state->res[0];
	for (size_t c = 0; c < COMPONENT_COUNT[format]; c += 1) {
		for (size_t i = 0; i < 4; i += 1) {
			for (size_t j = 0; j < 4; j += 1) {
				if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
					(*sliceAt(
							data,
							uint8_t,
							((((block << 2) + (width * i)) * COMPONENT_COUNT[format]) + (j * COMPONENT_COUNT[format]) + c))) = colors->c[c][(i << 2) + j] * 255;
				}
			}
		}
	}
	state->currentBlock += 1;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		ret = writer.write(writer, nullptr, state->data);
		state->currentBlock = 0;
	}
	return ret;
}

template <typename T>
static IO::Error rgbafReadScalar(
		IO::Reader reader,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t read;
	size_t block;
	::Image::Format format = state->format;
	uint32_t width = state->res[0];
	size_t c;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		if (!data.data) {
			return IO::Error::OutOfMemory;
		}
		ret = reader.read(reader, &read, data);
		if (read) {
			state->currentBlock = 0;
		}
	}
	block = state->currentBlock;
	if (ret == IO::Error::Okay) {
		for (c = 0; c < COMPONENT_COUNT[format]; c += 1) {
			for (size_t i = 0; i < 4; i += 1) {
				for (size_t j = 0; j < 4; j += 1) {
					if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
						colors->c[c][(i << 2) + j] = (*sliceAt(
								data,
								T,
								(((block << 2) + (width * i) + j) * COMPONENT_COUNT[format]) + c));
					} else {
						colors->c[c][(i << 2) + j] = 0;
					}
				}
			}
		}
		for (; c < 3; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 0;
			}
		}
		for (; c < 4; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 1;
			}
		}
		state->currentBlock += 1;
	}
	return ret;
}

template <typename T>
static IO::Error rgbafWriteScalar(
		IO::Writer writer,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t block = state->currentBlock;
	::Image::Format format = state->format;
	uint32_t width = state->res[0];
	for (size_t c = 0; c < COMPONENT_COUNT[format]; c += 1) {
		for (size_t i = 0; i < 4; i += 1) {
			for (size_t j = 0; j < 4; j += 1) {
				if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
					(*sliceAt(
							data,
							T,
							(((block << 2) + (width * i) + j) * COMPONENT_COUNT[format]) + c)) = (T)(colors->c[c][(i << 2) + j]);
				}
			}
		}
	}
	state->currentBlock += 1;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		ret = writer.write(writer, nullptr, state->data);
		state->currentBlock = 0;
	}
	return ret;
}

static IO::Error rgba4444ReadScalar(
		IO::Reader reader,
		UncompressedImageState *state,
		ColorRGBAF32x16 *colors) noexcept {
	IO::Error ret = IO::Error::Okay;
	Slice data = state->data;
	size_t read;
	size_t block;
	::Image::Format format = state->format;
	uint32_t width = state->res[0];
	size_t c;
	if (state->currentBlock >= ((width + 3) >> 2)) {
		if (!data.data) {
			return IO::Error::OutOfMemory;
		}
		ret = reader.read(reader, &read, data);
		if (ret == IO::Error::Okay) {
			state->currentBlock = 0;
		}
	}
	block = state->currentBlock;
	if (ret == IO::Error::Okay) {
		for (c = 0; c < COMPONENT_COUNT[format]; c += 1) {
			for (size_t i = 0; i < 4; i += 1) {
				for (size_t j = 0; j < 4; j += 1) {
					if ((((block << 2) + (width * i)) + j) < (width * (i + 1))) {
						colors->c[c][(i << 2) + j] = ((*sliceAt(data, uint8_t, ((((block << 2) + (width * i)) * CONSTANT_FACTORS[::Image::FORMAT_RGBA4444]) + (((j * COMPONENT_COUNT[format]) + c) >> 1)))) >> ((c & 1) << 2)) & 0x0f;
					} else {
						colors->c[c][(i << 2) + j] = 0;
					}
					colors->c[c][(i << 2) + j] /= 15;
				}
			}
		}
		for (; c < 3; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 0;
			}
		}
		for (; c < 4; c += 1) {
			for (size_t i = 0; i < 16; i += 1) {
				colors->c[c][i] = 1;
			}
		}
		state->currentBlock += 1;
	}
	return ret;
}

static IO::Error defaultFlush(
		IO::Writer writer,
		UncompressedImageState *state) noexcept {
	IO::Error err = IO::Error::Okay;
	size_t cursor;
	Slice tmp = {};
	err = IO::Writer::seek(writer, 0, &cursor, IO::WHENCE_CURRENT);
	if (state->currentBlock == 0) {
		return err;
	}
	for (size_t i = 0; i < 4; i += 1) {
		err = IO::Writer::seek(
				writer,
				cursor + (i * (state->res[0]) * CONSTANT_FACTORS[state->format]),
				nullptr,
				IO::WHENCE_SET);
		if (err != IO::Error::Okay) {
			return err;
		}
		Slice::subslice(
				&tmp,
				state->data,
				i * (state->res[0]) * CONSTANT_FACTORS[state->format],
				(state->currentBlock * CONSTANT_FACTORS[state->format]) << 2);
		err = IO::Writer::write(writer, nullptr, tmp);
		switch (err) {
			case IO::Error::Okay:
			case IO::Error::Eof:
				err = IO::Error::Okay;
				[[fallthrough]];
			default:
				return err;
		}
	}
	return err;
}

static Reader::VTbl READER_SCALAR_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .read = (ReadProc)l8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .read = (ReadProc)la8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .read = (ReadProc)rgba4444ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Writer::VTbl WRITER_SCALAR_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)l8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)la8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Reader::VTbl READER_SSE42_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .read = (ReadProc)l8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .read = (ReadProc)la8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .read = (ReadProc)rgba4444ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Writer::VTbl WRITER_SSE42_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)l8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)la8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Reader::VTbl READER_AVX_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .read = (ReadProc)l8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .read = (ReadProc)la8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .read = (ReadProc)rgba4444ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Writer::VTbl WRITER_AVX_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)l8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)la8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Reader::VTbl READER_AVX2_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .read = (ReadProc)l8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .read = (ReadProc)la8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .read = (ReadProc)rgba8ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .read = (ReadProc)rgba4444ReadScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .read = (ReadProc)rgbafReadScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .read = (ReadProc)rgbafReadScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .read = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .read = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

static Writer::VTbl WRITER_AVX2_FUNCTIONS[::Image::FORMAT_MAX] = {
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)l8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_L8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)la8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_LA8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_R8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RG8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB8
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgba8WriteScalar, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA8
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBA4444
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGB565
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<float>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAF
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBH
	{ .flush = (FlushProc)defaultFlush, .write = (WriteProc)rgbafWriteScalar<_Float16>, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBAH
	{ .flush = (FlushProc)defaultFlush, .write = nullptr, .destroy = (IO::DestroyProc)free }, // FORMAT_RGBE9995
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT3
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_R
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_RGTC_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBA
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBF
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_BPTC_RGBFU
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_R11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RG11S
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGBA8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RGB8A1
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ETC2_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_DXT5_RA_AS_RG
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_4x4_HDR
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8
	{ .flush = nullptr, .write = nullptr, .destroy = nullptr }, // FORMAT_ASTC_8x8_HDR
};

using ConstructorProc = IO::Error (*)(
		void **state,
		uint32_t width,
		uint32_t height,
		::Image::Format format);

static ConstructorProc READER_STATE_CONSTRUCTOR[::Image::FORMAT_MAX] = {
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_L8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_LA8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_R8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RG8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGB8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBA8
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBA4444
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGB565
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RF
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGF
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBF
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBAF
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RH
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGH
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBH
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBAH
	(ConstructorProc)constantFactorReaderStateCtor, // FORMAT_RGBE9995
	nullptr, // FORMAT_DXT1
	nullptr, // FORMAT_DXT3
	nullptr, // FORMAT_DXT5
	nullptr, // FORMAT_RGTC_R
	nullptr, // FORMAT_RGTC_RG
	nullptr, // FORMAT_BPTC_RGBA
	nullptr, // FORMAT_BPTC_RGBF
	nullptr, // FORMAT_BPTC_RGBFU
	nullptr, // FORMAT_ETC
	nullptr, // FORMAT_ETC2_R11
	nullptr, // FORMAT_ETC2_R11S
	nullptr, // FORMAT_ETC2_RG11
	nullptr, // FORMAT_ETC2_RG11S
	nullptr, // FORMAT_ETC2_RGB8
	nullptr, // FORMAT_ETC2_RGBA8
	nullptr, // FORMAT_ETC2_RGB8A1
	nullptr, // FORMAT_ETC2_RA_AS_RG
	nullptr, // FORMAT_DXT5_RA_AS_RG
	nullptr, // FORMAT_ASTC_4x4
	nullptr, // FORMAT_ASTC_4x4_HDR
	nullptr, // FORMAT_ASTC_8x8
	nullptr, // FORMAT_ASTC_8x8_HDR
};

static ConstructorProc WRITER_STATE_CONSTRUCTOR[::Image::FORMAT_MAX] = {
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_L8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_LA8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_R8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RG8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGB8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBA8
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBA4444
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGB565
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RF
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGF
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBF
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBAF
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RH
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGH
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBH
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBAH
	(ConstructorProc)constantFactorWriterStateCtor, // FORMAT_RGBE9995
	nullptr, // FORMAT_DXT1
	nullptr, // FORMAT_DXT3
	nullptr, // FORMAT_DXT5
	nullptr, // FORMAT_RGTC_R
	nullptr, // FORMAT_RGTC_RG
	nullptr, // FORMAT_BPTC_RGBA
	nullptr, // FORMAT_BPTC_RGBF
	nullptr, // FORMAT_BPTC_RGBFU
	nullptr, // FORMAT_ETC
	nullptr, // FORMAT_ETC2_R11
	nullptr, // FORMAT_ETC2_R11S
	nullptr, // FORMAT_ETC2_RG11
	nullptr, // FORMAT_ETC2_RG11S
	nullptr, // FORMAT_ETC2_RGB8
	nullptr, // FORMAT_ETC2_RGBA8
	nullptr, // FORMAT_ETC2_RGB8A1
	nullptr, // FORMAT_ETC2_RA_AS_RG
	nullptr, // FORMAT_DXT5_RA_AS_RG
	nullptr, // FORMAT_ASTC_4x4
	nullptr, // FORMAT_ASTC_4x4_HDR
	nullptr, // FORMAT_ASTC_8x8
	nullptr, // FORMAT_ASTC_8x8_HDR
};

IO::Error Reader::make(
		Reader *dst,
		IO::Reader r,
		::Image::Format format,
		uint32_t width,
		uint32_t height) noexcept {
	dst->reader = r;
#if (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
	if (__builtin_cpu_supports("avx2")) {
		dst->vtbl = &(READER_AVX2_FUNCTIONS[format]);
	} else if (__builtin_cpu_supports("avx")) {
		dst->vtbl = &(READER_AVX_FUNCTIONS[format]);
	} else if (__builtin_cpu_supports("sse4.2")) {
		dst->vtbl = &(READER_SSE42_FUNCTIONS[format]);
	} else {
		dst->vtbl = &(READER_SCALAR_FUNCTIONS[format]);
	}
#else
	dst->vtbl = &(READER_SCALAR_FUNCTIONS[format]);
#endif
	if (dst->vtbl->read == nullptr) {
		return IO::Error::NotImplemented;
	}
	if (READER_STATE_CONSTRUCTOR[format]) {
		return READER_STATE_CONSTRUCTOR[format](&(dst->state), width, height, format);
	}
	return IO::Error::Okay;
}

IO::Error Writer::make(
		Writer *dst,
		IO::Writer w,
		::Image::Format format,
		uint32_t width,
		uint32_t height) noexcept {
	dst->writer = w;
#if (defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)) && (defined(__GNUC__) || defined(__clang__))
	if (__builtin_cpu_supports("avx2")) {
		dst->vtbl = &(WRITER_AVX2_FUNCTIONS[format]);
	} else if (__builtin_cpu_supports("avx")) {
		dst->vtbl = &(WRITER_AVX_FUNCTIONS[format]);
	} else if (__builtin_cpu_supports("sse4.2")) {
		dst->vtbl = &(WRITER_SSE42_FUNCTIONS[format]);
	} else {
		dst->vtbl = &(WRITER_SCALAR_FUNCTIONS[format]);
	}
#else
	dst->vtbl = &(WRITER_SCALAR_FUNCTIONS[format]);
#endif
	if (dst->vtbl->write == nullptr) {
		return IO::Error::NotImplemented;
	}
	if (WRITER_STATE_CONSTRUCTOR[format]) {
		return WRITER_STATE_CONSTRUCTOR[format](&(dst->state), width, height, format);
	}
	return IO::Error::Okay;
}
