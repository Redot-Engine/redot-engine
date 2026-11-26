/**************************************************************************/
/*  unity_importer.cpp                                                    */
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

#include "unity_importer.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "core/version.h"

#include "unity_material.h"
#include "unity_project.h"
#include "unity_scene.h"
#include "unity_script.h"

namespace {

const char *const UNITY_ONLY_EXTENSIONS[] = {
	"cs", "js", "unity", "prefab", "mat", "shader", "cginc", "hlsl", "shadergraph",
	"shadersubgraph", "asset", "controller", "anim", "overridecontroller", "physicmaterial",
	"physicsmaterial2d", "guiskin", "fontsettings", "preset", "signal", "mixer", "playable",
	"mask", "spriteatlas", "lightingsettings", "lightingdata", "flare", "cubemap",
	"rendertexture", "terrainlayer", "brush", "shadervariants", "dll", "meta"
};

bool _is_unity_only(const String &p_extension) {
	for (const char *extension : UNITY_ONLY_EXTENSIONS) {
		if (p_extension == extension) {
			return true;
		}
	}
	return false;
}

String _unique_output(HashMap<String, int> &r_used, const String &p_dir, const String &p_name, const String &p_extension) {
	int count = r_used.has(p_name) ? r_used[p_name] : 0;
	r_used[p_name] = count + 1;
	const String file_name = count == 0 ? p_name : vformat("%s_%d", p_name, count + 1);
	return p_dir.path_join(file_name + "." + p_extension);
}

void _copy_assets(Ref<UnityProject> p_project, const String &p_target, Ref<ProjectImportReport> p_report) {
	const HashMap<String, UnityProject::AssetInfo> &assets = p_project->get_assets();
	for (const KeyValue<String, UnityProject::AssetInfo> &entry : assets) {
		const UnityProject::AssetInfo &asset = entry.value;
		if (asset.absolute_path.is_empty()) {
			continue;
		}
		const String relative = asset.relative_path.replace_char('\\', '/');
		const String destination = p_target.path_join(relative);

		if (asset.is_folder) {
			ProjectImportUtils::ensure_dir(destination);
			continue;
		}

		const String extension = asset.absolute_path.get_extension().to_lower();
		if (_is_unity_only(extension)) {
			continue;
		}
		if (ProjectImportUtils::copy_file(asset.absolute_path, destination) == OK) {
			p_report->add_converted();
		} else {
			p_report->warning(vformat("Could not copy asset '%s'.", relative), asset.absolute_path);
		}
	}
}

void _convert_scripts(Ref<UnityProject> p_project, const String &p_scripts_dir, Ref<ProjectImportReport> p_report) {
	const HashMap<String, UnityProject::AssetInfo> &assets = p_project->get_assets();
	for (const KeyValue<String, UnityProject::AssetInfo> &entry : assets) {
		const UnityProject::AssetInfo &asset = entry.value;
		if (asset.absolute_path.get_extension().to_lower() != "cs") {
			continue;
		}
		Error err = OK;
		const String source = FileAccess::get_file_as_string(asset.absolute_path, &err);
		if (err != OK) {
			p_report->warning("Could not read C# script.", asset.absolute_path);
			continue;
		}
		const UnityScriptConverter::Result converted = UnityScriptConverter::convert(source, asset.absolute_path.get_file().get_basename());
		const String file_name = vformat("%s_%s.gd", converted.class_name, entry.key.substr(0, MIN(8, entry.key.length())));
		if (ProjectImportUtils::write_text(p_scripts_dir.path_join(file_name), converted.code) == OK) {
			p_report->add_converted();
			for (const String &warning : converted.warnings) {
				p_report->warning(warning, asset.absolute_path);
			}
		} else {
			p_report->warning("Could not write converted script.", asset.absolute_path);
		}
	}
}

void _convert_shaders(Ref<UnityProject> p_project, const String &p_shaders_dir, Ref<ProjectImportReport> p_report) {
	const HashMap<String, UnityProject::AssetInfo> &assets = p_project->get_assets();
	for (const KeyValue<String, UnityProject::AssetInfo> &entry : assets) {
		const UnityProject::AssetInfo &asset = entry.value;
		if (asset.absolute_path.get_extension().to_lower() != "shader") {
			continue;
		}
		String code;
		if (UnityMaterialBuilder::convert_shader(p_project, asset.absolute_path, code) != OK) {
			p_report->warning("ShaderLab conversion failed; the shader was skipped.", asset.absolute_path);
			continue;
		}
		const String file_name = vformat("%s_%s.gdshader", asset.absolute_path.get_file().get_basename(), entry.key.substr(0, MIN(8, entry.key.length())));
		if (ProjectImportUtils::write_text(p_shaders_dir.path_join(file_name), code) == OK) {
			p_report->add_converted();
		} else {
			p_report->warning("Could not write converted shader.", asset.absolute_path);
		}
	}
}

String _project_file_text(const String &p_name, const String &p_main_scene) {
	String text;
	text += "; Engine configuration file.\n";
	text += "; Generated by the Redot Unity importer.\n";
	text += "config_version=5\n\n";
	text += "[application]\n\n";
	text += vformat("config/name=\"%s\"\n", p_name.replace("\"", "\\\""));
	text += vformat("config/features=PackedStringArray(\"%d.%d\", \"Forward Plus\")\n", VERSION_MAJOR, VERSION_MINOR);
	if (!p_main_scene.is_empty()) {
		text += vformat("run/main_scene=\"%s\"\n", p_main_scene);
	}
	text += "\n[rendering]\n\n";
	text += "renderer/rendering_method=\"forward_plus\"\n";
	return text;
}

} // namespace

bool UnityImporter::can_import(const String &p_source_dir) const {
	String engine;
	return ProjectSettings::detect_foreign_project(p_source_dir, &engine) && engine == "unity";
}

Dictionary UnityImporter::get_project_info(const String &p_source_dir) const {
	Dictionary info = EngineImporter::get_project_info(p_source_dir);
	Ref<UnityProject> project;
	project.instantiate();
	if (project->scan(p_source_dir) == OK) {
		info["name"] = project->get_project_name();
		info["version"] = project->get_unity_version();
		info["scene_count"] = project->get_scenes().size();
		info["prefab_count"] = project->get_prefabs().size();
		info["asset_count"] = project->get_assets().size();
		info["serialization"] = project->get_serialization_mode();
	}
	return info;
}

Dictionary UnityImporter::import_project(const String &p_source_dir, const String &p_target_dir, const Dictionary &p_options) {
	Ref<ProjectImportReport> report;
	report.instantiate();
	report->set_engine("unity");
	report->set_source_path(p_source_dir);
	report->set_target_path(p_target_dir);

	Ref<UnityProject> project;
	project.instantiate();
	if (project->scan(p_source_dir) != OK) {
		report->error("The Unity project could not be scanned.", p_source_dir);
		return report->to_dictionary();
	}

	if (p_target_dir == p_source_dir || p_target_dir.begins_with(p_source_dir + "/")) {
		report->error("The destination must be outside the Unity project folder.", p_target_dir);
		return report->to_dictionary();
	}
	if (ProjectImportUtils::ensure_dir(p_target_dir) != OK) {
		report->error("The destination folder could not be created.", p_target_dir);
		return report->to_dictionary();
	}

	const String scripts_dir = p_target_dir.path_join("Scripts");
	const String shaders_dir = p_target_dir.path_join("Shaders");
	const String scenes_dir = p_target_dir.path_join("Scenes");
	const String prefabs_dir = p_target_dir.path_join("Prefabs");
	ProjectImportUtils::ensure_dir(scripts_dir);
	ProjectImportUtils::ensure_dir(shaders_dir);
	ProjectImportUtils::ensure_dir(scenes_dir);
	ProjectImportUtils::ensure_dir(prefabs_dir);

	report->info(vformat("Unity %s project with %d scenes and %d prefabs.", project->get_unity_version(), project->get_scenes().size(), project->get_prefabs().size()));

	_copy_assets(project, p_target_dir, report);
	_convert_scripts(project, scripts_dir, report);
	_convert_shaders(project, shaders_dir, report);

	UnitySceneOptions options;
	options.target_prefix = "res://";
	options.script_dir = scripts_dir;
	options.script_res_dir = "res://Scripts";
	options.convert_scripts = true;

	HashMap<String, int> used_scene_names;
	HashMap<String, int> used_prefab_names;
	HashMap<String, String> scene_outputs; // Absolute source path -> res:// path.

	auto convert_asset = [&](const String &p_path, const String &p_output_dir, const String &p_res_dir, const String &p_extension) {
		UnitySceneResult result;
		if (UnitySceneBuilder::build(project, p_path, options, result) != OK || result.scene.is_null()) {
			report->warning("Could not convert the asset.", p_path);
			return String();
		}
		HashMap<String, int> &used = p_output_dir == scenes_dir ? used_scene_names : used_prefab_names;
		const String output = _unique_output(used, p_output_dir, p_path.get_file().get_basename(), p_extension);
		const Error save_error = ResourceSaver::save(output, result.scene);
		if (save_error != OK) {
			report->warning(vformat("Could not save '%s' (error %d).", output, save_error), p_path);
			return String();
		}
		for (const String &warning : result.warnings) {
			report->warning(warning, p_path);
		}
		report->add_converted();
		report->info(vformat("Converted %d nodes (%d scripts).", result.node_count, result.script_count), p_path);
		return p_res_dir.path_join(output.get_file());
	};

	for (const String &scene_path : project->get_scenes()) {
		const String output = convert_asset(scene_path, scenes_dir, "res://Scenes", "tscn");
		if (!output.is_empty()) {
			scene_outputs[scene_path] = output;
		}
	}

	for (const String &prefab_path : project->get_prefabs()) {
		convert_asset(prefab_path, prefabs_dir, "res://Prefabs", "tscn");
	}

	String main_scene;
	for (const String &build_scene : project->get_build_scenes()) {
		if (scene_outputs.has(build_scene)) {
			main_scene = scene_outputs[build_scene];
			break;
		}
	}
	if (main_scene.is_empty() && !scene_outputs.is_empty()) {
		for (const KeyValue<String, String> &entry : scene_outputs) {
			main_scene = entry.value;
			break;
		}
	}

	const String project_file = p_target_dir.path_join("project.godot");
	if (ProjectImportUtils::write_text(project_file, _project_file_text(project->get_project_name(), main_scene)) != OK) {
		report->error("Could not write project.godot.", project_file);
	}

	report->write_markdown(p_target_dir.path_join("import_report.md"));
	report->write_json(p_target_dir.path_join("import_report.json"));
	report->info("Import finished.");
	return report->to_dictionary();
}
