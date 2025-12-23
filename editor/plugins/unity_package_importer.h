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
#include "scene/3d/node_3d.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/animation.h"
#include "scene/resources/mesh.h"
#include "scene/resources/material.h"
#include "unity_shader_converter.h"

// Forward declarations
class Node3D;
class PackedScene;
class Animation;
class Mesh;
class Material;

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

// Mesh data structures
struct UnityMeshData {
	Vector<Vector3> vertices;
	Vector<Vector3> normals;
	Vector<Vector2> texcoords;
	Vector<Vector2> texcoords2;
	Vector<Color> colors;
	Vector<Vector4> tangents;
	Vector<Vector<int>> bone_indices;      // Per-vertex bone indices
	Vector<Vector<float>> bone_weights;    // Per-vertex bone weights
	HashMap<int, Vector<int>> submesh_triangles;  // Submesh index -> triangle list
};

// Animation track data
struct UnityAnimationTrack {
	String path;
	String property;
	Vector<float> times;
	Vector<Quaternion> rotations;
	Vector<Vector3> positions;
	Vector<float> scales;
};

// Scene/Prefab node data
struct UnitySceneNode {
	String name;
	int fileID;
	Transform3D transform;
	Vector<int> children;
	int parent_fileID;
	HashMap<String, Variant> components;  // Component type -> fileID
	bool is_prefab_instance;
	int prefab_source_id;
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

	// Advanced converters
	static Ref<Mesh> convert_mesh_data(const UnityMeshData &p_mesh_data);
	static Ref<Material> convert_standard_material(const HashMap<String, Variant> &p_properties);
	static Ref<Animation> convert_animation_clip(const UnityAnimationTrack &p_track_data);
	static Ref<PackedScene> convert_scene_hierarchy(const Vector<UnitySceneNode> &p_nodes);

private:
	// YAML parsing helpers
	static Vector3 _parse_vector3_from_yaml(const String &p_yaml);
	static Quaternion _parse_quaternion_from_yaml(const String &p_yaml);
	static Transform3D _parse_transform_from_yaml(const String &p_yaml);
	static Color _parse_color_from_yaml(const String &p_yaml);
	
	// Mesh data extraction
	static Error _extract_mesh_data(const HashMap<String, Variant> &p_yaml, UnityMeshData &r_mesh_data);
	static Error _extract_skin_data(const HashMap<String, Variant> &p_yaml, UnityMeshData &r_mesh_data);
	
	// Material property parsing
	static void _parse_material_properties(const HashMap<String, Variant> &p_yaml, HashMap<String, Variant> &r_properties);
	
	// Scene hierarchy building
	static Error _build_scene_hierarchy(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets, Vector<UnitySceneNode> &r_nodes);
	
	// Component converters
	static Node3D* _convert_transform_component(const UnitySceneNode &p_node);
	static Node3D* _convert_mesh_renderer(const UnityAsset &p_asset, const HashMap<String, UnityAsset> &p_all_assets);
	static Node3D* _convert_light_component(const HashMap<String, Variant> &p_props);
	static Node3D* _convert_camera_component(const HashMap<String, Variant> &p_props);
	static Node3D* _convert_rigidbody_component(const HashMap<String, Variant> &p_props);
	static Node3D* _convert_collider_component(const HashMap<String, Variant> &p_props);

	// Utility
	static String _translate_unity_terminology(const String &p_text);
	static String _translate_shader_keyword(const String &p_keyword);
	static float _linear_to_srgb_single(float p_linear);
	static Color _linear_to_srgb(const Color &p_linear_color);
};
