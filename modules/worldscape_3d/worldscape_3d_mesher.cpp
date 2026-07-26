/**************************************************************************/
/*  worldscape_3d_mesher.cpp                                              */
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

#include "core/version.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "servers/rendering_server.h"

#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_mesher.h"

///////////////////////////
// Private Functions
///////////////////////////

void WorldScape3DMesher::_generate_mesh_types(const int p_size) {
	_clear_mesh_types();
	LOG(INFO, "Generating all Mesh segments for clipmap of size ", p_size);
	// Create initial set of Mesh blocks to build the clipmap
	// # 0 TILE - mesh_size x mesh_size tiles
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size, p_size)));
	// # 1 EDGE_A - 2 by (mesh_size * 4 + 8) strips to bridge LOD transitions along +-Z axis
	_mesh_rids.push_back(_generate_mesh(Vector2i(2, p_size * 4 + 8)));
	// # 2 EDGE_B - (mesh_size * 4 + 4) by 2 strips to bridge LOD transitions along +-X axis
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size * 4 + 4, 2)));
	// # 3 FILL_A - 4 by mesh_size
	_mesh_rids.push_back(_generate_mesh(Vector2i(4, p_size)));
	// # 4 FILL_B - mesh_size by 4
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size, 4)));
	// # 5 STANDARD_TRIM_A - 2 by (mesh_size * 4 + 2) strips for LOD0 +-Z axis edge
	_mesh_rids.push_back(_generate_mesh(Vector2i(2, p_size * 4 + 2), true));
	// # 6 STANDARD_TRIM_B - (mesh_size * 4 + 4) by 2 strips for LOD0 +-X axis edge
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size * 4 + 2, 2), true));
	// # 7 STANDARD_TILE - mesh_size x mesh_size tiles
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size, p_size), true));
	// # 8 STANDARD_EDGE_A - 2 by (mesh_size * 4 + 8) strips to bridge LOD transitions along +-Z axis
	_mesh_rids.push_back(_generate_mesh(Vector2i(2, p_size * 4 + 8), true));
	// # 9 STANDARD_EDGE_B - (mesh_size * 4 + 4) by 2 strips to bridge LOD transitions along +-X axis
	_mesh_rids.push_back(_generate_mesh(Vector2i(p_size * 4 + 4, 2), true));
	return;
}

RID WorldScape3DMesher::_generate_mesh(const Vector2i &p_size, const bool p_standard_grid) {
	PackedVector3Array vertices;
	PackedInt32Array indices;
	AABB aabb = AABB(V3_ZERO, Vector3(p_size.x, 0.1f, p_size.y));
	LOG(DEBUG, "Generating vertices and indices for a", p_standard_grid ? " symmetric " : " standard ", "grid mesh of width: ", p_size.x, " and height: ", p_size.y);

	// Generate vertices
	for (int y = 0; y <= p_size.y; ++y) {
		for (int x = 0; x <= p_size.x; ++x) {
			// Match GDScript vertex definitions
			vertices.push_back(Vector3(x, 0, y)); // bottom-left
		}
	}

	// Generate indices for quads with alternating diagonals
	for (int y = 0; y < p_size.y; ++y) {
		for (int x = 0; x < p_size.x; ++x) {
			int bottomLeft = y * (p_size.x + 1) + x;
			int bottomRight = bottomLeft + 1;
			int topLeft = (y + 1) * (p_size.x + 1) + x;
			int topRight = topLeft + 1;

			if ((x + y) % 2 == 0 || p_standard_grid) {
				indices.push_back(bottomLeft);
				indices.push_back(topRight);
				indices.push_back(topLeft);

				indices.push_back(bottomLeft);
				indices.push_back(bottomRight);
				indices.push_back(topRight);
			} else {
				indices.push_back(bottomLeft);
				indices.push_back(bottomRight);
				indices.push_back(topLeft);

				indices.push_back(topLeft);
				indices.push_back(bottomRight);
				indices.push_back(topRight);
			}
		}
	}

	return _instantiate_mesh(vertices, indices, aabb);
}

RID WorldScape3DMesher::_instantiate_mesh(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices, const AABB &p_aabb) {
	Array arrays;
	arrays.resize(RenderingServer::ARRAY_MAX);
	arrays[RenderingServer::ARRAY_VERTEX] = p_vertices;
	arrays[RenderingServer::ARRAY_INDEX] = p_indices;

	PackedVector3Array normals;
	normals.resize(p_vertices.size());
	normals.fill(Vector3(0, 1, 0));
	arrays[RenderingServer::ARRAY_NORMAL] = normals;

	PackedFloat32Array tangents;
	tangents.resize(p_vertices.size() * 4);
	tangents.fill(0.0f);
	arrays[RenderingServer::ARRAY_TANGENT] = tangents;

	auto rs = RenderingServer::get_singleton();

	LOG(DEBUG, "Creating mesh via the Rendering server");
	RID mesh = rs->mesh_create();
	rs->mesh_add_surface_from_arrays(mesh, RenderingServer::PRIMITIVE_TRIANGLES, arrays);

	LOG(DEBUG, "Setting custom aabb: ", p_aabb.position, ", ", p_aabb.size);
	rs->mesh_set_custom_aabb(mesh, p_aabb);
	rs->mesh_surface_set_material(mesh, 0, _terrain->get_material()->get_material_rid());

	return mesh;
}

void WorldScape3DMesher::_generate_clipmap(const int p_size, const int p_lods, const RID &p_scenario) {
	_clear_clipmap();
	_generate_mesh_types(p_size);
	_generate_offset_data(p_size);
	LOG(DEBUG, "Creating instances for all mesh segments for clipmap of size ", p_size, " for ", p_lods, " LODs");

	for (int level = 0; level < p_lods; level++) {
		Array lod;
		// 12 Tiles LOD1+, 16 for LOD0
		Array tile_rids;
		int tile_ammount = (level == 0) ? 16 : 12;

		for (int i = 0; i < tile_ammount; i++) {
			RID tile_rid = RS::get_singleton()->instance_create2(_mesh_rids[level == 0 ? STANDARD_TILE : TILE], p_scenario);
			tile_rids.append(tile_rid);
		}
		lod.append(tile_rids); // index 0 TILE

		// 4 Edges present on all LODs
		Array edge_a_rids;
		for (int i = 0; i < 2; i++) {
			RID edge_a_rid = RS::get_singleton()->instance_create2(_mesh_rids[level == 0 ? STANDARD_EDGE_A : EDGE_A], p_scenario);
			edge_a_rids.append(edge_a_rid);
		}
		lod.append(edge_a_rids); // index 1 EDGE_A

		Array edge_b_rids;
		for (int i = 0; i < 2; i++) {
			RID edge_b_rid = RS::get_singleton()->instance_create2(_mesh_rids[level == 0 ? STANDARD_EDGE_B : EDGE_B], p_scenario);
			edge_b_rids.append(edge_b_rid);
		}
		lod.append(edge_b_rids); // index 2 EDGE_B

		// Fills only present on LODs 1+
		if (level > 0) {
			Array fill_a_rids;
			for (int i = 0; i < 2; i++) {
				RID fill_a_rid = RS::get_singleton()->instance_create2(_mesh_rids[FILL_A], p_scenario);
				fill_a_rids.append(fill_a_rid);
			}
			lod.append(fill_a_rids); // index 4 FILL_A

			Array fill_b_rids;
			for (int i = 0; i < 2; i++) {
				RID fill_b_rid = RS::get_singleton()->instance_create2(_mesh_rids[FILL_B], p_scenario);
				fill_b_rids.append(fill_b_rid);
			}
			lod.append(fill_b_rids); // index 5 FILL_B
			// Trims only on LOD 0 These share the indices of the fills for the offsets.
			// When snapping LOD 0 Trim a/b positions are looked up instead of Fill a/b
		} else {
			Array trim_a_rids;
			for (int i = 0; i < 2; i++) {
				RID trim_a_rid = RS::get_singleton()->instance_create2(_mesh_rids[STANDARD_TRIM_A], p_scenario);
				trim_a_rids.append(trim_a_rid);
			}
			lod.append(trim_a_rids); // index 4 TRIM_A

			Array trim_b_rids;
			for (int i = 0; i < 2; i++) {
				RID trim_b_rid = RS::get_singleton()->instance_create2(_mesh_rids[STANDARD_TRIM_B], p_scenario);
				trim_b_rids.append(trim_b_rid);
			}
			lod.append(trim_b_rids); // index 5 TRIM_B
		}

		// Append LOD to _lod_rids array
		_clipmap_rids.append(lod);
	}
}

// Precomputes all instance offset data into lookup arrays that match created instances.
// All meshes are created with 0,0 as their origin and grow along +xz. Offsets account for this.
void WorldScape3DMesher::_generate_offset_data(const int p_size) {
	LOG(INFO, "Computing all clipmap instance positioning offsets");
	_tile_pos_lod_0.clear();
	_trim_a_pos.clear();
	_trim_b_pos.clear();
	_edge_pos.clear();
	_fill_a_pos.clear();
	_fill_b_pos.clear();
	_tile_pos.clear();

	// LOD0 Tiles: Full 4x4 Grid of mesh size tiles
	_tile_pos_lod_0.push_back(Vector3(0, 0, p_size));
	_tile_pos_lod_0.push_back(Vector3(p_size, 0, p_size));
	_tile_pos_lod_0.push_back(Vector3(p_size, 0, 0));
	_tile_pos_lod_0.push_back(Vector3(p_size, 0, -p_size));
	_tile_pos_lod_0.push_back(Vector3(p_size, 0, -p_size * 2));
	_tile_pos_lod_0.push_back(Vector3(0, 0, -p_size * 2));
	_tile_pos_lod_0.push_back(Vector3(-p_size, 0, -p_size * 2));
	_tile_pos_lod_0.push_back(Vector3(-p_size * 2, 0, -p_size * 2));
	_tile_pos_lod_0.push_back(Vector3(-p_size * 2, 0, -p_size));
	_tile_pos_lod_0.push_back(Vector3(-p_size * 2, 0, 0));
	_tile_pos_lod_0.push_back(Vector3(-p_size * 2, 0, p_size));
	_tile_pos_lod_0.push_back(Vector3(-p_size, 0, p_size));
	// Inner tiles
	_tile_pos_lod_0.push_back(Vector3(0, 0, 0));
	_tile_pos_lod_0.push_back(Vector3(-p_size, 0, 0));
	_tile_pos_lod_0.push_back(Vector3(0, 0, -p_size));
	_tile_pos_lod_0.push_back(Vector3(-p_size, 0, -p_size));

	// LOD0 Trims: Fixed 2 unit wide ring around LOD0 tiles.
	_trim_a_pos.push_back(Vector3(p_size * 2, 0, -p_size * 2));
	_trim_a_pos.push_back(Vector3(-p_size * 2 - 2, 0, -p_size * 2 - 2));
	_trim_b_pos.push_back(Vector3(-p_size * 2, 0, -p_size * 2 - 2));
	_trim_b_pos.push_back(Vector3(-p_size * 2 - 2, 0, p_size * 2));

	// LOD1+: 4x4 Ring of mesh size tiles, with one 2 unit wide gap on each axis for fill meshes.
	_tile_pos.push_back(Vector3(2, 0, p_size + 2));
	_tile_pos.push_back(Vector3(p_size + 2, 0, p_size + 2));
	_tile_pos.push_back(Vector3(p_size + 2, 0, -2));
	_tile_pos.push_back(Vector3(p_size + 2, 0, -p_size - 2));
	_tile_pos.push_back(Vector3(p_size + 2, 0, -p_size * 2 - 2));
	_tile_pos.push_back(Vector3(-2, 0, -p_size * 2 - 2));
	_tile_pos.push_back(Vector3(-p_size - 2, 0, -p_size * 2 - 2));
	_tile_pos.push_back(Vector3(-p_size * 2 - 2, 0, -p_size * 2 - 2));
	_tile_pos.push_back(Vector3(-p_size * 2 - 2, 0, -p_size + 2));
	_tile_pos.push_back(Vector3(-p_size * 2 - 2, 0, +2));
	_tile_pos.push_back(Vector3(-p_size * 2 - 2, 0, p_size + 2));
	_tile_pos.push_back(Vector3(-p_size + 2, 0, p_size + 2));

	// Edge offsets set edge pair positions to either both before, straddle, or both after
	// Depending on current LOD position within the next LOD, (via test_x or test_z in snap())
	_offset_a = real_t(p_size * 2) + 2.f;
	_offset_b = real_t(p_size * 2) + 4.f;
	_offset_c = real_t(p_size * 2) + 6.f;
	_edge_pos.push_back(Vector3(_offset_a, _offset_a, -_offset_b));
	_edge_pos.push_back(Vector3(_offset_b, -_offset_b, -_offset_c));

	// Fills: Occupies the gaps between tiles for LOD1+ to complete the ring.
	_fill_a_pos.push_back(Vector3(p_size - 2, 0, -p_size * 2 - 2));
	_fill_a_pos.push_back(Vector3(-p_size - 2, 0, p_size + 2));
	_fill_b_pos.push_back(Vector3(p_size + 2, 0, p_size - 2));
	_fill_b_pos.push_back(Vector3(-p_size * 2 - 2, 0, -p_size - 2));

	return;
}

// Frees all clipmap instance RIDs. Mesh rids must be freed separately.
void WorldScape3DMesher::_clear_clipmap() {
	LOG(INFO, "Freeing all clipmap instances");
	auto rs = RenderingServer::get_singleton();
	for (int lod = 0; lod < _clipmap_rids.size(); lod++) {
		Array lod_array = _clipmap_rids[lod];
		for (int mesh = 0; mesh < lod_array.size(); mesh++) {
			Array mesh_array = lod_array[mesh];
			for (int instance = 0; instance < mesh_array.size(); instance++) {
				rs->free(mesh_array[instance]);
			}
			mesh_array.clear();
		}
		lod_array.clear();
	}
	_clipmap_rids.clear();
	return;
}

// Frees all Mesh RIDs use for clipmap instances.
void WorldScape3DMesher::_clear_mesh_types() {
	LOG(INFO, "Freeing all clipmap meshes");
	auto rs = RenderingServer::get_singleton();
	for (int m = 0; m < _mesh_rids.size(); m++) {
		rs->free(_mesh_rids[m]);
	}
	_mesh_rids.clear();
	return;
}

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DMesher::initialize(WorldScape3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	} else {
		return;
	}
	if (!_terrain->is_inside_world()) {
		LOG(DEBUG, "WorldScape3D's world3D is null");
		return;
	}
	LOG(INFO, "Initializing GeoMesh");
	int size = _terrain->get_mesh_size();
	int lods = _terrain->get_mesh_lods();
	_generate_clipmap(size, lods, _terrain->get_world_3d()->get_scenario());
	update();
	update_aabbs();
	snap(_terrain->get_snapped_position());
}

void WorldScape3DMesher::destroy() {
	LOG(INFO, "Destroying clipmap");
	_clear_clipmap();
	_clear_mesh_types();
	_tile_pos_lod_0.clear();
	_trim_a_pos.clear();
	_trim_b_pos.clear();
	_edge_pos.clear();
	_fill_a_pos.clear();
	_fill_b_pos.clear();
}

void WorldScape3DMesher::snap(const Vector3 &p_tracked_pos) {
	IS_INIT(VOID);
	const real_t mesh_density = _terrain->get_vertex_spacing();
	Vector3 pos = Vector3{ 0.f, 0.f, 0.f };

	const auto rs = RenderingServer::get_singleton();
	for (int lod = 0; lod < _clipmap_rids.size(); ++lod) {
		real_t snap_step = pow(2.f, lod + 1.f) * mesh_density;
		Vector3 lod_scale = Vector3(pow(2.f, lod) * mesh_density, 1.f, pow(2.f, lod) * mesh_density);

		// Snap pos.xz
		pos.x = round(p_tracked_pos.x / snap_step) * snap_step;
		pos.z = round(p_tracked_pos.z / snap_step) * snap_step;

		LOG(EXTREME, "Snapping clipmap LOD", lod, " to position: ", pos);

		// test_x and test_z for edge strip positions
		real_t next_snap_step = pow(2.f, lod + 2.f) * mesh_density;
		real_t next_x = round(p_tracked_pos.x / next_snap_step) * next_snap_step;
		real_t next_z = round(p_tracked_pos.z / next_snap_step) * next_snap_step;
		int test_x = CLAMP(int(round((pos.x - next_x) / snap_step)) + 1, 0, 2);
		int test_z = CLAMP(int(round((pos.z - next_z) / snap_step)) + 1, 0, 2);
		Array lod_array = _clipmap_rids[lod];
		for (int mesh = 0; mesh < lod_array.size(); ++mesh) {
			Array mesh_array = lod_array[mesh];
			for (int instance = 0; instance < mesh_array.size(); ++instance) {
				Transform3D t = Transform3D();
				switch (mesh) {
					case TILE: {
						t.origin = (lod == 0) ? _tile_pos_lod_0[instance] : _tile_pos[instance];
						break;
					}
					case EDGE_A: {
						Vector3 edge_pos_instance = _edge_pos[instance];
						t.origin.z -= _offset_a + (test_z * 2.f);
						t.origin.x = edge_pos_instance[test_x];
						break;
					}
					case EDGE_B: {
						Vector3 edge_pos_instance = _edge_pos[instance];
						t.origin.z = edge_pos_instance[test_z];
						t.origin.x -= _offset_a;
						break;
					}
					// LOD0 does not have fills so the trims share the same index.
					case FILL_A: {
						if (lod > 0) {
							t.origin = _fill_a_pos[instance];
						} else {
							t.origin = _trim_a_pos[instance];
						}
						break;
					}
					case FILL_B: {
						if (lod > 0) {
							t.origin = _fill_b_pos[instance];
						} else {
							t.origin = _trim_b_pos[instance];
						}
						break;
					}
					default: {
						break;
					}
				}
				t = t.scaled(lod_scale);
				t.origin += pos;
				rs->instance_set_transform(mesh_array[instance], t);
#if GODOT_VERSION_MAJOR == 4 && GODOT_VERSION_MINOR == 4
				rs->instance_reset_physics_interpolation(mesh_array[instance]);
#endif
			}
		}
	}
	return;
}

// Iterates over every instance of every mesh and updates all properties.
void WorldScape3DMesher::update() {
	IS_INIT(VOID);
	if (!_terrain->is_inside_world()) {
		LOG(DEBUG, "WorldScape3D's world3D is null");
		return;
	}
	bool baked_light;
	bool dynamic_gi;
	switch (_terrain->get_gi_mode()) {
		case GeometryInstance3D::GI_MODE_DISABLED: {
			baked_light = false;
			dynamic_gi = false;
		} break;
		case GeometryInstance3D::GI_MODE_DYNAMIC: {
			baked_light = false;
			dynamic_gi = true;
		} break;
		case GeometryInstance3D::GI_MODE_STATIC:
		default: {
			baked_light = true;
			dynamic_gi = false;
		} break;
	}

	RID scenario = _terrain->get_world_3d()->get_scenario();
	uint32_t render_layers = _terrain->get_render_layers();
	RenderingServer::ShadowCastingSetting cast_shadows = _terrain->get_cast_shadows();
	bool visible = _terrain->is_visible_in_tree();

	auto rs = RenderingServer::get_singleton();
	LOG(INFO, "Updating all mesh instances for ", _clipmap_rids.size(), " LODs");
	for (int lod = 0; lod < _clipmap_rids.size(); ++lod) {
		Array lod_array = _clipmap_rids[lod];
		for (int mesh = 0; mesh < lod_array.size(); ++mesh) {
			Array mesh_array = lod_array[mesh];
			for (int instance = 0; instance < mesh_array.size(); ++instance) {
				rs->instance_set_visible(mesh_array[instance], visible);
				rs->instance_set_scenario(mesh_array[instance], scenario);
				rs->instance_set_layer_mask(mesh_array[instance], render_layers);
				rs->instance_geometry_set_cast_shadows_setting(mesh_array[instance], cast_shadows);
				rs->instance_geometry_set_flag(mesh_array[instance], RenderingServer::INSTANCE_FLAG_USE_BAKED_LIGHT, baked_light);
				rs->instance_geometry_set_flag(mesh_array[instance], RenderingServer::INSTANCE_FLAG_USE_DYNAMIC_GI, dynamic_gi);
			}
		}
	}
	return;
}

// Iterates over all meshes and updates their AABBs
// All instances of each mesh inherit the updated AABB
void WorldScape3DMesher::update_aabbs() {
	IS_DATA_INIT(VOID);
	real_t cull_margin = _terrain->get_cull_margin();
	Vector2 height_range = _terrain->get_data()->get_height_range();
	height_range.y += abs(height_range.x);

	auto rs = RenderingServer::get_singleton();

	LOG(INFO, "Updating ", _mesh_rids.size(), " meshes AABBs")
	for (int m = 0; m < _mesh_rids.size(); m++) {
		RID mesh = _mesh_rids[m];
		AABB aabb = rs->mesh_get_custom_aabb(mesh);
		aabb.position.y = height_range.x - cull_margin;
		aabb.size.y = height_range.y + cull_margin * 2.f;
		rs->mesh_set_custom_aabb(mesh, aabb);
	}
	return;
}
