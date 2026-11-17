/**************************************************************************/
/*  project_converter_plugin.cpp                                          */
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

#include "project_converter_plugin.h"

#ifdef TOOLS_ENABLED

#include "gdevelop_converter.h"
#include "scratch_converter.h"
#include "source_converter.h"
#include "unreal_converter.h"

#include "core/config/project_settings.h"
#include "core/string/print_string.h"
#include "editor/editor_node.h"

// ---------------------------------------------------------------------------
// Helper: make sure res://imported/<sub>/ exists and return its filesystem path.
// ---------------------------------------------------------------------------
static String _output_dir(const String &p_sub) {
	String res_dir = "res://imported/" + p_sub;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (da.is_valid()) {
		da->make_dir_recursive(res_dir.replace("res://", ""));
	}
	// Return absolute filesystem path for the converters which use ACCESS_FILESYSTEM.
	String abs = ProjectSettings::get_singleton()->globalize_path(res_dir);
	return abs;
}

// ---------------------------------------------------------------------------
// _notification
// ---------------------------------------------------------------------------
void ProjectConverterPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// --- Scratch ---
			add_tool_menu_item(TTR("Import Scratch Project (.sb3)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_scratch_dialog));

			scratch_dialog = memnew(EditorFileDialog);
			scratch_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			scratch_dialog->set_title(TTR("Open Scratch Project (.sb3)"));
			scratch_dialog->add_filter("*.sb3", TTR("Scratch 3 Project"));
			scratch_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_scratch_file_selected));
			EditorNode::get_singleton()->add_child(scratch_dialog);

			// --- GDevelop ---
			add_tool_menu_item(TTR("Import GDevelop Project (.gdg/.json)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_gdevelop_dialog));

			gdevelop_dialog = memnew(EditorFileDialog);
			gdevelop_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			gdevelop_dialog->set_title(TTR("Open GDevelop Project"));
			gdevelop_dialog->add_filter("*.gdg", TTR("GDevelop Game File"));
			gdevelop_dialog->add_filter("*.json", TTR("GDevelop JSON Project"));
			gdevelop_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_gdevelop_file_selected));
			EditorNode::get_singleton()->add_child(gdevelop_dialog);

			// --- Source / S&BOX – VMF ---
			add_tool_menu_item(TTR("Import Source Engine Map (.vmf)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_source_vmf_dialog));

			source_vmf_dialog = memnew(EditorFileDialog);
			source_vmf_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			source_vmf_dialog->set_title(TTR("Open Source Engine Map (.vmf)"));
			source_vmf_dialog->add_filter("*.vmf", TTR("Valve Map Format"));
			source_vmf_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_source_vmf_file_selected));
			EditorNode::get_singleton()->add_child(source_vmf_dialog);

			// --- Source / S&BOX – VMT ---
			add_tool_menu_item(TTR("Import Source Engine Material (.vmt/.vmat)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_source_vmt_dialog));

			source_vmt_dialog = memnew(EditorFileDialog);
			source_vmt_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			source_vmt_dialog->set_title(TTR("Open Source Engine Material"));
			source_vmt_dialog->add_filter("*.vmt", TTR("Valve Material Type"));
			source_vmt_dialog->add_filter("*.vmat", TTR("S&BOX Material"));
			source_vmt_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_source_vmt_file_selected));
			EditorNode::get_singleton()->add_child(source_vmt_dialog);

			// --- Source / S&BOX – SMD ---
			add_tool_menu_item(TTR("Import Source Engine Mesh (.smd)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_source_smd_dialog));

			source_smd_dialog = memnew(EditorFileDialog);
			source_smd_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			source_smd_dialog->set_title(TTR("Open Source Engine Mesh (.smd)"));
			source_smd_dialog->add_filter("*.smd", TTR("Valve Studiomodel Data"));
			source_smd_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_source_smd_file_selected));
			EditorNode::get_singleton()->add_child(source_smd_dialog);

			// --- Unreal – T3D ---
			add_tool_menu_item(TTR("Import Unreal Level (.t3d)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_unreal_t3d_dialog));

			unreal_t3d_dialog = memnew(EditorFileDialog);
			unreal_t3d_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			unreal_t3d_dialog->set_title(TTR("Open Unreal Engine Level (.t3d)"));
			unreal_t3d_dialog->add_filter("*.t3d", TTR("Unreal Text 3D"));
			unreal_t3d_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_unreal_t3d_file_selected));
			EditorNode::get_singleton()->add_child(unreal_t3d_dialog);

			// --- Unreal – .uproject ---
			add_tool_menu_item(TTR("Import Unreal Project (.uproject)..."),
					callable_mp(this, &ProjectConverterPlugin::_show_unreal_uproject_dialog));

			unreal_uproject_dialog = memnew(EditorFileDialog);
			unreal_uproject_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			unreal_uproject_dialog->set_title(TTR("Open Unreal Project (.uproject)"));
			unreal_uproject_dialog->add_filter("*.uproject", TTR("Unreal Project"));
			unreal_uproject_dialog->connect("file_selected",
					callable_mp(this, &ProjectConverterPlugin::_on_unreal_uproject_file_selected));
			EditorNode::get_singleton()->add_child(unreal_uproject_dialog);

			print_line("Project Converter: Scratch, GDevelop, Source/S&BOX, Unreal importers ready.");
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_tool_menu_item(TTR("Import Scratch Project (.sb3)..."));
			remove_tool_menu_item(TTR("Import GDevelop Project (.gdg/.json)..."));
			remove_tool_menu_item(TTR("Import Source Engine Map (.vmf)..."));
			remove_tool_menu_item(TTR("Import Source Engine Material (.vmt/.vmat)..."));
			remove_tool_menu_item(TTR("Import Source Engine Mesh (.smd)..."));
			remove_tool_menu_item(TTR("Import Unreal Level (.t3d)..."));
			remove_tool_menu_item(TTR("Import Unreal Project (.uproject)..."));
		} break;
	}
}

// ---------------------------------------------------------------------------
// Show-dialog methods
// ---------------------------------------------------------------------------
void ProjectConverterPlugin::_show_scratch_dialog() {
	scratch_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_gdevelop_dialog() {
	gdevelop_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_source_vmf_dialog() {
	source_vmf_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_source_vmt_dialog() {
	source_vmt_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_source_smd_dialog() {
	source_smd_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_unreal_t3d_dialog() {
	unreal_t3d_dialog->popup_file_dialog();
}
void ProjectConverterPlugin::_show_unreal_uproject_dialog() {
	unreal_uproject_dialog->popup_file_dialog();
}

// ---------------------------------------------------------------------------
// File-selected callbacks
// ---------------------------------------------------------------------------
void ProjectConverterPlugin::_on_scratch_file_selected(const String &p_path) {
	String out = _output_dir("scratch");
	Error err = ScratchConverter::convert(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("Scratch project imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("Scratch import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_gdevelop_file_selected(const String &p_path) {
	String out = _output_dir("gdevelop");
	Error err = GDevelopConverter::convert(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("GDevelop project imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("GDevelop import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_source_vmf_file_selected(const String &p_path) {
	String out = _output_dir("source/maps");
	Error err = SourceConverter::convert_vmf(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("VMF map imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("VMF import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_source_vmt_file_selected(const String &p_path) {
	String out = _output_dir("source/materials");
	Error err = SourceConverter::convert_vmt(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("VMT/VMAT material imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("VMT import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_source_smd_file_selected(const String &p_path) {
	String out = _output_dir("source/meshes");
	Error err = SourceConverter::convert_smd(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("SMD mesh imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("SMD import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_unreal_t3d_file_selected(const String &p_path) {
	String out = _output_dir("unreal/levels");
	Error err = UnrealConverter::convert_t3d(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("Unreal level imported successfully.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("Unreal T3D import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

void ProjectConverterPlugin::_on_unreal_uproject_file_selected(const String &p_path) {
	String out = _output_dir("unreal/project");
	Error err = UnrealConverter::convert_uproject(p_path, out);
	if (err == OK) {
		EditorNode::get_singleton()->show_warning(
				TTR("Unreal project info imported.\nOutput: ") + out);
	} else {
		EditorNode::get_singleton()->show_warning(
				TTR("Unreal project import failed (error ") + itos(err) + TTR("). Check the Output panel for details."));
	}
}

#endif // TOOLS_ENABLED
