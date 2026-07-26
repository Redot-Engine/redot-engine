/**************************************************************************/
/*  worldscape_3d_mesh_asset.h                                            */
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

#include "constants.h"
#include "worldscape_3d_asset_resource.h"

#include "scene/3d/visual_instance_3d.h"

using ShadowCasting = GeometryInstance3D::ShadowCastingSetting;
constexpr ShadowCasting SHADOWS_ON = GeometryInstance3D::SHADOW_CASTING_SETTING_ON;
constexpr ShadowCasting SHADOWS_OFF = GeometryInstance3D::SHADOW_CASTING_SETTING_OFF;
constexpr ShadowCasting SHADOWS_ONLY = GeometryInstance3D::SHADOW_CASTING_SETTING_SHADOWS_ONLY;

class WorldScape3DMeshAsset : public WorldScape3DAssetResource {
	GDCLASS(WorldScape3DMeshAsset, WorldScape3DAssetResource);
	CLASS_NAME();
	friend class WorldScape3DAssets;

public:
	enum GenType {
		TYPE_NONE,
		TYPE_TEXTURE_CARD,
		TYPE_MAX,
	};

	static constexpr int MAX_LOD_COUNT = 10;
	static constexpr int SHADOW_LOD_ID = -1; // ID used for the shadow lod in instancer

private:
	// Saved data
	bool _enabled = true;
	Ref<PackedScene> _packed_scene;
	GenType _generated_type = TYPE_NONE;

	real_t _height_offset = 0.f;
	real_t _density = 10.f;
	ShadowCasting _cast_shadows = SHADOWS_ON;
	Ref<Material> _material_override;
	Ref<Material> _material_overlay;
	int _generated_faces = 2;
	Vector2 _generated_size = Vector2(1.f, 1.f);
	int _last_lod = MAX_LOD_COUNT - 1;
	int _last_shadow_lod = MAX_LOD_COUNT - 1;
	int _shadow_impostor = 0;
	PackedFloat32Array _lod_ranges;
	real_t _fade_margin = 0.f;

	// Working data
	TypedArray<Mesh> _meshes;
	Ref<Texture2D> _thumbnail;

	void _clear_lod_ranges();
	static bool _sort_lod_nodes(const Node *a, const Node *b);
	Ref<ArrayMesh> _get_generated_mesh() const;
	Ref<Material> _get_material();

public:
	WorldScape3DMeshAsset();
	~WorldScape3DMeshAsset() override = default;

	void clear() override;
	void set_name(const String &p_name) override;
	String get_name() const override { return _name; }
	void set_id(const int p_new_id) override;
	int get_id() const override { return _id; }
	void set_enabled(const bool p_enabled);
	bool is_enabled() const { return _enabled; }

	void set_scene_file(const Ref<PackedScene> &p_scene_file);
	Ref<PackedScene> get_scene_file() const { return _packed_scene; }
	void set_generated_type(const GenType p_type);
	GenType get_generated_type() const { return _generated_type; }
	Ref<Mesh> get_mesh(const int p_lod = 0) const;
	Ref<Texture2D> get_thumbnail() const { return _thumbnail; }
	void set_height_offset(const real_t p_offset);
	real_t get_height_offset() const { return _height_offset; }
	void set_density(const real_t p_density);
	real_t get_density() const { return _density; }
	void set_cast_shadows(const ShadowCasting p_cast_shadows);
	ShadowCasting get_cast_shadows() const { return _cast_shadows; }
	ShadowCasting get_lod_cast_shadows(const int p_lod_id) const;
	void set_material_override(const Ref<Material> &p_material);
	Ref<Material> get_material_override() const { return _material_override; }
	void set_material_overlay(const Ref<Material> &p_material);
	Ref<Material> get_material_overlay() const { return _material_overlay; }

	void set_generated_faces(const int p_count);
	int get_generated_faces() const { return _generated_faces; }
	void set_generated_size(const Vector2 &p_size);
	Vector2 get_generated_size() const { return _generated_size; }

	int get_lod_count() const { return _meshes.size(); }
	void set_last_lod(const int p_lod);
	int get_last_lod() const { return _last_lod; }
	void set_last_shadow_lod(const int p_lod);
	int get_last_shadow_lod() const { return _last_shadow_lod; }
	void set_shadow_impostor(const int p_lod);
	int get_shadow_impostor() const { return _shadow_impostor; }

	void set_lod_range(const int p_lod, const real_t p_distance);
	real_t get_lod_range(const int p_lod) const;
	real_t get_lod_range_begin(const int p_lod) const;
	real_t get_lod_range_end(const int p_lod) const;
	void set_lod0_range(const real_t p_distance) { set_lod_range(0, p_distance); }
	real_t get_lod0_range() const { return _lod_ranges[0]; }
	void set_lod1_range(const real_t p_distance) { set_lod_range(1, p_distance); }
	real_t get_lod1_range() const { return _lod_ranges[1]; }
	void set_lod2_range(const real_t p_distance) { set_lod_range(2, p_distance); }
	real_t get_lod2_range() const { return _lod_ranges[2]; }
	void set_lod3_range(const real_t p_distance) { set_lod_range(3, p_distance); }
	real_t get_lod3_range() const { return _lod_ranges[3]; }
	void set_lod4_range(const real_t p_distance) { set_lod_range(4, p_distance); }
	real_t get_lod4_range() const { return _lod_ranges[4]; }
	void set_lod5_range(const real_t p_distance) { set_lod_range(5, p_distance); }
	real_t get_lod5_range() const { return _lod_ranges[5]; }
	void set_lod6_range(const real_t p_distance) { set_lod_range(6, p_distance); }
	real_t get_lod6_range() const { return _lod_ranges[6]; }
	void set_lod7_range(const real_t p_distance) { set_lod_range(7, p_distance); }
	real_t get_lod7_range() const { return _lod_ranges[7]; }
	void set_lod8_range(const real_t p_distance) { set_lod_range(8, p_distance); }
	real_t get_lod8_range() const { return _lod_ranges[8]; }
	void set_lod9_range(const real_t p_distance) { set_lod_range(9, p_distance); }
	real_t get_lod9_range() const { return _lod_ranges[9]; }
	void set_fade_margin(const real_t p_fade_margin);
	real_t get_fade_margin() const { return _fade_margin; }

protected:
	void _validate_property(PropertyInfo &p_property) const;
	static void _bind_methods();
};

VARIANT_ENUM_CAST(WorldScape3DMeshAsset::GenType);
