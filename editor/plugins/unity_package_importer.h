/**************************************************************************/
/*  unity_package_importer.h                                              */
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

#include "core/io/file_access.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "unity_shader_converter.h"

// Unity Asset structures (ported from V-Sekai/unidot_importer)
struct UnityAsset {
	String guid;
	String pathname;
	String orig_pathname;
	String meta_data;
	PackedByteArray asset_data;
	PackedByteArray meta_bytes;
	int64_t fileID = 0;
	bool is_scene = false;
	bool is_prefab = false;
};

struct UnityMetadata {
	String guid;
	String path;
	String importer_type;
	int64_t main_object_id = 0;
	HashMap<String, Variant> keys;
	HashMap<String, String> dependency_guids;
	HashMap<int64_t, String> resource_paths;
};

class UnityPackageParser {
public:
	static Error parse_unitypackage(const String &p_path, HashMap<String, UnityAsset> &r_assets);
	static Error parse_tar_archive(const PackedByteArray &p_tar_data, HashMap<String, UnityAsset> &r_assets);
	static UnityMetadata parse_meta_file(const String &p_meta_text, const String &p_path);
	static HashMap<String, Variant> parse_yaml_simple(const String &p_yaml);
	static String convert_unity_path_to_godot(const String &p_unity_path);
};

class UnityAssetConverter {
public:
	static Error convert_texture(const UnityAsset &p_asset);
	static Error convert_material(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets);
	static Error convert_model(const UnityAsset &p_asset);
	static Error convert_scene(const UnityAsset &p_asset);
	static Error convert_prefab(const UnityAsset &p_asset);
	static Error convert_audio(const UnityAsset &p_asset);
	static Error convert_animation(const UnityAsset &p_asset);
	static Error convert_shader(const UnityAsset &p_asset);
	static Error extract_asset(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets);
	
private:
	// YAML parsing helpers for Transform data
	static Vector3 _parse_vector3_from_yaml(const String &p_yaml);
	static Quaternion _parse_quaternion_from_yaml(const String &p_yaml);
};
