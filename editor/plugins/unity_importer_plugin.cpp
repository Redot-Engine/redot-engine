/**************************************************************************/
/*  unity_importer_plugin.cpp                                             */
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

#include "unity_importer_plugin.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_toaster.h"
#include "scene/gui/progress_bar.h"

static Error ensure_dir(const String &p_res_path) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d.is_null()) {
		return ERR_CANT_CREATE;
	}
	Vector<String> parts = String(p_res_path).replace("res://", "").split("/");
	String path = "res://";
	for (int i = 0; i < parts.size(); i++) {
		if (parts[i].is_empty()) {
			continue;
		}
		path += parts[i];
		if (!d->dir_exists(path)) {
			Error mk = d->make_dir(path);
			if (mk != OK && mk != ERR_ALREADY_EXISTS) {
				return mk;
			}
		}
		path += "/";
	}
	return OK;
}

static Error copy_dir_recursive(const String &p_src_res, const String &p_dst_res) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d.is_null()) {
		return ERR_CANT_OPEN;
	}
	if (!d->dir_exists(p_src_res)) {
		return ERR_DOES_NOT_EXIST;
	}
	Error err = ensure_dir(p_dst_res);
	if (err != OK) {
		return err;
	}

	List<String> stack;
	stack.push_back(p_src_res);

	while (!stack.is_empty()) {
		String cur_src = stack.front()->get();
		stack.pop_front();
		String rel = cur_src.trim_prefix(p_src_res);
		String cur_dst = p_dst_res + rel;
		if (!d->dir_exists(cur_dst)) {
			Error mk = d->make_dir(cur_dst);
			if (mk != OK && mk != ERR_ALREADY_EXISTS) {
				return mk;
			}
		}

		Ref<DirAccess> sub = DirAccess::open(cur_src);
		if (sub.is_null()) {
			return ERR_CANT_OPEN;
		}
		sub->list_dir_begin();
		String name = sub->get_next();
		while (!name.is_empty()) {
			if (name == "." || name == "..") {
				name = sub->get_next();
				continue;
			}
			String src_path = cur_src.path_join(name);
			String dst_path = cur_dst.path_join(name);
			if (sub->current_is_dir()) {
				stack.push_back(src_path);
			} else {
				PackedByteArray buf = FileAccess::get_file_as_bytes(src_path);
				Ref<FileAccess> f = FileAccess::open(dst_path, FileAccess::WRITE);
				if (f.is_null()) {
					sub->list_dir_end();
					return ERR_CANT_CREATE;
				}
				f->store_buffer(buf);
			}
			name = sub->get_next();
		}
		sub->list_dir_end();
	}
	return OK;
}

void UnityImporterPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_import_unity_packages"), &UnityImporterPlugin::_import_unity_packages);
	ClassDB::bind_method(D_METHOD("_install_unity_to_godot"), &UnityImporterPlugin::_install_unity_to_godot);
	ClassDB::bind_method(D_METHOD("_install_shaderlab2godotsl"), &UnityImporterPlugin::_install_shaderlab2godotsl);
	ClassDB::bind_method(D_METHOD("_convert_unity_shader"), &UnityImporterPlugin::_convert_unity_shader);
	ClassDB::bind_method(D_METHOD("_file_selected"), &UnityImporterPlugin::_file_selected);
	ClassDB::bind_method(D_METHOD("_handle_shader_file"), &UnityImporterPlugin::_handle_shader_file);
}

void UnityImporterPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		add_tool_menu_item(TTR("Import Unity Package..."), callable_mp(this, &UnityImporterPlugin::_import_unity_packages));
		add_tool_menu_item(TTR("Install UnityToGodot Toolkit..."), callable_mp(this, &UnityImporterPlugin::_install_unity_to_godot));
		add_tool_menu_item(TTR("Install Shaderlab2GodotSL..."), callable_mp(this, &UnityImporterPlugin::_install_shaderlab2godotsl));
		add_tool_menu_item(TTR("Convert Unity Shader..."), callable_mp(this, &UnityImporterPlugin::_convert_unity_shader));
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_tool_menu_item(TTR("Import Unity Package..."));
		remove_tool_menu_item(TTR("Install UnityToGodot Toolkit..."));
		remove_tool_menu_item(TTR("Install Shaderlab2GodotSL..."));
		remove_tool_menu_item(TTR("Convert Unity Shader..."));
	}
}

void UnityImporterPlugin::_import_unity_packages() {
	_show_package_dialog();
}

void UnityImporterPlugin::_show_package_dialog() {
	if (!package_dialog) {
		package_dialog = memnew(EditorFileDialog);
		package_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		package_dialog->clear_filters();
		package_dialog->add_filter("*.unitypackage", TTR("Unity Package"));
		package_dialog->set_title(TTR("Select Unity Package"));
		package_dialog->connect("file_selected", callable_mp(this, &UnityImporterPlugin::_file_selected));
		EditorNode::get_singleton()->get_gui_base()->add_child(package_dialog);
	}
	package_dialog->popup_file_dialog();
}

Error UnityImporterPlugin::_parse_unity_package(const String &p_path) {
	parsed_assets.clear();
	print_line("Parsing Unity package: " + p_path);
	Error result = UnityPackageParser::parse_unitypackage(p_path, parsed_assets);
	if (result == OK) {
		print_line(vformat("Successfully parsed Unity package with %d assets", parsed_assets.size()));
	} else {
		print_error("Failed to parse Unity package: " + error_names[result]);
	}
	return result;
}

void UnityImporterPlugin::_file_selected(const String &p_path) {
	current_package_path = p_path;
	Error err = _parse_unity_package(p_path);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Failed to parse Unity package: %s"), error_names[err]), EditorToaster::SEVERITY_ERROR);
		return;
	}
	_import_assets();
}

void UnityImporterPlugin::_import_assets() {
	int imported = 0;
	int skipped = 0;
	int failed = 0;
	String error_log;

	auto process_pass = [&](bool p_textures_only) {
		for (const KeyValue<String, UnityAsset> &E : parsed_assets) {
			String ext = E.value.pathname.get_extension().to_lower();
			bool is_texture = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff");
			if (p_textures_only != is_texture) {
				continue;
			}

			Error err = UnityAssetConverter::extract_asset(E.value, parsed_assets);
			switch (err) {
				case OK:
					imported++;
					break;
				case ERR_SKIP:
					skipped++;
					break;
				default:
					failed++;
					String error_msg = vformat("Failed to import '%s' (extension: .%s): %s", E.value.pathname, ext, error_names[err]);
					print_error(error_msg);
					if (!error_log.is_empty()) {
						error_log += "\n";
					}
					error_log += error_msg;
					break;
			}
		}
	};

	process_pass(true); // Ensure textures are available before materials and models look them up.
	process_pass(false);

	String summary = vformat(TTR("Unity package import finished: %d imported, %d skipped, %d failed"), imported, skipped, failed);
	if (!error_log.is_empty()) {
		print_line("Unity import errors:\n" + error_log);
	}
	EditorToaster::Severity sev = failed > 0 ? EditorToaster::SEVERITY_WARNING : EditorToaster::SEVERITY_INFO;
	EditorToaster::get_singleton()->popup_str(summary, sev);
}

void UnityImporterPlugin::_install_unity_to_godot() {
	const String src_dir = "res://addons/_unity_bundled/UnityToGodot";
	const String dst_dir = "res://addons/UnityToGodot";
	Error err = copy_dir_recursive(src_dir, dst_dir);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Bundled UnityToGodot toolkit not found. Populate addons/_unity_bundled/UnityToGodot inside the editor install."), EditorToaster::SEVERITY_WARNING);
		return;
	}
	EditorToaster::get_singleton()->popup_str(TTR("UnityToGodot toolkit installed locally under res://addons/UnityToGodot."));
}

void UnityImporterPlugin::_install_shaderlab2godotsl() {
	EditorToaster::get_singleton()->popup_str(TTR("Shader converter is built-in. Use Tools > Convert Unity Shader."));
}

void UnityImporterPlugin::_convert_unity_shader() {
	if (!shader_dialog) {
		shader_dialog = memnew(EditorFileDialog);
		shader_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		shader_dialog->clear_filters();
		shader_dialog->add_filter("*.shader", TTR("Unity Shader"));
		shader_dialog->set_title(TTR("Select Unity Shader"));
		shader_dialog->connect("file_selected", callable_mp(this, &UnityImporterPlugin::_handle_shader_file));
		EditorNode::get_singleton()->get_gui_base()->add_child(shader_dialog);
	}
	shader_dialog->popup_file_dialog();
}

void UnityImporterPlugin::_handle_shader_file(const String &p_path) {
	Error file_read_err = OK;
	String shader_code = FileAccess::get_file_as_string(p_path, &file_read_err);
	if (file_read_err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to read shader file."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	String godot_shader;
	Error err = UnityShaderConverter::convert_shaderlab_to_godot(shader_code, godot_shader);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to convert shader."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	String output_path = p_path.get_basename() + ".gdshader";
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE);
	if (f.is_null()) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to save converted shader."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	f->store_string(godot_shader);
	EditorToaster::get_singleton()->popup_str(vformat(TTR("Shader converted successfully: %s"), output_path), EditorToaster::SEVERITY_INFO);
}
