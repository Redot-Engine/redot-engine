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
#include "editor/import/editor_import_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "unity_package_importer.h"
#include "unity_shader_converter.h"

class UnityAnimImportPlugin : public EditorImportPlugin {
	GDCLASS(UnityAnimImportPlugin, EditorImportPlugin);

public:
	virtual String get_importer_name() const override { return "unity_anim_importer"; }
	virtual String get_visible_name() const override { return "Unity Animation (.anim)"; }
	virtual void get_recognized_extensions(List<String> *p_extensions) const override { p_extensions->push_back("anim"); }
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
	virtual void get_recognized_extensions(List<String> *p_extensions) const override { p_extensions->push_back("mat"); }
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

class UnityScriptImportPlugin : public EditorImportPlugin {
	GDCLASS(UnityScriptImportPlugin, EditorImportPlugin);

public:
	virtual String get_importer_name() const override { return "unity_script_importer"; }
	virtual String get_visible_name() const override { return "Unity Script (.cs -> GDScript)"; }
	virtual void get_recognized_extensions(List<String> *p_extensions) const override { p_extensions->push_back("cs"); }
	virtual String get_save_extension() const override { return "gd"; }
	virtual String get_resource_type() const override { return "Script"; }
	virtual int get_import_order() const override { return 0; }
	virtual float get_priority() const override { return 1.0f; }
	virtual int get_format_version() const override { return 1; }
	virtual void get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const override {}
	virtual bool get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const override { return true; }
	virtual bool can_import_threaded() const override { return true; }
	virtual Error import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata = nullptr) override;
};

class UnityImporterPlugin : public EditorPlugin {
	GDCLASS(UnityImporterPlugin, EditorPlugin);

	EditorFileDialog *package_dialog = nullptr;
	EditorFileDialog *shader_dialog = nullptr;
	HashMap<String, UnityAsset> parsed_assets;
	String current_package_path;

	Ref<UnityAnimImportPlugin> anim_importer;
	Ref<UnityYamlSceneImportPlugin> scene_importer;
	Ref<UnityMatImportPlugin> mat_importer;
	Ref<UnityScriptImportPlugin> script_importer;

	void _import_unity_packages();
	void _show_package_dialog();
	void _file_selected(const String &p_path);
	Error _parse_unity_package(const String &p_path);
	void _import_assets();
	void _install_unity_to_godot();
	void _install_shaderlab2godotsl();
	void _convert_unity_shader();
	void _handle_shader_file(const String &p_path);
	void _browse_asset_stores();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	UnityImporterPlugin() = default;
	~UnityImporterPlugin() override = default;
};
