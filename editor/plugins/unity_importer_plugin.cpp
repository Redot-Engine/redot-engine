/**************************************************************************/
/*  unity_importer_plugin.cpp                                             */
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

#include "unity_importer_plugin.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/string/print_string.h"
#include "editor/editor_node.h"
#include "editor/gui/editor_toaster.h"
#include "scene/gui/progress_bar.h"

static Error ensure_dir(const String &p_res_path) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d.is_null()) {
		return ERR_CANT_CREATE;
	}
	Vector<String> parts = String(p_res_path).replace("res://", "").split("/");
	String path = "res://";
	for (int i = 0; i < parts.size(); i++) {
		if (parts[i].is_empty()) {
			continue;
		}
		path += parts[i];
		if (!d->dir_exists(path)) {
			Error mk = d->make_dir(path);
			if (mk != OK && mk != ERR_ALREADY_EXISTS) {
				return mk;
			}
		}
		path += "/";
	}
	return OK;
}

static Error copy_dir_recursive(const String &p_src_res, const String &p_dst_res) {
	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d.is_null()) {
		return ERR_CANT_OPEN;
	}
	if (!d->dir_exists(p_src_res)) {
		return ERR_DOES_NOT_EXIST;
	}
	Error err = ensure_dir(p_dst_res);
	if (err != OK) {
		return err;
	}

	List<String> stack;
	stack.push_back(p_src_res);

	while (!stack.is_empty()) {
		String cur_src = stack.front()->get();
		stack.pop_front();
		String rel = cur_src.trim_prefix(p_src_res);
		String cur_dst = p_dst_res + rel;
		if (!d->dir_exists(cur_dst)) {
			Error mk = d->make_dir(cur_dst);
			if (mk != OK && mk != ERR_ALREADY_EXISTS) {
				return mk;
			}
		}

		Ref<DirAccess> sub = DirAccess::open(cur_src);
		if (sub.is_null()) {
			return ERR_CANT_OPEN;
		}
		sub->list_dir_begin();
		String name = sub->get_next();
		while (!name.is_empty()) {
			if (name == "." || name == "..") {
				name = sub->get_next();
				continue;
			}
			String src_path = cur_src.path_join(name);
			String dst_path = cur_dst.path_join(name);
			if (sub->current_is_dir()) {
				stack.push_back(src_path);
			} else {
				PackedByteArray buf = FileAccess::get_file_as_bytes(src_path);
				Ref<FileAccess> f = FileAccess::open(dst_path, FileAccess::WRITE);
				if (f.is_null()) {
					sub->list_dir_end();
					return ERR_CANT_CREATE;
				}
				f->store_buffer(buf);
			}
			name = sub->get_next();
		}
		sub->list_dir_end();
	}
	return OK;
}

static Error _read_file_bytes(const String &p_path, PackedByteArray &r_bytes) {
	Error fe = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &fe);
	if (fe != OK || f.is_null()) {
		return fe != OK ? fe : ERR_CANT_OPEN;
	}
	r_bytes = f->get_buffer(f->get_length());
	return OK;
}

static String _strip_semicolon(const String &p_line) {
	String l = p_line.strip_edges();
	if (l.ends_with(";")) {
		l = l.substr(0, l.length() - 1);
	}
	return l;
}

static String _convert_unity_call(const String &p_line) {
	String l = p_line;
	l = l.replace("Debug.Log", "print");
	l = l.replace("Input.GetKeyDown", "Input.is_action_just_pressed");
	l = l.replace("Input.GetKey", "Input.is_action_pressed");
	l = l.replace("transform.position", "global_transform.origin");
	l = l.replace("Time.deltaTime", "delta");
	return l;
}

static String _default_value_for_type(const String &p_type) {
	String t = p_type.strip_edges().to_lower();
	if (t == "float" || t == "double") {
		return "0.0";
	}
	if (t == "int" || t == "long" || t == "short") {
		return "0";
	}
	if (t == "bool") {
		return "false";
	}
	if (t == "string") {
		return "\"\"";
	}
	if (t.contains("vector3")) {
		return "Vector3.ZERO";
	}
	if (t.contains("vector2")) {
		return "Vector2.ZERO";
	}
	return "null";
}

static String _convert_method_body(const Vector<String> &p_lines) {
	String body;
	for (int i = 0; i < p_lines.size(); i++) {
		String l = _strip_semicolon(_convert_unity_call(p_lines[i]));
		if (l.is_empty()) {
			continue;
		}
		body += String("	") + l + "\n";
	}
	if (body.is_empty()) {
		body = "\tpass\n";
	}
	return body;
}

static Vector<String> _extract_block(const Vector<String> &p_lines, int &r_index) {
	Vector<String> body;
	int depth = 0;
	for (int i = r_index; i < p_lines.size(); i++) {
		String l = p_lines[i];
		if (l.find("{") != -1) {
			depth++;
			if (depth == 1) {
				continue;
			}
		}
		if (l.find("}") != -1) {
			depth--;
			if (depth == 0) {
				r_index = i;
				break;
			}
		}
		if (depth >= 1) {
			body.push_back(l);
		}
	}
	return body;
}

static String _convert_csharp_to_gd(const String &p_source_code) {
	Vector<String> lines = p_source_code.split("\n");
	String class_name = "UnityScript";
	Vector<String> fields;
	HashMap<String, Vector<String>> method_map;
	static const struct {
		const char *unity;
		const char *gd;
	} method_pairs[] = {
		{ "Awake", "_ready" },
		{ "Start", "_ready" },
		{ "OnEnable", "_enter_tree" },
		{ "OnDisable", "_exit_tree" },
		{ "Update", "_process" },
		{ "FixedUpdate", "_physics_process" },
	};

	for (int i = 0; i < lines.size(); i++) {
		String l = lines[i].strip_edges();
		if (l.begins_with("using ") || l.begins_with("namespace ")) {
			continue;
		}
		int class_pos = l.find("class ");
		if (class_pos != -1) {
			Vector<String> tokens = l.substr(class_pos + 6).split(" ");
			if (!tokens.is_empty()) {
				class_name = tokens[0].strip_edges().trim_suffix(":").strip_edges();
			}
			continue;
		}

		bool is_method = false;
		for (unsigned int m = 0; m < sizeof(method_pairs) / sizeof(method_pairs[0]); m++) {
			String method_sig = String(method_pairs[m].unity) + "(";
			if (l.find(method_sig) != -1) {
				Vector<String> body = _extract_block(lines, i);
				String gd_name = method_pairs[m].gd;
				if (!method_map.has(gd_name)) {
					method_map[gd_name] = Vector<String>();
				}
				for (int b = 0; b < body.size(); b++) {
					method_map[gd_name].push_back(body[b]);
				}
				is_method = true;
				break;
			}
		}
		if (is_method) {
			continue;
		}

		if (l.find(";") != -1 && l.find("(") == -1 && l.find(")") == -1) {
			fields.push_back(l);
		}
	}

	String out;
	out += String("# Auto-converted from Unity C# script\n# Original class: ") + class_name + "\n";
	out += "extends Node\n\n";
	for (int f = 0; f < fields.size(); f++) {
		String fld = fields[f];
		Vector<String> bits = fld.replace(";", "").split(" ");
		for (int bi = bits.size() - 1; bi >= 0; bi--) {
			if (bits[bi].is_empty()) {
				bits.remove_at(bi);
			}
		}
		if (bits.size() >= 2) {
			String type = bits[bits.size() - 2];
			String name = bits[bits.size() - 1];
			String def_val = _default_value_for_type(type);
			out += vformat("var %s = %s\n", name, def_val);
		}
	}

	if (method_map.has("_ready")) {
		out += "\nfunc _ready():\n";
		out += _convert_method_body(method_map["_ready"]);
	}
	if (method_map.has("_process")) {
		out += "\nfunc _process(delta):\n";
		out += _convert_method_body(method_map["_process"]);
	}
	if (method_map.has("_physics_process")) {
		out += "\nfunc _physics_process(delta):\n";
		out += _convert_method_body(method_map["_physics_process"]);
	}
	if (method_map.has("_enter_tree")) {
		out += "\nfunc _enter_tree():\n";
		out += _convert_method_body(method_map["_enter_tree"]);
	}
	if (method_map.has("_exit_tree")) {
		out += "\nfunc _exit_tree():\n";
		out += _convert_method_body(method_map["_exit_tree"]);
	}

	out += "\n# Original C# source (for reference):\n";
	for (int i = 0; i < lines.size(); i++) {
		out += "# " + lines[i] + "\n";
	}

	return out;
}

Error UnityAnimImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	print_line(vformat("UnityAnimImportPlugin::import called for %s -> %s", p_source_file, p_save_path));
	
	PackedByteArray bytes;
	Error r = _read_file_bytes(p_source_file, bytes);
	if (r != OK) {
		print_error(vformat("Failed to read animation file: %s (error: %d)", p_source_file, r));
		return r;
	}

	UnityAsset asset;
	asset.pathname = p_save_path.get_basename() + ".tres";
	asset.asset_data = bytes;

	Error err = ensure_dir(asset.pathname.get_base_dir());
	if (err != OK) {
		print_error(vformat("Failed to create directory for animation: %s (error: %d)", asset.pathname.get_base_dir(), err));
		return err;
	}

	Error convert_err = UnityAssetConverter::convert_animation(asset);
	if (convert_err != OK) {
		print_error(vformat("Failed to convert animation: %s (error: %d)", p_source_file, convert_err));
		return convert_err;
	}
	
	if (r_gen_files) {
		r_gen_files->push_back(asset.pathname);
	}
	
	return OK;
}

Error UnityYamlSceneImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PackedByteArray bytes;
	Error r = _read_file_bytes(p_source_file, bytes);
	if (r != OK) {
		print_error(vformat("Failed to read file: %s", p_source_file));
		return r;
	}

	UnityAsset asset;
	// Add .tscn extension to the save path as required by Redot's import system
	String final_path = p_save_path;
	if (!final_path.ends_with(".tscn")) {
		final_path += ".tscn";
	}
	asset.pathname = final_path;
	asset.asset_data = bytes;

	String ext = p_source_file.get_extension().to_lower();
	if (ext == "prefab") {
		Error result = UnityAssetConverter::convert_prefab(asset);
		if (result == OK && r_gen_files) {
			r_gen_files->push_back(asset.pathname);
		}
		return result;
	}
	Error result = UnityAssetConverter::convert_scene(asset);
	if (result == OK && r_gen_files) {
		r_gen_files->push_back(asset.pathname);
	}
	return result;
}

Error UnityMatImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	PackedByteArray bytes;
	Error r = _read_file_bytes(p_source_file, bytes);
	if (r != OK) {
		print_error(vformat("Failed to read material file: %s", p_source_file));
		return r;
	}

	UnityAsset asset;
	asset.pathname = p_save_path.get_basename() + ".tres";
	asset.asset_data = bytes;

	Error err = ensure_dir(asset.pathname.get_base_dir());
	if (err != OK) {
		print_error(vformat("Failed to create directory for material: %s (error: %d)", asset.pathname.get_base_dir(), err));
		return err;
	}

	HashMap<String, UnityAsset> dummy_all;
	Error convert_err = UnityAssetConverter::convert_material(asset, dummy_all);
	if (convert_err != OK) {
		print_error(vformat("Failed to convert material: %s (error: %d)", p_source_file, convert_err));
		return convert_err;
	}
	
	if (r_gen_files) {
		r_gen_files->push_back(asset.pathname);
	}
	
	print_line(vformat("Material import: %s -> %s", p_source_file, asset.pathname));
	return OK;
}

Error UnityScriptImportPlugin::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	print_line(vformat("UnityScriptImportPlugin::import called for %s -> %s", p_source_file, p_save_path));

	Error fe = OK;
	String cs_code = FileAccess::get_file_as_string(p_source_file, &fe);
	if (fe != OK) {
		print_error(vformat("Failed to read C# script: %s", p_source_file));
		return fe;
	}

	// Detect whether the engine was built with C# support; fallback to GDScript when unavailable
	bool has_csharp = false;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		if (ScriptServer::get_language(i) && ScriptServer::get_language(i)->get_name() == "C#") {
			has_csharp = true;
			break;
		}
	}

	// Convert Unity C# namespaces to Godot C# namespaces
	// Keep it as C# but update the API calls
	cs_code = cs_code.replace("using UnityEngine;", "using Godot;\nusing System;\nusing System.Collections.Generic;");
	cs_code = cs_code.replace("using UnityEngine.UI;", "using Godot;");
	cs_code = cs_code.replace("using UnityEngine.Events;", "using Godot;");
	cs_code = cs_code.replace("using System.Collections;", "using System.Collections.Generic;");

	// MonoBehaviour -> Node (most common base class)
	cs_code = cs_code.replace("public class", "// Unity to Godot: Changed MonoBehaviour to Node3D\npublic partial class");
	cs_code = cs_code.replace(": MonoBehaviour", ": Node3D");

	// Common Unity lifecycle methods to Godot equivalents
	cs_code = cs_code.replace("void Awake()", "public override void _Ready() // Was Awake()");
	cs_code = cs_code.replace("void Start()", "public override void _Ready() // Was Start()");
	cs_code = cs_code.replace("void OnEnable()", "public override void _EnterTree() // Was OnEnable()");
	cs_code = cs_code.replace("void OnDisable()", "public override void _ExitTree() // Was OnDisable()");
	cs_code = cs_code.replace("void Update()", "public override void _Process(double delta) // Was Update()");
	cs_code = cs_code.replace("void FixedUpdate()", "public override void _PhysicsProcess(double delta) // Was FixedUpdate()");
	cs_code = cs_code.replace("void LateUpdate()", "public override void _Process(double delta) // Was LateUpdate()");

	// Unity Audio → Godot Audio (AudioSource → AudioStreamPlayer)
	cs_code = cs_code.replace("AudioSource", "AudioStreamPlayer");
	cs_code = cs_code.replace(".volume", ".VolumeDb");
	cs_code = cs_code.replace(".Play()", ".Play()");
	cs_code = cs_code.replace(".Stop()", ".Stop()");
	cs_code = cs_code.replace(".clip", ".Stream");

	// PlayerPrefs → Godot ConfigFile (based on JSONConfigFile pattern)
	cs_code = cs_code.replace("PlayerPrefs.GetFloat", "// TODO: Implement ConfigFile\n\t\tGetConfigFloat");
	cs_code = cs_code.replace("PlayerPrefs.SetFloat", "// TODO: Implement ConfigFile\n\t\tSetConfigFloat");
	cs_code = cs_code.replace("PlayerPrefs.GetInt", "// TODO: Implement ConfigFile\n\t\tGetConfigInt");
	cs_code = cs_code.replace("PlayerPrefs.SetInt", "// TODO: Implement ConfigFile\n\t\tSetConfigInt");
	cs_code = cs_code.replace("PlayerPrefs.GetString", "// TODO: Implement ConfigFile\n\t\tGetConfigString");
	cs_code = cs_code.replace("PlayerPrefs.SetString", "// TODO: Implement ConfigFile\n\t\tSetConfigString");
	cs_code = cs_code.replace("PlayerPrefs.Save()", "// TODO: ConfigFile.Save()");

	// Transform → Transform3D / Node3D
	cs_code = cs_code.replace(".transform.position", ".Position");
	cs_code = cs_code.replace(".transform.rotation", ".Rotation");
	cs_code = cs_code.replace(".transform.localPosition", ".Position");
	cs_code = cs_code.replace(".transform.localRotation", ".Rotation");
	cs_code = cs_code.replace(".transform.localScale", ".Scale");
	cs_code = cs_code.replace("transform.forward", "GlobalTransform.Basis.Z");
	cs_code = cs_code.replace("transform.up", "GlobalTransform.Basis.Y");
	cs_code = cs_code.replace("transform.right", "GlobalTransform.Basis.X");

	// Vector3 / Vector2 → Godot equivalents (Unity uses uppercase)
	cs_code = cs_code.replace("Vector3.zero", "Vector3.Zero");
	cs_code = cs_code.replace("Vector3.one", "Vector3.One");
	cs_code = cs_code.replace("Vector3.forward", "Vector3.Forward");
	cs_code = cs_code.replace("Vector3.up", "Vector3.Up");
	cs_code = cs_code.replace("Vector3.right", "Vector3.Right");
	cs_code = cs_code.replace("Vector2.zero", "Vector2.Zero");
	cs_code = cs_code.replace("Vector2.one", "Vector2.One");

	// Quaternion → Godot equivalents
	cs_code = cs_code.replace("Quaternion.identity", "Quaternion.Identity");
	cs_code = cs_code.replace("Quaternion.Euler", "Quaternion.FromEuler");

	// Mathf → Godot Mathf
	cs_code = cs_code.replace("Mathf.Lerp", "Mathf.Lerp");
	cs_code = cs_code.replace("Mathf.Clamp", "Mathf.Clamp");
	cs_code = cs_code.replace("Mathf.Abs", "Mathf.Abs");

	// Common Unity API replacements
	cs_code = cs_code.replace("Time.deltaTime", "(float)delta");
	cs_code = cs_code.replace("Time.time", "(float)Time.GetTicksMsec() / 1000.0f");
	cs_code = cs_code.replace("GameObject", "Node");
	cs_code = cs_code.replace("GetComponent<", "GetNode<");
	cs_code = cs_code.replace("AddComponent<", "AddChild(new ");
	cs_code = cs_code.replace("Debug.Log(", "GD.Print(");
	cs_code = cs_code.replace("Debug.LogWarning(", "GD.PushWarning(");
	cs_code = cs_code.replace("Debug.LogError(", "GD.PushError(");
	cs_code = cs_code.replace("Instantiate(", "// TODO: Use PackedScene.Instantiate()\n\t\t// Instantiate(");
	cs_code = cs_code.replace("Destroy(", "QueueFree() // Was Destroy(");
	cs_code = cs_code.replace(".SetActive(", ".Visible = ");
	cs_code = cs_code.replace(".activeSelf", ".Visible");

	// RigidBody → RigidBody3D
	cs_code = cs_code.replace("Rigidbody", "RigidBody3D");
	cs_code = cs_code.replace(".velocity", ".LinearVelocity");
	cs_code = cs_code.replace(".angularVelocity", ".AngularVelocity");
	cs_code = cs_code.replace(".AddForce(", ".ApplyCentralForce(");

	// Collider → CollisionShape3D
	cs_code = cs_code.replace("Collider", "CollisionShape3D");
	cs_code = cs_code.replace("BoxCollider", "BoxShape3D");
	cs_code = cs_code.replace("SphereCollider", "SphereShape3D");
	cs_code = cs_code.replace("CapsuleCollider", "CapsuleShape3D");

	if (!has_csharp) {
		// Fallback: convert to GDScript so the project still loads without the C# module
		String gd_code = _convert_csharp_to_gd(cs_code);
		String out_path = p_save_path;
		if (!out_path.ends_with(".gd")) {
			out_path += ".gd";
		}

		Error err = ensure_dir(out_path.get_base_dir());
		if (err != OK) {
			print_error(vformat("Failed to create directory for GDScript: %s", out_path.get_base_dir()));
			return err;
		}

		Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE, &fe);
		if (f.is_null() || fe != OK) {
			print_error(vformat("Failed to open GDScript file for writing: %s (error: %d)", out_path, fe));
			return fe != OK ? fe : ERR_CANT_CREATE;
		}

		f->store_string(gd_code);
		if (r_gen_files) {
			r_gen_files->push_back(out_path);
		}
		print_line(vformat("Converted Unity C# script to GDScript fallback: %s", out_path));
		return OK;
	}

	// Save as .cs file when C# is available
	String out_path = p_save_path;
	if (!out_path.ends_with(".cs")) {
		out_path += ".cs";
	}

	Error err = ensure_dir(out_path.get_base_dir());
	if (err != OK) {
		print_error(vformat("Failed to create directory for C# script: %s", out_path.get_base_dir()));
		return err;
	}

	Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE, &fe);
	if (f.is_null() || fe != OK) {
		print_error(vformat("Failed to open C# script file for writing: %s (error: %d)", out_path, fe));
		return fe != OK ? fe : ERR_CANT_CREATE;
	}

	f->store_string(cs_code);
	if (r_gen_files) {
		r_gen_files->push_back(out_path);
	}

	print_line(vformat("Converted Unity C# script to Godot C#: %s", out_path));
	return OK;
}

void UnityImporterPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_import_unity_packages"), &UnityImporterPlugin::_import_unity_packages);
	ClassDB::bind_method(D_METHOD("_install_unity_to_godot"), &UnityImporterPlugin::_install_unity_to_godot);
	ClassDB::bind_method(D_METHOD("_install_shaderlab2godotsl"), &UnityImporterPlugin::_install_shaderlab2godotsl);
	ClassDB::bind_method(D_METHOD("_convert_unity_shader"), &UnityImporterPlugin::_convert_unity_shader);
	ClassDB::bind_method(D_METHOD("_file_selected"), &UnityImporterPlugin::_file_selected);
	ClassDB::bind_method(D_METHOD("_handle_shader_file"), &UnityImporterPlugin::_handle_shader_file);
}

void UnityImporterPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		print_line("UnityImporterPlugin::_notification ENTER_TREE - registering import plugins");
		add_tool_menu_item(TTR("Import Unity Package..."), callable_mp(this, &UnityImporterPlugin::_import_unity_packages));
		add_tool_menu_item(TTR("Install UnityToGodot Toolkit..."), callable_mp(this, &UnityImporterPlugin::_install_unity_to_godot));
		add_tool_menu_item(TTR("Install Shaderlab2GodotSL..."), callable_mp(this, &UnityImporterPlugin::_install_shaderlab2godotsl));
		add_tool_menu_item(TTR("Convert Unity Shader..."), callable_mp(this, &UnityImporterPlugin::_convert_unity_shader));

		anim_importer.instantiate();
		scene_importer.instantiate();
		mat_importer.instantiate();
		script_importer.instantiate();

		add_import_plugin(anim_importer);
		add_import_plugin(scene_importer);
		add_import_plugin(mat_importer);
		add_import_plugin(script_importer);
		print_line("UnityImporterPlugin: Import plugins registered successfully");
	}
	if (p_what == NOTIFICATION_EXIT_TREE) {
		remove_tool_menu_item(TTR("Import Unity Package..."));
		remove_tool_menu_item(TTR("Install UnityToGodot Toolkit..."));
		remove_tool_menu_item(TTR("Install Shaderlab2GodotSL..."));
		remove_tool_menu_item(TTR("Convert Unity Shader..."));
		remove_tool_menu_item(TTR("Browse Asset Stores (Godot + Unity)..."));

		if (anim_importer.is_valid()) {
			remove_import_plugin(anim_importer);
		}
		if (scene_importer.is_valid()) {
			remove_import_plugin(scene_importer);
		}
		if (mat_importer.is_valid()) {
			remove_import_plugin(mat_importer);
		}
		if (script_importer.is_valid()) {
			remove_import_plugin(script_importer);
		}
	}
}

void UnityImporterPlugin::_import_unity_packages() {
	_show_package_dialog();
}

void UnityImporterPlugin::_show_package_dialog() {
	if (!package_dialog) {
		package_dialog = memnew(EditorFileDialog);
		package_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		package_dialog->clear_filters();
		package_dialog->add_filter("*.unitypackage", TTR("Unity Package"));
		package_dialog->set_title(TTR("Select Unity Package"));
		package_dialog->connect("file_selected", callable_mp(this, &UnityImporterPlugin::_file_selected));
		EditorNode::get_singleton()->get_gui_base()->add_child(package_dialog);
	}
	package_dialog->popup_file_dialog();
}

Error UnityImporterPlugin::_parse_unity_package(const String &p_path) {
	parsed_assets.clear();
	print_line("Parsing Unity package: " + p_path);
	Error result = UnityPackageParser::parse_unitypackage(p_path, parsed_assets);
	if (result == OK) {
		print_line(vformat("Successfully parsed Unity package with %d assets", parsed_assets.size()));
	} else {
		print_error("Failed to parse Unity package");
	}
	return result;
}

void UnityImporterPlugin::_file_selected(const String &p_path) {
	current_package_path = p_path;
	Error err = _parse_unity_package(p_path);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to parse Unity package"), EditorToaster::SEVERITY_ERROR);
		return;
	}
	_import_assets();
}

void UnityImporterPlugin::_import_assets() {
	int imported = 0;
	int skipped = 0;
	int failed = 0;
	String error_log;

	auto process_pass = [&](bool p_textures_only) {
		for (const KeyValue<String, UnityAsset> &E : parsed_assets) {
			String ext = E.value.pathname.get_extension().to_lower();
			bool is_texture = (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" || ext == "bmp" || ext == "tif" || ext == "tiff");
			if (p_textures_only != is_texture) {
				continue;
			}

			Error err = UnityAssetConverter::extract_asset(E.value, parsed_assets);
			switch (err) {
				case OK:
					imported++;
					break;
				case ERR_SKIP:
					skipped++;
					break;
				default:
					failed++;
					String error_msg = vformat("Failed to import '%s' (extension: .%s)", E.value.pathname, ext);
					print_error(error_msg);
					if (!error_log.is_empty()) {
						error_log += "\n";
					}
					error_log += error_msg;
					break;
			}
		}
	};

	process_pass(true);
	process_pass(false);

	String summary = vformat(TTR("Unity package import finished: %d imported, %d skipped, %d failed"), imported, skipped, failed);
	if (!error_log.is_empty()) {
		print_line("Unity import errors:\n" + error_log);
	}
	EditorToaster::Severity sev = failed > 0 ? EditorToaster::SEVERITY_WARNING : EditorToaster::SEVERITY_INFO;
	EditorToaster::get_singleton()->popup_str(summary, sev);
}

void UnityImporterPlugin::_install_unity_to_godot() {
	const String src_dir = "res://addons/_unity_bundled/UnityToGodot";
	const String dst_dir = "res://addons/UnityToGodot";
	Error err = copy_dir_recursive(src_dir, dst_dir);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Bundled UnityToGodot toolkit not found. Populate addons/_unity_bundled/UnityToGodot inside the editor install."), EditorToaster::SEVERITY_WARNING);
		return;
	}
	EditorToaster::get_singleton()->popup_str(TTR("UnityToGodot toolkit installed locally under res://addons/UnityToGodot."));
}

void UnityImporterPlugin::_install_shaderlab2godotsl() {
	EditorToaster::get_singleton()->popup_str(TTR("Shader converter is built-in. Use Tools > Convert Unity Shader."));
}

void UnityImporterPlugin::_convert_unity_shader() {
	if (!shader_dialog) {
		shader_dialog = memnew(EditorFileDialog);
		shader_dialog->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
		shader_dialog->clear_filters();
		shader_dialog->add_filter("*.shader", TTR("Unity Shader"));
		shader_dialog->set_title(TTR("Select Unity Shader"));
		shader_dialog->connect("file_selected", callable_mp(this, &UnityImporterPlugin::_handle_shader_file));
		EditorNode::get_singleton()->get_gui_base()->add_child(shader_dialog);
	}
	shader_dialog->popup_file_dialog();
}

void UnityImporterPlugin::_handle_shader_file(const String &p_path) {
	Error file_read_err = OK;
	String shader_code = FileAccess::get_file_as_string(p_path, &file_read_err);
	if (file_read_err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to read shader file."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	String godot_shader;
	Error err = UnityShaderConverter::convert_shaderlab_to_godot(shader_code, godot_shader);
	if (err != OK) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to convert shader."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	String output_path = p_path.get_basename() + ".gdshader";
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE);
	if (f.is_null()) {
		EditorToaster::get_singleton()->popup_str(TTR("Failed to save converted shader."), EditorToaster::SEVERITY_ERROR);
		return;
	}

	f->store_string(godot_shader);
	EditorToaster::get_singleton()->popup_str(vformat(TTR("Shader converted successfully: %s"), output_path), EditorToaster::SEVERITY_INFO);
}
