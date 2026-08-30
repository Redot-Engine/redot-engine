/**************************************************************************/
/*  worldscape_3d_instancer.cpp                                           */
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

#include "core/io/resource_saver.h"

#include "constants.h"
#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_instancer.h"
#include "worldscape_3d_region.h"
#include "worldscape_3d_util.h"
#ifdef TOOLS_ENABLED
#include "editor/worldscape_3d_editor.h"
#endif

///////////////////////////
// Private Functions
///////////////////////////

// Creates MMIs based on stored Multimesh data
void WorldScape3DInstancer::_update_mmis(const Vector2i &p_region_loc, const int p_mesh_id) {
	IS_DATA_INIT(VOID);
	LOG(INFO, "Updating MMIs for ", (p_region_loc.x == INT32_MAX) ? "all regions" : "region " + String(p_region_loc),
			(p_mesh_id == -1) ? ", all meshes" : ", mesh " + String::num_int64(p_mesh_id));

	// For specified region_location, or max for all
	Array region_locations;
	if (p_region_loc.x == INT32_MAX) {
		region_locations = _terrain->get_data()->get_region_locations();
	} else {
		region_locations.push_back(p_region_loc);
	}
	for (int r = 0; r < region_locations.size(); r++) {
		Vector2i region_loc = region_locations[r];
		WorldScape3DRegion *region = _terrain->get_data()->get_region_ptr(region_loc);
		if (!region) {
			LOG(WARN, "Errant null region found at: ", region_loc);
			continue;
		}
		Dictionary mesh_inst_dict = region->get_instances();

		// For specified mesh id in that region, or -1 for all
		Array mesh_types;
		if (p_mesh_id < 0) {
			mesh_types = mesh_inst_dict.keys();
		} else {
			mesh_types.push_back(p_mesh_id);
		}
		for (int m = 0; m < mesh_types.size(); m++) {
			int mesh_id = mesh_types[m];

			// Verify mesh id is valid and has some meshes
			Ref<WorldScape3DMeshAsset> ma = _terrain->get_assets()->get_mesh_asset(mesh_id);
			if (ma.is_valid()) {
				if (!ma->is_enabled()) {
					continue;
				}
				if (ma->get_lod_count() == 0) {
					LOG(WARN, "MeshAsset ", mesh_id, " valid but has no meshes, skipping");
					continue;
				}
			} else {
				LOG(WARN, "MeshAsset ", mesh_id, " is null, skipping");
				continue;
			}

			Dictionary cell_inst_dict = mesh_inst_dict[mesh_id];
			Array cell_locations = cell_inst_dict.keys();
			for (int c = 0; c < cell_locations.size(); c++) {
				// Get instances
				Vector2i cell = cell_locations[c];
				Array triple = cell_inst_dict[cell];
				if (triple.size() < 3) {
					LOG(WARN, "Triple is empty");
					continue;
				}
				TypedArray<Transform3D> xforms = triple[0];
				PackedColorArray colors = triple[1];
				bool modified = triple[2];
				if (xforms.size() == 0) {
					LOG(WARN, "Empty cell in region ", region_loc, " cell ", cell);
					continue;
				}

				// Create MMI container if needed
				String rname("Region" + Util::location_to_string(region_loc));
				if (_mmi_containers.count(region_loc) == 0) {
					LOG(DEBUG, "Creating new region MMI container WorldScape3D/MMI/", rname);
					Node3D *node = memnew(Node3D);
					node->set_name(rname);
					_mmi_containers[region_loc] = node;
					_terrain->get_mmi_parent()->add_child(node, true);
				}

				// Setup MMIs for each lod
				MeshMMIDict &mesh_mmi_dict = _mmi_nodes[region_loc];
				Ref<MultiMesh> shadow_impostor_source_mm;

				for (int lod = ma->get_last_lod(); lod >= WorldScape3DMeshAsset::SHADOW_LOD_ID; lod--) {
					// Don't create shadow MMI if not needed
					if (lod == WorldScape3DMeshAsset::SHADOW_LOD_ID) {
						if (ma->get_shadow_impostor() == 0 ||
								ma->get_cast_shadows() == SHADOWS_OFF ||
								ma->get_cast_shadows() == SHADOWS_ONLY) {
							continue;
						}
					}
					// Don't create MMIs for lods > last_shadow_lod in cast shadows mode
					if (lod > ma->get_last_shadow_lod() && ma->get_cast_shadows() == SHADOWS_ONLY) {
						continue;
					}

					// Get or create MMI
					Vector2i mesh_key(mesh_id, lod);
					CellMMIDict &cell_mmi_dict = mesh_mmi_dict[mesh_key];
					MultiMeshInstance3D *mmi;
					if (cell_mmi_dict.count(cell) == 0) {
						mmi = memnew(MultiMeshInstance3D);
						LOG(DEBUG, "No MMI found, Created new MultiMeshInstance3D: ", reinterpret_cast<uint64_t>(mmi));
						// Node name is MMI3D_Cell##_##_Mesh#_LOD#
						String cstring = "_C" + Util::location_to_string(cell).trim_prefix("_");
						String mstring = "_M" + String::num_int64(mesh_id);
						String lstring = "_L" + ((lod == WorldScape3DMeshAsset::SHADOW_LOD_ID) ? "S" : String::num_int64(lod));
						mmi->set_name("MMI3D" + cstring + mstring + lstring);
						mmi->set_as_top_level(true);
						cell_mmi_dict[cell] = mmi;

						//Attach to tree
						Node *node_container = _terrain->get_mmi_parent()->get_node_or_null(rname); // _terrain->get_mmi_parent()->get_node_internal(rname);
						if (!node_container) {
							LOG(ERROR, rname, " isn't attached to the tree.");
							continue;
						}
						node_container->add_child(mmi, true);
						// New MMIs must be updated
						modified = true;
					}
					// If data hasn't changed since last _update_mmis, skip
					if (modified == false) {
						continue;
					}

					// Create MM and assign to MMI
					mmi = cell_mmi_dict[cell];

					// Update instance count: Subtract previous count for this cell
					// FIXME not in stable
					// if (mmi->get_multimesh().is_valid() && lod == ma->get_last_lod()) {
					// 	ma->update_instance_count(-mmi->get_multimesh()->get_instance_count());
					// }
					Ref<MultiMesh> mm;
					if (lod == WorldScape3DMeshAsset::SHADOW_LOD_ID) {
						// Reuse LOD MM as shadow impostor
						mm = shadow_impostor_source_mm;
					} else {
						mm = _create_multimesh(mesh_id, lod, xforms, colors);
					}
					if (mm.is_null()) {
						continue;
					}
					// If LOD is shadow impostor, save it to use in shadow MMI
					if (lod == ma->get_shadow_impostor()) {
						shadow_impostor_source_mm = mm;
					}
					mmi->set_multimesh(mm);
					mmi->set_cast_shadows_setting(ma->get_lod_cast_shadows(lod));
					_setup_mmi_lod_ranges(mmi, ma, lod);
					// Setup materials
					Ref<Material> mat = ma->get_material_override();
					// if (mat.is_valid()) {
					// 	mmi->set_material_override(mat);
					// } else {
					//	mat = ma->get_material_override();
					if (mat.is_valid()) {
						mmi->set_material_override(mat);
					}
					mat = ma->get_material_overlay();
					if (mat.is_valid()) {
						mmi->set_material_overlay(mat);
					}
					// }
					// Reposition the MMI to its region location
					Transform3D t = Transform3D();
					int region_size = region->get_region_size();
					real_t vertex_spacing = _terrain->get_vertex_spacing();
					t.origin.x += region_loc.x * region_size * vertex_spacing;
					t.origin.z += region_loc.y * region_size * vertex_spacing;
					mmi->set_global_transform(t);
					// Update instance count: Add current count for this cell
					// if (lod == ma->get_last_lod()) {
					// 	ma->update_instance_count(mm->get_instance_count());
					// }
					// Clear the cell modified state
					triple[2] = false;
				}

				// FIXME not in stable
				// Set all LOD mmi AABB to match LOD0 to ensure no gaps between transitions.
				// AABB mmi_custom_aabb = AABB();
				// for (int lod = 0; lod <= ma->get_last_lod(); lod++) {
				// 	Vector2i mesh_key(mesh_id, lod);
				// 	CellMMIDict &cell_mmi_dict = mesh_mmi_dict[mesh_key];
				// 	MultiMeshInstance3D *mmi = cell_mmi_dict[cell];
				// 	if (lod == 0) {
				// 		mmi_custom_aabb = mmi->get_aabb();
				// 	} else {
				// 		mmi->set_custom_aabb(mmi_custom_aabb);
				// 	}
				// }
				// if (ma->get_shadow_impostor() > 0) {
				// 	Vector2i mesh_key(mesh_id, WorldScape3DMeshAsset::SHADOW_LOD_ID);
				// 	CellMMIDict &cell_mmi_dict = mesh_mmi_dict[mesh_key];
				// 	MultiMeshInstance3D *mmi = cell_mmi_dict[cell];
				// 	if (mmi) {
				// 		mmi->set_custom_aabb(mmi_custom_aabb);
				// 	}
				// }
			}
		}
	}
}

void WorldScape3DInstancer::_setup_mmi_lod_ranges(MultiMeshInstance3D *p_mmi, const Ref<WorldScape3DMeshAsset> &p_ma, const int p_lod) {
	if (!p_mmi || p_ma.is_null()) {
		return;
	}
	real_t margin = p_ma->get_fade_margin();
	if (margin > 0.f) {
		real_t lod_begin = p_ma->get_lod_range_begin(p_lod);
		lod_begin = MAX(lod_begin < 0.001f ? 0.f : lod_begin - margin, 0.f);
		real_t lod_end = p_ma->get_lod_range_end(p_lod);
		lod_end = MAX(lod_end < 0.001f ? 0.f : lod_end + margin, 0.f);
		p_mmi->set_visibility_range_begin(lod_begin);
		p_mmi->set_visibility_range_end(lod_end);
		p_mmi->set_visibility_range_begin_margin(lod_begin < 0.001f ? 0.f : margin);
		p_mmi->set_visibility_range_end_margin(lod_end < 0.001f ? 0.f : margin);
		p_mmi->set_visibility_range_fade_mode(GeometryInstance3D::VISIBILITY_RANGE_FADE_SELF);
	} else {
		p_mmi->set_visibility_range_begin(p_ma->get_lod_range_begin(p_lod));
		// MMI ranges have gaps. Some users experienced worse gaps with billboards (assumed to be last lod)
		const float lod_overlap = (p_lod < p_ma->get_last_lod() - 1) ? 1.0005f : 1.0024f;
		p_mmi->set_visibility_range_end(p_ma->get_lod_range_end(p_lod) * lod_overlap);
	}
}

void WorldScape3DInstancer::_update_vertex_spacing(const real_t p_vertex_spacing) {
	IS_DATA_INIT(VOID);
	Array region_locations = _terrain->get_data()->get_region_locations();
	for (int r = 0; r < region_locations.size(); r++) {
		Vector2i region_loc = region_locations[r];
		WorldScape3DRegion *region = _terrain->get_data()->get_region_ptr(region_loc);
		if (!region) {
			LOG(WARN, "Errant null region found at: ", region_loc);
			continue;
		}
		real_t old_spacing = region->get_vertex_spacing();
		if (old_spacing == p_vertex_spacing) {
			LOG(DEBUG, "region vertex spacing == vertex spacing, skipping update transform spacing for region at: ", region_loc);
			continue;
		}

		// For all mesh_ids in region
		Dictionary mesh_inst_dict = region->get_instances();
		LOG(DEBUG, "Updating MMIs from: ", region_loc);
		Array mesh_types = mesh_inst_dict.keys();
		for (int m = 0; m < mesh_types.size(); m++) {
			int mesh_id = mesh_types[m];
			Dictionary cell_inst_dict = mesh_inst_dict[mesh_id];
			Array cell_locations = cell_inst_dict.keys();
			for (int c = 0; c < cell_locations.size(); c++) {
				// Get instances
				Vector2i cell = cell_locations[c];
				Array triple = cell_inst_dict[cell];
				TypedArray<Transform3D> xforms = triple[0];
				// Descale, then Scale to the new value
				for (int i = 0; i < xforms.size(); i++) {
					Transform3D t = xforms[i];
					t.origin.x /= old_spacing;
					t.origin.x *= p_vertex_spacing;
					t.origin.z /= old_spacing;
					t.origin.z *= p_vertex_spacing;
					xforms[i] = t;
				}
				triple[0] = xforms;
				triple[2] = true;
				cell_inst_dict[cell] = triple;
			}
		}
		// After all transforms are updated, set the new region vertex spacing value
		region->set_vertex_spacing(p_vertex_spacing);
		region->set_modified(true);
	}
	destroy();
	_update_mmis();
}

void WorldScape3DInstancer::_destroy_mmi_by_cell(const Vector2i &p_region_loc, const int p_mesh_id, const Vector2i p_cell) {
	if (_mmi_nodes.count(p_region_loc) == 0) {
		return;
	}
	MeshMMIDict &mesh_mmi_dict = _mmi_nodes[p_region_loc];

	//Ref<WorldScape3DMeshAsset> ma = _terrain->get_assets()->get_mesh_asset(p_mesh_id);

	for (int lod = WorldScape3DMeshAsset::SHADOW_LOD_ID; lod < WorldScape3DMeshAsset::MAX_LOD_COUNT; lod++) {
		Vector2i mesh_key(p_mesh_id, lod);
		if (mesh_mmi_dict.count(mesh_key) == 0) {
			continue;
		}

		CellMMIDict &cell_mmi_dict = mesh_mmi_dict[mesh_key];
		if (cell_mmi_dict.count(p_cell) == 0) {
			continue;
		}

		MultiMeshInstance3D *mmi = cell_mmi_dict[p_cell];
		// if (ma.is_valid() && mmi && mmi->get_multimesh().is_valid()) {
		// 	if (lod == ma->get_last_lod()) {
		// 		ma->update_instance_count(-mmi->get_multimesh()->get_instance_count());
		// 	}
		// }

		LOG(EXTREME, "Freeing ", ptr_to_str(mmi), " and erasing mmi cell ", p_cell);
		remove_from_tree(mmi);
		memdelete_safely(mmi);
		cell_mmi_dict.erase(p_cell);
		if (cell_mmi_dict.empty()) {
			LOG(EXTREME, "Removing mesh ", mesh_key, " from cell MMI dictionary");
			mesh_mmi_dict.erase(mesh_key); // invalidates cell_mmi_dict
		}
	}

	if (mesh_mmi_dict.empty()) {
		LOG(EXTREME, "Removing region ", p_region_loc, " from mesh MMI dictionary");
		if (_mmi_containers.count(p_region_loc) > 0) {
			Node *node = _mmi_containers[p_region_loc];
			if (node && node->get_child_count() == 0) {
				LOG(EXTREME, "Removing ", node->get_name());
				_mmi_containers.erase(p_region_loc);
				remove_from_tree(node);
				memdelete_safely(node);
			}
		}
		_mmi_nodes.erase(p_region_loc); // invalidates mesh_mmi_dict
		return;
	}
}

void WorldScape3DInstancer::_destroy_mmi_by_location(const Vector2i &p_region_loc, const int p_mesh_id) {
	LOG(DEBUG, "Deleting all MMIs in region: ", p_region_loc, " for mesh_id: ", p_mesh_id);
	if (_mmi_nodes.count(p_region_loc) == 0) {
		return;
	}
	MeshMMIDict &mesh_mmi_dict = _mmi_nodes[p_region_loc];

	for (int lod = WorldScape3DMeshAsset::SHADOW_LOD_ID; lod < WorldScape3DMeshAsset::MAX_LOD_COUNT; lod++) {
		Vector2i mesh_key(p_mesh_id, lod);
		CellMMIDict &cell_mmi_dict = mesh_mmi_dict[mesh_key];

		// Iterate over keys as functions will invalidate standard iterator
		std::vector<Vector2i> keys;
		keys.reserve(cell_mmi_dict.size());
		for (auto &it : cell_mmi_dict) {
			keys.push_back(it.first);
		}
		for (auto &cell : keys) {
			_destroy_mmi_by_cell(p_region_loc, p_mesh_id, cell);
		}
	}
}

void WorldScape3DInstancer::_backup_region(const Ref<WorldScape3DRegion> &p_region) {
	if (p_region.is_null()) {
		return;
	}
#ifdef TOOLS_ENABLED
	if (_terrain && _terrain->get_editor() && _terrain->get_editor()->is_operating()) {
		_terrain->get_editor()->backup_region(p_region);
	} else {
#endif
		p_region->set_modified(true);
#ifdef TOOLS_ENABLED
	}
#endif
}

Ref<MultiMesh> WorldScape3DInstancer::_create_multimesh(const int p_mesh_id, const int p_lod, const TypedArray<Transform3D> &p_xforms, const PackedColorArray &p_colors) const {
	Ref<MultiMesh> mm;
	IS_INIT(mm);
	Ref<WorldScape3DMeshAsset> mesh_asset = _terrain->get_assets()->get_mesh_asset(p_mesh_id);
	if (mesh_asset.is_null()) {
		LOG(ERROR, "No mesh id ", p_mesh_id, " found");
		return mm;
	}
	Ref<Mesh> mesh = mesh_asset->get_mesh(p_lod);
	if (mesh.is_null()) {
		LOG(ERROR, "No LOD ", p_lod, " for mesh id ", p_mesh_id, " found");
		return mm;
	}
	mm.instantiate();
	mm->set_transform_format(MultiMesh::TRANSFORM_3D);
	mm->set_use_colors(true);
	mm->set_mesh(mesh);
	if (p_xforms.size() > 0) {
		mm->set_instance_count(p_xforms.size());
		for (int i = 0; i < p_xforms.size(); i++) {
			mm->set_instance_transform(i, p_xforms[i]);
			if (i < p_colors.size()) {
				mm->set_instance_color(i, p_colors[i]);
			}
		}
	}
	return mm;
}

Vector2i WorldScape3DInstancer::_get_cell(const Vector3 &p_global_position, const int p_region_size) {
	real_t vertex_spacing = _terrain->get_vertex_spacing();
	Vector2i cell;
	cell.x = VariantUtilityFunctions::posmod(VariantUtilityFunctions::floori(p_global_position.x / vertex_spacing), p_region_size) / CELL_SIZE;
	cell.y = VariantUtilityFunctions::posmod(VariantUtilityFunctions::floori(p_global_position.z / vertex_spacing), p_region_size) / CELL_SIZE;
	return cell;
}

// Get appropriate terrain height. Could find terrain (excluding slope or holes) or optional collision
// Array WorldScape3DInstancer::_get_usable_height(const Vector3 &p_global_position, const Vector2 &p_slope_range, const bool p_invert, const bool p_on_collision) const {
// 	IS_DATA_INIT(Array());
// 	WorldScape3DData *data = _terrain->get_data();
// 	real_t height = data->get_height(p_global_position);
// 	Dictionary raycast_result;
// 	bool raycast_hit = false;
// 	real_t raycast_height = FLT_MIN;
// 	Vector3 raycast_normal = Vector3(0.f, 1.f, 0.f);
// 	// Raycast physics if using on_collision
// 	if (p_on_collision) {
// 		raycast_result = _terrain->get_raycast_result(p_global_position + Vector3(0.0f, 100.0f, 0.0f), Vector3(0.0f, -200.0f, 0.0f));
// 		if (raycast_result.has("position")) {
// 			raycast_hit = true;
// 			raycast_height = ((Vector3)raycast_result["position"]).y;
// 			raycast_normal = raycast_result["normal"];
// 		}
// 	}
// 	// Hole, use collision if can or quit
// 	if (std::isnan(height)) {
// 		if (!raycast_hit) {
// 			return Array();
// 		}
// 		height = raycast_height;
// 	}
// 	// No hole, use collision if higher
// 	else if (raycast_hit && raycast_height > height) {
// 		height = raycast_height;
// 	}
// 	// No hole or collision, use height if in slope or quit
// 	else if (!data->is_in_slope(p_global_position, p_slope_range, p_invert)) {
// 		return Array();
// 	}
// 	Array triple;
// 	triple.resize(3);
// 	triple[0] = height;
// 	triple[1] = raycast_hit;
// 	triple[2] = raycast_normal;
// 	return triple;
// }

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DInstancer::initialize(WorldScape3D *p_terrain) {
	if (p_terrain) {
		_terrain = p_terrain;
	}
	IS_DATA_INIT_MESG("WorldScape3D not initialized yet", VOID);
	LOG(INFO, "Initializing Instancer");
	_update_mmis();
}

void WorldScape3DInstancer::destroy() {
	IS_DATA_INIT(VOID);
	LOG(INFO, "Destroying all MMIs");

	// Iterate over keys as subfunction will invalidate standard iterator
	std::vector<Vector2i> keys;
	keys.reserve(_mmi_nodes.size());
	//int i = 0;
	for (auto &it : _mmi_nodes) {
		keys.push_back(it.first);
		//i++;
	}
	int mesh_count = _terrain->get_assets()->get_mesh_count();
	for (auto &region_loc : keys) {
		for (int m = 0; m < mesh_count; m++) {
			_destroy_mmi_by_location(region_loc, m);
		}
	}
}

void WorldScape3DInstancer::clear_by_mesh(const int p_mesh_id) {
	LOG(INFO, "Deleting Multimeshes in all regions with mesh_id: ", p_mesh_id);
	Array region_locations = _terrain->get_data()->get_region_locations();
	for (int i = 0; i < region_locations.size(); i++) {
		clear_by_location(region_locations[i], p_mesh_id);
	}
	// Ref<WorldScape3DMeshAsset> ma = _terrain->get_assets()->get_mesh_asset(p_mesh_id);
	// ma.is_valid() ? ma->set_instance_count(0) : void(); // Reset count for this mesh
}

void WorldScape3DInstancer::clear_by_location(const Vector2i &p_region_loc, const int p_mesh_id) {
	LOG(INFO, "Deleting Multimeshes w/ mesh_id: ", p_mesh_id, " in region: ", p_region_loc);
	Ref<WorldScape3DRegion> region = _terrain->get_data()->get_region(p_region_loc);
	clear_by_region(region, p_mesh_id);
}

void WorldScape3DInstancer::clear_by_region(const Ref<WorldScape3DRegion> &p_region, const int p_mesh_id) {
	if (p_region.is_null()) {
		LOG(ERROR, "Region is null");
		return;
	}
	Vector2i region_loc = p_region->get_location();
	LOG(INFO, "Deleting Multimeshes w/ mesh_id: ", p_mesh_id, " in region: ", region_loc);
	Dictionary mesh_inst_dict = p_region->get_instances();
	if (mesh_inst_dict.has(p_mesh_id)) {
		_backup_region(p_region);
		mesh_inst_dict.erase(p_mesh_id);
	}
	_destroy_mmi_by_location(region_loc, p_mesh_id);
}

void WorldScape3DInstancer::add_instances(const Vector3 &p_global_position, const Dictionary &p_params) {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);

	int mesh_id = p_params.get("asset_id", 0);
	if (mesh_id < 0 || mesh_id >= _terrain->get_assets()->get_mesh_count()) {
		LOG(ERROR, "Mesh ID out of range: ", mesh_id, ", valid: 0 to ", _terrain->get_assets()->get_mesh_count() - 1);
		return;
	}
	Ref<WorldScape3DMeshAsset> mesh_asset = _terrain->get_assets()->get_mesh_asset(mesh_id);

	real_t brush_size = CLAMP(real_t(p_params.get("size", 10.f)), 0.1f, 4096.f); // Meters
	real_t radius = brush_size * .4f; // Ring1's inner radius
	real_t strength = CLAMP(real_t(p_params.get("strength", .1f)), .01f, 100.f); // (premul) 1-10k%
	real_t fixed_scale = CLAMP(real_t(p_params.get("fixed_scale", 100.f)) * .01f, .01f, 100.f); // 1-10k%
	real_t random_scale = CLAMP(real_t(p_params.get("random_scale", 0.f)) * .01f, 0.f, 10.f); // +/- 1000%
	real_t density = CLAMP(.1f * brush_size * strength * mesh_asset->get_density() /
					MAX(0.01f, fixed_scale + .5f * random_scale),
			.001f, 1000.f);

	// Density based on strength, mesh AABB and input scale determines how many to place, even fractional
	uint32_t count = _get_density_count(density);
	if (count <= 0) {
		return;
	}
	LOG(EXTREME, "Adding ", count, " instances at ", p_global_position);

	real_t fixed_spin = CLAMP(real_t(p_params.get("fixed_spin", 0.f)), .0f, 360.f); // degrees
	real_t random_spin = CLAMP(real_t(p_params.get("random_spin", 360.f)), 0.f, 360.f); // degrees
	real_t fixed_tilt = CLAMP(real_t(p_params.get("fixed_tilt", 0.f)), -180.f, 180.f); // degrees
	real_t random_tilt = CLAMP(real_t(p_params.get("random_tilt", 10.f)), 0.f, 180.f); // degrees
	bool align_to_normal = bool(p_params.get("align_to_normal", false));

	real_t height_offset = CLAMP(real_t(p_params.get("height_offset", 0.f)), -100.0f, 100.f); // meters
	real_t random_height = CLAMP(real_t(p_params.get("random_height", 0.f)), 0.f, 100.f); // meters

	Color vertex_color = Color(p_params.get("vertex_color", COLOR_WHITE));
	real_t random_hue = CLAMP(real_t(p_params.get("random_hue", 0.f)) / 360.f, 0.f, 1.f); // degrees -> 0-1
	real_t random_darken = CLAMP(real_t(p_params.get("random_darken", 0.f)) * .01f, 0.f, 1.f); // 0-100%

	Vector2 slope_range = p_params["slope"]; // 0-90 degrees already clamped in Editor
	//bool on_collision = bool(p_params.get("on_collision", false));
	bool invert = p_params["modifier_alt"];
	WorldScape3DData *data = _terrain->get_data();

	TypedArray<Transform3D> xforms;
	PackedColorArray colors;
	for (size_t i = 0; i < count; i++) {
		Transform3D t;

		// Get random XZ position and height in a circle
		real_t r_radius = radius * sqrt(VariantUtilityFunctions::randf());
		real_t r_theta = VariantUtilityFunctions::randf() * Math_TAU;
		Vector3 rand_vec = Vector3(r_radius * cos(r_theta), 0.f, r_radius * sin(r_theta));
		Vector3 position = p_global_position + rand_vec;

		// Get height, but skip holes
		real_t height = data->get_height(position);
		if (std::isnan(height)) {
			continue;
		} else {
			position.y = height;
		}
		if (!data->is_in_slope(position, slope_range, invert)) {
			continue;
		}
		// Orientation
		Vector3 normal = Vector3(0.f, 1.f, 0.f);
		if (align_to_normal) {
			normal = data->get_normal(position);
			if (std::isnan(normal.x)) {
				normal = Vector3(0.f, 1.f, 0.f);
			} else {
				normal = normal.normalized();
				Vector3 z_axis = Vector3(0.f, 0.f, 1.f);
				Vector3 x_axis = -z_axis.cross(normal);
				t.basis = Basis(x_axis, normal, z_axis).orthonormalized();
			}
		}
		real_t spin = (fixed_spin + random_spin * VariantUtilityFunctions::randf()) * Math_PI / 180.f;
		if (Math::abs(spin) > 0.001f) {
			t.basis = t.basis.rotated(normal, spin);
		}
		real_t tilt = (fixed_tilt + random_tilt * (2.f * VariantUtilityFunctions::randf() - 1.f)) * Math_PI / 180.f;
		if (Math::abs(tilt) > 0.001f) {
			t.basis = t.basis.rotated(t.basis.get_column(0), tilt); // Rotate pitch, X-axis
		}

		// Scale
		real_t t_scale = CLAMP(fixed_scale + random_scale * (2.f * VariantUtilityFunctions::randf() - 1.f), 0.01f, 10.f);
		t = t.scaled(Vector3(t_scale, t_scale, t_scale));

		// Position. mesh_asset height offset added in add_transforms
		real_t offset = height_offset + random_height * (2.f * VariantUtilityFunctions::randf() - 1.f);
		position += t.basis.get_column(1) * offset; // Offset along UP axis
		t = t.translated(position);

		// Color
		Color col = vertex_color;
		col.set_v(CLAMP(col.get_v() - random_darken * VariantUtilityFunctions::randf(), 0.f, 1.f));
		col.set_h(fmod(col.get_h() + random_hue * (2.f * VariantUtilityFunctions::randf() - 1.f), 1.f));

		xforms.push_back(t);
		colors.push_back(col);
	}

	// Append multimesh
	if (xforms.size() > 0) {
		add_transforms(mesh_id, xforms, colors);
	}
}

void WorldScape3DInstancer::remove_instances(const Vector3 &p_global_position, const Dictionary &p_params) {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);

	int mesh_id = p_params.get("asset_id", 0);
	int mesh_count = _terrain->get_assets()->get_mesh_count();
	if (mesh_id < 0 || mesh_id >= mesh_count) {
		LOG(ERROR, "Mesh ID out of range: ", mesh_id, ", valid: 0 to ", _terrain->get_assets()->get_mesh_count() - 1);
		return;
	}

	bool modifier_shift = p_params.get("modifier_shift", false);
	real_t brush_size = CLAMP(real_t(p_params.get("size", 10.f)), .5f, 4096.f); // Meters
	real_t half_brush_size = brush_size * 0.5f + 1.f; // 1m margin
	real_t radius = brush_size * .4f; // Ring1's inner radius
	real_t strength = CLAMP(real_t(p_params.get("strength", .1f)), .01f, 100.f); // (premul) 1-10k%
	//real_t fixed_scale = CLAMP(real_t(p_params.get("fixed_scale", 100.f)) * .01f, .01f, 100.f); // 1-10k%
	//real_t random_scale = CLAMP(real_t(p_params.get("random_scale", 0.f)) * .01f, 0.f, 10.f); // +/- 1000%

	Vector2 slope_range = p_params["slope"]; // 0-90 degrees already clamped in Editor
	bool invert = p_params["modifier_alt"];
	WorldScape3DData *data = _terrain->get_data();
	int region_size = _terrain->get_region_size();
	real_t vertex_spacing = _terrain->get_vertex_spacing();

	// Build list of potential regions to search, rather than searching the entire terrain, calculate possible regions covered
	// and check if they are valid; if so add that location to the dictionary keys.
	Dictionary r_locs;
	// Calculate step distance to ensure every region is checked inside the bounds of brush size.
	real_t step = brush_size / ceil(brush_size / real_t(region_size) / vertex_spacing);
	for (real_t x = p_global_position.x - half_brush_size; x <= p_global_position.x + half_brush_size; x += step) {
		for (real_t z = p_global_position.z - half_brush_size; z <= p_global_position.z + half_brush_size; z += step) {
			Vector2i region_loc = data->get_region_location(Vector3(x, 0.f, z));
			if (data->has_region(region_loc)) {
				r_locs[region_loc] = 1;
			}
		}
	}

	Array region_queue = r_locs.keys();
	if (region_queue.size() == 0) {
		return;
	}

	for (int r = 0; r < region_queue.size(); r++) {
		Vector2i region_loc = region_queue[r];
		Ref<WorldScape3DRegion> region = data->get_region(region_loc);
		if (region.is_null()) {
			LOG(WARN, "Errant null region found at: ", region_loc);
			continue;
		}

		Dictionary mesh_inst_dict = region->get_instances();
		Array mesh_types = mesh_inst_dict.keys();
		if (mesh_types.size() == 0) {
			continue;
		}
		Vector3 global_local_offset = Vector3(region_loc.x * region_size * vertex_spacing, 0.f, region_loc.y * region_size * vertex_spacing);
		Vector2 localised_ring_center = Vector2(p_global_position.x - global_local_offset.x, p_global_position.z - global_local_offset.z);
		// For this mesh id, or all mesh ids
		for (int m = (modifier_shift ? 0 : mesh_id); m <= (modifier_shift ? mesh_count - 1 : mesh_id); m++) {
			// Ensure this region has this mesh
			if (!mesh_inst_dict.has(m)) {
				continue;
			}
			Dictionary cell_inst_dict = mesh_inst_dict[m];
			Array cell_locations = cell_inst_dict.keys();
			// This shouldn't be empty
			if (cell_locations.size() == 0) {
				LOG(WARN, "Region at: ", region_loc, " has instance dictionary for mesh id: ", m, " but has no cells.")
				continue;
			}
			// Check potential cells rather than searching the entire region, whilst marginally
			// slower if there are very few cells for the given mesh present. It is significantly
			// faster when a large number of cells are present.
			Dictionary c_locs;
			// Calculate step distance to ensure every cell is checked inside the bounds of brush size.
			real_t cell_step = brush_size / ceil(brush_size / real_t(CELL_SIZE) / vertex_spacing);
			for (real_t x = p_global_position.x - half_brush_size; x <= p_global_position.x + half_brush_size; x += cell_step) {
				for (real_t z = p_global_position.z - half_brush_size; z <= p_global_position.z + half_brush_size; z += cell_step) {
					Vector3 cell_pos = Vector3(x, 0.f, z) - global_local_offset;
					// Manually calculate cell pos without modulus, locations not in the current region will not be found.
					Vector2i cell_loc;
					cell_loc.x = VariantUtilityFunctions::floori(cell_pos.x / vertex_spacing) / CELL_SIZE;
					cell_loc.y = VariantUtilityFunctions::floori(cell_pos.z / vertex_spacing) / CELL_SIZE;
					if (cell_locations.has(cell_loc)) {
						c_locs[cell_loc] = 1;
					}
				}
			}
			Array cell_queue = c_locs.keys();
			if (cell_queue.size() == 0) {
				continue;
			}
			Ref<WorldScape3DMeshAsset> mesh_asset = _terrain->get_assets()->get_mesh_asset(m);
			real_t mesh_height_offset = mesh_asset->get_height_offset();
			for (int c = 0; c < cell_queue.size(); c++) {
				Vector2i cell = cell_queue[c];
				Array triple = cell_inst_dict[cell];
				TypedArray<Transform3D> xforms = triple[0];
				PackedColorArray colors = triple[1];
				TypedArray<Transform3D> updated_xforms;
				PackedColorArray updated_colors;
				// Remove transforms if inside ring radius
				for (int i = 0; i < xforms.size(); i++) {
					Transform3D t = xforms[i];
					// Use localized ring center
					real_t radial_distance = localised_ring_center.distance_to(Vector2(t.origin.x, t.origin.z));
					Vector3 height_offset = t.basis.get_column(1) * mesh_height_offset;
					if (radial_distance < radius &&
							VariantUtilityFunctions::randf() < CLAMP(0.175f * strength, 0.005f, 10.f) &&
							data->is_in_slope(t.origin + global_local_offset - height_offset, slope_range, invert)) {
						_backup_region(region);
						continue;
					} else {
						updated_xforms.push_back(t);
						updated_colors.push_back(colors[i]);
					}
				}
				if (updated_xforms.size() > 0) {
					triple[0] = updated_xforms;
					triple[1] = updated_colors;
					triple[2] = true;
					cell_inst_dict[cell] = triple;
				} else {
					cell_inst_dict.erase(cell);
					_destroy_mmi_by_cell(region_loc, m, cell);
				}
			}
			if (cell_inst_dict.is_empty()) {
				mesh_inst_dict.erase(m);
			}
		}
		_update_mmis(region_loc);
	}
}

void WorldScape3DInstancer::add_multimesh(const int p_mesh_id, const Ref<MultiMesh> &p_multimesh, const Transform3D &p_xform, const bool p_update) {
	LOG(INFO, "Extracting ", p_multimesh->get_instance_count(), " transforms from multimesh");
	TypedArray<Transform3D> xforms;
	PackedColorArray colors;
	for (int i = 0; i < p_multimesh->get_instance_count(); i++) {
		xforms.push_back(p_xform * p_multimesh->get_instance_transform(i));
		Color c = COLOR_WHITE;
		if (p_multimesh->is_using_colors()) {
			c = p_multimesh->get_instance_color(i);
		}
		colors.push_back(c);
	}
	add_transforms(p_mesh_id, xforms, colors, p_update);
}

// Expects transforms in global space
void WorldScape3DInstancer::add_transforms(const int p_mesh_id, const TypedArray<Transform3D> &p_xforms, const PackedColorArray &p_colors, const bool p_update) {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);
	if (p_xforms.size() == 0) {
		return;
	}
	if (p_mesh_id < 0 || p_mesh_id >= _terrain->get_assets()->get_mesh_count()) {
		LOG(ERROR, "Mesh ID out of range: ", p_mesh_id, ", valid: 0 to ", _terrain->get_assets()->get_mesh_count() - 1);
		return;
	}

	Dictionary xforms_dict;
	Dictionary colors_dict;
	Ref<WorldScape3DMeshAsset> mesh_asset = _terrain->get_assets()->get_mesh_asset(p_mesh_id);

	// Separate incoming transforms/colors by region Dict{ region_loc => Array() }
	LOG(INFO, "Separating ", p_xforms.size(), " transforms and ", p_colors.size(), " colors into regions");
	for (int i = 0; i < p_xforms.size(); i++) {
		// Get adjusted xform/color
		Transform3D trns = p_xforms[i];
		trns.origin += trns.basis.get_column(1) * mesh_asset->get_height_offset(); // Offset along UP axis
		Color col = COLOR_WHITE;
		if (p_colors.size() > i) {
			col = p_colors[i];
		}

		// Store by region offset
		Vector2i region_loc = _terrain->get_data()->get_region_location(trns.origin);
		if (!xforms_dict.has(region_loc)) {
			xforms_dict[region_loc] = TypedArray<Transform3D>();
			colors_dict[region_loc] = PackedColorArray();
		}
		TypedArray<Transform3D> xforms = xforms_dict[region_loc];
		PackedColorArray colors = colors_dict[region_loc];
		xforms.push_back(trns);
		colors.push_back(col);
		colors_dict[region_loc] = colors; // Note similar bug as godot-cpp#1149 needs this for PCA
	}

	// Merge incoming transforms with existing transforms
	Array region_locations = xforms_dict.keys();
	for (int i = 0; i < region_locations.size(); i++) {
		Vector2i region_loc = region_locations[i];
		TypedArray<Transform3D> xforms = xforms_dict[region_loc];
		PackedColorArray colors = colors_dict[region_loc];
		LOG(MESG, "Appending ", xforms.size(), " xforms, ", colors, " colors to region location: ", region_loc);
		append_location(region_loc, p_mesh_id, xforms, colors, p_update);
	}
}

// Appends new global transforms to existing cells, offsetting transforms to region space, scaled by vertex spacing
void WorldScape3DInstancer::append_location(const Vector2i &p_region_loc, const int p_mesh_id,
		const TypedArray<Transform3D> &p_xforms, const PackedColorArray &p_colors, const bool p_update) {
	IS_DATA_INIT(VOID);
	Ref<WorldScape3DRegion> region = _terrain->get_data()->get_region(p_region_loc);
	if (region.is_null()) {
		return;
	}
	int region_size = region->get_region_size();
	real_t vertex_spacing = _terrain->get_vertex_spacing();
	Vector2 global_local_offset = Vector2(p_region_loc.x * region_size * vertex_spacing, p_region_loc.y * region_size * vertex_spacing);
	TypedArray<Transform3D> localised_xforms;
	for (int i = 0; i < p_xforms.size(); i++) {
		Transform3D t = p_xforms[i];
		// Localize the transform to "region space"
		t.origin.x -= global_local_offset.x;
		t.origin.z -= global_local_offset.y;
		localised_xforms.push_back(t);
	}
	append_region(region, p_mesh_id, localised_xforms, p_colors, p_update);
}

// append_region requires all transforms are in region space, 0 - region_size * vertex_spacing
void WorldScape3DInstancer::append_region(const Ref<WorldScape3DRegion> &p_region, const int p_mesh_id,
		const TypedArray<Transform3D> &p_xforms, const PackedColorArray &p_colors, const bool p_update) {
	if (p_region.is_null()) {
		LOG(ERROR, "Null region provided. Doing nothing.");
		return;
	}
	if (p_xforms.size() == 0) {
		LOG(ERROR, "No transforms to add. Doing nothing.");
		return;
	}

	_backup_region(p_region);

	Dictionary cell_locations = p_region->get_instances()[p_mesh_id];
	int region_size = p_region->get_region_size();

	for (int i = 0; i < p_xforms.size(); i++) {
		Transform3D xform = p_xforms[i];
		Color col = p_colors[i];
		Vector2i cell = _get_cell(xform.origin, region_size);

		// Get current instance arrays or create if none
		Array triple = cell_locations[cell];
		bool modified = true;
		if (triple.size() != 3) {
			LOG(DEBUG, "No data at ", p_region->get_location(), ":", cell, ". Creating triple");
			triple.resize(3);
			triple[0] = TypedArray<Transform3D>();
			triple[1] = PackedColorArray();
			triple[2] = modified;
		}
		TypedArray<Transform3D> xforms = triple[0];
		PackedColorArray colors = triple[1];
		xforms.push_back(xform);
		colors.push_back(col);

		// Must write back since there are copy constructors somewhere
		// see godot-cpp#1149
		triple[0] = xforms;
		triple[1] = colors;
		triple[2] = modified;
		cell_locations[cell] = triple;
	}

	// Write back dictionary. See above comments
	p_region->get_instances()[p_mesh_id] = cell_locations;
	if (p_update) {
		_update_mmis(p_region->get_location(), p_mesh_id);
	}
}

// Review all transforms in one area and adjust their transforms w/ the current height
void WorldScape3DInstancer::update_transforms(const AABB &p_aabb) {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);
	Rect2 rect = aabb2rect(p_aabb);
	LOG(EXTREME, "Updating transforms within ", rect);
	Vector2 global_position = rect.get_center();
	Vector2 size = rect.get_size();
	Vector2 half_size = size * 0.5f + Vector2(1.f, 1.f); // 1m margin
	if (size == V2_ZERO) {
		return;
	}

	WorldScape3DData *data = _terrain->get_data();
	//bool on_collision = _terrain->get_editor() ? (bool)(_terrain->get_editor()->get_brush_data().get("on_collision", false)) : false;
	int region_size = _terrain->get_region_size();
	real_t vertex_spacing = _terrain->get_vertex_spacing();

	// Build list of valid regions within AABB; add the locations as dictionary keys.
	Dictionary r_locs;
	// Calculate step distance to ensure every region is checked inside the bounds of AABB size.
	Vector2 step = Vector2(size.x / ceil(size.x / real_t(region_size) / vertex_spacing), size.y / ceil(size.y / real_t(region_size) / vertex_spacing));
	for (real_t x = global_position.x - half_size.x; x <= global_position.x + half_size.x; x += step.x) {
		for (real_t z = global_position.y - half_size.y; z <= global_position.y + half_size.y; z += step.y) {
			Vector2i region_loc = data->get_region_location(Vector3(x, 0.f, z));
			if (data->has_region(region_loc)) {
				r_locs[region_loc] = 0;
			}
		}
	}

	Array region_queue = r_locs.keys();
	if (region_queue.size() == 0) {
		return;
	}

	for (int r = 0; r < region_queue.size(); r++) {
		Vector2i region_loc = region_queue[r];
		Ref<WorldScape3DRegion> region = data->get_region(region_loc);
		_backup_region(region);

		Dictionary mesh_inst_dict = region->get_instances();
		Array mesh_types = mesh_inst_dict.keys();
		if (mesh_types.size() == 0) {
			continue;
		}
		Vector3 global_local_offset = Vector3(region_loc.x * region_size * vertex_spacing, 0.f, region_loc.y * region_size * vertex_spacing);

		// For this mesh id, or all mesh ids
		for (int m = 0; m < mesh_types.size(); m++) {
			// Check potential cells rather than searching the entire region, whilst marginally
			// slower if there are very few cells for the given mesh present it is significantly
			// faster when a very large number of cells are present.
			int region_mesh_id = mesh_types[m];
			Dictionary cell_inst_dict = mesh_inst_dict[region_mesh_id];
			Array cell_locations = cell_inst_dict.keys();
			if (cell_locations.size() == 0) {
				continue;
			}
			Dictionary c_locs;
			// Calculate step distance to ensure every cell is checked inside the bounds of brush size.
			Vector2 cell_step = Vector2(size.x / ceil(size.x / real_t(CELL_SIZE) / vertex_spacing), size.y / ceil(size.y / real_t(CELL_SIZE) / vertex_spacing));
			for (real_t x = global_position.x - half_size.x; x <= global_position.x + half_size.x; x += cell_step.x) {
				for (real_t z = global_position.y - half_size.y; z <= global_position.y + half_size.y; z += cell_step.y) {
					Vector3 cell_pos = Vector3(x, 0.f, z) - global_local_offset;
					// Manually calculate cell pos without modulus, locations not in the current region will not be found.
					Vector2i cell_loc;
					cell_loc.x = VariantUtilityFunctions::floori(cell_pos.x / vertex_spacing) / CELL_SIZE;
					cell_loc.y = VariantUtilityFunctions::floori(cell_pos.z / vertex_spacing) / CELL_SIZE;
					if (cell_locations.has(cell_loc)) {
						c_locs[cell_loc] = 0;
					}
				}
			}
			Array cell_queue = c_locs.keys();
			if (cell_queue.size() == 0) {
				continue;
			}
			Ref<WorldScape3DMeshAsset> mesh_asset = _terrain->get_assets()->get_mesh_asset(mesh_types[m]);
			real_t mesh_height_offset = mesh_asset->get_height_offset();
			for (int c = 0; c < cell_queue.size(); c++) {
				Vector2i cell = cell_queue[c];
				Array triple = cell_inst_dict[cell];
				TypedArray<Transform3D> xforms = triple[0];
				PackedColorArray colors = triple[1];
				TypedArray<Transform3D> updated_xforms;
				PackedColorArray updated_colors;
				for (int i = 0; i < xforms.size(); i++) {
					Transform3D t = xforms[i];
					Vector3 global_origin(t.origin + global_local_offset);
					if (rect.has_point(Vector2(global_origin.x, global_origin.z))) {
						Vector3 height_offset = t.basis.get_column(1) * mesh_height_offset;
						t.origin -= height_offset;
						real_t height = _terrain->get_data()->get_height(global_origin);
						// If the new height is a nan due to creating a hole, remove the instance
						if (std::isnan(height)) {
							continue;
						}
						t.origin.y = height;

						t.origin += height_offset;
					}
					updated_xforms.push_back(t);
					updated_colors.push_back(colors[i]);
				}
				if (updated_xforms.size() > 0) {
					triple[0] = updated_xforms;
					triple[1] = updated_colors;
					triple[2] = true;
					cell_inst_dict[cell] = triple;
				} else {
					// Removed if a hole erased everything
					cell_inst_dict.erase(cell);
					_destroy_mmi_by_cell(region_loc, m, cell);
				}
				if (cell_inst_dict.is_empty()) {
					mesh_inst_dict.erase(region_mesh_id);
				}
			}
		}
		_update_mmis(region_loc);
	}
}

// Transfer foliage data from one region to another
// p_src_rect is the vertex/pixel offset into the region data, NOT a global position
// Need to update_mmis() after
void WorldScape3DInstancer::copy_paste_dfr(const WorldScape3DRegion *p_src_region, const Rect2i &p_src_rect, const WorldScape3DRegion *p_dst_region) {
	if (!p_src_region || !p_dst_region) {
		LOG(ERROR, "Source (", p_src_region, ") or destination (", p_dst_region, ") regions are null");
		return;
	}
	LOG(INFO, "Copying foliage data from src ", p_src_region->get_location(), " to dest ", p_dst_region->get_location());

	real_t vertex_spacing = _terrain->get_vertex_spacing();
	// Offset to dst from src
	Vector2i src_region_loc = p_src_region->get_location();
	int src_region_size = p_src_region->get_region_size();
	Vector2i src_offset = Vector2i(src_region_loc.x * src_region_size, src_region_loc.y * src_region_size);
	Vector2i dst_region_loc = p_dst_region->get_location();
	int dst_region_size = p_dst_region->get_region_size();
	Vector2i dst_offset = src_offset - Vector2i(dst_region_loc.x * dst_region_size, dst_region_loc.y * dst_region_size);
	Vector3 dst_translate = Vector3(dst_offset.x, 0.f, dst_offset.y) * vertex_spacing;

	// Get all Cell locations in rect, which is already in region space.
	Vector2i cell_start = p_src_rect.get_position() / CELL_SIZE;
	Vector2i steps = p_src_rect.get_size() / CELL_SIZE;
	Dictionary cells_to_copy;
	for (int x = cell_start.x; x < cell_start.x + steps.x; x++) {
		for (int y = cell_start.y; y < cell_start.y + steps.y; y++) {
			cells_to_copy[Vector2i(x, y)] = 0;
		}
	}

	// For each mesh, for each cell, if in rect, convert xforms to target region space, append to target region.
	Dictionary mesh_inst_dict = p_src_region->get_instances();
	Array mesh_types = mesh_inst_dict.keys();
	for (int m = 0; m < mesh_types.size(); m++) {
		TypedArray<Transform3D> xforms;
		PackedColorArray colors;
		Dictionary cell_inst_dict = p_src_region->get_instances()[m];
		Array cell_locs = cell_inst_dict.keys();
		for (int c = 0; c < cell_locs.size(); c++) {
			if (cells_to_copy.has(cell_locs[c])) {
				Array triple = cell_inst_dict[cell_locs[c]];
				TypedArray<Transform3D> cell_xforms = triple[0];
				PackedColorArray cell_colors = triple[1];
				for (int i = 0; i < cell_xforms.size(); i++) {
					Transform3D t = cell_xforms[i];
					t.origin += dst_translate;
					xforms.push_back(t);
					colors.push_back(cell_colors[i]);
				}
			}
		}
		if (xforms.size() == 0) {
			continue;
		}
		append_region(Ref<WorldScape3DRegion>(p_dst_region), m, xforms, colors, false);
	}
}

// Changes the ID of a mesh, without changing the mesh on the ground
// Called when the mesh asset id has changed. Updates Multimeshes and MMIs dictionary keys
void WorldScape3DInstancer::swap_ids(const int p_src_id, const int p_dst_id) {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);
	Ref<WorldScape3DAssets> assets = _terrain->get_assets();
	int mesh_count = assets->get_mesh_count();
	LOG(INFO, "Swapping IDs of multimeshes: ", p_src_id, " and ", p_dst_id);
	if (p_src_id >= 0 && p_src_id < mesh_count && p_dst_id >= 0 && p_dst_id < mesh_count) {
		Array region_locations = _terrain->get_data()->get_region_locations();
		for (int i = 0; i < region_locations.size(); i++) {
			Vector2i region_loc = region_locations[i];
			Ref<WorldScape3DRegion> region = _terrain->get_data()->get_region(region_loc);
			if (region.is_null()) {
				LOG(WARN, "No region found at: ", region_loc);
				continue;
			}

			// mesh_inst_dict could have src, src+dst, dst or nothing. All 4 must be considered
			Dictionary mesh_inst_dict = region->get_instances();
			Dictionary cells_inst_dict_src;
			Dictionary cells_inst_dict_dst;
			// Extract src dict
			if (mesh_inst_dict.has(p_src_id)) {
				_backup_region(region);
				cells_inst_dict_src = mesh_inst_dict[p_src_id];
				mesh_inst_dict.erase(p_src_id);
			}
			// Extract dest dict
			if (mesh_inst_dict.has(p_dst_id)) {
				_backup_region(region);
				cells_inst_dict_dst = mesh_inst_dict[p_dst_id];
				mesh_inst_dict.erase(p_dst_id);
			}
			// If src exists, insert into dst slot
			if (!cells_inst_dict_src.is_empty()) {
				_backup_region(region);
				mesh_inst_dict[p_dst_id] = cells_inst_dict_src;
			}
			// If dst exists, insert into src slot
			if (!cells_inst_dict_dst.is_empty()) {
				_backup_region(region);
				mesh_inst_dict[p_src_id] = cells_inst_dict_dst;
			}
			LOG(MESG, "Swapped mesh_ids for region: ", region_loc);
		}
		update_mmis(true);
	}
}

void WorldScape3DInstancer::update_mmis(const bool p_rebuild) {
	if (p_rebuild) {
		destroy();
	}
	_update_mmis();
}

void WorldScape3DInstancer::dump_data() {
	IS_DATA_INIT_MESG("Instancer isn't initialized.", VOID);
	Array region_locations = _terrain->get_data()->get_region_locations();
	LOG(WARN, "Dumping Instancer data for ", region_locations.size(), " active regions");
	for (int i = 0; i < region_locations.size(); i++) {
		Vector2i region_loc = region_locations[i];
		Ref<WorldScape3DRegion> region = _terrain->get_data()->get_region(region_loc);
		if (region.is_null()) {
			LOG(WARN, "No region found at: ", region_loc);
			continue;
		}
		LOG(MESG, "Region: ", region_loc);
		Dictionary mesh_inst_dict = region->get_instances();
		Array mesh_ids = mesh_inst_dict.keys();
		for (int m = 0; m < mesh_ids.size(); m++) {
			int mesh_id = mesh_ids[m];
			LOG(MESG, "Mesh ID: ", mesh_id);
			Dictionary cell_inst_dict = mesh_inst_dict[mesh_id];
			Array cells = cell_inst_dict.keys();
			for (int c = 0; c < cells.size(); c++) {
				Vector2i cell = cells[c];
				Array triple = cell_inst_dict[cell];
				if (triple.size() != 3) {
					LOG(WARN, "Malformed triple at cell ", cell, ": ", triple);
					continue;
				}
				Array xforms = triple[0];
				Array colors = triple[1];
				[[maybe_unused]] bool modified = triple[2];
				LOG(MESG, "Mesh: ", mesh_id, " cell: ", cell, " xforms: ", xforms.size(), " colors: ", colors.size(), " modified: ", modified);
			}
		}
	}
}

void WorldScape3DInstancer::dump_mmis() {
	LOG(WARN, "Dumping MMI tree and node containers");
	LOG(MESG, "_mmi_containers size: ", int(_mmi_containers.size()));
	// for (auto &it : _mmi_containers) {
	// 	LOG(MESG, "_mmi_containers region: ", it.first, ", node ptr: ", static_cast<uint64_t>(it.second));
	// }
	LOG(MESG, "_mmi tree: ");
	_terrain->get_mmi_parent()->print_tree();
	LOG(MESG, "_mmi_nodes size: ", int(_mmi_nodes.size()));
	// for (auto &i : _mmi_nodes) {
	// 	LOG(MESG, "_mmi_nodes region: ", i.first, ", dict ptr: ", static_cast<uint64_t>(&i.second));
	// 	for (auto &j : i.second) {
	// 		LOG(MESG, "mesh_mmi_dict mesh: ", j.first, ", dict ptr: ", static_cast<uint64_t>(&j.second));
	// 		for (auto &k : j.second) {
	// 			LOG(MESG, "cell_mmi_dict cell: ", k.first, ", mmi ptr: ", static_cast<uint64_t>(k.second));
	// 		}
	// 	}
	// }
}

///////////////////////////
// Protected Functions
///////////////////////////

void WorldScape3DInstancer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("clear_by_mesh", "mesh_id"), &WorldScape3DInstancer::clear_by_mesh);
	ClassDB::bind_method(D_METHOD("clear_by_location", "region_location", "mesh_id"), &WorldScape3DInstancer::clear_by_location);
	ClassDB::bind_method(D_METHOD("clear_by_region", "region", "mesh_id"), &WorldScape3DInstancer::clear_by_region);
	ClassDB::bind_method(D_METHOD("add_instances", "global_position", "params"), &WorldScape3DInstancer::add_instances);
	ClassDB::bind_method(D_METHOD("remove_instances", "global_position", "params"), &WorldScape3DInstancer::remove_instances);
	ClassDB::bind_method(D_METHOD("add_multimesh", "mesh_id", "multimesh", "transform", "update"), &WorldScape3DInstancer::add_multimesh, DEFVAL(Transform3D()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("add_transforms", "mesh_id", "transforms", "colors", "update"), &WorldScape3DInstancer::add_transforms, DEFVAL(PackedColorArray()), DEFVAL(true));
	ClassDB::bind_method(D_METHOD("append_location", "region_location", "mesh_id", "transforms", "colors", "update"), &WorldScape3DInstancer::append_location, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("append_region", "region", "mesh_id", "transforms", "colors", "update"), &WorldScape3DInstancer::append_region, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("update_transforms", "aabb"), &WorldScape3DInstancer::update_transforms);
	ClassDB::bind_method(D_METHOD("update_mmis", "rebuild"), &WorldScape3DInstancer::update_mmis, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("swap_ids", "src_id", "dest_id"), &WorldScape3DInstancer::swap_ids);
	ClassDB::bind_method(D_METHOD("dump_data"), &WorldScape3DInstancer::dump_data);
	ClassDB::bind_method(D_METHOD("dump_mmis"), &WorldScape3DInstancer::dump_mmis);
}
