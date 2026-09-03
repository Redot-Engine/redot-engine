/**************************************************************************/
/*  worldscape_3d_asset_dock.h                                            */
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

#include "scene/gui/box_container.h"
#include "scene/gui/flow_container.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/texture_button.h"
#include "scene/resources/packed_scene.h"

#include "scene/resources/style_box.h"

#include "../../worldscape_3d_assets.h"
#include "../worldscape_3d_editor.h"
#include "scene/main/window.h"

class ConfirmationDialog;
class ScrollContainer;
class HSlider;
class Button;
class OptionButton;
class Label;
class WorldScape3DEditorPlugin;
class WorldScape3DAssetDock;

class ListEntry : public MarginContainer {
	GDCLASS(ListEntry, MarginContainer);

	Ref<WorldScape3DAssetResource> _resource = nullptr;
	WorldScape3DAssets::AssetType _type = WorldScape3DAssets::AssetType::TYPE_TEXTURE;
	Ref<Texture2D> _thumbnail;
	mutable bool _drop_data = false;
	bool _is_hovered = false;
	bool _is_selected = false;
	bool _is_highlighted = false;
	Ref<WorldScape3DAssets> _asset_list = nullptr;

	Ref<StyleBox> _focus_style;
	Ref<StyleBox> _background;
	VBoxContainer *_label_rows = nullptr;
	Label *_name_label = nullptr;
	Label *_count_label = nullptr;
	MarginContainer *_margin = nullptr;
	FlowContainer *_button_row = nullptr;
	TextureButton *_button_clear = nullptr;
	TextureButton *_button_edit = nullptr;
	Control *_spacer = nullptr;
	TextureButton *_button_enabled = nullptr;
	Ref<Texture2D> _clear_icon;
	Ref<Texture2D> _edit_icon;
	Ref<Texture2D> _enabled_icon;
	Ref<Texture2D> _disabled_icon;
	Ref<Texture2D> _add_icon;

	void draw();
	void mouse_enter();
	void mouse_exit();

	void setup_buttons();
	void setup_label();
	void setup_count_label();

	void init();

public:
	explicit ListEntry(WorldScape3DAssets::AssetType type);
	~ListEntry() override;

	void set_assets(Ref<WorldScape3DAssets> assets_list) {
		_asset_list = assets_list;
	}

	void set_focus_style(const Ref<StyleBox> &style) {
		_focus_style = style;
	}

	void set_background(const Ref<StyleBox> &background) {
		_background = background;
	}

	Ref<WorldScape3DAssetResource> get_resource() const { return _resource; }

	void update_count_label();

	bool can_drop_data(const Point2 &point, const Variant &data) const override;
	void drop_data(const Point2 &point, const Variant &data) override;

	void set_edited_resource(Ref<Resource> res, bool no_signal = true);
	void on_resource_changed();

	void set_selected(bool selected);

	void clear();
	void select();
	void inspect();
	void enable();

	void gui_input(const Ref<InputEvent> &p_event) override;

	static void _bind_methods();
	void _notification(int what);
};

class ListContainer : public Container {
	GDCLASS(ListContainer, Container);

	WorldScape3DEditorPlugin *_plugin = nullptr;
	WorldScape3DAssets::AssetType _type = WorldScape3DAssets::AssetType::TYPE_TEXTURE;
	Vector<ListEntry *> _entries;
	int _selected_id = 0;
	real_t _height = 0.f;
	real_t _width = 83.f;
	Ref<StyleBox> _focus_style;
	bool _clearing_resource = false;

public:
	explicit ListContainer(WorldScape3DEditorPlugin *plugin);
	~ListContainer() override;

	void redraw();
	void clear();
	int get_selected_id() const { return _selected_id; }
	real_t get_entry_width() const { return _width; }
	Vector2 get_minimum_size() const override {
		return Vector2{ 0.f, _height };
	}

	WorldScape3DAssets::AssetType get_type() const { return _type; }
	void set_type(const WorldScape3DAssets::AssetType type) { _type = type; }

	Vector<ListEntry *> &get_entries() { return _entries; }
	const Vector<ListEntry *> &get_entries() const { return _entries; }

	void update_asset_list();
	void add_item(Ref<Resource> resource = {}, Ref<WorldScape3DAssets> assets = {});

	void on_resource_hovered(int id);
	void on_resource_inspected(Ref<Resource> resource);
	void on_resource_changed(Ref<Resource> resource, int id);
	void resource_modify(Ref<Resource> resource, int id);

	void set_selected_id(int id);
	void set_selected_after_swap(WorldScape3DAssets::AssetType, int, const int new_id) {
		set_selected_id(Math::clamp(new_id, 0, static_cast<int>(_entries.size() - 2)));
	}
	void set_entry_width(const real_t value) {
		_width = Math::clamp(value, 66.f, 230.f);
		redraw();
	}

	void _notification(int what);

private:
	WorldScape3DAssetDock *get_dock() const;
};

class WorldScape3DAssetDock final : public PanelContainer {
	GDCLASS(WorldScape3DAssetDock, PanelContainer);

	WorldScape3DEditorPlugin *_plugin = nullptr;

	ListContainer *_texture_list = nullptr;
	ListContainer *_mesh_list = nullptr;
	ListContainer *_current_list = nullptr;
	bool _updating_list = false;
	uint64_t _last_thumb_update_time = 0;

	OptionButton *_placement_opt = nullptr;
	Button *_floating_btn = nullptr;
	Button *_pinned_btn = nullptr;
	HSlider *_size_slider = nullptr;
	BoxContainer *_box = nullptr;
	BoxContainer *_buttons = nullptr;
	Button *_textures_btn = nullptr;
	Button *_meshes_btn = nullptr;
	ScrollContainer *_asset_container = nullptr;
	ConfirmationDialog *_confirm_dialog = nullptr;
	bool _confirmed = false;

	enum State : int {
		HIDDEN = -1,
		SIDEBAR = 0,
		BOTTOM = 1,
		WINDOWED = 2,
	};
	State _state = HIDDEN;

	enum Slot : int {
		POS_LEFT_UL = 0,
		POS_LEFT_BL = 1,
		POS_LEFT_UR = 2,
		POS_LEFT_BR = 3,
		POS_RIGHT_UL = 4,
		POS_RIGHT_BL = 5,
		POS_RIGHT_UR = 6,
		POS_RIGHT_BR = 7,
		POS_BOTTOM = 8,
		POS_MAX = 9,
	};
	Slot _slot = POS_RIGHT_BR;

	bool _initialized = false;

	Window *_window = nullptr;
	Window::Mode _rex_editor_last_state = Window::Mode::MODE_FULLSCREEN;

	Window *get_grandparent() const;

	void create_layout();

	void init();

public:
	explicit WorldScape3DAssetDock(WorldScape3DEditorPlugin *plugin);
	~WorldScape3DAssetDock() override;

	ListContainer *get_current_list() const { return _current_list; }
	ConfirmationDialog *get_confirmation_dialog() const { return _confirm_dialog; }

	// Dock placement
	void set_slot(int slot);
	void remove_dock(bool force = false);
	void update_dock();
	void update_layout();
	void update_thumbnails();

	// Dock button handlers
	void on_slider_changed(real_t value);
	void on_pin_changed(bool toggled);
	void on_textures_pressed();
	void on_meshes_pressed();
	void on_tool_changed(WorldScape3DEditor::Tool tool, WorldScape3DEditor::Operation operation);

	// Update Dock contents
	void update_assets();
	void remove_all_highlights();

	// Window management
	void make_dock_float();
	void create_window();
	void clamp_window_position();
	void on_window_input(Ref<InputEvent> event);
	void on_rex_window_entered();
	void on_rex_focus_entered();
	void on_rex_focus_exited();

	// Settings management
	void load_editor_settings();
	void save_editor_settings();

	bool is_dialog_confirmed() const { return _confirmed; }

	static void _bind_methods();

	void _notification(int what);

private:
	void dialog_confirm();
	void dialog_cancel();
};
