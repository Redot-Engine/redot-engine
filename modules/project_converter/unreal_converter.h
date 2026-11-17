/**************************************************************************/
/*  unreal_converter.h                                                    */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/math/quaternion.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"

// Describes a single Unreal actor parsed from a T3D level file.
struct UnrealActor {
	String class_name;
	String name;
	String label;
	Transform3D transform;
	HashMap<String, String> properties;
	Vector<HashMap<String, String>> components;
};

// Converts Unreal Engine project files (.uproject, T3D, .ini) to Godot resources.
class UnrealConverter {
public:
	// Parse a .uproject JSON file and generate a summary .tscn listing the maps
	static Error convert_uproject(const String &p_uproject_path, const String &p_output_dir);
	// Convert a T3D text export of an Unreal level to a Godot .tscn scene
	static Error convert_t3d(const String &p_t3d_path, const String &p_output_dir);
	// Parse T3D content into UnrealActor structures
	static Vector<UnrealActor> parse_t3d(const String &p_content);
	// Parse an Unreal .ini config file into section→key→value maps
	static HashMap<String, HashMap<String, String>> parse_ini(const String &p_content);

private:
	// Build a Transform3D from actor property map (RelativeLocation/Rotation/Scale3D)
	static Transform3D _parse_unreal_transform(const HashMap<String, String> &p_props);
	// Parse "(X=1.0,Y=2.0,Z=3.0)" style vector
	static Vector3 _parse_unreal_vector(const String &p_value);
	// Parse "(Pitch=0.0,Yaw=90.0,Roll=0.0)" rotator into a Quaternion
	static Quaternion _parse_unreal_rotator(const String &p_value);
	// Map Unreal class paths to Godot node type strings
	static String _unreal_class_to_godot_type(const String &p_class);
	// Generate .tscn text from a list of UnrealActors
	static String _generate_t3d_tscn(const Vector<UnrealActor> &p_actors);
};

#endif // TOOLS_ENABLED
