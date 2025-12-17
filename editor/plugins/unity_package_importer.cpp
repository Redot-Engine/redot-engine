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

#include "core/io/image.h"
#include "core/string/print_string.h"

// Unity package parser implementation (ported from V-Sekai/unidot_importer)

Error UnityPackageParser::parse_unitypackage(const String &p_path, HashMap<String, UnityAsset> &r_assets) {
	// Read compressed .unitypackage file
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_OPEN, "Cannot open Unity package file.");

	PackedByteArray compressed = file->get_buffer(file->get_length());
	file.unref();

	// Decompress gzip
	PackedByteArray tar_data = compressed.decompress_dynamic(-1, FileAccess::COMPRESSION_GZIP);
	ERR_FAIL_COND_V_MSG(tar_data.is_empty(), ERR_FILE_CORRUPT, "Failed to decompress Unity package.");

	return parse_tar_archive(tar_data, r_assets);
}

Error UnityPackageParser::parse_tar_archive(const PackedByteArray &p_tar_data, HashMap<String, UnityAsset> &r_assets) {
	int offset = 0;
	HashMap<String, UnityAsset *> guid_map;

	while (offset + 512 <= p_tar_data.size()) {
		const uint8_t *header = p_tar_data.ptr() + offset;

		// Check for end of archive
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

		// Parse filename (offset 0, 100 bytes)
		char name_buf[101] = {};
		memcpy(name_buf, header, 100);
		String entry_name = String::utf8(name_buf);

		// Parse file size (offset 124, 12 bytes octal)
		char size_buf[13] = {};
		memcpy(size_buf, header + 124, 12);
		int64_t file_size = 0;
		// Parse octal size
		for (int i = 0; i < 12; i++) {
			if (size_buf[i] >= '0' && size_buf[i] <= '7') {
				file_size = file_size * 8 + (size_buf[i] - '0');
			}
		}

		offset += 512;

		// Extract file data
		if (file_size > 0 && offset + file_size <= p_tar_data.size()) {
			PackedByteArray entry_data;
			entry_data.resize(file_size);
			memcpy(entry_data.ptrw(), p_tar_data.ptr() + offset, file_size);

			// Parse entry structure: <guid>/asset, <guid>/pathname, <guid>/asset.meta
			Vector<String> parts = entry_name.split("/");
			if (parts.size() >= 2) {
				String guid = parts[0];
				String entry_type = parts[1];

				if (!guid_map.has(guid)) {
					UnityAsset asset;
					asset.guid = guid;
					r_assets[guid] = asset;
					guid_map[guid] = &r_assets[guid];
				}

				UnityAsset *asset = guid_map[guid];

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

			// Move to next 512-byte boundary
			offset += ((file_size + 511) / 512) * 512;
		}
	}

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

Error UnityAssetConverter::extract_asset(const UnityAsset &p_asset) {
	if (p_asset.asset_data.is_empty()) {
		return ERR_FILE_MISSING_DEPENDENCIES;
	}

	String ext = p_asset.pathname.get_extension().to_lower();

	// Route to appropriate converter
	if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff") {
		return convert_texture(p_asset);
	} else if (ext == "mat") {
		return convert_material(p_asset);
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
	// Direct copy - Godot can import images natively
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_material(const UnityAsset &p_asset) {
	// TODO: Parse Unity material YAML and convert to Godot StandardMaterial3D/ShaderMaterial
	print_line("Material conversion not yet implemented: " + p_asset.pathname);
	return ERR_SKIP;
}

Error UnityAssetConverter::convert_model(const UnityAsset &p_asset) {
	// Direct copy - Godot can import FBX/OBJ/DAE natively
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_scene(const UnityAsset &p_asset) {
	// TODO: Parse Unity scene YAML and convert to Godot .tscn
	print_line("Scene conversion not yet implemented: " + p_asset.pathname);
	return ERR_SKIP;
}

Error UnityAssetConverter::convert_prefab(const UnityAsset &p_asset) {
	// TODO: Parse Unity prefab YAML and convert to Godot .tscn
	print_line("Prefab conversion not yet implemented: " + p_asset.pathname);
	return ERR_SKIP;
}

Error UnityAssetConverter::convert_audio(const UnityAsset &p_asset) {
	// Direct copy - Godot supports WAV/MP3/OGG
	Ref<FileAccess> f = FileAccess::open(p_asset.pathname, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_buffer(p_asset.asset_data);
	return OK;
}

Error UnityAssetConverter::convert_animation(const UnityAsset &p_asset) {
	// TODO: Parse Unity animation YAML and convert to Godot Animation
	print_line("Animation conversion not yet implemented: " + p_asset.pathname);
	return ERR_SKIP;
}

Error UnityAssetConverter::convert_shader(const UnityAsset &p_asset) {
	// Use built-in shader converter
	String shader_code = String::utf8((const char *)p_asset.asset_data.ptr(), p_asset.asset_data.size());
	String godot_shader = UnityShaderConverter::convert_shaderlab_to_godot(shader_code);

	String output_path = p_asset.pathname.replace(".shader", ".gdshader");
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	f->store_string(godot_shader);

	print_line("Converted Unity shader: " + p_asset.pathname + " -> " + output_path);
	return OK;
}
