/**************************************************************************/
/*  png_driver_common.cpp                                                 */
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

#include "png_driver_common.h"

#include "core/config/engine.h"

#include <png.h>

namespace PNGDriverCommon {

// Print any warnings.
// On error, set explain and return true.
// Call should be wrapped in ERR_FAIL_COND
static bool check_error(const png_image &image) {
	const png_uint_32 failed = PNG_IMAGE_FAILED(image);
	if (failed & PNG_IMAGE_ERROR) {
		return true;
	} else if (failed) {
#ifdef TOOLS_ENABLED
		// suppress this warning, to avoid log spam when opening assetlib
		const static char *const noisy = "iCCP: known incorrect sRGB profile";
		const Engine *const eng = Engine::get_singleton();
		if (eng && eng->is_editor_hint() && !strcmp(image.message, noisy)) {
			return false;
		}
#endif
		WARN_PRINT(image.message);
	}
	return false;
}

Error png_to_image(const uint8_t *p_source, size_t p_size, bool p_force_linear, Ref<Image> p_image) {
	png_image png_img;
	memset(&png_img, 0, sizeof(png_img));
	png_img.version = PNG_IMAGE_VERSION;

	// fetch image properties
	int success = png_image_begin_read_from_memory(&png_img, p_source, p_size);
	ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
	ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT);

	// flags to be masked out of input format to give target format
	const png_uint_32 format_mask = ~(
			// convert component order to RGBA
			PNG_FORMAT_FLAG_BGR | PNG_FORMAT_FLAG_AFIRST
			// convert 16 bit components to 8 bit
			| PNG_FORMAT_FLAG_LINEAR
			// convert indexed image to direct color
			| PNG_FORMAT_FLAG_COLORMAP);

	png_img.format &= format_mask;

	Image::Format dest_format;
	switch (png_img.format) {
		case PNG_FORMAT_GRAY:
			dest_format = Image::FORMAT_L8;
			break;
		case PNG_FORMAT_GA:
			dest_format = Image::FORMAT_LA8;
			break;
		case PNG_FORMAT_RGB:
			dest_format = Image::FORMAT_RGB8;
			break;
		case PNG_FORMAT_RGBA:
			dest_format = Image::FORMAT_RGBA8;
			break;
		default:
			png_image_free(&png_img); // only required when we return before finish_read
			ERR_PRINT("Unsupported png format.");
			return ERR_UNAVAILABLE;
	}

	if (!p_force_linear) {
		// assume 16 bit pngs without sRGB or gAMA chunks are in sRGB format
		png_img.flags |= PNG_IMAGE_FLAG_16BIT_sRGB;
	}

	const png_uint_32 stride = PNG_IMAGE_ROW_STRIDE(png_img);
	Vector<uint8_t> buffer;
	Error err = buffer.resize(PNG_IMAGE_BUFFER_SIZE(png_img, stride));
	if (err) {
		png_image_free(&png_img); // only required when we return before finish_read
		return err;
	}
	uint8_t *writer = buffer.ptrw();

	// read image data to buffer and release libpng resources
	success = png_image_finish_read(&png_img, nullptr, writer, stride, nullptr);
	ERR_FAIL_COND_V_MSG(check_error(png_img), ERR_FILE_CORRUPT, png_img.message);
	ERR_FAIL_COND_V(!success, ERR_FILE_CORRUPT);

	//print_line("png width: "+itos(png_img.width)+" height: "+itos(png_img.height));
	p_image->set_data(png_img.width, png_img.height, false, dest_format, buffer);

	return OK;
}

Error image_to_png(const Ref<Image> &p_image, Vector<uint8_t> &p_buffer) {
	Ref<Image> source_image = p_image->duplicate();

	if (source_image->is_compressed()) {
		source_image->decompress();
	}

	ERR_FAIL_COND_V(source_image->is_compressed(), FAILED);

	png_image png_img;
	memset(&png_img, 0, sizeof(png_img));
	png_img.version = PNG_IMAGE_VERSION;
	png_img.width = source_image->get_width();
	png_img.height = source_image->get_height();

	switch (source_image->get_format()) {
		case Image::FORMAT_L8:
			png_img.format = PNG_FORMAT_GRAY;
			break;
		case Image::FORMAT_LA8:
			png_img.format = PNG_FORMAT_GA;
			break;
		case Image::FORMAT_RGB8:
			png_img.format = PNG_FORMAT_RGB;
			break;
		case Image::FORMAT_RGBA8:
			png_img.format = PNG_FORMAT_RGBA;
			break;
		default:
			if (source_image->detect_alpha() != Image::ALPHA_NONE) {
				source_image->convert(Image::FORMAT_RGBA8);
				png_img.format = PNG_FORMAT_RGBA;
			} else {
				source_image->convert(Image::FORMAT_RGB8);
				png_img.format = PNG_FORMAT_RGB;
			}
	}

	const Vector<uint8_t> image_data = source_image->get_data();
	const uint8_t *reader = image_data.ptr();

	// we may be passed a buffer with existing content we're expected to append to
	const int buffer_offset = p_buffer.size();

	const size_t png_size_estimate = PNG_IMAGE_PNG_SIZE_MAX(png_img);

	// try with estimated size
	size_t compressed_size = png_size_estimate;
	int success = 0;
	{ // scope writer lifetime
		Error err = p_buffer.resize(buffer_offset + png_size_estimate);
		ERR_FAIL_COND_V(err, err);

		uint8_t *writer = p_buffer.ptrw();
		success = png_image_write_to_memory(&png_img, &writer[buffer_offset],
				&compressed_size, 0, reader, 0, nullptr);
		ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
	}
	if (!success) {
		// buffer was big enough, must be some other error
		ERR_FAIL_COND_V(compressed_size <= png_size_estimate, FAILED);

		// write failed due to buffer size, resize and retry
		Error err = p_buffer.resize(buffer_offset + compressed_size);
		ERR_FAIL_COND_V(err, err);

		uint8_t *writer = p_buffer.ptrw();
		success = png_image_write_to_memory(&png_img, &writer[buffer_offset],
				&compressed_size, 0, reader, 0, nullptr);
		ERR_FAIL_COND_V_MSG(check_error(png_img), FAILED, png_img.message);
		ERR_FAIL_COND_V(!success, FAILED);
	}

	// trim buffer size to content
	Error err = p_buffer.resize(buffer_offset + compressed_size);
	ERR_FAIL_COND_V(err, err);

	return OK;
}

/// APNG functions

static void apng_error_func(png_struct *p_struct, const char *p_message) {
	ERR_PRINT(p_message);
}

static void apng_warn_func(png_struct *p_struct, const char *p_message) {
	WARN_PRINT(p_message);
}

struct APngBuffer {
	uint8_t *data;
	size_t size;
	int index;
};

static void apng_read_buffer(png_struct *p_struct, png_byte *p_data, size_t p_length) {
	APngBuffer *png_data = static_cast<APngBuffer *>(png_get_io_ptr(p_struct));
	if (png_data->index + p_length > png_data->size) {
		p_length = png_data->size - png_data->index;
	}

	memcpy(p_data, &png_data->data[png_data->index], p_length);
	png_data->index += p_length;
}

Error apng_to_image_frames(const uint8_t *p_source, size_t p_size, bool p_force_linear, uint32_t p_frame_limit, Ref<ImageFrames> p_frames) {
#ifdef PNG_READ_APNG_SUPPORTED
	struct Frame {
		Vector<uint8_t> buffer;
		uint32_t width;
		uint32_t height;
		uint32_t offset_x;
		uint32_t offset_y;
		float delay;
		uint8_t dispose_op;
		uint8_t blend_op;
	};

	png_image png_img;

	memset(&png_img, 0, sizeof(png_img));
	png_img.version = PNG_IMAGE_VERSION;
	png_struct *struct_ = png_create_read_struct(PNG_LIBPNG_VER_STRING, &png_img, &apng_error_func, &apng_warn_func);
	png_info *info = nullptr;

	if (struct_ == nullptr) {
		png_destroy_read_struct(&struct_, nullptr, nullptr);
		png_image_free(&png_img);
	} else {
		info = png_create_info_struct(struct_);
		if (info == nullptr) {
			png_destroy_read_struct(&struct_, nullptr, nullptr);
			png_image_free(&png_img);
		}
	}

	ERR_FAIL_COND_V_MSG(struct_ == nullptr, ERR_FILE_CORRUPT, "Couldn't create APNG structure.");
	ERR_FAIL_COND_V_MSG(info == nullptr, ERR_FILE_CORRUPT, "Couldn't create APNG info structure.");

	if (setjmp(png_jmpbuf(struct_))) {
		png_destroy_read_struct(&struct_, &info, nullptr);
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Couldn't load APNG.");
	}

	APngBuffer read_buffer_obj = { const_cast<uint8_t *>(p_source), p_size, 0 };

	png_set_read_fn(struct_, &read_buffer_obj, apng_read_buffer);
	png_read_info(struct_, info);

	png_byte bit_depth = png_get_bit_depth(struct_, info);
	if (bit_depth == 16) {
		png_set_scale_16(struct_);
	}

	const int8_t NO_TRANSPARENCY = -1;

	switch (png_get_color_type(struct_, info)) {
		case PNG_COLOR_TYPE_GRAY:
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			if (bit_depth < 8) {
				png_set_expand_gray_1_2_4_to_8(struct_);
			}
			break;
		case PNG_COLOR_TYPE_PALETTE:
			png_set_palette_to_rgb(struct_);
			break;
		case PNG_COLOR_TYPE_RGB:
		case PNG_COLOR_TYPE_RGB_ALPHA:
			break;
		default:
			ERR_PRINT("Unsupported png format.");
			return ERR_UNAVAILABLE;
	}

	png_read_update_info(struct_, info);

	Image::Format dest_format;
	int8_t alpha_component_index = NO_TRANSPARENCY;
	switch (png_get_color_type(struct_, info)) {
		case PNG_COLOR_TYPE_GRAY:
			dest_format = Image::FORMAT_L8;
			break;
		case PNG_COLOR_TYPE_GRAY_ALPHA:
			dest_format = Image::FORMAT_LA8;
			alpha_component_index = 1;
			break;
		case PNG_COLOR_TYPE_RGB:
			dest_format = Image::FORMAT_RGB8;
			break;
		case PNG_COLOR_TYPE_RGB_ALPHA:
			dest_format = Image::FORMAT_RGBA8;
			alpha_component_index = 3;
			break;
		default:
			ERR_PRINT("Unsupported png format.");
			return ERR_UNAVAILABLE;
	}

	uint8_t pixel_size = Image::get_format_pixel_size(dest_format);

	// PNG properties
	uint32_t width = png_get_image_width(struct_, info);
	uint32_t height = png_get_image_height(struct_, info);

	// APNG properties
	bool is_animated = png_get_valid(struct_, info, PNG_INFO_acTL);
	uint32_t frame_count = is_animated ? png_get_num_frames(struct_, info) : 1;
	uint32_t loop_count = png_get_num_plays(struct_, info);
	bool is_first_frame_hidden = is_animated ? png_get_first_frame_is_hidden(struct_, info) : false;

	auto read_image = [pixel_size, struct_](Vector<uint8_t> &p_buffer, uint32_t p_width, uint32_t p_height) {
		LocalVector<uint8_t *> line_buffer;
		line_buffer.resize(p_height);
		for (uint32_t y = 0; y < p_height; y++) {
			line_buffer[y] = p_buffer.ptrw() + (p_width * y * pixel_size);
		}
		png_read_image(struct_, line_buffer.ptr());
	};

	Vector<uint8_t> screen;
	screen.resize_initialized(width * height * pixel_size);
	if (is_animated) {
		// Skip first frame
		if (is_first_frame_hidden) {
			frame_count--;
			read_image(screen, width, height);
		}

		frame_count = p_frame_limit > 0 ? MIN(frame_count, p_frame_limit) : frame_count;

		p_frames->set_frame_count(frame_count);
		p_frames->set_loop_count(loop_count);

		auto read_frame = [pixel_size, struct_, info, &read_image]() -> Frame {
			Frame frame{};

			uint16_t delay_num;
			uint16_t delay_den;

			png_read_frame_head(struct_, info);
			if (png_get_next_frame_fcTL(struct_, info,
						&frame.width,
						&frame.height,
						&frame.offset_x,
						&frame.offset_y,
						&delay_num,
						&delay_den,
						&frame.dispose_op,
						&frame.blend_op) == 0) {
				return frame;
			}

			frame.delay = float(delay_num) / float(delay_den == 0 ? 100.0 : delay_den);

			frame.buffer.resize_initialized(frame.width * frame.height * pixel_size);
			read_image(frame.buffer, frame.width, frame.height);
			return frame;
		};

		// Read initial frame
		Frame previous_frame{};
		Frame current_frame = read_frame();

		ERR_FAIL_COND_V_MSG(current_frame.buffer.is_empty(), ERR_FILE_CORRUPT, "Couldn't read APNG initial frame.");

		if (current_frame.dispose_op == PNG_DISPOSE_OP_PREVIOUS) {
			current_frame.dispose_op = PNG_DISPOSE_OP_BACKGROUND;
			memset(screen.ptrw(), 0, screen.size());
		}

		Vector<uint8_t> backup_buffer;
		for (uint32_t current_frame_index = 0; current_frame_index < frame_count; current_frame_index++) {
			if (current_frame_index != 0) {
				previous_frame = std::move(current_frame);
				current_frame = read_frame();
			}

			ERR_FAIL_COND_V_MSG(current_frame.buffer.is_empty(), ERR_FILE_CORRUPT, "Couldn't read APNG frame.");

			// Optimize padding frames
			if (current_frame_index != 0 && current_frame.blend_op == PNG_BLEND_OP_OVER && current_frame.buffer.size() == pixel_size && current_frame.buffer.count(0) == current_frame.buffer.size()) {
				frame_count--;
				current_frame_index--;
				p_frames->set_frame_count(frame_count);
				p_frames->set_frame_delay(current_frame_index, p_frames->get_frame_delay(current_frame_index) + current_frame.delay);
				continue;
			}

			if (current_frame_index != 0 && current_frame.dispose_op == PNG_DISPOSE_OP_PREVIOUS && previous_frame.dispose_op == PNG_DISPOSE_OP_PREVIOUS) {
				if (backup_buffer.is_empty()) {
					backup_buffer.resize(screen.size());
					memcpy(backup_buffer.ptrw(), screen.ptr(), backup_buffer.size());
				} else {
					SWAP(screen, backup_buffer);
				}
			} else {
				if (current_frame.dispose_op == PNG_DISPOSE_OP_PREVIOUS) {
					if (backup_buffer.is_empty()) {
						backup_buffer.resize(screen.size());
					}
					memcpy(backup_buffer.ptrw(), screen.ptr(), backup_buffer.size());
				}

				if (current_frame_index != 0) {
					// Prepare from previous frame
					switch (previous_frame.dispose_op) {
						case PNG_DISPOSE_OP_NONE:
							break;
						case PNG_DISPOSE_OP_PREVIOUS:
							ERR_FAIL_COND_V_MSG(backup_buffer.is_empty(), ERR_FILE_CORRUPT, "Bug: Error in APNG frame processing logic, please report.");
							memcpy(screen.ptrw(), backup_buffer.ptr(), screen.size());
							break;
						default:
							memset(screen.ptrw(), 0, screen.size());
							break;
					}
				}
			}

			uint32_t copy_width = MIN(width - current_frame.offset_x, current_frame.width);
			uint32_t copy_height = MIN(height - current_frame.offset_y, current_frame.height);
			size_t length = copy_width * pixel_size;

			for (uint32_t y = 0; y < copy_height; y++) {
				const uint8_t *src = &current_frame.buffer[y * current_frame.width * pixel_size];
				uint8_t *dest = &screen.write[((current_frame.offset_y + y) * width + current_frame.offset_x) * pixel_size];

				// alpha_index == NO_TRANSPARENCY means there is no alpha component, treat as opaque
				if (alpha_component_index == NO_TRANSPARENCY || current_frame.blend_op != PNG_BLEND_OP_OVER) {
					memcpy(dest, src, length);
				} else {
					// Blend frames
					for (size_t index = 0; index < length; index += pixel_size, src += pixel_size, dest += pixel_size) {
						if (src[alpha_component_index] == 255) {
							memcpy(dest, src, pixel_size);
							continue;
						}

						if (src[alpha_component_index] != 0) {
							if (dest[alpha_component_index] != 0) {
								int u = src[alpha_component_index] * 255;
								int v = (255 - src[alpha_component_index]) * dest[alpha_component_index];
								int a1 = u + v;
								for (int color_index = 0; color_index < pixel_size; color_index++) {
									if (color_index == alpha_component_index) {
										continue;
									}
									dest[color_index] = (src[color_index] * u + dest[color_index] * v) / a1;
								}
								dest[alpha_component_index] = a1 / 255;
							} else {
								memcpy(dest, src, pixel_size);
							}
						}
					}
				}
			}

			Ref<Image> image = memnew(Image(width, height, false, dest_format, screen));
			p_frames->set_frame_image(current_frame_index, image);
			p_frames->set_frame_delay(current_frame_index, current_frame.delay);
		}

		png_read_end(struct_, info);
	} else {
		p_frames->set_frame_count(1);

		read_image(screen, width, height);

		Ref<Image> image = memnew(Image(width, height, false, dest_format, screen));
		p_frames->set_frame_image(0, image);

		png_read_end(struct_, info);
	}

	png_destroy_read_struct(&struct_, &info, nullptr);

	return OK;
#else
	WARN_PRINT("Reading APNG files is disabled, reading APNG as PNG instead. Compile with builtin_png=yes.");
	Ref<Image> image;
	image.instantiate();
	png_to_image(p_source, p_size, p_force_linear, image);
	p_frames->set_frame_count(1);
	p_frames->set_frame_image(0, image);
	return OK;
#endif // PNG_READ_APNG_SUPPORTED
}
} // namespace PNGDriverCommon
