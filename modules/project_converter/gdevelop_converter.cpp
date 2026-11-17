/**************************************************************************/
/*  gdevelop_converter.cpp                                                */
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

#include "gdevelop_converter.h"

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static String _gd_indent(int p_level) {
	String s;
	for (int i = 0; i < p_level; i++) {
		s += "\t";
	}
	return s;
}

// Quote a GDScript string literal safely
static String _gd_str(const String &p_val) {
	return "\"" + p_val.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
}

// Return the first element of a GDevelop "parameters" array, or default
static String _gdparam(const Array &p_params, int p_idx, const String &p_default = "") {
	if (p_params.size() > p_idx) {
		return p_params[p_idx].stringify();
	}
	return p_default;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
Error GDevelopConverter::convert(const String &p_gdg_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		print_error("GDevelopConverter: cannot create DirAccess");
		return ERR_CANT_CREATE;
	}
	Error dir_err = da->make_dir_recursive(p_output_dir);
	if (dir_err != OK) {
		print_error("GDevelopConverter: failed to create output dir: " + p_output_dir);
		return dir_err;
	}

	// Read the project file
	Error file_err = OK;
	Ref<FileAccess> fa = FileAccess::open(p_gdg_path, FileAccess::READ, &file_err);
	if (fa.is_null()) {
		print_error("GDevelopConverter: cannot open " + p_gdg_path);
		return file_err;
	}
	String json_text = fa->get_as_text();
	fa.unref();

	Variant parsed = JSON::parse_string(json_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		print_error("GDevelopConverter: " + p_gdg_path + " is not a valid JSON object");
		return ERR_PARSE_ERROR;
	}
	Dictionary project = parsed;

	// Copy resources
	Dictionary resources_section = project.get("resources", Dictionary());
	Array resources = resources_section.get("resources", Array());
	String project_dir = p_gdg_path.get_base_dir();
	_copy_resources(project_dir, p_output_dir, resources);

	// Process each layout
	Array layouts = project.get("layouts", Array());
	for (int li = 0; li < layouts.size(); li++) {
		Dictionary layout = layouts[li];
		String layout_name = layout.get("name", String("Layout") + itos(li));
		String safe_name = layout_name.replace(" ", "_");

		// Generate GDScript for events
		Array events = layout.get("events", Array());
		String script_body = _events_to_gdscript(events, project);
		String script_content = "extends Node\n# Auto-generated from GDevelop layout: " + layout_name + "\n\n";
		script_content += "func _ready():\n\t_init_objects()\n\n";
		script_content += "func _init_objects():\n\tpass\n\n";
		script_content += script_body;

		String script_path = p_output_dir.path_join(safe_name + "_events.gd");
		Ref<FileAccess> sf = FileAccess::open(script_path, FileAccess::WRITE);
		if (sf.is_valid()) {
			sf->store_string(script_content);
		} else {
			WARN_PRINT("GDevelopConverter: could not write script: " + script_path);
		}

		// Generate .tscn
		String tscn_content = _generate_layout_tscn(layout, p_output_dir, script_path);
		String tscn_path = p_output_dir.path_join(safe_name + ".tscn");
		Ref<FileAccess> tf = FileAccess::open(tscn_path, FileAccess::WRITE);
		if (tf.is_null()) {
			print_error("GDevelopConverter: cannot write .tscn: " + tscn_path);
			return ERR_CANT_CREATE;
		}
		tf->store_string(tscn_content);
		print_line("GDevelopConverter: wrote " + tscn_path);
	}

	return OK;
}

// ---------------------------------------------------------------------------
// Event → GDScript
// ---------------------------------------------------------------------------

String GDevelopConverter::_events_to_gdscript(const Array &p_events, const Dictionary &p_project) {
	if (p_events.is_empty()) {
		return String();
	}
	String code;
	for (int i = 0; i < p_events.size(); i++) {
		Dictionary ev = p_events[i];
		code += _event_to_gdscript(ev, 0);
	}
	return code;
}

String GDevelopConverter::_event_to_gdscript(const Dictionary &p_event, int p_indent) {
	String type = p_event.get("type", "");
	String ind = _gd_indent(p_indent);

	if (type == "BuiltinCommonInstructions::Standard") {
		Array conditions = p_event.get("conditions", Array());
		Array actions = p_event.get("actions", Array());
		Array sub_events = p_event.get("events", Array());

		// Build condition expression
		String cond_expr;
		for (int ci = 0; ci < conditions.size(); ci++) {
			Dictionary cond = conditions[ci];
			String expr = _condition_to_expr(cond);
			if (cond_expr.is_empty()) {
				cond_expr = expr;
			} else {
				cond_expr += " and " + expr;
			}
		}

		String result;
		if (!cond_expr.is_empty()) {
			result += ind + "if " + cond_expr + ":\n";
			for (int ai = 0; ai < actions.size(); ai++) {
				result += _action_to_stmt(actions[ai], p_indent + 1);
			}
			for (int si = 0; si < sub_events.size(); si++) {
				result += _event_to_gdscript(sub_events[si], p_indent + 1);
			}
		} else {
			// No conditions — emit actions at current indent level
			for (int ai = 0; ai < actions.size(); ai++) {
				result += _action_to_stmt(actions[ai], p_indent);
			}
			for (int si = 0; si < sub_events.size(); si++) {
				result += _event_to_gdscript(sub_events[si], p_indent);
			}
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::Repeat") {
		String times = p_event.get("repeatExpression", "1");
		Array actions = p_event.get("actions", Array());
		Array sub_events = p_event.get("events", Array());

		String result = ind + "for _i in range(" + times + "):\n";
		for (int ai = 0; ai < actions.size(); ai++) {
			result += _action_to_stmt(actions[ai], p_indent + 1);
		}
		for (int si = 0; si < sub_events.size(); si++) {
			result += _event_to_gdscript(sub_events[si], p_indent + 1);
		}
		if (actions.is_empty() && sub_events.is_empty()) {
			result += _gd_indent(p_indent + 1) + "pass\n";
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::While") {
		Array while_conditions = p_event.get("whileConditions", Array());
		Array actions = p_event.get("actions", Array());
		Array sub_events = p_event.get("events", Array());

		String cond_expr;
		for (int ci = 0; ci < while_conditions.size(); ci++) {
			String expr = _condition_to_expr(while_conditions[ci]);
			if (cond_expr.is_empty()) {
				cond_expr = expr;
			} else {
				cond_expr += " and " + expr;
			}
		}
		if (cond_expr.is_empty()) {
			cond_expr = "true";
		}
		String result = ind + "while " + cond_expr + ":\n";
		for (int ai = 0; ai < actions.size(); ai++) {
			result += _action_to_stmt(actions[ai], p_indent + 1);
		}
		for (int si = 0; si < sub_events.size(); si++) {
			result += _event_to_gdscript(sub_events[si], p_indent + 1);
		}
		if (actions.is_empty() && sub_events.is_empty()) {
			result += _gd_indent(p_indent + 1) + "pass\n";
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::ForEach") {
		String obj = p_event.get("object", "object");
		Array actions = p_event.get("actions", Array());
		Array sub_events = p_event.get("events", Array());

		String result = ind + "for " + obj.replace(" ", "_") + " in get_tree().get_nodes_in_group(" + _gd_str(obj) + "):\n";
		for (int ai = 0; ai < actions.size(); ai++) {
			result += _action_to_stmt(actions[ai], p_indent + 1);
		}
		for (int si = 0; si < sub_events.size(); si++) {
			result += _event_to_gdscript(sub_events[si], p_indent + 1);
		}
		if (actions.is_empty() && sub_events.is_empty()) {
			result += _gd_indent(p_indent + 1) + "pass\n";
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::Group") {
		String name = p_event.get("name", "");
		Array sub_events = p_event.get("events", Array());
		String result = ind + "# Group: " + name + "\n";
		for (int si = 0; si < sub_events.size(); si++) {
			result += _event_to_gdscript(sub_events[si], p_indent);
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::JsCode") {
		String js_code = p_event.get("inlineCode", "");
		String result = ind + "# JsCode (JavaScript cannot run in GDScript):\n";
		Vector<String> js_lines = js_code.split("\n");
		for (int li = 0; li < js_lines.size(); li++) {
			result += ind + "# " + js_lines[li] + "\n";
		}
		return result;
	}

	if (type == "BuiltinCommonInstructions::Link") {
		Array include_files = p_event.get("includeFiles", Array());
		String include_file = include_files.size() > 0 ? String(include_files[0]) : "";
		return ind + "# Link: external events from " + include_file + " (include manually)\n";
	}

	if (type == "BuiltinCommonInstructions::Once") {
		static int once_counter = 0;
		once_counter++;
		String once_key = "once_" + itos(once_counter);
		Array actions = p_event.get("actions", Array());
		Array sub_events = p_event.get("events", Array());
		String result = ind + "if not _once_flags.get(" + _gd_str(once_key) + ", false):\n";
		result += _gd_indent(p_indent + 1) + "_once_flags[" + _gd_str(once_key) + "] = true\n";
		for (int ai = 0; ai < actions.size(); ai++) {
			result += _action_to_stmt(actions[ai], p_indent + 1);
		}
		for (int si = 0; si < sub_events.size(); si++) {
			result += _event_to_gdscript(sub_events[si], p_indent + 1);
		}
		return result;
	}

	// Unknown event type
	return ind + "# " + type + " (event type not converted)\n";
}

String GDevelopConverter::_condition_to_expr(const Dictionary &p_condition) {
	String type = p_condition.get("type", Dictionary()).operator Dictionary().get("value", "");
	if (type.is_empty()) {
		// Newer GDevelop format stores type as a plain string
		type = p_condition.get("type", "");
	}
	Array params = p_condition.get("parameters", Array());
	bool inverted = p_condition.get("inverted", false);

	String expr;

	if (type == "VarScene" || type == "SceneVariableAsBoolean") {
		String var_name = _gdparam(params, 0, "variable");
		String op = _gdparam(params, 1, "==");
		String val = _gdparam(params, 2, "0");
		expr = var_name.replace(" ", "_") + " " + op + " " + val;
	} else if (type == "ObjectVariableAsBoolean" || type == "VarObject") {
		String obj = _gdparam(params, 0, "object");
		String var_name = _gdparam(params, 1, "variable");
		String op = _gdparam(params, 2, "==");
		String val = _gdparam(params, 3, "0");
		expr = obj.replace(" ", "_") + "." + var_name.replace(" ", "_") + " " + op + " " + val;
	} else if (type == "PlatformBehavior::IsOnFloor") {
		String obj = _gdparam(params, 0, "object");
		expr = obj.replace(" ", "_") + ".is_on_floor()";
	} else if (type == "Keyboard::IsKeyPressed") {
		String key = _gdparam(params, 0, "space");
		expr = "Input.is_key_pressed(KEY_" + key.to_upper().replace(" ", "_") + ")";
	} else if (type == "MouseButton") {
		String button = _gdparam(params, 0, "Left");
		String gd_btn = "MOUSE_BUTTON_LEFT";
		if (button == "Right") {
			gd_btn = "MOUSE_BUTTON_RIGHT";
		} else if (button == "Middle") {
			gd_btn = "MOUSE_BUTTON_MIDDLE";
		}
		expr = "Input.is_mouse_button_pressed(" + gd_btn + ")";
	} else if (type == "CollisionChecker::AreObjectsColliding") {
		String obj1 = _gdparam(params, 0, "object1");
		String obj2 = _gdparam(params, 1, "object2");
		expr = obj1.replace(" ", "_") + ".overlaps_body(" + obj2.replace(" ", "_") + ")";
	} else if (type == "Sprite::Animation") {
		String obj = _gdparam(params, 0, "object");
		String op = _gdparam(params, 1, "==");
		String anim = _gdparam(params, 2, "");
		expr = obj.replace(" ", "_") + ".animation " + op + " " + _gd_str(anim);
	} else if (type == "ObjectIsVisible") {
		String obj = _gdparam(params, 0, "object");
		expr = "$" + obj.replace(" ", "_") + ".visible";
	} else if (type == "MouseButtonPressed" || type == "MouseButtonDown") {
		String button = _gdparam(params, 0, "Left");
		String gd_btn = "MOUSE_BUTTON_LEFT";
		if (button == "Right") {
			gd_btn = "MOUSE_BUTTON_RIGHT";
		} else if (button == "Middle") {
			gd_btn = "MOUSE_BUTTON_MIDDLE";
		}
		expr = "Input.is_mouse_button_pressed(" + gd_btn + ")";
	} else if (type == "NumberOfObjectsCondition") {
		String obj = _gdparam(params, 0, "object");
		String op = _gdparam(params, 1, "==");
		String count = _gdparam(params, 2, "0");
		expr = "get_tree().get_nodes_in_group(" + _gd_str(obj) + ").size() " + op + " " + count;
	} else if (type == "CompareTimer") {
		String timer_name = _gdparam(params, 0, "timer");
		String op = _gdparam(params, 1, ">=");
		String val = _gdparam(params, 2, "0");
		expr = timer_name.replace(" ", "_") + "_elapsed " + op + " " + val;
	} else if (type == "StringContains") {
		String haystack = _gdparam(params, 0, "");
		String needle = _gdparam(params, 1, "");
		expr = "(" + haystack + ").contains(" + needle + ")";
	} else if (type == "VariableOfObject" || type == "VarObject") {
		String obj = _gdparam(params, 0, "object");
		String var_name = _gdparam(params, 1, "variable");
		String op = _gdparam(params, 2, "==");
		String val = _gdparam(params, 3, "0");
		expr = obj.replace(" ", "_") + "." + var_name.replace(" ", "_") + " " + op + " " + val;
	} else {
		// Generic fallback: emit the condition type as a comment placeholder
		expr = "true /* condition: " + type + " */";
	}

	if (inverted) {
		expr = "not (" + expr + ")";
	}
	return expr;
}

String GDevelopConverter::_action_to_stmt(const Dictionary &p_action, int p_indent) {
	String type = p_action.get("type", Dictionary()).operator Dictionary().get("value", "");
	if (type.is_empty()) {
		type = p_action.get("type", "");
	}
	Array params = p_action.get("parameters", Array());
	String ind = _gd_indent(p_indent);

	if (type == "SetNumberVariable" || type == "SetStringVariable") {
		String var_name = _gdparam(params, 0, "variable");
		String op = _gdparam(params, 1, "=");
		String val = _gdparam(params, 2, "0");
		if (op == "+" || op == "-" || op == "*" || op == "/") {
			return ind + var_name.replace(" ", "_") + " " + op + "= " + val + "\n";
		}
		return ind + var_name.replace(" ", "_") + " = " + val + "\n";
	}
	if (type == "ObjectsPositionMove" || type == "MoveObject") {
		String obj = _gdparam(params, 0, "object");
		String dx = _gdparam(params, 1, "0");
		String dy = _gdparam(params, 2, "0");
		return ind + obj.replace(" ", "_") + ".position += Vector2(" + dx + ", " + dy + ")\n";
	}
	if (type == "ObjectPositionSetX") {
		String obj = _gdparam(params, 0, "object");
		String op = _gdparam(params, 1, "=");
		String val = _gdparam(params, 2, "0");
		if (op == "+" || op == "-") {
			return ind + obj.replace(" ", "_") + ".position.x " + op + "= " + val + "\n";
		}
		return ind + obj.replace(" ", "_") + ".position.x = " + val + "\n";
	}
	if (type == "ObjectPositionSetY") {
		String obj = _gdparam(params, 0, "object");
		String op = _gdparam(params, 1, "=");
		String val = _gdparam(params, 2, "0");
		if (op == "+" || op == "-") {
			return ind + obj.replace(" ", "_") + ".position.y " + op + "= " + val + "\n";
		}
		return ind + obj.replace(" ", "_") + ".position.y = " + val + "\n";
	}
	if (type == "ObjectsPositionSetAngle") {
		String obj = _gdparam(params, 0, "object");
		String op = _gdparam(params, 1, "=");
		String val = _gdparam(params, 2, "0");
		if (op == "+" || op == "-") {
			return ind + obj.replace(" ", "_") + ".rotation_degrees " + op + "= " + val + "\n";
		}
		return ind + obj.replace(" ", "_") + ".rotation_degrees = " + val + "\n";
	}
	if (type == "Sprite::SetAnimation") {
		String obj = _gdparam(params, 0, "object");
		String anim = _gdparam(params, 1, "");
		return ind + obj.replace(" ", "_") + ".play(" + _gd_str(anim) + ")\n";
	}
	if (type == "PlaySound") {
		String sound = _gdparam(params, 0, "");
		String safe_sound = sound.get_file().get_basename().replace(" ", "_");
		if (safe_sound.is_empty()) {
			safe_sound = "SoundPlayer";
		}
		return ind + "# PlaySound: " + _gd_str(sound) + "\n" +
				ind + "if has_node(" + _gd_str(safe_sound) + "):\n" +
				_gd_indent(p_indent + 1) + "$" + safe_sound + ".play()\n";
	}
	if (type == "DeleteObject") {
		String obj = _gdparam(params, 0, "object");
		return ind + obj.replace(" ", "_") + ".queue_free()\n";
	}
	if (type == "CreateObject" || type == "CreateObjectFromGroupName") {
		String obj = _gdparam(params, 0, "object");
		String layer = _gdparam(params, 1, "");
		String x = _gdparam(params, 2, "0");
		String y = _gdparam(params, 3, "0");
		String safe_obj = obj.replace(" ", "_");
		return ind + "var _new_" + safe_obj + " = preload(\"res://\" + " + _gd_str(obj + ".tscn") + ").instantiate()\n" +
				ind + "add_child(_new_" + safe_obj + ")\n" +
				ind + "_new_" + safe_obj + ".position = Vector2(" + x + ", " + y + ")\n";
	}
	if (type == "ModVarScene" || type == "ModVarSceneNumber") {
		String var_name = _gdparam(params, 0, "variable");
		String op = _gdparam(params, 1, "+");
		String val = _gdparam(params, 2, "0");
		return ind + var_name.replace(" ", "_") + " " + op + "= " + val + "\n";
	}
	if (type == "SetObjectVariable" || type == "SetVariableOfObject") {
		String obj = _gdparam(params, 0, "object");
		String var_name = _gdparam(params, 1, "variable");
		String op = _gdparam(params, 2, "=");
		String val = _gdparam(params, 3, "0");
		if (op == "=" || op == "") {
			return ind + obj.replace(" ", "_") + ".set_meta(" + _gd_str(var_name) + ", " + val + ")\n";
		}
		return ind + obj.replace(" ", "_") + ".set_meta(" + _gd_str(var_name) + ", " +
				obj.replace(" ", "_") + ".get_meta(" + _gd_str(var_name) + ", 0) " + op + " " + val + ")\n";
	}
	if (type == "SetFullscreen" || type == "ToggleFullscreen") {
		return ind + "# " + type + ": use DisplayServer.window_set_mode() to toggle fullscreen\n";
	}
	if (type == "StopCurrentMusicChannel") {
		String channel = _gdparam(params, 0, "0");
		return ind + "# StopCurrentMusicChannel: stop music on channel " + channel + "\n" +
				ind + "if has_node(\"Music\"):\n" +
				_gd_indent(p_indent + 1) + "$Music.stop()\n";
	}
	if (type == "PlayMusic" || type == "PlaySoundOnChannel") {
		String sound = _gdparam(params, 0, "");
		return ind + "# PlayMusic/PlaySound: " + sound + "\n";
	}
	if (type == "ChangeScene") {
		String scene = _gdparam(params, 0, "");
		return ind + "get_tree().change_scene_to_file(" + _gd_str("res://" + scene + ".tscn") + ")\n";
	}

	// Fallback
	return ind + "# " + type + " (not converted)\n";
}

// ---------------------------------------------------------------------------
// .tscn generation for a layout
// ---------------------------------------------------------------------------

String GDevelopConverter::_generate_layout_tscn(const Dictionary &p_layout, const String &p_base_dir, const String &p_script_path) {
	String layout_name = p_layout.get("name", "Layout");
	Array objects = p_layout.get("objects", Array());
	Array instances = p_layout.get("instances", Array());

	// Count load steps: 1 (script) + objects with textures
	int load_steps = 2;
	String out;
	out += "[gd_scene load_steps=" + itos(load_steps) + " format=3]\n\n";

	// Script resource
	out += "[ext_resource type=\"Script\" path=\"" + p_script_path + "\" id=\"1\"]\n\n";

	// Build a map of object name → type for quick lookup
	HashMap<String, String> obj_types;
	for (int oi = 0; oi < objects.size(); oi++) {
		Dictionary obj = objects[oi];
		String name = obj.get("name", "");
		String gd_type = obj.get("type", "");
		obj_types[name] = gd_type;
	}

	// Root node with the event script attached
	out += "[node name=\"" + layout_name.replace(" ", "_") + "\" type=\"Node2D\"]\n";
	out += "script = ExtResource(\"1\")\n\n";

	// Place instances
	for (int ii = 0; ii < instances.size(); ii++) {
		Dictionary inst = instances[ii];
		String obj_name = inst.get("objectName", String("Object") + itos(ii));
		double x = inst.get("x", 0.0);
		double y = inst.get("y", 0.0);
		double angle = inst.get("angle", 0.0);

		String gd_node_type = "Sprite2D";
		if (obj_types.has(obj_name)) {
			String gdt = obj_types[obj_name];
			if (gdt == "TiledSprite" || gdt == "TiledBackground") {
				gd_node_type = "Sprite2D";
			} else if (gdt == "TextObject") {
				gd_node_type = "Label";
			} else if (gdt == "ParticleSystem") {
				gd_node_type = "GPUParticles2D";
			}
		}

		String safe_obj = obj_name.replace(" ", "_") + "_" + itos(ii);
		out += "[node name=\"" + safe_obj + "\" type=\"" + gd_node_type + "\" parent=\".\"]\n";
		out += "position = Vector2(" + rtos(x) + ", " + rtos(y) + ")\n";
		if (angle != 0.0) {
			out += "rotation_degrees = " + rtos(angle) + "\n";
		}
		out += "\n";
	}

	return out;
}

// ---------------------------------------------------------------------------
// Resource copying
// ---------------------------------------------------------------------------

Error GDevelopConverter::_copy_resources(const String &p_project_dir, const String &p_output_dir, const Array &p_resources) {
	for (int i = 0; i < p_resources.size(); i++) {
		Dictionary res = p_resources[i];
		String file_rel = res.get("file", "");
		if (file_rel.is_empty()) {
			continue;
		}
		String src = p_project_dir.path_join(file_rel);
		String dst = p_output_dir.path_join(file_rel.get_file());

		Error fe = OK;
		Ref<FileAccess> fa_src = FileAccess::open(src, FileAccess::READ, &fe);
		if (fa_src.is_null()) {
			WARN_PRINT("GDevelopConverter: resource not found: " + src);
			continue;
		}
		PackedByteArray data = fa_src->get_buffer(fa_src->get_length());

		// Ensure sub-directories exist
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (da.is_valid()) {
			da->make_dir_recursive(dst.get_base_dir());
		}

		Ref<FileAccess> fa_dst = FileAccess::open(dst, FileAccess::WRITE);
		if (fa_dst.is_null()) {
			WARN_PRINT("GDevelopConverter: cannot write resource: " + dst);
			continue;
		}
		fa_dst->store_buffer(data);
	}
	return OK;
}

#endif // TOOLS_ENABLED
