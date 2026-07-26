/**************************************************************************/
/*  worldscape_3d_data.h                                                  */
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

#include "constants.h"
#include "core/object/object.h"
#include "generated_texture.h"
#include "worldscape_3d_region.h"

#include <array>

class WorldScape3D;
#ifdef TOOLS_ENABLED
class EditorFileDialog;
#endif

class WorldScape3DData : public Object {
	GDCLASS(WorldScape3DData, Object);
	CLASS_NAME();
	friend WorldScape3D;

public: // Constants
	static constexpr real_t CURRENT_VERSION = 0.93f;
	static constexpr int REGION_MAP_SIZE = 32;
	static constexpr Vector2i REGION_MAP_VSIZE{ REGION_MAP_SIZE, REGION_MAP_SIZE };

	enum HeightFilter {
		HEIGHT_FILTER_NEAREST,
		HEIGHT_FILTER_MINIMUM
	};

private:
	WorldScape3D *_terrain = nullptr;

	// Data Settings & flags
	int _region_size = 0; // Set by WorldScape3D::set_region_size
	Vector2i _region_sizev = Vector2i(_region_size, _region_size);
	real_t _vertex_spacing = 1.f; // Set by WorldScape3D::set_vertex_spacing

	AABB _edited_area;
	Vector2 _master_height_range = V2_ZERO;

	/////////
	// WorldScape3DRegions house the maps, instances, and other data for each region.
	// Regions are dual indexed:
	// 1) By `region_location:Vector2i` as the primary key. This is the only stable index
	// so should be the main index for users.
	// 2) By `region_id:int`. This index changes on every add/remove, depends on load order,
	// and is not stable. It should not be relied on by users and is primarily for internal use.

	// Private functions should be indexed by region_id or region_location
	// Public functions by region_location or global_position

	// `_regions` stores all loaded WorldScape3DRegions, indexed by region_location. If marked for
	// deletion they are removed from here upon saving, however they may stay in memory if tracked
	// by the Undo system.
	Dictionary _regions; // Dict[region_location:Vector2i] -> WorldScape3DRegion

	// All _active_ region maps are maintained in these secondary indices.
	// Regions are considered active if and only if they exist in `_region_locations`. The other
	// arrays are built off of this index; its order defines region_id.
	// The image arrays are converted to TextureArrays for the shader.

	TypedArray<Vector2i> _region_locations;
	TypedArray<Image> _height_maps;
	TypedArray<Image> _control_maps;
	TypedArray<Image> _color_maps;

	// Editing occurs on the Image arrays above, which are converted to Texture arrays
	// below for the shader.

	// 32x32 grid with region_id:int at its location, no region = 0, region_ids >= 1
	PackedInt32Array _region_map;
	bool _region_map_dirty = true;

	// These contain the TextureArray RIDs from the RenderingServer
	GeneratedTexture _generated_height_maps;
	GeneratedTexture _generated_control_maps;
	GeneratedTexture _generated_color_maps;

	// Functions
	void _clear();
	void _copy_paste_dfr(const WorldScape3DRegion *p_src_region, const Rect2i &p_src_rect, const Rect2i &p_dst_rect, const WorldScape3DRegion *p_dst_region);

public:
	WorldScape3DData() {}
	void initialize(WorldScape3D *p_terrain);
	~WorldScape3DData() { _clear(); }

	// Regions

	int get_region_count() const { return _region_locations.size(); }
	void set_region_locations(const TypedArray<Vector2i> &p_locations);
	TypedArray<Vector2i> get_region_locations() const { return _region_locations; }
	TypedArray<WorldScape3DRegion> get_regions_active(const bool p_copy = false, const bool p_deep = false) const;
	Dictionary get_regions_all() const { return _regions; }
	PackedInt32Array get_region_map() const { return _region_map; }
	static int get_region_map_index(const Vector2i &p_region_loc);

	void do_for_regions(const Rect2i &p_area, const Callable &p_callback);
	void change_region_size(int region_size);

	Vector2i get_region_location(const Vector3 &p_global_position) const;
	int get_region_id(const Vector2i &p_region_loc) const;
	int get_region_idp(const Vector3 &p_global_position) const;

	bool has_region(const Vector2i &p_region_loc) const { return get_region_id(p_region_loc) != -1; }
	bool has_regionp(const Vector3 &p_global_position) const { return get_region_idp(p_global_position) != -1; }
	Ref<WorldScape3DRegion> get_region(const Vector2i &p_region_loc) const;
	WorldScape3DRegion *get_region_ptr(const Vector2i &p_region_loc) const;
	template <typename T> // Catch invalid types. See note below in implementation.
	WorldScape3DRegion *get_region_ptr(const T &p_region_loc) const = delete;
	Ref<WorldScape3DRegion> get_regionp(const Vector3 &p_global_position) const;

	void set_region_modified(const Vector2i &p_region_loc, const bool p_modified = true);
	bool is_region_modified(const Vector2i &p_region_loc) const;
	void set_region_deleted(const Vector2i &p_region_loc, const bool p_deleted = true);
	bool is_region_deleted(const Vector2i &p_region_loc) const;

	Ref<WorldScape3DRegion> add_region_blankp(const Vector3 &p_global_position, const bool p_update = true);
	Ref<WorldScape3DRegion> add_region_blank(const Vector2i &p_region_loc, const bool p_update = true);
	Error add_region(const Ref<WorldScape3DRegion> &p_region, const bool p_update = true);
	void remove_regionp(const Vector3 &p_global_position, const bool p_update = true);
	void remove_regionl(const Vector2i &p_region_loc, const bool p_update = true);
	void remove_region(const Ref<WorldScape3DRegion> &p_region, const bool p_update = true);

	// File I/O
	void save_directory(const String &p_dir);
	void save_region(const Vector2i &p_region_loc, const String &p_dir, const bool p_16_bit = false);
	void load_directory(const String &p_dir);
	void load_region(const Vector2i &p_region_loc, const String &p_dir, const bool p_update = true);

	// Maps
	TypedArray<Image> get_height_maps() const { return _height_maps; }
	TypedArray<Image> get_control_maps() const { return _control_maps; }
	TypedArray<Image> get_color_maps() const { return _color_maps; }
	TypedArray<Image> get_maps(const MapType p_map_type) const;
	void update_maps(const MapType p_map_type = TYPE_MAX, const bool p_all_regions = true, const bool p_generate_mipmaps = false);
	RID get_height_maps_rid() const { return _generated_height_maps.get_rid(); }
	RID get_control_maps_rid() const { return _generated_control_maps.get_rid(); }
	RID get_color_maps_rid() const { return _generated_color_maps.get_rid(); }

	void set_pixel(const MapType p_map_type, const Vector3 &p_global_position, const Color &p_pixel);
	Color get_pixel(const MapType p_map_type, const Vector3 &p_global_position) const;
	void set_height(const Vector3 &p_global_position, const real_t p_height);
	real_t get_height(const Vector3 &p_global_position) const;
	void set_color(const Vector3 &p_global_position, const Color &p_color);
	Color get_color(const Vector3 &p_global_position) const;
	void set_control(const Vector3 &p_global_position, const uint32_t p_control);
	uint32_t get_control(const Vector3 &p_global_position) const;
	void set_roughness(const Vector3 &p_global_position, const real_t p_roughness);
	real_t get_roughness(const Vector3 &p_global_position) const;

	// Control Map
	void set_control_base_id(const Vector3 &p_global_position, const uint8_t p_base);
	uint32_t get_control_base_id(const Vector3 &p_global_position) const;
	void set_control_overlay_id(const Vector3 &p_global_position, const uint8_t p_overlay);
	uint32_t get_control_overlay_id(const Vector3 &p_global_position) const;
	void set_control_blend(const Vector3 &p_global_position, const real_t p_blend);
	real_t get_control_blend(const Vector3 &p_global_position) const;
	void set_control_angle(const Vector3 &p_global_position, const real_t p_angle);
	real_t get_control_angle(const Vector3 &p_global_position) const;
	void set_control_scale(const Vector3 &p_global_position, const real_t p_scale);
	real_t get_control_scale(const Vector3 &p_global_position) const;
	void set_control_hole(const Vector3 &p_global_position, const bool p_hole);
	bool get_control_hole(const Vector3 &p_global_position) const;
	void set_control_navigation(const Vector3 &p_global_position, const bool p_navigation);
	bool get_control_navigation(const Vector3 &p_global_position) const;
	void set_control_auto(const Vector3 &p_global_position, const bool p_auto);
	bool get_control_auto(const Vector3 &p_global_position) const;

	Vector3 get_normal(const Vector3 &p_global_position) const;
	bool is_in_slope(const Vector3 &p_global_position, const Vector2 &p_slope_range, const bool p_invert = false) const;
	Vector3 get_texture_id(const Vector3 &p_global_position) const;
	Vector3 get_mesh_vertex(const int32_t p_lod, const HeightFilter p_filter, const Vector3 &p_global_position) const;

	void add_edited_area(const AABB &p_area);
	void clear_edited_area() { _edited_area = AABB(); }
	AABB get_edited_area() const { return _edited_area; }

	Vector2 get_height_range() const { return _master_height_range; }
	void update_master_height(const real_t p_height);
	void update_master_heights(const Vector2 &p_low_high);
	void calc_height_range(const bool p_recursive = false);

	void import_images(const TypedArray<Image> &p_images, const Vector3 &p_global_position = V3_ZERO,
			const real_t p_offset = 0.f, const real_t p_scale = 1.f);
	Error export_image(const String &p_file_name, const MapType p_map_type = TYPE_HEIGHT) const;
	Ref<Image> layered_to_image(const MapType p_map_type) const;

	// Utility
	void print_audit_data() const;

protected:
	static void _bind_methods();
#ifdef TOOLS_ENABLED
private:
	void select_data_directory();
	void on_data_dir_selected(const String &path, EditorFileDialog *);
	void on_data_dir_dialog_closed(EditorFileDialog *);
#endif
};

VARIANT_ENUM_CAST(WorldScape3DData::HeightFilter);

// Inline Region Functions

// Verifies the location is within the bounds of the _region_map array and
// the world, returning the _region_map index, which contains the region_id.
// Valid region locations are -16, -16 to 15, 15, or when offset: 0, 0 to 31, 31
// If any bits other than 0x1F are set, it's out of bounds and returns -1
inline int WorldScape3DData::get_region_map_index(const Vector2i &p_region_loc) {
	// Offset world to positive values only
	Vector2i loc = p_region_loc + (REGION_MAP_VSIZE / 2);
	// Catch values > 31
	if ((uint32_t(loc.x | loc.y) & uint32_t(~0x1F)) > 0) {
		return -1;
	}
	return loc.y * REGION_MAP_SIZE + loc.x;
}

// Returns a region location given a global position. No bounds checking nor data access.
inline Vector2i WorldScape3DData::get_region_location(const Vector3 &p_global_position) const {
	Vector2 descaled_position = v3v2(p_global_position) / _vertex_spacing;
	return Vector2i((descaled_position / real_t(_region_size)).floor());
}

// Returns id of any active region. -1 if out of bounds or no region, or region id
inline int WorldScape3DData::get_region_id(const Vector2i &p_region_loc) const {
	int map_index = get_region_map_index(p_region_loc);
	if (map_index >= 0) {
		int region_id = _region_map[map_index] - 1; // 0 = no region
		if (region_id >= 0 && region_id < _region_locations.size()) {
			return region_id;
		}
	}
	return -1;
}

inline int WorldScape3DData::get_region_idp(const Vector3 &p_global_position) const {
	return get_region_id(get_region_location(p_global_position));
}

// This function is slower than the version below, but safer when interacting with Godot, which requires
// References. This includes backing up regions in the UndoRedoManager.
// Ref<> has a pointer constructor, so a reference can be created with Ref<>(ptr). Godot detects the
// pointer is already tracked and increments the reference counter.
// Passing the pointer to a function with a Ref<> parameter works, and there's an implicit conversion to Ref.
// However, let's require explicit conversions for clarity, so wrap a Ref around it:
// eg. backup_region(Ref<WorldScape3D>(raw_ptr));
// Should be used for most functions in Editor and Instancer.
inline Ref<WorldScape3DRegion> WorldScape3DData::get_region(const Vector2i &p_region_loc) const {
	return _regions.get(p_region_loc, Ref<WorldScape3DRegion>());
}

// Using the raw pointer is faster than creating a Ref<>. It can also safely be converted to a Ref as needed
// with Ref<>(ptr). Use this function when retrieving regions frequently, eg looping over get_pixel().
// Should be used for most data processing in Data, but not region handling.
// Re overloaded template
// get_region_ptr(region_locs[i]) worked with an implicit conversion of Variant::Vector2i to Vector2i.
// However it also worked for Variant::Object, which silently sent invalid data.
// The overloaded template was added to catch this. Pulling out of a dictionary/array gives a Variant,
// so now explicit conversion is required, eg. get_region_ptr(Vector2i(locs[i])).
inline WorldScape3DRegion *WorldScape3DData::get_region_ptr(const Vector2i &p_region_loc) const {
	if (_regions.has(p_region_loc)) {
		return cast_to<WorldScape3DRegion>(_regions[p_region_loc]);
	}
	return nullptr;
}

inline Ref<WorldScape3DRegion> WorldScape3DData::get_regionp(const Vector3 &p_global_position) const {
	return _regions.get(get_region_location(p_global_position), Ref<WorldScape3DRegion>());
}

// Inline Map Functions

inline void WorldScape3DData::set_height(const Vector3 &p_global_position, const real_t p_height) {
	set_pixel(TYPE_HEIGHT, p_global_position, Color(p_height, 0.f, 0.f, 1.f));
}

inline void WorldScape3DData::set_color(const Vector3 &p_global_position, const Color &p_color) {
	Color clr = p_color;
	clr.a = get_roughness(p_global_position);
	set_pixel(TYPE_COLOR, p_global_position, clr);
}

inline Color WorldScape3DData::get_color(const Vector3 &p_global_position) const {
	Color clr = get_pixel(TYPE_COLOR, p_global_position);
	clr.a = 1.0f;
	return clr;
}

inline void WorldScape3DData::set_control(const Vector3 &p_global_position, const uint32_t p_control) {
	set_pixel(TYPE_CONTROL, p_global_position, Color(as_float(p_control), 0.f, 0.f, 1.f));
}

inline uint32_t WorldScape3DData::get_control(const Vector3 &p_global_position) const {
	real_t val = get_pixel(TYPE_CONTROL, p_global_position).r;
	return (std::isnan(val)) ? UINT32_MAX : as_uint(val);
}

inline void WorldScape3DData::set_control_base_id(const Vector3 &p_global_position, const uint8_t p_base) {
	uint32_t control = get_control(p_global_position);
	uint8_t base = CLAMP(p_base, uint8_t(0), uint8_t(31));
	set_control(p_global_position, (control & ~(0x1F << 27)) | enc_base(base));
}

inline uint32_t WorldScape3DData::get_control_base_id(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? UINT32_MAX : get_base(control);
}

inline void WorldScape3DData::set_control_overlay_id(const Vector3 &p_global_position, const uint8_t p_overlay) {
	uint32_t control = get_control(p_global_position);
	uint8_t overlay = CLAMP(p_overlay, uint8_t(0), uint8_t(31));
	set_control(p_global_position, (control & ~(0x1F << 22)) | enc_overlay(overlay));
}

inline uint32_t WorldScape3DData::get_control_overlay_id(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? UINT32_MAX : get_overlay(control);
}

// Expects 0.0 to 1.0 range
inline void WorldScape3DData::set_control_blend(const Vector3 &p_global_position, const real_t p_blend) {
	uint32_t control = get_control(p_global_position);
	uint8_t blend = uint8_t(CLAMP(Math::round(p_blend * 255.f), 0.f, 255.f));
	set_control(p_global_position, (control & ~(0xFF << 14)) | enc_blend(blend));
}

inline real_t WorldScape3DData::get_control_blend(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? NAN : real_t(get_blend(control)) / 255.f;
}

// Expects angle in degrees
inline void WorldScape3DData::set_control_angle(const Vector3 &p_global_position, const real_t p_angle) {
	uint32_t control = get_control(p_global_position);
	uint8_t uvrotation = uint8_t(CLAMP(Math::round(p_angle / 22.5f), 0.f, 15.f));
	set_control(p_global_position, (control & ~(0xF << 10)) | enc_uv_rotation(uvrotation));
}

// returns angle in degrees
inline real_t WorldScape3DData::get_control_angle(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	real_t angle = real_t(get_uv_rotation(control)) * 22.5f;
	return control == UINT32_MAX ? NAN : angle;
}

// Expects scale as a percentage modifier
inline void WorldScape3DData::set_control_scale(const Vector3 &p_global_position, const real_t p_scale) {
	uint32_t control = get_control(p_global_position);
	std::array<uint32_t, 8> scale_align = { 5, 6, 7, 0, 1, 2, 3, 4 };
	uint8_t uvscale = scale_align[uint8_t(CLAMP(Math::round((p_scale + 60.f) / 20.f), 0.f, 7.f))];
	set_control(p_global_position, (control & ~(0x7 << 7)) | enc_uv_scale(uvscale));
}

inline real_t WorldScape3DData::get_control_scale(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	const std::array<real_t, 8> scale_values = { 0.0f, 20.0f, 40.0f, 60.0f, 80.0f, -60.0f, -40.0f, -20.0f };
	real_t scale = scale_values[get_uv_scale(control)]; //select from array UI return values
	return control == UINT32_MAX ? NAN : scale;
}

inline void WorldScape3DData::set_control_hole(const Vector3 &p_global_position, const bool p_hole) {
	uint32_t control = get_control(p_global_position);
	set_control(p_global_position, (control & ~(0x1 << 2)) | enc_hole(p_hole));
}

inline bool WorldScape3DData::get_control_hole(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? false : is_hole(control);
}

inline void WorldScape3DData::set_control_navigation(const Vector3 &p_global_position, const bool p_navigation) {
	uint32_t control = get_control(p_global_position);
	set_control(p_global_position, (control & ~(0x1 << 1)) | enc_nav(p_navigation));
}

inline bool WorldScape3DData::get_control_navigation(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? false : is_nav(control);
}

inline void WorldScape3DData::set_control_auto(const Vector3 &p_global_position, const bool p_auto) {
	uint32_t control = get_control(p_global_position);
	set_control(p_global_position, (control & ~(0x1)) | enc_auto(p_auto));
}

inline bool WorldScape3DData::get_control_auto(const Vector3 &p_global_position) const {
	uint32_t control = get_control(p_global_position);
	return control == UINT32_MAX ? false : is_auto(control);
}

inline void WorldScape3DData::set_roughness(const Vector3 &p_global_position, const real_t p_roughness) {
	Color clr = get_pixel(TYPE_COLOR, p_global_position);
	clr.a = p_roughness;
	set_pixel(TYPE_COLOR, p_global_position, clr);
}

inline real_t WorldScape3DData::get_roughness(const Vector3 &p_global_position) const {
	return get_pixel(TYPE_COLOR, p_global_position).a;
}

inline void WorldScape3DData::update_master_height(const real_t p_height) {
	if (p_height < _master_height_range.x) {
		_master_height_range.x = p_height;
	} else if (p_height > _master_height_range.y) {
		_master_height_range.y = p_height;
	}
}

inline void WorldScape3DData::update_master_heights(const Vector2 &p_low_high) {
	if (p_low_high.x < _master_height_range.x) {
		_master_height_range.x = p_low_high.x;
	}
	if (p_low_high.y > _master_height_range.y) {
		_master_height_range.y = p_low_high.y;
	}
}
