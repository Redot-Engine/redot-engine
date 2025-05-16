/**************************************************************************/
/*  image_frames_loader.h                                                 */
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

#include "core/io/file_access.h"
#include "core/io/image_frames.h"
#include "core/io/resource_loader.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/variant/binder_common.h"

class ImageFramesLoader;

class ImageFramesFormatLoader : public RefCounted {
	GDCLASS(ImageFramesFormatLoader, RefCounted);

	friend class ImageFramesLoader;
	friend class ResourceFormatLoaderImageFrames;

public:
	enum LoaderFlags {
		FLAG_NONE = 0,
		FLAG_FORCE_LINEAR = 1,
	};

protected:
	static void _bind_methods();

	virtual Error load_image_frames(Ref<ImageFrames> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags = FLAG_NONE, float p_scale = 1.0, int p_max_frames = 0) = 0;
	virtual void get_recognized_extensions(List<String> *p_extensions) const = 0;
	bool recognize(const String &p_extension) const;

public:
	virtual ~ImageFramesFormatLoader() {}
};

VARIANT_BITFIELD_CAST(ImageFramesFormatLoader::LoaderFlags);

class ImageFramesFormatLoaderExtension : public ImageFramesFormatLoader {
	GDCLASS(ImageFramesFormatLoaderExtension, ImageFramesFormatLoader);

protected:
	static void _bind_methods();

public:
	virtual Error load_image_frames(Ref<ImageFrames> p_image, Ref<FileAccess> p_fileaccess, BitField<ImageFramesFormatLoader::LoaderFlags> p_flags = FLAG_NONE, float p_scale = 1.0, int p_max_frames = 0) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;

	void add_format_loader();
	void remove_format_loader();

	GDVIRTUAL0RC(PackedStringArray, _get_recognized_extensions);
	GDVIRTUAL5R(Error, _load_image_frames, Ref<ImageFrames>, Ref<FileAccess>, BitField<ImageFramesFormatLoader::LoaderFlags>, float, int);
};

class ImageFramesLoader {
	static Vector<Ref<ImageFramesFormatLoader>> loader;
	friend class ResourceFormatLoaderImageFrames;

protected:
public:
	static Error load_image_frames(const String &p_file, Ref<ImageFrames> p_image, Ref<FileAccess> p_custom = Ref<FileAccess>(), BitField<ImageFramesFormatLoader::LoaderFlags> p_flags = ImageFramesFormatLoader::FLAG_NONE, float p_scale = 1.0, int p_max_frames = 0);
	static void get_recognized_extensions(List<String> *p_extensions);
	static Ref<ImageFramesFormatLoader> recognize(const String &p_extension);

	static void add_image_frames_format_loader(Ref<ImageFramesFormatLoader> p_loader);
	static void remove_image_frames_format_loader(Ref<ImageFramesFormatLoader> p_loader);

	static void cleanup();
};

class ResourceFormatLoaderImageFrames : public ResourceFormatLoader {
public:
	virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_REUSE) override;
	virtual void get_recognized_extensions(List<String> *p_extensions) const override;
	virtual bool handles_type(const String &p_type) const override;
	virtual String get_resource_type(const String &p_path) const override;
};
