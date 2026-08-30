/**************************************************************************/
/*  worldscape_3d.cpp                                                     */
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
#include "core/os/os.h"
#include "core/os/time.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/compositor.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/surface_tool.h"
#include "servers/physics_server_3d.h"
#include "servers/rendering_server.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/inspector/editor_preview_plugins.h"
#endif

#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_util.h"
#ifdef TOOLS_ENABLED
#include "editor/worldscape_3d_editor.h"
#endif

// Initialize static member variable
WorldScape3D::DebugLevel WorldScape3D::debug_level{ ERROR };

///////////////////////////
// Private Functions
///////////////////////////

void WorldScape3D::_initialize() {
	LOG(INFO, "Instantiating main subsystems");

	// Make blank objects if needed
	if (!_data) {
		LOG(DEBUG, "Creating blank data object");
		_data = memnew(WorldScape3DData);
	}
	if (_material.is_null()) {
		LOG(DEBUG, "Creating blank material");
		_material.instantiate();
	}
	if (_assets.is_null()) {
		LOG(DEBUG, "Creating blank texture list");
		_assets.instantiate();
	}
	if (!_collision) {
		LOG(DEBUG, "Creating collision manager");
		_collision = memnew(WorldScape3DCollision);
	}
	if (!_instancer) {
		LOG(DEBUG, "Creating instancer");
		_instancer = memnew(WorldScape3DInstancer);
	}
	if (!_mesher) {
		LOG(DEBUG, "Creating mesher");
		_mesher = new WorldScape3DMesher();
	}

	// Connect signals
	// Any region was changed, update region labels
	if (!_data->is_connected("region_map_changed", callable_mp(this, &WorldScape3D::update_region_labels))) {
		LOG(DEBUG, "Connecting _data::region_map_changed signal to set_show_region_locations()");
		_data->connect("region_map_changed", callable_mp(this, &WorldScape3D::update_region_labels));
	}
	// Any region was changed, regenerate collision if enabled
	if (!_data->is_connected("region_map_changed", callable_mp(_collision, &WorldScape3DCollision::build))) {
		LOG(DEBUG, "Connecting _data::region_map_changed signal to build()");
		_data->connect("region_map_changed", callable_mp(_collision, &WorldScape3DCollision::build));
	}
	// Any map was regenerated or regions changed, update material
	if (!_data->is_connected("maps_changed", callable_mp(_material.ptr(), &WorldScape3DMaterial::_update_maps))) {
		LOG(DEBUG, "Connecting _data::maps_changed signal to _material->_update_maps()");
		_data->connect("maps_changed", callable_mp(_material.ptr(), &WorldScape3DMaterial::_update_maps));
	}
	// Height map was regenerated, update aabbs
	if (!_data->is_connected("height_maps_changed", callable_mp(this, &WorldScape3D::_update_mesher_aabbs))) {
		LOG(DEBUG, "Connecting _data::height_maps_changed signal to update_aabbs()");
		_data->connect("height_maps_changed", callable_mp(this, &WorldScape3D::_update_mesher_aabbs));
	}
	// Texture assets changed, update material
	if (!_assets->is_connected("textures_changed", callable_mp(_material.ptr(), &WorldScape3DMaterial::_update_texture_arrays))) {
		LOG(DEBUG, "Connecting _assets.textures_changed to _material->_update_texture_arrays()");
		_assets->connect("textures_changed", callable_mp(_material.ptr(), &WorldScape3DMaterial::_update_texture_arrays));
	}
	// MeshAssets changed, update instancer
	if (!_assets->is_connected("meshes_changed", callable_mp(_instancer, &WorldScape3DInstancer::_update_mmis).bind(V2I_MAX, -1))) {
		LOG(DEBUG, "Connecting _assets.meshes_changed to _instancer->_update_mmis()");
		_assets->connect("meshes_changed", callable_mp(_instancer, &WorldScape3DInstancer::_update_mmis).bind(V2I_MAX, -1));
	}

	// Initialize the system
	if (!_initialized && _is_inside_world && is_inside_tree()) {
		LOG(INFO, "Initializing main subsystems");
		_data->initialize(this);
		_material->initialize(this);
		_assets->initialize(this);
		_collision->initialize(this);
		_instancer->initialize(this);
		_mesher->initialize(this);
		_initialized = true;
		//snap(); // FIXME not in stable
	}
	update_configuration_warnings();
}

/**
 * This is a proxy for _process(delta) called by _notification() due to
 * https://github.com/godotengine/godot-cpp/issues/1022
 */
void WorldScape3D::__physics_process(const double p_delta) {
	if (!_initialized) {
		return;
	}

	// If the game/editor camera is not set, find it
	if (!is_instance_valid(_camera_instance_id, _camera)) {
		LOG(DEBUG, "Camera is null, getting the current one");
		_grab_camera();
	}

	// If camera has moved enough, re-center the terrain on it.
	if (is_instance_valid(_camera_instance_id) && _camera->is_inside_tree()) {
		Vector3 cam_pos = _camera->get_global_position();
		Vector2 cam_pos_2d = Vector2(cam_pos.x, cam_pos.z);
		RenderingServer::get_singleton()->material_set_param(_material->get_material_rid(), "_camera_pos", cam_pos);
		if (_camera_last_position.distance_to(cam_pos_2d) > 0.2f) {
			if (_mesher) {
				_mesher->snap(cam_pos);
			}
			_snapped_position = (cam_pos / _vertex_spacing).floor() * _vertex_spacing;
			_camera_last_position = cam_pos_2d;
			if (_collision && _collision->is_dynamic_mode()) {
				_collision->update();
			}
		}
	}
}

/**
 * If running in the editor, grab the first editor viewport camera.
 * The edited_scene_root is excluded in case the user already has a Camera3D in their scene.
 */
void WorldScape3D::_grab_camera() {
#ifdef TOOLS_ENABLED
	if (IS_EDITOR) {
		_camera = EditorInterface::get_singleton()->get_editor_viewport_3d(0)->get_camera_3d();
		LOG(DEBUG, "Grabbing the first editor viewport camera: ", _camera);
	} else {
#endif
		_camera = get_viewport()->get_camera_3d();
#ifdef TOOLS_ENABLED
	}
#endif
	LOG(DEBUG, "Grabbing the in-game viewport camera: ", _camera);
	if (_camera) {
		_camera_instance_id = _camera->get_instance_id();
	} else {
		_camera_instance_id = 0;
		set_physics_process(false); // disable snapping
		LOG(ERROR, "Cannot find the active camera. Set it manually with WorldScape3D.set_camera(). Stopping _physics_process()");
	}
}

void WorldScape3D::_build_containers() {
	_label_parent = memnew(Node3D);
	_label_parent->set_name("Labels");
	add_child(_label_parent, true);
	_mmi_parent = memnew(Node3D);
	_mmi_parent->set_name("MMI");
	add_child(_mmi_parent, true);
}

void WorldScape3D::_destroy_containers() {
	memdelete_safely(_label_parent);
	memdelete_safely(_mmi_parent);
}

void WorldScape3D::_destroy_labels() {
	Array labels = _label_parent->get_children();
	LOG(DEBUG, "Destroying ", labels.size(), " region labels");
	for (int i = 0; i < labels.size(); i++) {
		Node *label = cast_to<Node>(labels[i]);
		memdelete_safely(label);
	}
}

void WorldScape3D::_destroy_instancer() {
	LOG(INFO, "Destroying Instancer");
	memdelete_safely(_instancer);
}

void WorldScape3D::_destroy_collision(const bool p_final) {
	LOG(INFO, "Destroying Collision");
	if (_collision) {
		_collision->destroy();
	}
	if (p_final) {
		memdelete_safely(_collision);
	}
}

void WorldScape3D::_destroy_mesher(const bool p_final) {
	LOG(INFO, "Destroying GeoMesh");
	if (_mesher) {
		_mesher->destroy();
		if (p_final) {
			delete _mesher;
			_mesher = nullptr;
		}
	}
}

void WorldScape3D::_setup_mouse_picking() {
	if (!is_inside_tree()) {
		LOG(ERROR, "Not inside the tree, skipping mouse setup");
		return;
	}
	LOG(INFO, "Setting up mouse picker and get_intersection viewport, camera & screen quad");
	_mouse_vp = memnew(SubViewport);
	_mouse_vp->set_name("MouseViewport");
	add_child(_mouse_vp, true);
	_mouse_vp->set_size(Vector2i(2, 2));
	_mouse_vp->set_scaling_3d_mode(Viewport::SCALING_3D_MODE_BILINEAR);
	_mouse_vp->set_update_mode(SubViewport::UPDATE_ONCE);
	_mouse_vp->set_handle_input_locally(false);
	_mouse_vp->set_canvas_cull_mask(0);
	_mouse_vp->set_use_hdr_2d(true);
	_mouse_vp->set_default_canvas_item_texture_filter(Viewport::DEFAULT_CANVAS_ITEM_TEXTURE_FILTER_NEAREST);
	_mouse_vp->set_positional_shadow_atlas_size(0);
	_mouse_vp->set_positional_shadow_atlas_quadrant_subdiv(0, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	_mouse_vp->set_positional_shadow_atlas_quadrant_subdiv(1, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	_mouse_vp->set_positional_shadow_atlas_quadrant_subdiv(2, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);
	_mouse_vp->set_positional_shadow_atlas_quadrant_subdiv(3, Viewport::SHADOW_ATLAS_QUADRANT_SUBDIV_DISABLED);

	_mouse_cam = memnew(Camera3D);
	_mouse_cam->set_name("MouseCamera");
	_mouse_vp->add_child(_mouse_cam, true);
	Ref<Environment> env;
	env.instantiate();
	env->set_tonemapper(Environment::TONE_MAPPER_LINEAR);
	_mouse_cam->set_environment(env);
	Ref<Compositor> comp;
	comp.instantiate();
	_mouse_cam->set_compositor(comp);
	_mouse_cam->set_projection(Camera3D::PROJECTION_ORTHOGONAL);
	_mouse_cam->set_size(0.1f);
	_mouse_cam->set_far(100000.f);

	_mouse_quad = memnew(MeshInstance3D);
	_mouse_quad->set_name("MouseQuad");
	_mouse_cam->add_child(_mouse_quad, true);
	Ref<QuadMesh> quad;
	quad.instantiate();
	quad->set_size(Vector2(0.1f, 0.1f));
	_mouse_quad->set_mesh(quad);
	String shader_code = String(
#include "shaders/gpu_depth.glsl"
	);
	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(shader_code);
	Ref<ShaderMaterial> shader_material;
	shader_material.instantiate();
	shader_material->set_shader(shader);
	_mouse_quad->set_surface_override_material(0, shader_material);
	_mouse_quad->set_position(Vector3(0.f, 0.f, -0.5f));

	// Set terrain, terrain shader, mouse camera, and screen quad to mouse layer
	set_mouse_layer(_mouse_layer);
}

void WorldScape3D::_destroy_mouse_picking() {
	LOG(DEBUG, "Freeing mouse_quad");
	memdelete_safely(_mouse_quad);
	LOG(DEBUG, "Freeing mouse_cam");
	memdelete_safely(_mouse_cam);
	LOG(DEBUG, "Freeing mouse_vp");
	memdelete_safely(_mouse_vp);
}

void WorldScape3D::_generate_triangles(PackedVector3Array &p_vertices, PackedVector2Array *p_uvs, const int32_t p_lod,
		const WorldScape3DData::HeightFilter p_filter, const bool p_require_nav, const AABB &p_global_aabb) const {
	ERR_FAIL_COND(_data == nullptr);
	int32_t step = 1 << CLAMP(p_lod, 0, 8);

	// Bake whole mesh, e.g. bake_mesh and painted navigation
	if (!p_global_aabb.has_volume()) {
		int32_t region_size = (int32_t)_region_size;

		TypedArray<Vector2i> region_locations = _data->get_region_locations();
		for (int r = 0; r < region_locations.size(); ++r) {
			Vector2i region_loc = (Vector2i)region_locations[r] * region_size;

			for (int32_t z = region_loc.y; z < region_loc.y + region_size; z += step) {
				for (int32_t x = region_loc.x; x < region_loc.x + region_size; x += step) {
					_generate_triangle_pair(p_vertices, p_uvs, p_lod, p_filter, p_require_nav, x, z);
				}
			}
		}
	} else {
		// Bake within an AABB, e.g. runtime navigation baker
		int32_t z_start = (int32_t)Math::ceil(p_global_aabb.position.z / _vertex_spacing);
		int32_t z_end = (int32_t)Math::floor(p_global_aabb.get_end().z / _vertex_spacing) + 1;
		int32_t x_start = (int32_t)Math::ceil(p_global_aabb.position.x / _vertex_spacing);
		int32_t x_end = (int32_t)Math::floor(p_global_aabb.get_end().x / _vertex_spacing) + 1;

		for (int32_t z = z_start; z < z_end; ++z) {
			for (int32_t x = x_start; x < x_end; ++x) {
				real_t height = _data->get_height(Vector3(x, 0.f, z));
				if (height >= p_global_aabb.position.y && height <= p_global_aabb.get_end().y) {
					_generate_triangle_pair(p_vertices, p_uvs, p_lod, p_filter, p_require_nav, x, z);
				}
			}
		}
	}
}

// Generates two triangles: Top 124, Bottom 143
//		1  __  2
//		  |\ |
//		  | \|
//		3  --  4
// p_vertices is assumed to exist and the destination for data
// p_uvs might not exist, so a pointer is fine
// p_require_nav is false for the runtime baker, which ignores navigation
void WorldScape3D::_generate_triangle_pair(PackedVector3Array &p_vertices, PackedVector2Array *p_uvs,
		const int32_t p_lod, const WorldScape3DData::HeightFilter p_filter, const bool p_require_nav,
		const int32_t x, const int32_t z) const {
	int32_t step = 1 << CLAMP(p_lod, 0, 8);
	Vector3 xz = Vector3(x, 0.0f, z) * _vertex_spacing;
	Vector3 xsz = Vector3(x + step, 0.0f, z) * _vertex_spacing;
	Vector3 xzs = Vector3(x, 0.0f, z + step) * _vertex_spacing;
	Vector3 xszs = Vector3(x + step, 0.0f, z + step) * _vertex_spacing;
	Vector3 v1 = _data->get_mesh_vertex(p_lod, p_filter, xz);
	bool nan1 = std::isnan(v1.y);
	if (nan1) {
		return;
	}
	Vector3 v2 = _data->get_mesh_vertex(p_lod, p_filter, xsz);
	Vector3 v3 = _data->get_mesh_vertex(p_lod, p_filter, xzs);
	Vector3 v4 = _data->get_mesh_vertex(p_lod, p_filter, xszs);
	bool nan2 = std::isnan(v2.y);
	bool nan3 = std::isnan(v3.y);
	bool nan4 = std::isnan(v4.y);
	// If on the region edge, duplicate the edge pixels
	// Check #2 upper right
	if (nan2) {
		v2.y = v1.y;
	}
	// Check #3 lower left
	if (nan3) {
		v3.y = v1.y;
	}
	// Check #4 lower right
	if (nan4) {
		if (!nan2) {
			v4.y = v2.y;
		} else if (!nan3) {
			v4.y = v3.y;
		} else {
			v4.y = v1.y;
		}
	}
	uint32_t ctrl1 = _data->get_control(xz);
	uint32_t ctrl2 = _data->get_control(xsz);
	uint32_t ctrl3 = _data->get_control(xzs);
	uint32_t ctrl4 = _data->get_control(xszs);
	// Holes are only where the control map is valid and the bit is set
	bool hole1 = ctrl1 != UINT32_MAX && is_hole(ctrl1);
	bool hole2 = ctrl2 != UINT32_MAX && is_hole(ctrl2);
	bool hole3 = ctrl3 != UINT32_MAX && is_hole(ctrl3);
	bool hole4 = ctrl4 != UINT32_MAX && is_hole(ctrl4);
	// Navigation is where the control map is valid and the bit is set, or it's the region edge and nav1 is set
	bool nav1 = ctrl1 != UINT32_MAX && is_nav(ctrl1);
	bool nav2 = (ctrl2 != UINT32_MAX && is_nav(ctrl2)) || (nan2 && nav1);
	bool nav3 = (ctrl3 != UINT32_MAX && is_nav(ctrl3)) || (nan3 && nav1);
	bool nav4 = (ctrl4 != UINT32_MAX && is_nav(ctrl4)) || (nan4 && nav1);
	//Bottom 143 triangle
	if (!(hole1 || hole4 || hole3) && (!p_require_nav || (nav1 && nav4 && nav3))) {
		p_vertices.push_back(v1);
		p_vertices.push_back(v4);
		p_vertices.push_back(v3);
		if (p_uvs) {
			p_uvs->push_back(Vector2(v1.x, v1.z));
			p_uvs->push_back(Vector2(v4.x, v4.z));
			p_uvs->push_back(Vector2(v3.x, v3.z));
		}
	}
	// Top 124 triangle
	if (!(hole1 || hole2 || hole4) && (!p_require_nav || (nav1 && nav2 && nav4))) {
		p_vertices.push_back(v1);
		p_vertices.push_back(v2);
		p_vertices.push_back(v4);
		if (p_uvs) {
			p_uvs->push_back(Vector2(v1.x, v1.z));
			p_uvs->push_back(Vector2(v2.x, v2.z));
			p_uvs->push_back(Vector2(v4.x, v4.z));
		}
	}
}

///////////////////////////
// Public Functions
///////////////////////////

WorldScape3D::WorldScape3D() {
	// Process the command line
	List<String> args = OS::get_singleton()->get_cmdline_args();
	for (String arg : args) {
		if (arg.begins_with("--terrain-debug=")) {
			String value = arg.rsplit("=")[1];
			if (value == "ERROR") {
				set_debug_level(ERROR);
			} else if (value == "INFO") {
				set_debug_level(INFO);
			} else if (value == "DEBUG") {
				set_debug_level(DEBUG);
			} else if (value == "EXTREME") {
				set_debug_level(EXTREME);
			}
		}
	}
}

void WorldScape3D::set_debug_level(const DebugLevel p_level) {
	LOG(INFO, "Setting debug level: ", p_level);
	debug_level = CLAMP(p_level, ERROR, EXTREME);
}

void WorldScape3D::set_data_directory(String p_dir) {
	LOG(INFO, "Setting data directory to ", p_dir);
	if (_data_directory != p_dir) {
		if (_data_directory.is_empty() && Util::get_files(p_dir, "terrain*.res").size() == 0) {
			// If _data_directory was empty and now specified, and has no data
			// assume we want to retain the current data
			_data_directory = p_dir;
		} else {
			// Else clear data and if not null, load
			_initialized = false;
			_destroy_labels();
			_destroy_collision();
			_destroy_instancer();
			memdelete_safely(_data);
			_data_directory = p_dir;
			_initialize();
		}
	}
	update_configuration_warnings();
}

void WorldScape3D::set_material(const Ref<WorldScape3DMaterial> &p_material) {
	if (_material != p_material) {
		_initialized = false;
		LOG(INFO, "Setting material");
		_material = p_material;
		_initialize();
		emit_signal("material_changed");
	}
}

void WorldScape3D::set_assets(const Ref<WorldScape3DAssets> &p_assets) {
	if (_assets != p_assets) {
		_initialized = false;
		LOG(INFO, "Setting asset list");
		_assets = p_assets;
		_initialize();
		emit_signal("assets_changed");
	}
}

#ifdef TOOLS_ENABLED
void WorldScape3D::set_editor(WorldScape3DEditor *p_editor) {
	_editor = p_editor;
	if (_material.is_valid()) {
		_material->update();
	}
	LOG(DEBUG, "Set WorldScape3DEditor: ", p_editor);
}
#endif

#ifdef TOOLS_ENABLED
void WorldScape3D::set_plugin(WorldScape3DEditorPlugin *p_plugin) {
	_plugin = p_plugin;
	LOG(DEBUG, "Set EditorPlugin: ", p_plugin);
}
#endif

void WorldScape3D::set_camera(Camera3D *p_camera) {
	if (_camera != p_camera) {
		_camera = p_camera;
		if (!p_camera) {
			LOG(DEBUG, "Received null camera. Calling _grab_camera()");
			_grab_camera();
		} else {
			_camera = p_camera;
			_camera_instance_id = _camera->get_instance_id();
			LOG(DEBUG, "Setting camera: ", _camera);
			_initialize();
			set_physics_process(true); // enable snapping
		}
	}
}

void WorldScape3D::set_region_size(const RegionSize p_size) {
	LOG(INFO, "Setting region size: ", p_size);
	ERR_FAIL_COND(p_size < SIZE_64);
	ERR_FAIL_COND(p_size > SIZE_4096);
	_region_size = p_size;
	if (_data) {
		_data->_region_size = _region_size;
		_data->_region_sizev = Vector2i(_region_size, _region_size);
	}
	if (_material.is_valid()) {
		_material->_update_maps();
	}
}

void WorldScape3D::set_save_16_bit(const bool p_enabled) {
	LOG(INFO, p_enabled);
	_save_16_bit = p_enabled;
}

void WorldScape3D::set_label_distance(const real_t p_distance) {
	real_t distance = CLAMP(p_distance, 0.f, 100000.f);
	LOG(INFO, "Setting region label distance: ", distance);
	if (_label_distance != distance) {
		_label_distance = distance;
		update_region_labels();
	}
}

void WorldScape3D::set_label_size(const int p_size) {
	int size = CLAMP(p_size, 24, 128);
	LOG(INFO, "Setting region label size: ", size);
	if (_label_size != size) {
		_label_size = size;
		update_region_labels();
	}
}

void WorldScape3D::update_region_labels() {
	_destroy_labels();
	if (_label_distance > 0.f && _data) {
		Array region_locations = _data->get_region_locations();
		LOG(DEBUG, "Creating ", region_locations.size(), " region labels");
		for (int i = 0; i < region_locations.size(); i++) {
			Label3D *label = memnew(Label3D);
			String text = region_locations[i];
			label->set_name("Label3D" + text.replace(" ", ""));
			label->set_pixel_size(.001f);
			label->set_billboard_mode(BaseMaterial3D::BILLBOARD_ENABLED);
			label->set_draw_flag(Label3D::FLAG_DOUBLE_SIDED, true);
			label->set_draw_flag(Label3D::FLAG_DISABLE_DEPTH_TEST, true);
			label->set_draw_flag(Label3D::FLAG_FIXED_SIZE, true);
			label->set_text(text);
			label->set_modulate(Color(1.f, 1.f, 1.f, .5f));
			label->set_outline_modulate(Color(0.f, 0.f, 0.f, .5f));
			label->set_font_size(_label_size);
			label->set_outline_size(_label_size / 6);
			label->set_visibility_range_end(_label_distance);
			label->set_visibility_range_end_margin(_label_distance / 10.f);
			label->set_visibility_range_fade_mode(GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF);
			_label_parent->add_child(label, true);
			Vector2i loc = region_locations[i];
			Vector3 pos = Vector3(real_t(loc.x) + .5f, 0.f, real_t(loc.y) + .5f) * _region_size * _vertex_spacing;
			real_t height = _data->get_height(pos);
			pos.y = (std::isnan(height)) ? 0 : height;
			label->set_position(pos);
		}
	}
}

void WorldScape3D::set_mesh_lods(const int p_count) {
	if (_mesh_lods != p_count) {
		LOG(INFO, "Setting mesh levels: ", p_count);
		_mesh_lods = p_count;
		if (_mesher) {
			_mesher->initialize(this);
		}
	}
}

void WorldScape3D::set_mesh_size(const int p_size) {
	if (_mesh_size != p_size) {
		LOG(INFO, "Setting mesh size: ", p_size);
		_mesh_size = p_size;
		if (_mesher && _material.is_valid()) {
			_material->_update_maps();
			_mesher->initialize(this);
		}
	}
}

void WorldScape3D::set_vertex_spacing(const real_t p_spacing) {
	real_t spacing = CLAMP(p_spacing, 0.25f, 100.0f);
	if (_vertex_spacing != spacing) {
		_vertex_spacing = spacing;
		LOG(INFO, "Setting vertex spacing: ", _vertex_spacing);
		if (_collision && _data && _instancer && _material.is_valid()) {
			_data->_vertex_spacing = _vertex_spacing;
			update_region_labels();
			_instancer->_update_vertex_spacing(_vertex_spacing);
			_camera_last_position = V2_MAX;
			_material->_update_maps();
			_collision->destroy();
			_collision->build();
		}
	}
#ifdef TOOLS_ENABLED
	if (IS_EDITOR && _plugin) {
		_plugin->update_region_grid();
	}
#endif
}

void WorldScape3D::set_render_layers(const uint32_t p_layers) {
	LOG(INFO, "Setting terrain render layers to: ", p_layers);
	_render_layers = p_layers;
	if (_mesher) {
		_mesher->update();
	}
}

void WorldScape3D::set_mouse_layer(const uint32_t p_layer) {
	uint32_t layer = CLAMP(p_layer, 21u, 32u);
	_mouse_layer = layer;
	uint32_t mouse_mask = 1 << (_mouse_layer - 1);
	LOG(INFO, "Setting mouse layer: ", layer, " (", mouse_mask, ") on terrain mesh, material, mouse camera, mouse quad");

	// Set terrain meshes to mouse layer
	// Mask off editor render layers by ORing user layers 1-20 and current mouse layer
	set_render_layers((_render_layers & 0xFFFFF) | mouse_mask);
	// Set terrain shader to exclude mouse camera from showing holes
	if (_material.is_valid()) {
		_material->set_shader_param("_mouse_layer", mouse_mask);
	}
	// Set mouse camera to see only mouse layer
	if (_mouse_cam) {
		_mouse_cam->set_cull_mask(mouse_mask);
	}
	// Set screenquad to mouse layer
	if (_mouse_quad) {
		_mouse_quad->set_layer_mask(mouse_mask);
	}
}

void WorldScape3D::set_cast_shadows(const RenderingServer::ShadowCastingSetting p_cast_shadows) {
	_cast_shadows = p_cast_shadows;
	if (_mesher) {
		_mesher->update();
	}
}

void WorldScape3D::set_gi_mode(const GeometryInstance3D::GIMode p_gi_mode) {
	_gi_mode = p_gi_mode;
	if (_mesher) {
		_mesher->update();
	}
}

void WorldScape3D::set_cull_margin(const real_t p_margin) {
	LOG(INFO, "Setting extra cull margin: ", p_margin);
	_cull_margin = p_margin;
	if (_mesher) {
		_mesher->update_aabbs();
	}
}

/* Returns the point a ray intersects the ground using either raymarching or the GPU depth texture
 *	p_src_pos (camera position)
 *	p_direction (camera direction looking at the terrain)
 *  p_gpu_mode - false: use raymarching, true: use GPU mode
 * Returns Vec3(NAN) on error or vec3(3.402823466e+38F) on no intersection. Test w/ if (var.x < 3.4e38)
 */
Vector3 WorldScape3D::get_intersection(const Vector3 &p_src_pos, const Vector3 &p_direction, const bool p_gpu_mode) {
	if (!is_instance_valid(_camera_instance_id)) {
		LOG(ERROR, "Invalid camera");
		return Vector3(NAN, NAN, NAN);
	}
	if (!_mouse_cam) {
		LOG(ERROR, "Invalid mouse camera");
		return Vector3(NAN, NAN, NAN);
	}
	Vector3 direction = p_direction.normalized();
	Vector3 point;

	// Position mouse cam one unit behind the requested position
	_mouse_cam->set_global_position(p_src_pos - direction);

	// If looking straight down (eg orthogonal camera), just return height. look_at won't work
	if ((direction - Vector3(0.f, -1.f, 0.f)).length_squared() < 0.00001f) {
		_mouse_cam->set_rotation_degrees(Vector3(-90.f, 0.f, 0.f));
		point = p_src_pos;
		point.y = _data->get_height(p_src_pos);
		if (std::isnan(point.y)) {
			point.y = 0;
		}

	} else if (!p_gpu_mode) {
		// Else if not gpu mode, use raymarching mode
		point = p_src_pos;
		for (int i = 0; i < 4000; i++) {
			real_t height = _data->get_height(point);
			if (point.y - height <= 0) {
				return point;
			}
			point += direction;
		}
		return V3_MAX;

	} else {
		// Else use GPU mode, which requires multiple calls
		// Get depth from perspective camera snapshot
		_mouse_cam->look_at(_mouse_cam->get_global_position() + direction, Vector3(0.f, 1.f, 0.f));
		_mouse_vp->set_update_mode(SubViewport::UPDATE_ONCE);
		Ref<ViewportTexture> vp_tex = _mouse_vp->get_texture();
		Ref<Image> vp_img = vp_tex->get_image();

		// Read the depth pixel from the camera viewport
		Color screen_depth = vp_img->get_pixel(0, 0);

		// Get position from depth packed in RGB - unpack back to float.
		// Forward+ is 16bit, mobile and compatibility is 10bit.
		// Compatibility also has precision loss for values below 0.5, so
		// we use only the top half of the range, for 21bit depth encoded.
		real_t r = floor((screen_depth.r * 256.0) - 128.0);
		real_t g = floor((screen_depth.g * 256.0) - 128.0);
		real_t b = floor((screen_depth.b * 256.0) - 128.0);

		// Decode the full depth value
		real_t decoded_depth = (r + g / 127.0 + b / (127.0 * 127.0)) / 127.0;

		if (decoded_depth < 0.00001f) {
			return V3_MAX;
		}
		// Necessary for a correct value depth = 1
		if (decoded_depth > 0.99999f) {
			decoded_depth = 1.0f;
		}

		// Denormalize distance to get real depth and terrain position.
		decoded_depth *= _mouse_cam->get_far();

		// Project the camera position by the depth value to get the intersection point.
		point = _mouse_cam->get_global_position() + direction * decoded_depth;
	}

	return point;
}

/**
 * Generates a static ArrayMesh for the terrain.
 * p_lod (0-8): Determines the granularity of the generated mesh.
 * p_filter: Controls how vertices' Y coordinates are generated from the height map.
 *  HEIGHT_FILTER_NEAREST: Samples the height map in a 'nearest neighbour' fashion.
 *  HEIGHT_FILTER_MINIMUM: Samples a range of heights around each vertex and returns the lowest.
 *   This takes longer than ..._NEAREST, but can be used to create occluders, since it can guarantee the
 *   generated mesh will not extend above or outside the clipmap at any LOD.
 */
Ref<Mesh> WorldScape3D::bake_mesh(const int p_lod, const WorldScape3DData::HeightFilter p_filter) const {
	LOG(INFO, "Baking mesh at lod: ", p_lod, " with filter: ", p_filter);
	Ref<Mesh> result;
	ERR_FAIL_COND_V(_data == nullptr, result);

	Ref<SurfaceTool> st;
	st.instantiate();
	st->begin(Mesh::PRIMITIVE_TRIANGLES);

	PackedVector3Array vertices;
	PackedVector2Array uvs;
	_generate_triangles(vertices, &uvs, p_lod, p_filter, false, AABB());

	ERR_FAIL_COND_V(vertices.size() != uvs.size(), result);
	for (int i = 0; i < vertices.size(); ++i) {
		st->set_uv(uvs[i]);
		st->add_vertex(vertices[i]);
	}

	st->index();
	st->generate_normals();
	st->generate_tangents();
	st->optimize_indices_for_cache();
	result = st->commit();
	return result;
}

/**
 * Generates source geometry faces for input to nav mesh baking. Geometry is only generated where there
 * are no holes and the terrain has been painted as navigable.
 * p_global_aabb: If non-empty, geometry will be generated only within this AABB. If empty, geometry
 *  will be generated for the entire terrain.
 * p_require_nav: If true, this function will only generate geometry for terrain marked navigable.
 *  Otherwise, geometry is generated for the entire terrain within the AABB (which can be useful for
 *  dynamic and/or runtime nav mesh baking).
 */
PackedVector3Array WorldScape3D::generate_nav_mesh_source_geometry(const AABB &p_global_aabb, const bool p_require_nav) const {
	LOG(INFO, "Generating NavMesh source geometry from terrain");
	PackedVector3Array faces;
	_generate_triangles(faces, nullptr, 0, WorldScape3DData::HEIGHT_FILTER_NEAREST, p_require_nav, p_global_aabb);
	return faces;
}

void WorldScape3D::set_warning(const uint8_t p_warning, const bool p_enabled) {
	if (p_enabled) {
		_warnings |= p_warning;
	} else {
		_warnings &= ~p_warning;
	}
	update_configuration_warnings();
}

PackedStringArray WorldScape3D::get_configuration_warnings() const {
	PackedStringArray psa;
	if (_data_directory.is_empty()) {
		psa.push_back("No data directory specified. Select a directory then save the scene to write data.");
	}
	if (_warnings & WARN_MISMATCHED_SIZE) {
		psa.push_back("Texture dimensions don't match. Double-click a texture in the FileSystem panel to see its size. Read Texture Prep in docs.");
	}
	if (_warnings & WARN_MISMATCHED_FORMAT) {
		psa.push_back("Texture formats don't match. Double-click a texture in the FileSystem panel to see its format. Check Import panel. Read Texture Prep in docs.");
	}
	if (_warnings & WARN_MISMATCHED_MIPMAPS) {
		psa.push_back("Texture mipmap settings don't match. Change on the Import panel.");
	}
	return psa;
}

///////////////////////////
// Protected Functions
///////////////////////////

// Notifications are defined in individual classes: Object, Node, Node3D
// Listed below in order of operation
void WorldScape3D::_notification(const int p_what) {
	switch (p_what) {
			/// Startup notifications

		case NOTIFICATION_POSTINITIALIZE: {
			// Object initialized, before script is attached
			LOG(INFO, "NOTIFICATION_POSTINITIALIZE");
			_build_containers();
			break;
		}

		case NOTIFICATION_ENTER_WORLD: {
			// Node3D registered to new World3D resource
			// Sent on scene changes
			LOG(INFO, "NOTIFICATION_ENTER_WORLD");
			_is_inside_world = true;
			if (_mesher) {
				_mesher->update();
			}
			break;
		}

		case NOTIFICATION_ENTER_TREE: {
			// Node entered a SceneTree
			// Sent on scene changes
			LOG(INFO, "NOTIFICATION_ENTER_TREE");
			set_as_top_level(true); // Don't inherit transforms from parent. Global only.
			set_transform(Transform3D());
			set_notify_transform(true);
			set_meta("_edit_lock_", true);
			_setup_mouse_picking();
			if (_free_editor_textures && !IS_EDITOR && _assets.is_valid() && !_assets->get_path().contains("WorldScape3DAssets")) {
				LOG(INFO, "free_editor_textures enabled, reloading Assets path: ", _assets->get_path());
				_assets = ResourceLoader::load(_assets->get_path(), "", ResourceFormatLoader::CACHE_MODE_IGNORE);
			}
			_initialize(); // Rebuild anything freed: meshes, collision, instancer
			set_physics_process(true);
			break;
		}

		case NOTIFICATION_READY: {
			// Node is ready
			LOG(INFO, "NOTIFICATION_READY");
			if (_free_editor_textures && !IS_EDITOR && _assets.is_valid()) {
				if (_assets->get_path().contains("WorldScape3DAssets")) {
					LOG(WARN, "free_editor_textures requires `Assets` be saved to a file. Do so, or disable the former to turn off this warning");
				} else {
					LOG(INFO, "free_editor_textures enabled, clearing texture assets");
					_assets->clear_textures();
				}
			}
			break;
		}

			/// Game Loop notifications

		case NOTIFICATION_PHYSICS_PROCESS: {
			// Node is processing one physics frame
			__physics_process(get_process_delta_time());
			break;
		}

		case NOTIFICATION_TRANSFORM_CHANGED: {
			break;
		}

		case NOTIFICATION_VISIBILITY_CHANGED: {
			// Node3D visibility changed
			LOG(INFO, "NOTIFICATION_VISIBILITY_CHANGED");
			if (_mesher) {
				_mesher->update();
			}
			break;
		}

		case NOTIFICATION_EXTENSION_RELOADED: {
			// Object finished hot reloading
			LOG(INFO, "NOTIFICATION_EXTENSION_RELOADED");
			break;
		}

#ifdef TOOLS_ENABLED
		case NOTIFICATION_EDITOR_PRE_SAVE: {
			// Editor Node is about to save the current scene
			LOG(INFO, "NOTIFICATION_EDITOR_PRE_SAVE");
			if (_data_directory.is_empty()) {
				LOG(ERROR, "Data directory is empty. Set it to save regions to disk.");
			} else if (!_data) {
				LOG(DEBUG, "Save requested, but no valid data object. Skipping");
			} else {
				_data->save_directory(_data_directory);
			}
			if (!_material.is_valid()) {
				LOG(DEBUG, "Save requested, but no valid material. Skipping");
			} else {
				_material->save();
			}
			if (!_assets.is_valid()) {
				LOG(DEBUG, "Save requested, but no valid texture list. Skipping");
			} else {
				_assets->save();
			}
			break;
		}
#endif

#ifdef TOOLS_ENABLED
		case NOTIFICATION_EDITOR_POST_SAVE: {
			// Editor Node finished saving current scene
			break;
		}
#endif

		case NOTIFICATION_CRASH: {
			// Redot's crash handler reports engine is about to crash
			// Only works on desktop if the crash handler is enabled
			LOG(INFO, "NOTIFICATION_CRASH");
			break;
		}

			/// Shut down notifications

		case NOTIFICATION_EXIT_TREE: {
			// Node is about to exit a SceneTree
			// Sent on scene changes
			LOG(INFO, "NOTIFICATION_EXIT_TREE");
			set_physics_process(false);
			_destroy_mesher();
			_destroy_mouse_picking();
			if (_assets.is_valid()) {
				_assets->uninitialize();
			}
			if (_material.is_valid()) {
				_material->uninitialize();
			}
			_initialized = false;
			break;
		}

		case NOTIFICATION_EXIT_WORLD: {
			// Node3D unregistered from current World3D
			// Sent on scene changes
			LOG(INFO, "NOTIFICATION_EXIT_WORLD");
			_is_inside_world = false;
			break;
		}

		case NOTIFICATION_PREDELETE: {
			// Object is about to be deleted
			LOG(INFO, "NOTIFICATION_PREDELETE");
			_destroy_mesher(true);
			_destroy_instancer();
			_destroy_collision(true);
			_assets.unref();
			_material.unref();
			memdelete_safely(_data);
			_destroy_labels();
			_destroy_containers();
			break;
		}

		default:
			break;
	}
}

void WorldScape3D::_bind_methods() {
	BIND_ENUM_CONSTANT(ERROR);
	BIND_ENUM_CONSTANT(INFO);
	BIND_ENUM_CONSTANT(DEBUG);
	BIND_ENUM_CONSTANT(EXTREME);

	BIND_ENUM_CONSTANT(SIZE_64);
	BIND_ENUM_CONSTANT(SIZE_128);
	BIND_ENUM_CONSTANT(SIZE_256);
	BIND_ENUM_CONSTANT(SIZE_512);
	BIND_ENUM_CONSTANT(SIZE_1024);
	BIND_ENUM_CONSTANT(SIZE_2048);
	BIND_ENUM_CONSTANT(SIZE_4096);

	ClassDB::bind_method(D_METHOD("set_debug_level", "level"), &WorldScape3D::set_debug_level);
	ClassDB::bind_method(D_METHOD("get_debug_level"), &WorldScape3D::get_debug_level);
	ClassDB::bind_method(D_METHOD("set_data_directory", "directory"), &WorldScape3D::set_data_directory);
	ClassDB::bind_method(D_METHOD("get_data_directory"), &WorldScape3D::get_data_directory);

	// Object references
	ClassDB::bind_method(D_METHOD("get_data"), &WorldScape3D::get_data);
	ClassDB::bind_method(D_METHOD("set_material", "material"), &WorldScape3D::set_material);
	ClassDB::bind_method(D_METHOD("get_material"), &WorldScape3D::get_material);
	ClassDB::bind_method(D_METHOD("set_assets", "assets"), &WorldScape3D::set_assets);
	ClassDB::bind_method(D_METHOD("get_assets"), &WorldScape3D::get_assets);
	ClassDB::bind_method(D_METHOD("get_collision"), &WorldScape3D::get_collision);
	ClassDB::bind_method(D_METHOD("get_instancer"), &WorldScape3D::get_instancer);

	ClassDB::bind_method(D_METHOD("set_camera", "camera"), &WorldScape3D::set_camera);
	ClassDB::bind_method(D_METHOD("get_camera"), &WorldScape3D::get_camera);

	// Regions
	ClassDB::bind_method(D_METHOD("change_region_size", "size"), &WorldScape3D::change_region_size);
	ClassDB::bind_method(D_METHOD("get_region_size"), &WorldScape3D::get_region_size);
	ClassDB::bind_method(D_METHOD("set_save_16_bit", "enabled"), &WorldScape3D::set_save_16_bit);
	ClassDB::bind_method(D_METHOD("get_save_16_bit"), &WorldScape3D::get_save_16_bit);
	ClassDB::bind_method(D_METHOD("set_label_distance", "distance"), &WorldScape3D::set_label_distance);
	ClassDB::bind_method(D_METHOD("get_label_distance"), &WorldScape3D::get_label_distance);
	ClassDB::bind_method(D_METHOD("set_label_size", "size"), &WorldScape3D::set_label_size);
	ClassDB::bind_method(D_METHOD("get_label_size"), &WorldScape3D::get_label_size);

	// Collision
	ClassDB::bind_method(D_METHOD("set_collision_mode", "mode"), &WorldScape3D::set_collision_mode);
	ClassDB::bind_method(D_METHOD("get_collision_mode"), &WorldScape3D::get_collision_mode);
	ClassDB::bind_method(D_METHOD("set_collision_shape_size", "size"), &WorldScape3D::set_collision_shape_size);
	ClassDB::bind_method(D_METHOD("get_collision_shape_size"), &WorldScape3D::get_collision_shape_size);
	ClassDB::bind_method(D_METHOD("set_collision_radius", "radius"), &WorldScape3D::set_collision_radius);
	ClassDB::bind_method(D_METHOD("get_collision_radius"), &WorldScape3D::get_collision_radius);
	ClassDB::bind_method(D_METHOD("set_collision_layer", "layers"), &WorldScape3D::set_collision_layer);
	ClassDB::bind_method(D_METHOD("get_collision_layer"), &WorldScape3D::get_collision_layer);
	ClassDB::bind_method(D_METHOD("set_collision_mask", "mask"), &WorldScape3D::set_collision_mask);
	ClassDB::bind_method(D_METHOD("get_collision_mask"), &WorldScape3D::get_collision_mask);
	ClassDB::bind_method(D_METHOD("set_collision_priority", "priority"), &WorldScape3D::set_collision_priority);
	ClassDB::bind_method(D_METHOD("get_collision_priority"), &WorldScape3D::get_collision_priority);
	ClassDB::bind_method(D_METHOD("set_physics_material", "material"), &WorldScape3D::set_physics_material);
	ClassDB::bind_method(D_METHOD("get_physics_material"), &WorldScape3D::get_physics_material);

	// Meshes
	ClassDB::bind_method(D_METHOD("set_mesh_lods", "count"), &WorldScape3D::set_mesh_lods);
	ClassDB::bind_method(D_METHOD("get_mesh_lods"), &WorldScape3D::get_mesh_lods);
	ClassDB::bind_method(D_METHOD("set_mesh_size", "size"), &WorldScape3D::set_mesh_size);
	ClassDB::bind_method(D_METHOD("get_mesh_size"), &WorldScape3D::get_mesh_size);
	ClassDB::bind_method(D_METHOD("set_vertex_spacing", "scale"), &WorldScape3D::set_vertex_spacing);
	ClassDB::bind_method(D_METHOD("get_vertex_spacing"), &WorldScape3D::get_vertex_spacing);
	ClassDB::bind_method(D_METHOD("get_snapped_position"), &WorldScape3D::get_snapped_position);

	// Rendering
	ClassDB::bind_method(D_METHOD("set_render_layers", "layers"), &WorldScape3D::set_render_layers);
	ClassDB::bind_method(D_METHOD("get_render_layers"), &WorldScape3D::get_render_layers);
	ClassDB::bind_method(D_METHOD("set_mouse_layer", "layer"), &WorldScape3D::set_mouse_layer);
	ClassDB::bind_method(D_METHOD("get_mouse_layer"), &WorldScape3D::get_mouse_layer);
	ClassDB::bind_method(D_METHOD("set_cast_shadows", "shadow_casting_setting"), &WorldScape3D::set_cast_shadows);
	ClassDB::bind_method(D_METHOD("get_cast_shadows"), &WorldScape3D::get_cast_shadows);
	ClassDB::bind_method(D_METHOD("set_gi_mode", "gi_mode"), &WorldScape3D::set_gi_mode);
	ClassDB::bind_method(D_METHOD("get_gi_mode"), &WorldScape3D::get_gi_mode);
	ClassDB::bind_method(D_METHOD("set_cull_margin", "margin"), &WorldScape3D::set_cull_margin);
	ClassDB::bind_method(D_METHOD("get_cull_margin"), &WorldScape3D::get_cull_margin);
	ClassDB::bind_method(D_METHOD("set_free_editor_textures", "free_textures"), &WorldScape3D::set_free_editor_textures);
	ClassDB::bind_method(D_METHOD("get_free_editor_textures"), &WorldScape3D::get_free_editor_textures);
	ClassDB::bind_method(D_METHOD("set_show_instances", "visible"), &WorldScape3D::set_show_instances);
	ClassDB::bind_method(D_METHOD("get_show_instances"), &WorldScape3D::get_show_instances);

	// Overlays
	ClassDB::bind_method(D_METHOD("set_show_region_grid", "enabled"), &WorldScape3D::set_show_region_grid);
	ClassDB::bind_method(D_METHOD("get_show_region_grid"), &WorldScape3D::get_show_region_grid);
	ClassDB::bind_method(D_METHOD("set_show_instancer_grid", "enabled"), &WorldScape3D::set_show_instancer_grid);
	ClassDB::bind_method(D_METHOD("get_show_instancer_grid"), &WorldScape3D::get_show_instancer_grid);
	ClassDB::bind_method(D_METHOD("set_show_vertex_grid", "enabled"), &WorldScape3D::set_show_vertex_grid);
	ClassDB::bind_method(D_METHOD("get_show_vertex_grid"), &WorldScape3D::get_show_vertex_grid);
	ClassDB::bind_method(D_METHOD("set_show_contours", "enabled"), &WorldScape3D::set_show_contours);
	ClassDB::bind_method(D_METHOD("get_show_contours"), &WorldScape3D::get_show_contours);
	ClassDB::bind_method(D_METHOD("set_show_navigation", "enabled"), &WorldScape3D::set_show_navigation);
	ClassDB::bind_method(D_METHOD("get_show_navigation"), &WorldScape3D::get_show_navigation);

	// Debug Views
	ClassDB::bind_method(D_METHOD("set_show_checkered", "enabled"), &WorldScape3D::set_show_checkered);
	ClassDB::bind_method(D_METHOD("get_show_checkered"), &WorldScape3D::get_show_checkered);
	ClassDB::bind_method(D_METHOD("set_show_grey", "enabled"), &WorldScape3D::set_show_grey);
	ClassDB::bind_method(D_METHOD("get_show_grey"), &WorldScape3D::get_show_grey);
	ClassDB::bind_method(D_METHOD("set_show_heightmap", "enabled"), &WorldScape3D::set_show_heightmap);
	ClassDB::bind_method(D_METHOD("get_show_heightmap"), &WorldScape3D::get_show_heightmap);
	ClassDB::bind_method(D_METHOD("set_show_jaggedness", "enabled"), &WorldScape3D::set_show_jaggedness);
	ClassDB::bind_method(D_METHOD("get_show_jaggedness"), &WorldScape3D::get_show_jaggedness);
	ClassDB::bind_method(D_METHOD("set_show_colormap", "enabled"), &WorldScape3D::set_show_colormap);
	ClassDB::bind_method(D_METHOD("get_show_colormap"), &WorldScape3D::get_show_colormap);
	ClassDB::bind_method(D_METHOD("set_show_roughmap", "enabled"), &WorldScape3D::set_show_roughmap);
	ClassDB::bind_method(D_METHOD("get_show_roughmap"), &WorldScape3D::get_show_roughmap);
	ClassDB::bind_method(D_METHOD("set_show_control_texture", "enabled"), &WorldScape3D::set_show_control_texture);
	ClassDB::bind_method(D_METHOD("get_show_control_texture"), &WorldScape3D::get_show_control_texture);
	ClassDB::bind_method(D_METHOD("set_show_control_angle", "enabled"), &WorldScape3D::set_show_control_angle);
	ClassDB::bind_method(D_METHOD("get_show_control_angle"), &WorldScape3D::get_show_control_angle);
	ClassDB::bind_method(D_METHOD("set_show_control_scale", "enabled"), &WorldScape3D::set_show_control_scale);
	ClassDB::bind_method(D_METHOD("get_show_control_scale"), &WorldScape3D::get_show_control_scale);
	ClassDB::bind_method(D_METHOD("set_show_control_blend", "enabled"), &WorldScape3D::set_show_control_blend);
	ClassDB::bind_method(D_METHOD("get_show_control_blend"), &WorldScape3D::get_show_control_blend);
	ClassDB::bind_method(D_METHOD("set_show_autoshader", "enabled"), &WorldScape3D::set_show_autoshader);
	ClassDB::bind_method(D_METHOD("get_show_autoshader"), &WorldScape3D::get_show_autoshader);
	ClassDB::bind_method(D_METHOD("set_show_texture_height", "enabled"), &WorldScape3D::set_show_texture_height);
	ClassDB::bind_method(D_METHOD("get_show_texture_height"), &WorldScape3D::get_show_texture_height);
	ClassDB::bind_method(D_METHOD("set_show_texture_normal", "enabled"), &WorldScape3D::set_show_texture_normal);
	ClassDB::bind_method(D_METHOD("get_show_texture_normal"), &WorldScape3D::get_show_texture_normal);
	ClassDB::bind_method(D_METHOD("set_show_texture_rough", "enabled"), &WorldScape3D::set_show_texture_rough);
	ClassDB::bind_method(D_METHOD("get_show_texture_rough"), &WorldScape3D::get_show_texture_rough);

	// Utility
	ClassDB::bind_method(D_METHOD("get_intersection", "src_pos", "direction", "gpu_mode"), &WorldScape3D::get_intersection, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("bake_mesh", "lod", "filter"), &WorldScape3D::bake_mesh, DEFVAL(WorldScape3DData::HEIGHT_FILTER_NEAREST));
	ClassDB::bind_method(D_METHOD("generate_nav_mesh_source_geometry", "global_aabb", "require_nav"), &WorldScape3D::generate_nav_mesh_source_geometry, DEFVAL(true));

	ADD_PROPERTY(PropertyInfo(Variant::INT, "debug_level", PROPERTY_HINT_ENUM, "Errors,Info,Debug,Extreme"), "set_debug_level", "get_debug_level");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "data_directory", PROPERTY_HINT_DIR), "set_data_directory", "get_data_directory");

	// Object references
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "data", PROPERTY_HINT_NONE, "WorldScape3DData", PROPERTY_USAGE_NONE), "", "get_data");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material", PROPERTY_HINT_RESOURCE_TYPE, "WorldScape3DMaterial"), "set_material", "get_material");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "assets", PROPERTY_HINT_RESOURCE_TYPE, "WorldScape3DAssets"), "set_assets", "get_assets");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "collision", PROPERTY_HINT_NONE, "WorldScape3DCollision", PROPERTY_USAGE_NONE), "", "get_collision");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "instancer", PROPERTY_HINT_NONE, "WorldScape3DInstancer", PROPERTY_USAGE_NONE), "", "get_instancer");

	ADD_GROUP("Regions", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "region_size", PROPERTY_HINT_ENUM, "64:64,128:128,256:256,512:512,1024:1024,2048:2048,4096:4096", PROPERTY_USAGE_EDITOR), "change_region_size", "get_region_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "save_16_bit"), "set_save_16_bit", "get_save_16_bit");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "label_distance", PROPERTY_HINT_RANGE, "0.0,10000.0,0.5,or_greater"), "set_label_distance", "get_label_distance");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "label_size", PROPERTY_HINT_RANGE, "24,128,1"), "set_label_size", "get_label_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_grid"), "set_show_region_grid", "get_show_region_grid");

	ADD_GROUP("Collision", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mode", PROPERTY_HINT_ENUM, "Disabled,Dynamic / Game,Dynamic / Editor,Full / Game,Full / Editor"), "set_collision_mode", "get_collision_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_shape_size", PROPERTY_HINT_RANGE, "8,64,8"), "set_collision_shape_size", "get_collision_shape_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_radius", PROPERTY_HINT_RANGE, "16,256,16"), "set_collision_radius", "get_collision_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_layer", "get_collision_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collision_mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_collision_mask", "get_collision_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_priority", PROPERTY_HINT_RANGE, "0.1,256,.1"), "set_collision_priority", "get_collision_priority");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material", "get_physics_material");

	ADD_GROUP("Mesh", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_lods", PROPERTY_HINT_RANGE, "1,10,1"), "set_mesh_lods", "get_mesh_lods");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mesh_size", PROPERTY_HINT_RANGE, "8,64,2"), "set_mesh_size", "get_mesh_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vertex_spacing", PROPERTY_HINT_RANGE, "0.25,10.0,0.05,or_greater"), "set_vertex_spacing", "get_vertex_spacing");

	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_layers", PROPERTY_HINT_LAYERS_3D_RENDER), "set_render_layers", "get_render_layers");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mouse_layer", PROPERTY_HINT_RANGE, "21, 32"), "set_mouse_layer", "get_mouse_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadows", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows", "get_cast_shadows");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gi_mode", PROPERTY_HINT_ENUM, "Disabled,Static,Dynamic"), "set_gi_mode", "get_gi_mode");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cull_margin", PROPERTY_HINT_RANGE, "0.0,10000.0,.5,or_greater"), "set_cull_margin", "get_cull_margin");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "free_editor_textures"), "set_free_editor_textures", "get_free_editor_textures");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_instances"), "set_show_instances", "get_show_instances");

	ADD_GROUP("Overlays", "show_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_region_grid"), "set_show_region_grid", "get_show_region_grid");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_instancer_grid"), "set_show_instancer_grid", "get_show_instancer_grid");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_vertex_grid"), "set_show_vertex_grid", "get_show_vertex_grid");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_contours"), "set_show_contours", "get_show_contours");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_navigation"), "set_show_navigation", "get_show_navigation");

	ADD_GROUP("Debug Views", "show_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_checkered"), "set_show_checkered", "get_show_checkered");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_grey"), "set_show_grey", "get_show_grey");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_heightmap"), "set_show_heightmap", "get_show_heightmap");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_jaggedness"), "set_show_jaggedness", "get_show_jaggedness");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_colormap"), "set_show_colormap", "get_show_colormap");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_roughmap"), "set_show_roughmap", "get_show_roughmap");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_control_texture"), "set_show_control_texture", "get_show_control_texture");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_control_angle"), "set_show_control_angle", "get_show_control_angle");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_control_scale"), "set_show_control_scale", "get_show_control_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_control_blend"), "set_show_control_blend", "get_show_control_blend");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_autoshader"), "set_show_autoshader", "get_show_autoshader");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_texture_height"), "set_show_texture_height", "get_show_texture_height");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_texture_normal"), "set_show_texture_normal", "get_show_texture_normal");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_texture_rough"), "set_show_texture_rough", "get_show_texture_rough");

	ADD_SIGNAL(MethodInfo("material_changed"));
	ADD_SIGNAL(MethodInfo("assets_changed"));
}
