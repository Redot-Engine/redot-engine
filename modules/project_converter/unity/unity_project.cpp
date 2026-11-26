/**************************************************************************/
/*  unity_project.cpp                                                     */
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

#include "unity_project.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

#include "../import/engine_importer.h"

void UnityProject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("scan", "root"), &UnityProject::scan);
	ClassDB::bind_method(D_METHOD("get_root"), &UnityProject::get_root);
	ClassDB::bind_method(D_METHOD("get_project_name"), &UnityProject::get_project_name);
	ClassDB::bind_method(D_METHOD("get_unity_version"), &UnityProject::get_unity_version);
	ClassDB::bind_method(D_METHOD("get_path_for_guid", "guid"), &UnityProject::get_path_for_guid);
	ClassDB::bind_method(D_METHOD("get_res_path_for_guid", "guid"), &UnityProject::get_res_path_for_guid);
	ClassDB::bind_method(D_METHOD("get_scenes"), &UnityProject::get_scenes);
	ClassDB::bind_method(D_METHOD("get_prefabs"), &UnityProject::get_prefabs);
}

Error UnityProject::_parse_meta(const String &p_meta_path) {
	Error err = OK;
	const String text = FileAccess::get_file_as_string(p_meta_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot read Unity meta file '%s'.", p_meta_path));

	String guid;
	String importer;
	bool folder = false;
	int64_t main_id = 0;

	const Vector<String> lines = text.replace("\r\n", "\n").split("\n");
	for (const String &line : lines) {
		const String trimmed = line.strip_edges();
		if (guid.is_empty() && trimmed.begins_with("guid:")) {
			guid = trimmed.substr(5).strip_edges();
		} else if (trimmed.begins_with("folderAsset:")) {
			folder = trimmed.substr(12).strip_edges() == "yes";
		} else if (trimmed.begins_with("mainObjectFileID:")) {
			main_id = trimmed.substr(17).strip_edges().to_int();
		} else if (importer.is_empty() && trimmed.ends_with(":") && !trimmed.contains(" ") && line == line.strip_edges()) {
			// Top-level keys such as `TextureImporter:` name the importer.
			const String candidate = trimmed.substr(0, trimmed.length() - 1);
			if (candidate.ends_with("Importer") || candidate.ends_with("ScriptedImporter")) {
				importer = candidate;
			}
		}
	}

	if (guid.is_empty()) {
		return ERR_FILE_CORRUPT;
	}

	const String asset_path = p_meta_path.substr(0, p_meta_path.length() - 5); // Strip ".meta".
	guid_by_meta[asset_path] = guid;
	guid_by_path[asset_path] = guid;

	AssetInfo info;
	info.guid = guid;
	info.absolute_path = asset_path;
	info.relative_path = ProjectImportUtils::relative_path(root, asset_path);
	info.meta_path = p_meta_path;
	info.importer = importer;
	info.main_object_id = main_id;
	info.is_folder = folder;
	assets_by_guid[guid] = info;
	return OK;
}

Error UnityProject::_scan_dir(const String &p_dir) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V_MSG(d.is_null(), ERR_CANT_CREATE, "Cannot create directory access.");
	if (d->change_dir(p_dir) != OK) {
		return ERR_FILE_NOT_FOUND;
	}

	d->list_dir_begin();
	String name = d->get_next();
	while (!name.is_empty()) {
		if (name == "." || name == "..") {
			name = d->get_next();
			continue;
		}

		const String path = p_dir.path_join(name);
		if (d->current_is_dir()) {
			if (!ProjectImportUtils::is_ignored_directory(name)) {
				_scan_dir(path);
			}
		} else if (name.ends_with(".meta")) {
			_parse_meta(path);
		} else {
			const String ext = name.get_extension().to_lower();
			if (ext == "unity") {
				scenes.push_back(path);
			} else if (ext == "prefab") {
				prefabs.push_back(path);
			}
			// Fill GUIDs that were parsed from a meta file before the asset
			// itself was visited.
			if (guid_by_meta.has(path)) {
				const String guid = guid_by_meta[path];
				if (assets_by_guid.has(guid)) {
					assets_by_guid[guid].absolute_path = path;
					assets_by_guid[guid].relative_path = ProjectImportUtils::relative_path(root, path);
				}
			}
		}
		name = d->get_next();
	}
	d->list_dir_end();
	return OK;
}

Error UnityProject::_read_project_version() {
	const String path = root.path_join("ProjectSettings/ProjectVersion.txt");
	if (!FileAccess::exists(path)) {
		return ERR_FILE_NOT_FOUND;
	}
	Error err = OK;
	const String text = FileAccess::get_file_as_string(path, &err);
	ERR_FAIL_COND_V(err != OK, err);
	const Vector<String> lines = text.replace("\r\n", "\n").split("\n");
	for (const String &line : lines) {
		const String trimmed = line.strip_edges();
		if (trimmed.begins_with("m_EditorVersion:")) {
			unity_version = trimmed.substr(15).strip_edges();
			break;
		}
	}
	return OK;
}

Error UnityProject::_read_build_settings() {
	const String path = root.path_join("ProjectSettings/EditorBuildSettings.asset");
	if (!FileAccess::exists(path)) {
		return ERR_FILE_NOT_FOUND;
	}
	Vector<UnityYamlDocument> docs;
	if (UnityYaml::parse_file(path, docs) != OK) {
		return ERR_FILE_CORRUPT;
	}
	for (const UnityYamlDocument &doc : docs) {
		if (doc.type_name != "EditorBuildSettings") {
			continue;
		}
		const Variant scenes_value = doc.body.get("m_Scenes", Variant());
		if (scenes_value.get_type() != Variant::ARRAY) {
			continue;
		}
		const Array scenes_array = scenes_value;
		for (int i = 0; i < scenes_array.size(); i++) {
			if (scenes_array[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			const Dictionary entry = scenes_array[i];
			if (!UnityYaml::get_bool(entry, "enabled", true)) {
				continue;
			}
			const String scene_path = entry.get("path", String());
			if (!scene_path.is_empty()) {
				build_scenes.push_back(root.path_join(scene_path));
			}
		}
	}
	return OK;
}

Error UnityProject::scan(const String &p_root) {
	root = p_root.replace_char('\\', '/');
	if (root.ends_with("/")) {
		root = root.substr(0, root.length() - 1);
	}
	project_name = root.get_file();

	assets_by_guid.clear();
	guid_by_path.clear();
	guid_by_meta.clear();
	yaml_cache.clear();
	scenes.clear();
	prefabs.clear();
	build_scenes.clear();

	_read_project_version();
	_read_build_settings();

	const String assets_dir = root.path_join("Assets");
	Error err = _scan_dir(assets_dir);
	if (err != OK) {
		// A project without an Assets folder is not usable.
		return err;
	}

	// Determine the serialization mode from the first scene/prefab we can
	// read. Unity projects are either "Force Text" (YAML) or binary.
	serialization_mode = "text";
	Vector<String> candidates = scenes;
	candidates.append_array(prefabs);
	for (const String &path : candidates) {
		if (!ProjectImportUtils::is_text_yaml(path)) {
			serialization_mode = "binary";
		}
		break;
	}

	scanned = true;
	return OK;
}

String UnityProject::get_path_for_guid(const String &p_guid) const {
	const AssetInfo *info = assets_by_guid.getptr(p_guid);
	return info ? info->absolute_path : String();
}

String UnityProject::get_res_path_for_guid(const String &p_guid) const {
	const String path = get_path_for_guid(p_guid);
	return path.is_empty() ? String() : localize(path);
}

String UnityProject::get_guid_for_path(const String &p_path) const {
	const String *guid = guid_by_path.getptr(p_path.replace_char('\\', '/'));
	return guid ? *guid : String();
}

String UnityProject::get_meta_path_for_guid(const String &p_guid) const {
	const AssetInfo *info = assets_by_guid.getptr(p_guid);
	return info ? info->meta_path : String();
}

UnityProject::AssetInfo UnityProject::get_asset(const String &p_guid) const {
	const AssetInfo *info = assets_by_guid.getptr(p_guid);
	return info ? *info : AssetInfo();
}

String UnityProject::globalize(const String &p_res_path) const {
	if (p_res_path.begins_with("res://")) {
		return root.path_join(p_res_path.substr(6));
	}
	return p_res_path;
}

String UnityProject::localize(const String &p_absolute_path) const {
	const String normalized = p_absolute_path.replace_char('\\', '/');
	if (normalized.begins_with(root + "/")) {
		return "res://" + normalized.substr(root.length() + 1);
	}
	if (normalized == root) {
		return "res://";
	}
	return normalized;
}

Error UnityProject::load_documents(const String &p_path, Vector<UnityYamlDocument> &r_documents) {
	const String key = p_path.replace_char('\\', '/');
	if (const Vector<UnityYamlDocument> *cached = yaml_cache.getptr(key)) {
		r_documents = *cached;
		return r_documents.is_empty() ? ERR_FILE_UNRECOGNIZED : OK;
	}

	Vector<UnityYamlDocument> documents;
	const Error err = UnityYaml::parse_file(key, documents);
	yaml_cache[key] = documents;
	r_documents = documents;
	return err;
}
