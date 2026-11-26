/**************************************************************************/
/*  unity_format_loader.cpp                                               */
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

#include "unity_format_loader.h"

#include "core/config/project_settings.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "scene/resources/shader.h"

#include "unity_material.h"
#include "unity_scene.h"

String UnityFormatLoader::_get_project_root(const String &p_path) {
	String directory = p_path.get_base_dir();
	if (directory.is_empty()) {
		return String();
	}
	// Walk up to the folder that contains `Assets/` and `ProjectSettings/`.
	for (int depth = 0; depth < 64; depth++) {
		if (ProjectSettings::detect_foreign_project(directory)) {
			return directory;
		}
		const String parent = directory.get_base_dir();
		if (parent == directory || parent.is_empty()) {
			break;
		}
		directory = parent;
	}
	return String();
}

Ref<UnityProject> UnityFormatLoader::_get_project(const String &p_path) {
	const String absolute = ProjectSettings::get_singleton()->globalize_path(p_path);
	const String root = _get_project_root(absolute);
	if (root.is_empty()) {
		return Ref<UnityProject>();
	}
	if (const Ref<UnityProject> *cached = projects.getptr(root)) {
		return *cached;
	}

	Ref<UnityProject> project;
	project.instantiate();
	if (project->scan(root) != OK) {
		return Ref<UnityProject>();
	}
	projects[root] = project;
	return project;
}

Ref<Resource> UnityFormatLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	if (r_error) {
		*r_error = ERR_FILE_CANT_OPEN;
	}
	if (r_progress) {
		*r_progress = 0.0f;
	}

	const String extension = p_path.get_extension().to_lower();
	if (extension != "unity" && extension != "prefab" && extension != "mat" && extension != "shader") {
		return Ref<Resource>();
	}

	Ref<UnityProject> project = _get_project(p_path);
	if (project.is_null()) {
		if (r_error) {
			*r_error = ERR_FILE_UNRECOGNIZED;
		}
		return Ref<Resource>();
	}

	const String absolute = ProjectSettings::get_singleton()->globalize_path(p_path);

	if (extension == "shader") {
		String code;
		if (UnityMaterialBuilder::convert_shader(project, absolute, code) != OK) {
			if (r_error) {
				*r_error = ERR_FILE_CORRUPT;
			}
			return Ref<Resource>();
		}
		Ref<Shader> shader;
		shader.instantiate();
		shader->set_code(code);
		shader->set_path(p_path);
		if (r_error) {
			*r_error = OK;
		}
		return shader;
	}

	if (extension == "mat") {
		const Ref<Material> material = UnityMaterialBuilder::build_from_file(project, absolute, "res://", nullptr);
		if (material.is_valid()) {
			if (r_error) {
				*r_error = OK;
			}
			return material;
		}
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		return Ref<Resource>();
	}

	// Scenes and prefabs.
	const String project_root = project->get_root();
	const bool direct_open = ProjectSettings::get_singleton()->get_foreign_project_root() == project_root;

	String data_root;
	if (direct_open) {
		data_root = ProjectSettings::get_singleton()->get_project_data_path();
	} else {
		data_root = "user://redot_unity_cache/" + project_root.md5_text();
	}
	const String script_res_dir = data_root.path_join("unity_scripts");

	UnitySceneOptions options;
	options.target_prefix = "res://";
	options.script_res_dir = script_res_dir;
	options.script_dir = ProjectSettings::get_singleton()->globalize_path(script_res_dir);
	options.convert_scripts = true;

	UnitySceneResult result;
	if (UnitySceneBuilder::build(project, absolute, options, result) != OK || result.scene.is_null()) {
		if (r_error) {
			*r_error = ERR_FILE_CORRUPT;
		}
		return Ref<Resource>();
	}

	result.scene->set_path(p_path);
	for (const String &warning : result.warnings) {
		WARN_PRINT(vformat("[unity] %s", warning));
	}
	if (r_error) {
		*r_error = OK;
	}
	return result.scene;
}

void UnityFormatLoader::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("unity");
	p_extensions->push_back("prefab");
	p_extensions->push_back("mat");
	p_extensions->push_back("shader");
}

bool UnityFormatLoader::handles_type(const String &p_type) const {
	return p_type == "PackedScene" || p_type == "Material" || p_type == "Shader";
}

String UnityFormatLoader::get_resource_type(const String &p_path) const {
	const String extension = p_path.get_extension().to_lower();
	if (extension == "unity" || extension == "prefab") {
		return "PackedScene";
	}
	if (extension == "mat") {
		return "Material";
	}
	if (extension == "shader") {
		return "Shader";
	}
	return String();
}

bool UnityFormatLoader::exists(const String &p_path) const {
	return FileAccess::exists(p_path);
}

void UnityFormatLoader::clear_cache() {
	projects.clear();
}
