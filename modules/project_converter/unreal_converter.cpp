/**************************************************************************/
/*  unreal_converter.cpp                                                  */
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

#include "unreal_converter.h"

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/math/quaternion.h"
#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/vector.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Parse a value string like "(X=1.0,Y=2.0,Z=3.0)" or "1.0 2.0 3.0"
Vector3 UnrealConverter::_parse_unreal_vector(const String &p_value) {
	String v = p_value.strip_edges();
	// Strip outer parentheses if present
	if (v.begins_with("(") && v.ends_with(")")) {
		v = v.substr(1, v.length() - 2);
	}
	float x = 0, y = 0, z = 0;
	// Named components: X=...,Y=...,Z=...
	Vector<String> parts = v.split(",", false);
	for (const String &part : parts) {
		int eq = part.find("=");
		if (eq < 0) {
			continue;
		}
		String key = part.substr(0, eq).strip_edges().to_upper();
		float val = part.substr(eq + 1).strip_edges().to_float();
		if (key == "X") {
			x = val;
		} else if (key == "Y") {
			y = val;
		} else if (key == "Z") {
			z = val;
		}
	}
	// Fallback: space-separated
	if (parts.size() == 1) {
		Vector<String> sp = v.split(" ", false);
		if (sp.size() >= 3) {
			x = sp[0].to_float();
			y = sp[1].to_float();
			z = sp[2].to_float();
		}
	}
	return Vector3(x, y, z);
}

Quaternion UnrealConverter::_parse_unreal_rotator(const String &p_value) {
	String v = p_value.strip_edges();
	if (v.begins_with("(") && v.ends_with(")")) {
		v = v.substr(1, v.length() - 2);
	}
	float pitch = 0, yaw = 0, roll = 0;
	Vector<String> parts = v.split(",", false);
	for (const String &part : parts) {
		int eq = part.find("=");
		if (eq < 0) {
			continue;
		}
		String key = part.substr(0, eq).strip_edges().to_upper();
		float val = part.substr(eq + 1).strip_edges().to_float();
		if (key == "PITCH") {
			pitch = val;
		} else if (key == "YAW") {
			yaw = val;
		} else if (key == "ROLL") {
			roll = val;
		}
	}
	// Convert Unreal Engine Euler angles to Godot quaternion.
	// Unreal Engine rotation axes (left-handed Z-up):
	//   Yaw   = rotation around Z (up)   → Godot: rotation around Y (up),   negated for handedness flip
	//   Pitch = rotation around Y (right) → Godot: rotation around X (right), sign preserved
	//   Roll  = rotation around X (forward) → Godot: rotation around -Z (forward = -Z), negated
	Quaternion q_yaw(Vector3(0, 1, 0), Math::deg_to_rad(-yaw));
	Quaternion q_pitch(Vector3(1, 0, 0), Math::deg_to_rad(pitch));
	Quaternion q_roll(Vector3(0, 0, 1), Math::deg_to_rad(-roll));
	// Apply in Unreal Engine's intrinsic order: Yaw first, then Pitch, then Roll
	return (q_yaw * q_pitch * q_roll).normalized();
}

Transform3D UnrealConverter::_parse_unreal_transform(const HashMap<String, String> &p_props) {
	// Look for component-level properties first (RelativeLocation etc.)
	Vector3 loc, scale(1, 1, 1);
	Quaternion rot;

	if (p_props.has("RelativeLocation")) {
		Vector3 ul = _parse_unreal_vector(p_props["RelativeLocation"]);
		// Unreal (cm, left-handed Z-up): X=forward, Y=right, Z=up
		// Godot   (m,  right-handed Y-up): X=right,   Y=up,   Z=back
		// Mapping: Godot.X = Unreal.Y * 0.01
		//          Godot.Y = Unreal.Z * 0.01
		//          Godot.Z = -Unreal.X * 0.01
		loc = Vector3(ul.y * 0.01f, ul.z * 0.01f, -ul.x * 0.01f);
	}
	if (p_props.has("RelativeRotation")) {
		rot = _parse_unreal_rotator(p_props["RelativeRotation"]);
	}
	if (p_props.has("RelativeScale3D")) {
		scale = _parse_unreal_vector(p_props["RelativeScale3D"]);
	}

	Basis basis(rot);
	basis.scale(scale);
	return Transform3D(basis, loc);
}

String UnrealConverter::_unreal_class_to_godot_type(const String &p_class) {
	// Strip module path prefix if present (e.g. "/Script/Engine.StaticMeshActor" → "StaticMeshActor")
	String cls = p_class;
	int dot = cls.rfind(".");
	if (dot >= 0) {
		cls = cls.substr(dot + 1);
	}
	// Also handle C++ class prefixes used in T3D (Class=...)
	if (cls.begins_with("A") || cls.begins_with("U")) {
		cls = cls.substr(1);
	}

	if (cls == "StaticMeshActor" || cls == "StaticMesh") {
		return "MeshInstance3D";
	}
	if (cls == "PointLight" || cls == "PointLightActor") {
		return "OmniLight3D";
	}
	if (cls == "SpotLight" || cls == "SpotLightActor") {
		return "SpotLight3D";
	}
	if (cls == "DirectionalLight" || cls == "DirectionalLightActor") {
		return "DirectionalLight3D";
	}
	if (cls == "SkyLight" || cls == "SkyLightActor") {
		return "WorldEnvironment";
	}
	if (cls == "PlayerStart" || cls.begins_with("PlayerStart")) {
		return "Node3D";
	}
	if (cls.begins_with("Trigger") || cls.begins_with("Volume")) {
		return "Area3D";
	}
	if (cls == "SoundActor" || cls == "AmbientSound") {
		return "AudioStreamPlayer3D";
	}
	if (cls == "CameraActor") {
		return "Camera3D";
	}
	if (cls == "SkeletalMeshActor") {
		return "AnimatableBody3D";
	}
	if (cls == "LandscapeProxy" || cls == "Landscape") {
		return "Node3D"; // Terrain — needs manual setup
	}
	if (cls == "ReflectionCapture" || cls.ends_with("ReflectionCapture")) {
		return "ReflectionProbe";
	}
	return "Node3D";
}

// ---------------------------------------------------------------------------
// T3D parser
// ---------------------------------------------------------------------------

Vector<UnrealActor> UnrealConverter::parse_t3d(const String &p_content) {
	Vector<UnrealActor> actors;
	Vector<String> lines = p_content.split("\n");

	// Simple stack-based parser
	enum T3DState {
		S_NONE,
		S_MAP,
		S_LEVEL,
		S_ACTOR,
		S_OBJECT
	};
	T3DState state = S_NONE;
	UnrealActor current_actor;
	HashMap<String, String> current_component_props;
	bool in_component = false;

	for (int li = 0; li < lines.size(); li++) {
		String line = lines[li].strip_edges();
		if (line.is_empty()) {
			continue;
		}

		if (line.begins_with("Begin Map")) {
			state = S_MAP;
			continue;
		}
		if (line.begins_with("End Map")) {
			state = S_NONE;
			continue;
		}
		if (line.begins_with("Begin Level")) {
			state = S_LEVEL;
			continue;
		}
		if (line.begins_with("End Level")) {
			state = S_MAP;
			continue;
		}

		if (line.begins_with("Begin Actor")) {
			state = S_ACTOR;
			current_actor = UnrealActor();
			in_component = false;

			// Parse: Begin Actor Class=... Name=...
			Vector<String> tokens = line.split(" ", false);
			for (const String &tok : tokens) {
				if (tok.begins_with("Class=")) {
					current_actor.class_name = tok.substr(6);
				} else if (tok.begins_with("Name=")) {
					current_actor.name = tok.substr(5);
				}
			}
			continue;
		}
		if (line.begins_with("End Actor")) {
			if (in_component && !current_component_props.is_empty()) {
				current_actor.components.push_back(current_component_props);
				current_component_props.clear();
				in_component = false;
			}
			// Build transform from merged properties (actor + first component)
			HashMap<String, String> merged = current_actor.properties;
			if (!current_actor.components.is_empty()) {
				for (const auto &kv : current_actor.components[0]) {
					if (!merged.has(kv.key)) {
						merged[kv.key] = kv.value;
					}
				}
			}
			current_actor.transform = _parse_unreal_transform(merged);
			actors.push_back(current_actor);
			state = S_LEVEL;
			continue;
		}

		if (state == S_ACTOR || state == S_OBJECT) {
			if (line.begins_with("Begin Object")) {
				// Save current component if any
				if (in_component && !current_component_props.is_empty()) {
					current_actor.components.push_back(current_component_props);
					current_component_props.clear();
				}
				in_component = true;
				state = S_OBJECT;
				continue;
			}
			if (line.begins_with("End Object")) {
				if (in_component) {
					current_actor.components.push_back(current_component_props);
					current_component_props.clear();
					in_component = false;
				}
				state = S_ACTOR;
				continue;
			}

			// Key=Value property
			int eq = line.find("=");
			if (eq > 0) {
				String key = line.substr(0, eq).strip_edges();
				String value = line.substr(eq + 1).strip_edges();
				if (key == "ActorLabel") {
					current_actor.label = value.strip_edges().replace("\"", "");
				} else if (in_component) {
					current_component_props[key] = value;
				} else {
					current_actor.properties[key] = value;
				}
			}
		}
	}

	return actors;
}

// ---------------------------------------------------------------------------
// .ini parser
// ---------------------------------------------------------------------------

HashMap<String, HashMap<String, String>> UnrealConverter::parse_ini(const String &p_content) {
	HashMap<String, HashMap<String, String>> result;
	Vector<String> lines = p_content.split("\n");
	String current_section = "Global";

	for (int li = 0; li < lines.size(); li++) {
		String line = lines[li].strip_edges();
		if (line.is_empty() || line.begins_with(";") || line.begins_with("#")) {
			continue;
		}
		if (line.begins_with("[") && line.ends_with("]")) {
			current_section = line.substr(1, line.length() - 2);
			if (!result.has(current_section)) {
				result[current_section] = HashMap<String, String>();
			}
			continue;
		}
		int eq = line.find("=");
		if (eq > 0) {
			String key = line.substr(0, eq).strip_edges();
			String value = line.substr(eq + 1).strip_edges();
			// Handle +key (append) and -key (remove) prefixes from Unreal ini format
			if (key.begins_with("+") || key.begins_with("-") || key.begins_with(".") || key.begins_with("!")) {
				key = key.substr(1);
			}
			if (!result.has(current_section)) {
				result[current_section] = HashMap<String, String>();
			}
			result[current_section][key] = value;
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// .tscn generation
// ---------------------------------------------------------------------------

String UnrealConverter::_generate_t3d_tscn(const Vector<UnrealActor> &p_actors) {
	String out;
	out += "[gd_scene load_steps=" + itos(1 + p_actors.size()) + " format=3]\n\n";
	out += "[node name=\"UnrealLevel\" type=\"Node3D\"]\n\n";

	for (int i = 0; i < p_actors.size(); i++) {
		const UnrealActor &actor = p_actors[i];
		String godot_type = _unreal_class_to_godot_type(actor.class_name);
		String node_name = actor.label.is_empty() ? actor.name : actor.label;
		// Sanitize name for .tscn
		node_name = node_name.replace(" ", "_").replace("/", "_").replace("\\", "_");
		if (node_name.is_empty()) {
			node_name = "Actor_" + itos(i);
		}

		out += "[node name=\"" + node_name + "\" type=\"" + godot_type + "\" parent=\".\"]\n";

		const Transform3D &tf = actor.transform;
		Vector3 pos = tf.origin;
		out += "position = Vector3(" + rtos(pos.x) + ", " + rtos(pos.y) + ", " + rtos(pos.z) + ")\n";

		// Emit basis as rotation (Euler XYZ in degrees)
		Vector3 euler = tf.basis.get_euler();
		out += "rotation = Vector3(" + rtos(euler.x) + ", " + rtos(euler.y) + ", " + rtos(euler.z) + ")\n";

		Vector3 scl = tf.basis.get_scale();
		if (!scl.is_equal_approx(Vector3(1, 1, 1))) {
			out += "scale = Vector3(" + rtos(scl.x) + ", " + rtos(scl.y) + ", " + rtos(scl.z) + ")\n";
		}

		// Light-specific properties
		if (godot_type == "OmniLight3D" || godot_type == "SpotLight3D" || godot_type == "DirectionalLight3D") {
			// Gather from merged component properties
			HashMap<String, String> merged = actor.properties;
			if (!actor.components.is_empty()) {
				for (const auto &kv : actor.components[0]) {
					merged[kv.key] = kv.value;
				}
			}
			if (merged.has("Intensity")) {
				float intensity = merged["Intensity"].to_float();
				// Unreal intensity (lm) → Godot energy (approximate mapping)
				out += "light_energy = " + rtos(intensity / 1000.0f) + "\n";
			}
			if (merged.has("LightColor")) {
				// Format: (R=255,G=255,B=200,A=255)
				String lc_str = merged["LightColor"];
				if (!lc_str.is_empty()) {
					Vector3 rgb = _parse_unreal_vector(lc_str.replace("R=", "X=").replace("G=", "Y=").replace("B=", "Z=").replace(",A=", ",W="));
					out += "light_color = Color(" + rtos(rgb.x / 255.0f) + ", " + rtos(rgb.y / 255.0f) + ", " + rtos(rgb.z / 255.0f) + ")\n";
				}
			}
		}

		// Static mesh path as comment
		if (godot_type == "MeshInstance3D") {
			HashMap<String, String> merged = actor.properties;
			if (!actor.components.is_empty()) {
				for (const auto &kv : actor.components[0]) {
					merged[kv.key] = kv.value;
				}
			}
			if (merged.has("StaticMesh")) {
				out += "# StaticMesh: " + merged["StaticMesh"] + "\n";
			}
		}

		out += "\n";
	}

	return out;
}

// ---------------------------------------------------------------------------
// convert_t3d
// ---------------------------------------------------------------------------

Error UnrealConverter::convert_t3d(const String &p_t3d_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(p_output_dir);
	}

	Error fe = OK;
	Ref<FileAccess> fa = FileAccess::open(p_t3d_path, FileAccess::READ, &fe);
	if (fa.is_null()) {
		print_error("UnrealConverter: cannot open T3D: " + p_t3d_path);
		return fe;
	}
	String content = fa->get_as_text();
	fa.unref();

	Vector<UnrealActor> actors = parse_t3d(content);
	if (actors.is_empty()) {
		WARN_PRINT("UnrealConverter: no actors found in " + p_t3d_path);
	}

	String tscn_content = _generate_t3d_tscn(actors);
	String base_name = p_t3d_path.get_file().get_basename();
	String out_path = p_output_dir.path_join(base_name + ".tscn");

	Ref<FileAccess> out_fa = FileAccess::open(out_path, FileAccess::WRITE);
	if (out_fa.is_null()) {
		print_error("UnrealConverter: cannot write .tscn: " + out_path);
		return ERR_CANT_CREATE;
	}
	out_fa->store_string(tscn_content);
	print_line("UnrealConverter: wrote level scene " + out_path);
	return OK;
}

// ---------------------------------------------------------------------------
// convert_uproject
// ---------------------------------------------------------------------------

Error UnrealConverter::convert_uproject(const String &p_uproject_path, const String &p_output_dir) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_valid()) {
		da->make_dir_recursive(p_output_dir);
	}

	Error fe = OK;
	Ref<FileAccess> fa = FileAccess::open(p_uproject_path, FileAccess::READ, &fe);
	if (fa.is_null()) {
		print_error("UnrealConverter: cannot open .uproject: " + p_uproject_path);
		return fe;
	}
	String json_text = fa->get_as_text();
	fa.unref();

	Variant parsed = JSON::parse_string(json_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		print_error("UnrealConverter: .uproject is not valid JSON: " + p_uproject_path);
		return ERR_PARSE_ERROR;
	}
	Dictionary uproject = parsed;

	String engine_version = uproject.get("EngineAssociation", "unknown");
	String description = uproject.get("Description", "");
	Array plugins = uproject.get("Plugins", Array());
	Array modules = uproject.get("Modules", Array());

	// Generate an info .tscn that acts as a project overview scene
	String tscn;
	tscn += "[gd_scene load_steps=1 format=3]\n\n";
	tscn += "[node name=\"UnrealProject\" type=\"Node\"]\n";
	tscn += "# Unreal Engine version: " + engine_version + "\n";
	tscn += "# Description: " + description + "\n";
	tscn += "# Modules:\n";
	for (int i = 0; i < modules.size(); i++) {
		Dictionary mod = modules[i];
		String mod_name = mod.get("Name", "");
		String mod_type = mod.get("Type", "");
		tscn += "#   - " + mod_name + " (" + mod_type + ")\n";
	}
	tscn += "# Plugins:\n";
	for (int i = 0; i < plugins.size(); i++) {
		Dictionary plug = plugins[i];
		String plug_name = plug.get("Name", "");
		bool enabled = plug.get("Enabled", true);
		tscn += "#   - " + plug_name + " (enabled=" + (enabled ? "true" : "false") + ")\n";
	}
	tscn += "\n";

	String proj_name = p_uproject_path.get_file().get_basename();
	String out_path = p_output_dir.path_join(proj_name + "_project.tscn");

	Ref<FileAccess> out_fa = FileAccess::open(out_path, FileAccess::WRITE);
	if (out_fa.is_null()) {
		print_error("UnrealConverter: cannot write project scene: " + out_path);
		return ERR_CANT_CREATE;
	}
	out_fa->store_string(tscn);
	print_line("UnrealConverter: wrote project overview " + out_path);

	// Also scan for T3D maps in the project's Content directory
	String content_dir = p_uproject_path.get_base_dir().path_join("Content");
	Ref<DirAccess> content_da = DirAccess::open(content_dir);
	if (content_da.is_valid()) {
		content_da->list_dir_begin();
		String entry = content_da->get_next();
		while (!entry.is_empty()) {
			if (!content_da->current_is_dir()) {
				String ext = entry.get_extension().to_lower();
				if (ext == "t3d") {
					String map_path = content_dir.path_join(entry);
					convert_t3d(map_path, p_output_dir);
				}
			}
			entry = content_da->get_next();
		}
		content_da->list_dir_end();
	}

	return OK;
}

#endif // TOOLS_ENABLED
