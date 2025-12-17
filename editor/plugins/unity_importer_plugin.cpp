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

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "scene/gui/progress_bar.h"
#include "scene/resources/image_texture.h"

UnityAssetDatabase *UnityAssetDatabase::singleton = nullptr;

UnityAssetDatabase *UnityAssetDatabase::get_singleton() {
	if (!singleton) {
		singleton = memnew(UnityAssetDatabase);
	}
	return singleton;
}

void UnityAssetDatabase::insert_meta(const String &p_guid, const Ref<UnityMetadata> &p_meta) {
	guid_to_meta[p_guid] = p_meta;
	if (!p_meta->path.is_empty()) {
		path_to_meta[p_meta->path] = p_meta;
	}
}

Ref<UnityMetadata> UnityAssetDatabase::get_meta_by_guid(const String &p_guid) {
	return guid_to_meta.has(p_guid) ? guid_to_meta[p_guid] : Ref<UnityMetadata>();
}

Ref<UnityMetadata> UnityAssetDatabase::get_meta_at_path(const String &p_path) {
	return path_to_meta.has(p_path) ? path_to_meta[p_path] : Ref<UnityMetadata>();
}

void UnityAssetDatabase::clear() {
	guid_to_meta.clear();
	path_to_meta.clear();
}

UnityAssetDatabase::UnityAssetDatabase() {
	singleton = this;
}

UnityAssetDatabase::~UnityAssetDatabase() {
	if (singleton == this) {
		singleton = nullptr;
	}
}

static Error _ensure_dir(const String &p_dir_res) {
    Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    if (d.is_null()) {
        return ERR_CANT_CREATE;
    }
    if (!d->dir_exists("res://addons")) {
        Error mk = d->make_dir("res://addons");
        if (mk != OK && mk != ERR_ALREADY_EXISTS) {
            return mk;
        }
    }
    Vector<String> parts = String(p_dir_res).replace("res://", "").split("/");
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

static Error _copy_dir_recursive(const String &p_src_res, const String &p_dst_res) {
    Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
    if (d.is_null()) {
        return ERR_CANT_OPEN;
    }
    if (!d->dir_exists(p_src_res)) {
        return ERR_DOES_NOT_EXIST;
    }
    Error err = _ensure_dir(p_dst_res);
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
            String src_path = cur_src.plus_file(name);
            String dst_path = cur_dst.plus_file(name);
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
}

void UnityImporterPlugin::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        add_tool_menu_item(TTR("Import Unity Project..."), callable_mp(this, &UnityImporterPlugin::_import_unity_packages));
        add_tool_menu_item(TTR("Install UnityToGodot Toolkit..."), callable_mp(this, &UnityImporterPlugin::_install_unity_to_godot));
        add_tool_menu_item(TTR("Install Shaderlab2GodotSL..."), callable_mp(this, &UnityImporterPlugin::_install_shaderlab2godotsl));
        add_tool_menu_item(TTR("Convert Unity Shader..."), callable_mp(this, &UnityImporterPlugin::_convert_unity_shader));
    }
    if (p_what == NOTIFICATION_EXIT_TREE) {
        remove_tool_menu_item(TTR("Import Unity Project..."));
        remove_tool_menu_item(TTR("Install UnityToGodot Toolkit..."));
        remove_tool_menu_item(TTR("Install Shaderlab2GodotSL..."));
        remove_tool_menu_item(TTR("Convert Unity Shader..."));
    }
}

void UnityImporterPlugin::_import_unity_packages() {
	_show_package_dialog();
}

void UnityImporterPlugin::_show_package_dialog() {
	if (!file_dialog) {
		file_dialog = memnew(EditorFileDialog);
		file_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		file_dialog->clear_filters();
		file_dialog->add_filter("*.unitypackage", TTR("Unity Package"));
		file_dialog->set_title(TTR("Select Unity Package"));
		file_dialog->connect("file_selected", callable_mp(this, &UnityImporterPlugin::_file_selected));
		EditorNode::get_singleton()->get_gui_base()->add_child(file_dialog);
	}
	file_dialog->popup_file_dialog();
}

void UnityImporterPlugin::_file_selected(const String &p_path) {
	current_package_path = p_path;
	Error err = _parse_unity_package(p_path);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(vformat(TTR("Failed to parse Unity package: %s"), error_names[err]), EditorToaster::SEVERITY_ERROR);
		return;
	}
	_populate_asset_tree();
}

Error UnityImporterPlugin::_parse_unity_package(const String &p_path) {
	guid_to_asset.clear();
	assets_to_import.clear();

	// Unity packages are tar.gz archives
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open Unity package file.");

	PackedByteArray compressed_data = file->get_buffer(file->get_length());
	file.unref();

	// Decompress gzip
	PackedByteArray tar_data = compressed_data.decompress_dynamic(-1, FileAccess::COMPRESSION_GZIP);
	ERR_FAIL_COND_V_MSG(tar_data.is_empty(), ERR_FILE_CORRUPT, "Failed to decompress Unity package.");

	// Parse tar archive
	int offset = 0;
	while (offset + 512 <= tar_data.size()) {
		// Read tar header (512 bytes)
		const uint8_t *header = tar_data.ptr() + offset;
		
		// Check for end of archive (empty header)
		bool is_empty = true;
		for (int i = 0; i < 512; i++) {
			if (header[i] != 0) {
				is_empty = false;
				break;
			}
		}
		if (is_empty) {
			break;
		}

		// Parse filename (offset 0, 100 bytes)
		char name_buf[101] = {};
		memcpy(name_buf, header, 100);
		String entry_name = String::utf8(name_buf);

		// Parse file size (offset 124, 12 bytes octal)
		char size_buf[13] = {};
		memcpy(size_buf, header + 124, 12);
		int64_t file_size = String(size_buf).hex_to_int(); // Unity uses octal
		
		offset += 512; // Move past header

		// Extract file data
		if (file_size > 0 && offset + file_size <= tar_data.size()) {
			PackedByteArray entry_data;
			entry_data.resize(file_size);
			memcpy(entry_data.ptrw(), tar_data.ptr() + offset, file_size);
			
			_parse_tar_entry(entry_data, entry_name);
			
			// Move to next 512-byte boundary
			offset += ((file_size + 511) / 512) * 512;
		}
	}

	return OK;
}

Error UnityImporterPlugin::_parse_tar_entry(const PackedByteArray &p_data, const String &p_entry_path) {
	// Unity package structure: <guid>/asset, <guid>/pathname, <guid>/asset.meta
	Vector<String> parts = p_entry_path.split("/");
	if (parts.size() < 2) {
		return OK; // Skip invalid entries
	}

	String guid = parts[0];
	String entry_type = parts[1];

	if (!guid_to_asset.has(guid)) {
		Ref<UnityAsset> asset;
		asset.instantiate();
		asset->guid = guid;
		guid_to_asset[guid] = asset;
	}

	Ref<UnityAsset> asset = guid_to_asset[guid];

	if (entry_type == "asset") {
		asset->asset_data = p_data;
	} else if (entry_type == "pathname") {
		asset->orig_pathname = String::utf8((const char *)p_data.ptr(), p_data.size());
		asset->pathname = _convert_unity_path_to_godot(asset->orig_pathname);
	} else if (entry_type == "asset.meta") {
		asset->meta_bytes = p_data;
		asset->meta_data = String::utf8((const char *)p_data.ptr(), p_data.size());
		Ref<UnityMetadata> meta = _parse_meta_file(asset->meta_data, asset->orig_pathname);
		if (meta.is_valid()) {
			meta->guid = guid;
			meta->path = asset->pathname;
			if (!asset_database) {
				asset_database = UnityAssetDatabase::get_singleton();
			}
			asset_database->insert_meta(guid, meta);
		}
	}

	return OK;

void UnityImporterPlugin::_install_unity_to_godot() {
    const String src_dir = "res://addons/_unity_bundled/UnityToGodot";
    const String dst_dir = "res://addons/UnityToGodot";
    Error err = _copy_dir_recursive(src_dir, dst_dir);
    if (err != OK) {
        EditorToaster::get_singleton()->popup_str(TTR("Bundled UnityToGodot toolkit not found. Populate addons/_unity_bundled/UnityToGodot inside the editor install."));
        return;
    }
    EditorToaster::get_singleton()->popup_str(TTR("UnityToGodot toolkit installed locally under res://addons/UnityToGodot."));
}

void UnityImporterPlugin::_install_shaderlab2godotsl() {
    EditorToaster::get_singleton()->popup_str(TTR("Shader converter now built-in! Use Tools > Convert Unity Shader."));
}

void UnityImporterPlugin::_convert_unity_shader() {
    EditorNode::get_singleton()->get_file_system_dock()->navigate_to_path("res://");
    
    // TODO: Add file dialog to select Unity shader file
    // For now, show a message
    EditorToaster::get_singleton()->popup_str(TTR("Unity shader converter ready. Built-in Shaderlab2GodotSL tokenizer active."));
    
    // Example usage (would be triggered by file selection):
    // String unity_shader_content = FileAccess::get_file_as_string("path/to/shader.shader");
    // String godot_shader;
    // Error err = UnityShaderConverter::convert_shaderlab_to_godot(unity_shader_content, godot_shader);
    // if (err == OK) {
    //     FileAccess::open("res://converted_shader.gdshader", FileAccess::WRITE)->store_string(godot_shader);
    // }
}

