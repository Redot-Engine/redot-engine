/**************************************************************************/
/*  image_frames_loader.cpp                                               */
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

#include "image_frames_loader.h"

void ImageFramesFormatLoader::_bind_methods() {
	BIND_BITFIELD_FLAG(FLAG_NONE);
	BIND_BITFIELD_FLAG(FLAG_FORCE_LINEAR);
}

bool ImageFramesFormatLoader::recognize(const String &p_extension) const {
	List<String> extensions;
	get_recognized_extensions(&extensions);
	for (const String &E : extensions) {
		if (E.nocasecmp_to(p_extension) == 0) {
			return true;
		}
	}

	return false;
}

Error ImageFramesFormatLoaderExtension::load_image_frames(Ref<ImageFrames> p_image_frames, Ref<FileAccess> p_fileaccess, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags, float p_scale, int p_max_frames) {
	Error err = ERR_UNAVAILABLE;
	GDVIRTUAL_CALL(_load_image_frames, p_image_frames, p_fileaccess, p_flags, p_scale, p_max_frames, err);
	return err;
}

void ImageFramesFormatLoaderExtension::get_recognized_extensions(List<String> *p_extension) const {
	PackedStringArray ext;
	if (GDVIRTUAL_CALL(_get_recognized_extensions, ext)) {
		for (int i = 0; i < ext.size(); i++) {
			p_extension->push_back(ext[i]);
		}
	}
}

void ImageFramesFormatLoaderExtension::add_format_loader() {
	ImageFramesLoader::add_image_frames_format_loader(this);
}

void ImageFramesFormatLoaderExtension::remove_format_loader() {
	ImageFramesLoader::remove_image_frames_format_loader(this);
}

void ImageFramesFormatLoaderExtension::_bind_methods() {
	GDVIRTUAL_BIND(_get_recognized_extensions);
	GDVIRTUAL_BIND(_load_image_frames, "image_frames", "fileaccess", "flags", "scale", "max_frames");
	ClassDB::bind_method(D_METHOD("add_format_loader"), &ImageFramesFormatLoaderExtension::add_format_loader);
	ClassDB::bind_method(D_METHOD("remove_format_loader"), &ImageFramesFormatLoaderExtension::remove_format_loader);
}

Error ImageFramesLoader::load_image_frames(const String &p_file, Ref<ImageFrames> p_image_frames, Ref<FileAccess> p_custom, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags, float p_scale, int p_max_frames) {
	ERR_FAIL_COND_V_MSG(p_image_frames.is_null(), ERR_INVALID_PARAMETER, "Can't load image frames: invalid ImageFrames object.");

	Ref<FileAccess> f = p_custom;
	if (f.is_null()) {
		Error err;
		f = FileAccess::open(p_file, FileAccess::READ, &err);
		ERR_FAIL_COND_V_MSG(f.is_null(), err, "Error opening file '" + p_file + "'.");
	}

	String extension = p_file.get_extension();

	for (int i = 0; i < loader.size(); i++) {
		if (!loader[i]->recognize(extension)) {
			continue;
		}
		Error err = loader.write[i]->load_image_frames(p_image_frames, f, p_flags, p_scale);
		if (err != OK) {
			ERR_PRINT("Error loading image frames: " + p_file);
		}

		if (err != ERR_FILE_UNRECOGNIZED) {
			return err;
		}
	}

	return ERR_FILE_UNRECOGNIZED;
}

void ImageFramesLoader::get_recognized_extensions(List<String> *p_extensions) {
	for (int i = 0; i < loader.size(); i++) {
		loader[i]->get_recognized_extensions(p_extensions);
	}
}

Ref<ImageFramesFormatLoader> ImageFramesLoader::recognize(const String &p_extension) {
	for (int i = 0; i < loader.size(); i++) {
		if (loader[i]->recognize(p_extension)) {
			return loader[i];
		}
	}

	return nullptr;
}

Vector<Ref<ImageFramesFormatLoader>> ImageFramesLoader::loader;

void ImageFramesLoader::add_image_frames_format_loader(Ref<ImageFramesFormatLoader> p_loader) {
	loader.push_back(p_loader);
}

void ImageFramesLoader::remove_image_frames_format_loader(Ref<ImageFramesFormatLoader> p_loader) {
	loader.erase(p_loader);
}

void ImageFramesLoader::cleanup() {
	while (loader.size()) {
		remove_image_frames_format_loader(loader[0]);
	}
}

/////////////////

Ref<Resource> ResourceFormatLoaderImageFrames::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		if (r_error) {
			*r_error = ERR_CANT_OPEN;
		}
		return Ref<Resource>();
	}

	uint8_t header[5] = { 0, 0, 0, 0, 0 };
	f->get_buffer(header, 5);

	bool unrecognized = header[0] != 'R' || header[1] != 'D' || header[2] != 'I' || header[3] != 'M' || header[4] != 'F';
	if (unrecognized) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V(Ref<Resource>());
	}

	String extension = f->get_pascal_string();

	int idx = -1;

	for (int i = 0; i < ImageFramesLoader::loader.size(); i++) {
		if (ImageFramesLoader::loader[i]->recognize(extension)) {
			idx = i;
			break;
		}
	}

	if (idx == -1) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		ERR_FAIL_V(Ref<Resource>());
	}

	Ref<ImageFrames> image;
	image.instantiate();

	Error err = ImageFramesLoader::loader.write[idx]->load_image_frames(image, f);

	if (err != OK) {
		if (r_error) {
			*r_error = err;
		}
		return Ref<Resource>();
	}

	if (r_error) {
		*r_error = OK;
	}

	return image;
}

void ResourceFormatLoaderImageFrames::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("image_frames");
}

bool ResourceFormatLoaderImageFrames::handles_type(const String &p_type) const {
	return p_type == "ImageFrames";
}

String ResourceFormatLoaderImageFrames::get_resource_type(const String &p_path) const {
	return p_path.get_extension().to_lower() == "image_frames" ? "ImageFrames" : String();
}
