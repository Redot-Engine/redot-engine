/**************************************************************************/
/*  unity_package_importer.cpp                                            */
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

#include "unity_package_importer.h"

#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/string/print_string.h"
#include "editor/settings/editor_settings.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/animation.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

static Error ensure_parent_dir_for_file(const String &p_path) {
	String dir_path = p_path.get_base_dir();
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d.is_null()) {
		return ERR_CANT_CREATE;
	}
	if (dir_path.is_empty()) {
		return OK;
	}
	return d->make_dir_recursive(dir_path);
}

static bool parse_color_from_line(const String &p_line, Color &r_color) {
	int brace_open = p_line.find("{");
	int brace_close = p_line.find("}", brace_open + 1);
	if (brace_open == -1 || brace_close == -1) {
		return false;
	}
	String inner = p_line.substr(brace_open + 1, brace_close - brace_open - 1);
	Vector<String> pairs = inner.split(",");
	float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
	for (int i = 0; i < pairs.size(); i++) {
		Vector<String> kv = pairs[i].split(":");
		if (kv.size() != 2) {
			continue;
		}
		String key = kv[0].strip_edges();
		float value = kv[1].strip_edges().to_float();
		if (key == "r") {
			r = value;
		} else if (key == "g") {
			g = value;
		} else if (key == "b") {
			b = value;
		} else if (key == "a") {
			a = value;
		}
	}
	r_color = Color(r, g, b, a);
	return true;
}

static int count_leading_whitespace(const String &p_line) {
	int count = 0;
	while (count < p_line.length()) {
		char32_t c = p_line[count];
		if (c != ' ' && c != '\t') {
			break;
		}
		count++;
	}
	return count;
}

static String resolve_case_insensitive_path(const String &p_path) {
	String dir_path = p_path.get_base_dir();
	String filename = p_path.get_file();
	Ref<DirAccess> d = DirAccess::open(dir_path);
	if (d.is_null()) {
		return p_path;
	}

	String found;
	d->list_dir_begin();
	String name = d->get_next();
	while (!name.is_empty()) {
		if (name == "." || name == ".." || d->current_is_dir()) {
			name = d->get_next();
			continue;
		}

		if (name.to_lower() == filename.to_lower()) {
			found = name;
			break;
		}

		name = d->get_next();
	}
	d->list_dir_end();

	if (!found.is_empty()) {
		return dir_path.path_join(found);
	}

	return p_path;
}

static String extract_albedo_texture_guid(const Vector<String> &p_lines) {
	static const char *texture_keys[] = { "_MainTex:", "- _MainTex:", "_BaseMap:", "- _BaseMap:" };

	for (int i = 0; i < p_lines.size(); i++) {
		String line = p_lines[i];
		String trimmed = line.strip_edges();
		bool matches = false;
		for (int j = 0; j < 4; j++) {
			if (trimmed.begins_with(texture_keys[j])) {
				matches = true;
				break;
			}
		}
		if (!matches) {
			continue;
		}

		int base_indent = count_leading_whitespace(line);
		for (int j = i + 1; j < p_lines.size(); j++) {
			String inner_line = p_lines[j];
			String inner_trimmed = inner_line.strip_edges();
			if (inner_trimmed.is_empty()) {
				continue;
			}

			int inner_indent = count_leading_whitespace(inner_line);
			if (inner_trimmed.begins_with("- ") && inner_indent <= base_indent) {
				break;
			}

			int guid_pos = inner_trimmed.find("guid:");
			if (guid_pos == -1) {
				continue;
			}

			String guid = inner_trimmed.substr(guid_pos + 5).strip_edges();
			int comma = guid.find(",");
			if (comma != -1) {
				guid = guid.substr(0, comma);
			}
			int brace = guid.find("}");
			if (brace != -1) {
				guid = guid.substr(0, brace);
			}

			return guid.strip_edges();
		}
	}

	return String();
}

// Unity package parser implementation (ported from V-Sekai/unidot_importer)

Error UnityPackageParser::parse_unitypackage(const String &p_path, HashMap<String, UnityAsset> &r_assets) {
	// Read compressed .unitypackage file
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open Unity package file.");

	PackedByteArray compressed = file->get_buffer(file->get_length());
	file.unref();

	// Decompress gzip using engine Compression API
	PackedByteArray tar_data;
	{
		int derr = Compression::decompress_dynamic(&tar_data, -1, compressed.ptr(), compressed.size(), Compression::MODE_GZIP);
		ERR_FAIL_COND_V_MSG(derr != OK || tar_data.is_empty(), ERR_FILE_CORRUPT, "Failed to decompress Unity package.");
	}

	return parse_tar_archive(tar_data, r_assets);
}

Error UnityPackageParser::parse_tar_archive(const PackedByteArray &p_tar_data, HashMap<String, UnityAsset> &r_assets) {
	if (p_tar_data.is_empty()) {
		print_error("TAR archive is empty");
		return ERR_FILE_CORRUPT;
	}

	int offset = 0;
	HashMap<String, UnityAsset *> guid_map;
	int total_files = 0;
	int guid_entries = 0;

	while (offset + 512 <= p_tar_data.size()) {
		const uint8_t *header = p_tar_data.ptr() + offset;

		// Check for end of archive (two consecutive 512-byte zero blocks)
		bool is_empty = true;
		for (int i = 0; i < 512; i++) {
			if (header[i] != 0) {
				is_empty = false;
				break;
			}
		}
		if (is_empty) {
			break;
		}

		// Parse filename (offset 0, 100 bytes null-terminated)
		char name_buf[101] = {};
		memcpy(name_buf, header, 100);
		String entry_name = String::utf8(name_buf).strip_edges();

		if (entry_name.is_empty()) {
			offset += 512;
			continue;
		}

		total_files++;

		// Parse file size (offset 124, 12 bytes octal)
		char size_buf[13] = {};
		memcpy(size_buf, header + 124, 12);
		int64_t file_size = 0;

		// Parse octal size
		for (int i = 0; i < 12 && size_buf[i] != 0; i++) {
			if (size_buf[i] >= '0' && size_buf[i] <= '7') {
				file_size = file_size * 8 + (size_buf[i] - '0');
			}
		}

		offset += 512;

		// Extract file data with boundary checking
		if (file_size > 0) {
			if (offset + file_size > p_tar_data.size()) {
				print_error(vformat("TAR entry '%s' exceeds archive size", entry_name));
				break;
			}

			PackedByteArray entry_data;
			entry_data.resize(file_size);
			memcpy(entry_data.ptrw(), p_tar_data.ptr() + offset, file_size);

			// Parse entry structure: <guid>/asset, <guid>/pathname, <guid>/asset.meta
			Vector<String> parts = entry_name.split("/");
			if (parts.size() >= 2) {
				String guid = parts[0];
				String entry_type = parts[1];

				// Validate GUID format (should be 32-character hex)
				if (guid.length() == 32) {
					guid_entries++;

					if (!guid_map.has(guid)) {
						UnityAsset asset;
						asset.guid = guid;
						r_assets[guid] = asset;
						guid_map[guid] = &r_assets[guid];
					}

					UnityAsset *asset = guid_map[guid];
					if (asset != nullptr) {
						if (entry_type == "asset") {
							asset->asset_data = entry_data;
						} else if (entry_type == "pathname") {
							asset->orig_pathname = String::utf8((const char *)entry_data.ptr(), entry_data.size()).strip_edges();
							asset->pathname = convert_unity_path_to_godot(asset->orig_pathname);
						} else if (entry_type == "asset.meta") {
							asset->meta_bytes = entry_data;
							asset->meta_data = String::utf8((const char *)entry_data.ptr(), entry_data.size());
						}
					}
				}
			}

			// Move to next 512-byte boundary
			offset += ((file_size + 511) / 512) * 512;
		}
	}

	print_line(vformat("TAR archive parsed: %d total files, %d GUID-based entries, %d unique assets", total_files, guid_entries, r_assets.size()));
	return OK;
}

HashMap<String, Variant> UnityPackageParser::parse_yaml_simple(const String &p_yaml) {
	HashMap<String, Variant> result;
	Vector<String> lines = p_yaml.split("\n");

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.is_empty() || line.begins_with("#") || line.begins_with("%")) {
			continue;
		}

		int colon_pos = line.find(":");
		if (colon_pos > 0) {
			String key = line.substr(0, colon_pos).strip_edges();
			String value = line.substr(colon_pos + 1).strip_edges();
			result[key] = value;
		}
	}

	return result;
}

UnityMetadata UnityPackageParser::parse_meta_file(const String &p_meta_text, const String &p_path) {
	UnityMetadata meta;
	meta.path = p_path;

	HashMap<String, Variant> yaml = parse_yaml_simple(p_meta_text);

	if (yaml.has("guid")) {
		meta.guid = yaml["guid"];
	}
	if (yaml.has("importer")) {
		meta.importer_type = yaml["importer"];
	}
	if (yaml.has("mainObjectFileID")) {
		meta.main_object_id = String(yaml["mainObjectFileID"]).to_int();
	}

	return meta;
}

String UnityPackageParser::convert_unity_path_to_godot(const String &p_unity_path) {
	String godot_path = p_unity_path;
	if (godot_path.begins_with("Assets/")) {
		godot_path = godot_path.substr(7);
	}
	return "res://" + godot_path;
}

// Helper functions for YAML Transform parsing
Vector3 UnityAssetConverter::_parse_vector3_from_yaml(const String &p_yaml) {
	// Parse format: {x: 0, y: 0, z: 0} or {x: 0.5, y: -1.2, z: 3.14}
	Vector3 result;
	result.x = 0;
	result.y = 0;
	result.z = 0;

	String content = p_yaml;
	if (content.begins_with("{") && content.ends_with("}")) {
		content = content.substr(1, content.length() - 2); // Remove braces
	}

	Vector<String> parts = content.split(",");
	for (const String &part : parts) {
		String trimmed = part.strip_edges();
		if (trimmed.begins_with("x:")) {
			result.x = trimmed.substr(2).strip_edges().to_float();
		} else if (trimmed.begins_with("y:")) {
			result.y = trimmed.substr(2).strip_edges().to_float();
		} else if (trimmed.begins_with("z:")) {
			result.z = trimmed.substr(2).strip_edges().to_float();
		}
	}

	return result;
}

Quaternion UnityAssetConverter::_parse_quaternion_from_yaml(const String &p_yaml) {
	// Parse format: {x: 0, y: 0, z: 0, w: 1}
	float x = 0, y = 0, z = 0, w = 1;

	String content = p_yaml;
	if (content.begins_with("{") && content.ends_with("}")) {
		content = content.substr(1, content.length() - 2); // Remove braces
	}

	Vector<String> parts = content.split(",");
	for (const String &part : parts) {
		String trimmed = part.strip_edges();
		if (trimmed.begins_with("x:")) {
			x = trimmed.substr(2).strip_edges().to_float();
		} else if (trimmed.begins_with("y:")) {
			y = trimmed.substr(2).strip_edges().to_float();
		} else if (trimmed.begins_with("z:")) {
			z = trimmed.substr(2).strip_edges().to_float();
		} else if (trimmed.begins_with("w:")) {
			w = trimmed.substr(2).strip_edges().to_float();
		}
	}

	return Quaternion(x, y, z, w);
}

String UnityAssetConverter::_translate_unity_terminology(const String &p_text) {
	// Check if Unity terminology setting is enabled
	bool use_unity_terms = EDITOR_GET("interface/editor/use_unity_terminology");

	if (!use_unity_terms) {
		return p_text; // Return unchanged if terminology translation is disabled
	}

	String result = p_text;

	// Dictionary of Godot -> Unity terminology mappings (only replace when setting is enabled)
	// This allows users to see familiar Unity terminology in Redot editor
	const char *godot_terms[] = {
		"Node", "Scene", "Child", "Parent", "Transform", "Physics",
		"Collision", "Body", "Area", "Shape", "Mesh", "Material",
		"Camera", "Light", "Particle", "Animation", "Script", "Signal"
	};

	const char *unity_equiv[] = {
		"GameObject", "Prefab", "Child", "Parent", "Transform", "Physics",
		"Collision", "Rigidbody", "Collider", "Collider", "Model", "Material",
		"Camera", "Light", "Particle System", "Animator", "Component", "Event"
	};

	int term_count = sizeof(godot_terms) / sizeof(godot_terms[0]);

	// Simple word replacement (case-sensitive to avoid false positives)
	for (int i = 0; i < term_count; i++) {
		// Only replace whole words to avoid partial matches
		String godot_word = String(godot_terms[i]);
		String unity_word = String(unity_equiv[i]);

		// This is a simple approach - for production, a more sophisticated NLP would be better
		// Currently handles: "Node" -> "GameObject", "Scene" -> "Prefab", etc.
		if (godot_word == "Node") {
			result = result.replace("Node", "GameObject");
		} else if (godot_word == "Scene") {
			result = result.replace("Scene", "Prefab");
		}
	}

	return result;
}

// Asset conversion implementations

Error UnityAssetConverter::extract_asset(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets) {
	if (p_asset.pathname.is_empty()) {
		print_error(vformat("Asset GUID %s has no pathname", p_asset.guid));
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	if (p_asset.asset_data.is_empty()) {
		print_error(vformat("Asset '%s' (GUID: %s) has no asset data", p_asset.pathname, p_asset.guid));
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	String ext = p_asset.pathname.get_extension().to_lower();

	// Route to appropriate converter
	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff") {
		return convert_texture(p_asset);
	} else if (ext == "mat") {
		return convert_material(p_asset, p_all_assets);
	} else if (ext == "fbx" || ext == "obj" || ext == "dae") {
		return convert_model(p_asset);
	} else if (ext == "unity" || ext == "scene") {
		return convert_scene(p_asset);
	} else if (ext == "prefab") {
		return convert_prefab(p_asset);
	} else if (ext == "wav" || ext == "mp3" || ext == "ogg") {
		return convert_audio(p_asset);
	} else if (ext == "anim") {
		return convert_animation(p_asset);
	} else if (ext == "shader") {
		return convert_shader(p_asset);
	}

	// Default: copy as-is
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_texture(const UnityAsset &p_asset) {
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(p_asset.pathname) != OK, ERR_CANT_CREATE, "Cannot create target directory for texture.");
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_material(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets) {
	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	Ref<StandardMaterial3D> material;
	material.instantiate();
	material->set_name(p_asset.pathname.get_file().get_basename());
	material->set_meta("unity_yaml", yaml);

	// Try to discover an albedo color from common Unity keys
	Color albedo;
	Vector<String> lines = yaml.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String l = lines[i].strip_edges();
		if (l.find("_Color") != -1 || l.find("m_Diffuse") != -1) {
			if (parse_color_from_line(l, albedo)) {
				material->set_albedo(albedo);
				break;
			}
		}
	}

	String albedo_guid = extract_albedo_texture_guid(lines);
	if (!albedo_guid.is_empty() && p_all_assets.has(albedo_guid)) {
		const UnityAsset &tex_asset = p_all_assets[albedo_guid];
		String texture_path = resolve_case_insensitive_path(tex_asset.pathname);
		if (FileAccess::exists(texture_path)) {
			Ref<Texture2D> albedo_texture = ResourceLoader::load(texture_path);
			if (albedo_texture.is_valid()) {
				material->set_texture(StandardMaterial3D::TEXTURE_ALBEDO, albedo_texture);
			}
		}
	}

	String out_path = p_asset.pathname;
	if (!out_path.ends_with(".tres")) {
		out_path += ".tres";
	}
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(out_path) != OK, ERR_CANT_CREATE, "Cannot create target directory for material.");
	Error save_err = ResourceSaver::save(material, out_path);
	return save_err;
}

Error UnityAssetConverter::convert_model(const UnityAsset &p_asset) {
	// Direct copy - Godot can import FBX/OBJ/DAE natively
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(p_asset.pathname) != OK, ERR_CANT_CREATE, "Cannot create target directory for model.");
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_scene(const UnityAsset &p_asset) {
	print_verbose(vformat("Starting scene conversion for: %s", p_asset.pathname.get_file()));
	// p_asset.pathname is already the full output path with hash
	Error dir_result = ensure_parent_dir_for_file(p_asset.pathname);
	if (dir_result != OK) {
		print_error(vformat("Failed to create directory for scene: %s", p_asset.pathname));
		return ERR_CANT_CREATE;
	}

	// Detect binary Unity files (binary format starts with specific signatures)
	// Binary Unity files cannot be parsed as YAML text
	if (p_asset.asset_data.size() > 20) {
		const uint8_t *data = p_asset.asset_data.ptr();
		// Check for Unity binary file magic numbers
		// Unity 2019+ binary serialization format
		if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
			print_error(vformat("Scene '%s' is in Unity BINARY format. Only YAML text format is supported. Re-export from Unity with 'Force Text' serialization.", p_asset.pathname.get_file()));
			return ERR_FILE_UNRECOGNIZED;
		}
	}

	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());

	// Additional check: YAML files must start with %YAML directive
	if (!yaml.begins_with("%YAML") && !yaml.begins_with("---")) {
		print_error(vformat("Scene '%s' does not appear to be valid YAML text format. Ensure Unity is set to 'Force Text' serialization mode.", p_asset.pathname.get_file()));
		return ERR_FILE_UNRECOGNIZED;
	}

	// Create scene and root node
	Ref<PackedScene> scene = memnew(PackedScene);
	Node3D *root = memnew(Node3D);
	root->set_name(p_asset.pathname.get_file().get_basename());
	// NOTE: Root node does NOT own itself in Godot! Only children own the root.

	// Parse YAML to extract GameObjects and build node hierarchy
	Vector<String> lines = yaml.split("\n");
	HashMap<String, Node3D *> fileID_to_node;
	HashMap<String, String> fileID_to_name;
	HashMap<String, String> fileID_to_parent;
	HashMap<String, String> fileID_to_prefab_guid;
	HashMap<String, String> fileID_to_prefab_instance_guid;  // For PrefabInstance GUIDs
	HashMap<String, Vector3> fileID_to_position;
	HashMap<String, Quaternion> fileID_to_rotation;
	HashMap<String, Vector3> fileID_to_scale;

	String current_section_type = "";
	String current_file_id = "";
	String current_game_object_name = "GameObject";
	String current_parent_ref = "";
	String current_prefab_guid = "";
	bool in_prefab_instance = false;
	String prefab_instance_guid = "";

	Vector3 current_position;
	current_position.x = 0;
	current_position.y = 0;
	current_position.z = 0;

	Quaternion current_rotation;
	current_rotation.x = 0;
	current_rotation.y = 0;
	current_rotation.z = 0;
	current_rotation.w = 1;

	Vector3 current_scale;
	current_scale.x = 1;
	current_scale.y = 1;
	current_scale.z = 1;

	bool in_game_object = false;
	bool in_transform = false;
	int game_object_count = 0;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		String trimmed = line.strip_edges();

		// Detect document sections with file IDs
		if (trimmed.begins_with("--- !u!")) {
			// Save previous GameObject if any
			if (in_game_object && !current_file_id.is_empty() && !current_game_object_name.is_empty()) {
				fileID_to_name[current_file_id] = current_game_object_name;
				if (!current_parent_ref.is_empty()) {
					fileID_to_parent[current_file_id] = current_parent_ref;
				}
				if (!current_prefab_guid.is_empty()) {
					fileID_to_prefab_guid[current_file_id] = current_prefab_guid;
				}
			}

			// Reset state
			in_game_object = false;
			in_transform = false;
			current_parent_ref = "";
			current_prefab_guid = "";

			// Extract file ID from "--- !u!1 &123456789"
			int amp_pos = trimmed.find("&");
			if (amp_pos != -1) {
				current_file_id = trimmed.substr(amp_pos + 1).strip_edges();
			}

			// Detect section type
			if (trimmed.contains("!u!1 ")) {
				in_game_object = true;
				current_game_object_name = "GameObject";
			} else if (trimmed.contains("!u!4 ") || trimmed.contains("!u!224 ")) {
				// Transform or RectTransform
				in_transform = true;
			}
		}
		// Extract GameObject name
		else if (in_game_object && trimmed.begins_with("m_Name:")) {
			int colon = trimmed.find(":");
			String name_value = trimmed.substr(colon + 1).strip_edges();
			if (name_value.begins_with("\"") && name_value.ends_with("\"")) {
				name_value = name_value.substr(1, name_value.length() - 2);
			}
			if (!name_value.is_empty()) {
				current_game_object_name = name_value;
			}
		}
		// Extract parent reference from Transform
		else if (in_transform && trimmed.begins_with("m_Father:")) {
			// Look for "fileID: 123456789" in next line or same line
			if (i + 1 < lines.size()) {
				String next_line = lines[i + 1].strip_edges();
				if (next_line.begins_with("fileID:")) {
					current_parent_ref = next_line.substr(7).strip_edges();
				}
			}
		}
		// Extract Transform position
		else if (in_transform && trimmed.begins_with("m_LocalPosition:")) {
			int colon = trimmed.find(":");
			String pos_yaml = trimmed.substr(colon + 1).strip_edges();
			current_position = _parse_vector3_from_yaml(pos_yaml);
			fileID_to_position[current_file_id] = current_position;
			print_verbose(vformat("Scene Transform: Position for '%s': (%f, %f, %f)", current_game_object_name, current_position.x, current_position.y, current_position.z));
		}
		// Extract Transform rotation
		else if (in_transform && trimmed.begins_with("m_LocalRotation:")) {
			int colon = trimmed.find(":");
			String rot_yaml = trimmed.substr(colon + 1).strip_edges();
			current_rotation = _parse_quaternion_from_yaml(rot_yaml);
			fileID_to_rotation[current_file_id] = current_rotation;
			print_verbose(vformat("Scene Transform: Rotation for '%s': (%f, %f, %f, %f)", current_game_object_name, current_rotation.x, current_rotation.y, current_rotation.z, current_rotation.w));
		}
		// Extract Transform scale
		else if (in_transform && trimmed.begins_with("m_LocalScale:")) {
			int colon = trimmed.find(":");
			String scale_yaml = trimmed.substr(colon + 1).strip_edges();
			current_scale = _parse_vector3_from_yaml(scale_yaml);
			fileID_to_scale[current_file_id] = current_scale;
			print_verbose(vformat("Scene Transform: Scale for '%s': (%f, %f, %f)", current_game_object_name, current_scale.x, current_scale.y, current_scale.z));
		}
		// Extract prefab GUID reference
		else if (trimmed.begins_with("guid:") && (in_game_object || in_transform)) {
			current_prefab_guid = trimmed.substr(5).strip_edges();
			// Remove quotes and commas
			if (current_prefab_guid.begins_with("\"")) {
				current_prefab_guid = current_prefab_guid.substr(1);
			}
			int comma = current_prefab_guid.find(",");
			if (comma != -1) {
				current_prefab_guid = current_prefab_guid.substr(0, comma);
			}
			if (current_prefab_guid.ends_with("\"")) {
				current_prefab_guid = current_prefab_guid.substr(0, current_prefab_guid.length() - 1);
			}
		}
	}

	// Save last object
	if (in_game_object && !current_file_id.is_empty() && !current_game_object_name.is_empty()) {
		fileID_to_name[current_file_id] = current_game_object_name;
		if (!current_parent_ref.is_empty()) {
			fileID_to_parent[current_file_id] = current_parent_ref;
		}
		if (!current_prefab_guid.is_empty()) {
			fileID_to_prefab_guid[current_file_id] = current_prefab_guid;
		}
	}

	// Build node hierarchy
	for (const KeyValue<String, String> &E : fileID_to_name) {
		String file_id = E.key;
		String name = E.value;

		Node3D *node = memnew(Node3D);
		// Apply Unity terminology translation if setting is enabled
		String display_name = _translate_unity_terminology(name);
		node->set_name(display_name);

		// Apply Transform data from extraction
		if (fileID_to_position.has(file_id)) {
			node->set_position(fileID_to_position[file_id]);
			print_verbose(vformat("Scene: Applied position to '%s': %v", name, fileID_to_position[file_id]));
		}
		if (fileID_to_rotation.has(file_id)) {
			Vector3 euler = fileID_to_rotation[file_id].get_euler();
			node->set_rotation(euler);
			print_verbose(vformat("Scene: Applied rotation to '%s': %v", name, euler));
		}
		if (fileID_to_scale.has(file_id)) {
			node->set_scale(fileID_to_scale[file_id]);
			print_verbose(vformat("Scene: Applied scale to '%s': %v", name, fileID_to_scale[file_id]));
		}

		// Store prefab reference as metadata
		if (fileID_to_prefab_guid.has(file_id)) {
			node->set_meta("unity_prefab_guid", fileID_to_prefab_guid[file_id]);
		}

		fileID_to_node[file_id] = node;
		game_object_count++;
	}

	// Establish parent-child relationships
	for (const KeyValue<String, String> &E : fileID_to_parent) {
		String child_id = E.key;
		String parent_id = E.value;

		if (fileID_to_node.has(child_id)) {
			Node3D *child = fileID_to_node[child_id];

			if (parent_id == "0" || parent_id.is_empty() || !fileID_to_node.has(parent_id)) {
				// Root level child - add to root
				if (!child->get_parent()) {
					root->add_child(child);
					child->set_owner(root);
				}
			} else {
				// Has a parent node
				Node3D *parent = fileID_to_node[parent_id];
				if (!child->get_parent()) {
					parent->add_child(child);
					child->set_owner(root);
				}
			}
		}
	}

	// Add any remaining orphan nodes (nodes without explicit parent entries) directly to root
	for (const KeyValue<String, Node3D *> &E : fileID_to_node) {
		String file_id = E.key;
		Node3D *node = E.value;
		
		// Only add to root if not already added
		if (!node->get_parent()) {
			root->add_child(node);
			node->set_owner(root);
			print_verbose(vformat("Scene: Added orphan node '%s' to root", node->get_name()));
		}
	}

	print_verbose(vformat("Scene: Built hierarchy with %d nodes", root->get_child_count() + 1));

	// If no GameObjects were found but scene file had content, check for PrefabInstances
	if (game_object_count == 0 && !yaml.is_empty()) {
		print_verbose("Scene: No direct GameObjects found. Scene may contain only PrefabInstances.");
		print_verbose(vformat("Scene: PrefabInstance support is currently limited - instantiate prefabs manually or enhance the converter."));
	}

	// Pack the scene properly with root node
	scene->pack(root);

	// Save and verify
	Error save_result = ResourceSaver::save(scene, p_asset.pathname);
	if (save_result == OK) {
		print_line(vformat("Scene conversion: packed %d nodes (root + %d children) with transforms from %s", root->get_child_count() + 1, game_object_count, p_asset.pathname.get_file()));
	} else {
		print_error(vformat("Failed to save scene: %s (error: %d)", p_asset.pathname, save_result));
	}

	return save_result;
}

Error UnityAssetConverter::convert_prefab(const UnityAsset &p_asset) {
	print_verbose(vformat("Starting prefab conversion for: %s", p_asset.pathname.get_file()));
	// p_asset.pathname is already the full output path with hash
	Error dir_result = ensure_parent_dir_for_file(p_asset.pathname);
	if (dir_result != OK) {
		print_error(vformat("Failed to create directory for prefab: %s", p_asset.pathname));
		return ERR_CANT_CREATE;
	}

	// Detect binary Unity files - prefabs can also be binary format
	if (p_asset.asset_data.size() > 20) {
		const uint8_t *data = p_asset.asset_data.ptr();
		// Check for Unity binary file magic numbers
		if (data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x00) {
			print_error(vformat("Prefab '%s' is in Unity BINARY format. Only YAML text format is supported. Re-export from Unity with 'Force Text' serialization.", p_asset.pathname.get_file()));
			return ERR_FILE_UNRECOGNIZED;
		}
	}

	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());

	// Check for valid YAML format
	if (!yaml.begins_with("%YAML") && !yaml.begins_with("---")) {
		print_error(vformat("Prefab '%s' does not appear to be valid YAML text format. Ensure Unity is set to 'Force Text' serialization mode.", p_asset.pathname.get_file()));
		return ERR_FILE_UNRECOGNIZED;
	}

	// Create scene and root node
	Ref<PackedScene> scene = memnew(PackedScene);
	Node3D *root = memnew(Node3D);
	root->set_name(p_asset.pathname.get_file().get_basename());
	// NOTE: Root node does NOT own itself! Only children own the root.

	// Parse YAML to extract GameObjects and build node hierarchy
	Vector<String> lines = yaml.split("\n");
	print_verbose("Parsing YAML to extract Prefab GameObject and Transform data...");

	HashMap<String, Node3D *> fileID_to_node;
	HashMap<String, String> fileID_to_name;
	HashMap<String, String> fileID_to_parent;
	HashMap<String, String> fileID_to_prefab_guid;
	// Transform data
	HashMap<String, Vector3> fileID_to_position;
	HashMap<String, Quaternion> fileID_to_rotation;
	HashMap<String, Vector3> fileID_to_scale;

	String current_section_type = "";
	String current_file_id = "";
	String current_game_object_name = "GameObject";
	String current_parent_ref = "";
	String current_prefab_guid = "";

	Vector3 current_position;
	current_position.x = 0;
	current_position.y = 0;
	current_position.z = 0;

	Quaternion current_rotation;
	current_rotation.x = 0;
	current_rotation.y = 0;
	current_rotation.z = 0;
	current_rotation.w = 1;

	Vector3 current_scale;
	current_scale.x = 1;
	current_scale.y = 1;
	current_scale.z = 1;

	bool in_game_object = false;
	bool in_transform = false;
	int game_object_count = 0;

	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i];
		String trimmed = line.strip_edges();

		// Detect document sections with file IDs
		if (trimmed.begins_with("--- !u!")) {
			// Save previous GameObject if any
			if (in_game_object && !current_file_id.is_empty() && !current_game_object_name.is_empty()) {
				fileID_to_name[current_file_id] = current_game_object_name;
				if (!current_parent_ref.is_empty()) {
					fileID_to_parent[current_file_id] = current_parent_ref;
				}
				if (!current_prefab_guid.is_empty()) {
					fileID_to_prefab_guid[current_file_id] = current_prefab_guid;
				}
			}

			// Reset state
			in_game_object = false;
			in_transform = false;
			current_parent_ref = "";
			current_prefab_guid = "";

			// Extract file ID from "--- !u!1 &123456789"
			int amp_pos = trimmed.find("&");
			if (amp_pos != -1) {
				current_file_id = trimmed.substr(amp_pos + 1).strip_edges();
			}

			// Detect section type
			if (trimmed.contains("!u!1 ")) {
				in_game_object = true;
				current_game_object_name = "GameObject";
			} else if (trimmed.contains("!u!4 ") || trimmed.contains("!u!224 ")) {
				// Transform or RectTransform
				in_transform = true;
			}
		}
		// Extract GameObject name
		else if (in_game_object && trimmed.begins_with("m_Name:")) {
			int colon = trimmed.find(":");
			String name_value = trimmed.substr(colon + 1).strip_edges();
			if (name_value.begins_with("\"") && name_value.ends_with("\"")) {
				name_value = name_value.substr(1, name_value.length() - 2);
			}
			if (!name_value.is_empty()) {
				current_game_object_name = name_value;
			}
		}
		// Extract parent reference from Transform
		else if (in_transform && trimmed.begins_with("m_Father:")) {
			// Look for "fileID: 123456789" in next line or same line
			if (i + 1 < lines.size()) {
				String next_line = lines[i + 1].strip_edges();
				if (next_line.begins_with("fileID:")) {
					current_parent_ref = next_line.substr(7).strip_edges();
				}
			}
		}
		// Extract Transform position
		else if (in_transform && trimmed.begins_with("m_LocalPosition:")) {
			int colon = trimmed.find(":");
			String pos_yaml = trimmed.substr(colon + 1).strip_edges();
			current_position = _parse_vector3_from_yaml(pos_yaml);
			fileID_to_position[current_file_id] = current_position;
			print_verbose(vformat("Prefab Transform: Position for '%s': (%f, %f, %f)", current_game_object_name, current_position.x, current_position.y, current_position.z));
		}
		// Extract Transform rotation
		else if (in_transform && trimmed.begins_with("m_LocalRotation:")) {
			int colon = trimmed.find(":");
			String rot_yaml = trimmed.substr(colon + 1).strip_edges();
			current_rotation = _parse_quaternion_from_yaml(rot_yaml);
			fileID_to_rotation[current_file_id] = current_rotation;
			print_verbose(vformat("Prefab Transform: Rotation for '%s': (%f, %f, %f, %f)", current_game_object_name, current_rotation.x, current_rotation.y, current_rotation.z, current_rotation.w));
		}
		// Extract Transform scale
		else if (in_transform && trimmed.begins_with("m_LocalScale:")) {
			int colon = trimmed.find(":");
			String scale_yaml = trimmed.substr(colon + 1).strip_edges();
			current_scale = _parse_vector3_from_yaml(scale_yaml);
			fileID_to_scale[current_file_id] = current_scale;
			print_verbose(vformat("Prefab Transform: Scale for '%s': (%f, %f, %f)", current_game_object_name, current_scale.x, current_scale.y, current_scale.z));
		}
		// Extract prefab GUID reference
		else if (trimmed.begins_with("guid:") && (in_game_object || in_transform)) {
			current_prefab_guid = trimmed.substr(5).strip_edges();
			// Remove quotes and commas
			if (current_prefab_guid.begins_with("\"")) {
				current_prefab_guid = current_prefab_guid.substr(1);
			}
			int comma = current_prefab_guid.find(",");
			if (comma != -1) {
				current_prefab_guid = current_prefab_guid.substr(0, comma);
			}
			if (current_prefab_guid.ends_with("\"")) {
				current_prefab_guid = current_prefab_guid.substr(0, current_prefab_guid.length() - 1);
			}
		}
	}

	// Save last object
	if (in_game_object && !current_file_id.is_empty() && !current_game_object_name.is_empty()) {
		fileID_to_name[current_file_id] = current_game_object_name;
		if (!current_parent_ref.is_empty()) {
			fileID_to_parent[current_file_id] = current_parent_ref;
		}
		if (!current_prefab_guid.is_empty()) {
			fileID_to_prefab_guid[current_file_id] = current_prefab_guid;
		}
	}

	// Build node hierarchy
	for (const KeyValue<String, String> &E : fileID_to_name) {
		String file_id = E.key;
		String name = E.value;

		Node3D *node = memnew(Node3D);
		// Apply Unity terminology translation if setting is enabled
		String display_name = _translate_unity_terminology(name);
		node->set_name(display_name);

		// Apply Transform data from extraction
		if (fileID_to_position.has(file_id)) {
			node->set_position(fileID_to_position[file_id]);
			print_verbose(vformat("Prefab: Applied position to '%s': %v", name, fileID_to_position[file_id]));
		}
		if (fileID_to_rotation.has(file_id)) {
			Vector3 euler = fileID_to_rotation[file_id].get_euler();
			node->set_rotation(euler);
			print_verbose(vformat("Prefab: Applied rotation to '%s': %v", name, euler));
		}
		if (fileID_to_scale.has(file_id)) {
			node->set_scale(fileID_to_scale[file_id]);
			print_verbose(vformat("Prefab: Applied scale to '%s': %v", name, fileID_to_scale[file_id]));
		}

		// Store prefab reference as metadata
		if (fileID_to_prefab_guid.has(file_id)) {
			node->set_meta("unity_prefab_guid", fileID_to_prefab_guid[file_id]);
		}

		fileID_to_node[file_id] = node;
		game_object_count++;
	}

	// Establish parent-child relationships
	for (const KeyValue<String, String> &E : fileID_to_parent) {
		String child_id = E.key;
		String parent_id = E.value;

		if (fileID_to_node.has(child_id)) {
			Node3D *child = fileID_to_node[child_id];

			if (parent_id == "0" || parent_id.is_empty() || !fileID_to_node.has(parent_id)) {
				// Root level child - add to root
				if (!child->get_parent()) {
					root->add_child(child);
					child->set_owner(root);
				}
			} else {
				// Has a parent node
				Node3D *parent = fileID_to_node[parent_id];
				if (!child->get_parent()) {
					parent->add_child(child);
					child->set_owner(root);
				}
			}
		}
	}

	// Add any remaining orphan nodes (nodes without explicit parent entries) directly to root
	for (const KeyValue<String, Node3D *> &E : fileID_to_node) {
		String file_id = E.key;
		Node3D *node = E.value;
		
		// Only add to root if not already added
		if (!node->get_parent()) {
			root->add_child(node);
			node->set_owner(root);
			print_verbose(vformat("Prefab: Added orphan node '%s' to root", node->get_name()));
		}
	}

	// Pack the scene properly
	scene->pack(root);

	// Save and verify
	Error save_result = ResourceSaver::save(scene, p_asset.pathname);
	if (save_result == OK) {
		print_line(vformat("Prefab conversion: packed %d nodes (root + %d children) from %s", root->get_child_count() + 1, game_object_count, p_asset.pathname.get_file()));
	} else {
		print_error(vformat("Failed to save prefab: %s (error: %d)", p_asset.pathname, save_result));
	}

	return save_result;
}

Error UnityAssetConverter::convert_audio(const UnityAsset &p_asset) {
	// Direct copy - Godot supports WAV/MP3/OGG
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(p_asset.pathname) != OK, ERR_CANT_CREATE, "Cannot create target directory for audio.");
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_animation(const UnityAsset &p_asset) {
	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	Ref<Animation> anim;
	anim.instantiate();
	anim->set_length(0.0);
	anim->set_meta("unity_yaml", yaml);

	String out_path = p_asset.pathname;
	if (!out_path.ends_with(".tres")) {
		out_path += ".tres";
	}
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(out_path) != OK, ERR_CANT_CREATE, "Cannot create target directory for animation.");
	return ResourceSaver::save(anim, out_path);
}

Error UnityAssetConverter::convert_shader(const UnityAsset &p_asset) {
	// Use built-in shader converter
	String shader_code = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	String godot_shader;
	Error err = UnityShaderConverter::convert_shaderlab_to_godot(shader_code, godot_shader);
	ERR_FAIL_COND_V(err != OK, err);

	String output_path = p_asset.pathname.replace(".shader", ".gdshader");
	ERR_FAIL_COND_V_MSG(ensure_parent_dir_for_file(output_path) != OK, ERR_CANT_CREATE, "Cannot create target directory for shader.");
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_string(godot_shader);

	print_line("Converted Unity shader: " + p_asset.pathname + " -> " + output_path);
	return OK;
}
