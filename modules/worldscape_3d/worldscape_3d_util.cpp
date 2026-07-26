/**************************************************************************/
/*  worldscape_3d_util.cpp                                                */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/time.h"

#include "constants.h"
#include "logger.h"
#include "worldscape_3d_util.h"

#include "worldscape_3d.h"

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DUtil::print_arr(const String &p_name, const Array &p_arr, const int p_level) {
	LOG(p_level, "Array[", p_arr.size(), "]: ", p_name);
	for (int i = 0; i < p_arr.size(); i++) {
		Variant var = p_arr[i];
		switch (var.get_type()) {
			case Variant::ARRAY: {
				print_arr(p_name + String::num_int64(i), var, p_level);
				break;
			}
			case Variant::DICTIONARY: {
				print_dict(p_name + String::num_int64(i), var, p_level);
				break;
			}
			case Variant::OBJECT: {
				Object *obj = cast_to<Object>(var);
				String str = "Object#" + String::num_uint64(obj->get_instance_id()) + ", " + ptr_to_str(obj);
				LOG(p_level, i, ": ", str);
				break;
			}
			default: {
				LOG(p_level, i, ": ", p_arr[i]);
				break;
			}
		}
	}
}

void WorldScape3DUtil::print_dict(const String &p_name, const Dictionary &p_dict, const int p_level) {
	LOG(p_level, "Dictionary: ", p_name);
	Array keys = p_dict.keys();
	for (int i = 0; i < keys.size(); i++) {
		Variant var = p_dict[keys[i]];
		switch (var.get_type()) {
			case Variant::ARRAY: {
				print_arr(String(keys[i]), var, p_level);
				break;
			}
			case Variant::DICTIONARY: {
				print_dict(String(keys[i]), var, p_level);
				break;
			}
			case Variant::OBJECT: {
				Object *obj = cast_to<Object>(var);
				String str = "Object#" + String::num_uint64(obj->get_instance_id()) + ", " + ptr_to_str(obj);
				LOG(p_level, "\"", keys[i], "\": ", str);
				break;
			}
			default: {
				LOG(p_level, "\"", keys[i], "\": Value: ", var);
				break;
			}
		}
	}
}

void WorldScape3DUtil::dump_gentex(const GeneratedTexture p_gen, const String &p_name, const int p_level) {
	LOG(p_level, "Generated ", p_name, " RID: ", p_gen.get_rid(), ", dirty: ", p_gen.is_dirty(), ", image: ", p_gen.get_image());
}

void WorldScape3DUtil::dump_maps(const TypedArray<Image> &p_maps, const String &p_name) {
	LOG(DEBUG, "Dumping ", p_name, " map array. Size: ", p_maps.size());
	for (int i = 0; i < p_maps.size(); i++) {
		Ref<Image> img = p_maps[i];
		LOG(DEBUG, "[", i, "]: Map size: ", img->get_size(), " format: ", img->get_format(), " ", img);
	}
}

// Expects a filename in a String like: "terrain-01_02.res" which returns (-1, 2)
Vector2i WorldScape3DUtil::filename_to_location(const String &p_filename) {
	String location_string = p_filename.trim_prefix("terrain").trim_suffix(".res");
	return string_to_location(location_string);
}

// Expects a string formatted as: "±##±##" which returns (##,##)
Vector2i WorldScape3DUtil::string_to_location(const String &p_string) {
	String x_str = p_string.left(3).replace("_", "");
	String y_str = p_string.right(3).replace("_", "");
	if (!x_str.is_valid_int() || !y_str.is_valid_int()) {
		LOG(ERROR, "Malformed string '", p_string, "'. Result: ", x_str, ", ", y_str);
		return V2I_MAX;
	}
	return Vector2i(x_str.to_int(), y_str.to_int());
}

// Expects a v2i(-1,2) and returns terrain-01_02.res
String WorldScape3DUtil::location_to_filename(const Vector2i &p_region_loc) {
	return "terrain" + location_to_string(p_region_loc) + ".res";
}

// Expects a v2i(-1,2) and returns -01_02
String WorldScape3DUtil::location_to_string(const Vector2i &p_region_loc) {
	const String POS_REGION_FORMAT = "_%02d";
	const String NEG_REGION_FORMAT = "%03d";
	String x_str, y_str;
	x_str = vformat((p_region_loc.x >= 0) ? POS_REGION_FORMAT : NEG_REGION_FORMAT, p_region_loc.x);
	y_str = vformat((p_region_loc.y >= 0) ? POS_REGION_FORMAT : NEG_REGION_FORMAT, p_region_loc.y);
	return x_str + y_str;
}

PackedStringArray WorldScape3DUtil::get_files(const String &p_dir, const String &p_glob) {
	PackedStringArray files;
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		LOG(ERROR, "Cannot open directory: ", p_dir);
		return files;
	}
	PackedStringArray dir_files = da->get_files();
	for (int i = 0; i < dir_files.size(); i++) {
		String fname = dir_files[i].trim_suffix(".remap");
		if (!fname.matchn(p_glob)) {
			continue;
		}
		LOG(DEBUG, "Found file: ", p_dir + String("/") + fname);
		files.push_back(fname);
	}
	return files;
}

Ref<Image> WorldScape3DUtil::black_to_alpha(const Ref<Image> &p_image) {
	if (p_image.is_null()) {
		return Ref<Image>();
	}
	Ref<Image> img = Image::create_empty(p_image->get_width(), p_image->get_height(), false, Image::FORMAT_RGBAF);
	for (int y = 0; y < img->get_height(); y++) {
		for (int x = 0; x < img->get_width(); x++) {
			Color pixel = p_image->get_pixel(x, y);
			pixel.a = pixel.get_luminance();
			img->set_pixel(x, y, pixel);
		}
	}
	if (p_image->has_mipmaps()) {
		img->generate_mipmaps();
	}
	return img;
}

/**
 * Returns the minimum and maximum values for a heightmap (red channel only)
 */
Vector2 WorldScape3DUtil::get_min_max(const Ref<Image> &p_image) {
	if (p_image.is_null()) {
		LOG(ERROR, "Provided image is not valid. Nothing to analyze");
		return Vector2(INFINITY, INFINITY);
	} else if (p_image->is_empty()) {
		LOG(ERROR, "Provided image is empty. Nothing to analyze");
		return Vector2(INFINITY, INFINITY);
	}

	Vector2 min_max = Vector2(FLT_MAX, FLT_MIN);

	for (int y = 0; y < p_image->get_height(); y++) {
		for (int x = 0; x < p_image->get_width(); x++) {
			Color col = p_image->get_pixel(x, y);
			if (col.r < min_max.x) {
				min_max.x = col.r;
			}
			if (col.r > min_max.y) {
				min_max.y = col.r;
			}
		}
	}

	LOG(INFO, "Calculating minimum and maximum values of the image: ", min_max);
	return min_max;
}

/**
 * Returns a Image of a float heightmap normalized to RGB8 grayscale and scaled
 * Minimum of 8x8
 */
Ref<Image> WorldScape3DUtil::get_thumbnail(const Ref<Image> &p_image, const Vector2i &p_size) {
	if (p_image.is_null()) {
		LOG(ERROR, "Provided image is not valid. Nothing to process");
		return Ref<Image>();
	} else if (p_image->is_empty()) {
		LOG(ERROR, "Provided image is empty. Nothing to process");
		return Ref<Image>();
	}
	Vector2i size = Vector2i(CLAMP(p_size.x, 8, 16384), CLAMP(p_size.y, 8, 16384));

	LOG(INFO, "Drawing a thumbnail sized: ", size);
	// Create a temporary work image scaled to desired width
	Ref<Image> img;
	img.instantiate();
	img->copy_from(p_image);
	img->resize(size.x, size.y, Image::INTERPOLATE_LANCZOS);

	// Get minimum and maximum height values on the scaled image
	Vector2 minmax = get_min_max(img);
	real_t hmin = minmax.x;
	real_t hmax = minmax.y;
	// Define maximum range
	hmin = abs(hmin);
	hmax = abs(hmax) + hmin;
	// Avoid divide by zero
	hmax = (hmax == 0) ? 0.001f : hmax;

	// Create a new image w / normalized values
	Ref<Image> thumb = Image::create_empty(size.x, size.y, false, Image::FORMAT_RGB8);
	for (int y = 0; y < thumb->get_height(); y++) {
		for (int x = 0; x < thumb->get_width(); x++) {
			Color col = img->get_pixel(x, y);
			col.r = (col.r + hmin) / hmax;
			col.g = col.r;
			col.b = col.r;
			thumb->set_pixel(x, y, col);
		}
	}
	return thumb;
}

/* Get an Image filled with specified color and format
 * If p_color.a < 0, fill with checkered pattern multiplied by p_color.rgb
 *
 * Behavior changes if a compressed format is requested:
 * If the editor is running and format is DXT1/5, BPTC_RGBA, it returns a filled image.
 * Otherwise, it returns a blank image in that format.
 *
 * The reason is the Image compression library is available only in the editor. And it is
 * unreliable, offering little control over the output format, choosing automatically and
 * often wrong. We have selected a few compressed formats it gets right.
 */
Ref<Image> WorldScape3DUtil::get_filled_image(const Vector2i &p_size, const Color &p_color,
		const bool p_create_mipmaps, const Image::Format p_format) {
	Image::Format format = p_format;
	if (format < 0 || format >= Image::FORMAT_MAX) {
		format = Image::FORMAT_DXT5;
	}

	Image::CompressMode compression_format = Image::COMPRESS_MAX;
	Image::UsedChannels channels = Image::USED_CHANNELS_RGBA;
	bool compress = false;
	bool fill_image = true;

	if (format >= Image::Format::FORMAT_DXT1) {
		switch (format) {
			case Image::FORMAT_DXT1:
				format = Image::FORMAT_RGB8;
				channels = Image::USED_CHANNELS_RGB;
				compression_format = Image::COMPRESS_S3TC;
				compress = true;
				break;
			case Image::FORMAT_DXT5:
				format = Image::FORMAT_RGBA8;
				channels = Image::USED_CHANNELS_RGBA;
				compression_format = Image::COMPRESS_S3TC;
				compress = true;
				break;
			case Image::FORMAT_BPTC_RGBA:
				format = Image::FORMAT_RGBA8;
				channels = Image::USED_CHANNELS_RGBA;
				compression_format = Image::COMPRESS_BPTC;
				compress = true;
				break;
			default:
				compress = false;
				fill_image = false;
				break;
		}
	}

	Ref<Image> img = Image::create_empty(p_size.x, p_size.y, p_create_mipmaps, format);

	Color color = p_color;
	if (fill_image) {
		if (color.a < 0.0f) {
			color.a = 1.0f;
			Color col_a = Color(0.8f, 0.8f, 0.8f, 1.0) * color;
			Color col_b = Color(0.5f, 0.5f, 0.5f, 1.0) * color;
			img->fill_rect(Rect2i(V2I_ZERO, p_size / 2), col_a);
			img->fill_rect(Rect2i(p_size / 2, p_size / 2), col_a);
			img->fill_rect(Rect2i(Vector2(p_size.x, 0) / 2, p_size / 2), col_b);
			img->fill_rect(Rect2i(Vector2(0, p_size.y) / 2, p_size / 2), col_b);
		} else {
			img->fill(color);
		}
		if (p_create_mipmaps) {
			img->generate_mipmaps();
		}
	}
#ifdef TOOLS_ENABLED
	if (compress && IS_EDITOR) {
		img->compress_from_channels(compression_format, channels);
	}
#endif
	return img;
}

/**
 * Loads a file from disk and returns an Image
 * Parameters:
 *	p_filename - file on disk to load. EXR, R16/RAW, PNG, or a ResourceLoader format (jpg, res, tres, etc)
 *	p_cache_mode - Send this flag to the resource loader to force caching or not
 *	p_height_range - R16 format: x=Min & y=Max value ranges. Required for R16 import
 *	p_size - R16 format: Image dimensions. Default (0,0) auto detects f/ square images. Required f/ non-square R16
 */
Ref<Image> WorldScape3DUtil::load_image(const String &p_file_name, const int p_cache_mode, const Vector2 &p_r16_height_range, const Vector2i &p_r16_size) {
	if (p_file_name.is_empty()) {
		LOG(ERROR, "No file specified. Nothing imported");
		return Ref<Image>();
	}
	if (!FileAccess::exists(p_file_name)) {
		LOG(ERROR, "File ", p_file_name, " does not exist. Nothing to import");
		return Ref<Image>();
	}

	// Load file based on extension
	Ref<Image> img;
	LOG(INFO, "Attempting to load: ", p_file_name);
	String ext = p_file_name.get_extension().to_lower();
	PackedStringArray imgloader_extensions{ "bmp", "dds", "exr", "hdr", "jpg", "jpeg", "png", "tga", "svg", "webp" };

	// If R16 integer format (read/writable by Krita)
	if (ext == "r16" || ext == "raw") {
		LOG(DEBUG, "Loading file as an r16");
		Ref<FileAccess> file = FileAccess::open(p_file_name, FileAccess::READ);
		// If p_size is zero, assume square and try to auto detect size
		Vector2i r16_size = p_r16_size;
		if (r16_size <= V2I_ZERO) {
			file->seek_end();
			int fsize = file->get_position();
			int fwidth = sqrt(fsize / 2);
			r16_size = Vector2i(fwidth, fwidth);
			LOG(DEBUG, "Total file size is: ", fsize, " calculated width: ", fwidth, " dimensions: ", r16_size);
			file->seek(0);
		}
		img = Image::create_empty(r16_size.x, r16_size.y, false, FORMAT[TYPE_HEIGHT]);
		for (int y = 0; y < r16_size.y; y++) {
			for (int x = 0; x < r16_size.x; x++) {
				real_t h = real_t(file->get_16()) / 65535.0f;
				h = h * (p_r16_height_range.y - p_r16_height_range.x) + p_r16_height_range.x;
				img->set_pixel(x, y, Color(h, 0.f, 0.f));
			}
		}

		// If an Image extension, use Image loader
	} else if (imgloader_extensions.has(ext)) {
		LOG(DEBUG, "ImageFormatLoader loading recognized file type: ", ext);
		img = Image::load_from_file(p_file_name);

		// Else, see if Godot's resource loader will read it as an image: RES, TRES, etc
	} else {
		LOG(DEBUG, "Loading file as a resource");
		img = ResourceLoader::load(p_file_name, "", static_cast<ResourceFormatLoader::CacheMode>(p_cache_mode));
	}

	if (!img.is_valid()) {
		LOG(ERROR, "File", p_file_name, " cannot be loaded");
		return Ref<Image>();
	}
	if (img->is_empty()) {
		LOG(ERROR, "File", p_file_name, " is empty");
		return Ref<Image>();
	}
	LOG(DEBUG, "Loaded Image size: ", img->get_size(), " format: ", img->get_format());
	return img;
}

/* From source RGB and selected source for Alpha channel, create a new RGBA image.
 * If p_invert_green is true, the destination green channel will be 1.0 - input green channel.
 * If p_invert_alpha is true, the destination alpha channel will be 1.0 - input source channel.
 */
Ref<Image> WorldScape3DUtil::pack_image(const Ref<Image> &p_src_rgb, const Ref<Image> &p_src_a,
		const bool p_invert_green, const bool p_invert_alpha, const bool p_normalize_alpha, const int p_alpha_channel) {
	if (!p_src_rgb.is_valid() || !p_src_a.is_valid()) {
		LOG(ERROR, "Provided images are not valid. Cannot pack");
		return Ref<Image>();
	}
	if (p_src_rgb->get_size() != p_src_a->get_size()) {
		LOG(ERROR, "Provided images are not the same size. Cannot pack");
		return Ref<Image>();
	}
	if (p_src_rgb->is_empty() || p_src_a->is_empty()) {
		LOG(ERROR, "Provided images are empty. Cannot pack");
		return Ref<Image>();
	}
	if (p_alpha_channel < 0 || p_alpha_channel > 3) {
		LOG(ERROR, "Source Channel of Height/Roughness invalid. Cannot Pack");
		return Ref<Image>();
	}

	real_t a_max = 0.0f;
	real_t a_min = 0.0f;
	real_t contrast = 1.0f;
	if (p_normalize_alpha) {
		a_min = 1.0f;
		// Calculate contrast and offset so that we can make full use of the height channel range.
		for (int y = 0; y < p_src_rgb->get_height(); y++) {
			for (int x = 0; x < p_src_rgb->get_width(); x++) {
				real_t h = p_src_a->get_pixel(x, y)[p_alpha_channel];
				a_max = MAX(h, a_max);
				a_min = MIN(h, a_min);
			}
		}
		contrast /= MAX(a_max - a_min, 1e-6f);
	}

	Ref<Image> dst = Image::create_empty(p_src_rgb->get_width(), p_src_rgb->get_height(), false, Image::FORMAT_RGBA8);
	LOG(INFO, "Creating image from source RGB + source channel images");
	for (int y = 0; y < p_src_rgb->get_height(); y++) {
		for (int x = 0; x < p_src_rgb->get_width(); x++) {
			Color col = p_src_rgb->get_pixel(x, y);
			col.a = p_src_a->get_pixel(x, y)[p_alpha_channel];
			if (p_normalize_alpha) {
				col.a = CLAMP((col.a * contrast - a_min), 0.0f, 1.0f);
			}
			if (p_invert_green) {
				col.g = 1.0f - col.g;
			}
			if (p_invert_alpha) {
				col.a = 1.0f - col.a;
			}
			dst->set_pixel(x, y, col);
		}
	}
	return dst;
}

// From source RGB, create a new L image that is scaled to use full 0 - 1 range.
Ref<Image> WorldScape3DUtil::luminance_to_height(const Ref<Image> &p_src_rgb) {
	if (!p_src_rgb.is_valid()) {
		LOG(ERROR, "Provided images are not valid. Cannot pack");
		return Ref<Image>();
	}
	if (p_src_rgb->is_empty()) {
		LOG(ERROR, "Provided images are empty. Cannot pack");
		return Ref<Image>();
	}
	real_t lum_contrast;
	real_t l_max = 0.0f;
	real_t l_min = 1.0f;
	// Calculate contrast and offset so that we can make the most use of the height channel range.
	for (int y = 0; y < p_src_rgb->get_height(); y++) {
		for (int x = 0; x < p_src_rgb->get_width(); x++) {
			Color col = p_src_rgb->get_pixel(x, y);
			real_t l = 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
			l_max = MAX(l, l_max);
			l_min = MIN(l, l_min);
		}
	}
	lum_contrast = 1.0f / MAX(l_max - l_min, 1e-6);
	Ref<Image> dst = Image::create_empty(p_src_rgb->get_width(), p_src_rgb->get_height(), false, Image::FORMAT_RGB8);
	for (int y = 0; y < p_src_rgb->get_height(); y++) {
		for (int x = 0; x < p_src_rgb->get_width(); x++) {
			Color col = p_src_rgb->get_pixel(x, y);
			real_t lum = 0.299f * col.r + 0.587f * col.g + 0.114f * col.b;
			lum = CLAMP((lum * lum_contrast - l_min), 0.0f, 1.0f);
			// some shaping
			col.r = 0.5f - sin(asin(1.0f - 2.0f * lum) / 3.0f);
			col.g = col.r;
			col.b = col.r;
			col.a = col.r;
			dst->set_pixel(x, y, col);
		}
	}
	return dst;
}

void WorldScape3DUtil::benchmark(WorldScape3D *p_terrain) {
	if (!p_terrain) {
		return;
	}
	WorldScape3DData *data = p_terrain->get_data();
	if (!data) {
		return;
	}
	uint64_t start_time;
	Vector3 vec;
	Color col;
	for (int i = 0; i < 3; i++) {
		start_time = Time::get_singleton()->get_ticks_msec();
		for (int j = 0; j < 10000000; j++) {
			col = data->get_pixel(TYPE_HEIGHT, vec);
		}
		LOG(MESG, "get_pixel() 10M: ", Time::get_singleton()->get_ticks_msec() - start_time, "ms");
	}

	vec = Vector3(0.5f, 0.f, 0.5f);
	for (int i = 0; i < 3; i++) {
		start_time = Time::get_singleton()->get_ticks_msec();
		for (int j = 0; j < 1000000; j++) {
			data->get_height(vec);
		}
		LOG(MESG, "get_height() 1M interpolated: ", Time::get_singleton()->get_ticks_msec() - start_time, "ms");
	}

	for (int i = 0; i < 2; i++) {
		start_time = Time::get_singleton()->get_ticks_msec();
		p_terrain->bake_mesh(0);
		LOG(MESG, "Bake ArrayMesh: ", Time::get_singleton()->get_ticks_msec() - start_time, "ms");
	}
}
