/**************************************************************************/
/*  unity_project.h                                                       */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

#include "unity_yaml.h"

/// In-memory index of a Unity project on disk.
///
/// The index is shared by the direct-open resource loaders and the baking
/// importer. It owns the GUID database built from `*.meta` files, the asset
/// paths, and a cache of parsed YAML documents. Nothing is written to the
/// Unity folder.
class UnityProject : public RefCounted {
	GDCLASS(UnityProject, RefCounted);

public:
	struct AssetInfo {
		String guid;
		String absolute_path;
		String relative_path;
		String meta_path;
		String importer;
		int64_t main_object_id = 0;
		bool is_folder = false;
	};

private:
	String root;
	String project_name;
	String unity_version;
	String serialization_mode;
	bool scanned = false;

	HashMap<String, AssetInfo> assets_by_guid;
	HashMap<String, String> guid_by_path;
	HashMap<String, String> guid_by_meta;
	HashMap<String, Vector<UnityYamlDocument>> yaml_cache;

	Vector<String> scenes;
	Vector<String> prefabs;
	Vector<String> build_scenes;

	Error _scan_dir(const String &p_dir);
	Error _parse_meta(const String &p_meta_path);
	Error _read_project_version();
	Error _read_build_settings();

protected:
	static void _bind_methods();

public:
	Error scan(const String &p_root);

	bool is_scanned() const { return scanned; }
	String get_root() const { return root; }
	String get_project_name() const { return project_name; }
	String get_unity_version() const { return unity_version; }
	String get_serialization_mode() const { return serialization_mode; }

	bool has_guid(const String &p_guid) const { return assets_by_guid.has(p_guid); }
	String get_path_for_guid(const String &p_guid) const;
	String get_res_path_for_guid(const String &p_guid) const;
	String get_guid_for_path(const String &p_path) const;
	String get_meta_path_for_guid(const String &p_guid) const;
	AssetInfo get_asset(const String &p_guid) const;

	const HashMap<String, AssetInfo> &get_assets() const { return assets_by_guid; }
	const Vector<String> &get_scenes() const { return scenes; }
	const Vector<String> &get_prefabs() const { return prefabs; }
	const Vector<String> &get_build_scenes() const { return build_scenes; }

	/// Absolute path for a `res://` path inside this project.
	String globalize(const String &p_res_path) const;
	/// `res://` path for an absolute path inside this project, or the input
	/// unchanged when the path is outside the project.
	String localize(const String &p_absolute_path) const;

	/// Parses (and caches) the YAML documents of an asset. Returns an error
	/// for binary or malformed assets.
	Error load_documents(const String &p_path, Vector<UnityYamlDocument> &r_documents);
};
