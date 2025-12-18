/**************************************************************************/
/*  unity_importer_plugin.h                                               */
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

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/plugins/editor_plugin.h"
#include "unity_package_importer.h"
#include "unity_shader_converter.h"

class UnityImporterPlugin : public EditorPlugin {
    GDCLASS(UnityImporterPlugin, EditorPlugin);

    EditorFileDialog *package_dialog = nullptr;
    EditorFileDialog *shader_dialog = nullptr;
    HashMap<String, UnityAsset> parsed_assets;
    String current_package_path;

    void _import_unity_packages();
    void _show_package_dialog();
    void _file_selected(const String &p_path);
    Error _parse_unity_package(const String &p_path);
    void _import_assets();
    void _install_unity_to_godot();
    void _install_shaderlab2godotsl();
    void _convert_unity_shader();
    void _handle_shader_file(const String &p_path);

protected:
    void _notification(int p_what) override;
    static void _bind_methods();

public:
    UnityImporterPlugin() = default;
    ~UnityImporterPlugin() override = default;
};
