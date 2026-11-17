/**************************************************************************/
/*  project_converter_plugin.h                                            */
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

#ifdef TOOLS_ENABLED

#include "editor/gui/editor_file_dialog.h"
#include "editor/plugins/editor_plugin.h"

// Single umbrella EditorPlugin that hosts all cross-engine project converters:
//   Scratch · GDevelop · Source/S&BOX · Unreal
// (Unity converters are registered via editor/plugins/unity_importer_plugin.h)
// Each converter type gets a tool-menu entry that opens a file dialog and
// writes the converted output to res://imported/<engine>/.

class ProjectConverterPlugin : public EditorPlugin {
	GDCLASS(ProjectConverterPlugin, EditorPlugin);

	// Per-engine file dialogs (created lazily in NOTIFICATION_ENTER_TREE).
	EditorFileDialog *scratch_dialog = nullptr;
	EditorFileDialog *gdevelop_dialog = nullptr;
	EditorFileDialog *source_vmf_dialog = nullptr;
	EditorFileDialog *source_vmt_dialog = nullptr;
	EditorFileDialog *source_smd_dialog = nullptr;
	EditorFileDialog *unreal_t3d_dialog = nullptr;
	EditorFileDialog *unreal_uproject_dialog = nullptr;

	// Tool-menu callbacks – each shows the matching file dialog.
	void _show_scratch_dialog();
	void _show_gdevelop_dialog();
	void _show_source_vmf_dialog();
	void _show_source_vmt_dialog();
	void _show_source_smd_dialog();
	void _show_unreal_t3d_dialog();
	void _show_unreal_uproject_dialog();

	// File-selected callbacks – called by the dialogs when the user picks a file.
	void _on_scratch_file_selected(const String &p_path);
	void _on_gdevelop_file_selected(const String &p_path);
	void _on_source_vmf_file_selected(const String &p_path);
	void _on_source_vmt_file_selected(const String &p_path);
	void _on_source_smd_file_selected(const String &p_path);
	void _on_unreal_t3d_file_selected(const String &p_path);
	void _on_unreal_uproject_file_selected(const String &p_path);

protected:
	void _notification(int p_what);
	static void _bind_methods() {}

public:
	ProjectConverterPlugin() = default;
	~ProjectConverterPlugin() override = default;
};

#endif // TOOLS_ENABLED
