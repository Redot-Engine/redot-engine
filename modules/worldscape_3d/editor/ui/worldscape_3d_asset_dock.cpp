/**************************************************************************/
/*  worldscape_3d_asset_dock.cpp                                          */
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

#include "worldscape_3d_asset_dock.h"

#include "editor/editor_interface.h"
#include "editor/settings/event_listener_line_edit.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/slider.h"
#include "scene/resources/image_texture.h"

#include "../../worldscape_3d.h"
#include "worldscape_3d_ui.h"

namespace {
constexpr auto ES_DOCK_SLOT = "terrain3d/dock/slot";
constexpr auto ES_DOCK_TILE_SIZE = "terrain3d/dock/tile_size";
constexpr auto ES_DOCK_FLOATING = "terrain3d/dock/floating";
constexpr auto ES_DOCK_PINNED = "terrain3d/dock/always_on_top";
constexpr auto ES_DOCK_WINDOW_POSITION = "terrain3d/dock/window_position";
constexpr auto ES_DOCK_WINDOW_SIZE = "terrain3d/dock/window_size";
constexpr auto ES_DOCK_TAB = "terrain3d/dock/tab";

constexpr uint64_t MAX_UPDATE_TIME = 1000;

// String format_number(int num) {
// 	bool is_negative = num < 0;
// 	String str_num = String::num_int64(Math::abs(num));
// 	String result = "";
// 	int length = str_num.length();
// 	for (int i = 0; i < length; ++i) {
// 		result = str_num[length - 1 - i] + result;
// 		if (i < length - 1 && (i + 1) % 3 == 0) {
// 			result = "," + result;
// 		}
// 	}
// 	return is_negative ? "-" + result : result;
// }
} //namespace

void ListEntry::init() {
	_clear_icon = get_theme_icon("Close", "EditorIcons");
	_edit_icon = get_theme_icon("Edit", "EditorIcons");
	_enabled_icon = get_theme_icon("GuiVisibilityVisible", "EditorIcons");
	_disabled_icon = get_theme_icon("GuiVisibilityHidden", "EditorIcons");
	_add_icon = get_theme_icon("Add", "EditorIcons");

	_focus_style = get_theme_stylebox("focus", "Button")->duplicate();
	_focus_style->set("border_width_all", 2);
	_focus_style->set("border_color", Color{ 1., 1., 1., .67 });
	_background = get_theme_stylebox("pressed", "Button");

	set_mouse_filter(Control::MOUSE_FILTER_PASS);
	add_theme_constant_override("margin_top", 5);
	add_theme_constant_override("margin_left", 5);
	add_theme_constant_override("margin_right", 5);
	if (!get_children().has(_label_rows)) {
		add_child(_label_rows, true);
	}

	setup_buttons();
	setup_label();
	setup_count_label();
}

ListEntry::ListEntry(const WorldScape3DAssets::AssetType type) {
	set_name("ListEntry");

	_type = type;
	_label_rows = memnew(VBoxContainer);
	_margin = memnew(MarginContainer);
	_button_row = memnew(FlowContainer);
	_button_clear = memnew(TextureButton);
	_button_edit = memnew(TextureButton);
	_spacer = memnew(Control);
	_button_enabled = memnew(TextureButton);
}

ListEntry::~ListEntry() {
	if (_count_label) {
		_count_label->queue_free();
	}
	if (_name_label) {
		_name_label->queue_free();
	}
	_button_enabled->queue_free();
	_spacer->queue_free();
	_button_edit->queue_free();
	_button_clear->queue_free();
	_button_row->queue_free();
	_margin->queue_free();
	_label_rows->queue_free();
}

void ListEntry::_bind_methods() {
	ADD_SIGNAL(MethodInfo("hovered"));
	ADD_SIGNAL(MethodInfo("selected"));
	ADD_SIGNAL(MethodInfo("changed", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE)));
	ADD_SIGNAL(MethodInfo("inspected", PropertyInfo(Variant::OBJECT, "resource", PROPERTY_HINT_RESOURCE_TYPE)));
}

void ListEntry::_notification(int what) {
	switch (what) {
		case NOTIFICATION_POST_ENTER_TREE:
			init();
			break;
		case NOTIFICATION_DRAW:
			draw();
			break;
		case NOTIFICATION_MOUSE_ENTER:
			mouse_enter();
			break;
		case NOTIFICATION_MOUSE_EXIT:
			mouse_exit();
			break;
		default:
			break;
	}
}

void ListEntry::draw() {
	// Hide spacer if icons are crowding small textures
	_spacer->set_visible(get_size().x > 70); // || _type == WorldScape3DAssets::TYPE_TEXTURE);

	auto rect = Rect2(Vector2{ 0, 0 }, get_size());
	if (_resource.is_null()) {
		draw_style_box(_background, rect);
		draw_texture(_add_icon, (get_size() / 2) - (_add_icon->get_size() / 2));
	} else {
		if (_type == WorldScape3DAssets::TYPE_TEXTURE) {
			Ref<WorldScape3DTextureAsset> texture_asset = _resource;
			if (texture_asset.is_valid()) {
				this->set_self_modulate(texture_asset->get_albedo_color());
				_thumbnail = texture_asset->get_albedo_texture();
				if (_thumbnail.is_valid()) {
					draw_texture_rect(_thumbnail, rect, false);
					set_texture_filter(CanvasItem::TEXTURE_FILTER_NEAREST_WITH_MIPMAPS);
				}
			}
		} else if (Ref<WorldScape3DMeshAsset> mesh_asset = _resource; mesh_asset.is_valid()) {
			//int id = mesh_asset->get_id();
			_thumbnail = mesh_asset->get_thumbnail();
			if (_thumbnail.is_valid()) {
				draw_texture_rect(_thumbnail, rect, false);
				set_texture_filter(CanvasItem::TEXTURE_FILTER_LINEAR_WITH_MIPMAPS);
			} else {
				draw_rect(rect, Color(.15, .15, .15, 1.));
			}
			_button_enabled->set_pressed_no_signal(!mesh_asset->is_enabled());
		}
	}
	//_count_label->add_theme_font_size_override("font_size", std::max<int>(11, rect.size.x/12));
	_name_label->add_theme_font_size_override("font_size", std::max<int>(12, 4 + rect.size.x / 10));
	if (_drop_data) {
		draw_style_box(_focus_style, rect);
	}
	if (_is_hovered) {
		draw_rect(rect, Color{ 1, 1, 1, 0.2 });
	}
	if (_is_selected) {
		draw_style_box(_focus_style, rect);
	}
}

void ListEntry::mouse_enter() {
	if (get_size().x <= 70) {
		_name_label->set_text("");
	} else if (_resource.is_null()) {
		_name_label->set_text(String{ "\n\nAdd " } + (_type == WorldScape3DAssets::TYPE_TEXTURE ? "Texture" : "Mesh"));
	} else if (Ref<WorldScape3DTextureAsset> texture_asset = _resource;
			_type == WorldScape3DAssets::TYPE_TEXTURE && texture_asset.is_valid()) {
		auto name = texture_asset->Resource::get_name();
		if (name.is_empty()) {
			name = texture_asset->get_name();
		}
		_name_label->set_text(name);
	} else if (Ref<WorldScape3DMeshAsset> mesh_asset = _resource; mesh_asset.is_valid()) {
		auto name = mesh_asset->Resource::get_name();
		if (name.is_empty()) {
			name = mesh_asset->get_name();
		}
		_name_label->set_text(name);
	}
	_name_label->set_visible(true);
	_is_hovered = true;
	emit_signal("hovered");
	queue_redraw();
}

void ListEntry::mouse_exit() {
	_name_label->set_visible(false);
	_is_hovered = false;
	_drop_data = false;
	queue_redraw();
}

void ListEntry::setup_buttons() {
	static constexpr Vector2 icon_size{ 12, 12 };

	_margin->set_mouse_filter(MOUSE_FILTER_PASS);
	_margin->add_theme_constant_override("margin_top", 5);
	_margin->add_theme_constant_override("margin_left", 5);
	_margin->add_theme_constant_override("margin_right", 5);
	if (!get_children().has(_margin)) {
		add_child(_margin);
	}

	_button_row->set_h_size_flags(SIZE_EXPAND_FILL);
	_button_row->set_alignment(FlowContainer::ALIGNMENT_CENTER);
	_button_row->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	if (!_margin->get_children().has(_button_row)) {
		_margin->add_child(_button_row);
	}

	if (_type == WorldScape3DAssets::TYPE_MESH) {
		_button_enabled->set_texture_normal(_enabled_icon);
		_button_enabled->set_texture_pressed(_disabled_icon);
		_button_enabled->set_custom_minimum_size(icon_size);
		_button_enabled->set_h_size_flags(Control::SIZE_SHRINK_END);
		_button_enabled->set_visible(_resource.is_valid());
		_button_enabled->set_tooltip_text("Enable Instances");
		_button_enabled->set_toggle_mode(true);
		_button_enabled->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		_button_enabled->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
		if (!_button_enabled->is_connected("pressed", callable_mp(this, &ListEntry::enable))) {
			_button_enabled->connect("pressed", callable_mp(this, &ListEntry::enable));
		}
		if (!_button_row->get_children().has(_button_enabled)) {
			_button_row->add_child(_button_enabled, true);
		}
	}

	_button_edit->set_texture_normal(_edit_icon);
	_button_edit->set_custom_minimum_size(icon_size);
	_button_edit->set_h_size_flags(Control::SIZE_SHRINK_END);
	_button_edit->set_visible(_resource.is_valid());
	_button_edit->set_tooltip_text("Edit Asset");
	_button_edit->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	_button_edit->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	if (!_button_edit->is_connected("pressed", callable_mp(this, &ListEntry::inspect))) {
		_button_edit->connect("pressed", callable_mp(this, &ListEntry::inspect));
	}
	if (!_button_row->get_children().has(_button_edit)) {
		_button_row->add_child(_button_edit, true);
	}

	_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	if (!_button_row->get_children().has(_spacer)) {
		_button_row->add_child(_spacer, true);
	}

	_button_clear->set_texture_normal(_clear_icon);
	_button_clear->set_custom_minimum_size(icon_size);
	_button_clear->set_h_size_flags(Control::SIZE_SHRINK_END);
	_button_clear->set_visible(_resource.is_valid());
	_button_clear->set_tooltip_text("Clear Asset");
	_button_clear->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	_button_clear->set_default_cursor_shape(Control::CURSOR_POINTING_HAND);
	if (!_button_clear->is_connected("pressed", callable_mp(this, &ListEntry::clear))) {
		_button_clear->connect("pressed", callable_mp(this, &ListEntry::clear));
	}
	if (!_button_row->get_children().has(_button_clear)) {
		_button_row->add_child(_button_clear, true);
	}
}

void ListEntry::setup_label() {
	if (!_name_label) {
		_name_label = memnew(Label);
	}
	_name_label->set_name("MeshLabel");
	_name_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	_name_label->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
	_name_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_name_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_name_label->add_theme_color_override("font_color", Colors::White);
	_name_label->add_theme_color_override("font_shadow_color", Colors::Black);
	_name_label->add_theme_constant_override("shadow_offset_x", 1.);
	_name_label->add_theme_constant_override("shadow_offset_y", 1.);
	_name_label->add_theme_font_size_override("font_size", 15);
	_name_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	_name_label->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	if (!get_children().has(_name_label)) {
		add_child(_name_label, true);
	}
}

void ListEntry::setup_count_label() {
	if (!_count_label) {
		_count_label = memnew(Label);
	}
	_count_label->set_name("CountLabel");
	_count_label->set_text("");
	_count_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	_count_label->set_vertical_alignment(VERTICAL_ALIGNMENT_BOTTOM);
	_count_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_count_label->add_theme_color_override("font_color", Colors::White);
	_count_label->add_theme_color_override("font_shadow_color", Colors::Black);
	_count_label->add_theme_constant_override("shadow_offset_x", 1.);
	_count_label->add_theme_constant_override("shadow_offset_y", 1.);
	_count_label->add_theme_font_size_override("font_size", 14);
	if (!_label_rows->get_children().has(_count_label)) {
		_label_rows->add_child(_count_label, true);
	}
	_label_rows->add_theme_constant_override("separation", -5.);

	Ref<WorldScape3DMeshAsset> mesh_resource = _resource;
	if (mesh_resource.is_null()) {
		return;
	}
	update_count_label();
}

void ListEntry::update_count_label() {
	if (_type != WorldScape3DAssets::AssetType::TYPE_MESH || (_resource.is_valid() && !_resource->is_referenced())) {
		_count_label->set_text("");
		return;
	}
	Ref<WorldScape3DMeshAsset> mesh_resource = _resource;
	if (mesh_resource.is_null()) {
		_count_label->set_text(String::num_int64(0));
	}
}

void ListEntry::gui_input(const Ref<InputEvent> &p_event) {
	MarginContainer::gui_input(p_event);
	Ref<InputEventMouseButton> inputMouseButton = p_event;
	if (inputMouseButton.is_valid()) {
		switch (inputMouseButton->get_button_index()) {
			case MouseButton::LEFT: {
				// If "Add new" is clicked
				if (_resource.is_valid()) {
					emit_signal("selected");
				} else {
					if (_type == WorldScape3DAssets::TYPE_TEXTURE) {
						set_edited_resource(memnew(WorldScape3DTextureAsset), false);
					} else {
						set_edited_resource(memnew(WorldScape3DMeshAsset), false);
					}
				}
				select();
				break;
			}
			case MouseButton::RIGHT: {
				if (_resource.is_valid()) {
					select();
				}
				break;
			}
			case MouseButton::MIDDLE: {
				if (_resource.is_valid()) {
					clear();
				}
				break;
			}
			default:
				break;
		}
	}
}

bool ListEntry::can_drop_data(const Point2 &point, const Variant &p_dropped_data) const {
	_drop_data = false;
	if (p_dropped_data.get_type() == Variant::DICTIONARY) {
		PackedStringArray files = Dictionary{ p_dropped_data }["files"];
		if (files.size() == 1) {
			const_cast<ListEntry *>(this)->queue_redraw(); // no const in GDScript
			_drop_data = true;
		}
	}
	return _drop_data;
}

void ListEntry::drop_data(const Point2 &point, const Variant &p_dropped_data) {
	if (p_dropped_data.get_type() == Variant::DICTIONARY) {
		PackedStringArray files = Dictionary{ p_dropped_data }["files"];
		Ref<Resource> res = ResourceLoader::load(files[0]);
		if (Ref<Texture2D> texture = res; res.is_valid() && _type == WorldScape3DAssets::TYPE_TEXTURE) {
			Ref ta = memnew(WorldScape3DTextureAsset);
			if (Ref<WorldScape3DTextureAsset> t3d = _resource; t3d.is_valid()) {
				ta->set_id(t3d->get_id());
			}
			ta->set_albedo_texture(texture);
			set_edited_resource(ta, false);
			_resource = ta;
		} else if (Ref<WorldScape3DTextureAsset> text3d = res; text3d.is_valid() && _type == WorldScape3DAssets::TYPE_TEXTURE) {
			if (Ref<WorldScape3DTextureAsset> texture_asset = _resource; texture_asset.is_valid()) {
				text3d->set_id(texture_asset->get_id());
			}
			set_edited_resource(text3d, false);
		} else if (Ref<PackedScene> scene = res; scene.is_valid() && _type == WorldScape3DAssets::TYPE_MESH) {
			Ref ma = memnew(WorldScape3DMeshAsset);
			if (Ref<WorldScape3DMeshAsset> mesh_asset = _resource; mesh_asset.is_valid()) {
				ma->set_id(mesh_asset->get_id());
			}
			set_edited_resource(ma, false);
			ma->set_scene_file(scene);
			_resource = ma;
		} else if (Ref<WorldScape3DMeshAsset> mesh3d = res; mesh3d.is_valid() && _type == WorldScape3DAssets::TYPE_MESH) {
			if (Ref<WorldScape3DMeshAsset> mesh_asset = _resource; mesh_asset.is_valid()) {
				mesh3d->set_id(mesh_asset->get_id());
			}
			set_edited_resource(mesh3d, false);
		}
		emit_signal("selected");
		emit_signal("inspected", _resource);
	}
}

void ListEntry::set_edited_resource(Ref<Resource> res, const bool no_signal) {
	if (no_signal || res.is_valid()) {
		_resource = res;
		if (_resource.is_valid()) {
			_resource->connect("setting_changed", callable_mp(this, &ListEntry::on_resource_changed));
			_resource->connect("file_changed", callable_mp(this, &ListEntry::on_resource_changed));
			if (Ref<WorldScape3DMeshAsset> mesh = _resource; mesh.is_valid()) {
				mesh->connect("instancer_setting_changed", callable_mp(this, &ListEntry::on_resource_changed));
			}
		}
		if (_button_clear) {
			_button_clear->set_visible(_resource.is_valid());
		}
		queue_redraw();
	}
	if (!no_signal) {
		emit_signal("changed", res);
	}
}

void ListEntry::on_resource_changed() {
	queue_redraw();
	emit_signal("changed", _resource);
}

void ListEntry::set_selected(const bool selected) {
	_is_selected = selected;
	queue_redraw();
}

void ListEntry::enable() {
	if (const Ref<WorldScape3DMeshAsset> mesh = _resource; mesh.is_valid()) {
		mesh->set_enabled(!mesh->is_enabled());
	}
}

void ListEntry::select() {
	emit_signal("selected");
}
void ListEntry::inspect() {
	emit_signal("inspected", _resource);
}

void ListEntry::clear() {
	if (_resource.is_valid()) {
		set_edited_resource(nullptr, false);
		update_count_label();
	}
}

ListContainer::ListContainer(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin } {
	set_name("ListContainer");
	set_v_size_flags(SIZE_EXPAND_FILL);
	set_h_size_flags(SIZE_EXPAND_FILL);
}

ListContainer::~ListContainer() {
	clear();
}

void ListContainer::clear() {
	for (auto *e : _entries) {
		e->queue_free();
	}
	_entries.clear();
}

void ListContainer::update_asset_list() {
	clear();

	// Grab terrain
	WorldScape3D *terrain = nullptr;
	if (auto t = _plugin->get_terrain(); t) {
		terrain = t;
	} else if (auto lt = _plugin->get_last_terrain(); lt) {
		terrain = lt;
	} else {
		return;
	}

	auto assets = terrain->get_assets();
	if (assets.is_null()) {
		return;
	}

	if (_type == WorldScape3DAssets::TYPE_TEXTURE) {
		const auto texture_count = assets->get_texture_count();
		for (int i = 0; i < texture_count; ++i) {
			auto texture = assets->get_texture(i);
			add_item(texture);
		}
		if (texture_count < WorldScape3DAssets::MAX_TEXTURES) {
			add_item();
		}
	} else {
		const auto mesh_count = assets->get_mesh_count();
		for (int i = 0; i < mesh_count; ++i) {
			auto mesh = assets->get_mesh_asset(i);
			add_item(mesh, assets);
		}
		if (mesh_count < WorldScape3DAssets::MAX_MESHES) {
			add_item();
		}
		if (_selected_id >= mesh_count || _selected_id < 0) {
			set_selected_id(0);
		}
	}
}

void ListContainer::add_item(Ref<Resource> resource, Ref<WorldScape3DAssets> assets) {
	auto entry = memnew(ListEntry(_type));
	entry->set_focus_style(_focus_style);
	const int id = _entries.size();

	entry->set_edited_resource(resource);
	if (!entry->is_connected("hovered", callable_mp(this, &ListContainer::on_resource_hovered).bind(id))) {
		entry->connect("hovered", callable_mp(this, &ListContainer::on_resource_hovered).bind(id));
	}
	if (!entry->is_connected("selected", callable_mp(this, &ListContainer::set_selected_id).bind(id))) {
		entry->connect("selected", callable_mp(this, &ListContainer::set_selected_id).bind(id));
	}
	if (!entry->is_connected("inspected", callable_mp(this, &ListContainer::on_resource_inspected))) {
		entry->connect("inspected", callable_mp(this, &ListContainer::on_resource_inspected));
	}
	if (!entry->is_connected("changed", callable_mp(this, &ListContainer::on_resource_changed).bind(id))) {
		entry->connect("changed", callable_mp(this, &ListContainer::on_resource_changed).bind(id));
	}
	entry->set_assets(assets);
	add_child(entry, true);
	_entries.push_back(entry);

	if (resource.is_valid()) {
		entry->set_selected(id == _selected_id);
		if (!resource->is_connected("id_changed", callable_mp(this, &ListContainer::set_selected_after_swap))) {
			resource->connect("id_changed", callable_mp(this, &ListContainer::set_selected_after_swap));
		}
	}
}

void ListContainer::on_resource_hovered(const int id) {
	if (_type == WorldScape3DAssets::TYPE_MESH) {
		auto terrain = _plugin->get_terrain();
		if (terrain) {
			terrain->get_assets()->create_mesh_thumbnails(id);
		}
	}
}

void ListContainer::set_selected_id(const int id) {
	_selected_id = id;

	for (int i = 0; i < _entries.size(); ++i) {
		_entries[i]->set_selected(i == _selected_id);
	}
	_plugin->select_terrain();

	// Select Paint tool if clicking a texture
	auto tool = _plugin->get_editor()->get_tool();
	if (_type == WorldScape3DAssets::TYPE_TEXTURE && !(tool == WorldScape3DEditor::Tool::TEXTURE || tool == WorldScape3DEditor::Tool::COLOR || tool == WorldScape3DEditor::ROUGHNESS)) {
		if (auto paint_btn = cast_to<Button>(_plugin->get_ui()->get_toolbar()->find_button("TerrainPaintTexture")); paint_btn) {
			paint_btn->set_pressed(true);
			_plugin->get_ui()->on_tool_changed(WorldScape3DEditor::Tool::TEXTURE, WorldScape3DEditor::Operation::REPLACE);
		}
	} else if (_type == WorldScape3DAssets::TYPE_MESH && tool != WorldScape3DEditor::Tool::INSTANCER) {
		if (auto instance_btn = cast_to<Button>(_plugin->get_ui()->get_toolbar()->find_button("TerrainInstancer")); instance_btn) {
			instance_btn->set_pressed(true);
			_plugin->get_ui()->on_tool_changed(WorldScape3DEditor::Tool::INSTANCER, WorldScape3DEditor::Operation::ADD);
		}
	}
	// Update editor with selected brush
	_plugin->get_ui()->on_setting_changed();
}

void ListContainer::on_resource_inspected(Ref<Resource> resource) {
	EditorInterface::get_singleton()->edit_resource(resource);
}

void ListContainer::on_resource_changed(Ref<Resource> resource, int id) {
	if (resource.is_null() && _clearing_resource) {
		return;
	}

	if (resource.is_null()) {
		auto asset_dock = get_dock();
		auto confirm_dialog = asset_dock->get_confirmation_dialog();
		if (_type == WorldScape3DAssets::TYPE_TEXTURE) {
			confirm_dialog->set_text("Are you sure you want to clear this texture?");
		} else {
			confirm_dialog->set_text("Are you sure you want to clear this mesh and delete all instances?");
		}
		if (!confirm_dialog->is_connected("confirmed", callable_mp(this, &ListContainer::resource_modify).bind(resource, id))) {
			confirm_dialog->connect("confirmed", callable_mp(this, &ListContainer::resource_modify).bind(resource, id));
		}
		confirm_dialog->popup_centered();
		return;
	}
	resource_modify(resource, id);
}

void ListContainer::resource_modify(Ref<Resource> resource, int id) {
	auto asset_dock = get_dock();
	auto confirm_dialog = asset_dock->get_confirmation_dialog();
	if (confirm_dialog->is_connected("confirmed", callable_mp(this, &ListContainer::resource_modify).bind(resource, id))) {
		confirm_dialog->disconnect("confirmed", callable_mp(this, &ListContainer::resource_modify).bind(resource, id));
		if (!asset_dock->is_dialog_confirmed()) {
			update_asset_list();
			set_selected_id(id);
			queue_redraw();
			return;
		}
	}

	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		_plugin->select_terrain();
	}

	terrain = _plugin->get_terrain();
	if (terrain) {
		auto assets = terrain->get_assets();
		if (_type == WorldScape3DAssets::TYPE_TEXTURE) {
			assets->set_texture(id, resource);
		} else {
			assets->set_mesh_asset(id, resource);
			assets->create_mesh_thumbnails(id);
		}
		// If removing an entry, clear inspector
		if (resource.is_null()) {
			EditorInterface::get_singleton()->inspect_object(nullptr);
		}
	}

	// If null resource, remove from list
	if (resource.is_null()) {
		int last_offset = 2;
		if (id == _entries.size() - 2) {
			last_offset = 3;
		}
		set_selected_id(Math::clamp(_selected_id, 0, static_cast<int>(_entries.size()) - last_offset));
	}
}

void ListContainer::redraw() {
	_height = 0.f;
	int id = 0;
	real_t separation = 4.f;
	int columns = Math::clamp(get_size().x / _width, 1.f, 100.f);

	for (auto &child : get_children()) {
		Control *control = cast_to<Control>(child);
		if (control) {
			Vector2 sz = Vector2{ _width, _width } - Vector2{ separation, separation };
			Vector2 pos = _width * Vector2{ static_cast<real_t>(id % columns), static_cast<real_t>(id / columns) } + Vector2{ separation / columns, separation / columns };
			control->set_size(sz);
			control->set_position(pos);
			_height = std::max(_height, pos.y + _width);
			++id;
		}
	}
}

void ListContainer::_notification(int what) {
	if (what == NOTIFICATION_SORT_CHILDREN) {
		redraw();
	}
}

WorldScape3DAssetDock *ListContainer::get_dock() const {
	auto parent = get_parent();
	if (parent) {
		parent = parent->get_parent();
		if (parent) {
			return cast_to<WorldScape3DAssetDock>(parent->get_parent());
		}
	}
	return nullptr;
}

Window *WorldScape3DAssetDock::get_grandparent() const {
	auto parent = get_parent();
	if (parent) {
		return cast_to<Window>(parent->get_parent());
	}
	return nullptr;
}

void WorldScape3DAssetDock::create_layout() {
	set_custom_minimum_size(Vector2{ 256.f, 95.f });
	set_offset(Side::SIDE_RIGHT, 766.f);
	set_offset(Side::SIDE_BOTTOM, 100.f);

	if (!_box) {
		_box = memnew(BoxContainer);
	}
	_box->set_name("Box");
	_box->set_layout_direction(LAYOUT_DIRECTION_LTR);
	_box->set_v_size_flags(SIZE_FILL | SIZE_EXPAND);
	_box->set_vertical(true);
	if (!get_children().has(_box)) {
		add_child(_box);
	}

	if (!_buttons) {
		_buttons = memnew(BoxContainer);
	}
	_buttons->set_name("Buttons");
	if (!_box->get_children().has(_buttons)) {
		_box->add_child(_buttons);
	}

	if (!_textures_btn) {
		_textures_btn = memnew(Button);
	}
	_textures_btn->set_name("TexturesBtn");
	_textures_btn->set_custom_minimum_size(Vector2{ 80.f, 30.f });
	_textures_btn->set_h_size_flags(SIZE_FILL | SIZE_EXPAND);
	_textures_btn->set_v_size_flags(SIZE_SHRINK_BEGIN);
	_textures_btn->set_toggle_mode(true);
	_textures_btn->set_pressed(true);
	_textures_btn->set_text("Textures");
	if (!_buttons->get_children().has(_textures_btn)) {
		_buttons->add_child(_textures_btn);
	}

	if (!_meshes_btn) {
		_meshes_btn = memnew(Button);
	}
	_meshes_btn->set_name("MeshesBtn");
	_meshes_btn->set_custom_minimum_size(Vector2{ 80.f, 30.f });
	_meshes_btn->set_h_size_flags(SIZE_FILL | SIZE_EXPAND);
	_meshes_btn->set_v_size_flags(SIZE_SHRINK_BEGIN);
	_meshes_btn->set_toggle_mode(true);
	_meshes_btn->set_text("Meshes");
	if (!_buttons->get_children().has(_meshes_btn)) {
		_buttons->add_child(_meshes_btn);
	}

	if (!_placement_opt) {
		_placement_opt = memnew(OptionButton);
	}
	_placement_opt->set_name("PlacementOpt");
	_placement_opt->set_custom_minimum_size(Vector2{ 80.f, 30.f });
	_placement_opt->set_h_size_flags(SIZE_FILL | SIZE_EXPAND);
	_placement_opt->set_v_size_flags(SIZE_SHRINK_BEGIN);
	_placement_opt->add_item("Left_UL", 0);
	_placement_opt->add_item("Left_BL", 1);
	_placement_opt->add_item("Left_UR", 2);
	_placement_opt->add_item("Left_BR", 3);
	_placement_opt->add_item("Right_UL", 4);
	_placement_opt->add_item("Right_BL", 5);
	_placement_opt->add_item("Right_UR", 6);
	_placement_opt->add_item("Right_BR", 7);
	_placement_opt->add_item("Bottom", 8);
	_placement_opt->select(7);
	if (!_buttons->get_children().has(_placement_opt)) {
		_buttons->add_child(_placement_opt);
	}

	if (!_size_slider) {
		_size_slider = memnew(HSlider);
	}
	_size_slider->set_name("SizeSlider");
	_size_slider->set_h_size_flags(SIZE_FILL | SIZE_EXPAND);
	_size_slider->set_min(66.f);
	_size_slider->set_max(230.f);
	_size_slider->set_value(90.f);
	if (!_buttons->get_children().has(_size_slider)) {
		_buttons->add_child(_size_slider);
	}

	if (!_floating_btn) {
		_floating_btn = memnew(Button);
	}
	_floating_btn->set_name("Floating");
	_floating_btn->set_h_size_flags(SIZE_SHRINK_BEGIN);
	_floating_btn->set_v_size_flags(SIZE_SHRINK_BEGIN);
	_floating_btn->set_tooltip_text("Pop this dock out to a floating window.");
	_floating_btn->set_toggle_mode(true);
	_floating_btn->set_flat(true);
	_floating_btn->set_text("F");
	if (!_buttons->get_children().has(_floating_btn)) {
		_buttons->add_child(_floating_btn);
	}

	if (!_pinned_btn) {
		_pinned_btn = memnew(Button);
	}
	_pinned_btn->set_name("Pinned");
	_pinned_btn->set_h_size_flags(SIZE_SHRINK_BEGIN);
	_pinned_btn->set_v_size_flags(SIZE_SHRINK_BEGIN);
	_pinned_btn->set_tooltip_text("Make this window \"Always on top\".");
	_pinned_btn->set_toggle_mode(true);
	_pinned_btn->set_flat(true);
	_pinned_btn->set_text("P");
	if (!_buttons->get_children().has(_pinned_btn)) {
		_buttons->add_child(_pinned_btn);
	}

	if (!_asset_container) {
		_asset_container = memnew(ScrollContainer);
	}
	_asset_container->set_name("ScrollContainer");
	_asset_container->set_h_size_flags(SIZE_FILL | SIZE_EXPAND);
	_asset_container->set_v_size_flags(SIZE_FILL | SIZE_EXPAND);
	if (!_box->get_children().has(_asset_container)) {
		_box->add_child(_asset_container);
	}
}

void WorldScape3DAssetDock::init() {
	if (!_initialized) {
		return;
	}

	if (!is_connected("resized", callable_mp(this, &WorldScape3DAssetDock::update_layout))) {
		connect("resized", callable_mp(this, &WorldScape3DAssetDock::update_layout));
	}
	if (!_textures_btn->is_connected("pressed", callable_mp(this, &WorldScape3DAssetDock::on_textures_pressed))) {
		_textures_btn->connect("pressed", callable_mp(this, &WorldScape3DAssetDock::on_textures_pressed));
	}
	if (!_meshes_btn->is_connected("pressed", callable_mp(this, &WorldScape3DAssetDock::on_meshes_pressed))) {
		_meshes_btn->connect("pressed", callable_mp(this, &WorldScape3DAssetDock::on_meshes_pressed));
	}
	if (!_placement_opt->is_connected("item_selected", callable_mp(this, &WorldScape3DAssetDock::set_slot))) {
		_placement_opt->connect("item_selected", callable_mp(this, &WorldScape3DAssetDock::set_slot));
	}
	if (!_size_slider->is_connected("value_changed", callable_mp(this, &WorldScape3DAssetDock::on_slider_changed))) {
		_size_slider->connect("value_changed", callable_mp(this, &WorldScape3DAssetDock::on_slider_changed));
	}
	if (!_floating_btn->is_connected("pressed", callable_mp(this, &WorldScape3DAssetDock::make_dock_float))) {
		_floating_btn->connect("pressed", callable_mp(this, &WorldScape3DAssetDock::make_dock_float));
	}
	if (!_pinned_btn->is_connected("toggled", callable_mp(this, &WorldScape3DAssetDock::on_pin_changed))) {
		_pinned_btn->connect("toggled", callable_mp(this, &WorldScape3DAssetDock::on_pin_changed));
	}

	_pinned_btn->set_visible(_window != nullptr);
	auto toolbar = _plugin->get_ui()->get_toolbar();
	if (toolbar && !toolbar->is_connected("tool_changed", callable_mp(this, &WorldScape3DAssetDock::on_tool_changed))) {
		toolbar->connect("tool_changed", callable_mp(this, &WorldScape3DAssetDock::on_tool_changed));
	}

	update_thumbnails();
	_confirm_dialog = memnew(ConfirmationDialog);
	add_child(_confirm_dialog, true);
	_confirm_dialog->hide();
	_confirm_dialog->connect("confirmed", callable_mp(this, &WorldScape3DAssetDock::dialog_confirm));
	_confirm_dialog->connect("canceled", callable_mp(this, &WorldScape3DAssetDock::dialog_cancel));

	const auto scale = EditorInterface::get_singleton()->get_editor_scale();
	_meshes_btn->add_theme_font_size_override("font_size", 16 * scale);
	_textures_btn->add_theme_font_size_override("font_size", 16 * scale);

	update_dock();
	update_layout();

	// Setup styles
	set("theme_override_styles/panel", get_theme_stylebox("panel", "Panel"));
	// Avoid saving icon resources in tscn when editing w/ a tool script
	auto scene_root = EditorInterface::get_singleton()->get_edited_scene_root();
	if (scene_root != this) {
		_pinned_btn->set_button_icon(get_theme_icon("Pin", "EditorIcons"));
		_pinned_btn->set_text("");
		_floating_btn->set_button_icon(get_theme_icon("MakeFloating", "EditorIcons"));
		_floating_btn->set_text("");
	}
}

WorldScape3DAssetDock::WorldScape3DAssetDock(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin } {
	create_layout();
	_rex_editor_last_state = _plugin->get_rex_editor_window()->get_mode();

	// FIXME: this would leak?
	//_floating_btn->set_owner(nullptr);
	//_size_slider->set_owner(nullptr);

	_texture_list = memnew(ListContainer(_plugin));
	_texture_list->set_name("TextureList");
	_texture_list->set_type(WorldScape3DAssets::AssetType::TYPE_TEXTURE);
	_asset_container->add_child(_texture_list);

	_mesh_list = memnew(ListContainer(_plugin));
	_mesh_list->set_name("MeshList");
	_mesh_list->set_type(WorldScape3DAssets::AssetType::TYPE_MESH);
	_mesh_list->set_visible(false);
	_asset_container->add_child(_mesh_list);

	_current_list = _texture_list;

	load_editor_settings();

	_initialized = true;
}

WorldScape3DAssetDock::~WorldScape3DAssetDock() {
	_mesh_list->clear();
	_mesh_list->queue_free();
	_texture_list->clear();
	_texture_list->queue_free();

	_placement_opt->queue_free();
	_floating_btn->queue_free();
	_pinned_btn->queue_free();
	_size_slider->queue_free();
	_box->queue_free();
	_buttons->queue_free();
	_textures_btn->queue_free();
	_meshes_btn->queue_free();
	_asset_container->queue_free();
	_confirm_dialog->queue_free();
}

// Dock placement

void WorldScape3DAssetDock::set_slot(const int slot) {
	_slot = Math::clamp(_slot, POS_LEFT_UL, POS_BOTTOM);

	if (slot != static_cast<int>(_slot)) {
		_slot = static_cast<Slot>(slot);
		_placement_opt->select(_slot);
		save_editor_settings();
		_plugin->select_terrain();
		update_dock();
	}
}

void WorldScape3DAssetDock::remove_dock(const bool force) {
	switch (_state) {
		case State::SIDEBAR:
			_plugin->remove_control_from_docks(this);
			_state = State::HIDDEN;
			break;
		case State::BOTTOM:
			_plugin->remove_control_from_bottom_panel(this);
			_state = State::HIDDEN;
			break;
		case State::WINDOWED: {
			if (force && _window) {
				if (auto parent = get_parent(); parent) {
					parent->remove_child(this);
				}
				auto rex = _plugin->get_rex_editor_window();
				rex->disconnect("mouse_entered", callable_mp(this, &WorldScape3DAssetDock::on_rex_window_entered));
				rex->disconnect("focus_entered", callable_mp(this, &WorldScape3DAssetDock::on_rex_focus_entered));
				rex->disconnect("focus_exited", callable_mp(this, &WorldScape3DAssetDock::on_rex_focus_exited));
				_window->hide();
				_window->queue_free();
				_window = nullptr;
				_floating_btn->set_pressed(false);
				_floating_btn->set_visible(true);
				_pinned_btn->set_visible(false);
				_placement_opt->set_visible(true);
				_state = State::HIDDEN;
				update_dock(); // return dock to side/bottom
			}
			break;
		}
		case State::HIDDEN:
			break;
	}
}

void WorldScape3DAssetDock::update_dock() {
	if (_window || !_initialized) {
		return;
	}

	update_assets();

	// Move dock to new destination
	remove_dock();
	if (_slot < POS_BOTTOM) { // Sidebar
		_state = SIDEBAR;
		_plugin->add_control_to_dock(static_cast<EditorPlugin::DockSlot>(_slot), this);
	} else if (_slot == POS_BOTTOM) { // Bottom
		_state = BOTTOM;
		_plugin->add_control_to_bottom_panel(this, "Terrain Assets");
		_plugin->make_bottom_panel_item_visible(this);
	}
}

void WorldScape3DAssetDock::update_layout() {
	if (!_initialized) {
		return;
	}

	// Detect if we have a new window from Make floating, grab it so we can free it properly
	if (auto gp = get_grandparent(); !_window && gp) {
		_window = gp;
		make_dock_float();
		return; // Will call this function again upon display
	}

	auto size_parent = cast_to<Control>(_size_slider->get_parent());
	if (_window || _slot < POS_BOTTOM) { // Vertical layout in window / sidebar
		_box->set_vertical(true);
		_buttons->set_vertical(false);

		const auto size = get_size();
		if (size.x >= 500 && size_parent != _buttons) {
			_size_slider->reparent(_buttons);
			_buttons->move_child(_size_slider, 3);
		} else if (size.x < 500 && size_parent != _box) {
			_size_slider->reparent(_box);
			_box->move_child(_size_slider, 1);
		}
		_floating_btn->reparent(_buttons);
		_buttons->move_child(_floating_btn, 4);
	} else { // Wide layout on Bottom bar
		_size_slider->reparent(_buttons);
		_buttons->move_child(_size_slider, 3);
		_floating_btn->reparent(_box);
		_box->set_vertical(false);
		_buttons->set_vertical(true);
	}
	save_editor_settings();
}

void WorldScape3DAssetDock::update_thumbnails() {
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		return;
	}
	uint64_t delta_time = OS::get_singleton()->get_ticks_msec() - _last_thumb_update_time;
	if (_current_list->get_type() == WorldScape3DAssets::TYPE_MESH && delta_time > MAX_UPDATE_TIME) {
		terrain->get_assets()->create_mesh_thumbnails();
		_last_thumb_update_time = OS::get_singleton()->get_ticks_msec();
		for (auto mesh_asset : _mesh_list->get_entries()) {
			mesh_asset->queue_redraw();
		}
	}
}

// Dock Button handlers

void WorldScape3DAssetDock::on_pin_changed(bool toggled) {
	if (_window) {
		_window->set_flag(Window::FLAG_ALWAYS_ON_TOP, _pinned_btn->is_pressed());
	}
	save_editor_settings();
}

void WorldScape3DAssetDock::on_slider_changed(real_t value) {
	if (_texture_list) {
		_texture_list->set_entry_width(value);
	}
	if (_mesh_list) {
		_mesh_list->set_entry_width(value);
	}
	save_editor_settings();
}

void WorldScape3DAssetDock::on_textures_pressed() {
	_current_list = _texture_list;
	_texture_list->update_asset_list();
	_texture_list->set_visible(true);
	_mesh_list->set_visible(false);
	_textures_btn->set_pressed(true);
	_meshes_btn->set_pressed(false);
	_texture_list->set_selected_id(_texture_list->get_selected_id());
	auto terrain = _plugin->get_terrain();
	if (terrain) {
		EditorInterface::get_singleton()->edit_node(terrain);
	}
	save_editor_settings();
}

void WorldScape3DAssetDock::on_meshes_pressed() {
	_current_list = _mesh_list;
	_mesh_list->update_asset_list();
	_texture_list->set_visible(false);
	_mesh_list->set_visible(true);
	_textures_btn->set_pressed(false);
	_meshes_btn->set_pressed(true);
	_mesh_list->set_selected_id(_mesh_list->get_selected_id());
	auto terrain = _plugin->get_terrain();
	if (terrain) {
		EditorInterface::get_singleton()->edit_node(terrain);
	}
	update_thumbnails();
	save_editor_settings();
}

void WorldScape3DAssetDock::on_tool_changed(WorldScape3DEditor::Tool tool, WorldScape3DEditor::Operation operation) {
	//remove_all_highlights();
	switch (tool) {
		case WorldScape3DEditor::Tool::INSTANCER:
			on_meshes_pressed();
			break;
		case WorldScape3DEditor::Tool::TEXTURE:
		case WorldScape3DEditor::Tool::COLOR:
		case WorldScape3DEditor::Tool::ROUGHNESS:
			on_textures_pressed();
			break;
		default:
			break;
	}
}

//

void WorldScape3DAssetDock::update_assets() {
	if (!_initialized) {
		return;
	}

	// Verify signals to individual lists
	auto terrain = _plugin->get_terrain();
	if (terrain && terrain->get_assets().is_valid()) {
		const auto assets = terrain->get_assets();
		if (!assets->is_connected("textures_changed", callable_mp(_texture_list, &ListContainer::update_asset_list))) {
			assets->connect("textures_changed", callable_mp(_texture_list, &ListContainer::update_asset_list));
		}
		if (!assets->is_connected("meshes_changed", callable_mp(_mesh_list, &ListContainer::update_asset_list))) {
			assets->connect("meshes_changed", callable_mp(_mesh_list, &ListContainer::update_asset_list));
		}
	}
	_current_list->update_asset_list();
}

void WorldScape3DAssetDock::remove_all_highlights() {
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		return;
	}
	for (int i = 0; i < _mesh_list->get_entries().size(); ++i) {
		if (Ref<WorldScape3DMeshAsset> resource = _mesh_list->get_entries()[i]->get_resource(); resource.is_valid()) {
			//resource->set_highlighted(false); // FIXME
		}
	}
}

// Window Management

void WorldScape3DAssetDock::make_dock_float() {
	// If not already created (eg from editor panel 'Make Floating' button)
	if (!_window) {
		remove_dock();
		create_window();
	}

	_state = WINDOWED;
	set_visible(true); // Asset dock contents are hidden when popping out of the bottom!
	_pinned_btn->set_visible(true);
	_floating_btn->set_visible(false);
	_placement_opt->set_visible(false);
	_window->set_title("Terrain Assets");
	_window->set_flag(Window::FLAG_ALWAYS_ON_TOP, _pinned_btn->is_pressed());
	_window->connect("close_requested", callable_mp(this, &WorldScape3DAssetDock::remove_dock).bind(true));
	_window->connect("window_input", callable_mp(this, &WorldScape3DAssetDock::on_window_input));
	_window->connect("focus_exited", callable_mp(this, &WorldScape3DAssetDock::save_editor_settings));
	_window->connect("mouse_exited", callable_mp(this, &WorldScape3DAssetDock::save_editor_settings));
	_window->connect("size_changed", callable_mp(this, &WorldScape3DAssetDock::save_editor_settings));
	const auto editor_window = _plugin->get_rex_editor_window();
	editor_window->connect("mouse_entered", callable_mp(this, &WorldScape3DAssetDock::on_rex_window_entered));
	editor_window->connect("focus_entered", callable_mp(this, &WorldScape3DAssetDock::on_rex_focus_entered));
	editor_window->connect("focus_exited", callable_mp(this, &WorldScape3DAssetDock::on_rex_focus_exited));
	editor_window->grab_focus();
	update_assets();
	save_editor_settings();
}

void WorldScape3DAssetDock::create_window() {
	_window = memnew(Window);
	_window->set_wrap_controls(true);
	auto mc = memnew(MarginContainer);
	mc->set_anchors_preset(PRESET_FULL_RECT, false);
	mc->add_child(this, true);
	_window->add_child(mc, true);
	_window->set_transient(false);
	_window->set_size(_plugin->get_setting(ES_DOCK_WINDOW_SIZE, Vector2{ 512, 512 }));
	_window->set_position(_plugin->get_setting(ES_DOCK_WINDOW_POSITION, Vector2i{ 704, 284 }));
	_plugin->add_child(_window, true);
	_window->show();
}

void WorldScape3DAssetDock::clamp_window_position() {
	if (_window && _window->is_visible()) {
		Vector2i bounds;
		auto editor_iface = EditorInterface::get_singleton();
		if (editor_iface && editor_iface->get_editor_settings()->get_setting("interface/editor/single_window_mode")) {
			bounds = editor_iface->get_base_control()->get_size();
		} else {
			auto server = DisplayServer::get_singleton();
			bounds = server->screen_get_position(_window->get_current_screen());
			bounds += server->screen_get_size(_window->get_current_screen());
		}
		static constexpr int margin = 40;
		auto pos = _window->get_position();
		const auto size = _window->get_size();
		pos.x = CLAMP(pos.x, -size.x + 2 * margin, bounds.x - margin);
		pos.y = CLAMP(pos.y, 25, bounds.y - margin);
		_window->set_position(pos);
	}
}

void WorldScape3DAssetDock::on_window_input(Ref<InputEvent> event) {
	// Capture CTRL+S when doc focused to save scene
	Ref<InputEventKey> key_event = event;
	if (key_event.is_valid() && key_event->is_command_or_control_pressed() && key_event->is_pressed() && key_event->get_keycode() == Key::S) {
		save_editor_settings();
		EditorInterface::get_singleton()->save_scene();
	}
}

void WorldScape3DAssetDock::on_rex_window_entered() {
	if (_window && _window->has_focus()) {
		_plugin->get_rex_editor_window()->grab_focus();
	}
}

void WorldScape3DAssetDock::on_rex_focus_entered() {
	// If asset dock is windowed, and ReX was minimized, and now is not, restore asset dock window
	if (_window) {
		auto editor_window = _plugin->get_rex_editor_window();
		if (_rex_editor_last_state == Window::MODE_MINIMIZED && editor_window->get_mode() != Window::MODE_MINIMIZED) {
			_window->show();
			_rex_editor_last_state = editor_window->get_mode();
			editor_window->grab_focus();
		}
	}
}

void WorldScape3DAssetDock::on_rex_focus_exited() {
	if (auto editor_window = _plugin->get_rex_editor_window(); _window && editor_window->get_mode() == Window::MODE_MINIMIZED) {
		_window->hide();
		_rex_editor_last_state = editor_window->get_mode();
	}
}

// Manage Editor Settings

void WorldScape3DAssetDock::load_editor_settings() {
	_floating_btn->set_pressed(_plugin->get_setting(ES_DOCK_FLOATING, false));
	_pinned_btn->set_pressed(_plugin->get_setting(ES_DOCK_PINNED, true));
	_size_slider->set_value(_plugin->get_setting(ES_DOCK_TILE_SIZE, 90.f));
	on_slider_changed(_size_slider->get_value());
	set_slot(_plugin->get_setting(ES_DOCK_SLOT, POS_BOTTOM));
	if (_floating_btn->is_pressed()) {
		make_dock_float();
	}
	// TODO Don't load tab until thumbnail generation is more reliable
	// if (static_cast<int>(_plugin->get_setting(ES_DOCK_TAB, 0)) == 1) {
	// 	on_meshes_pressed();
	// }
}

void WorldScape3DAssetDock::save_editor_settings() {
	if (!_initialized) {
		return;
	}
	clamp_window_position();
	_plugin->set_setting(ES_DOCK_SLOT, _slot);
	_plugin->set_setting(ES_DOCK_TILE_SIZE, _size_slider->get_value());
	_plugin->set_setting(ES_DOCK_FLOATING, _floating_btn->is_pressed());
	_plugin->set_setting(ES_DOCK_PINNED, _pinned_btn->is_pressed());
	// TODO Don't save tab until thumbnail generation is more reliable
	//_plugin->set_setting(ES_DOCK_TAB, _current_list == _texture_list ? 0 : 1);
	if (_window) {
		_plugin->set_setting(ES_DOCK_WINDOW_SIZE, _window->get_size());
		_plugin->set_setting(ES_DOCK_WINDOW_POSITION, _window->get_position());
	}
}

void WorldScape3DAssetDock::_bind_methods() {
	ADD_SIGNAL(MethodInfo("confirmation_closed"));
	ADD_SIGNAL(MethodInfo("confirmation_confirmed"));
	ADD_SIGNAL(MethodInfo("confirmation_canceled"));
}

void WorldScape3DAssetDock::_notification(int what) {
	if (what == NOTIFICATION_POSTINITIALIZE) {
		init();
	}
}

void WorldScape3DAssetDock::dialog_confirm() {
	_confirmed = true;
	emit_signal("confirmation_closed");
	emit_signal("confirmation_confirmed");
}

void WorldScape3DAssetDock::dialog_cancel() {
	_confirmed = false;
	emit_signal("confirmation_closed");
	emit_signal("confirmation_canceled");
}
