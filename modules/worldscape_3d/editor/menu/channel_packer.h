/**************************************************************************/
/*  channel_packer.h                                                      */
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

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/window.h"

class OptionButton;
class Button;
class LineEdit;
class MarginContainer;
class PanelContainer;
class EditorFileDialog;
class WorldScape3DEditorPlugin;
class ChannelSelector;
class ChannelPanel;
class SpinBox;

class ChannelPackerDialog : public AcceptDialog {
	GDCLASS(ChannelPackerDialog, AcceptDialog);

	enum Index {
		IMAGE_ALBEDO,
		IMAGE_HEIGHT,
		IMAGE_NORMAL,
		IMAGE_ROUGHNESS
	};

	static int albedo_file_index;
	static int normal_file_index;

	MarginContainer *_margin = nullptr;
	VBoxContainer *_vbox = nullptr;
	HBoxContainer *_top_hbox = nullptr;
	HBoxContainer *_bottom_hbox = nullptr;

	ChannelPanel *_albedo_panel = nullptr;
	ChannelPanel *_height_panel = nullptr;
	ChannelSelector *_height_channel_selector = nullptr;
	ChannelPanel *_normal_panel = nullptr;
	ChannelPanel *_roughness_panel = nullptr;
	ChannelSelector *_roughness_channel_selector = nullptr;
	PanelContainer *_general_options_panel = nullptr;

	Button *_height_from_luminance_btn = nullptr;
	CheckBox *_depth_to_height = nullptr;
	CheckBox *_norm_height = nullptr;
	CheckBox *_directx_to_opengl = nullptr;
	CheckBox *_orthogonalize_normals = nullptr;
	CheckBox *_smoothness_to_roughness = nullptr;

	CheckBox *use_height_as_roughness_checkbox = nullptr;
	OptionButton *channel_selection_option = nullptr;
	CheckBox *_resize_packed = nullptr;
	SpinBox *_resize_box = nullptr;
	CheckBox *_gen_mipmaps = nullptr;
	CheckBox *_high_quality = nullptr;
	Button *pack_albedo_button = nullptr;
	Button *pack_normal_button = nullptr;

	EditorFileDialog *_save_file_dialog = nullptr;
	String _last_saved_directory;

	Vector3 _normal_vector;

	void setup_layout();
	void init_file_dialog();
	void init();

	void create_general_options_panel();

	void on_resize_checked();

	void generate_height_from_luminance();

	void create_import_file(const String &png_path) const;

	void set_normal_vector(Ref<Image> image, bool quiet = false);
	void align_normals(Ref<Image> image, int iteration = 0);

	Error pack_textures(
			Ref<Image> rgb_img, Ref<Image> a_img,
			const String &path, bool invert_gree,
			bool invert_smooth, bool align_normal,
			bool normalize_height, int alpha_channel);

public:
	ChannelPackerDialog();
	~ChannelPackerDialog() override;

	void _notification(int what);

	void on_pack_albedo_pressed();
	void on_pack_normal_pressed();

	void on_save_albedo_selected(const String &path);
	void on_save_normal_selected(const String &path);

	void on_resources_reimported(PackedStringArray const &resources);
};
