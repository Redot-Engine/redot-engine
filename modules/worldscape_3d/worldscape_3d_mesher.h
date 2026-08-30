/**************************************************************************/
/*  worldscape_3d_mesher.h                                                */
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

#include "scene/resources/packed_scene.h"

#include "constants.h"

class WorldScape3D;

class WorldScape3DMesher {
	CLASS_NAME_STATIC("WorldScape3DMesher");

public:
	// Constants
	enum MeshType {
		TILE,
		EDGE_A,
		EDGE_B,
		FILL_A,
		FILL_B,
		STANDARD_TRIM_A,
		STANDARD_TRIM_B,
		STANDARD_TILE,
		STANDARD_EDGE_A,
		STANDARD_EDGE_B,
	};

private:
	WorldScape3D *_terrain = nullptr;
	Vector2 _last_target_position = V2_MAX;

	Array _mesh_rids;
	// LODs -> MeshTypes -> Instances
	Array _clipmap_rids;

	// Mesh offset data
	// LOD0 only
	PackedVector3Array _trim_a_pos;
	PackedVector3Array _trim_b_pos;
	PackedVector3Array _tile_pos_lod_0;
	// LOD1 +
	PackedVector3Array _fill_a_pos;
	PackedVector3Array _fill_b_pos;
	PackedVector3Array _tile_pos;
	// All LOD Levels
	real_t _offset_a = 0.f;
	real_t _offset_b = 0.f;
	real_t _offset_c = 0.f;
	PackedVector3Array _edge_pos;

	void _generate_mesh_types(const int p_mesh_size);
	RID _generate_mesh(const Vector2i &p_size, const bool p_standard_grid = false);
	RID _instantiate_mesh(const PackedVector3Array &p_vertices, const PackedInt32Array &p_indices, const AABB &p_aabb);
	void _generate_clipmap(const int p_size, const int p_lods, const RID &scenario);
	void _generate_offset_data(const int p_mesh_size);

	void _clear_clipmap();
	void _clear_mesh_types();

public:
	WorldScape3DMesher() {}
	~WorldScape3DMesher() { destroy(); }

	void initialize(WorldScape3D *p_terrain);
	void destroy();

	void snap(const Vector3 &p_tracked_pos);
	//void reset_target_position() { _last_target_position = V2_MAX; }
	void update();
	void update_aabbs();
};
