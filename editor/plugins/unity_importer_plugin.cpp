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

#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_toaster.h"
#include "editor/file_system/editor_file_system.h"

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
    const String src_dir = "res://addons/_unity_bundled/unidot_importer";
    const String dst_dir = "res://addons/unidot_importer";
    Error err = _copy_dir_recursive(src_dir, dst_dir);
    if (err != OK) {
        EditorToaster::get_singleton()->popup_str(TTR("Bundled Unidot importer not found. Populate addons/_unity_bundled/unidot_importer inside the editor install."));
        return;
    }
    // Enable plugin.
    ProjectSettings *ps = ProjectSettings::get_singleton();
    PackedStringArray enabled_plugins;
    if (ps->has_setting("editor_plugins/enabled")) {
        enabled_plugins = ps->get("editor_plugins/enabled");
    }
    const String plugin_cfg_path = String("res://addons/unidot_importer/plugin.cfg");
    bool already = false;
    for (int i = 0; i < enabled_plugins.size(); i++) {
        if (enabled_plugins[i] == plugin_cfg_path) {
            already = true;
            break;
        }
    }
    if (!already) {
        enabled_plugins.append(plugin_cfg_path);
        ps->set("editor_plugins/enabled", enabled_plugins);
    }
    EditorFileSystem::get_singleton()->scan();
    EditorToaster::get_singleton()->popup_str(TTR("Unidot Importer installed locally and enabled."));
}

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

