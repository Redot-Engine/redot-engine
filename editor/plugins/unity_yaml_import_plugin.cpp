/**************************************************************************/
/*  unity_yaml_import_plugin.cpp                                          */
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

#include "unity_yaml_import_plugin.h"

#include "core/io/file_access.h"
#include "core/string/print_string.h"

static Error _read_file_bytes(const String &p_path, PackedByteArray &r_bytes) {
    Error fe = OK;
    String text = FileAccess::get_file_as_string(p_path, &fe);
    if (fe != OK) {
        return fe;
    }
    r_bytes.resize(text.length());
    memcpy(r_bytes.ptrw(), text.utf8().get_data(), text.length());
    return OK;
}

Error UnityAnimImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
    PackedByteArray bytes;
    Error r = _read_file_bytes(p_source_file, bytes);
    if (r != OK) {
        return r;
    }

    UnityAsset asset;
    asset.pathname = p_save_path; // converter appends .tres
    asset.asset_data = bytes;

    return UnityAssetConverter::convert_animation(asset);
}

Error UnityYamlSceneImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
    PackedByteArray bytes;
    Error r = _read_file_bytes(p_source_file, bytes);
    if (r != OK) {
        return r;
    }

    UnityAsset asset;
    asset.pathname = p_save_path; // converter appends .tscn
    asset.asset_data = bytes;

    String ext = p_source_file.get_extension().to_lower();
    if (ext == "prefab") {
        return UnityAssetConverter::convert_prefab(asset);
    }
    return UnityAssetConverter::convert_scene(asset);
}

Error UnityMatImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
    PackedByteArray bytes;
    Error r = _read_file_bytes(p_source_file, bytes);
    if (r != OK) {
        return r;
    }

    UnityAsset asset;
    asset.pathname = p_save_path; // converter appends .tres
    asset.asset_data = bytes;

    HashMap<String, UnityAsset> dummy_all; // no cross-asset lookup in direct import
    return UnityAssetConverter::convert_material(asset, dummy_all);
}
