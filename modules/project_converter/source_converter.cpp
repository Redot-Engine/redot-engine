/**************************************************************************/
/*  source_converter.cpp                                                  */
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

#include "source_converter.h"

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/color.h"
#include "core/math/vector3.h"
#include "core/string/char_utils.h"
#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"

// ---------------------------------------------------------------------------
// VKV parser
// ---------------------------------------------------------------------------

// Tokenise a single line into key and optional value, respecting quotes.
// Returns false if the line is a comment or empty.
static bool _vkv_parse_line(const String &p_line, String &r_key, String &r_value) {
	// Strip leading/trailing whitespace
	String line = p_line.strip_edges();
	if (line.is_empty() || line.begins_with("//")) {
		return false;
	}

	// Extract quoted or unquoted tokens
	Vector<String> tokens;
	int i = 0;
	while (i < line.length()) {
		char32_t c = line[i];
		if (c == ' ' || c == '\t') {
			i++;
			continue;
		}
		if (c == '/' && i + 1 < line.length() && line[i + 1] == '/') {
			break; // Rest of line is a comment
		}
		if (c == '"') {
			// Quoted token
			int start = i + 1;
			int end = start;
			while (end < line.length() && line[end] != '"') {
				end++;
			}
			tokens.push_back(line.substr(start, end - start));
			i = end + 1;
		} else {
			// Unquoted token (ends at whitespace or '{' or '}')
			int start = i;
			while (i < line.length() && line[i] != ' ' && line[i] != '\t' &&
					line[i] != '"') {
				i++;
			}
			tokens.push_back(line.substr(start, i - start));
		}
	}

	if (tokens.is_empty()) {
		return false;
	}
	r_key = tokens[0];
	r_value = (tokens.size() > 1) ? tokens[1] : String();
	return true;
}

Vector<VKVNode> SourceConverter::parse_vkv(const String &p_content) {
	Vector<VKVNode> result;
	// Use a stack to handle nested blocks.
	// stack[i] holds a pointer-equivalent: we use indices into a flat pool.
	// Simple approach: maintain a Vector<VKVNode*> stack using a local list.

	struct ParseCtx {
		Vector<VKVNode> *dest = nullptr;
		String pending_key;
	};

	Vector<ParseCtx> stack;
	ParseCtx root_ctx;
	root_ctx.dest = &result;
	stack.push_back(root_ctx);

	Vector<String> lines = p_content.split("\n");
	for (int li = 0; li < lines.size(); li++) {
		String line = lines[li].strip_edges();
		if (line.is_empty() || line.begins_with("//")) {
			continue;
		}

		if (line == "{") {
			// Begin block: create a new block node under the current destination
			ParseCtx &cur = stack.write[stack.size() - 1];
			VKVNode block_node;
			block_node.key = cur.pending_key;
			block_node.is_block = true;
			cur.pending_key = String();
			cur.dest->push_back(block_node);

			ParseCtx child_ctx;
			// Point to the children of the node we just pushed
			child_ctx.dest = &stack.write[stack.size() - 1].dest->write[stack[stack.size() - 1].dest->size() - 1].children;
			stack.push_back(child_ctx);
			continue;
		}

		if (line == "}") {
			if (stack.size() > 1) {
				stack.resize(stack.size() - 1);
			}
			continue;
		}

		String key, value;
		if (!_vkv_parse_line(line, key, value)) {
			continue;
		}

		ParseCtx &cur = stack.write[stack.size() - 1];

		if (value.is_empty()) {
			// This key might be followed by a block on the next line
			cur.pending_key = key;
		} else {
			// Leaf key-value pair
			VKVNode node;
			node.key = key;
			node.value = value;
			node.is_block = false;
			cur.dest->push_back(node);
			cur.pending_key = String();
		}
	}

	return result;
}

// ---------------------------------------------------------------------------
// VKV lookup helpers
// ---------------------------------------------------------------------------

String SourceConverter::_vkv_get(const Vector<VKVNode> &p_nodes, const String &p_key, const String &p_default) {
	String lower_key = p_key.to_lower();
	for (int i = 0; i < p_nodes.size(); i++) {
		if (!p_nodes[i].is_block && p_nodes[i].key.to_lower() == lower_key) {
			return p_nodes[i].value;
		}
	}
	return p_default;
}

const VKVNode *SourceConverter::_vkv_find_block(const Vector<VKVNode> &p_nodes, const String &p_key) {
	String lower_key = p_key.to_lower();
	for (int i = 0; i < p_nodes.size(); i++) {
		if (p_nodes[i].is_block && p_nodes[i].key.to_lower() == lower_key) {
			return &p_nodes[i];
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Color / vector parsers
// ---------------------------------------------------------------------------

Color SourceConverter::_parse_vkv_color(const String &p_value) {
	// Formats: "[R G B]", "[R G B A]", "R G B", or "{R G B}" with 0-255 ints
	String v = p_value.strip_edges();
	if (v.begins_with("[") || v.begins_with("{")) {
		v = v.substr(1, v.length() - 2).strip_edges();
	}
	Vector<String> parts = v.split(" ", false);
	if (parts.size() >= 3) {
		float r = parts[0].to_float();
		float g = parts[1].to_float();
		float b = parts[2].to_float();
		float a = (parts.size() >= 4) ? parts[3].to_float() : 255.0f;
		// Detect 0-255 vs 0-1 range by magnitude
		if (r > 1.0f || g > 1.0f || b > 1.0f) {
			return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
		}
		return Color(r, g, b, a);
	}
	return Color(1, 1, 1, 1);
}

Vector3 SourceConverter::_parse_vkv_vector3(const String &p_value) {
	String v = p_value.strip_edges();
	if (v.begins_with("[") || v.begins_with("{")) {
		v = v.substr(1, v.length() - 2).strip_edges();
	}
	Vector<String> parts = v.split(" ", false);
	if (parts.size() >= 3) {
		return Vector3(parts[0].to_float(), parts[1].to_float(), parts[2].to_float());
	}
	return Vector3();
}

// ---------------------------------------------------------------------------
// VMT converter
// ---------------------------------------------------------------------------

Error SourceConverter::convert_vmt(const String &p_vmt_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(p_output_dir);
	}

	Error fe = OK;
	Ref<FileAccess> fa = FileAccess::open(p_vmt_path, FileAccess::READ, &fe);
	if (fa.is_null()) {
		print_error("SourceConverter: cannot open VMT: " + p_vmt_path);
		return fe;
	}
	String content = fa->get_as_text();
	fa.unref();

	Vector<VKVNode> nodes = parse_vkv(content);
	if (nodes.is_empty()) {
		print_error("SourceConverter: empty or unparsable VMT: " + p_vmt_path);
		return ERR_PARSE_ERROR;
	}

	// The root node is the shader type (e.g. "VertexLitGeneric")
	String shader_type = nodes[0].key;
	const VKVNode *shader_block = (nodes[0].is_block) ? &nodes[0] : nullptr;
	Vector<VKVNode> params;
	if (shader_block != nullptr) {
		params = shader_block->children;
	}

	String tres_content = _generate_vmt_tres(shader_type, params);
	String base_name = p_vmt_path.get_file().get_basename();
	String out_path = p_output_dir.path_join(base_name + ".tres");

	Ref<FileAccess> out_fa = FileAccess::open(out_path, FileAccess::WRITE);
	if (out_fa.is_null()) {
		print_error("SourceConverter: cannot write .tres: " + out_path);
		return ERR_CANT_CREATE;
	}
	out_fa->store_string(tres_content);
	print_line("SourceConverter: wrote material " + out_path);
	return OK;
}

String SourceConverter::_generate_vmt_tres(const String &p_shader_type, const Vector<VKVNode> &p_params) {
	String shader_lower = p_shader_type.to_lower();

	// Collect textures to emit as ext_resources
	String base_texture = _vkv_get(p_params, "$basetexture");
	String bumpmap = _vkv_get(p_params, "$bumpmap");
	if (bumpmap.is_empty()) {
		bumpmap = _vkv_get(p_params, "$normalmap");
	}
	String selfillum_mask = _vkv_get(p_params, "$selfillummask");

	int res_id = 1;
	String ext_resources;
	int base_tex_id = -1;
	int bump_tex_id = -1;
	int emit_tex_id = -1;

	if (!base_texture.is_empty()) {
		base_tex_id = res_id++;
		String tex_path = "res://imported/" + base_texture.replace("\\", "/").to_lower() + ".png";
		ext_resources += "[ext_resource type=\"Texture2D\" path=\"" + tex_path + "\" id=\"" + itos(base_tex_id) + "\"]\n";
	}
	if (!bumpmap.is_empty()) {
		bump_tex_id = res_id++;
		String tex_path = "res://imported/" + bumpmap.replace("\\", "/").to_lower() + ".png";
		ext_resources += "[ext_resource type=\"Texture2D\" path=\"" + tex_path + "\" id=\"" + itos(bump_tex_id) + "\"]\n";
	}
	if (!selfillum_mask.is_empty()) {
		emit_tex_id = res_id++;
		String tex_path = "res://imported/" + selfillum_mask.replace("\\", "/").to_lower() + ".png";
		ext_resources += "[ext_resource type=\"Texture2D\" path=\"" + tex_path + "\" id=\"" + itos(emit_tex_id) + "\"]\n";
	}

	int load_steps = (ext_resources.is_empty()) ? 1 : res_id;
	String out;
	out += "[gd_resource type=\"StandardMaterial3D\" load_steps=" + itos(load_steps) + " format=3]\n\n";
	if (!ext_resources.is_empty()) {
		out += ext_resources + "\n";
	}
	out += "[resource]\n";

	// Shader-type-specific settings
	if (shader_lower == "unlitgeneric") {
		out += "shading_mode = 2\n"; // SHADING_MODE_UNSHADED
	} else if (shader_lower == "lightmappedgeneric") {
		out += "# lightmapped_generic: baked lighting handled by LightmapGI\n";
	} else if (shader_lower == "refract") {
		out += "refraction_enabled = true\n";
		String refract_amount = _vkv_get(p_params, "$refractamount", "0.05");
		out += "refraction_scale = " + refract_amount + "\n";
	} else if (shader_lower == "water") {
		out += "# water shader: use a custom ShaderMaterial for full water effects\n";
		out += "shading_mode = 0\n"; // start with unshaded as base
	}

	// Albedo texture
	if (base_tex_id >= 0) {
		out += "albedo_texture = ExtResource(\"" + itos(base_tex_id) + "\")\n";
	}

	// Albedo color
	String color_str = _vkv_get(p_params, "$color");
	if (color_str.is_empty()) {
		color_str = _vkv_get(p_params, "$color2");
	}
	if (!color_str.is_empty()) {
		Color c = _parse_vkv_color(color_str);
		out += "albedo_color = Color(" + rtos(c.r) + ", " + rtos(c.g) + ", " + rtos(c.b) + ", " + rtos(c.a) + ")\n";
	}

	// Normal map
	if (bump_tex_id >= 0) {
		out += "normal_enabled = true\n";
		out += "normal_texture = ExtResource(\"" + itos(bump_tex_id) + "\")\n";
	}

	// Transparency / alpha modes
	String alpha_test = _vkv_get(p_params, "$alphatest");
	String translucent = _vkv_get(p_params, "$translucent");
	if (alpha_test == "1") {
		out += "transparency = 1\n"; // ALPHA_SCISSOR
	} else if (translucent == "1") {
		out += "transparency = 1\n"; // ALPHA
	}

	// Additive blend
	String additive = _vkv_get(p_params, "$additive");
	if (additive == "1") {
		out += "blend_mode = 1\n"; // ADD
	}

	// Emissive / self-illumination
	String selfillum = _vkv_get(p_params, "$selfillum");
	if (selfillum == "1") {
		out += "emission_enabled = true\n";
		if (emit_tex_id >= 0) {
			out += "emission_texture = ExtResource(\"" + itos(emit_tex_id) + "\")\n";
		}
	}

	// Specular / metallic from envmap
	String envmap = _vkv_get(p_params, "$envmap");
	if (!envmap.is_empty()) {
		String envmap_tint = _vkv_get(p_params, "$envmaptint", "1");
		out += "metallic = " + envmap_tint + "\n";
		out += "metallic_specular = " + envmap_tint + "\n";
	}

	// No-cull (two-sided)
	String no_cull = _vkv_get(p_params, "$nocull");
	if (no_cull == "1") {
		out += "cull_mode = 2\n"; // CULL_DISABLED
	}

	// Ignored parameters
	String nodecal = _vkv_get(p_params, "$nodecal");
	if (nodecal == "1") {
		out += "# $nodecal: decal projection disabled\n";
	}
	String no_draw = _vkv_get(p_params, "$no_draw");
	if (no_draw == "1") {
		out += "# $no_draw: this surface is invisible in Source; consider hiding the mesh\n";
	}

	return out;
}

// ---------------------------------------------------------------------------
// VMF converter
// ---------------------------------------------------------------------------

String SourceConverter::_vmf_plane_normal(const String &p_plane_str) {
	// Plane string format: "(x1 y1 z1) (x2 y2 z2) (x3 y3 z3)"
	String s = p_plane_str.strip_edges();
	Vector<String> parts;
	// Extract numbers from parentheses
	String cur_num;
	Vector<float> nums;
	for (int i = 0; i < s.length(); i++) {
		char32_t c = s[i];
		if (c == '(' || c == ')' || c == ' ' || c == '\t') {
			if (!cur_num.is_empty()) {
				nums.push_back(cur_num.to_float());
				cur_num = String();
			}
		} else {
			cur_num += c;
		}
	}
	if (!cur_num.is_empty()) {
		nums.push_back(cur_num.to_float());
	}

	if (nums.size() < 9) {
		return "Vector3(0, 1, 0)";
	}
	Vector3 p1(nums[0], nums[1], nums[2]);
	Vector3 p2(nums[3], nums[4], nums[5]);
	Vector3 p3(nums[6], nums[7], nums[8]);

	Vector3 normal = (p2 - p1).cross(p3 - p1).normalized();
	// Convert Source coords (X right, Y forward, Z up) → Godot (X right, Y up, -Z forward)
	Vector3 godot_normal(normal.x, normal.z, -normal.y);
	return "Vector3(" + rtos(godot_normal.x) + ", " + rtos(godot_normal.y) + ", " + rtos(godot_normal.z) + ")";
}

String SourceConverter::_generate_vmf_tscn(const Vector<VKVNode> &p_world_nodes, const Vector<VKVNode> &p_entities) {
	String out;

	// Count solid brushes and entities for load_steps
	int solid_count = 0;
	for (int i = 0; i < p_world_nodes.size(); i++) {
		if (p_world_nodes[i].is_block && p_world_nodes[i].key.to_lower() == "solid") {
			solid_count++;
		}
	}
	int entity_count = (int)p_entities.size();

	out += "[gd_scene load_steps=" + itos(1 + solid_count + entity_count) + " format=3]\n\n";
	out += "[node name=\"World\" type=\"Node3D\"]\n\n";

	// World brushes
	int brush_idx = 0;
	for (int i = 0; i < p_world_nodes.size(); i++) {
		const VKVNode &node = p_world_nodes[i];
		if (!node.is_block || node.key.to_lower() != "solid") {
			continue;
		}
		String solid_id = _vkv_get(node.children, "id", itos(brush_idx));

		// Use first side to get material and an approximate normal
		String material_name = "default";
		String plane_normal = "Vector3(0, 1, 0)";
		for (int si = 0; si < node.children.size(); si++) {
			const VKVNode &side = node.children[si];
			if (side.is_block && side.key.to_lower() == "side") {
				String mat = _vkv_get(side.children, "material");
				if (!mat.is_empty()) {
					material_name = mat.replace("/", "_").replace("\\", "_").to_lower();
				}
				String plane = _vkv_get(side.children, "plane");
				if (!plane.is_empty()) {
					plane_normal = _vmf_plane_normal(plane);
				}
				break;
			}
		}

		out += "[node name=\"Brush_" + solid_id + "\" type=\"StaticBody3D\" parent=\".\"]\n";
		out += "# material: " + material_name + "\n";
		out += "\n";
		out += "[node name=\"CollisionShape3D\" type=\"CollisionShape3D\" parent=\"Brush_" + solid_id + "\"]\n";
		out += "# plane normal: " + plane_normal + "\n";
		out += "\n";
		brush_idx++;
	}

	// Entities
	for (int i = 0; i < p_entities.size(); i++) {
		const VKVNode &ent = p_entities[i];
		String classname = _vkv_get(ent.children, "classname", "entity");
		String origin_str = _vkv_get(ent.children, "origin", "0 0 0");
		String ent_id = _vkv_get(ent.children, "id", itos(i));

		Vector3 origin = _parse_vkv_vector3(origin_str);
		// Convert Source → Godot coordinates
		Vector3 godot_origin(origin.x, origin.z, -origin.y);
		// Scale: Source units to meters (approximate: 1 unit ≈ 0.01905 m)
		godot_origin *= 0.01905f;

		String godot_type = "Node3D";
		if (classname == "light" || classname == "light_spot") {
			godot_type = "OmniLight3D";
		} else if (classname == "light_environment") {
			godot_type = "DirectionalLight3D";
		} else if (classname.begins_with("info_player")) {
			godot_type = "Node3D"; // Spawn point marker
		} else if (classname.begins_with("prop_") || classname.begins_with("func_")) {
			godot_type = "StaticBody3D";
		} else if (classname == "trigger_teleport" || classname.begins_with("trigger_")) {
			godot_type = "Area3D";
		}

		String safe_class = classname.replace(" ", "_");
		out += "[node name=\"" + safe_class + "_" + ent_id + "\" type=\"" + godot_type + "\" parent=\".\"]\n";
		out += "position = Vector3(" + rtos(godot_origin.x) + ", " + rtos(godot_origin.y) + ", " + rtos(godot_origin.z) + ")\n";

		// Light intensity for light entities
		if (classname == "light" || classname == "light_spot") {
			String light_val = _vkv_get(ent.children, "_light", "255 255 255 200");
			Color lc = _parse_vkv_color(light_val);
			out += "light_color = Color(" + rtos(lc.r) + ", " + rtos(lc.g) + ", " + rtos(lc.b) + ")\n";
		}
		out += "\n";
	}

	return out;
}

Error SourceConverter::convert_vmf(const String &p_vmf_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(p_output_dir);
	}

	Error fe = OK;
	Ref<FileAccess> fa = FileAccess::open(p_vmf_path, FileAccess::READ, &fe);
	if (fa.is_null()) {
		print_error("SourceConverter: cannot open VMF: " + p_vmf_path);
		return fe;
	}
	String content = fa->get_as_text();
	fa.unref();

	Vector<VKVNode> root_nodes = parse_vkv(content);

	// Find world block and entity blocks
	Vector<VKVNode> world_children;
	Vector<VKVNode> entities;

	for (int i = 0; i < root_nodes.size(); i++) {
		const VKVNode &n = root_nodes[i];
		if (!n.is_block) {
			continue;
		}
		if (n.key.to_lower() == "world") {
			world_children = n.children;
		} else if (n.key.to_lower() == "entity") {
			entities.push_back(n);
		}
	}

	String tscn_content = _generate_vmf_tscn(world_children, entities);
	String base_name = p_vmf_path.get_file().get_basename();
	String out_path = p_output_dir.path_join(base_name + ".tscn");

	Ref<FileAccess> out_fa = FileAccess::open(out_path, FileAccess::WRITE);
	if (out_fa.is_null()) {
		print_error("SourceConverter: cannot write .tscn: " + out_path);
		return ERR_CANT_CREATE;
	}
	out_fa->store_string(tscn_content);
	print_line("SourceConverter: wrote map scene " + out_path);
	return OK;
}

// ---------------------------------------------------------------------------
// SMD converter
// ---------------------------------------------------------------------------

Error SourceConverter::convert_smd(const String &p_smd_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(p_output_dir);
	}

	Error fe = OK;
	Ref<FileAccess> fa = FileAccess::open(p_smd_path, FileAccess::READ, &fe);
	if (fa.is_null()) {
		print_error("SourceConverter: cannot open SMD: " + p_smd_path);
		return fe;
	}
	String content = fa->get_as_text();
	fa.unref();

	Vector<String> lines = content.split("\n");

	// Parse sections: nodes, skeleton, triangles
	enum Section {
		NONE,
		NODES,
		SKELETON,
		TRIANGLES
	};
	Section cur_section = NONE;

	// OBJ output buffers
	Vector<Vector3> positions;
	Vector<Vector3> normals;
	Vector<Vector2> uvs;
	// Each face: indices into the above arrays (triples of pos/norm/uv)
	struct ObjFace {
		int vi[3], ni[3], ti[3];
		String material;
	};
	Vector<ObjFace> faces;
	String current_material;

	for (int li = 0; li < lines.size(); li++) {
		String line = lines[li].strip_edges();
		if (line.is_empty() || line.begins_with("//")) {
			continue;
		}

		if (line == "end") {
			cur_section = NONE;
			continue;
		}
		if (line == "nodes") {
			cur_section = NODES;
			continue;
		}
		if (line == "skeleton") {
			cur_section = SKELETON;
			continue;
		}
		if (line == "triangles") {
			cur_section = TRIANGLES;
			continue;
		}

		if (cur_section == TRIANGLES) {
			// Check if this line is a material name (no numeric tokens at start)
			Vector<String> tokens = line.split(" ", false);
			if (tokens.is_empty()) {
				continue;
			}
			// A vertex line starts with a bone index (integer)
			bool is_vertex = true;
			const String &tok0 = tokens[0];
			for (int ci = 0; ci < tok0.length(); ci++) {
				char32_t c = tok0[ci];
				if (!is_digit(c) && c != '-') {
					is_vertex = false;
					break;
				}
			}

			if (!is_vertex) {
				// Material name line
				current_material = line;
				continue;
			}

			// Vertex line: bone_id  x y z  nx ny nz  u v  [links...]
			if (tokens.size() < 9) {
				continue;
			}
			float x = tokens[1].to_float();
			float y = tokens[2].to_float();
			float z = tokens[3].to_float();
			float nx = tokens[4].to_float();
			float ny = tokens[5].to_float();
			float nz = tokens[6].to_float();
			float u = tokens[7].to_float();
			float v = tokens[8].to_float();

			// Convert Source → Godot coordinates
			positions.push_back(Vector3(x, z, -y) * 0.01905f);
			normals.push_back(Vector3(nx, nz, -ny).normalized());
			uvs.push_back(Vector2(u, 1.0f - v));

			// Every 3 vertices form a triangle
			int base = positions.size() - 1;
			if (base % 3 == 2) {
				ObjFace f;
				f.material = current_material;
				for (int k = 0; k < 3; k++) {
					int idx = base - 2 + k;
					f.vi[k] = idx;
					f.ni[k] = idx;
					f.ti[k] = idx;
				}
				faces.push_back(f);
			}
		}
	}

	// Write .obj file
	String obj_out;
	obj_out += "# Converted from SMD: " + p_smd_path.get_file() + "\n";
	obj_out += "mtllib " + p_smd_path.get_file().get_basename() + ".mtl\n\n";

	for (int i = 0; i < positions.size(); i++) {
		obj_out += "v " + rtos(positions[i].x) + " " + rtos(positions[i].y) + " " + rtos(positions[i].z) + "\n";
	}
	for (int i = 0; i < uvs.size(); i++) {
		obj_out += "vt " + rtos(uvs[i].x) + " " + rtos(uvs[i].y) + "\n";
	}
	for (int i = 0; i < normals.size(); i++) {
		obj_out += "vn " + rtos(normals[i].x) + " " + rtos(normals[i].y) + " " + rtos(normals[i].z) + "\n";
	}

	String last_mat;
	for (int fi = 0; fi < faces.size(); fi++) {
		const ObjFace &f = faces[fi];
		if (f.material != last_mat) {
			obj_out += "usemtl " + f.material + "\n";
			last_mat = f.material;
		}
		obj_out += "f";
		for (int k = 0; k < 3; k++) {
			int idx = f.vi[k] + 1; // OBJ is 1-indexed
			obj_out += " " + itos(idx) + "/" + itos(idx) + "/" + itos(idx);
		}
		obj_out += "\n";
	}

	String base_name = p_smd_path.get_file().get_basename();
	String out_path = p_output_dir.path_join(base_name + ".obj");
	Ref<FileAccess> out_fa = FileAccess::open(out_path, FileAccess::WRITE);
	if (out_fa.is_null()) {
		print_error("SourceConverter: cannot write .obj: " + out_path);
		return ERR_CANT_CREATE;
	}
	out_fa->store_string(obj_out);
	print_line("SourceConverter: wrote mesh " + out_path);

	// Write companion .mtl file listing unique materials
	HashSet<String> seen_materials;
	String mtl_out;
	mtl_out += "# MTL for " + base_name + "\n";
	for (int fi = 0; fi < faces.size(); fi++) {
		const String &mat = faces[fi].material;
		if (!mat.is_empty() && !seen_materials.has(mat)) {
			seen_materials.insert(mat);
			mtl_out += "newmtl " + mat + "\n";
			mtl_out += "Ka 1.0 1.0 1.0\n";
			mtl_out += "Kd 1.0 1.0 1.0\n";
			mtl_out += "Ks 0.0 0.0 0.0\n";
			// Reference a texture if one can be inferred from the material name
			String tex_name = mat.get_file().is_empty() ? mat : mat.get_file();
			mtl_out += "map_Kd " + tex_name + ".png\n\n";
		}
	}
	String mtl_path = p_output_dir.path_join(base_name + ".mtl");
	Ref<FileAccess> mtl_fa = FileAccess::open(mtl_path, FileAccess::WRITE);
	if (mtl_fa.is_valid()) {
		mtl_fa->store_string(mtl_out);
		print_line("SourceConverter: wrote material library " + mtl_path);
	}
	return OK;
}

#endif // TOOLS_ENABLED
