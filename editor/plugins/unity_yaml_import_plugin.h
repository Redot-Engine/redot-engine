/**************************************************************************/
/*  unity_yaml_import_plugin.h                                            */
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

#include "editor/import/editor_import_plugin.h"
#include "unity_package_importer.h"

class UnityAnimImportPlugin : public EditorImportPlugin {
    GDCLASS(UnityAnimImportPlugin, EditorImportPlugin);

public:
    virtual String get_importer_name() const override { return "unity_anim_importer"; }
    virtual String get_visible_name() const override { return "Unity Animation (.anim)"; }
    virtual void get_recognized_extensions(List<String> *p_extensions) const override {
        p_extensions->push_back("anim");
    }
    virtual String get_save_extension() const override { return "tres"; }
    virtual String get_resource_type() const override { return "Animation"; }
    virtual int get_import_order() const override { return 0; }
    virtual float get_priority() const override { return 1.0f; }
    virtual int get_format_version() const override { return 1; }
    virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override {}
    virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override { return true; }
    virtual bool can_import_threaded() const override { return true; }

    virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};

class UnityYamlSceneImportPlugin : public EditorImportPlugin {
    GDCLASS(UnityYamlSceneImportPlugin, EditorImportPlugin);

public:
    virtual String get_importer_name() const override { return "unity_yaml_scene_importer"; }
    virtual String get_visible_name() const override { return "Unity Scene/Prefab (.unity/.prefab)"; }
    virtual void get_recognized_extensions(List<String> *p_extensions) const override {
        p_extensions->push_back("unity");
        p_extensions->push_back("prefab");
    }
    virtual String get_save_extension() const override { return "tscn"; }
    virtual String get_resource_type() const override { return "PackedScene"; }
    virtual int get_import_order() const override { return 0; }
    virtual float get_priority() const override { return 1.0f; }
    virtual int get_format_version() const override { return 1; }
    virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override {}
    virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override { return true; }
    virtual bool can_import_threaded() const override { return true; }

    virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};

class UnityMatImportPlugin : public EditorImportPlugin {
    GDCLASS(UnityMatImportPlugin, EditorImportPlugin);

public:
    virtual String get_importer_name() const override { return "unity_mat_importer"; }
    virtual String get_visible_name() const override { return "Unity Material (.mat)"; }
    virtual void get_recognized_extensions(List<String> *p_extensions) const override {
        p_extensions->push_back("mat");
    }
    virtual String get_save_extension() const override { return "tres"; }
    virtual String get_resource_type() const override { return "Material"; }
    virtual int get_import_order() const override { return 0; }
    virtual float get_priority() const override { return 1.0f; }
    virtual int get_format_version() const override { return 1; }
    virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override {}
    virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override { return true; }
    virtual bool can_import_threaded() const override { return true; }

    virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};
