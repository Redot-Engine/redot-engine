/**************************************************************************/
/*  resource_importer_sprite_frames.cpp                                   */
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

#include "resource_importer_sprite_frames.h"

#include "core/config/project_settings.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "core/io/image_frames.h"
#include "core/io/image_frames_loader.h"
#include "core/io/image_loader.h"
#include "core/io/resource_importer.h"
#include "core/templates/vector.h"
#include "core/typedefs.h"
#include "editor/import/resource_importer_texture.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "scene/resources/compressed_texture.h"
#include "scene/resources/sprite_frames.h"

String ResourceImporterSpriteFrames::get_importer_name() const {
	return "sprite_frames";
}

String ResourceImporterSpriteFrames::get_visible_name() const {
	return "SpriteFrames";
}

void ResourceImporterSpriteFrames::get_recognized_extensions(List<String> *p_extensions) const {
	ImageFramesLoader::get_recognized_extensions(p_extensions);
}

String ResourceImporterSpriteFrames::get_save_extension() const {
	return "csfm";
}

String ResourceImporterSpriteFrames::get_resource_type() const {
	return "SpriteFrames";
}

bool ResourceImporterSpriteFrames::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	if (p_option == "compress/high_quality" || p_option == "compress/hdr_compression") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED) {
			return false;
		}

	} else if (p_option == "compress/lossy_quality") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode != ResourceImporterTexture::COMPRESS_LOSSY) {
			return false;
		}

	} else if (p_option == "compress/hdr_mode") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode < ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED) {
			return false;
		}

	} else if (p_option == "compress/normal_map") {
		int compress_mode = int(p_options["compress/mode"]);
		if (compress_mode == ResourceImporterTexture::COMPRESS_LOSSLESS) {
			return false;
		}

	} else if (p_option == "mipmaps/limit") {
		return p_options["mipmaps/generate"];

	} else if (p_option == "compress/uastc_level" || p_option == "compress/rdo_quality_loss") {
		return int(p_options["compress/mode"]) == ResourceImporterTexture::COMPRESS_BASIS_UNIVERSAL;
	}

	return true;
}

int ResourceImporterSpriteFrames::get_preset_count() const {
	return 0;
}

String ResourceImporterSpriteFrames::get_preset_name(int p_idx) const {
	return String();
}

static void _remap_channels(Ref<Image> &r_image, ResourceImporterTexture::ChannelRemap p_options[4]) {
	bool attempted_hdr_inverted = false;

	if (r_image->get_format() >= Image::FORMAT_RF && r_image->get_format() <= Image::FORMAT_RGBE9995) {
		// Formats which can hold HDR data cannot be inverted the same way as unsigned normalized ones (1.0 - channel).
		for (int i = 0; i < 4; i++) {
			switch (p_options[i]) {
				case ResourceImporterTexture::REMAP_INV_R:
					attempted_hdr_inverted = true;
					p_options[i] = ResourceImporterTexture::REMAP_R;
					break;
				case ResourceImporterTexture::REMAP_INV_G:
					attempted_hdr_inverted = true;
					p_options[i] = ResourceImporterTexture::REMAP_G;
					break;
				case ResourceImporterTexture::REMAP_INV_B:
					attempted_hdr_inverted = true;
					p_options[i] = ResourceImporterTexture::REMAP_B;
					break;
				case ResourceImporterTexture::REMAP_INV_A:
					attempted_hdr_inverted = true;
					p_options[i] = ResourceImporterTexture::REMAP_A;
					break;
				default:
					break;
			}
		}
	}

	if (attempted_hdr_inverted) {
		WARN_PRINT("Attempted to use an inverted channel remap on an HDR image. The remap has been changed to its uninverted equivalent.");
	}

	if (p_options[0] == ResourceImporterTexture::REMAP_R && p_options[1] == ResourceImporterTexture::REMAP_G && p_options[2] == ResourceImporterTexture::REMAP_B && p_options[3] == ResourceImporterTexture::REMAP_A) {
		// Default color map, do nothing.
		return;
	}

	for (int x = 0; x < r_image->get_width(); x++) {
		for (int y = 0; y < r_image->get_height(); y++) {
			Color src = r_image->get_pixel(x, y);
			Color dst;

			for (int i = 0; i < 4; i++) {
				switch (p_options[i]) {
					case ResourceImporterTexture::REMAP_R:
						dst[i] = src.r;
						break;
					case ResourceImporterTexture::REMAP_G:
						dst[i] = src.g;
						break;
					case ResourceImporterTexture::REMAP_B:
						dst[i] = src.b;
						break;
					case ResourceImporterTexture::REMAP_A:
						dst[i] = src.a;
						break;

					case ResourceImporterTexture::REMAP_INV_R:
						dst[i] = 1.0f - src.r;
						break;
					case ResourceImporterTexture::REMAP_INV_G:
						dst[i] = 1.0f - src.g;
						break;
					case ResourceImporterTexture::REMAP_INV_B:
						dst[i] = 1.0f - src.b;
						break;
					case ResourceImporterTexture::REMAP_INV_A:
						dst[i] = 1.0f - src.a;
						break;

					case ResourceImporterTexture::REMAP_UNUSED:
						// For Alpha the unused value is 1, for other channels it's 0.
						dst[i] = (i == 3) ? 1.0f : 0.0f;
						break;

					case ResourceImporterTexture::REMAP_0:
						dst[i] = 0.0f;
						break;
					case ResourceImporterTexture::REMAP_1:
						dst[i] = 1.0f;
						break;

					default:
						break;
				}
			}

			r_image->set_pixel(x, y, dst);
		}
	}
}

static void _clamp_hdr_exposure(Ref<Image> &r_image) {
	// Clamp HDR exposure following Filament's tonemapping formula.
	// This can be used to reduce fireflies in environment maps or reduce the influence
	// of the sun from an HDRI panorama on environment lighting (when a DirectionalLight3D is used instead).
	const int height = r_image->get_height();
	const int width = r_image->get_width();

	// These values are chosen arbitrarily and seem to produce good results with 4,096 samples.
	const float linear = 4096.0;
	const float compressed = 16384.0;

	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			const Color color = r_image->get_pixel(i, j);
			const float luma = color.get_luminance();

			Color clamped_color;
			if (luma <= linear) {
				clamped_color = color;
			} else {
				clamped_color = (color / luma) * ((linear * linear - compressed * luma) / (2 * linear - compressed - luma));
			}

			r_image->set_pixel(i, j, clamped_color);
		}
	}
}

static void _save_sprite_frame(const Ref<Image> &p_image, Ref<FileAccess> p_file, float p_frame_delay, ResourceImporterTexture::CompressMode p_compress_mode, float p_lossy_quality, const Image::BasisUniversalPackerParams &p_basisu_params, Image::CompressMode p_vram_compression, bool p_mipmaps, bool p_streamable, bool p_detect_normal, bool p_force_normal, bool p_srgb_friendly, bool p_force_po2_for_compressed, uint32_t p_limit_mipmap) {
	// Reserved.
	p_file->store_32(0);
	p_file->store_32(0);
	p_file->store_32(0);
	p_file->store_float(p_frame_delay);

	if ((p_compress_mode == ResourceImporterTexture::COMPRESS_LOSSLESS || p_compress_mode == ResourceImporterTexture::COMPRESS_LOSSY) && p_image->get_format() >= Image::FORMAT_RF) {
		p_compress_mode = ResourceImporterTexture::COMPRESS_VRAM_UNCOMPRESSED; //these can't go as lossy
	}

	Ref<Image> image = p_image->duplicate();

	if (p_mipmaps) {
		if (p_force_po2_for_compressed && (p_compress_mode == ResourceImporterTexture::COMPRESS_BASIS_UNIVERSAL || p_compress_mode == ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED)) {
			image->resize_to_po2();
		}

		if (!image->has_mipmaps() || p_force_normal) {
			image->generate_mipmaps(p_force_normal);
		}

	} else {
		image->clear_mipmaps();
	}

	// Optimization: Only check for color channels when compressing as BasisU or VRAM.
	Image::UsedChannels used_channels = Image::USED_CHANNELS_RGBA;

	if (p_compress_mode == ResourceImporterTexture::COMPRESS_BASIS_UNIVERSAL || p_compress_mode == ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED) {
		Image::CompressSource comp_source = Image::COMPRESS_SOURCE_GENERIC;
		if (p_force_normal) {
			comp_source = Image::COMPRESS_SOURCE_NORMAL;
		} else if (p_srgb_friendly) {
			comp_source = Image::COMPRESS_SOURCE_SRGB;
		}

		used_channels = image->detect_used_channels(comp_source);
	}

	ResourceImporterTexture::save_to_ctex_format(p_file, image, p_compress_mode, used_channels, p_vram_compression, p_lossy_quality, p_basisu_params);
}

void ResourceImporterSpriteFrames::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/mode", PROPERTY_HINT_ENUM, "Lossless,Lossy,Basis Universal:4", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "compress/lossy_quality", PROPERTY_HINT_RANGE, "0,1,0.01"), 0.7));

	Image::BasisUniversalPackerParams basisu_params;
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/uastc_level", PROPERTY_HINT_ENUM, "Fastest,Faster,Medium,Slower,Slowest"), basisu_params.uastc_level));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "compress/rdo_quality_loss", PROPERTY_HINT_RANGE, "0,10,0.001,or_greater"), basisu_params.rdo_quality_loss));

	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/normal_map", PROPERTY_HINT_ENUM, "Detect,Enable,Disabled"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "compress/channel_pack", PROPERTY_HINT_ENUM, "sRGB Friendly,Optimized"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "mipmaps/generate", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_UPDATE_ALL_IF_MODIFIED), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "mipmaps/limit", PROPERTY_HINT_RANGE, "-1,256"), -1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/channel_remap/red", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Inverted Red,Inverted Green,Inverted Blue,Inverted Alpha,Unused,Zero,One"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/channel_remap/green", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Inverted Red,Inverted Green,Inverted Blue,Inverted Alpha,Unused,Zero,One"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/channel_remap/blue", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Inverted Red,Inverted Green,Inverted Blue,Inverted Alpha,Unused,Zero,One"), 2));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/channel_remap/alpha", PROPERTY_HINT_ENUM, "Red,Green,Blue,Alpha,Inverted Red,Inverted Green,Inverted Blue,Inverted Alpha,Unused,Zero,One"), 3));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/fix_alpha_border"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/premult_alpha"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_as_srgb"), false));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "process/hdr_clamp_exposure"), false));

	// Maximum bound is the highest allowed value for lossy compression (the lowest common denominator).
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "process/size_limit", PROPERTY_HINT_RANGE, "0,16383,1"), 0));

	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "animation/max_frames", PROPERTY_HINT_RANGE, "0,4096,1"), 0));
	r_options->push_back(ImportOption(PropertyInfo(Variant::FLOAT, "animation/frame_speed_multiplier", PROPERTY_HINT_RANGE, "0.01,100,1"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "animation/loops"), true));
}

Error ResourceImporterSpriteFrames::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	// Parse import options.
	int32_t loader_flags = ImageFormatLoader::FLAG_NONE;

	// Compression.
	ResourceImporterTexture::CompressMode compress_mode = ResourceImporterTexture::CompressMode(int(p_options["compress/mode"]));
	const float lossy = p_options["compress/lossy_quality"];
	const int pack_channels = p_options["compress/channel_pack"];
	const int normal = p_options["compress/normal_map"];

	// Mipmaps.
	const bool mipmaps = p_options["mipmaps/generate"];
	const uint32_t mipmap_limit = mipmaps ? uint32_t(p_options["mipmaps/limit"]) : uint32_t(-1);

	// Processing.
	const int remap_r = p_options["process/channel_remap/red"];
	const int remap_g = p_options["process/channel_remap/green"];
	const int remap_b = p_options["process/channel_remap/blue"];
	const int remap_a = p_options["process/channel_remap/alpha"];
	const bool fix_alpha_border = p_options["process/fix_alpha_border"];
	const bool premult_alpha = p_options["process/premult_alpha"];

	const bool hdr_as_srgb = p_options["process/hdr_as_srgb"];
	const bool hdr_clamp_exposure = p_options["process/hdr_clamp_exposure"];
	int size_limit = p_options["process/size_limit"];
	int max_frames = p_options["animation/max_frames"];
	float frame_speed_multiplier = p_options["animation/frame_speed_multiplier"];
	bool will_loop = p_options["animation/loops"];

	const Image::BasisUniversalPackerParams basisu_params = {
		p_options["compress/uastc_level"],
		p_options["compress/rdo_quality_loss"],
	};

	bool using_fallback_size_limit = false;
	if (size_limit == 0) {
		using_fallback_size_limit = true;
		// If no size limit is defined, use a fallback size limit to prevent textures from looking incorrect or failing to import.
		switch (compress_mode) {
			case ResourceImporterTexture::COMPRESS_LOSSY:
				// Maximum WebP size on either axis.
				size_limit = 16383;
				break;
			case ResourceImporterTexture::COMPRESS_BASIS_UNIVERSAL:
				// Maximum Basis Universal size on either axis.
				size_limit = 16384;
				break;
			default:
				// As of June 2024, no GPU can correctly display a texture larger than 32768 pixels on either axis.
				size_limit = 32768;
				break;
		}
	}

	// Support for texture streaming is not implemented yet.
	const bool stream = false;

	if (hdr_as_srgb) {
		loader_flags |= ImageFormatLoader::FLAG_FORCE_LINEAR;
	}

	Ref<ImageFrames> image_frames;
	image_frames.instantiate();
	Error err = ImageFramesLoader::load_image_frames(p_source_file, image_frames, nullptr, loader_flags, 1.0f, max_frames);
	if (err != OK) {
		return err;
	}

	int frame_count = image_frames->get_frame_count();
	bool detect_normal = normal == 0;
	bool force_normal = normal == 1;
	bool srgb_friendly_pack = pack_channels == 0;

	Ref<FileAccess> f = FileAccess::open(p_save_path + ".csfm", FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_OPEN);

	// Redot Streamable Sprite Frame.
	f->store_8('R');
	f->store_8('S');
	f->store_8('S');
	f->store_8('F');

	Ref<Image> first_frame = image_frames->get_frame_image(0);

	// Current format version.
	f->store_32(SpriteFrames::FORMAT_VERSION);

	// Textures may be resized later, so original size must be saved first.
	f->store_32(first_frame->get_width());
	f->store_32(first_frame->get_height());

	uint32_t flags = 0;
	if (stream) {
		flags |= CompressedTexture2D::FORMAT_BIT_STREAM;
	}
	if (mipmaps) {
		flags |= CompressedTexture2D::FORMAT_BIT_HAS_MIPMAPS;
	}
	if (detect_normal) {
		flags |= CompressedTexture2D::FORMAT_BIT_DETECT_NORMAL;
	}

	f->store_32(flags);
	f->store_32(frame_count);
	f->store_float(frame_speed_multiplier);
	f->store_8(will_loop);

	// Reserved.
	f->store_32(0);
	f->store_32(0);
	f->store_32(0);

	ResourceImporterTexture::ChannelRemap remaps[4] = {
		(ResourceImporterTexture::ChannelRemap)remap_r,
		(ResourceImporterTexture::ChannelRemap)remap_g,
		(ResourceImporterTexture::ChannelRemap)remap_b,
		(ResourceImporterTexture::ChannelRemap)remap_a,
	};

	Array formats_imported;

	const bool can_s3tc_bptc = ResourceImporterTextureSettings::should_import_s3tc_bptc();
	const bool can_etc2_astc = ResourceImporterTextureSettings::should_import_etc2_astc();

	if (compress_mode == ResourceImporterTexture::COMPRESS_VRAM_COMPRESSED) {
		// Add list of formats imported.
		if (can_s3tc_bptc) {
			formats_imported.push_back("s3tc_bptc");
		}
		if (can_etc2_astc) {
			formats_imported.push_back("etc2_astc");
		}
	}

	for (int i = 0; i < frame_count; i++) {
		Ref<Image> target_image = image_frames->get_frame_image(i);
		float target_delay = image_frames->get_frame_delay(i);

		// Apply the size limit.
		if (size_limit > 0 && (target_image->get_width() > size_limit || target_image->get_height() > size_limit)) {
			if (target_image->get_width() >= target_image->get_height()) {
				int new_width = size_limit;
				int new_height = target_image->get_height() * new_width / target_image->get_width();

				if (using_fallback_size_limit) {
					// Only warn if downsizing occurred when the user did not explicitly request it.
					WARN_PRINT(vformat("%s: Texture was downsized on import as its width (%d pixels) exceeded the importable size limit (%d pixels).", p_source_file, target_image->get_width(), size_limit));
				}
				target_image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
			} else {
				int new_height = size_limit;
				int new_width = target_image->get_width() * new_height / target_image->get_height();

				if (using_fallback_size_limit) {
					// Only warn if downsizing occurred when the user did not explicitly request it.
					WARN_PRINT(vformat("%s: Texture was downsized on import as its height (%d pixels) exceeded the importable size limit (%d pixels).", p_source_file, target_image->get_height(), size_limit));
				}
				target_image->resize(new_width, new_height, Image::INTERPOLATE_CUBIC);
			}

			if (normal == 1) {
				target_image->normalize();
			}
		}

		_remap_channels(target_image, remaps);

		// Fix alpha border.
		if (fix_alpha_border) {
			target_image->fix_alpha_edges();
		}

		// Premultiply the alpha.
		if (premult_alpha) {
			target_image->premultiply_alpha();
		}

		// Clamp HDR exposure.
		if (hdr_clamp_exposure) {
			_clamp_hdr_exposure(target_image);
		}

		// Import normally.
		_save_sprite_frame(target_image, f, target_delay, compress_mode, lossy, basisu_params, Image::COMPRESS_S3TC /* This is ignored. */,
				mipmaps, stream, detect_normal, force_normal, srgb_friendly_pack, false, mipmap_limit);
	}

	return OK;
}

ResourceImporterSpriteFrames::ResourceImporterSpriteFrames() {
}
