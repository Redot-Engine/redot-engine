/**************************************************************************/
/*  directory_setup.h                                                     */
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

#include "editor/gui/editor_file_dialog.h"
#include "scene/gui/dialogs.h"
#include "scene/main/node.h"

#include "modules/worldscape_3d/editor/worldscape_3d_editor.h"

class Button;
class ConfirmationDialog;
class EditorFileDialog;
class HBoxContainer;
class Label;
class LineEdit;
class MarginContainer;
class VBoxContainer;

class DirectorySetupDialog final : public ConfirmationDialog {
	GDCLASS(DirectorySetupDialog, ConfirmationDialog);

	MarginContainer *_margin = nullptr;
	VBoxContainer *_vbox = nullptr;
	Label *_instructions = nullptr;
	Label *_dir_label = nullptr;
	HBoxContainer *_dir_hbox = nullptr;
	LineEdit *_select_dir_le = nullptr;
	Button *_select_dir_btn = nullptr;
	Control *_spacer = nullptr;

	void setup_layout();

public:
	DirectorySetupDialog();
	~DirectorySetupDialog() override;

	Button *get_dir_button() const { return _select_dir_btn; }
	LineEdit *get_dir_edit() const { return _select_dir_le; }
};

class DirectorySetup final : public Node {
	GDCLASS(DirectorySetup, Node);

	WorldScape3DEditorPlugin *_plugin = nullptr;
	DirectorySetupDialog *_dialog = nullptr;
	EditorFileDialog *_file_dialog = nullptr;

	void setup_layout();

public:
	explicit DirectorySetup(WorldScape3DEditorPlugin *plugin);
	~DirectorySetup() override;

	void directory_setup_popup();

	void on_select_file_pressed(EditorFileDialog::FileMode mode);
	void on_dir_selected(const String &path);
	void on_close_dialog();
	void on_ok();
};
