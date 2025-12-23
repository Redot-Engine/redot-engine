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
	print_line(vformat("convert_scene called with pathname: %s", p_asset.pathname));
	// p_asset.pathname is already the full output path with hash
	Error dir_result = ensure_parent_dir_for_file(p_asset.pathname);
	if (dir_result != OK) {
		print_error(vformat("Cannot create target directory for scene. Path: %s, Error: %d", p_asset.pathname, dir_result));
		return ERR_CANT_CREATE;
	}

	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	
	// Create scene and root node
	Ref<PackedScene> scene = memnew(PackedScene);
	Node3D *root = memnew(Node3D);
	root->set_name(p_asset.pathname.get_file().get_basename());
	root->set_owner(root);  // Root node owns itself in Redot scenes
	
	// Parse YAML to extract GameObjects and build node hierarchy
	Vector<String> lines = yaml.split("\n");
	String current_game_object_name = "GameObject";
	bool in_game_object = false;
	
	for (int i = 0; i < lines.size(); i++) {
		String trimmed = lines[i].strip_edges();
		
		// Detect GameObject sections (type ID 1)
		if (trimmed.contains("--- !u!1")) {
			// If we were in a previous GameObject, create node
			if (in_game_object && !current_game_object_name.is_empty()) {
				Node3D *child = memnew(Node3D);
				child->set_name(current_game_object_name);
				root->add_child(child);
				child->set_owner(root);
			}
			
			in_game_object = true;
			current_game_object_name = "GameObject";
		}
		// End of current GameObject section (any other --- marker)
		else if (in_game_object && trimmed.begins_with("---")) {
			in_game_object = false;
		}
		
		// Extract GameObject name
		if (in_game_object && trimmed.begins_with("m_Name:")) {
			int colon = trimmed.find(":");
			String name_value = trimmed.substr(colon + 1).strip_edges();
			
			// Remove quotes if present
			if (name_value.begins_with("\"") && name_value.ends_with("\"")) {
				name_value = name_value.substr(1, name_value.length() - 2);
			}
			
			if (!name_value.is_empty()) {
				current_game_object_name = name_value;
			}
		}
	}
	
	// Add final GameObject if needed
	if (in_game_object && !current_game_object_name.is_empty()) {
		Node3D *child = memnew(Node3D);
		child->set_name(current_game_object_name);
		root->add_child(child);
		child->set_owner(root);
	}
	
	// Pack the scene properly with root node
	scene->pack(root);
	
	// Save and verify
	Error save_result = ResourceSaver::save(scene, p_asset.pathname);
	if (save_result == OK) {
		print_line(vformat("Scene conversion: packed %d nodes from %s", root->get_child_count() + 1, p_asset.pathname.get_file()));
	} else {
		print_error(vformat("Failed to save scene: %s (error: %d)", p_asset.pathname, save_result));
	}
	
	return save_result;
}

Error UnityAssetConverter::convert_prefab(const UnityAsset &p_asset) {
	// p_asset.pathname is already the full output path with hash
	print_line(vformat("convert_prefab called with pathname: %s", p_asset.pathname));
	Error dir_result = ensure_parent_dir_for_file(p_asset.pathname);
	if (dir_result != OK) {
		print_error(vformat("Cannot create target directory for prefab. Path: %s, Error: %d", p_asset.pathname, dir_result));
		return ERR_CANT_CREATE;
	}

	String yaml = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	
	// Create scene and root node
	Ref<PackedScene> scene = memnew(PackedScene);
	Node3D *root = memnew(Node3D);
	root->set_name(p_asset.pathname.get_file().get_basename());
	root->set_owner(root);  // Root node owns itself in Redot scenes
	
	// Parse YAML to extract GameObjects and build node hierarchy
	Vector<String> lines = yaml.split("\n");
	String current_game_object_name = "GameObject";
	bool in_game_object = false;
	
	for (int i = 0; i < lines.size(); i++) {
		String trimmed = lines[i].strip_edges();
		
		// Detect GameObject sections (type ID 1)
		if (trimmed.contains("--- !u!1")) {
			// If we were in a previous GameObject, create node
			if (in_game_object && !current_game_object_name.is_empty()) {
				Node3D *child = memnew(Node3D);
				child->set_name(current_game_object_name);
				root->add_child(child);
				child->set_owner(root);
			}
			
			in_game_object = true;
			current_game_object_name = "GameObject";
		}
		// End of current GameObject section (any other --- marker)
		else if (in_game_object && trimmed.begins_with("---")) {
			in_game_object = false;
		}
		
		// Extract GameObject name
		if (in_game_object && trimmed.begins_with("m_Name:")) {
			int colon = trimmed.find(":");
			String name_value = trimmed.substr(colon + 1).strip_edges();
			
			// Remove quotes if present
			if (name_value.begins_with("\"") && name_value.ends_with("\"")) {
				name_value = name_value.substr(1, name_value.length() - 2);
			}
			
			if (!name_value.is_empty()) {
				current_game_object_name = name_value;
			}
		}
	}
	
	// Add final GameObject if needed
	if (in_game_object && !current_game_object_name.is_empty()) {
		Node3D *child = memnew(Node3D);
		child->set_name(current_game_object_name);
		root->add_child(child);
		child->set_owner(root);
	}
	
	// Pack the scene properly
	scene->pack(root);
	
	// Save and verify
	Error save_result = ResourceSaver::save(scene, p_asset.pathname);
	if (save_result == OK) {
		print_line(vformat("Prefab conversion: packed %d nodes from %s", root->get_child_count() + 1, p_asset.pathname.get_file()));
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
