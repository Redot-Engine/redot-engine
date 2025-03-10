/**************************************************************************/
/*  resource_importer_animated_texture.cpp                                */
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

#include "resource_importer_animated_texture.h"

#include "core/io/image.h"
#include "core/io/image_frames_loader.h"
#include "core/typedefs.h"
#include "scene/resources/animated_texture.h"

String ResourceImporterAnimatedTexture::get_importer_name() const {
	return "animated_texture";
}

String ResourceImporterAnimatedTexture::get_visible_name() const {
	return "AnimatedTexture";
}

void ResourceImporterAnimatedTexture::get_recognized_extensions(List<String> *p_extensions) const {
	ImageFramesLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterAnimatedTexture::get_save_extension() const {
	return "atex";
}

String ResourceImporterAnimatedTexture::get_resource_type() const {
	return "AnimatedTexture";
}

bool ResourceImporterAnimatedTexture::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

int ResourceImporterAnimatedTexture::get_preset_count() const {
	return 0;
}

String ResourceImporterAnimatedTexture::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterAnimatedTexture::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_as_srgb"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/size_limit", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/frame_limit", PROPERTY_HINT_RANGE, vformat("0, %d, 1", AnimatedTexture::MAX_FRAMES)), 0));
}

Error ResourceImporterAnimatedTexture::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	// Parse import options.
	int32_t loader_flags = ImageFramesFormatLoader::FLAG_NONE;

	// Processing.
	const bool fix_alpha_border = p_options["process/fix_alpha_border"];
	const bool premult_alpha = p_options["process/premult_alpha"];
	const int size_limit = p_options["process/size_limit"];
	const bool hdr_as_srgb = p_options["process/hdr_as_srgb"];
	if (hdr_as_srgb) {
		loader_flags |= ImageFramesFormatLoader::FLAG_FORCE_LINEAR;
	}
	const int frame_limit = p_options["process/frame_limit"];

	Ref<ImageFrames> image_frames;
	image_frames.instantiate();
	Error err = ImageFramesLoader::load_image_frames(p_source_file, image_frames, Ref<FileAccess>(), loader_flags);
	if (err != OK) {
		return err;
	}

	int frame_count = frame_limit <= 0 ? AnimatedTexture::MAX_FRAMES : frame_limit;
	frame_count = MIN(frame_count, MIN(image_frames->get_frame_count(), AnimatedTexture::MAX_FRAMES));

	Ref<FileAccess> f = FileAccess::open(p_save_path + ".atex", FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, "Cannot create file in path '" + p_save_path + ".atex'.");

	const uint8_t header[4] = { 'R', 'D', 'A', 'T' };
	f->store_buffer(header, 4); // Redot Animated Texture.
	f->store_32(loader_flags);
	f->store_32(frame_count);

	int width = image_frames->get_frame_image(0)->get_width();
	int height = image_frames->get_frame_image(0)->get_height();
	int new_width = width;
	int new_height = height;
	if (size_limit > 0) {
		// Apply the size limit.
		if (width > size_limit || height > size_limit) {
			if (width >= height) {
				new_width = size_limit;
				new_height = height * new_width / width;
			} else {
				new_height = size_limit;
				new_width = width * new_height / height;
			}
		}
	}

	// We already assume image frames already contains at least one frame,
	// and that all frames have the same size.
	f->store_32(new_width);
	f->store_32(new_height);

	for (int current_frame = 0; current_frame < frame_count; current_frame++) {
		Ref<Image> image = image_frames->get_frame_image(current_frame);
		image->convert(Image::FORMAT_RGBA8);
		if (width != new_width || height != new_height) {
			image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
		}

		// Fix alpha border.
		if (fix_alpha_border) {
			image->fix_alpha_edges();
		}

		// Premultiply the alpha.
		if (premult_alpha) {
			image->premultiply_alpha();
		}

		// Frame image data.
		Vector<uint8_t> data = image->get_data();
		f->store_32(data.size());
		f->store_buffer(data.ptr(), data.size());
		// Frame delay data.
		const real_t delay = image_frames->get_frame_delay(current_frame);
		f->store_real(delay);
	}

	return OK;
}

ResourceImporterAnimatedTexture::ResourceImporterAnimatedTexture() {
}
