/**************************************************************************/
/*  worldscape_3d_instancer.h                                             */
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

#include "scene/3d/multimesh_instance_3d.h"
#include "scene/resources/multimesh.h"

#include <unordered_map>

#include "constants.h"
#include "worldscape_3d_region.h"

class WorldScape3DMeshAsset;
class WorldScape3D;
class WorldScape3DAssets;

class WorldScape3DInstancer : public Object {
	GDCLASS(WorldScape3DInstancer, Object);
	CLASS_NAME();
	friend WorldScape3D;

public: // Constants
	static constexpr int CELL_SIZE = 32;

private:
	WorldScape3D *_terrain = nullptr;

	// MM Resources stored in WorldScape3DRegion::_instances as
	// Region::_instances{mesh_id:int} -> cell{v2i} -> [ TypedArray<Transform3D>, PackedColorArray, modified:bool ]

	// MMI Objects attached to tree, freed in destructor, stored as
	// _mmi_nodes{region_loc} -> mesh{v2i(mesh_id,lod)} -> cell{v2i} -> MultiMeshInstance3D
	typedef std::unordered_map<Vector2i, MultiMeshInstance3D *, Vector2iHash> CellMMIDict;
	typedef std::unordered_map<Vector2i, CellMMIDict, Vector2iHash> MeshMMIDict;
	std::unordered_map<Vector2i, MeshMMIDict, Vector2iHash> _mmi_nodes;

	// Region MMI containers named WorldScape3D/MMI/Region* are stored here as
	// _mmi_containers{region_loc} -> Node3D
	std::unordered_map<Vector2i, Node3D *, Vector2iHash> _mmi_containers;

	uint32_t _density_counter = 0;
	uint32_t _get_density_count(const real_t p_density);

	void _update_mmis(const Vector2i &p_region_loc = V2I_MAX, const int p_mesh_id = -1);
	void _update_vertex_spacing(const real_t p_vertex_spacing);
	void _destroy_mmi_by_cell(const Vector2i &p_region_loc, const int p_mesh_id, const Vector2i p_cell);
	void _destroy_mmi_by_location(const Vector2i &p_region_loc, const int p_mesh_id);
	void _backup_region(const Ref<WorldScape3DRegion> &p_region);
	Ref<MultiMesh> _create_multimesh(const int p_mesh_id, const int p_lod, const TypedArray<Transform3D> &p_xforms = TypedArray<Transform3D>(), const PackedColorArray &p_colors = PackedColorArray()) const;
	Vector2i _get_cell(const Vector3 &p_global_position, const int p_region_size);
	void _setup_mmi_lod_ranges(MultiMeshInstance3D *p_mmi, const Ref<WorldScape3DMeshAsset> &p_ma, const int p_lod);
	//Array _get_usable_height(const Vector3 &p_global_position, const Vector2 &p_slope_range, const bool p_invert, const bool p_on_collision) const;

public:
	~WorldScape3DInstancer() override { destroy(); }

	void initialize(WorldScape3D *p_terrain);
	void destroy();

	void clear_by_mesh(const int p_mesh_id);
	void clear_by_location(const Vector2i &p_region_loc, const int p_mesh_id);
	void clear_by_region(const Ref<WorldScape3DRegion> &p_region, const int p_mesh_id);

	void add_instances(const Vector3 &p_global_position, const Dictionary &p_params);
	void remove_instances(const Vector3 &p_global_position, const Dictionary &p_params);
	void add_multimesh(const int p_mesh_id, const Ref<MultiMesh> &p_multimesh, const Transform3D &p_xform = Transform3D(), const bool p_update = true);
	void add_transforms(const int p_mesh_id, const TypedArray<Transform3D> &p_xforms, const PackedColorArray &p_colors = PackedColorArray(), const bool p_update = true);
	void append_location(const Vector2i &p_region_loc, const int p_mesh_id, const TypedArray<Transform3D> &p_xforms,
			const PackedColorArray &p_colors, const bool p_update = true);
	void append_region(const Ref<WorldScape3DRegion> &p_region, const int p_mesh_id, const TypedArray<Transform3D> &p_xforms,
			const PackedColorArray &p_colors, const bool p_update = true);
	void update_transforms(const AABB &p_aabb);
	void copy_paste_dfr(const WorldScape3DRegion *p_src_region, const Rect2i &p_src_rect, const WorldScape3DRegion *p_dst_region);

	void swap_ids(const int p_src_id, const int p_dst_id);
	void update_mmis(const bool p_rebuild = false);

	void reset_density_counter() { _density_counter = 0; }
	void dump_data();
	void dump_mmis();

protected:
	static void _bind_methods();
};

// Allows us to instance every X function calls for sparse placement
// Modifies _density_counter, not const!
inline uint32_t WorldScape3DInstancer::_get_density_count(const real_t p_density) {
	uint32_t count = 0;
	if (p_density < 1.f && _density_counter++ % uint32_t(1.f / p_density) == 0) {
		count = 1;
	} else if (p_density >= 1.f) {
		count = uint32_t(p_density);
	}
	return count;
}
