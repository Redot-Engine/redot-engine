/**************************************************************************/
/*  worldscape_3d_ui.h                                                    */
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

#include "scene/gui/base_button.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"
#include "scene/resources/image_texture.h"

#include "worldscape_3d_tools.h"

class EditorPlugin;
class EditorFileDialog;
class WorldScape3DMenu;
class WorldScape3DOperationBuilder;

class WorldScape3DToolbar final : public VFlowContainer {
	GDCLASS(WorldScape3DToolbar, VFlowContainer);

	Ref<ButtonGroup> _add_tools = nullptr;
	Ref<ButtonGroup> _sub_tools = nullptr;
	WorldScape3DEditor *_terrain = nullptr;

public:
	explicit WorldScape3DToolbar(WorldScape3DEditor *editor);
	~WorldScape3DToolbar() override;

	Button *find_button(const StringName &icon) const;

	void on_tool_selected(BaseButton *button);
	void show_buttons(bool show_add, bool show_sub);
	void _notification(int p_what);

	static void _bind_methods();
};

class WorldScape3DUI final : public Node {
	GDCLASS(WorldScape3DUI, Node);
	CLASS_NAME();

	static constexpr auto ColorRaise = Colors::White;
	static constexpr auto ColorLower = Colors::Black;
	static constexpr Color ColorSmooth{ 0.5f, 0.f, 0.2f };
	static constexpr auto ColorLift = Colors::Orange;
	static constexpr auto ColorFlatten = Colors::BlueViolet;
	static constexpr Color ColorHeight{ 0.f, 0.32f, 0.4f };
	static constexpr auto ColorSlope = Colors::Yellow;
	static constexpr auto ColorPaint = Colors::WebGreen;
	static constexpr auto ColorSpray = Colors::PaleGreen;
	static constexpr auto ColorRoughness = Colors::RoyalBlue;
	static constexpr auto ColorAutoshader = Colors::DodgerBlue;
	static constexpr auto ColorHoles = Colors::Black;
	static constexpr Color ColorNavigation{ 0.28f, 0.f, 0.25f };
	static constexpr auto ColorInstancer = Colors::Crimson;
	static constexpr auto ColorPickColor = Colors::White;
	static constexpr auto ColorPickHeight = Colors::DarkRed;
	static constexpr auto ColorPickRough = Colors::RoyalBlue;

	static constexpr int OpNone = 0;
	static constexpr int OpPositiveOnly = 1;
	static constexpr int OpNegativeOnly = 2;

	Ref<ImageTexture> _region_texture = nullptr;
	WorldScape3DEditorPlugin *_plugin = nullptr;
	WorldScape3DMenu *_menu = nullptr;
	WorldScape3DToolbar *_toolbar = nullptr;
	WorldScape3DToolSettings *_tool_settings = nullptr;
	bool _settings_has_changed = false;
	bool _visible = false;

	WorldScape3DEditor::Tool _picking = WorldScape3DEditor::Tool::TOOL_MAX;
	Callable _pick_callback;
	Dictionary _brush_data;
	std::unique_ptr<WorldScape3DOperationBuilder> _operation_builder;
	WorldScape3DEditor::Tool _active_tool;
	WorldScape3DEditor::Tool _selected_tool;
	WorldScape3DEditor::Operation _active_operation;
	WorldScape3DEditor::Operation _selected_operation;
	bool _inverted_input = false;

	// 3 Editor decals: 0=cursor, 1=slope point 1, 2=slope point 2
	RID _mat_rid;
	RID editor_brush_texture_rid;

	Array editor_decal_position = { {}, {}, {} };
	Array editor_decal_rotation = { 0.f, 0.f, 0.f };
	Array editor_decal_size = { 0.f, 0.f, 0.f };
	Array editor_decal_color = { {}, {}, {} };
	Array editor_decal_visible = { false, false, false };
	real_t editor_decal_fade = 0.f;

	Ref<SceneTreeTimer> _update_timer;
	Timer *_editor_decal_timer = nullptr;

	Ref<ImageTexture> _ring_texture;
	RID editor_ring_texture_rid;

	void reset_decals();

public:
	explicit WorldScape3DUI(WorldScape3DEditorPlugin *plugin);
	WorldScape3DUI() = default;
	~WorldScape3DUI() override;

	void on_visible_update();

	void set_region_texture(Ref<ImageTexture> texture);

	bool is_shader_valid() const;

	void set_editor_decal_fade(real_t value);
	void set_decal_rotation(real_t angle);
	real_t get_editor_decal_fade() const { return editor_decal_fade; }

	void update_decal();
	void hide_decal();

	bool input_inverted() const { return _inverted_input; }

	void on_picking(WorldScape3DEditor::Tool tool, const Callable &callback);
	void clear_picking();
	bool is_picking() const;
	void pick(Vector3 global_pos);

	void set_active_operation();

	void set_visible(bool show, bool menu_only = false);
	bool is_visible() const { return _visible; }

	WorldScape3DMenu *get_menu() const { return _menu; }
	WorldScape3DToolbar *get_toolbar() const { return _toolbar; }
	WorldScape3DToolSettings *get_tool_settings() const { return _tool_settings; }

	void set_menu_visibility(Control *list, bool visible);

	void on_tool_changed(WorldScape3DEditor::Tool tool, WorldScape3DEditor::Operation operation);
	void on_setting_changed(const Variant &setting = {});

	WorldScape3DOperationBuilder *get_operation_builder() const {
		return _operation_builder.get();
	}

	void on_decal_timer();

	static void _bind_methods();

protected:
	void _notification(int what);
};
