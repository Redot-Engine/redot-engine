/**************************************************************************/
/*  worldscape_3d_texture_asset.h                                         */
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

#pragma once

#include "scene/resources/texture.h"

#include "constants.h"
#include "worldscape_3d_asset_resource.h"

class WorldScape3DTextureAsset : public WorldScape3DAssetResource {
	GDCLASS(WorldScape3DTextureAsset, WorldScape3DAssetResource);
	CLASS_NAME();
	friend class WorldScape3DAssets;

	Color _albedo_color = Color(1.f, 1.f, 1.f, 1.f);
	Ref<Texture2D> _albedo_texture;
	Ref<Texture2D> _normal_texture;
	real_t _normal_depth = 0.5f;
	real_t _ao_strength = 0.5f;
	real_t _roughness = 0.f;
	real_t _uv_scale = 0.1f;
	//bool _vertical_projection = false;
	real_t _detiling_rotation = 0.0f;
	real_t _detiling_shift = 0.0f;

	bool _is_valid_format(const Ref<Texture2D> &p_texture) const;

public:
	WorldScape3DTextureAsset() { WorldScape3DTextureAsset::clear(); }
	~WorldScape3DTextureAsset() override = default;

	void clear() override;

	void set_name(const String &p_name) override;
	String get_name() const override { return _name; }

	void set_id(const int p_new_id) override;
	int get_id() const override { return _id; }

	void set_albedo_color(const Color &p_color);
	Color get_albedo_color() const { return _albedo_color; }

	void set_albedo_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_albedo_texture() const { return _albedo_texture; }

	void set_normal_texture(const Ref<Texture2D> &p_texture);
	Ref<Texture2D> get_normal_texture() const { return _normal_texture; }

	void set_normal_depth(const real_t p_normal_depth);
	real_t get_normal_depth() const { return _normal_depth; }

	void set_ao_strength(const real_t p_ao_strength);
	real_t get_ao_strength() const { return _ao_strength; }

	void set_roughness(const real_t p_roughness);
	real_t get_roughness() const { return _roughness; }

	void set_uv_scale(const real_t p_scale);
	real_t get_uv_scale() const { return _uv_scale; }

	// void set_vertical_projection(const bool p_projection);
	// bool get_vertical_projection() const { return _vertical_projection; }

	void set_detiling_rotation(const real_t p_detiling_rotation);
	real_t get_detiling_rotation() const { return _detiling_rotation; }

	void set_detiling_shift(const real_t p_detiling_shift);
	real_t get_detiling_shift() const { return _detiling_shift; }

protected:
	static void _bind_methods();
};
