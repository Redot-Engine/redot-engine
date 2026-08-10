/**************************************************************************/
/*  channel_packer.cpp                                                    */
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

#include "channel_packer.h"

#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_about.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/settings/project_settings_editor.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"

#include "modules/worldscape_3d/worldscape_3d_util.h"

namespace {

auto constexpr import_template =
		R"_(
[remap]

importer="texture"
type="CompressedTexture2D"
metadata={
"imported_formats": ["s3tc_bptc"],
"vram_texture": true
}

[deps]

source_file="$SOURCE_FILE"

[params]

compress/mode=2
compress/high_quality=$HIGH_QUALITY
compress/lossy_quality=0.7
compress/hdr_compression=1
compress/normal_map=2
compress/channel_pack=0
mipmaps/generate=$GENERATE_MIPMAPS
mipmaps/limit=-1
roughness/mode=0
roughness/src_normal=""
process/fix_alpha_border=true
process/premult_alpha=false
process/normal_map_invert_y=false
process/hdr_as_srgb=false
process/hdr_clamp_exposure=false
process/size_limit=0
detect_3d/compress_to=1
)_";

Basis alignment_basis(Vector3 normal) {
	static constexpr Vector3 up{ .0, .0, 1. };
	auto v = normal.cross(up);
	auto c = normal.dot(up);
	auto k = 1.f / (1.f + c);

	auto vxy = v.x * v.y * k;
	auto vxz = v.x * v.z * k;
	auto vyz = v.y * v.z * k;

	return Basis{
		Vector3{ v.x * v.x * k + c, vxy - v.z, vxz + v.y },
		Vector3{ vxy + v.z, v.y * v.y * k + c, vyz - v.x },
		Vector3{ vxz - v.y, vyz + v.x, v.z * v.z * k + c }
	};
}

} //namespace

class ChannelSelector final : public HBoxContainer {
	GDCLASS(ChannelSelector, HBoxContainer);

	Label *_label = nullptr;
	OptionButton *_option = nullptr;

public:
	ChannelSelector() {
		set_v_size_flags(SIZE_EXPAND_FILL);

		_label = memnew(Label);
		_label->set_text("Source Channel:");
		add_child(_label);

		// TODO use buttons instead? This is much simpler
		_option = memnew(OptionButton);
		_option->add_item("Red");
		_option->add_item("Green");
		_option->add_item("Blue");
		_option->add_item("Alpha");
		add_child(_option);
	}

	~ChannelSelector() override {
		_option->queue_free();
		_label->queue_free();
	}

	void set_channels(int used_channels) {
		int channel_count = 4;
		switch (used_channels) {
			case Image::USED_CHANNELS_L:
			case Image::USED_CHANNELS_R:
				channel_count = 1;
				break;
			case Image::USED_CHANNELS_LA:
			case Image::USED_CHANNELS_RG:
				channel_count = 2;
				break;
			case Image::USED_CHANNELS_RGB:
				channel_count = 3;
				break;
			case Image::USED_CHANNELS_RGBA:
				channel_count = 4;
				break;
		}
		for (int i = 0; i < 4; ++i) {
			_option->set_item_disabled(i, i > channel_count);
		}
		_option->select(0);
	}

	int get_selected_channel() const {
		if (!_option) {
			return -1;
		}
		return _option->get_selected_id();
	}
};

class TexturePackerButton final : public Button {
	GDCLASS(TexturePackerButton, Button);

	~TexturePackerButton() override = default;

public:
	static void _bind_methods() {
		ADD_SIGNAL(MethodInfo("dropped",
				PropertyInfo(Variant::STRING, "path", PROPERTY_HINT_NONE)));
	}

	bool can_drop_data(const Point2 &, const Variant &p_dropped_data) const override {
		if (p_dropped_data.get_type() == Variant::DICTIONARY) {
			PackedStringArray files = Dictionary{ p_dropped_data }["files"];
			if (files.size() == 1) {
				auto ext = files[0].get_extension();
				return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "exr" || ext == "hdr" || ext == "tga" || ext == "svg" || ext == "webp" || ext == "ktx" || ext == "dds";
			}
		}
		return false;
	}

	void drop_data(const Point2 &, const Variant &p_dropped_data) override {
		if (p_dropped_data.get_type() == Variant::DICTIONARY) {
			PackedStringArray files = Dictionary{ p_dropped_data }["files"];
			if (files.size() == 1) {
				auto ext = files[0].get_extension();
				if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "exr" || ext == "hdr" || ext == "tga" || ext == "svg" || ext == "webp" || ext == "ktx" || ext == "dds") {
					emit_signal(SNAME("dropped"), files[0]);
				}
			}
		}
	}
};

class ChannelPanel final : public PanelContainer {
	GDCLASS(ChannelPanel, PanelContainer);

	String _name;

	MarginContainer *_outer_margin = nullptr;
	HBoxContainer *_outer_hbox = nullptr;
	VBoxContainer *_vbox = nullptr;
	VBoxContainer *_vbox2 = nullptr;
	Label *_label = nullptr;
	HBoxContainer *_hbox = nullptr;
	LineEdit *_path_edit = nullptr;
	MarginContainer *_margin = nullptr;
	Button *_pick_btn = nullptr;
	Button *_clear_btn = nullptr;
	Panel *_preview_panel = nullptr;
	TextureRect *_preview = nullptr;
	TexturePackerButton *_texture_button = nullptr;
	HBoxContainer *_wh_hbox = nullptr;
	Label *_width_label = nullptr;
	Label *_height_label = nullptr;

	Ref<Image> _image = nullptr;

	EditorFileDialog *_open_file_dialog = nullptr;
	String _last_opened_dir;

	void setup_layout() {
		set_h_size_flags(SIZE_EXPAND_FILL);
		set_v_size_flags(SIZE_EXPAND_FILL);

		Ref panel_style = memnew(StyleBoxFlat);
		panel_style->set_bg_color(Color{ 0.168627, 0.211765, 0.266667, 1 });
		panel_style->set_border_width(Side::SIDE_LEFT, 3);
		panel_style->set_border_width(Side::SIDE_TOP, 3);
		panel_style->set_border_width(Side::SIDE_RIGHT, 3);
		panel_style->set_border_width(Side::SIDE_BOTTOM, 3);
		panel_style->set_border_color(Color{ 0.270588, 0.435294, 0.580392, 1 });
		panel_style->set_corner_radius(Corner::CORNER_TOP_LEFT, 5);
		panel_style->set_corner_radius(Corner::CORNER_TOP_RIGHT, 5);
		panel_style->set_corner_radius(Corner::CORNER_BOTTOM_RIGHT, 5);
		panel_style->set_corner_radius(Corner::CORNER_BOTTOM_LEFT, 5);
		add_theme_style_override("theme_override_styles/panel", panel_style);

		_outer_margin = memnew(MarginContainer);
		_outer_margin->set_name("MarginContainer");
		_outer_margin->add_theme_constant_override("theme_override_constants/margin_left", 10);
		_outer_margin->add_theme_constant_override("theme_override_constants/margin_top", 10);
		_outer_margin->add_theme_constant_override("theme_override_constants/margin_right", 10);
		_outer_margin->add_theme_constant_override("theme_override_constants/margin_bottom", 10);
		add_child(_outer_margin);

		_outer_hbox = memnew(HBoxContainer);
		_outer_hbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		_outer_margin->add_child(_outer_hbox);

		_vbox = memnew(VBoxContainer);
		_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_outer_hbox->add_child(_vbox);

		_label = memnew(Label);
		_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
		_label->set_text(_name + " texture");
		_vbox->add_child(_label);

		_hbox = memnew(HBoxContainer);
		_vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		_vbox->add_child(_hbox);

		_path_edit = memnew(LineEdit);
		_path_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_hbox->add_child(_path_edit);

		_pick_btn = memnew(Button);
		_hbox->add_child(_pick_btn);

		_clear_btn = memnew(Button);
		_hbox->add_child(_clear_btn);

		_margin = memnew(MarginContainer);
		_margin->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		_margin->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		_margin->add_theme_constant_override("theme_override_constants/margin_top", 10);
		_vbox->add_child(_margin);

		_preview_panel = memnew(Panel);
		_preview_panel->set_custom_minimum_size(Size2{ 110, 110 });
		_preview_panel->set_h_size_flags(SizeFlags::SIZE_SHRINK_CENTER);
		_preview_panel->set_v_size_flags(SizeFlags::SIZE_SHRINK_CENTER);
		Ref preview_style = memnew(StyleBoxFlat);
		preview_style->set_bg_color(Color{ 0.137255, 0.137255, 0.137255, 1 });
		preview_style->set_border_width(Side::SIDE_LEFT, 3);
		preview_style->set_border_width(Side::SIDE_TOP, 3);
		preview_style->set_border_width(Side::SIDE_RIGHT, 3);
		preview_style->set_border_width(Side::SIDE_BOTTOM, 3);
		preview_style->set_border_color(Color{ 0.784314, 0.784314, 0.784314, 1 });
		preview_style->set_corner_radius(Corner::CORNER_TOP_LEFT, 5);
		preview_style->set_corner_radius(Corner::CORNER_TOP_RIGHT, 5);
		preview_style->set_corner_radius(Corner::CORNER_BOTTOM_RIGHT, 5);
		preview_style->set_corner_radius(Corner::CORNER_BOTTOM_LEFT, 5);
		_preview_panel->add_theme_style_override("theme_override_styles/panel", preview_style);
		_margin->add_child(_preview_panel);

		_texture_button = memnew(TexturePackerButton);
		_texture_button->set_anchors_preset(LayoutPreset::PRESET_FULL_RECT);
		_texture_button->set_anchor(Side::SIDE_RIGHT, 1.f);
		_texture_button->set_anchor(Side::SIDE_BOTTOM, 1.f);
		_texture_button->set_h_grow_direction(GrowDirection::GROW_DIRECTION_END);
		_texture_button->set_v_grow_direction(GrowDirection::GROW_DIRECTION_END);
		Ref empty_style = memnew(StyleBoxFlat);
		_texture_button->add_theme_style_override("theme_override_styles/normal", empty_style);
		_preview_panel->add_child(_texture_button);

		_preview = memnew(TextureRect);
		_preview->set_anchors_preset(LayoutPreset::PRESET_CENTER);
		_preview->set_anchor(Side::SIDE_LEFT, 0.5f);
		_preview->set_anchor(Side::SIDE_TOP, 0.5f);
		_preview->set_anchor(Side::SIDE_RIGHT, 0.5f);
		_preview->set_anchor(Side::SIDE_BOTTOM, 0.5f);
		_preview->set_offset(Side::SIDE_LEFT, -50.0);
		_preview->set_offset(Side::SIDE_TOP, -50.0);
		_preview->set_offset(Side::SIDE_RIGHT, 50.0);
		_preview->set_offset(Side::SIDE_BOTTOM, 50.0);
		_preview->set_h_grow_direction(GrowDirection::GROW_DIRECTION_BOTH);
		_preview->set_v_grow_direction(GrowDirection::GROW_DIRECTION_BOTH);
		_preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		_texture_button->add_child(_preview);

		_wh_hbox = memnew(HBoxContainer);
		_wh_hbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		_vbox->add_child(_wh_hbox);

		_width_label = memnew(Label);
		_height_label = memnew(Label);
		_wh_hbox->add_child(_width_label);
		_wh_hbox->add_child(_height_label);

		_vbox2 = memnew(VBoxContainer);
		_vbox2->set_alignment(BoxContainer::ALIGNMENT_CENTER);
		_vbox2->set_h_size_flags(SIZE_EXPAND_FILL);
		_vbox->add_child(_vbox2);
	}

	void init_file_dialog() {
		if (_open_file_dialog) {
			return;
		}

		_open_file_dialog = memnew(EditorFileDialog);
		_open_file_dialog->set_filters(
				{ "*.png", "*.bmp", "*.exr", "*.hdr", "*.jpg", "*.jpeg", "*.tga", "*.svg", "*.webp", "*.ktx", "*.dds" });
		_open_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		_open_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
		_open_file_dialog->set_ok_button_text("Open");
		_open_file_dialog->set_size(Size2i{ 550, 550 });
		_open_file_dialog->set_transient(false);
		_open_file_dialog->set_exclusive(false);
		_open_file_dialog->set_flag(Window::FLAG_POPUP, true);
		_open_file_dialog->connect("file_selected", callable_mp(this, &ChannelPanel::on_file_selected));

		add_child(_open_file_dialog);
	}

	void init() {
		init_file_dialog();
		if (_pick_btn) {
			_pick_btn->set_button_icon(get_theme_icon("Folder", "EditorIcons"));
			if (!_pick_btn->is_connected("pressed", callable_mp(this, &ChannelPanel::open))) {
				_pick_btn->connect("pressed", callable_mp(this, &ChannelPanel::open));
			}
		}
		if (_clear_btn) {
			_clear_btn->set_button_icon(get_theme_icon("Remove", "EditorIcons"));
			if (!_clear_btn->is_connected("pressed", callable_mp(this, &ChannelPanel::clear))) {
				_clear_btn->connect("pressed", callable_mp(this, &ChannelPanel::clear));
			}
		}
		if (_path_edit && !_path_edit->is_connected("text_submitted", callable_mp(this, &ChannelPanel::on_path_edited))) {
			_path_edit->connect("text_submitted", callable_mp(this, &ChannelPanel::on_path_edited));
		}
		if (_texture_button) {
			if (!_texture_button->is_connected("pressed", callable_mp(this, &ChannelPanel::open))) {
				_texture_button->connect("pressed", callable_mp(this, &ChannelPanel::open));
			}
			if (!_texture_button->is_connected("dropped", callable_mp(this, &ChannelPanel::on_rex_drop))) {
				_texture_button->connect("dropped", callable_mp(this, &ChannelPanel::on_rex_drop));
			}
		}
	}

	void on_path_edited(const String &path) {
		_path_edit->set_text(path);
		load_image(path);
	}

	void on_file_selected(const String &path) {
		_last_opened_dir = _open_file_dialog->get_current_dir();
		on_path_edited(path);
	}

	void on_rex_drop(const String &path) {
		auto gpath = ProjectSettings::get_singleton()->globalize_path(path);
		on_path_edited(gpath);
	}

	void open() {
		_open_file_dialog->set_current_path(_last_opened_dir);
		_open_file_dialog->popup_centered_ratio();
	}

public:
	explicit ChannelPanel(const String &channel_name) :
			_name{ channel_name } {
		setup_layout();
	}

	~ChannelPanel() override {
		_height_label->queue_free();
		_width_label->queue_free();
		_wh_hbox->queue_free();
		_margin->queue_free();
		_texture_button->queue_free();
		_preview->queue_free();
		_preview_panel->queue_free();
		_pick_btn->queue_free();
		_path_edit->queue_free();
		_label->queue_free();
		_outer_hbox->queue_free();
		_vbox->queue_free();
	}

	void add(Control *control) {
		if (_vbox2) {
			_vbox2->add_child(control);
		}
	}

	void set_path_edit(const String &path) {
		_path_edit->set_text(path);
	}

	void set_image(Ref<Image> image, ChannelSelector *selector = nullptr) {
		_image = image;
		if (_image.is_valid()) {
			_preview->set_texture(ImageTexture::create_from_image(_image));
			set_wh_labels(image->get_width(), image->get_height());
			if (selector) {
				selector->set_channels(_image->detect_used_channels());
			}
		} else {
			_preview->set_texture(nullptr);
		}
	}

	void load_image(String const &path, ChannelSelector *selector = nullptr) {
		_image.instantiate();
		int error = 0;
		// Special case for DDS files
		if (path.get_extension() == "dds") {
			_image = ResourceLoader::load(path);
			if (_image.is_valid()) {
				// if the dds file is loaded, we must clear any mipmaps and
				// decompress if needed in order to do per pixel operations.
				_image->clear_mipmaps();
				_image->decompress();
			} else {
				error = 1;
			}
		} else {
			error = _image->load(path);
		}

		if (error != OK) {
			EditorNode::get_singleton()->show_warning("Failed to load texture \'" + path + "\'");
			_preview->set_texture(nullptr);
		} else {
			_preview->set_texture(ImageTexture::create_from_image(_image));
			set_wh_labels(_image->get_width(), _image->get_height());

			if (selector) {
				selector->set_channels(_image->detect_used_channels());
			}

			__print_line("Loaded texture \'" + path + "\'");
		}
	}

	void set_wh_labels(const int width, const int height) {
		String w;
		String h;
		if (width > 0 && height > 0) {
			w = "w: " + String::num_int64(width);
			h = "h: " + String::num_int64(height);
		}
		_width_label->set_text(w);
		_height_label->set_text(h);
	}

	void clear() {
		_path_edit->set_text("");
		set_image(nullptr);
		set_wh_labels(-1, -1);
	}

	Ref<Image> get_image() const {
		return _image;
	}

	void _notification(const int what) {
		if (what == NOTIFICATION_ENTER_TREE) {
			init();
		}
	}
};

int ChannelPackerDialog::albedo_file_index = 0;
int ChannelPackerDialog::normal_file_index = 0;

void ChannelPackerDialog::setup_layout() {
	set_title("Terrain Channel Packer");
	set_size(Vector2(600, 400));

	_margin = memnew(MarginContainer);
	_margin->set_name("MarginContainer");
	_margin->add_theme_constant_override("theme_override_constants/margin_left", 5);
	_margin->add_theme_constant_override("theme_override_constants/margin_top", 5);
	_margin->add_theme_constant_override("theme_override_constants/margin_right", 5);
	_margin->add_theme_constant_override("theme_override_constants/margin_bottom", 5);
	add_child(_margin);

	_vbox = memnew(VBoxContainer);
	_vbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	_vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	add_child(_vbox);

	_top_hbox = memnew(HBoxContainer);
	_top_hbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_albedo_panel = memnew(ChannelPanel("Albedo"));
	_height_from_luminance_btn = memnew(Button);
	_height_from_luminance_btn->set_text("Generate Height from Luminance");
	_albedo_panel->add(_height_from_luminance_btn);
	_height_from_luminance_btn->connect("pressed", callable_mp(this, &ChannelPackerDialog::generate_height_from_luminance));
	auto spacer = memnew(Control);
	spacer->set_custom_minimum_size(Size2{ 120, 60 });
	_albedo_panel->add(spacer);
	_height_panel = memnew(ChannelPanel("Height"));
	_depth_to_height = memnew(CheckBox);
	_depth_to_height->set_text("Convert Depth to Height");
	_height_panel->add(_depth_to_height);
	_norm_height = memnew(CheckBox);
	_norm_height->set_text("Normalize Height");
	_height_panel->add(_norm_height);
	_height_channel_selector = memnew(ChannelSelector);
	_height_panel->add(_height_channel_selector);
	_top_hbox->add_child(_albedo_panel);
	_top_hbox->add_child(_height_panel);
	_vbox->add_child(_top_hbox);

	pack_albedo_button = memnew(Button);
	pack_albedo_button->set_text("Pack albedo and height");
	_vbox->add_child(pack_albedo_button);
	pack_albedo_button->connect("pressed", callable_mp(this, &ChannelPackerDialog::on_pack_albedo_pressed));

	_bottom_hbox = memnew(HBoxContainer);
	_bottom_hbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_normal_panel = memnew(ChannelPanel("Normal"));
	_directx_to_opengl = memnew(CheckBox);
	_directx_to_opengl->set_text("Convert DirectX to OpenGL");
	_normal_panel->add(_directx_to_opengl);
	_orthogonalize_normals = memnew(CheckBox);
	_orthogonalize_normals->set_text("Orthogonalize Normals");
	_normal_panel->add(_orthogonalize_normals);
	_roughness_panel = memnew(ChannelPanel("Roughness"));
	_smoothness_to_roughness = memnew(CheckBox);
	_smoothness_to_roughness->set_text("Convert Smoothness To Roughness");
	_roughness_panel->add(_smoothness_to_roughness);
	_roughness_channel_selector = memnew(ChannelSelector);
	_roughness_panel->add(_roughness_channel_selector);
	_bottom_hbox->add_child(_normal_panel);
	_bottom_hbox->add_child(_roughness_panel);
	_vbox->add_child(_bottom_hbox);

	pack_normal_button = memnew(Button);
	pack_normal_button->set_text("Pack normal and roughness");
	_vbox->add_child(pack_normal_button);
	pack_normal_button->connect("pressed", callable_mp(this, &ChannelPackerDialog::on_pack_normal_pressed));

	create_general_options_panel();
	_vbox->add_child(_general_options_panel);
}

void ChannelPackerDialog::init_file_dialog() {
	_last_saved_directory = "res://";

	_save_file_dialog = memnew(EditorFileDialog);
	_save_file_dialog->set_filters({ "*.png" });
	_save_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	_save_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	_save_file_dialog->set_ok_button_text("Save");
	_save_file_dialog->set_size(Size2i{ 550, 550 });
	_save_file_dialog->set_transient(false);
	_save_file_dialog->set_exclusive(false);
	_save_file_dialog->set_flag(Window::FLAG_POPUP, true);

	_margin->add_child(_save_file_dialog);
}

void ChannelPackerDialog::init() {
}

void ChannelPackerDialog::generate_height_from_luminance() {
	if (_albedo_panel->get_image().is_null()) {
		EditorNode::get_singleton()->show_warning("Albedo Image Required for Operation");
		return;
	}

	auto height_texture = WorldScape3DUtil::luminance_to_height(_albedo_panel->get_image());
	if (height_texture->is_empty()) {
		EditorNode::get_singleton()->show_warning("Height Texture Generation error");
		return;
	}
	// blur the image by resizing down and back...
	int w = height_texture->get_width();
	int h = height_texture->get_height();
	height_texture->resize(w / 4, h / 4);
	height_texture->resize(w, h, Image::INTERPOLATE_CUBIC);
	// "Load" the height texture...
	_height_panel->set_image(height_texture, _height_channel_selector);
	_height_panel->set_path_edit("Generated Height");
	__print_line("Height Texture generated successfully");
}

void ChannelPackerDialog::create_import_file(const String &png_path) const {
	auto dst_import_path = png_path + ".import";
	auto template_content = String{ import_template };
	auto import_content = template_content
								  .replace("$SOURCE_FILE", png_path)
								  .replace("$HIGH_QUALITY", (_high_quality->is_pressed() ? "true" : "false"))
								  .replace("$GENERATE_MIPMAPS", (_gen_mipmaps->is_pressed() ? "true" : "false"));
	auto file = FileAccess::open(dst_import_path, FileAccess::WRITE);
	file->store_string(import_content);
	file->close();
}

ChannelPackerDialog::ChannelPackerDialog() {
	set_ok_button_text("Close");
	setup_layout();
	init_file_dialog();
}

ChannelPackerDialog::~ChannelPackerDialog() {
	_general_options_panel->queue_free();
	_roughness_panel->queue_free();
	_normal_panel->queue_free();
	_bottom_hbox->queue_free();
	_height_panel->queue_free();
	_albedo_panel->queue_free();
	_top_hbox->queue_free();
	_vbox->queue_free();
	_margin->queue_free();
}

void ChannelPackerDialog::_notification(const int what) {
	if (what == NOTIFICATION_POSTINITIALIZE) {
		init();
	}
}

void ChannelPackerDialog::create_general_options_panel() {
	_general_options_panel = memnew(PanelContainer);
	VBoxContainer *vbox = memnew(VBoxContainer);
	vbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	_general_options_panel->add_child(vbox);

	Label *label = memnew(Label);
	label->set_text("General options");
	label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	vbox->add_child(label);

	HBoxContainer *hbox = memnew(HBoxContainer);
	hbox->set_alignment(BoxContainer::ALIGNMENT_CENTER);
	vbox->add_child(hbox);

	_resize_packed = memnew(CheckBox);
	_resize_packed->set_text("Resize Packed Image");
	hbox->add_child(_resize_packed);
	_resize_packed->connect("pressed", callable_mp(this, &ChannelPackerDialog::on_resize_checked));
	_resize_box = memnew(SpinBox);
	_resize_box->set_min(0);
	_resize_box->set_max(128);
	_resize_box->set_value(128);
	hbox->add_child(_resize_box);
	_resize_box->hide();
	_gen_mipmaps = memnew(CheckBox);
	_gen_mipmaps->set_text("Generate Mipmaps");
	hbox->add_child(_gen_mipmaps);
	_high_quality = memnew(CheckBox);
	_high_quality->set_text("Import High Quality");
	hbox->add_child(_high_quality);
}

void ChannelPackerDialog::on_resize_checked() {
	_resize_box->set_visible(_resize_packed->is_pressed());
}

void ChannelPackerDialog::on_pack_albedo_pressed() {
	if (_albedo_panel->get_image().is_null() || _height_panel->get_image().is_null()) {
		EditorNode::get_singleton()->show_warning("Please select an albedo and height texture");
		return;
	}
	static const String default_path = _last_saved_directory + "/packed_albedo_height";
	String path = albedo_file_index > 0 ? default_path + "_" + String::num_int64(albedo_file_index) : default_path;
	while (FileAccess::exists(path + ".png")) {
		path = default_path + "_" + String::num_int64(++albedo_file_index);
	}
	_save_file_dialog->set_current_path(path);
	_save_file_dialog->set_title("Save Packed Albedo/Height Texture");
	if (_save_file_dialog->is_connected("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_normal_selected))) {
		_save_file_dialog->disconnect("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_normal_selected));
	}
	if (!_save_file_dialog->is_connected("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_albedo_selected))) {
		_save_file_dialog->connect("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_albedo_selected));
	}
	_save_file_dialog->popup_centered_ratio();
}

void ChannelPackerDialog::on_pack_normal_pressed() {
	if (_normal_panel->get_image().is_null() || _roughness_panel->get_image().is_null()) {
		EditorNode::get_singleton()->show_warning("Please select a normal and roughness texture");
		return;
	}
	static const String default_path = _last_saved_directory + "/packed_normal_roughness";
	String path = normal_file_index > 0 ? default_path + "_" + String::num_int64(normal_file_index) : default_path;
	while (FileAccess::exists(path + ".png")) {
		path = default_path + "_" + String::num_int64(++normal_file_index);
	};
	_save_file_dialog->set_current_path(path);
	_save_file_dialog->set_title("Save Packed Normal/Roughness Texture");
	if (_save_file_dialog->is_connected("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_albedo_selected))) {
		_save_file_dialog->disconnect("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_albedo_selected));
	}
	if (!_save_file_dialog->is_connected("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_normal_selected))) {
		_save_file_dialog->connect("file_selected", callable_mp(this, &ChannelPackerDialog::on_save_normal_selected));
	}
	_save_file_dialog->popup_centered_ratio();
}

void ChannelPackerDialog::on_save_albedo_selected(const String &path) {
	if (_save_file_dialog->get_current_file().find("packed_albedo_height") == 0) {
		++albedo_file_index;
	}
	_last_saved_directory = _save_file_dialog->get_current_dir();
	Error error = pack_textures(_albedo_panel->get_image(), _height_panel->get_image(),
			path, false, _depth_to_height->is_pressed(), false, _norm_height->is_pressed(),
			_height_channel_selector->get_selected_channel());

	if (error == OK) {
		const auto editor_fs = EditorFileSystem::get_singleton();
		if (!editor_fs->is_connected("resources_reimported", callable_mp(this, &ChannelPackerDialog::on_resources_reimported))) {
			editor_fs->connect("resources_reimported", callable_mp(this, &ChannelPackerDialog::on_resources_reimported));
		}
		if (is_visible()) {
			hide();
		}
		editor_fs->scan();
	}
}

void ChannelPackerDialog::on_save_normal_selected(const String &path) {
	if (_save_file_dialog->get_current_file().find("packed_normal_roughness") == 0) {
		++normal_file_index;
	}
	_last_saved_directory = _save_file_dialog->get_current_dir();
	Error error = pack_textures(_normal_panel->get_image(), _roughness_panel->get_image(),
			path, _directx_to_opengl->is_pressed(), _smoothness_to_roughness->is_pressed(),
			_orthogonalize_normals->is_pressed(), false,
			_roughness_channel_selector->get_selected_channel());

	if (error == OK) {
		const auto editor_fs = EditorFileSystem::get_singleton();
		if (!editor_fs->is_connected("resources_reimported", callable_mp(this, &ChannelPackerDialog::on_resources_reimported))) {
			editor_fs->connect("resources_reimported", callable_mp(this, &ChannelPackerDialog::on_resources_reimported));
		}
		if (is_visible()) {
			hide();
		}
		editor_fs->scan();
	}
}

void ChannelPackerDialog::on_resources_reimported(PackedStringArray const &) {
	show();
	call_deferred("grab_focus");
}

void ChannelPackerDialog::set_normal_vector(Ref<Image> image, bool quiet) {
	// Calculate texture normal sum direction
	auto normal = image;
	Color sum{ .0, .0, .0, .0 };
	for (int x = 0; x < normal->get_width(); ++x) {
		for (int y = 0; y < normal->get_height(); ++y) {
			sum += normal->get_pixel(x, y);
		}
	}
	real_t div = normal->get_width() * normal->get_height();
	sum += Color{ div, div, div, div };
	sum *= 2.f;
	sum -= Color{ 1., 1., 1. };
	_normal_vector = Vector3{ sum.r, sum.g, sum.b }.normalized();
	if (_normal_vector.dot(Vector3{ .0, .0, 1. }) < 0.999 && !quiet) {
		EditorNode::get_singleton()->show_warning("Normal Texture Not Orthogonal to UV plane.\nFor Compatibility with Detiling and Rotation, Select Orthogonalize Normals");
	}
}

void ChannelPackerDialog::align_normals(Ref<Image> image, int iteration) {
	// generate matrix to re-align the normalmap
	auto mat3 = alignment_basis(_normal_vector);
	// re-align the normal map pixels
	for (int x = 0; x < image->get_width(); ++x) {
		for (int y = 0; y < image->get_height(); ++y) {
			auto old_pixel = image->get_pixel(x, y);
			Vector3 vector_pixel{ old_pixel.r, old_pixel.g, old_pixel.b };
			vector_pixel *= 2.f;
			vector_pixel -= Vector3{ 1.f, 1.f, 1.f };
			vector_pixel.normalize();
			vector_pixel = mat3.xform(vector_pixel);
			vector_pixel += Vector3{ 1.f, 1.f, 1.f };
			vector_pixel *= 0.5f;
			const Color new_pixel{ vector_pixel.x, vector_pixel.y, vector_pixel.z, old_pixel.a };
			image->set_pixel(x, y, new_pixel);
		}
	}
	set_normal_vector(image, true);
	if (_normal_vector.dot(Vector3{ 0.f, 0.f, 1.f }) < 0.999 && iteration < 3) {
		++iteration;
		align_normals(image, iteration);
	}
}

Error ChannelPackerDialog::pack_textures(
		Ref<Image> rgb_img, Ref<Image> a_img, const String &path,
		bool invert_green, bool invert_smooth, bool align_the_normals,
		bool normalize_height, int alpha_channel) {
	if (rgb_img.is_valid() && a_img.is_valid()) {
		Error error = OK;

		if (rgb_img->get_size() != a_img->get_size() && !_resize_packed->is_pressed()) {
			EditorNode::get_singleton()->show_warning("Textures must be the same size.\nEnable resize to override image dimensions");
			return FAILED;
		}

		if (_resize_packed->is_pressed()) {
			auto sz = std::max(128., _resize_box->get_value());
			rgb_img->resize(sz, sz, Image::INTERPOLATE_CUBIC);
			a_img->resize(sz, sz, Image::INTERPOLATE_CUBIC);
		}

		if (align_the_normals && _normal_vector.dot(Vector3{ .0, .0, 1.0 }) < 0.999) {
			align_normals(rgb_img);
		} else if (align_the_normals) {
			EditorNode::get_singleton()->show_warning("Alignment OK, skipping Normal Orthogonalization");
		}

		auto output_image = WorldScape3DUtil::pack_image(
				rgb_img, a_img, invert_green, invert_smooth, normalize_height, alpha_channel);

		if (output_image.is_null()) {
			EditorNode::get_singleton()->show_warning("Failed to pack textures");
			return FAILED;
		}

		if (output_image->detect_alpha() != Image::ALPHA_BLEND) {
			EditorNode::get_singleton()->show_warning("Warning, Alpha channel empty");
		}

		error = output_image->save_png(path);
		if (error != OK) {
			EditorNode::get_singleton()->show_warning("Failed to save texture");
			return error;
		}
		create_import_file(path);
		__print_line_rich("Packed to " + path + ".");
		return OK;
	} else {
		EditorNode::get_singleton()->show_warning("Failed to load one or more textures");
		return FAILED;
	}
}
