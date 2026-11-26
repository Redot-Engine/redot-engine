/**************************************************************************/
/*  unity_scene.h                                                         */
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

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "scene/resources/packed_scene.h"

#include "unity_project.h"

/// Options controlling how a Unity scene or prefab is materialized.
struct UnitySceneOptions {
	/// Prefix applied to resource paths. Direct-open uses `res://`, baking uses
	/// `res://` in the destination project as well.
	String target_prefix = "res://";
	/// Directory that receives converted GDScript files. Empty disables script
	/// conversion (the scene then carries metadata only).
	String script_dir;
	/// Resource path that corresponds to `script_dir`, e.g. `user://.../scripts`.
	String script_res_dir;
	/// Whether to convert C# scripts.
	bool convert_scripts = true;
	/// Whether inactive GameObjects are kept (hidden) or dropped.
	bool include_inactive = true;
	/// Recursion guard for prefab instancing.
	int max_prefab_depth = 8;
};

struct UnitySceneResult {
	Ref<PackedScene> scene;
	Vector<String> warnings;
	int node_count = 0;
	int script_count = 0;
};

/// Converts Unity `.unity` scenes and `.prefab` assets into `PackedScene`s.
///
/// The builder walks Unity's flat YAML object graph (GameObject, Transform,
/// components, PrefabInstance) and produces a real node hierarchy: transforms,
/// meshes with materials, cameras, lights, colliders, rigid bodies, audio
/// sources and converted scripts. Prefab instances are expanded inline so the
/// result is self-contained and works both for direct-open and for baking.
class UnitySceneBuilder {
public:
	static Error build(Ref<UnityProject> p_project, const String &p_scene_path, const UnitySceneOptions &p_options, UnitySceneResult &r_result);

	/// Converts a single MonoBehaviour C# file into GDScript source, rewriting
	/// constructs that would not compile in isolation (unknown base classes,
	/// `class_name` collisions).
	static String convert_script_source(const String &p_csharp, const String &p_expected_class, String *r_class_name = nullptr);

	static void clear_cache();
};
