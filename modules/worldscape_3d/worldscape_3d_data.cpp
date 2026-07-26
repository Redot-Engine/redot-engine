/**************************************************************************/
/*  worldscape_3d_data.cpp                                                */
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

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#endif
#include "core/config/engine.h"

#include "logger.h"
#include "worldscape_3d.h"
#include "worldscape_3d_data.h"

///////////////////////////
// Private Functions
///////////////////////////

void WorldScape3DData::_clear() {
	LOG(INFO, "Clearing data");
	_region_map_dirty = true;
	_region_map.clear();
	_region_map.resize(REGION_MAP_SIZE * REGION_MAP_SIZE);
	_regions.clear();
	_region_locations.clear();
	_master_height_range = V2_ZERO;
	_generated_height_maps.clear();
	_generated_control_maps.clear();
	_generated_color_maps.clear();
}

// Structured to work with do_for_regions. Should be renamed when copy_paste is expanded
void WorldScape3DData::_copy_paste_dfr(const WorldScape3DRegion *p_src_region, const Rect2i &p_src_rect, const Rect2i &p_dst_rect, const WorldScape3DRegion *p_dst_region) {
	if (!p_src_region || !p_dst_region) {
		return;
	}
	TypedArray<Image> src_maps = p_src_region->get_maps();
	TypedArray<Image> dst_maps = p_dst_region->get_maps();
	for (int i = 0; i < dst_maps.size(); i++) {
		Image *img = cast_to<Image>(dst_maps[i]);
		if (img) {
			img->blit_rect(src_maps[i], p_src_rect, p_dst_rect.position);
		}
	}
	_terrain->get_instancer()->copy_paste_dfr(p_src_region, p_src_rect, p_dst_region);
}

///////////////////////////
// Public Functions
///////////////////////////

void WorldScape3DData::initialize(WorldScape3D *p_terrain) {
	if (!p_terrain) {
		LOG(ERROR, "Initialization failed, p_terrain is null");
		return;
	}
	LOG(INFO, "Initializing data");
	bool prev_initialized = _terrain != nullptr;
	_terrain = p_terrain;
	_region_map.resize(REGION_MAP_SIZE * REGION_MAP_SIZE);
	_vertex_spacing = _terrain->get_vertex_spacing();
	if (!prev_initialized) {
#ifdef TOOLS_ENABLED
		if (_terrain->get_data_directory().is_empty()) {
			select_data_directory();
			return;
		}
#endif
		if (!_terrain->get_data_directory().is_empty()) {
			load_directory(_terrain->get_data_directory());
		}
	}
	_region_size = _terrain->get_region_size();
	_region_sizev = Vector2i(_region_size, _region_size);
}

void WorldScape3DData::set_region_locations(const TypedArray<Vector2i> &p_locations) {
	LOG(INFO, "Setting _region_locations with array sized: ", p_locations.size());
	_region_locations = p_locations;
	_region_map_dirty = true;
	update_maps(TYPE_MAX, false, false); // only rebuild region map
}

// Returns an array of active regions, optionally a shallow or deep copy
TypedArray<WorldScape3DRegion> WorldScape3DData::get_regions_active(const bool p_copy, const bool p_deep) const {
	TypedArray<WorldScape3DRegion> region_arr;
	for (int i = 0; i < _region_locations.size(); i++) {
		Vector2i region_loc = _region_locations[i];
		Ref<WorldScape3DRegion> region = get_region(region_loc);
		if (region.is_valid()) {
			region_arr.push_back(p_copy ? region->duplicate(p_deep) : region);
		}
	}
	return region_arr;
}

// Calls the callback function for every region within the given (descaled) area
// The callable receives: source WorldScape3DRegion, source Rect2i, dest Rect2i, (bindings)
// Used with change_region_size, dest WorldScape3DRegion is bound as the 4th parameter
void WorldScape3DData::do_for_regions(const Rect2i &p_area, const Callable &p_callback) {
	Rect2i location_bounds(V2I_DIVIDE_FLOOR(p_area.position, _region_size), V2I_DIVIDE_CEIL(p_area.size, _region_size));
	LOG(DEBUG, "Processing global area: ", p_area, " -> ", location_bounds);
	Point2i current_region_loc;
	for (int y = location_bounds.position.y; y < location_bounds.get_end().y; y++) {
		current_region_loc.y = y;
		for (int x = location_bounds.position.x; x < location_bounds.get_end().x; x++) {
			current_region_loc.x = x;
			WorldScape3DRegion *region = get_region_ptr(current_region_loc);
			if (region && !region->is_deleted()) {
				LOG(DEBUG, "Current region: ", current_region_loc);
				Rect2i region_area = p_area.intersection(Rect2i(current_region_loc * _region_size, _region_sizev));
				LOG(DEBUG, "Region bounds: ", Rect2i(current_region_loc * _region_size, _region_sizev));
				LOG(DEBUG, "Region area: ", region_area);
				Rect2i dst_coords(region_area.position - p_area.position, region_area.size);
				Rect2i src_coords(region_area.position - (region->get_location() * _region_sizev), dst_coords.size);
				LOG(DEBUG, "src map coords: ", src_coords);
				LOG(DEBUG, "dst map coords: ", dst_coords);
				p_callback.call(region, src_coords, dst_coords);
			}
		}
	}
}

void WorldScape3DData::change_region_size(int p_new_size) {
	LOG(INFO, "Changing region size from: ", _region_size, " to ", p_new_size);
	if (p_new_size < 64 || p_new_size > 2048 || !is_power_of_2(p_new_size)) {
		LOG(ERROR, "Invalid region size: ", p_new_size, ". Must be 64, 128, 256, 512, 1024, 2048");
		return;
	}
	if (p_new_size == _region_size) {
		return;
	}

	// Get current region corners expressed in new region_size coordinates
	Dictionary new_region_points;
	Array locs = _regions.keys();
	for (int i = 0; i < locs.size(); i++) {
		WorldScape3DRegion *region = get_region_ptr(Vector2i(locs[i]));
		if (region && !region->is_deleted()) {
			Point2i region_position = region->get_location() * _region_size;
			Rect2i location_bounds(V2I_DIVIDE_FLOOR(region_position, p_new_size), V2I_DIVIDE_CEIL(_region_sizev, p_new_size));
			for (int y = location_bounds.position.y; y < location_bounds.get_end().y; y++) {
				for (int x = location_bounds.position.x; x < location_bounds.get_end().x; x++) {
					new_region_points[Point2i(x, y)] = 1;
				}
			}
		}
	}

	// Make new regions to receive copied data
	TypedArray<WorldScape3DRegion> new_regions;
	Array keys = new_region_points.keys();
	for (int i = 0; i < keys.size(); i++) {
		Point2i loc = keys[i];
		Ref<WorldScape3DRegion> new_region;
		new_region.instantiate();
		new_region->set_location(loc);
		new_region->set_region_size(p_new_size);
		new_region->set_vertex_spacing(_vertex_spacing);
		new_region->set_modified(true);
		new_region->sanitize_maps();

		// Copy current data from current into new region, up to new region size
		Rect2i area;
		area.position = loc * p_new_size;
		area.size = Vector2i(p_new_size, p_new_size);
		do_for_regions(area, callable_mp(this, &WorldScape3DData::_copy_paste_dfr).bind(new_region.ptr()));
		new_regions.push_back(new_region);
	}

	// Remove old data
	_terrain->get_instancer()->destroy();
	TypedArray<WorldScape3DRegion> old_regions = get_regions_active();
	for (int i = 0; i < old_regions.size(); i++) {
		remove_region(old_regions[i], false);
	}

	// Change region size
	_terrain->set_region_size((WorldScape3D::RegionSize)p_new_size);

	// Add new regions and rebuild
	for (int i = 0; i < new_regions.size(); i++) {
		add_region(new_regions[i], false);
	}

	calc_height_range(true);
	update_maps(TYPE_MAX, true, true);
	_terrain->get_instancer()->update_mmis(true);
}

void WorldScape3DData::set_region_modified(const Vector2i &p_region_loc, const bool p_modified) {
	WorldScape3DRegion *region = get_region_ptr(p_region_loc);
	if (!region) {
		LOG(ERROR, "Region not found at: ", p_region_loc);
		return;
	}
	return region->set_modified(p_modified);
}

bool WorldScape3DData::is_region_modified(const Vector2i &p_region_loc) const {
	WorldScape3DRegion *region = get_region_ptr(p_region_loc);
	if (!region) {
		LOG(ERROR, "Region not found at: ", p_region_loc);
		return false;
	}
	return region->is_modified();
}

void WorldScape3DData::set_region_deleted(const Vector2i &p_region_loc, const bool p_deleted) {
	WorldScape3DRegion *region = get_region_ptr(p_region_loc);
	if (!region) {
		LOG(ERROR, "Region not found at: ", p_region_loc);
		return;
	}
	return region->set_deleted(p_deleted);
}

bool WorldScape3DData::is_region_deleted(const Vector2i &p_region_loc) const {
	WorldScape3DRegion *region = get_region_ptr(p_region_loc);
	if (!region) {
		LOG(ERROR, "Region not found at: ", p_region_loc);
		return true;
	}
	return region->is_deleted();
}

Ref<WorldScape3DRegion> WorldScape3DData::add_region_blankp(const Vector3 &p_global_position, const bool p_update) {
	return add_region_blank(get_region_location(p_global_position));
}

Ref<WorldScape3DRegion> WorldScape3DData::add_region_blank(const Vector2i &p_region_loc, const bool p_update) {
	Ref<WorldScape3DRegion> region;
	region.instantiate();
	region->set_location(p_region_loc);
	region->set_region_size(_region_size);
	region->set_vertex_spacing(_vertex_spacing);
	if (add_region(region, p_update) == OK) {
		region->set_modified(true);
		return region;
	}
	return Ref<WorldScape3DRegion>();
}

/** Adds a WorldScape3DRegion to the terrain
 * Marks region as modified
 *	p_update - rebuild the maps if true. Set to false if bulk adding many regions.
 */
Error WorldScape3DData::add_region(const Ref<WorldScape3DRegion> &p_region, const bool p_update) {
	if (p_region.is_null()) {
		LOG(ERROR, "Provided region is null. Returning");
		return FAILED;
	}
	Vector2i region_loc = p_region->get_location();
	LOG(INFO, "Adding region at location ", region_loc, ", update maps: ", p_update ? "yes" : "no");

	// Check bounds and slow report errors
	if (get_region_map_index(region_loc) < 0) {
		LOG(ERROR, "Location ", region_loc, " out of bounds. Max: ",
				-REGION_MAP_SIZE / 2, " to ", REGION_MAP_SIZE / 2 - 1);
		return FAILED;
	}
	p_region->sanitize_maps();
	p_region->set_deleted(false);
	if (!_region_locations.has(region_loc)) {
		_region_locations.push_back(region_loc);
	} else {
		LOG(INFO, "Overwriting ", (_regions.has(region_loc)) ? "deleted" : "existing", " region at ", region_loc);
	}
	_regions[region_loc] = p_region;
	_region_map_dirty = true;
	LOG(DEBUG, "Storing region ", region_loc, " version ", vformat("%.3f", p_region->get_version()), " id: ", _region_locations.size());
	if (p_update) {
		update_maps(TYPE_MAX, true, false);
		_terrain->get_instancer()->update_mmis(true);
	}
	return OK;
}

void WorldScape3DData::remove_regionp(const Vector3 &p_global_position, const bool p_update) {
	Ref<WorldScape3DRegion> region = get_region(get_region_location(p_global_position));
	remove_region(region, p_update);
}

void WorldScape3DData::remove_regionl(const Vector2i &p_region_loc, const bool p_update) {
	Ref<WorldScape3DRegion> region = get_region(p_region_loc);
	remove_region(region, p_update);
}

// Remove region marks the region for deletion, and removes it from the active arrays indexed by ID
// It remains stored in _regions and the file remains on disk until saved, when both are removed
void WorldScape3DData::remove_region(const Ref<WorldScape3DRegion> &p_region, const bool p_update) {
	if (p_region.is_null()) {
		LOG(ERROR, "Region not found or is null. Returning");
		return;
	}

	Vector2i region_loc = p_region->get_location();
	int region_id = _region_locations.find(region_loc);
	LOG(INFO, "Marking region ", region_loc, " for deletion. update_maps: ", p_update ? "yes" : "no");
	if (region_id < 0) {
		LOG(ERROR, "Region ", region_loc, " not found in region_locations. Returning");
		return;
	}
	p_region->set_deleted(true);
	_region_locations.remove_at(region_id);
	_region_map_dirty = true;
	LOG(DEBUG, "Removing from region_locations, new size: ", _region_locations.size());
	if (p_update) {
		LOG(DEBUG, "Updating generated maps");
		update_maps(TYPE_MAX, true, false);
		_terrain->get_instancer()->update_mmis(true);
	}
}

void WorldScape3DData::save_directory(const String &p_dir) {
	LOG(INFO, "Saving data files to ", p_dir);
	Array locations = _regions.keys();
	for (int i = 0; i < locations.size(); i++) {
		save_region(locations[i], p_dir, _terrain->get_save_16_bit());
	}
#ifdef TOOLS_ENABLED
	if (IS_EDITOR && !EditorInterface::get_singleton()->get_resource_file_system()->is_scanning()) {
		EditorInterface::get_singleton()->get_resource_file_system()->scan();
	}
#endif
}

// You may need to do a file system scan to update FileSystem panel
void WorldScape3DData::save_region(const Vector2i &p_region_loc, const String &p_dir, const bool p_16_bit) {
	Ref<WorldScape3DRegion> region = get_region(p_region_loc);
	if (region.is_null()) {
		LOG(ERROR, "No region found at: ", p_region_loc);
		return;
	}
	String fname = Util::location_to_filename(p_region_loc);
	String path = p_dir + String("/") + fname;
	// If region marked for deletion, remove from disk and from _regions, but don't free in case stored in undo
	if (region->is_deleted()) {
		LOG(DEBUG, "Removing ", p_region_loc, " from _regions");
		_regions.erase(p_region_loc);
		LOG(DEBUG, "File to be deleted: ", path);
		if (!FileAccess::exists(path)) {
			LOG(INFO, "File to delete ", path, " doesn't exist. (Maybe from add, undo, save)");
			return;
		}
		Ref<DirAccess> da = DirAccess::open(p_dir);
		if (da.is_null()) {
			LOG(ERROR, "Cannot open directory for writing: ", p_dir, " error: ", DirAccess::get_open_error());
			return;
		}
		Error err = da->remove(fname);
		if (err != OK) {
			LOG(ERROR, "Could not remove file: ", fname, ", error code: ", err);
		}
		LOG(INFO, "File ", path, " deleted");
		return;
	}
	Error err = region->save(path, p_16_bit);
	if (!(err == OK || err == ERR_SKIP)) {
		LOG(ERROR, "Could not save file: ", path, ", error: ", UtilityFunctions::error_string(err), " (", err, ")");
	}
}

void WorldScape3DData::load_directory(const String &p_dir) {
	if (p_dir.is_empty()) {
		LOG(ERROR, "Specified directory name is blank");
		return;
	}

	LOG(INFO, "Loading region files from ", p_dir);
	PackedStringArray files = Util::get_files(p_dir, "terrain*.res");
	if (files.size() == 0) {
		LOG(INFO, "No WorldScape3D region files found in: ", p_dir);
		return;
	}

	_clear();
	for (int i = 0; i < files.size(); i++) {
		String fname = files[i];
		String path = p_dir + String("/") + fname;
		LOG(DEBUG, "Loading region from ", path);
		Vector2i loc = Util::filename_to_location(fname);
		if (loc.x == INT32_MAX) {
			LOG(ERROR, "Cannot get region location from file name: ", fname);
			continue;
		}
		Ref<WorldScape3DRegion> region = ResourceLoader::load(path, "WorldScape3DRegion", ResourceFormatLoader::CacheMode::CACHE_MODE_IGNORE);
		if (region.is_null()) {
			LOG(ERROR, "Cannot load region at ", path);
			continue;
		}
		LOG(INFO, "Loaded region: ", loc, " size: ", region->get_region_size());
		if (_regions.is_empty()) {
			_terrain->set_region_size(static_cast<WorldScape3D::RegionSize>(region->get_region_size()));
		} else {
			if (_terrain->get_region_size() != static_cast<WorldScape3D::RegionSize>(region->get_region_size())) {
				LOG(ERROR, "Region size mismatch. First loaded: ", _terrain->get_region_size(), " next: ",
						region->get_region_size(), " in file: ", path);
				return;
			}
		}
		region->set_path(path, true);
		region->set_location(loc);
		region->set_version(CURRENT_VERSION); // Sends upgrade warning if old version
		add_region(region, false);
	}
	update_maps(TYPE_MAX, true, false);
}

//TODO have load_directory call load_region, or make a load_file that loads a specific path
void WorldScape3DData::load_region(const Vector2i &p_region_loc, const String &p_dir, const bool p_update) {
	LOG(INFO, "Loading region from location ", p_region_loc);
	String path = p_dir + String("/") + Util::location_to_filename(p_region_loc);
	if (!FileAccess::exists(path)) {
		LOG(ERROR, "File ", path, " doesn't exist");
		return;
	}
	Ref<WorldScape3DRegion> region = ResourceLoader::load(path, "WorldScape3DRegion", ResourceFormatLoader::CacheMode::CACHE_MODE_IGNORE);
	if (region.is_null()) {
		LOG(ERROR, "Cannot load region at ", path);
		return;
	}
	if (_regions.is_empty()) {
		_terrain->set_region_size((WorldScape3D::RegionSize)region->get_region_size());
	} else {
		if (_terrain->get_region_size() != (WorldScape3D::RegionSize)region->get_region_size()) {
			LOG(ERROR, "Region size mismatch. First loaded: ", _terrain->get_region_size(), " next: ",
					region->get_region_size(), " in file: ", path);
			return;
		}
	}
	region->set_path(path, true);
	region->set_location(p_region_loc);
	region->set_version(CURRENT_VERSION); // Sends upgrade warning if old version
	add_region(region, p_update);
}

TypedArray<Image> WorldScape3DData::get_maps(const MapType p_map_type) const {
	if (p_map_type < 0 || p_map_type >= TYPE_MAX) {
		LOG(ERROR, "Specified map type out of range");
		return TypedArray<Image>();
	}
	switch (p_map_type) {
		case TYPE_HEIGHT:
			return get_height_maps();
			break;
		case TYPE_CONTROL:
			return get_control_maps();
			break;
		case TYPE_COLOR:
			return get_color_maps();
			break;
		default:
			break;
	}
	return TypedArray<Image>();
}

void WorldScape3DData::update_maps(const MapType p_map_type, const bool p_all_regions, const bool p_generate_mipmaps) {
	// Generate region color mipmaps
	if (p_generate_mipmaps && (p_map_type == TYPE_COLOR || p_map_type == TYPE_MAX)) {
		LOG(EXTREME, "Regenerating color mipmaps");
		for (int i = 0; i < _region_locations.size(); i++) {
			Vector2i region_loc = _region_locations[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			// Generate all or only those marked edited
			if (region && (p_all_regions || region->is_edited())) {
				region->get_color_map()->generate_mipmaps();
			}
		}
	}

	// Mark texture arrays dirty for rebuilding
	if (p_all_regions) {
		LOG(EXTREME, "Marking dirty maps of type: ", p_map_type);
		switch (p_map_type) {
			case TYPE_HEIGHT:
				_generated_height_maps.clear();
				break;
			case TYPE_CONTROL:
				_generated_control_maps.clear();
				break;
			case TYPE_COLOR:
				_generated_color_maps.clear();
				break;
			default:
				_generated_height_maps.clear();
				_generated_control_maps.clear();
				_generated_color_maps.clear();
				_region_map_dirty = true;
				break;
		}
	}

	bool any_changed = false;

	// Rebuild region map if dirty
	if (_region_map_dirty) {
		LOG(EXTREME, "Regenerating ", REGION_MAP_VSIZE, " region map array from active regions");
		_region_map.clear();
		_region_map.resize(REGION_MAP_SIZE * REGION_MAP_SIZE);
		_region_map_dirty = false;
		_region_locations = TypedArray<Vector2i>(); // enforce new pointer
		Array locs = _regions.keys();
		int region_id = 0;
		for (int i = 0; i < locs.size(); i++) {
			Vector2i region_loc = locs[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			if (region && !region->is_deleted()) {
				region_id += 1; // Begin at 1 since 0 = no region
				int map_index = get_region_map_index(region_loc);
				if (map_index >= 0) {
					_region_map.set(map_index, region_id);
					_region_locations.push_back(region_loc);
				}
			}
		}
		any_changed = true;
		emit_signal("region_map_changed");
	}

	// Rebuild height maps if dirty
	if (_generated_height_maps.is_dirty()) {
		LOG(EXTREME, "Regenerating height texture array from regions");
		_height_maps.clear();
		for (int i = 0; i < _region_locations.size(); i++) {
			Vector2i region_loc = _region_locations[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			if (region) {
				_height_maps.push_back(region->get_height_map());
			} else {
				LOG(ERROR, "Can't find region ", region_loc, ", _regions: ", _regions,
						", locations: ", _region_locations, ". Please report this error.");
				return;
			}
		}
		_generated_height_maps.create(_height_maps);
		calc_height_range();
		any_changed = true;
		emit_signal("height_maps_changed");
	}

	// Rebuild control maps if dirty
	if (_generated_control_maps.is_dirty()) {
		LOG(EXTREME, "Regenerating control texture array from regions");
		_control_maps.clear();
		for (int i = 0; i < _region_locations.size(); i++) {
			Vector2i region_loc = _region_locations[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			if (region) {
				_control_maps.push_back(region->get_control_map());
			}
		}
		_generated_control_maps.create(_control_maps);
		any_changed = true;
		emit_signal("control_maps_changed");
	}

	// Rebuild color maps if dirty
	if (_generated_color_maps.is_dirty()) {
		LOG(EXTREME, "Regenerating color texture array from regions");
		_color_maps.clear();
		for (int i = 0; i < _region_locations.size(); i++) {
			Vector2i region_loc = _region_locations[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			if (region) {
				_color_maps.push_back(region->get_color_map());
			}
		}
		_generated_color_maps.create(_color_maps);
		any_changed = true;
		emit_signal("color_maps_changed");
	}

	// If no maps have been rebuilt, update only individual regions in the array.
	// Regions marked Edited have been changed by WorldScape3DEditor::_operate_map or undo / redo processing.
	if (!any_changed) {
		for (int i = 0; i < _region_locations.size(); i++) {
			Vector2i region_loc = _region_locations[i];
			WorldScape3DRegion *region = get_region_ptr(region_loc);
			if (region && region->is_edited()) {
				int region_id = get_region_id(region_loc);
				switch (p_map_type) {
					case TYPE_HEIGHT:
						_generated_height_maps.update(region->get_height_map(), region_id);
						emit_signal("height_maps_changed");
						break;
					case TYPE_CONTROL:
						_generated_control_maps.update(region->get_control_map(), region_id);
						emit_signal("control_maps_changed");
						break;
					case TYPE_COLOR:
						_generated_color_maps.update(region->get_color_map(), region_id);
						emit_signal("color_maps_changed");
						break;
					default:
						_generated_height_maps.update(region->get_height_map(), region_id);
						_generated_control_maps.update(region->get_control_map(), region_id);
						_generated_color_maps.update(region->get_color_map(), region_id);
						emit_signal("height_maps_changed");
						emit_signal("control_maps_changed");
						emit_signal("color_maps_changed");
						break;
				}
			}
		}
	}
	emit_signal("maps_changed");
}

void WorldScape3DData::set_pixel(const MapType p_map_type, const Vector3 &p_global_position, const Color &p_pixel) {
	if (p_map_type < 0 || p_map_type >= TYPE_MAX) {
		LOG(ERROR, "Specified map type out of range");
		return;
	}
	Vector2i region_loc = get_region_location(p_global_position);
	WorldScape3DRegion *region = get_region_ptr(region_loc);
	if (!region) {
		LOG(ERROR, "No active region found at: ", p_global_position);
		return;
	}
	if (region->is_deleted()) {
		LOG(ERROR, "No active region found at: ", p_global_position);
		return;
	}
	Vector2i global_offset = region_loc * _region_size;
	Vector3 descaled_pos = p_global_position / _vertex_spacing;
	Vector2i img_pos = Vector2i(descaled_pos.x - global_offset.x, descaled_pos.z - global_offset.y);
	img_pos = img_pos.clamp(V2I_ZERO, Vector2i(_region_size - 1, _region_size - 1));
	Image *map = region->get_map_ptr(p_map_type);
	if (map) {
		map->set_pixelv(img_pos, p_pixel);
		region->set_modified(true);
	}
}

Color WorldScape3DData::get_pixel(const MapType p_map_type, const Vector3 &p_global_position) const {
	if (p_map_type < 0 || p_map_type >= TYPE_MAX) {
		LOG(ERROR, "Specified map type out of range");
		return COLOR_NAN;
	}
	Vector2i region_loc = get_region_location(p_global_position);
	WorldScape3DRegion *region = get_region_ptr(region_loc);
	if (!region) {
		return COLOR_NAN;
	}
	if (region->is_deleted()) {
		return COLOR_NAN;
	}
	Vector2i global_offset = region_loc * _region_size;
	Vector3 descaled_pos = p_global_position / _vertex_spacing;
	Vector2i img_pos = Vector2i(descaled_pos.x - global_offset.x, descaled_pos.z - global_offset.y);
	img_pos = img_pos.clamp(V2I_ZERO, Vector2i(_region_size - 1, _region_size - 1));
	Image *map = region->get_map_ptr(p_map_type);
	if (map) {
		return map->get_pixelv(img_pos);
	} else {
		return COLOR_NAN;
	}
}

real_t WorldScape3DData::get_height(const Vector3 &p_global_position) const {
	if (is_hole(get_control(p_global_position))) {
		return NAN;
	}
	Vector3 pos = p_global_position;
	const real_t &step = _vertex_spacing;
	pos.y = 0.f;
	// Round to nearest vertex
	Vector3 pos_round = pos.snapped(Vector3(step, 0.f, step));
	// If requested position is close to a vertex, return its height
	if ((pos - pos_round).length_squared() < 0.0001f) {
		return get_pixel(TYPE_HEIGHT, pos).r;
	} else {
		// Otherwise, bilinearly interpolate 4 surrounding vertices
		Vector3 pos00 = Vector3(floor(pos.x / step) * step, 0.f, floor(pos.z / step) * step);
		real_t ht00 = get_pixel(TYPE_HEIGHT, pos00).r;
		Vector3 pos01 = pos00 + Vector3(0.f, 0.f, step);
		real_t ht01 = get_pixel(TYPE_HEIGHT, pos01).r;
		Vector3 pos10 = pos00 + Vector3(step, 0.f, 0.f);
		real_t ht10 = get_pixel(TYPE_HEIGHT, pos10).r;
		Vector3 pos11 = pos00 + Vector3(step, 0.f, step);
		real_t ht11 = get_pixel(TYPE_HEIGHT, pos11).r;
		return bilerp(ht00, ht01, ht10, ht11, pos00, pos11, pos);
	}
}

Vector3 WorldScape3DData::get_normal(const Vector3 &p_global_position) const {
	if (get_region_idp(p_global_position) < 0 || is_hole(get_control(p_global_position))) {
		return Vector3(NAN, NAN, NAN);
	}
	real_t height = get_height(p_global_position);
	real_t u = height - get_height(p_global_position + Vector3(_vertex_spacing, 0.0f, 0.0f));
	real_t v = height - get_height(p_global_position + Vector3(0.f, 0.f, _vertex_spacing));
	Vector3 normal = Vector3(u, _vertex_spacing, v);
	normal.normalize();
	return normal;
}

bool WorldScape3DData::is_in_slope(const Vector3 &p_global_position, const Vector2 &p_slope_range, const bool p_invert) const {
	// If slope is full range, it's disabled
	const Vector2 slope_range = CLAMP(p_slope_range, V2_ZERO, Vector2(90.f, 90.f));
	if (slope_range.y - slope_range.x > 89.99f) {
		return true;
	}

	// Adapted from get_normal to work with holes
	Vector3 slope_normal;
	{
		if (get_region_idp(p_global_position) < 0) {
			return false;
		}
		// Adapted from get_height() to work with holes
		auto get_height = [&](Vector3 pos) -> real_t {
			real_t step = _terrain->get_vertex_spacing();
			// Round to nearest vertex
			Vector3 pos_round = pos.snapped(Vector3(step, 0.f, step));
			real_t height = get_pixel(TYPE_HEIGHT, pos_round).r;
			return std::isnan(height) ? 0.f : height;
		};

		const real_t vertex_spacing = _terrain->get_vertex_spacing();
		const real_t height = get_height(p_global_position);
		const real_t u = height - get_height(p_global_position + Vector3(vertex_spacing, 0.0f, 0.0f));
		const real_t v = height - get_height(p_global_position + Vector3(0.f, 0.f, vertex_spacing));
		slope_normal = Vector3(u, vertex_spacing, v);
		slope_normal.normalize();
	}

	const real_t slope_angle = Math::acos(slope_normal.dot(Vector3(0.f, 1.f, 0.f)));
	const real_t slope_angle_degrees = Math::rad_to_deg(slope_angle);

	// XOR: If invert return !a || !b else return a && b
	return p_invert ^ ((slope_range.x <= slope_angle_degrees) && (slope_angle_degrees <= slope_range.y));
}

/**
 * Returns:
 * X = base index
 * Y = overlay index
 * Z = percentage blend between X and Y. Limited to the fixed values in RANGE.
 * Interpretation of this data is up to the gamedev. Unfortunately due to blending, this isn't
 * pixel perfect. I would have your player print this location as you walk around to see how the
 * blending values look, then consider that the overlay texture is visible starting at a blend
 * value of .3-.5, otherwise it's the base texture.
 **/
Vector3 WorldScape3DData::get_texture_id(const Vector3 &p_global_position) const {
	// Verify in a region
	int region_id = get_region_idp(p_global_position);
	if (region_id < 0) {
		return Vector3(NAN, NAN, NAN);
	}

	// Verify not in a hole
	float src = get_pixel(TYPE_CONTROL, p_global_position).r; // 32-bit float, not double/real
	if (is_hole(src)) {
		return Vector3(NAN, NAN, NAN);
	}

	// If material available, autoshader enabled, and pixel set to auto
	if (_terrain) {
		Ref<WorldScape3DMaterial> t_material = _terrain->get_material();
		bool auto_enabled = t_material->get_auto_shader();
		bool control_auto = is_auto(src);
		if (auto_enabled && control_auto) {
			real_t auto_slope = real_t(t_material->get_shader_param("auto_slope")) * 2.f - 1.f;
			real_t auto_height_reduction = real_t(t_material->get_shader_param("auto_height_reduction"));
			real_t height = get_height(p_global_position);
			Vector3 normal = get_normal(p_global_position);
			uint32_t base_id = t_material->get_shader_param("auto_base_texture");
			uint32_t overlay_id = t_material->get_shader_param("auto_overlay_texture");
			real_t blend = CLAMP((auto_slope * 2.f * (normal.y - 1.f) + 1.f) - auto_height_reduction * .01f * height, 0.f, 1.f);
			return Vector3(real_t(base_id), real_t(overlay_id), blend);
		}
	}

	// Else, just get textures from control map
	uint32_t base_id = get_base(src);
	uint32_t overlay_id = get_overlay(src);
	real_t blend = real_t(get_blend(src)) / 255.0f;
	return Vector3(real_t(base_id), real_t(overlay_id), blend);
}

/**
 * Returns the location of a terrain vertex at a certain LOD. If there is a hole at the position, it returns
 * NAN in the vector's Y coordinate.
 * p_lod (0-8): Determines how many heights around the given global position will be sampled.
 * p_filter:
 *  HEIGHT_FILTER_NEAREST: Samples the height map at the exact coordinates given.
 *  HEIGHT_FILTER_MINIMUM: Samples (1 << p_lod) ** 2 heights around the given coordinates and returns the lowest.
 * p_global_position: X and Z coordinates of the vertex. Heights will be sampled around these coordinates.
 */
Vector3 WorldScape3DData::get_mesh_vertex(const int32_t p_lod, const HeightFilter p_filter, const Vector3 &p_global_position) const {
	LOG(INFO, "Calculating vertex location");
	int32_t step = 1 << CLAMP(p_lod, 0, 8);
	real_t height = 0.0f;

	switch (p_filter) {
		case HEIGHT_FILTER_NEAREST: {
			if (is_hole(get_control(p_global_position))) {
				height = NAN;
			} else {
				height = get_height(p_global_position);
			}
		} break;
		case HEIGHT_FILTER_MINIMUM: {
			height = get_height(p_global_position);
			for (int32_t dx = -step / 2; dx < step / 2; dx += 1) {
				for (int32_t dz = -step / 2; dz < step / 2; dz += 1) {
					Vector3 position = p_global_position + Vector3(dx, 0.f, dz) * _vertex_spacing;
					if (is_hole(get_control(position))) {
						height = NAN;
						break;
					}
					real_t h = get_height(position);
					if (h < height) {
						height = h;
					}
				}
			}
		} break;
	}
	return Vector3(p_global_position.x, height, p_global_position.z);
}

void WorldScape3DData::add_edited_area(const AABB &p_area) {
	if (_edited_area.has_surface()) {
		_edited_area = _edited_area.merge(p_area);
	} else {
		_edited_area = p_area;
	}
	emit_signal("maps_edited", p_area);
}

// Recalculates master height range from all active regions current height ranges
// Recursive mode has all regions to recalculate from each heightmap pixel
void WorldScape3DData::calc_height_range(const bool p_recursive) {
	_master_height_range = V2_ZERO;
	for (int i = 0; i < _region_locations.size(); i++) {
		Vector2i region_loc = _region_locations[i];
		WorldScape3DRegion *region = get_region_ptr(region_loc);
		if (!region) {
			continue;
		}
		if (p_recursive) {
			region->calc_height_range();
		}
		update_master_heights(region->get_height_range());
	}
	LOG(EXTREME, "Accumulated height range for all regions: ", _master_height_range);
}

/**
 * Imports an Image set (Height, Control, Color) into WorldScape3DData
 * It does NOT normalize values to 0-1. You must do that using get_min_max() and adjusting scale and offset.
 * Parameters:
 *	p_images - MapType.TYPE_MAX sized array of Images for Height, Control, Color. Images can be blank or null
 *	p_global_position - X,0,Z location on the region map. Valid range is ~ (+/-8192, +/-8192)
 *	p_offset - Add this factor to all height values, can be negative
 *	p_scale - Scale all height values by this factor (applied after offset)
 */
void WorldScape3DData::import_images(const TypedArray<Image> &p_images, const Vector3 &p_global_position, const real_t p_offset, const real_t p_scale) {
	IS_INIT_MESG("Data not initialized", VOID);
	if (p_images.size() != TYPE_MAX) {
		LOG(ERROR, "p_images.size() is ", p_images.size(), ". It should be ", TYPE_MAX, " even if some Images are blank or null");
		return;
	}

	Vector2i img_size = V2I_ZERO;
	for (int i = 0; i < TYPE_MAX; i++) {
		Ref<Image> img = p_images[i];
		if (img.is_valid() && !img->is_empty()) {
			LOG(INFO, "Importing image type ", TYPESTR[i], ", size: ", img->get_size(), ", format: ", img->get_format());
			if (i == TYPE_HEIGHT) {
				LOG(INFO, "Applying offset: ", p_offset, ", scale: ", p_scale);
			}
			if (img_size == V2I_ZERO) {
				img_size = img->get_size();
			} else if (img_size != img->get_size()) {
				LOG(ERROR, "Included Images in p_images have different dimensions. Aborting import");
				return;
			}
		}
	}
	if (img_size == V2I_ZERO) {
		LOG(ERROR, "All images are empty. Nothing to import");
		return;
	}

	Vector3 descaled_position = p_global_position / _vertex_spacing;
	int max_dimension = _region_size * REGION_MAP_SIZE / 2;
	if ((abs(descaled_position.x) > max_dimension) || (abs(descaled_position.z) > max_dimension)) {
		LOG(ERROR, "Specify a position within +/-", Vector3(max_dimension, 0.f, max_dimension) * _vertex_spacing);
		return;
	}
	if ((descaled_position.x + img_size.x > max_dimension) ||
			(descaled_position.z + img_size.y > max_dimension)) {
		LOG(ERROR, img_size, " image will not fit at ", p_global_position,
				". Try ", -(img_size * _vertex_spacing) / 2.f, " to center");
		return;
	}

	TypedArray<Image> tmp_images;
	tmp_images.resize(TYPE_MAX);

	for (int i = 0; i < TYPE_MAX; i++) {
		Ref<Image> img = p_images[i];
		tmp_images[i] = img;
		if (img.is_null()) {
			continue;
		}

		// Apply scale and offsets to the heightmap and filter out invalid data
		if (i == TYPE_HEIGHT) {
			LOG(DEBUG, "Creating new temp image to adjust scale: ", p_scale, " offset: ", p_offset);
			Ref<Image> newimg = Image::create_empty(img->get_size().x, img->get_size().y, false, FORMAT[TYPE_HEIGHT]);
			for (int y = 0; y < img->get_height(); y++) {
				for (int x = 0; x < img->get_width(); x++) {
					Color clr = img->get_pixel(x, y);
					if (std::isnormal(clr.r)) {
						clr.r = (clr.r * p_scale) + p_offset;
					} else {
						clr.r = p_offset;
					}
					newimg->set_pixel(x, y, clr);
				}
			}
			tmp_images[i] = newimg;
		}
	}

	// Slice up incoming image into segments of region_size^2, and pad any remainder
	int slices_width = ceil(real_t(img_size.x) / real_t(_region_size));
	int slices_height = ceil(real_t(img_size.y) / real_t(_region_size));
	slices_width = CLAMP(slices_width, 1, REGION_MAP_SIZE);
	slices_height = CLAMP(slices_height, 1, REGION_MAP_SIZE);
	LOG(DEBUG, "Creating ", Vector2i(slices_width, slices_height), " slices for ", img_size, " images.");

	for (int y = 0; y < slices_height; y++) {
		for (int x = 0; x < slices_width; x++) {
			Vector2i start_coords = Vector2i(x * _region_size, y * _region_size);
			Vector2i end_coords = Vector2i((x + 1) * _region_size - 1, (y + 1) * _region_size - 1);
			LOG(DEBUG, "Reviewing image section ", start_coords, " to ", end_coords);

			Vector2i size_to_copy;
			if (end_coords.x <= img_size.x && end_coords.y <= img_size.y) {
				size_to_copy = _region_sizev;
			} else {
				size_to_copy.x = img_size.x - start_coords.x;
				size_to_copy.y = img_size.y - start_coords.y;
				LOG(DEBUG, "Uneven end piece. Copying padded slice ", Vector2i(x, y), " size to copy: ", size_to_copy);
			}
			LOG(DEBUG, "Copying ", size_to_copy, " sized segment");

			TypedArray<Image> images;
			images.resize(TYPE_MAX);

			// Vector3 global_position = Vector3(descaled_position.x + start_coords.x, 0.f, descaled_position.z + start_coords.y) * _vertex_spacing;
			// Vector2i region_loc = get_region_location(global_position);
			// Ref<WorldScape3DRegion> region = get_region(region_loc);
			// if (region.is_null()) {
			// 	region.instantiate();
			// 	region->set_location(region_loc);
			// 	region->set_region_size(_region_size);
			// 	region->set_vertex_spacing(_vertex_spacing);
			// 	region->set_modified(true);
			// 	add_region(region, false);
			// }
			for (int i = 0; i < TYPE_MAX; i++) {
				Ref<Image> img = tmp_images[i];
				Ref<Image> img_slice;
				// If incoming map is valid, import it, overwriting
				if (img.is_valid() && !img->is_empty()) {
					img_slice = Util::get_filled_image(_region_sizev, COLOR[i], false, img->get_format());
					img_slice->blit_rect(tmp_images[i], Rect2i(start_coords, size_to_copy), V2I_ZERO);
					//region->set_map(static_cast<MapType>(i), img_slice);
				} else {
					img_slice = Util::get_filled_image(_region_sizev, COLOR[i], false, FORMAT[i]);
				}
				images[i] = img_slice;
			}
			// Add the heightmap slice and only regenerate on the last one
			Ref<WorldScape3DRegion> region;
			region.instantiate();
			auto position = Vector3(descaled_position.x + start_coords.x, 0.f, descaled_position.z + start_coords.y);
			position *= _vertex_spacing;
			region->set_location(get_region_location(position));
			region->set_maps(images);
			add_region(region, (x == slices_width - 1 && y == slices_height - 1));
		}
	} // for y < slices_height, x < slices_width
}

/** Exports a specified map as one of r16/raw, exr, jpg, png, webp, res, tres
 * r16 or exr are recommended for roundtrip external editing
 * r16 can be edited by Krita, however you must know the dimensions and min/max before reimporting
 * res/tres stores in Godot's native format.
 */
Error WorldScape3DData::export_image(const String &p_file_name, const MapType p_map_type) const {
	if (p_map_type < 0 || p_map_type >= TYPE_MAX) {
		LOG(ERROR, "Invalid map type specified: ", p_map_type, " max: ", TYPE_MAX - 1);
		return FAILED;
	}

	if (p_file_name.is_empty()) {
		LOG(ERROR, "No file specified. Nothing to export");
		return FAILED;
	}

	if (get_region_count() == 0) {
		LOG(ERROR, "No valid regions. Nothing to export");
		return FAILED;
	}

	// Simple file name validation
	static const String bad_chars = "?*|%<>\"";
	for (int i = 0; i < bad_chars.length(); ++i) {
		for (int j = 0; j < p_file_name.length(); ++j) {
			if (bad_chars[i] == p_file_name[j]) {
				LOG(ERROR, "Invalid file path '" + p_file_name + "'");
				return FAILED;
			}
		}
	}

	// Update path delimiter
	String file_name = p_file_name.replace("\\", "/");

	// Check if p_file_name has a path and prepend "res://" if not
	bool is_simple_filename = true;
	for (int i = 0; i < file_name.length(); ++i) {
		char32_t c = file_name[i];
		if (c == '/' || c == ':') {
			is_simple_filename = false;
			break;
		}
	}
	if (is_simple_filename) {
		file_name = "res://" + file_name;
	}

	// Check if the file can be opened for writing
	Ref<FileAccess> file_ref = FileAccess::open(file_name, FileAccess::ModeFlags::WRITE);
	if (file_ref.is_null()) {
		LOG(ERROR, "Cannot open file '" + file_name + "' for writing");
		return FAILED;
	}
	file_ref->close();

	// Filename is validated. Begin export image generation
	Ref<Image> img = layered_to_image(p_map_type);
	if (img.is_null() || img->is_empty()) {
		LOG(ERROR, "Cannot create an export image for map type: ", TYPESTR[p_map_type]);
		return FAILED;
	}

	String ext = file_name.get_extension().to_lower();
	LOG(MESG, "Saving ", img->get_size(), " sized ", TYPESTR[p_map_type],
			" map in format ", img->get_format(), " as ", ext, " to: ", file_name);
	Vector2i minmax = Util::get_min_max(img);
	LOG(MESG, "Minimum height: ", minmax.x, ", Maximum height: ", minmax.y);
	if (ext == "r16" || ext == "raw") {
		Ref<FileAccess> file = FileAccess::open(file_name, FileAccess::WRITE);
		real_t height_min = minmax.x;
		real_t height_max = minmax.y;
		real_t hscale = 65535.0 / (height_max - height_min);
		for (int y = 0; y < img->get_height(); y++) {
			for (int x = 0; x < img->get_width(); x++) {
				int h = int((img->get_pixel(x, y).r - height_min) * hscale);
				h = CLAMP(h, 0, 65535);
				file->store_16(h);
			}
		}
		return file->get_error();
	} else if (ext == "exr") {
		return img->save_exr(file_name, (p_map_type == TYPE_HEIGHT) ? true : false);
	} else if (ext == "png") {
		return img->save_png(file_name);
	} else if (ext == "jpg") {
		return img->save_jpg(file_name);
	} else if (ext == "webp") {
		return img->save_webp(file_name);
	} else if ((ext == "res") || (ext == "tres")) {
		return ResourceSaver::save(img, file_name, ResourceSaver::FLAG_COMPRESS);
	}

	LOG(ERROR, "No recognized file type. See docs for valid extensions");
	return FAILED;
}

Ref<Image> WorldScape3DData::layered_to_image(const MapType p_map_type) const {
	LOG(INFO, "Generating a full sized image for all regions including empty regions");
	MapType map_type = p_map_type;
	if (map_type >= TYPE_MAX) {
		map_type = TYPE_HEIGHT;
	}
	Vector2i top_left = V2I_ZERO;
	Vector2i bottom_right = V2I_ZERO;
	for (int i = 0; i < _region_locations.size(); i++) {
		LOG(DEBUG, "Region locations[", i, "]: ", _region_locations[i]);
		Vector2i region_loc = _region_locations[i];
		if (region_loc.x < top_left.x) {
			top_left.x = region_loc.x;
		} else if (region_loc.x > bottom_right.x) {
			bottom_right.x = region_loc.x;
		}
		if (region_loc.y < top_left.y) {
			top_left.y = region_loc.y;
		} else if (region_loc.y > bottom_right.y) {
			bottom_right.y = region_loc.y;
		}
	}

	LOG(DEBUG, "Full range to cover all regions: ", top_left, " to ", bottom_right);
	Vector2i img_size = Vector2i(1 + bottom_right.x - top_left.x, 1 + bottom_right.y - top_left.y) * _region_size;
	LOG(DEBUG, "Image size: ", img_size);
	Ref<Image> img = Util::get_filled_image(img_size, COLOR[map_type], false, FORMAT[map_type]);

	for (int i = 0; i < _region_locations.size(); i++) {
		Vector2i region_loc = _region_locations[i];
		Vector2i img_location = (region_loc - top_left) * _region_size;
		LOG(DEBUG, "Region to blit: ", region_loc, " Export image coords: ", img_location);
		WorldScape3DRegion *region = get_region_ptr(region_loc);
		if (region) {
			img->blit_rect(region->get_map(map_type), Rect2i(V2I_ZERO, _region_sizev), img_location);
		}
	}
	return img;
}

void WorldScape3DData::print_audit_data() const {
	LOG(INFO, "Dumping data");
	LOG(INFO, "Region_locations size: ", _region_locations.size(), " ", _region_locations);
	LOG(INFO, "Region map");
	for (int i = 0; i < _region_map.size(); i++) {
		if (_region_map[i]) {
			LOG(INFO, "Region id: ", _region_map[i], " array index: ", i);
		}
	}
	Util::dump_maps(_height_maps, "Height maps");
	Util::dump_maps(_control_maps, "Control maps");
	Util::dump_maps(_color_maps, "Color maps");

	Util::dump_gentex(_generated_height_maps, "height");
	Util::dump_gentex(_generated_control_maps, "control");
	Util::dump_gentex(_generated_color_maps, "color");
}

///////////////////////////
// Protected Functions
///////////////////////////

void WorldScape3DData::_bind_methods() {
	BIND_ENUM_CONSTANT(HEIGHT_FILTER_NEAREST);
	BIND_ENUM_CONSTANT(HEIGHT_FILTER_MINIMUM);

	BIND_CONSTANT(REGION_MAP_SIZE);

	ClassDB::bind_method(D_METHOD("get_region_count"), &WorldScape3DData::get_region_count);
	ClassDB::bind_method(D_METHOD("set_region_locations", "region_locations"), &WorldScape3DData::set_region_locations);
	ClassDB::bind_method(D_METHOD("get_region_locations"), &WorldScape3DData::get_region_locations);
	ClassDB::bind_method(D_METHOD("get_regions_active", "copy", "deep"), &WorldScape3DData::get_regions_active, DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_regions_all"), &WorldScape3DData::get_regions_all);
	ClassDB::bind_method(D_METHOD("get_region_map"), &WorldScape3DData::get_region_map);
	ClassDB::bind_static_method("WorldScape3DData", D_METHOD("get_region_map_index", "region_location"), &WorldScape3DData::get_region_map_index);

	ClassDB::bind_method(D_METHOD("do_for_regions", "area", "callback"), &WorldScape3DData::do_for_regions);
	ClassDB::bind_method(D_METHOD("change_region_size", "region_size"), &WorldScape3DData::change_region_size);

	ClassDB::bind_method(D_METHOD("get_region_location", "global_position"), &WorldScape3DData::get_region_location);
	ClassDB::bind_method(D_METHOD("get_region_id", "region_location"), &WorldScape3DData::get_region_id);
	ClassDB::bind_method(D_METHOD("get_region_idp", "global_position"), &WorldScape3DData::get_region_idp);

	ClassDB::bind_method(D_METHOD("has_region", "region_location"), &WorldScape3DData::has_region);
	ClassDB::bind_method(D_METHOD("has_regionp", "global_position"), &WorldScape3DData::has_regionp);
	ClassDB::bind_method(D_METHOD("get_region", "region_location"), &WorldScape3DData::get_region);
	ClassDB::bind_method(D_METHOD("get_regionp", "global_position"), &WorldScape3DData::get_regionp);

	ClassDB::bind_method(D_METHOD("set_region_modified", "region_location", "modified"), &WorldScape3DData::set_region_modified);
	ClassDB::bind_method(D_METHOD("is_region_modified", "region_location"), &WorldScape3DData::is_region_modified);
	ClassDB::bind_method(D_METHOD("set_region_deleted", "region_location", "deleted"), &WorldScape3DData::set_region_deleted);
	ClassDB::bind_method(D_METHOD("is_region_deleted", "region_location"), &WorldScape3DData::is_region_deleted);

	ClassDB::bind_method(D_METHOD("add_region_blankp", "global_position", "update"), &WorldScape3DData::add_region_blankp, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("add_region_blank", "region_location", "update"), &WorldScape3DData::add_region_blank, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("add_region", "region", "update"), &WorldScape3DData::add_region, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("remove_regionp", "global_position", "update"), &WorldScape3DData::remove_regionp, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("remove_regionl", "region_location", "update"), &WorldScape3DData::remove_regionl, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("remove_region", "region", "update"), &WorldScape3DData::remove_region, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("save_directory", "directory"), &WorldScape3DData::save_directory);
	ClassDB::bind_method(D_METHOD("save_region", "region_location", "directory", "save_16_bit"), &WorldScape3DData::save_region, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("load_directory", "directory"), &WorldScape3DData::load_directory);
	ClassDB::bind_method(D_METHOD("load_region", "region_location", "directory", "update"), &WorldScape3DData::load_region, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("get_height_maps"), &WorldScape3DData::get_height_maps);
	ClassDB::bind_method(D_METHOD("get_control_maps"), &WorldScape3DData::get_control_maps);
	ClassDB::bind_method(D_METHOD("get_color_maps"), &WorldScape3DData::get_color_maps);
	ClassDB::bind_method(D_METHOD("get_maps", "map_type"), &WorldScape3DData::get_maps);
	ClassDB::bind_method(D_METHOD("update_maps", "map_type", "all_regions", "generate_mipmaps"), &WorldScape3DData::update_maps, DEFVAL(TYPE_MAX), DEFVAL(true), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_height_maps_rid"), &WorldScape3DData::get_height_maps_rid);
	ClassDB::bind_method(D_METHOD("get_control_maps_rid"), &WorldScape3DData::get_control_maps_rid);
	ClassDB::bind_method(D_METHOD("get_color_maps_rid"), &WorldScape3DData::get_color_maps_rid);

	ClassDB::bind_method(D_METHOD("set_pixel", "map_type", "global_position", "pixel"), &WorldScape3DData::set_pixel);
	ClassDB::bind_method(D_METHOD("get_pixel", "map_type", "global_position"), &WorldScape3DData::get_pixel);
	ClassDB::bind_method(D_METHOD("set_height", "global_position", "height"), &WorldScape3DData::set_height);
	ClassDB::bind_method(D_METHOD("get_height", "global_position"), &WorldScape3DData::get_height);
	ClassDB::bind_method(D_METHOD("set_color", "global_position", "color"), &WorldScape3DData::set_color);
	ClassDB::bind_method(D_METHOD("get_color", "global_position"), &WorldScape3DData::get_color);
	ClassDB::bind_method(D_METHOD("set_control", "global_position", "control"), &WorldScape3DData::set_control);
	ClassDB::bind_method(D_METHOD("get_control", "global_position"), &WorldScape3DData::get_control);
	ClassDB::bind_method(D_METHOD("set_roughness", "global_position", "roughness"), &WorldScape3DData::set_roughness);
	ClassDB::bind_method(D_METHOD("get_roughness", "global_position"), &WorldScape3DData::get_roughness);

	ClassDB::bind_method(D_METHOD("set_control_base_id", "global_position", "texture_id"), &WorldScape3DData::set_control_base_id);
	ClassDB::bind_method(D_METHOD("get_control_base_id", "global_position"), &WorldScape3DData::get_control_base_id);
	ClassDB::bind_method(D_METHOD("set_control_overlay_id", "global_position", "texture_id"), &WorldScape3DData::set_control_overlay_id);
	ClassDB::bind_method(D_METHOD("get_control_overlay_id", "global_position"), &WorldScape3DData::get_control_overlay_id);
	ClassDB::bind_method(D_METHOD("set_control_blend", "global_position", "blend_value"), &WorldScape3DData::set_control_blend);
	ClassDB::bind_method(D_METHOD("get_control_blend", "global_position"), &WorldScape3DData::get_control_blend);
	ClassDB::bind_method(D_METHOD("set_control_angle", "global_position", "degrees"), &WorldScape3DData::set_control_angle);
	ClassDB::bind_method(D_METHOD("get_control_angle", "global_position"), &WorldScape3DData::get_control_angle);
	ClassDB::bind_method(D_METHOD("set_control_scale", "global_position", "percentage_modifier"), &WorldScape3DData::set_control_scale);
	ClassDB::bind_method(D_METHOD("get_control_scale", "global_position"), &WorldScape3DData::get_control_scale);
	ClassDB::bind_method(D_METHOD("set_control_hole", "global_position", "enable"), &WorldScape3DData::set_control_hole);
	ClassDB::bind_method(D_METHOD("get_control_hole", "global_position"), &WorldScape3DData::get_control_hole);
	ClassDB::bind_method(D_METHOD("set_control_navigation", "global_position", "enable"), &WorldScape3DData::set_control_navigation);
	ClassDB::bind_method(D_METHOD("get_control_navigation", "global_position"), &WorldScape3DData::get_control_navigation);
	ClassDB::bind_method(D_METHOD("set_control_auto", "global_position", "enable"), &WorldScape3DData::set_control_auto);
	ClassDB::bind_method(D_METHOD("get_control_auto", "global_position"), &WorldScape3DData::get_control_auto);

	ClassDB::bind_method(D_METHOD("get_normal", "global_position"), &WorldScape3DData::get_normal);
	ClassDB::bind_method(D_METHOD("is_in_slope", "global_position", "slope_range", "invert"), &WorldScape3DData::is_in_slope, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_texture_id", "global_position"), &WorldScape3DData::get_texture_id);
	ClassDB::bind_method(D_METHOD("get_mesh_vertex", "lod", "filter", "global_position"), &WorldScape3DData::get_mesh_vertex);

	ClassDB::bind_method(D_METHOD("get_height_range"), &WorldScape3DData::get_height_range);
	ClassDB::bind_method(D_METHOD("calc_height_range", "recursive"), &WorldScape3DData::calc_height_range, DEFVAL(false));

	ClassDB::bind_method(D_METHOD("import_images", "images", "global_position", "offset", "scale"), &WorldScape3DData::import_images, DEFVAL(Vector3(0, 0, 0)), DEFVAL(0.0), DEFVAL(1.0));
	ClassDB::bind_method(D_METHOD("export_image", "file_name", "map_type"), &WorldScape3DData::export_image);
	ClassDB::bind_method(D_METHOD("layered_to_image", "map_type"), &WorldScape3DData::layered_to_image);

	int ro_flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_READ_ONLY;
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "region_locations", PROPERTY_HINT_ARRAY_TYPE, "Vector2i", ro_flags), "set_region_locations", "get_region_locations");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "height_maps", PROPERTY_HINT_ARRAY_TYPE, "Image", ro_flags), "", "get_height_maps");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "control_maps", PROPERTY_HINT_ARRAY_TYPE, "Image", ro_flags), "", "get_control_maps");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "color_maps", PROPERTY_HINT_ARRAY_TYPE, "Image", ro_flags), "", "get_color_maps");

	ADD_SIGNAL(MethodInfo("maps_changed"));
	ADD_SIGNAL(MethodInfo("region_map_changed"));
	ADD_SIGNAL(MethodInfo("height_maps_changed"));
	ADD_SIGNAL(MethodInfo("control_maps_changed"));
	ADD_SIGNAL(MethodInfo("color_maps_changed"));
	ADD_SIGNAL(MethodInfo("maps_edited", PropertyInfo(Variant::AABB, "edited_area")));
}

#ifdef TOOLS_ENABLED
void WorldScape3DData::select_data_directory() {
	EditorFileDialog *file_dialog = memnew(EditorFileDialog);
	file_dialog->set_filters({ "*.res" });
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_DIR);
	file_dialog->set_ok_button_text("Open");
	file_dialog->set_title("Select the terrain data directory");
	file_dialog->set_size(Size2i{ 850, 550 });
	file_dialog->set_transient(false);
	file_dialog->set_exclusive(true);
	file_dialog->set_flag(Window::Flags::FLAG_POPUP, true);
	file_dialog->connect("dir_selected", callable_mp(this, &WorldScape3DData::on_data_dir_selected).bind(file_dialog));
	file_dialog->connect("canceled", callable_mp(this, &WorldScape3DData::on_data_dir_dialog_closed).bind(file_dialog));
	file_dialog->set_visible(true);
	EditorNode::get_singleton()->add_child(file_dialog);
	file_dialog->move_to_center();
}

void WorldScape3DData::on_data_dir_selected(const String &path, EditorFileDialog *dialog) {
	dialog->hide();
	_terrain->set_data_directory(path);

	_region_size = _terrain->get_region_size();
	_region_sizev = Vector2i(_region_size, _region_size);

	dialog->queue_free();
}

void WorldScape3DData::on_data_dir_dialog_closed(EditorFileDialog *dialog) {
	dialog->hide();
	dialog->queue_free();
	call_deferred("select_data_directory");
}
#endif
