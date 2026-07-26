/**************************************************************************/
/*  worldscape_3d_tools.h                                                 */
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

#include "modules/worldscape_3d/editor/worldscape_3d_editor.h"
#include "scene/gui/box_container.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/texture.h"

class Button;
class WorldScape3DEditorPlugin;

class WorldScape3DToolSettings : public PanelContainer {
	GDCLASS(WorldScape3DToolSettings, PanelContainer);

	WorldScape3DEditorPlugin *_plugin = nullptr;

	Ref<ShaderMaterial> _brush_preview_material;
	Button *_select_brush_button = nullptr;
	Array _selected_brush_imgs;

	HFlowContainer *_main_list = nullptr;
	VBoxContainer *_advanced_list = nullptr;
	VBoxContainer *_height_list = nullptr;
	VBoxContainer *_scale_list = nullptr;
	VBoxContainer *_rotation_list = nullptr;
	VBoxContainer *_color_list = nullptr;

	Dictionary _settings;

public:
	enum Layout {
		HORIZONTAL,
		VERTICAL,
		GRID,
	};

	enum SettingType {
		CHECKBOX,
		COLOR_SELECT,
		DOUBLE_SLIDER,
		OPTION,
		PICKER,
		MULTI_PICKER,
		SLIDER,
		LABEL,
		TYPE_MAX,
	};

	explicit WorldScape3DToolSettings(WorldScape3DEditorPlugin *plugin);
	~WorldScape3DToolSettings() override;

	void on_show_submenu(bool toggled, Button *button);

	// Settings
	void add_setting(const Dictionary &setting);

	// Brush data
	Dictionary get_brush_data() const;

	// Settings lists
	VBoxContainer *advanced_list() const { return _advanced_list; }
	VBoxContainer *scale_list() const { return _scale_list; }
	VBoxContainer *rotation_list() const { return _rotation_list; }
	VBoxContainer *height_list() const { return _height_list; }
	VBoxContainer *color_list() const { return _color_list; }

	Variant convert_setting(const String &setting) const;
	void set_setting(const String &setting, Variant value);
	void show_settings(const PackedStringArray &settings);
	void on_setting_changed(Variant setting = {});

	void on_pick(WorldScape3DEditor::Tool type);
	void on_picked(WorldScape3DEditor::Tool type, Color color, Vector3 position);
	void on_point_pick(WorldScape3DEditor::Tool type, const String &name);
	void on_point_picked(WorldScape3DEditor::Tool type, Color color, Vector3 position, const String &name);
	void on_plugin_setting(Variant value, const String &path);
	void on_label_pressed(const String &name, Variant vdefault);
	void on_brush_hover(bool hovering, Button *button);
	void on_drawable_toggled(bool button_pressed);

	void generate_brush_texture(Button *button);
	Ref<ShaderMaterial> get_brush_preview_material();

	static void _bind_methods();

	void _notification(int what);

private:
	void init();

	Container *create_submenu(Control *parent, const String &button_name, Layout layout, bool hover_pop = true);
	void add_brushes(Control *parent);

	// Utility functions to construct Dictionaries with variadics

	template <typename... Args>
	static void set_to_dictionary(Dictionary &dict, const char *key, Variant value, Args &&...args) {
		dict.set(key, std::move(value));
		set_to_dictionary(dict, std::forward<Args>(args)...);
	}
	static void set_to_dictionary(Dictionary &) {}

	template <typename... Args>
	void make_setting(Args &&...args) {
		Dictionary setting;
		set_to_dictionary(setting, std::forward<Args>(args)...);
		add_setting(setting);
	}
};
