/**************************************************************************/
/*  gif_common.cpp                                                        */
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

#include "gif_common.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/image.h"
#include "core/io/image_frames.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"
#include "core/variant/variant.h"
#include <gif_lib.h>
#include <cstdint>
#include <cstring>
#include <type_traits>

struct GifFileTypeRAII {
	GifFileType *file_type;
	int error = 0;

	GifFileTypeRAII(void *p_user_ptr, InputFunc p_read_func) {
		file_type = DGifOpen(p_user_ptr, p_read_func, &error);
	}

	~GifFileTypeRAII() {
		int gif_err = 0;
		if (!DGifCloseFile(file_type, &error)) {
			ERR_PRINT(GifErrorString(gif_err));
		}
	}
};

struct GifBuffer {
	uint8_t *data;
	int64_t size;
	int index;
};

int gif_read_buffer(GifFileType *p_gif, GifByteType *p_data, int p_length) {
	GifBuffer *gif_data = (GifBuffer *)(p_gif->UserData);
	if (gif_data->index + p_length > gif_data->size) {
		p_length = gif_data->size - gif_data->index;
	}

	memcpy(p_data, &gif_data->data[gif_data->index], p_length);
	gif_data->index += p_length;
	return p_length;
}

template <typename T>
Error gif_load_from_buffer_t(T *p_dest, const uint8_t *p_buffer, int p_buffer_len, int p_max_frames) {
	ERR_FAIL_NULL_V(p_dest, ERR_INVALID_PARAMETER);

	GifBuffer buffer = { const_cast<uint8_t *>(p_buffer), p_buffer_len, 0 };
	GifFileTypeRAII gif(&buffer, gif_read_buffer);

	ERR_FAIL_COND_V_MSG(!gif.file_type, FAILED, vformat("Failed to open GIF buffer: %s", GifErrorString(gif.error)));
	ERR_FAIL_COND_V_MSG(DGifSlurp(gif.file_type) == GIF_ERROR, FAILED,
			vformat("Failed to read GIF buffer: %s", GifErrorString(gif.file_type->Error)));

	ERR_FAIL_COND_V_MSG(gif.file_type->SWidth <= 0, FAILED, "GIF Image width must be greater than 0.");
	ERR_FAIL_COND_V_MSG(gif.file_type->SHeight <= 0, FAILED, "GIF Image height must be greater than 0.");
	ERR_FAIL_COND_V_MSG(gif.file_type->SWidth > Image::MAX_WIDTH, FAILED, vformat("GIF Image width cannot be greater than %d.", Image::MAX_WIDTH));
	ERR_FAIL_COND_V_MSG(gif.file_type->SHeight > Image::MAX_HEIGHT, FAILED, vformat("GIF Image height cannot be greater than %d.", Image::MAX_HEIGHT));
	ERR_FAIL_COND_V_MSG(gif.file_type->SWidth * gif.file_type->SHeight > Image::MAX_PIXELS, FAILED, vformat("Too many pixels for a GIF Image, maximum is %d.", Image::MAX_PIXELS));

	if constexpr (std::is_same_v<ImageFrames, T>) {
		static_cast<ImageFrames *>(p_dest)->set_frame_count(gif.file_type->ImageCount);

		for (int ext_block_index = 0; ext_block_index < gif.file_type->ExtensionBlockCount; ext_block_index++) {
			ExtensionBlock &ext_block = gif.file_type->ExtensionBlocks[ext_block_index];
			if (ext_block.Function == APPLICATION_EXT_FUNC_CODE && ext_block.ByteCount >= 14 && memcmp(ext_block.Bytes, reinterpret_cast<const GifByteType *>("NETSCAPE2.0"), 11) == 0) {
				static_cast<ImageFrames *>(p_dest)->set_loop_count(ext_block.Bytes[12] + (ext_block.Bytes[13] << 8));
			}
		}
	}

	const int RGBA_COUNT = 4;

	int image_size = gif.file_type->SWidth * gif.file_type->SHeight * RGBA_COUNT;
	Vector<uint8_t> screen;
	screen.resize_initialized(image_size);

	ColorMapObject *common_map = gif.file_type->SColorMap;
	int last_undisposed_frame = -1;
	for (int current_frame = 0; current_frame < gif.file_type->ImageCount; ++current_frame) {
		const SavedImage &current_frame_image = gif.file_type->SavedImages[current_frame];
		const GifImageDesc &current_frame_desc = current_frame_image.ImageDesc;
		ColorMapObject *current_color_map = current_frame_desc.ColorMap ? current_frame_desc.ColorMap : common_map;

		ERR_CONTINUE_MSG(!current_color_map, vformat("Failed to extract color map of GIF Frame index %d.", current_frame));

		GraphicsControlBlock gcb;
		ERR_FAIL_COND_V_MSG(DGifSavedExtensionToGCB(gif.file_type, current_frame, &gcb) == GIF_ERROR, FAILED,
				vformat("Failed to extract GIF Frame Graphics Control Block: %s", GifErrorString(gif.file_type->Error)));

		for (int y = current_frame_desc.Top; y < current_frame_desc.Top + current_frame_desc.Height; y++) {
			uint32_t global_offset = y * gif.file_type->SWidth + current_frame_desc.Left;
			uint32_t local_offset = (y - current_frame_desc.Top) * current_frame_desc.Width;

			for (int x = 0; x < current_frame_desc.Width; x++) {
				uint8_t color_index = current_frame_image.RasterBits[local_offset + x];

				if (color_index == gcb.TransparentColor) {
					continue;
				}

				uint32_t write_index = (global_offset + x) * RGBA_COUNT;
				GifColorType color_type = current_color_map->Colors[color_index];
				screen.write[write_index] = color_type.Red;
				screen.write[write_index + 1] = color_type.Green;
				screen.write[write_index + 2] = color_type.Blue;
				screen.write[write_index + 3] = 255;
			}
		}

		PackedByteArray frame_data;
		frame_data.resize(image_size);
		memcpy(frame_data.ptrw(), screen.ptr(), image_size);

		float delay = gcb.DelayTime / 100.0;
		if (delay == 0) {
			delay = 0.05;
		}

		if constexpr (std::is_same_v<Image, T>) {
			static_cast<Image *>(p_dest)->set_data(gif.file_type->SWidth, gif.file_type->SHeight, false, Image::FORMAT_RGBA8, frame_data);
			break;
		} else if (std::is_same_v<ImageFrames, T>) {
			Ref<Image> img;
			img.instantiate();
			img->set_data(gif.file_type->SWidth, gif.file_type->SHeight, false, Image::FORMAT_RGBA8, frame_data);
			static_cast<ImageFrames *>(p_dest)->set_frame_image(current_frame, img);
			static_cast<ImageFrames *>(p_dest)->set_frame_delay(current_frame, delay);
		}

		const int row_size = current_frame_desc.Width * RGBA_COUNT;
		// What should happen after the frame has been drawn.
		switch (gcb.DisposalMode) {
			// Make the area of the current frame transparent.
			case DISPOSE_BACKGROUND: {
				for (int y = 0; y < current_frame_desc.Height; y++) {
					uint32_t write_index = ((y + current_frame_desc.Top) * gif.file_type->SWidth + current_frame_desc.Left) * RGBA_COUNT;
					memset(&screen.write[write_index], 0, row_size);
				}
			} break;
			// Reset the screen to the last undisposed frame.
			case DISPOSE_PREVIOUS: {
				// Clear the frame.
				if (last_undisposed_frame == -1) {
					for (int y = 0; y < current_frame_desc.Height; y++) {
						uint32_t write_index = ((y + current_frame_desc.Top) * gif.file_type->SWidth + current_frame_desc.Left) * RGBA_COUNT;
						memset(&screen.write[write_index], 0, row_size);
					}
					break;
				}

				if constexpr (std::is_same_v<ImageFrames, T>) {
					PackedByteArray last_frame_data = static_cast<ImageFrames *>(p_dest)->get_frame_image(last_undisposed_frame)->get_data();
					for (int y = 0; y < current_frame_desc.Height; y++) {
						uint32_t write_index = ((y + current_frame_desc.Top) * gif.file_type->SWidth + current_frame_desc.Left) * RGBA_COUNT;
						memcpy(&screen.write[write_index], &last_frame_data.ptr()[write_index], row_size);
					}
				}
			} break;
			default: {
				last_undisposed_frame = current_frame;
			}
		}

		if (gif.file_type->ImageCount == p_max_frames && gif.file_type->ImageCount > 0) {
			break;
		}
	}

	ERR_FAIL_COND_V_MSG(gif.file_type->ImageCount == 0, FAILED, "No frames found.");

	return OK;
}

namespace GIFCommon {
Ref<Image> _gif_unpack(const Vector<uint8_t> &p_buffer) {
	int size = p_buffer.size();
	ERR_FAIL_COND_V(size <= 0, Ref<Image>());
	const uint8_t *r = p_buffer.ptr();

	Ref<Image> img;
	Error err = gif_load_image_from_buffer(*img, r, size);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), "Failed decoding GIF image.");
	return img;
}

Ref<ImageFrames> _gif_animated_unpack(const Vector<uint8_t> &p_buffer, int p_max_frames) {
	int size = p_buffer.size();
	ERR_FAIL_COND_V(size <= 0, Ref<ImageFrames>());
	const uint8_t *r = p_buffer.ptr();

	Ref<ImageFrames> texture;
	Error err = gif_load_image_frames_from_buffer(*texture, r, size, p_max_frames);

	ERR_FAIL_COND_V_MSG(err != OK, Ref<ImageFrames>(), "Failed decoding animated GIF image.");
	return texture;
}

Error gif_load_image_from_buffer(Image *p_image, const uint8_t *p_buffer, int p_buffer_len) {
	return gif_load_from_buffer_t<Image>(p_image, p_buffer, p_buffer_len, 1);
}

Error gif_load_image_frames_from_buffer(ImageFrames *p_image_frames, const uint8_t *p_buffer, int p_buffer_len, int p_max_frames) {
	return gif_load_from_buffer_t<ImageFrames>(p_image_frames, p_buffer, p_buffer_len, p_max_frames);
}
} //namespace GIFCommon
