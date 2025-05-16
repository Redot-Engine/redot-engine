/**************************************************************************/
/*  resource_format_animated_texture.cpp                                  */
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

#include "resource_format_animated_texture.h"

#include "scene/resources/animated_texture.h"
#include "scene/resources/image_texture.h"

Ref<Resource> ResourceFormatLoaderAnimatedTexture::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (!f->is_open()) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return Ref<Resource>();
	}

	uint8_t header[4] = { 0, 0, 0, 0 };
	f->get_buffer(header, 4);

	bool unrecognized = header[0] != 'R' || header[1] != 'D' || header[2] != 'A' || header[3] != 'T';
	if (unrecognized) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V(Ref<Resource>());
	}

	Ref<AnimatedTexture> atex;
	atex.instantiate();

	[[maybe_unused]] uint32_t tex_flags = f->get_32();
	uint32_t frame_count = f->get_32();
	atex->set_frames(frame_count);

	uint32_t width = f->get_32();
	uint32_t height = f->get_32();

	for (size_t current_frame = 0; current_frame < frame_count; current_frame++) {
		// Frame image data.
		LocalVector<uint8_t> data;
		uint32_t frame_byte_length = f->get_32();
		data.resize(frame_byte_length);
		f->get_buffer(data.ptr(), frame_byte_length);

		Ref<Image> image;
		image.instantiate();
		image->set_data(width, height, false, Image::FORMAT_RGBA8, data);
		Ref<ImageTexture> frame = ImageTexture::create_from_image(image);
		atex->set_frame_texture(current_frame, frame);

		// Frame delay data.
		atex->set_frame_duration(current_frame, f->get_real());
	}

	return atex;
}

void ResourceFormatLoaderAnimatedTexture::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("atex");
}

bool ResourceFormatLoaderAnimatedTexture::handles_type(const String &p_type) const {
	return p_type == "AnimatedTexture";
}

String ResourceFormatLoaderAnimatedTexture::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "atex" ? "AnimatedTexture" : String();
}
