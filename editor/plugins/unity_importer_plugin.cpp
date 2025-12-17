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
// Self-contained payloads embedded below.

void UnityImporterPlugin::_bind_methods() {
    ClassDB::bind_method(D_METHOD("_import_unity_packages"), &UnityImporterPlugin::_import_unity_packages);
    ClassDB::bind_method(D_METHOD("_install_unity_to_godot"), &UnityImporterPlugin::_install_unity_to_godot);
}

void UnityImporterPlugin::_notification(int p_what) {
    if (p_what == NOTIFICATION_ENTER_TREE) {
        add_tool_menu_item(TTR("Import Unity Project..."), callable_mp(this, &UnityImporterPlugin::_import_unity_packages));
        add_tool_menu_item(TTR("Install UnityToGodot Toolkit..."), callable_mp(this, &UnityImporterPlugin::_install_unity_to_godot));
    }
    if (p_what == NOTIFICATION_EXIT_TREE) {
        remove_tool_menu_item(TTR("Import Unity Project..."));
        remove_tool_menu_item(TTR("Install UnityToGodot Toolkit..."));
    }
}

struct _PayloadFile { const char *path; const uint8_t *data; unsigned int size; };

// NOTE: Embed actual payloads here (generated offline) for full offline install.
// Minimal placeholders below; the plugin warns if empty.
static const _PayloadFile _UNIDOT_IMPORTER[] = { { nullptr, nullptr, 0 } };
static const unsigned _UNIDOT_IMPORTER_COUNT = 0;
static const _PayloadFile _UNITYTOGODOT[] = { { nullptr, nullptr, 0 } };
static const unsigned _UNITYTOGODOT_COUNT = 0;

static Error _extract_bundle(const _PayloadFile *files, unsigned count, const String &dest_dir_res) {
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
    if (!d->dir_exists(dest_dir_res)) {
        Error mk2 = d->make_dir(dest_dir_res);
        if (mk2 != OK && mk2 != ERR_ALREADY_EXISTS) {
            return mk2;
        }
    }
    for (unsigned i = 0; i < count; i++) {
        String rel_path = String(files[i].path);
        if (rel_path.is_empty()) {
            continue;
        }
        // Ensure subdirs exist
        Vector<String> parts = rel_path.split("/");
        String subdir = dest_dir_res;
        for (int pi = 0; pi < parts.size() - 1; ++pi) {
            subdir += "/" + parts[pi];
            if (!d->dir_exists(subdir)) {
                Error mk3 = d->make_dir(subdir);
                if (mk3 != OK && mk3 != ERR_ALREADY_EXISTS) {
                    return mk3;
                }
            }
        }
        String out_path = dest_dir_res + "/" + rel_path;
        Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE);
        if (f.is_null()) {
            return ERR_CANT_CREATE;
        }
        PackedByteArray pba;
        pba.resize(files[i].size);
        // Copy bytes
        for (unsigned j = 0; j < files[i].size; j++) {
            pba.set(j, files[i].data[j]);
        }
        f->store_buffer(pba);
        f->close();
    }
    return OK;
}

void UnityImporterPlugin::_import_unity_packages() {
    unsigned count = _UNIDOT_IMPORTER_COUNT;
    Error err = _extract_bundle(_UNIDOT_IMPORTER, count, "res://addons/unidot_importer");
    if (err != OK || count == 0) {
        EditorToaster::get_singleton()->popup_str(TTR("Unidot bundle not embedded. Populate editor/vendor_sources/unidot_importer and rebuild."));
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
    unsigned count = _UNITYTOGODOT_COUNT;
    Error err = _extract_bundle(_UNITYTOGODOT, count, "res://addons/UnityToGodot");
    if (err != OK || count == 0) {
        EditorToaster::get_singleton()->popup_str(TTR("UnityToGodot bundle not embedded. Populate editor/vendor_sources/UnityToGodot and rebuild."));
        return;
    }
    EditorToaster::get_singleton()->popup_str(TTR("UnityToGodot toolkit installed locally under res://addons/UnityToGodot."));
}

static const char *unidot_files[] = {
    "aligned_byte_buffer.gd",
    "asset_adapter.gd",
    "asset_database.gd",
    "asset_meta.gd",
    "bone_map_editor_plugin.gd",
    "convert_scene.gd",
    "import_worker.gd",
    "meta_worker.gd",
    "monoscript.gd",
    "object_adapter.gd",
    "package_file.gd",
    "package_import_dialog.gd",
    "plugin.cfg",
    "plugin.gd",
    "post_import_model.gd",
    "queue_lib.gd",
    "raw_parsed_asset.gd",
    "scene_node_state.gd",
    "static_preload.gd",
    "tarfile.gd",
    "unidot_utils.gd",
    "vrm_integration.gd",
    "yaml_parser.gd",
};

Error UnityImporterPlugin::_ensure_unidot_installed() {
    // Create addon directory.
    const String addon_dir = "res://addons/unidot_importer";
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
    if (!d->dir_exists(addon_dir)) {
        Error mk2 = d->make_dir(addon_dir);
        if (mk2 != OK && mk2 != ERR_ALREADY_EXISTS) {
            return mk2;
        }
    }

    // Download files from GitHub raw.
    const String base_url = "https://raw.githubusercontent.com/V-Sekai/unidot_importer/master/";
    for (size_t i = 0; i < sizeof(unidot_files) / sizeof(unidot_files[0]); ++i) {
        String fn = unidot_files[i];
        String url = base_url + fn;
        String path = addon_dir + "/" + fn;
        Error derr = _download_to_file(url, path);
        if (derr != OK) {
            return derr;
        }
    }

    // Enable plugin in project settings.
    ProjectSettings *ps = ProjectSettings::get_singleton();
    PackedStringArray enabled_plugins;
    if (ps->has_setting("editor_plugins/enabled")) {
        enabled_plugins = ps->get("editor_plugins/enabled");
    }
    const String plugin_cfg_path = addon_dir + "/plugin.cfg";
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

    // Refresh scripts so the plugin is loaded.
    EditorFileSystem::get_singleton()->scan();
    return OK;
}

// Network and git helpers removed for local-only bundling.
