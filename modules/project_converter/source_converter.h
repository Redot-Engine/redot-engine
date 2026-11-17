/**************************************************************************/
/*  source_converter.h                                                    */
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
#include "core/math/color.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

// Represents a single node in the Valve KeyValue tree.
// Leaf nodes have is_block=false and a string value.
// Block nodes have is_block=true and children.
struct VKVNode {
	String key;
	String value;
	Vector<VKVNode> children;
	bool is_block = false;
};

// Converts Source Engine / S&BOX assets (VMT, VMF, SMD) to Godot resources.
class SourceConverter {
public:
	// Convert a .vmf map file to a Godot .tscn scene
	static Error convert_vmf(const String &p_vmf_path, const String &p_output_dir);
	// Convert a .vmt or .vmat material to a Godot .tres StandardMaterial3D
	static Error convert_vmt(const String &p_vmt_path, const String &p_output_dir);
	// Convert a .smd mesh to a Godot .obj mesh file
	static Error convert_smd(const String &p_smd_path, const String &p_output_dir);
	// Parse Valve KeyValue text format into a node tree
	static Vector<VKVNode> parse_vkv(const String &p_content);

private:
	// Look up a key's value in a flat list of VKV nodes (case-insensitive)
	static String _vkv_get(const Vector<VKVNode> &p_nodes, const String &p_key, const String &p_default = "");
	// Find the first block child matching p_key (case-insensitive)
	static const VKVNode *_vkv_find_block(const Vector<VKVNode> &p_nodes, const String &p_key);
	// Parse a VKV color value "[R G B]" or "R G B" into a Color
	static Color _parse_vkv_color(const String &p_value);
	// Parse a VKV vector value "[X Y Z]" or "X Y Z" into a Vector3
	static Vector3 _parse_vkv_vector3(const String &p_value);
	// Compute a plane normal string from the three-point VMF plane definition
	static String _vmf_plane_normal(const String &p_plane_str);
	// Generate a .tres StandardMaterial3D text resource
	static String _generate_vmt_tres(const String &p_shader_type, const Vector<VKVNode> &p_params);
	// Generate a .tscn scene from parsed VMF world brushes and entities
	static String _generate_vmf_tscn(const Vector<VKVNode> &p_world_nodes, const Vector<VKVNode> &p_entities);
};

#endif // TOOLS_ENABLED
