/**************************************************************************/
/*  worldscape_3d_texture_asset.cpp                                       */
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

#include "core/io/image.h"

#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_texture_asset.h"

///////////////////////////
// Private Functions
///////////////////////////

// Note a null texture is considered a valid format
bool WorldScape3DTextureAsset::_is_valid_format(const Ref<Texture2D> &p_texture) const {
	if (p_texture.is_null()) {
		print_line_rich("Provided texture is null.");
		return true;
	}

	Ref<Image> img = p_texture->get_image();
	Image::Format format = Image::FORMAT_MAX;
	if (img.is_valid()) {
		format = img->get_format();
	}
	if (format < 0 || format >= Image::FORMAT_MAX) {
		print_error("Invalid texture format. See documentation for format specification.");
		return false;
	}

	return true;
}

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DTextureAsset::clear() {
	_name = "New Texture";
	_id = 0;
	_albedo_color = Color(1.0f, 1.0f, 1.0f, 1.0f);
	_albedo_texture.unref();
	_normal_texture.unref();
	_uv_scale = 0.1f;
	//_vertical_projection = false;
	_detiling_rotation = 0.0f;
	_detiling_shift = 0.0f;
}

void WorldScape3DTextureAsset::set_name(const String &p_name) {
	print_line_rich("Setting name: ", p_name);
	_name = p_name;
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_id(const int p_new_id) {
	int old_id = _id;
	_id = CLAMP(p_new_id, 0, WorldScape3DAssets::MAX_TEXTURES);
	LOG(INFO, "Setting texture id: ", _id);
	emit_signal("id_changed", WorldScape3DAssets::TYPE_TEXTURE, old_id, _id);
}

void WorldScape3DTextureAsset::set_albedo_color(const Color &p_color) {
	LOG(INFO, "Setting color: ", p_color);
	_albedo_color = p_color;
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_albedo_texture(const Ref<Texture2D> &p_texture) {
	LOG(INFO, "Setting albedo texture: ", p_texture);
	if (_is_valid_format(p_texture)) {
		_albedo_texture = p_texture;
		if (p_texture.is_valid()) {
			String filename = p_texture->get_path().get_file().get_basename();
			if (_name == "New Texture") {
				_name = filename;
				LOG(INFO, "Naming texture based on filename: ", _name);
			}
			Ref<Image> img = p_texture->get_image();
			if (!img->has_mipmaps()) {
				LOG(WARN, "Texture '", filename, "' has no mipmaps. Change on the Import panel if desired.");
			}
			if (img->get_width() != img->get_height()) {
				LOG(WARN, "Texture '", filename, "' is not square. Mipmaps might have artifacts.");
			}
			if (!is_power_of_2(img->get_width()) || !is_power_of_2(img->get_height())) {
				LOG(WARN, "Texture '", filename, "' size is not power of 2. This is sub-optimal.");
			}
		}
		emit_signal("file_changed");
	}
}

void WorldScape3DTextureAsset::set_normal_texture(const Ref<Texture2D> &p_texture) {
	LOG(INFO, "Setting normal texture: ", p_texture);
	if (_is_valid_format(p_texture)) {
		_normal_texture = p_texture;
		if (p_texture.is_valid()) {
			String filename = p_texture->get_path().get_file().get_basename();
			Ref<Image> img = p_texture->get_image();
			if (!img->has_mipmaps()) {
				LOG(WARN, "Texture '", filename, "' has no mipmaps. Change on the Import panel if desired.");
			}
			if (img->get_width() != img->get_height()) {
				LOG(WARN, "Texture '", filename, "' is not square. Not recommended. Mipmaps might have artifacts.");
			}
			if (!is_power_of_2(img->get_width()) || !is_power_of_2(img->get_height())) {
				LOG(WARN, "Texture '", filename, "' dimensions are not power of 2. This is sub-optimal.");
			}
		}
		emit_signal("file_changed");
	}
}

void WorldScape3DTextureAsset::set_normal_depth(const real_t p_normal_depth) {
	_normal_depth = CLAMP(p_normal_depth, 0.0f, 2.0f);
	LOG(INFO, "Setting normal_depth: ", _normal_depth);
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_ao_strength(const real_t p_ao_strength) {
	_ao_strength = CLAMP(p_ao_strength, 0.0f, 2.0f);
	LOG(INFO, "Setting ao_strength: ", _ao_strength);
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_roughness(const real_t p_roughness) {
	_roughness = CLAMP(p_roughness, -1.0f, 1.0f);
	LOG(INFO, "Setting roughness modifier: ", _roughness);
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_uv_scale(const real_t p_scale) {
	_uv_scale = CLAMP(p_scale, 0.001f, 100.0f);
	LOG(INFO, "Setting uv_scale: ", _uv_scale);
	emit_signal("setting_changed");
}

// void WorldScape3DTextureAsset::set_vertical_projection(const bool p_projection) {
// 	_vertical_projection = p_projection;
// 	LOG(INFO, "Setting uv projection: ", _vertical_projection);
// 	emit_signal("setting_changed");
// }

void WorldScape3DTextureAsset::set_detiling_rotation(const real_t p_detiling_rotation) {
	_detiling_rotation = CLAMP(p_detiling_rotation, 0.0f, 1.0f);
	LOG(INFO, "Setting detiling_rotation: ", _detiling_rotation);
	emit_signal("setting_changed");
}

void WorldScape3DTextureAsset::set_detiling_shift(const real_t p_detiling_shift) {
	_detiling_shift = CLAMP(p_detiling_shift, 0.0f, 1.0f);
	LOG(INFO, "Setting detiling_shift: ", _detiling_shift);
	emit_signal("setting_changed");
}

///////////////////////////
// Protected Functions
///////////////////////////

void WorldScape3DTextureAsset::_bind_methods() {
	ADD_SIGNAL(MethodInfo("id_changed"));
	ADD_SIGNAL(MethodInfo("file_changed"));
	ADD_SIGNAL(MethodInfo("setting_changed"));

	ClassDB::bind_method(D_METHOD("clear"), &WorldScape3DTextureAsset::clear);
	ClassDB::bind_method(D_METHOD("set_name", "name"), &WorldScape3DTextureAsset::set_name);
	ClassDB::bind_method(D_METHOD("get_name"), &WorldScape3DTextureAsset::get_name);
	ClassDB::bind_method(D_METHOD("set_id", "id"), &WorldScape3DTextureAsset::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &WorldScape3DTextureAsset::get_id);
	ClassDB::bind_method(D_METHOD("set_albedo_color", "color"), &WorldScape3DTextureAsset::set_albedo_color);
	ClassDB::bind_method(D_METHOD("get_albedo_color"), &WorldScape3DTextureAsset::get_albedo_color);
	ClassDB::bind_method(D_METHOD("set_albedo_texture", "texture"), &WorldScape3DTextureAsset::set_albedo_texture);
	ClassDB::bind_method(D_METHOD("get_albedo_texture"), &WorldScape3DTextureAsset::get_albedo_texture);
	ClassDB::bind_method(D_METHOD("set_normal_texture", "texture"), &WorldScape3DTextureAsset::set_normal_texture);
	ClassDB::bind_method(D_METHOD("get_normal_texture"), &WorldScape3DTextureAsset::get_normal_texture);
	ClassDB::bind_method(D_METHOD("set_normal_depth", "normal_depth"), &WorldScape3DTextureAsset::set_normal_depth);
	ClassDB::bind_method(D_METHOD("get_normal_depth"), &WorldScape3DTextureAsset::get_normal_depth);
	ClassDB::bind_method(D_METHOD("set_ao_strength", "ao_strength"), &WorldScape3DTextureAsset::set_ao_strength);
	ClassDB::bind_method(D_METHOD("get_ao_strength"), &WorldScape3DTextureAsset::get_ao_strength);
	ClassDB::bind_method(D_METHOD("set_roughness", "roughness"), &WorldScape3DTextureAsset::set_roughness);
	ClassDB::bind_method(D_METHOD("get_roughness"), &WorldScape3DTextureAsset::get_roughness);
	ClassDB::bind_method(D_METHOD("set_uv_scale", "scale"), &WorldScape3DTextureAsset::set_uv_scale);
	ClassDB::bind_method(D_METHOD("get_uv_scale"), &WorldScape3DTextureAsset::get_uv_scale);
	// ClassDB::bind_method(D_METHOD("set_vertical_projection", "projection"), &WorldScape3DTextureAsset::set_vertical_projection);
	// ClassDB::bind_method(D_METHOD("get_vertical_projection"), &WorldScape3DTextureAsset::get_vertical_projection);
	ClassDB::bind_method(D_METHOD("set_detiling_rotation", "detiling_rotation"), &WorldScape3DTextureAsset::set_detiling_rotation);
	ClassDB::bind_method(D_METHOD("get_detiling_rotation"), &WorldScape3DTextureAsset::get_detiling_rotation);
	ClassDB::bind_method(D_METHOD("set_detiling_shift", "detiling_shift"), &WorldScape3DTextureAsset::set_detiling_shift);
	ClassDB::bind_method(D_METHOD("get_detiling_shift"), &WorldScape3DTextureAsset::get_detiling_shift);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_NONE), "set_id", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "albedo_color", PROPERTY_HINT_COLOR_NO_ALPHA), "set_albedo_color", "get_albedo_color");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "albedo_texture", PROPERTY_HINT_RESOURCE_TYPE, "ImageTexture,CompressedTexture2D"), "set_albedo_texture", "get_albedo_texture");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "normal_texture", PROPERTY_HINT_RESOURCE_TYPE, "ImageTexture,CompressedTexture2D"), "set_normal_texture", "get_normal_texture");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "normal_depth", PROPERTY_HINT_RANGE, "0.0, 2.0"), "set_normal_depth", "get_normal_depth");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "ao_strength", PROPERTY_HINT_RANGE, "0.0, 2.0"), "set_ao_strength", "get_ao_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "roughness", PROPERTY_HINT_RANGE, "-1.0, 1.0"), "set_roughness", "get_roughness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "uv_scale", PROPERTY_HINT_RANGE, "0.001, 2.0, or_greater"), "set_uv_scale", "get_uv_scale");
	//ADD_PROPERTY(PropertyInfo(Variant::BOOL, "vertical_projection", PROPERTY_HINT_NONE), "set_vertical_projection", "get_vertical_projection");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "detiling_rotation", PROPERTY_HINT_RANGE, "0.0, 1.0"), "set_detiling_rotation", "get_detiling_rotation");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "detiling_shift", PROPERTY_HINT_RANGE, "0.0, 1.0"), "set_detiling_shift", "get_detiling_shift");
}
