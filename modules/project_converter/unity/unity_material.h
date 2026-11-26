/**************************************************************************/
/*  unity_material.h                                                      */
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
#include "scene/resources/material.h"
#include "scene/resources/texture.h"

#include "unity_project.h"
#include "unity_yaml.h"

/// Builds Godot materials from Unity `.mat` assets and shader references.
///
/// Two paths are supported:
///  * Unity's built-in shaders become a `StandardMaterial3D` with the familiar
///    `_Color`/`_MainTex`/`_Metallic`/`_Glossiness` properties mapped.
///  * Project shaders (`*.shader`) are translated to Godot shader code and
///    wrapped in a `ShaderMaterial`, with the serialized parameters applied.
///
/// The builder never fails hard: unknown shaders fall back to the standard
/// material and a warning is recorded by the caller.
class UnityMaterialBuilder {
public:
	/// `p_target_prefix` is prepended to resource paths (e.g. `res://`) so the
	/// same code can build materials for direct-open and for baked projects.
	static Ref<Material> build_from_document(Ref<UnityProject> p_project, const String &p_material_path, const UnityYamlDocument &p_document, const String &p_target_prefix, Vector<String> *r_warnings = nullptr);

	/// Builds a material from a `.mat` file on disk.
	static Ref<Material> build_from_file(Ref<UnityProject> p_project, const String &p_material_path, const String &p_target_prefix, Vector<String> *r_warnings = nullptr);

	/// Loads a texture referenced by a Unity GUID, or returns null.
	static Ref<Texture2D> load_texture(Ref<UnityProject> p_project, const String &p_guid, const String &p_target_prefix);

	/// Converts a project `.shader` asset into Godot shader code.
	static Error convert_shader(Ref<UnityProject> p_project, const String &p_shader_path, String &r_code);

	/// Clears the per-project caches (call when a project is rescanned).
	static void clear_cache();
};
