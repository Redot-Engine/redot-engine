/**************************************************************************/
/*  image_frames.cpp                                                      */
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

#include "image_frames.h"

#include "core/error/error_macros.h"
#include "core/io/image_frames_loader.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

void ImageFrames::set_frame_count(int p_frames) {
	ERR_FAIL_COND(p_frames < 0);

	frames.resize(p_frames);
}

int ImageFrames::get_frame_count() const {
	return frames.size();
}

void ImageFrames::set_frame_image(int p_frame, Ref<Image> p_image) {
	ERR_FAIL_INDEX(p_frame, frames.size());

	frames.write[p_frame].image = p_image;
}

Ref<Image> ImageFrames::get_frame_image(int p_frame) const {
	ERR_FAIL_INDEX_V(p_frame, frames.size(), Ref<Image>());

	return frames[p_frame].image;
}

void ImageFrames::set_frame_delay(int p_frame, float p_delay) {
	ERR_FAIL_INDEX(p_frame, frames.size());

	frames.write[p_frame].delay = p_delay;
}

float ImageFrames::get_frame_delay(int p_frame) const {
	ERR_FAIL_INDEX_V(p_frame, frames.size(), 0);

	return frames[p_frame].delay;
}

void ImageFrames::set_loop_count(int p_loop) {
	ERR_FAIL_COND(p_loop < 0);

	loop_count = p_loop;
}

int ImageFrames::get_loop_count() const {
	return loop_count;
}

bool ImageFrames::is_empty() const {
	return get_frame_count() == 0;
}

Error ImageFrames::_load_from_buffer(const Vector<uint8_t> &p_array, ImageFramesMemLoadFunc p_loader, int p_max_frames) {
	int buffer_size = p_array.size();

	ERR_FAIL_COND_V(buffer_size == 0, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_loader, ERR_INVALID_PARAMETER);

	const uint8_t *r = p_array.ptr();

	Ref<ImageFrames> img_frames = p_loader(r, buffer_size, p_max_frames);
	ERR_FAIL_COND_V(!img_frames.is_valid(), ERR_PARSE_ERROR);

	copy_internals_from(img_frames);

	return OK;
}

Error ImageFrames::load(const String &p_path) {
#ifdef DEBUG_ENABLED
	if (p_path.begins_with("res://") && ResourceLoader::exists(p_path)) {
		WARN_PRINT("Loaded resource as image frames file, this will not work on export: '" + p_path + "'. Instead, import the image frames file as an ImageFrames resource and load it normally as a resource.");
	}
#endif
	return ImageFramesLoader::load_image_frames(p_path, this);
}

Ref<ImageFrames> ImageFrames::load_from_file(const String &p_path) {
#ifdef DEBUG_ENABLED
	if (p_path.begins_with("res://") && ResourceLoader::exists(p_path)) {
		WARN_PRINT("Loaded resource as image frames file, this will not work on export: '" + p_path + "'. Instead, import the image frames file as an ImageFrames resource and load it normally as a resource.");
	}
#endif
	Ref<ImageFrames> img_frames;
	img_frames.instantiate();
	Error err = ImageFramesLoader::load_image_frames(p_path, img_frames);
	if (err != OK) {
		ERR_FAIL_V_MSG(Ref<ImageFrames>(), vformat("Failed to load image frames. Error %d", err));
	}
	return img_frames;
}

Error ImageFrames::load_apng_from_buffer(const PackedByteArray &p_array, int p_max_frames) {
	return _load_from_buffer(p_array, _apng_mem_loader_func, p_max_frames);
}

Error ImageFrames::load_webp_from_buffer(const PackedByteArray &p_array, int p_max_frames) {
	return _load_from_buffer(p_array, _webp_mem_loader_func, p_max_frames);
}

Error ImageFrames::load_gif_from_buffer(const PackedByteArray &p_array, int p_max_frames) {
	ERR_FAIL_NULL_V_MSG(
			_gif_mem_loader_func,
			ERR_UNAVAILABLE,
			"The GIF module isn't enabled. Recompile the Redot editor or export template binary with the `module_gif_enabled=yes` SCons option.");
	return _load_from_buffer(p_array, _gif_mem_loader_func, p_max_frames);
}

ImageFrames::ImageFrames(const uint8_t *p_mem_apng, int p_len) {
	if (_apng_mem_loader_func) {
		copy_internals_from(_apng_mem_loader_func(p_mem_apng, p_len, 0));
	}
}

ImageFrames::ImageFrames(const Vector<Ref<Image>> &p_images, float p_delay) {
	set_frame_count(p_images.size());
	for (uint32_t index = 0; index < p_images.size(); index++) {
		ERR_CONTINUE(p_images[index].is_null());
		frames.write[index] = Frame{ p_images[index], p_delay };
	}
}

ImageFrames::ImageFrames(const Vector<Ref<Image>> &p_images, const Vector<float> &p_delays) {
	ERR_FAIL_COND_MSG(p_images.size() != p_delays.size(), vformat("The Image count (%d) does not match the delays count (%d).", p_images.size(), p_delays.size()));

	set_frame_count(p_images.size());
	for (uint32_t index = 0; index < p_images.size(); index++) {
		ERR_CONTINUE(p_images[index].is_null());
		frames.write[index] = Frame{ p_images[index], p_delays[index] };
	}
}

void ImageFrames::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_frame_count", "frames"), &ImageFrames::set_frame_count);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &ImageFrames::get_frame_count);

	ClassDB::bind_method(D_METHOD("set_frame_image", "frame", "image"), &ImageFrames::set_frame_image);
	ClassDB::bind_method(D_METHOD("get_frame_image", "frame"), &ImageFrames::get_frame_image);

	ClassDB::bind_method(D_METHOD("set_frame_delay", "frame", "delay"), &ImageFrames::set_frame_delay);
	ClassDB::bind_method(D_METHOD("get_frame_delay", "frame"), &ImageFrames::get_frame_delay);

	ClassDB::bind_method(D_METHOD("set_loop_count", "loop"), &ImageFrames::set_loop_count);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &ImageFrames::get_loop_count);

	ClassDB::bind_method(D_METHOD("is_empty"), &ImageFrames::is_empty);

	ClassDB::bind_method(D_METHOD("load", "path"), &ImageFrames::load);
	ClassDB::bind_static_method("ImageFrames", D_METHOD("load_from_file", "path"), &ImageFrames::load_from_file);

	ClassDB::bind_method(D_METHOD("load_apng_from_buffer", "buffer", "max_frames"), &ImageFrames::load_apng_from_buffer);
	ClassDB::bind_method(D_METHOD("load_webp_from_buffer", "buffer", "max_frames"), &ImageFrames::load_webp_from_buffer);
	ClassDB::bind_method(D_METHOD("load_gif_from_buffer", "buffer", "max_frames"), &ImageFrames::load_gif_from_buffer);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "frame_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_frame_count", "get_frame_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), "set_loop_count", "get_loop_count");
}
