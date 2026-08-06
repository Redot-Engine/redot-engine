/**************************************************************************/
/*  worldscape_3d_editor.h                                                */
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

#include "editor/plugins/editor_plugin.h"
#include "editor/settings/editor_settings.h"

#include "scene/gui/base_button.h"
#include "scene/gui/box_container.h"
#include "scene/gui/flow_container.h"

#include "../worldscape_3d_region.h"
#include "gizmos.h"

class WorldScape3D;
class WorldScape3DUI;
class WorldScape3DAssetDock;
class EditorFileDialog;
class MenuButton;
class NavigationRegion3D;

class WorldScape3DEditor : public Object {
	GDCLASS(WorldScape3DEditor, Object);
	CLASS_NAME();

public: // Constants
	enum Tool {
		REGION,
		SCULPT,
		HEIGHT,
		TEXTURE,
		COLOR,
		ROUGHNESS,
		AUTOSHADER,
		HOLES,
		NAVIGATION,
		INSTANCER,
		ANGLE, // used for picking, TODO change to a picking tool
		SCALE, // used for picking
		TOOL_MAX,
	};

	static inline const char *TOOLNAME[] = {
		"Region",
		"Sculpt",
		"Height",
		"Texture",
		"Color",
		"Roughness",
		"Auto Shader",
		"Holes",
		"Navigation",
		"Instancer",
		"Angle",
		"Scale",
		"TOOL_MAX",
	};

	enum Operation {
		ADD,
		SUBTRACT,
		REPLACE,
		AVERAGE,
		GRADIENT,
		OP_MAX,
	};

	static inline const char *OPNAME[] = {
		"Add",
		"Subtract",
		"Replace",
		"Average",
		"Gradient",
		"OP_MAX",
	};

private:
	WorldScape3D *_terrain = nullptr;

	// Painter settings & variables
	Tool _tool = REGION;
	Operation _operation = ADD;
	Dictionary _brush_data;
	Vector3 _operation_position;
	Vector3 _operation_movement;
	Array _operation_movement_history;
	bool _is_operating = false;
	uint64_t _last_region_bounds_error = 0;
	TypedArray<WorldScape3DRegion> _original_regions; // Queue for undo
	TypedArray<WorldScape3DRegion> _edited_regions; // Queue for redo
	TypedArray<Vector2i> _added_removed_locations; // Queue for added/removed locations
	AABB _modified_area;
	Dictionary _undo_data; // See _get_undo_data for definition
	uint64_t _last_pen_tick = 0;

	void _send_region_aabb(const Vector2i &p_region_loc, const Vector2 &p_height_range = Vector2());
	Ref<WorldScape3DRegion> _operate_region(const Vector2i &p_region_loc);
	void _operate_map(const Vector3 &p_global_position, const real_t p_camera_direction);
	constexpr MapType _get_map_type() const;
	bool _is_in_bounds(const Point2i &p_pixel, const Point2i &p_size) const;
	Vector2 _get_uv_position(const Vector3 &p_global_position, const int p_region_size, const real_t p_vertex_spacing) const;
	Vector2 _get_rotated_uv(const Vector2 &p_uv, const real_t p_angle) const;
	void _store_undo();
	void _apply_undo(const Dictionary &p_data);

public:
	void set_terrain(WorldScape3D *p_terrain) { _terrain = p_terrain; }
	WorldScape3D *get_terrain() const { return _terrain; }

	void set_brush_data(const Dictionary &p_data);
	Dictionary get_brush_data() const { return _brush_data; }
	void set_tool(const Tool p_tool);
	Tool get_tool() const { return _tool; }
	void set_operation(const Operation p_operation) { _operation = CLAMP(p_operation, Operation(0), OP_MAX); }
	Operation get_operation() const { return _operation; }

	void start_operation(const Vector3 &p_global_position);
	bool is_operating() const { return _is_operating; }
	void operate(const Vector3 &p_global_position, const real_t p_camera_direction);
	void backup_region(const Ref<WorldScape3DRegion> &p_region);
	void stop_operation();

protected:
	static void _bind_methods();
};

VARIANT_ENUM_CAST(WorldScape3DEditor::Operation);
VARIANT_ENUM_CAST(WorldScape3DEditor::Tool);

// Inline functions

constexpr MapType WorldScape3DEditor::_get_map_type() const {
	switch (_tool) {
		case SCULPT:
		case HEIGHT:
		case INSTANCER:
			return TYPE_HEIGHT;
		case TEXTURE:
		case AUTOSHADER:
		case HOLES:
		case NAVIGATION:
		case ANGLE:
		case SCALE:
			return TYPE_CONTROL;
		case COLOR:
		case ROUGHNESS:
			return TYPE_COLOR;
		default:
			return TYPE_MAX;
	}
}

class WorldScape3DEditorPlugin final : public EditorPlugin {
	GDCLASS(WorldScape3DEditorPlugin, EditorPlugin);
	CLASS_NAME();

public:
	enum MouseMode {
		CameraMove = -1,
		None = 0,
		Operating = 1,
	};

private:
	std::unique_ptr<WorldScape3DEditor> _editor;

	Ref<EditorSettings> _editor_settings;

	WorldScape3DUI *_ui = nullptr;
	WorldScape3D *_last_terrain = nullptr;
	WorldScape3DAssetDock *_asset_dock = nullptr;
	NavigationRegion3D *_nav_region = nullptr;
	Ref<SceneTreeTimer> _scene_change_timer;

	// Events handling
	MouseMode _mouse_mode = None;
	bool _use_meta = false;
	bool _mod_alt = false;
	bool _mod_ctrl = false;
	bool _mod_shift = false;
	int _rmb_release_time = 0;
	int _last_mods = 0;
	Vector3 _mouse_global_position;
	Vector2 _current_region_pos;
	Ref<WorldScape3DRegionGizmo> _region_gizmo;
	Window *_rex_editor_window = nullptr;

	void init();

public:
	WorldScape3DEditorPlugin();
	~WorldScape3DEditorPlugin() override;

	WorldScape3DUI *get_ui() const { return _ui; }

	WorldScape3DEditor *get_editor() const;
	WorldScape3D *get_terrain() const;
	WorldScape3D *get_last_terrain() const;
	NavigationRegion3D *get_nav_region() const { return _nav_region; }

	WorldScape3DAssetDock *get_asset_dock() const { return _asset_dock; }
	Window *get_rex_editor_window() const { return _rex_editor_window; }

	bool is_selected() const;
	void select_terrain();

	MouseMode get_mouse_mode() const { return _mouse_mode; }
	Vector3 get_mouse_position() const { return _mouse_global_position; }
	bool is_modifier_on() const { return _mod_ctrl; }
	bool is_alt_modifier_on() const { return _mod_alt; }
	bool is_shift_on() const { return _mod_shift; }

	int get_rmb_release_time() const { return _rmb_release_time; }

	void make_visible(bool visible) override;
	void edit(Object *object) override;
	bool handles(Object *object) const override;
	void clear() override;

	void on_scene_changed(Node *scene_root);
	void on_scene_change_timeout();
	void on_focus_entered();

	void _notification(int p_what);

	AfterGUIInput forward_3d_gui_input(Camera3D *camera, const Ref<InputEvent> &event) override;
	AfterGUIInput read_input(const Ref<InputEvent> &event);

	EditorUndoRedoManager *get_undo_redo() {
		return EditorPlugin::get_undo_redo();
	}

	String get_plugin_name() const override { return "WorldScape3DEditor"; }

	// EditorSettings:
	void setup_editor_settings();
	void set_setting(const String &str, Variant value);
	Variant get_setting(const String &str, Variant default_value) const;
	bool has_settings(const String &str) const;
	void erase_setting(const String &str);
	void update_region_grid();

protected:
	bool consume_hotkey(Key code);
	bool is_terrain_valid(WorldScape3D *terrain = nullptr) const;
};

inline bool WorldScape3DEditor::_is_in_bounds(const Point2i &p_pixel, const Point2i &p_size) const {
	bool positive = p_pixel.x >= 0 && p_pixel.y >= 0;
	bool less_than_max = p_pixel.x < p_size.x && p_pixel.y < p_size.y;
	return positive && less_than_max;
}

inline Vector2 WorldScape3DEditor::_get_uv_position(const Vector3 &p_global_position, const int p_region_size, const real_t p_vertex_spacing) const {
	Vector2 descaled_position_2d = Vector2(p_global_position.x, p_global_position.z) / p_vertex_spacing;
	Vector2 region_position = descaled_position_2d / real_t(p_region_size);
	region_position = region_position.floor();
	Vector2 uv_position = (descaled_position_2d / real_t(p_region_size)) - region_position;
	return uv_position;
}

inline Vector2 WorldScape3DEditor::_get_rotated_uv(const Vector2 &p_uv, const real_t p_angle) const {
	Vector2 rotation_offset = Vector2(0.5f, 0.5f);
	Vector2 uv = (p_uv - rotation_offset).rotated(p_angle) + rotation_offset;
	return uv.clamp(V2_ZERO, Vector2(1.f, 1.f));
}
