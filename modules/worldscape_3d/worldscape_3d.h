/**************************************************************************/
/*  worldscape_3d.h                                                       */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#include "scene/3d/node_3d.h"

#include "constants.h"
#include "target_node_3d.h"
#include "worldscape_3d_assets.h"
#include "worldscape_3d_collision.h"
#include "worldscape_3d_data.h"
#include "worldscape_3d_instancer.h"
#include "worldscape_3d_material.h"
#include "worldscape_3d_mesher.h"

class MeshInstance3D;
class SubViewport;
class EditorPlugin;
class WorldScape3DEditor;
class WorldScape3DEditorPlugin;

class WorldScape3D : public Node3D {
	GDCLASS(WorldScape3D, Node3D);
	CLASS_NAME();

public: // Constants
	enum DebugLevel {
		MESG = -2, // Always print except in release builds
		WARN = -1, // Always print except in release builds
		ERROR = 0, // Always print except in release builds
		INFO = 1, // Print every function call and important entries
		DEBUG = 2, // Print details within functions
		EXTREME = 3, // Continuous operations like snapping
	};

	enum RegionSize {
		SIZE_64 = 64,
		SIZE_128 = 128,
		SIZE_256 = 256,
		SIZE_512 = 512,
		SIZE_1024 = 1024,
		SIZE_2048 = 2048,
		SIZE_4096 = 4096
	};

private:
	String _version = "26.3";
	String _data_directory;
	bool _is_inside_world = false;
	bool _initialized = false;
	uint8_t _warnings = 0;

	// Object references
	WorldScape3DData *_data = nullptr;
	Ref<WorldScape3DMaterial> _material;
	Ref<WorldScape3DAssets> _assets;
	WorldScape3DInstancer *_instancer = nullptr;
	WorldScape3DCollision *_collision = nullptr;
	WorldScape3DMesher *_mesher = nullptr;
#ifdef TOOLS_ENABLED
	WorldScape3DEditor *_editor = nullptr;
	WorldScape3DEditorPlugin *_plugin = nullptr;
#endif
	// Current editor or gameplay camera we are centering the terrain on.
	Camera3D *_camera = nullptr;
	uint64_t _camera_instance_id = 0;
	// X,Z Position of the camera during the previous snapping. Set to max real_t value to force a snap update.
	Vector2 _camera_last_position = V2_MAX;

	// Tracked Targets // FIXME not in stable
	// TargetNode3D _clipmap_target;
	// TargetNode3D _collision_target;
	// TargetNode3D _camera; // Fallback target for clipmap and collision

	// Regions
	RegionSize _region_size = SIZE_256;
	bool _save_16_bit = false;
	real_t _label_distance = 0.f;
	int _label_size = 48;

	// Meshes
	int _mesh_lods = 7;
	int _mesh_size = 48;
	real_t _vertex_spacing = 1.0f;
	Vector3 _snapped_position;

	// Rendering
	uint32_t _render_layers = 1 | (1 << 31); // Bit 1 and 32 for the cursor
	RenderingServer::ShadowCastingSetting _cast_shadows = RenderingServer::SHADOW_CASTING_SETTING_ON;
	GeometryInstance3D::GIMode _gi_mode = GeometryInstance3D::GI_MODE_STATIC;
	real_t _cull_margin = 0.0f;
	bool _free_editor_textures = true;

	// Mouse cursor
	SubViewport *_mouse_vp = nullptr;
	Camera3D *_mouse_cam = nullptr;
	MeshInstance3D *_mouse_quad = nullptr;
	uint32_t _mouse_layer = 32;

	// Parent containers for child nodes
	Node3D *_label_parent;
	Node3D *_mmi_parent;

	void _initialize();
	void __physics_process(const double p_delta);
	void _grab_camera();

	void _build_containers();
	void _destroy_containers();
	void _destroy_labels();

	void _destroy_instancer();
	void _destroy_collision(const bool p_final = false);
	void _destroy_mesher(const bool p_final = false);
	void _update_mesher_aabbs() { _mesher ? _mesher->update_aabbs() : void(); }

	void _setup_mouse_picking();
	void _destroy_mouse_picking();

	void _generate_triangles(PackedVector3Array &p_vertices, PackedVector2Array *p_uvs, const int32_t p_lod,
			const WorldScape3DData::HeightFilter p_filter, const bool require_nav, const AABB &p_global_aabb) const;
	void _generate_triangle_pair(PackedVector3Array &p_vertices, PackedVector2Array *p_uvs, const int32_t p_lod,
			const WorldScape3DData::HeightFilter p_filter, const bool require_nav, const int32_t x, const int32_t z) const;

public:
	static DebugLevel debug_level; // Initialized in terrain_3d.cpp

	WorldScape3D();
	~WorldScape3D() override {}
	bool is_inside_world() const { return _is_inside_world; }

	// Terrain
	void set_debug_level(const DebugLevel p_level);
	DebugLevel get_debug_level() const { return debug_level; }
	void set_data_directory(String p_dir);
	String get_data_directory() const { return _data ? _data_directory : ""; }

	// Object references
	WorldScape3DData *get_data() const { return _data; }
	void set_material(const Ref<WorldScape3DMaterial> &p_material);
	Ref<WorldScape3DMaterial> get_material() const { return _material; }
	void set_assets(const Ref<WorldScape3DAssets> &p_assets);
	Ref<WorldScape3DAssets> get_assets() const { return _assets; }
	WorldScape3DCollision *get_collision() const { return _collision; }
	WorldScape3DInstancer *get_instancer() const { return _instancer; }
	Node *get_mmi_parent() const { return _mmi_parent; }
#ifdef TOOLS_ENABLED
	void set_editor(WorldScape3DEditor *p_editor);
	WorldScape3DEditor *get_editor() const { return _editor; }
	void set_plugin(WorldScape3DEditorPlugin *p_plugin);
	WorldScape3DEditorPlugin *get_plugin() const { return _plugin; }
#endif

	void set_camera(Camera3D *p_camera);
	Camera3D *get_camera() const { return _camera; }
	// FIXME not in stable
	// Node3D *get_clipmap_target() const { return _clipmap_target.ptr(); }
	// void set_clipmap_target(Node3D *p_node);
	// Vector3 get_clipmap_target_position() const;
	// Node3D *get_collision_target() const { return _collision_target.ptr(); }
	// void set_collision_target(Node3D *p_node);
	// Vector3 get_collision_target_position() const;
	// void snap();

	// Regions
	void set_region_size(const RegionSize p_size);
	RegionSize get_region_size() const { return _region_size; }
	void change_region_size(const RegionSize p_size) { _data ? _data->change_region_size(p_size) : void(); }
	void set_save_16_bit(const bool p_enabled);
	bool get_save_16_bit() const { return _save_16_bit; }
	void set_label_distance(const real_t p_distance);
	real_t get_label_distance() const { return _label_distance; }
	void set_label_size(const int p_size);
	int get_label_size() const { return _label_size; }
	void update_region_labels();

	// Meshes
	void set_mesh_lods(const int p_count);
	int get_mesh_lods() const { return _mesh_lods; }
	void set_mesh_size(const int p_size);
	int get_mesh_size() const { return _mesh_size; }
	void set_vertex_spacing(const real_t p_spacing);
	real_t get_vertex_spacing() const { return _vertex_spacing; }
	Vector3 get_snapped_position() const { return _snapped_position; }

	// Rendering
	void set_render_layers(const uint32_t p_layers);
	uint32_t get_render_layers() const { return _render_layers; }
	void set_mouse_layer(const uint32_t p_layer);
	uint32_t get_mouse_layer() const { return _mouse_layer; }
	void set_cast_shadows(const RenderingServer::ShadowCastingSetting p_cast_shadows);
	RenderingServer::ShadowCastingSetting get_cast_shadows() const { return _cast_shadows; }
	void set_gi_mode(const GeometryInstance3D::GIMode p_gi_mode);
	GeometryInstance3D::GIMode get_gi_mode() const { return _gi_mode; }
	void set_cull_margin(const real_t p_margin);
	real_t get_cull_margin() const { return _cull_margin; }
	void set_free_editor_textures(const bool p_free_textures) { _free_editor_textures = p_free_textures; }
	bool get_free_editor_textures() const { return _free_editor_textures; }
	void set_show_instances(const bool p_visible) { _mmi_parent ? _mmi_parent->set_visible(p_visible) : void(); }
	bool get_show_instances() const { return _mmi_parent ? _mmi_parent->is_visible() : false; }

	// Utility
	Vector3 get_intersection(const Vector3 &p_src_pos, const Vector3 &p_direction, const bool p_gpu_mode = false);
	//Dictionary get_raycast_result(const Vector3 &p_src_pos, const Vector3 &p_destination, const bool p_exclude_self = true) const;
	Ref<Mesh> bake_mesh(const int p_lod, const WorldScape3DData::HeightFilter p_filter = WorldScape3DData::HEIGHT_FILTER_NEAREST) const;
	PackedVector3Array generate_nav_mesh_source_geometry(const AABB &p_global_aabb, const bool p_require_nav = true) const;

	// Warnings
	void set_warning(const uint8_t p_warning, const bool p_enabled);
	uint8_t get_warnings() const { return _warnings; }
	PackedStringArray get_configuration_warnings() const override;

	// Collision Aliases
	void set_collision_mode(const CollisionMode p_mode) { _collision ? _collision->set_mode(p_mode) : void(); }
	CollisionMode get_collision_mode() const { return _collision ? _collision->get_mode() : CollisionMode::DYNAMIC_GAME; }
	void set_collision_shape_size(const uint16_t p_size) { _collision ? _collision->set_shape_size(p_size) : void(); }
	uint16_t get_collision_shape_size() const { return _collision ? _collision->get_shape_size() : 16; }
	void set_collision_radius(const uint16_t p_radius) { _collision ? _collision->set_radius(p_radius) : void(); }
	uint16_t get_collision_radius() const { return _collision ? _collision->get_radius() : 64; }
	void set_collision_layer(const uint32_t p_layers) { _collision ? _collision->set_layer(p_layers) : void(); }
	uint32_t get_collision_layer() const { return _collision ? _collision->get_layer() : 1; }
	void set_collision_mask(const uint32_t p_mask) { _collision ? _collision->set_mask(p_mask) : void(); }
	uint32_t get_collision_mask() const { return _collision ? _collision->get_mask() : 1; }
	void set_collision_priority(const real_t p_priority) { _collision ? _collision->set_priority(p_priority) : void(); }
	real_t get_collision_priority() const { return _collision ? _collision->get_priority() : 1.f; }
	void set_physics_material(const Ref<PhysicsMaterial> &p_mat) { _collision ? _collision->set_physics_material(p_mat) : void(); }
	Ref<PhysicsMaterial> get_physics_material() const { return _collision ? _collision->get_physics_material() : Ref<PhysicsMaterial>(); }

	// Overlay Aliases
	void set_show_region_grid(const bool p_enabled) { _material.is_valid() ? _material->set_show_region_grid(p_enabled) : void(); }
	bool get_show_region_grid() const { return _material.is_valid() ? _material->get_show_region_grid() : false; }
	void set_show_instancer_grid(const bool p_enabled) { _material.is_valid() ? _material->set_show_instancer_grid(p_enabled) : void(); }
	bool get_show_instancer_grid() const { return _material.is_valid() ? _material->get_show_instancer_grid() : false; }
	void set_show_vertex_grid(const bool p_enabled) { _material.is_valid() ? _material->set_show_vertex_grid(p_enabled) : void(); }
	bool get_show_vertex_grid() const { return _material.is_valid() ? _material->get_show_vertex_grid() : false; }
	void set_show_contours(const bool p_enabled) { _material.is_valid() ? _material->set_show_contours(p_enabled) : void(); }
	bool get_show_contours() const { return _material.is_valid() ? _material->get_show_contours() : false; }
	void set_show_navigation(const bool p_enabled) { _material.is_valid() ? _material->set_show_navigation(p_enabled) : void(); }
	bool get_show_navigation() const { return _material.is_valid() ? _material->get_show_navigation() : false; }

	// Debug View Aliases
	void set_show_checkered(const bool p_enabled) { _material.is_valid() ? _material->set_show_checkered(p_enabled) : void(); }
	bool get_show_checkered() const { return _material.is_valid() ? _material->get_show_checkered() : false; }
	void set_show_grey(const bool p_enabled) { _material.is_valid() ? _material->set_show_grey(p_enabled) : void(); }
	bool get_show_grey() const { return _material.is_valid() ? _material->get_show_grey() : false; }
	void set_show_heightmap(const bool p_enabled) { _material.is_valid() ? _material->set_show_heightmap(p_enabled) : void(); }
	bool get_show_heightmap() const { return _material.is_valid() ? _material->get_show_heightmap() : false; }
	void set_show_jaggedness(const bool p_enabled) { _material.is_valid() ? _material->set_show_jaggedness(p_enabled) : void(); }
	bool get_show_jaggedness() const { return _material.is_valid() ? _material->get_show_jaggedness() : false; }
	void set_show_colormap(const bool p_enabled) { _material.is_valid() ? _material->set_show_colormap(p_enabled) : void(); }
	bool get_show_colormap() const { return _material.is_valid() ? _material->get_show_colormap() : false; }
	void set_show_roughmap(const bool p_enabled) { _material.is_valid() ? _material->set_show_roughmap(p_enabled) : void(); }
	bool get_show_roughmap() const { return _material.is_valid() ? _material->get_show_roughmap() : false; }
	void set_show_control_texture(const bool p_enabled) { _material.is_valid() ? _material->set_show_control_texture(p_enabled) : void(); }
	bool get_show_control_texture() const { return _material.is_valid() ? _material->get_show_control_texture() : false; }
	void set_show_control_angle(const bool p_enabled) { _material.is_valid() ? _material->set_show_control_angle(p_enabled) : void(); }
	bool get_show_control_angle() const { return _material.is_valid() ? _material->get_show_control_angle() : false; }
	void set_show_control_scale(const bool p_enabled) { _material.is_valid() ? _material->set_show_control_scale(p_enabled) : void(); }
	bool get_show_control_scale() const { return _material.is_valid() ? _material->get_show_control_scale() : false; }
	void set_show_control_blend(const bool p_enabled) { _material.is_valid() ? _material->set_show_control_blend(p_enabled) : void(); }
	bool get_show_control_blend() const { return _material.is_valid() ? _material->get_show_control_blend() : false; }
	void set_show_autoshader(const bool p_enabled) { _material.is_valid() ? _material->set_show_autoshader(p_enabled) : void(); }
	bool get_show_autoshader() const { return _material.is_valid() ? _material->get_show_autoshader() : false; }
	void set_show_texture_height(const bool p_enabled) { _material.is_valid() ? _material->set_show_texture_height(p_enabled) : void(); }
	bool get_show_texture_height() const { return _material.is_valid() ? _material->get_show_texture_height() : false; }
	void set_show_texture_normal(const bool p_enabled) { _material.is_valid() ? _material->set_show_texture_normal(p_enabled) : void(); }
	bool get_show_texture_normal() const { return _material.is_valid() ? _material->get_show_texture_normal() : false; }
	void set_show_texture_rough(const bool p_enabled) { _material.is_valid() ? _material->set_show_texture_rough(p_enabled) : void(); }
	bool get_show_texture_rough() const { return _material.is_valid() ? _material->get_show_texture_rough() : false; }

protected:
	void _notification(int p_what);
	static void _bind_methods();
};

VARIANT_ENUM_CAST(WorldScape3D::RegionSize);
VARIANT_ENUM_CAST(WorldScape3D::DebugLevel);

constexpr WorldScape3D::DebugLevel MESG = WorldScape3D::DebugLevel::MESG;
constexpr WorldScape3D::DebugLevel WARN = WorldScape3D::DebugLevel::WARN;
constexpr WorldScape3D::DebugLevel ERROR = WorldScape3D::DebugLevel::ERROR;
constexpr WorldScape3D::DebugLevel INFO = WorldScape3D::DebugLevel::INFO;
constexpr WorldScape3D::DebugLevel DEBUG = WorldScape3D::DebugLevel::DEBUG;
constexpr WorldScape3D::DebugLevel EXTREME = WorldScape3D::DebugLevel::EXTREME;
