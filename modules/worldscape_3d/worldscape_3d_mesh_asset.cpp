/**************************************************************************/
/*  worldscape_3d_mesh_asset.cpp                                          */
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

#include "core/io/file_access.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#endif

#include "logger.h"
#include "worldscape_3d_assets.h"
#include "worldscape_3d_instancer.h"
#include "worldscape_3d_mesh_asset.h"

///////////////////////////
// Private Functions
///////////////////////////

void WorldScape3DMeshAsset::_clear_lod_ranges() {
	_lod_ranges.resize(MAX_LOD_COUNT);
	for (int i = 0; i < MAX_LOD_COUNT; i++) {
		_lod_ranges.set(i, (i + 1) * WorldScape3DInstancer::CELL_SIZE);
	}
	_lod_ranges.set(_last_lod, MAX(_lod_ranges[_last_lod], 128.f));
}

bool WorldScape3DMeshAsset::_sort_lod_nodes(const Node *a, const Node *b) {
	ASSERT(a && b, false);
	return String(a->get_name()).right(1) < String(b->get_name()).right(1);
}

Ref<ArrayMesh> WorldScape3DMeshAsset::_get_generated_mesh() const {
	LOG(EXTREME, "Regeneratingn new mesh");
	Ref<ArrayMesh> array_mesh;
	array_mesh.instantiate();
	PackedVector3Array vertices;
	PackedVector3Array normals;
	PackedFloat32Array tangents;
	PackedVector2Array uvs;
	PackedInt32Array indices;

	int prevrow, thisrow, point = 0;
	float x, z;
	Size2 start_pos = Vector2(_generated_size.x * -0.5, -0.5f);
	Vector3 normal = Vector3(0.0, 0.0, 1.0);
	thisrow = point;
	prevrow = 0;
	Vector3 Up = Vector3(0.f, 1.f, 0.f);
	for (int m = 1; m <= _generated_faces; m++) {
		z = start_pos.y;
		real_t angle = 0.f;
		if (m > 1) {
			angle = (m - 1) * Math_PI / _generated_faces;
		}
		for (int j = 0; j <= 1; j++) {
			x = start_pos.x;
			for (int i = 0; i <= 1; i++) {
				float u = i;
				float v = j;

				vertices.push_back(Vector3(-x, z, 0.0).rotated(Up, angle));
				normals.push_back(normal);
				tangents.push_back(1.0);
				tangents.push_back(0.0);
				tangents.push_back(0.0);
				tangents.push_back(1.0);
				uvs.push_back(Vector2(1.0 - u, 1.0 - v));
				point++;
				if (i > 0 && j > 0) {
					indices.push_back(prevrow + i - 1);
					indices.push_back(prevrow + i);
					indices.push_back(thisrow + i - 1);
					indices.push_back(prevrow + i);
					indices.push_back(thisrow + i);
					indices.push_back(thisrow + i - 1);
				}
				x += _generated_size.x;
			}
			z += _generated_size.y;
			prevrow = thisrow;
			thisrow = point;
		}
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TANGENT] = tangents;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;
	array_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	return array_mesh;
}

Ref<Material> WorldScape3DMeshAsset::_get_material() {
	if (_material_override.is_null()) {
		Ref<StandardMaterial3D> mat;
		mat.instantiate();
		mat->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA_DEPTH_PRE_PASS);
		mat->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
		mat->set_feature(BaseMaterial3D::FEATURE_BACKLIGHT, true);
		mat->set_backlight(Color(.5f, .5f, .5f));
		mat->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		mat->set_distance_fade(BaseMaterial3D::DISTANCE_FADE_PIXEL_ALPHA);
		mat->set_distance_fade_min_distance(128.f);
		mat->set_distance_fade_max_distance(96.f);
		return mat;
	} else {
		return _material_override;
	}
}

///////////////////////////
// Public Functions
///////////////////////////

WorldScape3DMeshAsset::WorldScape3DMeshAsset() {
	clear();
}

void WorldScape3DMeshAsset::clear() {
	LOG(INFO, "Clearing MeshAsset");
	_name = "New Mesh";
	_id = 0;
	_enabled = true;
	_packed_scene.unref();
	_generated_type = TYPE_NONE;
	_meshes.clear();
	_thumbnail.unref();
	_height_offset = 0.f;
	_density = 10.f;
	_cast_shadows = SHADOWS_ON;
	_material_override.unref();
	_material_overlay.unref();
	_generated_faces = 2.f;
	_generated_size = Vector2(1.f, 1.f);
	_last_lod = MAX_LOD_COUNT - 1;
	_last_shadow_lod = MAX_LOD_COUNT - 1;
	_shadow_impostor = 0;
	_clear_lod_ranges();
	_fade_margin = 0.f;
}

void WorldScape3DMeshAsset::set_name(const String &p_name) {
	LOG(INFO, "Setting name: ", p_name);
	_name = p_name;
	emit_signal("setting_changed");
}

void WorldScape3DMeshAsset::set_id(const int p_new_id) {
	int old_id = _id;
	_id = CLAMP(p_new_id, 0, WorldScape3DAssets::MAX_MESHES);
	LOG(INFO, "Setting mesh id: ", _id);
	emit_signal("id_changed", WorldScape3DAssets::TYPE_MESH, old_id, p_new_id);
}

void WorldScape3DMeshAsset::set_enabled(const bool p_enabled) {
	_enabled = p_enabled;
	LOG(INFO, "Setting enabled: ", p_enabled);
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_scene_file(const Ref<PackedScene> &p_scene_file) {
	LOG(INFO, "Setting scene file and instantiating node: ", p_scene_file);
	_packed_scene = p_scene_file;
	_meshes.clear();
	if (_packed_scene.is_valid()) {
		Node *node = _packed_scene->instantiate();
		if (!node) {
			print_error("Drag a non-empty glb, fbx, scn, or tscn file into the scene_file slot");
			_packed_scene.unref();
			return;
		}
		_generated_type = TYPE_NONE;
		_height_offset = 0.0f;
		_material_override.unref();

		// Look for MeshInstance3D nodes
		LOG(DEBUG, "Loaded scene with parent node: ", node);
		TypedArray<Node> mesh_instances;

		// First look for XXXXLOD# meshes, sorted by last digit
		mesh_instances = node->find_children("*LOD?", "MeshInstance3D");
		if (mesh_instances.size() > 0) {
			LOG(INFO, "Found ", mesh_instances.size(), " meshes using LOD# naming convention, using the first ", MAX_LOD_COUNT);
			mesh_instances.sort_custom(callable_mp_static(&WorldScape3DMeshAsset::_sort_lod_nodes));
		}

		// Fallback to using all the meshes in provided order
		if (mesh_instances.size() == 0) {
			mesh_instances = node->find_children("*", "MeshInstance3D");
			if (mesh_instances.size() > 0) {
				LOG(INFO, "No meshes with LOD# suffixes found, using the first ", MAX_LOD_COUNT, " meshes as LOD0-LOD3");
			}
		}

		// Fallback to the scene root mesh
		if (mesh_instances.size() == 0) {
			if (node->is_class("MeshInstance3D")) {
				LOG(INFO, "No LOD# meshes found, assuming the root mesh is LOD0");
				mesh_instances.push_back(node);
			}
		}
		if (mesh_instances.size() == 0) {
			LOG(ERROR, "No MeshInstance3D found in scene file");
		}

		// Now process the meshes
		for (int i = 0, count = MIN(mesh_instances.size(), MAX_LOD_COUNT); i < count; i++) {
			MeshInstance3D *mi = cast_to<MeshInstance3D>(mesh_instances[i]);
			LOG(DEBUG, "Found mesh: ", mi->get_name());
			if (_name == "New Mesh") {
				_name = _packed_scene->get_path().get_file().get_basename();
				LOG(INFO, "Setting name based on filename: ", _name);
			}
			// Duplicate the mesh to make each WorldScape3DMeshAsset unique
			Ref<Mesh> orig_mesh = mi->get_mesh();
			if (orig_mesh.is_valid()) {
				Ref<Mesh> mesh = orig_mesh->duplicate();
				// Apply the active material from the scene to the mesh, including MI or Geom overrides
				for (int j = 0; j < mi->get_surface_override_material_count(); j++) {
					Ref<Material> mat = mi->get_active_material(j);
					mesh->surface_set_material(j, mat);
				}
				_meshes.push_back(mesh);
			}
		}
		node->queue_free();
	}
	if (_meshes.size() > 0) {
		Ref<Mesh> mesh = _meshes[0];
		_density = CLAMP(10.f / mesh->get_aabb().get_volume(), 0.01f, 10.0f);
	} else {
		set_generated_type(TYPE_TEXTURE_CARD);
	}
	_last_lod = _meshes.size() - 1;
	_last_shadow_lod = _last_lod;
	_shadow_impostor = 0;
	_clear_lod_ranges();
	notify_property_list_changed(); // Call _validate_property to update inspector
	LOG(DEBUG, "Emitting file_changed");
	emit_signal("file_changed");
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_generated_type(const GenType p_type) {
	_generated_type = p_type;
	LOG(INFO, "Setting is_generated: ", p_type);
	if (p_type == TYPE_NONE && _packed_scene.is_null()) {
		_generated_type = TYPE_TEXTURE_CARD;
	}
	if (p_type > TYPE_NONE && p_type < TYPE_MAX) {
		_packed_scene.unref();
		_meshes.clear();
		LOG(DEBUG, "Generating card mesh");
		_meshes.push_back(_get_generated_mesh());
		if (_material_override.is_null()) {
			_material_override = _get_material();
		}
		_density = 10.f;
		_height_offset = 0.5f;
		_last_lod = 0;
		_last_shadow_lod = 0;
		_shadow_impostor = 0;
		_clear_lod_ranges();
	}
	notify_property_list_changed(); // Call _validate_property to update inspector
	LOG(DEBUG, "Emitting file_changed");
	emit_signal("file_changed");
	emit_signal("instancer_setting_changed");
}

Ref<Mesh> WorldScape3DMeshAsset::get_mesh(const int p_lod) const {
	if (p_lod >= 0 && p_lod < _meshes.size()) {
		return _meshes[p_lod];
	}
	return Ref<Mesh>();
}

void WorldScape3DMeshAsset::set_height_offset(const real_t p_offset) {
	_height_offset = CLAMP(p_offset, -50.f, 50.f);
	LOG(INFO, "Setting height offset: ", _height_offset);
	emit_signal("setting_changed");
}

void WorldScape3DMeshAsset::set_density(const real_t p_density) {
	LOG(INFO, "Setting mesh density: ", p_density);
	_density = CLAMP(p_density, 0.01f, 10.f);
}

void WorldScape3DMeshAsset::set_cast_shadows(const ShadowCasting p_cast_shadows) {
	_cast_shadows = p_cast_shadows;
	LOG(INFO, "Setting shadow casting mode: ", _cast_shadows);
	emit_signal("instancer_setting_changed");
}

// Returns the appropriate cast_shadows setting for the given LOD id
ShadowCasting WorldScape3DMeshAsset::get_lod_cast_shadows(const int p_lod_id) const {
	// If cast shadows is off, disable all shadows
	if (_cast_shadows == SHADOWS_OFF) {
		return _cast_shadows;
	}
	// Return shadows only if set, ensuring shadow impostor and last lod are processed first
	if (_cast_shadows == SHADOWS_ONLY) {
		return _cast_shadows;
	}
	// Set to shadows only if this lod is the shadow impostor, which is only ever set to shadows only
	if (p_lod_id == SHADOW_LOD_ID) {
		return SHADOWS_ONLY;
	}
	// Disable shadows if this lod uses the shadow impostor
	if (p_lod_id < _shadow_impostor) {
		return SHADOWS_OFF;
	}
	// Disable shadows if this lod is too far
	if (p_lod_id > _last_shadow_lod) {
		return SHADOWS_OFF;
	}
	return _cast_shadows;
}

void WorldScape3DMeshAsset::set_material_override(const Ref<Material> &p_material) {
	LOG(INFO, _name, ": Setting material override: ", p_material);
	_material_override = p_material;
	LOG(DEBUG, "Emitting setting_changed");
	emit_signal("file_changed");
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_material_overlay(const Ref<Material> &p_material) {
	LOG(INFO, _name, ": Setting material overlay: ", p_material);
	_material_overlay = p_material;
	LOG(DEBUG, "Emitting setting_changed");
	emit_signal("file_changed");
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_generated_faces(const int p_count) {
	if (_generated_faces != p_count) {
		_generated_faces = CLAMP(p_count, 1, 3);
		LOG(INFO, "Setting generated face count: ", _generated_faces);
		if (_generated_type > TYPE_NONE && _generated_type < TYPE_MAX && _meshes.size() == 1) {
			_meshes[0] = _get_generated_mesh();
			if (_material_override.is_null()) {
				_material_override = _get_material();
			}
			LOG(DEBUG, "Emitting setting_changed");
			emit_signal("file_changed");
			emit_signal("instancer_setting_changed");
		}
	}
}

void WorldScape3DMeshAsset::set_generated_size(const Vector2 &p_size) {
	if (_generated_size != p_size) {
		_generated_size = p_size;
		LOG(INFO, "Setting generated size: ", _generated_faces);
		if (_generated_type > TYPE_NONE && _generated_type < TYPE_MAX && _meshes.size() == 1) {
			_meshes[0] = _get_generated_mesh();
			if (_material_override.is_null()) {
				_material_override = _get_material();
			}
			LOG(DEBUG, "Emitting setting_changed");
			emit_signal("file_changed");
			emit_signal("instancer_setting_changed");
		}
	}
}

void WorldScape3DMeshAsset::set_last_lod(const int p_lod) {
	int max_lod = _generated_type != TYPE_NONE ? 0 : CLAMP(_meshes.size(), 2, MAX_LOD_COUNT) - 1;
	_last_lod = CLAMP(p_lod, 0, max_lod);
	if (_last_shadow_lod > _last_lod) {
		_last_shadow_lod = _last_lod;
	}
	if (_shadow_impostor > _last_lod) {
		_shadow_impostor = _last_lod;
	}
	LOG(INFO, "Setting last LOD: ", _last_lod);
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_last_shadow_lod(const int p_lod) {
	_last_shadow_lod = CLAMP(p_lod, 0, _last_lod);
	if (_shadow_impostor > _last_shadow_lod) {
		_shadow_impostor = _last_shadow_lod;
	}
	LOG(INFO, "Setting last shadow LOD: ", _last_shadow_lod);
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_shadow_impostor(const int p_lod) {
	_shadow_impostor = CLAMP(p_lod, 0, MIN(_last_lod, _last_shadow_lod));
	LOG(INFO, "Setting shadow imposter LOD: ", _shadow_impostor);
	emit_signal("instancer_setting_changed");
}

void WorldScape3DMeshAsset::set_lod_range(const int p_lod, const real_t p_distance) {
	if (p_lod < 0 || p_lod >= _lod_ranges.size()) {
		return;
	}
	_lod_ranges.set(p_lod, CLAMP(p_distance, 0.f, 100000.f));
	LOG(INFO, "Setting LOD ", p_lod, " visibility range: ", _lod_ranges[p_lod]);
	emit_signal("instancer_setting_changed");
}

real_t WorldScape3DMeshAsset::get_lod_range(const int p_lod) const {
	if (p_lod < 0 || p_lod >= _lod_ranges.size()) {
		return -1.f;
	}
	return _lod_ranges[p_lod];
}

real_t WorldScape3DMeshAsset::get_lod_range_begin(const int p_lod) const {
	if (p_lod <= 0) {
		return 0.f;
	}
	ASSERT(p_lod <= _last_lod, 0.f);
	return _lod_ranges[p_lod - 1];
}

real_t WorldScape3DMeshAsset::get_lod_range_end(const int p_lod) const {
	if (p_lod == SHADOW_LOD_ID) {
		return _lod_ranges[MAX(_shadow_impostor - 1, 0)];
	}
	ASSERT(p_lod >= 0 && p_lod <= _last_lod, 0.f);
	return _lod_ranges[p_lod];
}

void WorldScape3DMeshAsset::set_fade_margin(const real_t p_fade_margin) {
	int max_range = CLAMP(_lod_ranges[1] - _lod_ranges[0], 0.f, 64.f);
	_fade_margin = CLAMP(p_fade_margin, 0.f, max_range);
	LOG(INFO, "Setting visibility margin: ", _fade_margin);
	emit_signal("instancer_setting_changed");
}

///////////////////////////
// Protected Functions
///////////////////////////

void WorldScape3DMeshAsset::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name != StringName("generated_type") && p_property.name.begins_with("generated_")) {
		if (_generated_type == TYPE_NONE) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
		return;
	} else if (p_property.name.match("lod?_range")) {
		int lod = p_property.name.substr(3, 1).to_int();
		if (_last_lod < lod) {
			p_property.usage = PROPERTY_USAGE_NO_EDITOR;
		} else {
			p_property.usage = PROPERTY_USAGE_DEFAULT;
		}
	}
}

void WorldScape3DMeshAsset::_bind_methods() {
	BIND_ENUM_CONSTANT(TYPE_NONE);
	BIND_ENUM_CONSTANT(TYPE_TEXTURE_CARD);
	BIND_ENUM_CONSTANT(TYPE_MAX);

	ADD_SIGNAL(MethodInfo("id_changed", PropertyInfo(Variant::INT, "type"), PropertyInfo(Variant::INT, "old_id"), PropertyInfo(Variant::INT, "new_id")));
	ADD_SIGNAL(MethodInfo("file_changed"));
	ADD_SIGNAL(MethodInfo("setting_changed"));
	ADD_SIGNAL(MethodInfo("instancer_setting_changed"));

	ClassDB::bind_method(D_METHOD("clear"), &WorldScape3DMeshAsset::clear);
	//ClassDB::bind_method(D_METHOD("set_name", "name"), &WorldScape3DMeshAsset::set_name);
	//ClassDB::bind_method(D_METHOD("get_name"), &WorldScape3DMeshAsset::get_name);
	ClassDB::bind_method(D_METHOD("set_id", "id"), &WorldScape3DMeshAsset::set_id);
	ClassDB::bind_method(D_METHOD("get_id"), &WorldScape3DMeshAsset::get_id);
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &WorldScape3DMeshAsset::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &WorldScape3DMeshAsset::is_enabled);

	ClassDB::bind_method(D_METHOD("set_scene_file", "scene_file"), &WorldScape3DMeshAsset::set_scene_file);
	ClassDB::bind_method(D_METHOD("get_scene_file"), &WorldScape3DMeshAsset::get_scene_file);
	ClassDB::bind_method(D_METHOD("set_generated_type", "type"), &WorldScape3DMeshAsset::set_generated_type);
	ClassDB::bind_method(D_METHOD("get_generated_type"), &WorldScape3DMeshAsset::get_generated_type);
	ClassDB::bind_method(D_METHOD("get_mesh", "lod"), &WorldScape3DMeshAsset::get_mesh, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_thumbnail"), &WorldScape3DMeshAsset::get_thumbnail);
	ClassDB::bind_method(D_METHOD("set_height_offset", "offset"), &WorldScape3DMeshAsset::set_height_offset);
	ClassDB::bind_method(D_METHOD("get_height_offset"), &WorldScape3DMeshAsset::get_height_offset);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &WorldScape3DMeshAsset::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &WorldScape3DMeshAsset::get_density);
	ClassDB::bind_method(D_METHOD("set_cast_shadows", "mode"), &WorldScape3DMeshAsset::set_cast_shadows);
	ClassDB::bind_method(D_METHOD("get_cast_shadows"), &WorldScape3DMeshAsset::get_cast_shadows);
	ClassDB::bind_method(D_METHOD("set_material_override", "material"), &WorldScape3DMeshAsset::set_material_override);
	ClassDB::bind_method(D_METHOD("get_material_override"), &WorldScape3DMeshAsset::get_material_override);
	ClassDB::bind_method(D_METHOD("set_material_overlay", "material"), &WorldScape3DMeshAsset::set_material_overlay);
	ClassDB::bind_method(D_METHOD("get_material_overlay"), &WorldScape3DMeshAsset::get_material_overlay);

	ClassDB::bind_method(D_METHOD("set_generated_faces", "count"), &WorldScape3DMeshAsset::set_generated_faces);
	ClassDB::bind_method(D_METHOD("get_generated_faces"), &WorldScape3DMeshAsset::get_generated_faces);
	ClassDB::bind_method(D_METHOD("set_generated_size", "size"), &WorldScape3DMeshAsset::set_generated_size);
	ClassDB::bind_method(D_METHOD("get_generated_size"), &WorldScape3DMeshAsset::get_generated_size);

	ClassDB::bind_method(D_METHOD("get_lod_count"), &WorldScape3DMeshAsset::get_lod_count);
	ClassDB::bind_method(D_METHOD("set_last_lod", "lod"), &WorldScape3DMeshAsset::set_last_lod);
	ClassDB::bind_method(D_METHOD("get_last_lod"), &WorldScape3DMeshAsset::get_last_lod);
	ClassDB::bind_method(D_METHOD("set_last_shadow_lod", "lod"), &WorldScape3DMeshAsset::set_last_shadow_lod);
	ClassDB::bind_method(D_METHOD("get_last_shadow_lod"), &WorldScape3DMeshAsset::get_last_shadow_lod);
	ClassDB::bind_method(D_METHOD("set_shadow_impostor", "lod"), &WorldScape3DMeshAsset::set_shadow_impostor);
	ClassDB::bind_method(D_METHOD("get_shadow_impostor"), &WorldScape3DMeshAsset::get_shadow_impostor);

	ClassDB::bind_method(D_METHOD("set_lod_range", "lod", "distance"), &WorldScape3DMeshAsset::set_lod_range);
	ClassDB::bind_method(D_METHOD("get_lod_range", "lod"), &WorldScape3DMeshAsset::get_lod_range);
	ClassDB::bind_method(D_METHOD("set_lod0_range", "distance"), &WorldScape3DMeshAsset::set_lod0_range);
	ClassDB::bind_method(D_METHOD("get_lod0_range"), &WorldScape3DMeshAsset::get_lod0_range);
	ClassDB::bind_method(D_METHOD("set_lod1_range", "distance"), &WorldScape3DMeshAsset::set_lod1_range);
	ClassDB::bind_method(D_METHOD("get_lod1_range"), &WorldScape3DMeshAsset::get_lod1_range);
	ClassDB::bind_method(D_METHOD("set_lod2_range", "distance"), &WorldScape3DMeshAsset::set_lod2_range);
	ClassDB::bind_method(D_METHOD("get_lod2_range"), &WorldScape3DMeshAsset::get_lod2_range);
	ClassDB::bind_method(D_METHOD("set_lod3_range", "distance"), &WorldScape3DMeshAsset::set_lod3_range);
	ClassDB::bind_method(D_METHOD("get_lod3_range"), &WorldScape3DMeshAsset::get_lod3_range);
	ClassDB::bind_method(D_METHOD("set_lod4_range", "distance"), &WorldScape3DMeshAsset::set_lod4_range);
	ClassDB::bind_method(D_METHOD("get_lod4_range"), &WorldScape3DMeshAsset::get_lod4_range);
	ClassDB::bind_method(D_METHOD("set_lod5_range", "distance"), &WorldScape3DMeshAsset::set_lod5_range);
	ClassDB::bind_method(D_METHOD("get_lod5_range"), &WorldScape3DMeshAsset::get_lod5_range);
	ClassDB::bind_method(D_METHOD("set_lod6_range", "distance"), &WorldScape3DMeshAsset::set_lod6_range);
	ClassDB::bind_method(D_METHOD("get_lod6_range"), &WorldScape3DMeshAsset::get_lod6_range);
	ClassDB::bind_method(D_METHOD("set_lod7_range", "distance"), &WorldScape3DMeshAsset::set_lod7_range);
	ClassDB::bind_method(D_METHOD("get_lod7_range"), &WorldScape3DMeshAsset::get_lod7_range);
	ClassDB::bind_method(D_METHOD("set_lod8_range", "distance"), &WorldScape3DMeshAsset::set_lod8_range);
	ClassDB::bind_method(D_METHOD("get_lod8_range"), &WorldScape3DMeshAsset::get_lod8_range);
	ClassDB::bind_method(D_METHOD("set_lod9_range", "distance"), &WorldScape3DMeshAsset::set_lod9_range);
	ClassDB::bind_method(D_METHOD("get_lod9_range"), &WorldScape3DMeshAsset::get_lod9_range);
	ClassDB::bind_method(D_METHOD("set_fade_margin", "distance"), &WorldScape3DMeshAsset::set_fade_margin);
	ClassDB::bind_method(D_METHOD("get_fade_margin"), &WorldScape3DMeshAsset::get_fade_margin);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "name", PROPERTY_HINT_NONE), "set_name", "get_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "id", PROPERTY_HINT_NONE), "set_id", "get_id");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled", PROPERTY_HINT_NONE), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "scene_file", PROPERTY_HINT_RESOURCE_TYPE, "PackedScene"), "set_scene_file", "get_scene_file");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "generated_type", PROPERTY_HINT_ENUM, "None,Texture Card"), "set_generated_type", "get_generated_type");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height_offset", PROPERTY_HINT_RANGE, "-20.0,20.0,.005"), "set_height_offset", "get_height_offset");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, ".01,10.0,.005"), "set_density", "get_density");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "cast_shadows", PROPERTY_HINT_ENUM, "Off,On,Double-Sided,Shadows Only"), "set_cast_shadows", "get_cast_shadows");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_override", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material_override", "get_material_override");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "material_overlay", PROPERTY_HINT_RESOURCE_TYPE, "BaseMaterial3D,ShaderMaterial"), "set_material_overlay", "get_material_overlay");

	ADD_GROUP("Generated Mesh", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "generated_faces", PROPERTY_HINT_NONE), "set_generated_faces", "get_generated_faces");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "generated_size", PROPERTY_HINT_NONE), "set_generated_size", "get_generated_size");

	ADD_GROUP("LODs", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "lod_count", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY), "", "get_lod_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_lod", PROPERTY_HINT_NONE), "set_last_lod", "get_last_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "last_shadow_lod", PROPERTY_HINT_NONE), "set_last_shadow_lod", "get_last_shadow_lod");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_impostor", PROPERTY_HINT_NONE), "set_shadow_impostor", "get_shadow_impostor");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod0_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod0_range", "get_lod0_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod1_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod1_range", "get_lod1_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod2_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod2_range", "get_lod2_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod3_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod3_range", "get_lod3_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod4_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod4_range", "get_lod4_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod5_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod5_range", "get_lod5_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod6_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod6_range", "get_lod6_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod7_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod7_range", "get_lod7_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod8_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod8_range", "get_lod8_range");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "lod9_range", PROPERTY_HINT_RANGE, "0.,4096.0,.05,or_greater"), "set_lod9_range", "get_lod9_range");
	// Fade disabled until https://github.com/godotengine/godot/issues/102799 is fixed
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fade_margin", PROPERTY_HINT_RANGE, "0.,64.0,.05,or_greater", PROPERTY_USAGE_NO_EDITOR), "set_fade_margin", "get_fade_margin");
}
