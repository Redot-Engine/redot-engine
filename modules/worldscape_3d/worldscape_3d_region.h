/**************************************************************************/
/*  worldscape_3d_region.h                                                */
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
#include "worldscape_3d_util.h"

class WorldScape3DRegion : public Resource {
	GDCLASS(WorldScape3DRegion, Resource);
	CLASS_NAME();

public:
	// Constants
	enum MapType {
		TYPE_HEIGHT,
		TYPE_CONTROL,
		TYPE_COLOR,
		TYPE_MAX,
	};

	static constexpr Image::Format FORMAT[] = {
		Image::FORMAT_RF, // TYPE_HEIGHT
		Image::FORMAT_RF, // TYPE_CONTROL
		Image::FORMAT_RGBA8, // TYPE_COLOR
		Image::Format(TYPE_MAX), // Proper size of array instead of FORMAT_MAX
	};

	static constexpr const char *TYPESTR[] = {
		"TYPE_HEIGHT",
		"TYPE_CONTROL",
		"TYPE_COLOR",
		"TYPE_MAX",
	};

	static constexpr Color COLOR[] = {
		COLOR_BLACK, // TYPE_HEIGHT
		COLOR_CONTROL, // TYPE_CONTROL
		COLOR_ROUGHNESS, // TYPE_COLOR
		COLOR_NAN, // TYPE_MAX, unused just in case someone indexes the array
	};

private:
	// Saved data
	real_t _version = 0.8f; // Set to first version to ensure we always upgrades this
	int _region_size = 0;
	Vector2 _height_range = V2_ZERO;
	// Maps
	Ref<Image> _height_map;
	Ref<Image> _control_map;
	Ref<Image> _color_map;
	// Instancer
	Dictionary _instances; // Meshes{int} -> Cells{v2i} -> [ Transform3D, Color, Modified ]
	real_t _vertex_spacing = 1.f; // Vertex Spacing value that transforms are currently scaled.

	// Working data not saved to disk
	bool _deleted = false; // Marked for deletion on save
	bool _edited = false; // Marked for undo/redo storage
	bool _modified = false; // Marked for saving
	Vector2i _location = V2I_MAX;

public:
	WorldScape3DRegion() = default;
	~WorldScape3DRegion() override = default;

	void set_version(const real_t p_version);
	real_t get_version() const { return _version; }
	void set_region_size(const int p_region_size) { _region_size = CLAMP(p_region_size, 64, 2048); }
	int get_region_size() const { return _region_size; }

	// Maps
	void set_map(const MapType p_map_type, const Ref<Image> &p_image);
	Ref<Image> get_map(const MapType p_map_type) const;
	Image *get_map_ptr(const MapType p_map_type) const;
	void set_maps(const TypedArray<Image> &p_maps);
	TypedArray<Image> get_maps() const;
	void set_height_map(const Ref<Image> &p_map);
	Ref<Image> get_height_map() const { return _height_map; }
	void set_control_map(const Ref<Image> &p_map);
	Ref<Image> get_control_map() const { return _control_map; }
	void set_color_map(const Ref<Image> &p_map);
	Ref<Image> get_color_map() const { return _color_map; }
	void sanitize_maps();
	Ref<Image> sanitize_map(const MapType p_map_type, const Ref<Image> &p_map) const;
	bool validate_map_size(const Ref<Image> &p_map) const;

	void set_height_range(const Vector2 &p_range);
	Vector2 get_height_range() const { return _height_range; }
	void update_height(const real_t p_height);
	void update_heights(const Vector2 &p_low_high);
	void calc_height_range();

	// Instancer
	void set_instances(const Dictionary &p_instances) { _instances = p_instances; }
	Dictionary get_instances() const { return _instances; }
	void set_vertex_spacing(const real_t p_vertex_spacing) { _vertex_spacing = CLAMP(p_vertex_spacing, 0.25f, 100.f); }
	real_t get_vertex_spacing() const { return _vertex_spacing; }

	// File I/O
	Error save(const String &p_path = "", const bool p_16_bit = false);

	// Working Data
	void set_deleted(const bool p_deleted) { _deleted = p_deleted; }
	bool is_deleted() const { return _deleted; }
	void set_edited(const bool p_edited) { _edited = p_edited; }
	bool is_edited() const { return _edited; }
	void set_modified(const bool p_modified) { _modified = p_modified; }
	bool is_modified() const { return _modified; }
	void set_location(const Vector2i &p_location);
	Vector2i get_location() const { return _location; }

	// Utility
	void set_data(const Dictionary &p_data);
	Dictionary get_data() const;
	Ref<WorldScape3DRegion> duplicate(const bool p_deep = false);

protected:
	static void _bind_methods();
};

using MapType = WorldScape3DRegion::MapType;
VARIANT_ENUM_CAST(WorldScape3DRegion::MapType);
constexpr WorldScape3DRegion::MapType TYPE_HEIGHT = MapType::TYPE_HEIGHT;
constexpr WorldScape3DRegion::MapType TYPE_CONTROL = MapType::TYPE_CONTROL;
constexpr WorldScape3DRegion::MapType TYPE_COLOR = MapType::TYPE_COLOR;
constexpr WorldScape3DRegion::MapType TYPE_MAX = MapType::TYPE_MAX;
constexpr inline const Image::Format *FORMAT = WorldScape3DRegion::FORMAT;
constexpr inline const char *const *TYPESTR = WorldScape3DRegion::TYPESTR;
constexpr inline const Color *COLOR = WorldScape3DRegion::COLOR;

// Inline functions

inline void WorldScape3DRegion::update_height(const real_t p_height) {
	if (p_height < _height_range.x) {
		_height_range.x = p_height;
		_modified = true;
	} else if (p_height > _height_range.y) {
		_height_range.y = p_height;
		_modified = true;
	}
}

inline void WorldScape3DRegion::update_heights(const Vector2 &p_low_high) {
	if (p_low_high.x < _height_range.x) {
		_height_range.x = p_low_high.x;
		_modified = true;
	}
	if (p_low_high.y > _height_range.y) {
		_height_range.y = p_low_high.y;
		_modified = true;
	}
}
