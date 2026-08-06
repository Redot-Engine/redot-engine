/**************************************************************************/
/*  directory_setup.cpp                                                   */
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

#include "directory_setup.h"

#include "editor/editor_interface.h"
#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/margin_container.h"

#include "modules/worldscape_3d/worldscape_3d.h"

void DirectorySetupDialog::setup_layout() {
	set_transient(false);
	set_exclusive(false);

	_margin = memnew(MarginContainer);
	_margin->set_name("Margin");
	_margin->set_offset(Side::SIDE_LEFT, 0.0);
	_margin->set_offset(Side::SIDE_TOP, 8.0);
	_margin->set_offset(Side::SIDE_RIGHT, 742.0);
	_margin->set_offset(Side::SIDE_BOTTOM, 281.0);
	_margin->add_theme_constant_override("theme_override_constants/margin_left", 20);
	_margin->add_theme_constant_override("theme_override_constants/margin_top", 20);
	_margin->add_theme_constant_override("theme_override_constants/margin_right", 20);
	_margin->add_theme_constant_override("theme_override_constants/margin_bottom", 20);
	add_child(_margin);

	_vbox = memnew(VBoxContainer);
	_vbox->set_name("VBox");
	_margin->add_child(_vbox);

	_instructions = memnew(Label);
	_instructions->set_name("Instructions");
	_instructions->set_custom_minimum_size(Size2{ 400, 0 });
	_instructions->set_text("WorldScape3D data is stored in a directory instead of a single file. Each region is stored in a separate file named `terrain[-_]##[-_]##.res`. For instance, the region at location (-1, 1) would be named `terrain-01_01.res`. Enable WorldScape3D / Debug / Show Region Labels for a visual display.");
	_instructions->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	_vbox->add_child(_instructions);

	_dir_label = memnew(Label);
	_dir_label->set_name("DirectoryLabel");
	_dir_label->set_custom_minimum_size(Size2{ 400, 0 });
	_dir_label->set_text("Specify the directory to store your data. Any existing region files will be loaded.");
	_dir_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	_vbox->add_child(_dir_label);

	_dir_hbox = memnew(HBoxContainer);
	_dir_hbox->set_name("DirHBox");
	_vbox->add_child(_dir_hbox);

	_select_dir_le = memnew(LineEdit);
	_select_dir_le->set_name("LineEdit");
	_select_dir_le->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	_dir_hbox->add_child(_select_dir_le);

	_select_dir_btn = memnew(Button);
	_select_dir_btn->set_name("SelectDir");
	_dir_hbox->add_child(_select_dir_btn);

	_spacer = memnew(Control);
	_spacer->set_name("Spacer");
	_spacer->set_custom_minimum_size(Size2{ 0, 40 });
	_vbox->add_child(_spacer);
}

DirectorySetupDialog::DirectorySetupDialog() {
	set_name("DirectorySetup");
	set_title("Terrain Data Directory Setup");
	set_position(Point2i{ 0, 36 });
	set_size(Size2i{ 750, 330 });
	setup_layout();
	ConfirmationDialog::set_visible(true);
}

DirectorySetupDialog::~DirectorySetupDialog() {
	_spacer->queue_free();
	_select_dir_btn->queue_free();
	_select_dir_le->queue_free();
	_dir_hbox->queue_free();
	_dir_label->queue_free();
	_instructions->queue_free();
	_vbox->queue_free();
	_margin->queue_free();
}

void DirectorySetup::setup_layout() {
	_file_dialog->set_filters({ "*.res" });
	_file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	_file_dialog->set_access(EditorFileDialog::ACCESS_RESOURCES);
	_file_dialog->set_ok_button_text("Open");
	_file_dialog->set_title("Open a folder or file");
	_file_dialog->set_size(Size2i{ 850, 550 });
	_file_dialog->set_transient(false);
	_file_dialog->set_exclusive(false);
	_file_dialog->set_flag(Window::Flags::FLAG_POPUP, true);
	_file_dialog->connect("dir_selected", callable_mp(this, &DirectorySetup::on_dir_selected));
	add_child(_file_dialog);

	_dialog->get_dir_button()->connect("pressed", callable_mp(this, &DirectorySetup::on_select_file_pressed).bind(EditorFileDialog::FILE_MODE_OPEN_DIR));
	_dialog->connect("confirmed", callable_mp(this, &DirectorySetup::on_close_dialog));
	_dialog->connect("canceled", callable_mp(this, &DirectorySetup::on_close_dialog));
	_dialog->get_ok_button()->connect("pressed", callable_mp(this, &DirectorySetup::on_ok));

	auto folder_icon = EditorInterface::get_singleton()->get_base_control()->get_theme_icon("Folder", "EditorIcons");
	_dialog->get_dir_button()->set_button_icon(folder_icon);
}

DirectorySetup::DirectorySetup(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin }, _dialog{ memnew(DirectorySetupDialog) }, _file_dialog{ memnew(EditorFileDialog) } {
	_dialog->hide();
	setup_layout();
}

DirectorySetup::~DirectorySetup() {
	_file_dialog->queue_free();
	_dialog->queue_free();
}

void DirectorySetup::directory_setup_popup() {
	auto current_data_directory = _plugin->get_terrain()->get_data_directory();
	if (!current_data_directory.is_empty()) {
		_dialog->get_dir_edit()->set_text(current_data_directory);
	}

	_dialog->set_visible(true);

	// popup
	EditorInterface::get_singleton()->popup_dialog_centered(_dialog);
}

void DirectorySetup::on_select_file_pressed(EditorFileDialog::FileMode mode) {
	_file_dialog->set_file_mode(mode);
	_file_dialog->set_visible(true);
	_file_dialog->popup_centered();
}

void DirectorySetup::on_dir_selected(const String &path) {
	_dialog->get_dir_edit()->set_text(path);
}

void DirectorySetup::on_close_dialog() {
	_dialog->hide();
}

void DirectorySetup::on_ok() {
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		print_error("Not connected terrain. Click the WorldScape3D node first");
		return;
	}

	auto selected_dir = _dialog->get_dir_edit()->get_text();

	if (selected_dir.is_empty()) {
		print_error("No data directory specified.");
		return;
	}

	if (!DirAccess::exists(selected_dir)) {
		print_error("Directory doesn't exist");
		return;
	}

	// TODO Check for existing files??

	print_line("Setting terrain directory ", selected_dir);
	terrain->set_data_directory(selected_dir);
}
