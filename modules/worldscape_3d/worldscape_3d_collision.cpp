/**************************************************************************/
/*  worldscape_3d_collision.cpp                                           */
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
#include "core/os/time.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/3d/world_3d.h"

#include "scene/main/scene_tree.h"

#include "constants.h"
#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_collision.h"
#include "worldscape_3d_data.h"
#include "worldscape_3d_util.h"

///////////////////////////
// Private Functions
///////////////////////////

// Calculates shape data from top left position. Assumes descaled and snapped.
Dictionary WorldScape3DCollision::_get_shape_data(const Vector2i &p_position, const int p_size) {
	IS_DATA_INIT_MESG("Terrain not initialized", Dictionary());
	WorldScape3DData *data = _terrain->get_data();
	int region_size = _terrain->get_region_size();

	int hshape_size = p_size + 1; // Calculate last vertex at end
	PackedRealArray map_data = PackedRealArray();
	map_data.resize(hshape_size * hshape_size);
	real_t min_height = FLT_MAX;
	real_t max_height = FLT_MIN;

	Ref<Image> map, map_x, map_z, map_xz; // height maps
	Ref<Image> cmap, cmap_x, cmap_z, cmap_xz; // control maps w/ holes

	// Get region_loc of top left corner of descaled and grid snapped collision shape position
	Vector2i region_loc = V2I_DIVIDE_FLOOR(p_position, region_size);
	Ref<WorldScape3DRegion> region = data->get_region(region_loc);
	if (region.is_null() || (region.is_valid() && region->is_deleted())) {
		LOG(EXTREME, "Region not found at: ", region_loc, ". Returning blank");
		return Dictionary();
	}
	map = region->get_map(TYPE_HEIGHT);
	cmap = region->get_map(TYPE_CONTROL);

	// Get +X, +Z adjacent regions in case we run over
	region = data->get_region(region_loc + Vector2i(1, 0));
	if (region.is_valid() && !region->is_deleted()) {
		map_x = region->get_map(TYPE_HEIGHT);
		cmap_x = region->get_map(TYPE_CONTROL);
	}
	region = data->get_region(region_loc + Vector2i(0, 1));
	if (region.is_valid() && !region->is_deleted()) {
		map_z = region->get_map(TYPE_HEIGHT);
		cmap_z = region->get_map(TYPE_CONTROL);
	}
	region = data->get_region(region_loc + Vector2i(1, 1));
	if (region.is_valid() && !region->is_deleted()) {
		map_xz = region->get_map(TYPE_HEIGHT);
		cmap_xz = region->get_map(TYPE_CONTROL);
	}

	for (int z = 0; z < hshape_size; z++) {
		for (int x = 0; x < hshape_size; x++) {
			// Choose array indexing to match triangulation of heightmapshape with the mesh
			// https://stackoverflow.com/questions/16684856/rotating-a-2d-pixel-array-by-90-degrees
			// Normal array index rotated Y=0 - shape rotation Y=0 (xform below)
			// int index = z * hshape_size + x;
			// Array Index Rotated Y=-90 - must rotate shape Y=+90 (xform below)
			int index = hshape_size - 1 - z + x * hshape_size;

			Vector2i shape_pos = p_position + Vector2i(x, z);
			Vector2i shape_region_loc = V2I_DIVIDE_FLOOR(shape_pos, region_size);
			int img_x = Math::posmod(shape_pos.x, region_size);
			bool next_x = shape_region_loc.x > region_loc.x;
			int img_y = Math::posmod(shape_pos.y, region_size);
			bool next_z = shape_region_loc.y > region_loc.y;

			// Set heights on local map, or adjacent maps if on the last row/col
			real_t height = 0.f;
			if (!next_x && !next_z && map.is_valid()) {
				height = is_hole(cmap->get_pixel(img_x, img_y).r) ? NAN : map->get_pixel(img_x, img_y).r;
			} else if (next_x && !next_z) {
				if (map_x.is_valid()) {
					height = is_hole(cmap_x->get_pixel(img_x, img_y).r) ? NAN : map_x->get_pixel(img_x, img_y).r;
				} else {
					height = is_hole(cmap->get_pixel(region_size - 1, img_y).r) ? NAN : map->get_pixel(region_size - 1, img_y).r;
				}
			} else if (!next_x && next_z) {
				if (map_z.is_valid()) {
					height = is_hole(cmap_z->get_pixel(img_x, img_y).r) ? NAN : map_z->get_pixel(img_x, img_y).r;
				} else {
					height = (is_hole(cmap->get_pixel(img_x, region_size - 1).r)) ? NAN : map->get_pixel(img_x, region_size - 1).r;
				}
			} else if (next_x && next_z) {
				if (map_xz.is_valid()) {
					height = is_hole(cmap_xz->get_pixel(img_x, img_y).r) ? NAN : map_xz->get_pixel(img_x, img_y).r;
				} else {
					height = (is_hole(cmap->get_pixel(region_size - 1, region_size - 1).r)) ? NAN : map->get_pixel(region_size - 1, region_size - 1).r;
				}
			}
			map_data.set(index, height);
			if (!std::isnan(height)) {
				min_height = MIN(min_height, height);
				max_height = MAX(max_height, height);
			}
		}
	}

	// Non rotated shape for normal array index above
	//Transform3D xform = Transform3D(Basis(), global_pos);
	// Rotated shape Y=90 for -90 rotated array index
	Transform3D xform = Transform3D(Basis(Vector3(0, 1.0, 0), Math_PI * .5), v2iv3(p_position + V2I(p_size / 2)));
	Dictionary shape_data;
	shape_data["width"] = hshape_size;
	shape_data["depth"] = hshape_size;
	shape_data["heights"] = map_data;
	shape_data["xform"] = xform;
	shape_data["min_height"] = min_height;
	shape_data["max_height"] = max_height;
	return shape_data;
}

void WorldScape3DCollision::_shape_set_disabled(const int p_shape_id, const bool p_disabled) {
	if (is_editor_mode()) {
		CollisionShape3D *shape = _shapes[p_shape_id];
		shape->set_disabled(p_disabled);
		shape->set_visible(!p_disabled);
	} else {
		PhysicsServer3D::get_singleton()->body_set_shape_disabled(_static_body_rid, p_shape_id, p_disabled);
	}
}

void WorldScape3DCollision::_shape_set_transform(const int p_shape_id, const Transform3D &p_xform) {
	if (is_editor_mode()) {
		CollisionShape3D *shape = _shapes[p_shape_id];
		shape->set_transform(p_xform);
	} else {
		PhysicsServer3D::get_singleton()->body_set_shape_transform(_static_body_rid, p_shape_id, p_xform);
	}
}

Vector3 WorldScape3DCollision::_shape_get_position(const int p_shape_id) const {
	if (is_editor_mode()) {
		CollisionShape3D *shape = _shapes[p_shape_id];
		return shape->get_global_position();
	} else {
		return PhysicsServer3D::get_singleton()->body_get_shape_transform(_static_body_rid, p_shape_id).origin;
	}
}

void WorldScape3DCollision::_shape_set_data(const int p_shape_id, const Dictionary &p_dict) {
	if (is_editor_mode()) {
		CollisionShape3D *shape = _shapes[p_shape_id];
		Ref<HeightMapShape3D> hshape = shape->get_shape();
		hshape->set_map_data(p_dict["heights"]);
	} else {
		const auto ps = PhysicsServer3D::get_singleton();
		RID shape_rid = ps->body_get_shape(_static_body_rid, p_shape_id);
		ps->shape_set_data(shape_rid, p_dict);
	}
}

void WorldScape3DCollision::_reload_physics_material() {
	if (is_editor_mode()) {
		if (_static_body) {
			_static_body->set_physics_material_override(_physics_material);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			const auto ps = PhysicsServer3D::get_singleton();
			if (_physics_material.is_null()) {
				ps->body_set_param(_static_body_rid, PhysicsServer3D::BODY_PARAM_BOUNCE, 0.f);
				ps->body_set_param(_static_body_rid, PhysicsServer3D::BODY_PARAM_FRICTION, 1.f);
			} else {
				const real_t computed_bounce = _physics_material->get_bounce() * (_physics_material->is_absorbent() ? -1.f : 1.f);
				const real_t computed_friction = _physics_material->get_friction() * (_physics_material->is_rough() ? -1.f : 1.f);
				ps->body_set_param(_static_body_rid, PhysicsServer3D::BODY_PARAM_BOUNCE, computed_bounce);
				ps->body_set_param(_static_body_rid, PhysicsServer3D::BODY_PARAM_FRICTION, computed_friction);
			}
		}
	}
	if (_physics_material.is_valid()) {
		LOG(DEBUG, "Setting PhysicsMaterial bounce: ", _physics_material->get_bounce(), ", friction: ", _physics_material->get_friction());
	}
}

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DCollision::initialize(WorldScape3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	} else {
		return;
	}
#ifdef TOOLS_ENABLED
	if (!IS_EDITOR && is_editor_mode()) {
		LOG(WARN, "Change collision mode to a non-editor mode for releases");
	}
#endif
	build();
}

void WorldScape3DCollision::build() {
	IS_DATA_INIT(VOID);
	if (!_terrain->is_inside_world()) {
		LOG(ERROR, "Terrain isn't inside world. Returning.");
		return;
	}

	// Clear collision as the user might change modes in the editor
	destroy();

	// Build only in applicable modes
	if (!is_enabled()
#ifdef TOOLS_ENABLED
			|| (IS_EDITOR && !is_editor_mode())
#endif
	) {
		return;
	}

	const auto ps = PhysicsServer3D::get_singleton();

	// Create StaticBody3D
	if (is_editor_mode()) {
		LOG(INFO, "Building editor collision");
		_static_body = memnew(StaticBody3D);
		_static_body->set_name("StaticBody3D");
		_static_body->set_as_top_level(true);
		_terrain->add_child(_static_body, true);
		_static_body->set_owner(_terrain);
		_static_body->set_collision_mask(_mask);
		_static_body->set_collision_layer(_layer);
		_static_body->set_collision_priority(_priority);
	} else {
		LOG(INFO, "Building collision with Physics Server");
		_static_body_rid = ps->body_create();
		ps->body_set_mode(_static_body_rid, PhysicsServer3D::BODY_MODE_STATIC);
		ps->body_set_space(_static_body_rid, _terrain->get_world_3d()->get_space());
		ps->body_attach_object_instance_id(_static_body_rid, _terrain->get_instance_id());
		ps->body_set_collision_mask(_static_body_rid, _mask);
		ps->body_set_collision_layer(_static_body_rid, _layer);
		ps->body_set_collision_priority(_static_body_rid, _priority);
	}
	_reload_physics_material();

	// Create CollisionShape3Ds
	int shape_count;
	int hshape_size;
	if (is_dynamic_mode()) {
		int grid_width = _radius * 2 / _shape_size;
		grid_width = int_ceil_pow2(grid_width, 4);
		shape_count = grid_width * grid_width;
		hshape_size = _shape_size + 1;
		LOG(DEBUG, "Grid width: ", grid_width);
	} else {
		shape_count = _terrain->get_data()->get_region_count();
		hshape_size = _terrain->get_region_size() + 1;
	}
	// Preallocate memory for push_back()
	if (is_editor_mode()) {
		_shapes.reserve(shape_count);
	}
	LOG(DEBUG, "Shape count: ", shape_count);
	LOG(DEBUG, "Shape size: ", _shape_size, ", hshape_size: ", hshape_size);
	Transform3D xform(Basis(), V3_MAX);
	for (int i = 0; i < shape_count; i++) {
		if (is_editor_mode()) {
			CollisionShape3D *col_shape = memnew(CollisionShape3D);
			_shapes.push_back(col_shape);
			col_shape->set_name("CollisionShape3D");
			col_shape->set_disabled(true);
			col_shape->set_visible(true);
			//col_shape->set_debug_fill_enabled(false);
			Ref<HeightMapShape3D> hshape;
			hshape.instantiate();
			hshape->set_map_width(hshape_size);
			hshape->set_map_depth(hshape_size);
			col_shape->set_shape(hshape);
			_static_body->add_child(col_shape, true);
			col_shape->set_owner(_static_body);
			col_shape->set_transform(xform);
		} else {
			RID shape_rid = ps->heightmap_shape_create();
			ps->body_add_shape(_static_body_rid, shape_rid, xform, true);
			LOG(DEBUG, "Adding shape: ", i, ", rid: ", shape_rid.get_id(), " pos: ", _shape_get_position(i));
		}
	}

	_initialized = true;
	update();
}

void WorldScape3DCollision::update(const bool p_rebuild) {
	if (!_initialized) {
		return;
	}
	if (p_rebuild && !is_dynamic_mode()) {
		build();
		return;
	}
	[[maybe_unused]] int time = Time::get_singleton()->get_ticks_usec();
	real_t spacing = _terrain->get_vertex_spacing();

	if (is_dynamic_mode()) {
		// Snap descaled position to a _shape_size grid (eg. multiples of 16)
		Vector2i snapped_pos = _snap_to_grid(_terrain->get_snapped_position() / spacing);
		LOG(EXTREME, "Updating collision at ", snapped_pos);

		// Skip if location hasn't moved to next step
		if (!p_rebuild && (_last_snapped_pos - snapped_pos).length_squared() < (_shape_size * _shape_size)) {
			return;
		}

		LOG(EXTREME, "---- 1. Defining area as a radius on a grid ----");
		// Create a 0-N grid, center on snapped_pos
		PackedInt32Array grid;
		int grid_width = _radius * 2 / _shape_size; // 64*2/16 = 8
		grid_width = int_ceil_pow2(grid_width, 4);
		grid.resize(grid_width * grid_width);
		grid.fill(-1);
		Vector2i grid_offset = -V2I(grid_width / 2); // offset # cells to center of grid
		Vector2i shape_offset = V2I(_shape_size / 2); // offset meters to top left corner of shape
		Vector2i grid_pos = snapped_pos + grid_offset * _shape_size; // Top left of grid
		LOG(EXTREME, "New Snapped position: ", snapped_pos);
		LOG(EXTREME, "Grid_pos: ", grid_pos);
		LOG(EXTREME, "Radius: ", _radius, ", Grid_width: ", grid_width, ", Grid_offset: ", grid_offset, ", # cells: ", grid.size());
		LOG(EXTREME, "Shape_size: ", _shape_size, ", shape_offset: ", shape_offset);

		LOG(EXTREME, "---- 2. Checking existing shapes ----");
		// If shape is within area, skip
		// Else, mark unused

		// Stores index into _shapes array
		TypedArray<int> inactive_shape_ids;

		auto radius_sqr = static_cast<real_t>(_radius * _radius);
		int shape_count = is_editor_mode() ? _shapes.size() : PhysicsServer3D::get_singleton()->body_get_shape_count(_static_body_rid);
		for (int i = 0; i < shape_count; i++) {
			// Descaled global position of shape center
			Vector3 shape_center = _shape_get_position(i) / spacing;
			// Unique key: Top left corner of shape, snapped to grid
			Vector2i shape_pos = _snap_to_grid(v3v2i(shape_center) - shape_offset);
			// Optionally could adjust radius to account for corner (sqrt(_shape_size*2))
			if (!p_rebuild && (shape_center.x < FLT_MAX && v3v2i(shape_center).distance_squared_to(snapped_pos) <= radius_sqr)) {
				// Get index into shape array
				Vector2i grid_loc = (shape_pos - grid_pos) / _shape_size;
				grid.set(grid_loc.y * grid_width + grid_loc.x, i);
				_shape_set_disabled(i, false);
				LOG(EXTREME, "Shape ", i, ": shape_center: ", shape_center.x < FLT_MAX ? shape_center : V3(-999), ", shape_pos: ", shape_pos,
						", grid_loc: ", grid_loc, ", index: ", (grid_loc.y * grid_width + grid_loc.x), " active");
			} else {
				inactive_shape_ids.push_back(i);
				_shape_set_disabled(i, true);
				LOG(EXTREME, "Shape ", i, ": shape_center: ", shape_center.x < FLT_MAX ? shape_center : V3(-999), ", shape_pos: ", shape_pos,
						" out of bounds, marking inactive");
			}
		}
		LOG(EXTREME, "_inactive_shapes size: ", inactive_shape_ids.size());

		LOG(EXTREME, "---- 3. Review grid cells in area ----");
		// If cell is full, skip
		// Else assign shape and form it

		for (int i = 0; i < grid.size(); i++) {
			Vector2i grid_loc(i % grid_width, i / grid_width);
			// Unique key: Top left corner of shape, snapped to grid
			Vector2i shape_pos = grid_pos + grid_loc * _shape_size;

			if ((shape_pos + shape_offset).distance_squared_to(snapped_pos) > radius_sqr) {
				LOG(EXTREME, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " out of circle, skipping");
				continue;
			}
			if (!p_rebuild && grid[i] >= 0) {
				[[maybe_unused]] Vector2i center_pos = v3v2i(_shape_get_position(i));
				LOG(EXTREME, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " act ", center_pos - shape_offset, " Has active shape id: ", grid[i]);
				continue;
			} else {
				if (inactive_shape_ids.size() == 0) {
					LOG(ERROR, "No more unused shapes! Aborting!");
					break;
				}
				Dictionary shape_data = _get_shape_data(shape_pos, _shape_size);
				if (shape_data.is_empty()) {
					LOG(EXTREME, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " No region found");
					continue;
				}
				int shape_id = inactive_shape_ids.pop_back();
				Transform3D xform = shape_data["xform"];
				LOG(EXTREME, "grid[", i, ":", grid_loc, "] shape_pos : ", shape_pos, " act ", v3v2i(xform.origin) - shape_offset, " placing shape id ", shape_id);
				xform.scale(Vector3(spacing, 1.f, spacing));
				_shape_set_transform(shape_id, xform);
				_shape_set_disabled(shape_id, false);
				_shape_set_data(shape_id, shape_data);
			}
		}
		_last_snapped_pos = snapped_pos;
		LOG(EXTREME, "Setting _last_snapped_pos: ", _last_snapped_pos);
		LOG(EXTREME, "inactive_shape_ids size: ", inactive_shape_ids.size());

	} else {
		// Full collision
		//int shape_count = _terrain->get_data()->get_region_count();
		int region_size = _terrain->get_region_size();
		TypedArray<Vector2i> region_locs = _terrain->get_data()->get_region_locations();
		for (int i = 0; i < region_locs.size(); i++) {
			Vector2i region_loc = region_locs[i];
			Vector2i shape_pos = region_loc * region_size;
			Dictionary shape_data = _get_shape_data(shape_pos, region_size);
			if (shape_data.is_empty()) {
				LOG(ERROR, "Can't get shape data for ", region_loc);
				continue;
			}
			Transform3D xform = shape_data["xform"];
			xform.scale(Vector3(spacing, 1.f, spacing));
			_shape_set_transform(i, xform);
			_shape_set_disabled(i, false);
			_shape_set_data(i, shape_data);
		}
	}
	LOG(EXTREME, "Collision update time: ", Time::get_singleton()->get_ticks_usec() - time, " us");
}

void WorldScape3DCollision::destroy() {
	_initialized = false;
	_last_snapped_pos = V2I_MAX;

	// Physics Server
	const auto ps = PhysicsServer3D::get_singleton();
	if (_static_body_rid.is_valid()) {
		// Shape IDs change as they are freed, so it's not safe to iterate over them while freeing.
		while (ps->body_get_shape_count(_static_body_rid) > 0) {
			RID rid = ps->body_get_shape(_static_body_rid, 0);
			LOG(DEBUG, "Freeing CollisionShape RID ", rid);
			ps->free(rid);
		}

		LOG(DEBUG, "Freeing StaticBody RID");
		ps->free(_static_body_rid);
		_static_body_rid = RID();
	}

	// Scene Tree
	for (size_t i = 0; i < _shapes.size(); i++) {
		CollisionShape3D *shape = _shapes[i];
		LOG(DEBUG, "Freeing CollisionShape3D ", shape->get_name());
		remove_from_tree(shape);
		memdelete_safely(shape);
	}
	_shapes.clear();
	if (_static_body) {
		LOG(DEBUG, "Freeing StaticBody3D");
		remove_from_tree(_static_body);
		memdelete_safely(_static_body);
	}
}

void WorldScape3DCollision::set_mode(const CollisionMode p_mode) {
	LOG(INFO, "Setting collision mode: ", p_mode);
	if (p_mode != _mode) {
		_mode = p_mode;
		if (is_enabled()) {
			build();
		} else {
			destroy();
		}
	}
}

void WorldScape3DCollision::set_shape_size(const uint16_t p_size) {
	int size = CLAMP(p_size, 8, 64);
	size = int_round_mult(size, 8);
	LOG(INFO, "Setting collision dynamic shape size: ", size);
	_shape_size = size;
	// Ensure size:radius always results in at least one valid shape
	if (_shape_size > _radius - 8) {
		set_radius(_shape_size + 16);
	} else if (is_dynamic_mode()) {
		build();
	}
}

void WorldScape3DCollision::set_radius(const uint16_t p_radius) {
	int radius = CLAMP(p_radius, 16, 256);
	radius = int_ceil_pow2(radius, 16);
	LOG(INFO, "Setting collision dynamic radius: ", radius);
	_radius = radius;
	// Ensure size:radius always results in at least one valid shape
	if (_radius < _shape_size + 8) {
		set_shape_size(_radius - 8);
	} else if (_shape_size < 16 && _radius > 128) {
		set_shape_size(16);
	} else if (is_dynamic_mode()) {
		build();
	}
}

void WorldScape3DCollision::set_layer(const uint32_t p_layers) {
	LOG(INFO, "Setting collision layers: ", p_layers);
	_layer = p_layers;
	if (is_editor_mode()) {
		if (_static_body) {
			_static_body->set_collision_layer(_layer);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_layer(_static_body_rid, _layer);
		}
	}
}

void WorldScape3DCollision::set_mask(const uint32_t p_mask) {
	LOG(INFO, "Setting collision mask: ", p_mask);
	_mask = p_mask;
	if (is_editor_mode()) {
		if (_static_body) {
			_static_body->set_collision_mask(_mask);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_mask(_static_body_rid, _mask);
		}
	}
}

void WorldScape3DCollision::set_priority(const real_t p_priority) {
	LOG(INFO, "Setting collision priority: ", p_priority);
	_priority = p_priority;
	if (is_editor_mode()) {
		if (_static_body) {
			_static_body->set_collision_priority(_priority);
		}
	} else {
		if (_static_body_rid.is_valid()) {
			PhysicsServer3D::get_singleton()->body_set_collision_priority(_static_body_rid, _priority);
		}
	}
}

void WorldScape3DCollision::set_physics_material(const Ref<PhysicsMaterial> &p_mat) {
	LOG(INFO, "Setting physics material: ", p_mat);
	if (_physics_material.is_valid()) {
		if (_physics_material->is_connected("changed", callable_mp(this, &WorldScape3DCollision::_reload_physics_material))) {
			LOG(DEBUG, "Disconnecting _physics_material::changed signal to _reload_physics_material()");
			_physics_material->disconnect("changed", callable_mp(this, &WorldScape3DCollision::_reload_physics_material));
		}
	}
	_physics_material = p_mat;
	if (_physics_material.is_valid()) {
		LOG(DEBUG, "Connecting _physics_material::changed signal to _reload_physics_material()");
		_physics_material->connect("changed", callable_mp(this, &WorldScape3DCollision::_reload_physics_material));
	}
	_reload_physics_material();
}

RID WorldScape3DCollision::get_rid() const {
	if (!is_editor_mode()) {
		return _static_body_rid;
	} else {
		if (_static_body) {
			return _static_body->get_rid();
		}
	}
	return RID();
}

///////////////////////////
// Protected Functions
///////////////////////////

void WorldScape3DCollision::_bind_methods() {
	BIND_ENUM_CONSTANT(DISABLED);
	BIND_ENUM_CONSTANT(DYNAMIC_GAME);
	BIND_ENUM_CONSTANT(DYNAMIC_EDITOR);
	BIND_ENUM_CONSTANT(FULL_GAME);
	BIND_ENUM_CONSTANT(FULL_EDITOR);

	ClassDB::bind_method(D_METHOD("build"), &WorldScape3DCollision::build);
	ClassDB::bind_method(D_METHOD("update", "rebuild"), &WorldScape3DCollision::update, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("destroy"), &WorldScape3DCollision::destroy);
	ClassDB::bind_method(D_METHOD("set_mode", "mode"), &WorldScape3DCollision::set_mode);
	ClassDB::bind_method(D_METHOD("get_mode"), &WorldScape3DCollision::get_mode);
	ClassDB::bind_method(D_METHOD("is_enabled"), &WorldScape3DCollision::is_enabled);
	ClassDB::bind_method(D_METHOD("is_editor_mode"), &WorldScape3DCollision::is_editor_mode);
	ClassDB::bind_method(D_METHOD("is_dynamic_mode"), &WorldScape3DCollision::is_dynamic_mode);

	ClassDB::bind_method(D_METHOD("set_shape_size", "size"), &WorldScape3DCollision::set_shape_size);
	ClassDB::bind_method(D_METHOD("get_shape_size"), &WorldScape3DCollision::get_shape_size);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &WorldScape3DCollision::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &WorldScape3DCollision::get_radius);
	ClassDB::bind_method(D_METHOD("set_layer", "layers"), &WorldScape3DCollision::set_layer);
	ClassDB::bind_method(D_METHOD("get_layer"), &WorldScape3DCollision::get_layer);
	ClassDB::bind_method(D_METHOD("set_mask", "mask"), &WorldScape3DCollision::set_mask);
	ClassDB::bind_method(D_METHOD("get_mask"), &WorldScape3DCollision::get_mask);
	ClassDB::bind_method(D_METHOD("set_priority", "priority"), &WorldScape3DCollision::set_priority);
	ClassDB::bind_method(D_METHOD("get_priority"), &WorldScape3DCollision::get_priority);
	ClassDB::bind_method(D_METHOD("set_physics_material", "material"), &WorldScape3DCollision::set_physics_material);
	ClassDB::bind_method(D_METHOD("get_physics_material"), &WorldScape3DCollision::get_physics_material);
	ClassDB::bind_method(D_METHOD("get_rid"), &WorldScape3DCollision::get_rid);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "mode", PROPERTY_HINT_ENUM, "Disabled,Dynamic / Game,Dynamic / Editor,Full / Game,Full / Editor"), "set_mode", "get_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape_size", PROPERTY_HINT_RANGE, "8,64,8"), "set_shape_size", "get_shape_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "radius", PROPERTY_HINT_RANGE, "16,256,16"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "layer", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_layer", "get_layer");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mask", PROPERTY_HINT_LAYERS_3D_PHYSICS), "set_mask", "get_mask");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "priority", PROPERTY_HINT_RANGE, "0.1,256,.1"), "set_priority", "get_priority");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "physics_material", PROPERTY_HINT_RESOURCE_TYPE, "PhysicsMaterial"), "set_physics_material", "get_physics_material");
}
