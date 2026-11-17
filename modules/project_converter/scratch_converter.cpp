/**************************************************************************/
/*  scratch_converter.cpp                                                 */
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

#include "scratch_converter.h"

#ifdef TOOLS_ENABLED

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/string/char_utils.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "modules/zip/zip_reader.h"

// ---------------------------------------------------------------------------
// Helper: build an indentation string
// ---------------------------------------------------------------------------
static String _indent(int p_level) {
	String s;
	for (int i = 0; i < p_level; i++) {
		s += "\t";
	}
	return s;
}

// ---------------------------------------------------------------------------
// Public entry point
// ---------------------------------------------------------------------------
Error ScratchConverter::convert(const String &p_sb3_path, const String &p_output_dir) {
	// Ensure output directory exists
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		print_error("ScratchConverter: cannot create DirAccess");
		return ERR_CANT_CREATE;
	}
	Error dir_err = da->make_dir_recursive(p_output_dir);
	if (dir_err != OK) {
		print_error("ScratchConverter: failed to create output dir: " + p_output_dir);
		return dir_err;
	}

	// Open the .sb3 (which is a ZIP archive)
	Ref<ZIPReader> zip;
	zip.instantiate();
	Error zip_err = zip->open(p_sb3_path);
	if (zip_err != OK) {
		print_error("ScratchConverter: cannot open .sb3 file: " + p_sb3_path);
		return zip_err;
	}

	// Read project.json from the archive
	PackedByteArray json_bytes = zip->read_file("project.json", true);
	if (json_bytes.is_empty()) {
		print_error("ScratchConverter: project.json not found inside " + p_sb3_path);
		zip->close();
		return ERR_FILE_NOT_FOUND;
	}

	// Write all archive entries to the output dir so assets can be resolved
	PackedStringArray all_files = zip->get_files();
	for (int i = 0; i < all_files.size(); i++) {
		const String &entry = all_files[i];
		if (entry == "project.json") {
			continue;
		}
		PackedByteArray data = zip->read_file(entry, true);
		String dst = p_output_dir.path_join(entry);
		Ref<FileAccess> fa = FileAccess::open(dst, FileAccess::WRITE);
		if (fa.is_valid()) {
			fa->store_buffer(data);
		}
	}
	zip->close();

	// Parse project.json
	String json_text = String::utf8((const char *)json_bytes.ptr(), json_bytes.size());
	Variant parsed = JSON::parse_string(json_text);
	if (parsed.get_type() != Variant::DICTIONARY) {
		print_error("ScratchConverter: project.json is not a valid JSON object");
		return ERR_PARSE_ERROR;
	}
	Dictionary project = parsed;

	Array targets = project.get("targets", Array());
	if (targets.is_empty()) {
		WARN_PRINT("ScratchConverter: project.json has no targets");
	}

	// Build richer target descriptors for tscn generation
	Array target_descs;
	for (int ti = 0; ti < targets.size(); ti++) {
		Dictionary tgt = targets[ti];
		String tgt_name = tgt.get("name", String("Target") + itos(ti));
		bool is_stage = tgt.get("isStage", false);

		// Extract assets
		Array costumes = tgt.get("costumes", Array());
		Array sounds = tgt.get("sounds", Array());
		_extract_assets(p_output_dir, p_output_dir, costumes, sounds);

		// Generate GDScript for this target's blocks
		Dictionary blocks = tgt.get("blocks", Dictionary());
		String script_content = _blocks_to_gdscript(blocks, tgt_name);

		String safe_name = tgt_name.replace(" ", "_");
		String script_path = p_output_dir.path_join(safe_name + ".gd");
		if (!script_content.is_empty()) {
			_write_script(script_path, script_content);
		}

		Dictionary desc;
		desc["name"] = tgt_name;
		desc["safe_name"] = safe_name;
		desc["is_stage"] = is_stage;
		desc["x"] = tgt.get("x", 0.0);
		desc["y"] = tgt.get("y", 0.0);
		desc["size"] = tgt.get("size", 100.0);
		desc["direction"] = tgt.get("direction", 90.0);
		desc["visible"] = tgt.get("visible", true);
		desc["costumes"] = costumes;
		desc["has_script"] = !script_content.is_empty();
		desc["script_path"] = script_path;
		target_descs.push_back(desc);
	}

	// Generate and write the .tscn file
	String tscn_content = _generate_tscn(target_descs, p_output_dir);
	String project_name = p_sb3_path.get_file().get_basename();
	String tscn_path = p_output_dir.path_join(project_name + ".tscn");

	Ref<FileAccess> tscn_file = FileAccess::open(tscn_path, FileAccess::WRITE);
	if (tscn_file.is_null()) {
		print_error("ScratchConverter: cannot write .tscn to " + tscn_path);
		return ERR_CANT_CREATE;
	}
	tscn_file->store_string(tscn_content);
	print_line("ScratchConverter: wrote scene to " + tscn_path);
	return OK;
}

// ---------------------------------------------------------------------------
// Block → GDScript
// ---------------------------------------------------------------------------

String ScratchConverter::_blocks_to_gdscript(const Dictionary &p_blocks, const String &p_target_name) {
	if (p_blocks.is_empty()) {
		return String();
	}

	// Collect all top-level hat block IDs
	Vector<String> hat_ids;
	Array block_keys = p_blocks.keys();
	for (int i = 0; i < block_keys.size(); i++) {
		String bid = block_keys[i];
		Dictionary blk = p_blocks[bid];
		bool top_level = blk.get("topLevel", false);
		if (!top_level) {
			continue;
		}
		String opcode = blk.get("opcode", "");
		if (opcode.begins_with("event_") || opcode.begins_with("control_start")) {
			hat_ids.push_back(bid);
		}
	}

	if (hat_ids.is_empty()) {
		return String();
	}

	String code;
	code += "extends Node2D\n";
	code += "# Auto-generated from Scratch target: " + p_target_name + "\n\n";

	for (const String &hat_id : hat_ids) {
		code += _chain_to_gdscript(hat_id, p_blocks, 0);
		code += "\n";
	}
	return code;
}

String ScratchConverter::_chain_to_gdscript(const String &p_hat_id, const Dictionary &p_blocks, int p_indent) {
	if (!p_blocks.has(p_hat_id)) {
		return String();
	}
	Dictionary hat_blk = p_blocks[p_hat_id];
	String opcode = hat_blk.get("opcode", "");

	String func_sig;
	if (opcode == "event_whenflagclicked") {
		func_sig = "func _ready():";
	} else if (opcode == "event_whenkeypressed") {
		Dictionary fields = hat_blk.get("fields", Dictionary());
		Dictionary key_field = fields.get("KEY_OPTION", Dictionary());
		String key_name = key_field.get("0", "space");
		func_sig = "func _on_key_" + key_name.replace(" ", "_") + "_pressed():";
	} else if (opcode == "event_whenstageclicked" || opcode == "event_whenthisspriteclicked") {
		func_sig = "func _on_clicked():";
	} else if (opcode == "event_whenbroadcastreceived") {
		Dictionary fields = hat_blk.get("fields", Dictionary());
		Dictionary msg_field = fields.get("BROADCAST_OPTION", Dictionary());
		String msg = msg_field.get("0", "message");
		func_sig = "func _on_broadcast_" + msg.replace(" ", "_").to_lower() + "():";
	} else if (opcode == "event_whenbackdropswitchesto") {
		Dictionary fields = hat_blk.get("fields", Dictionary());
		Dictionary bd_field = fields.get("BACKDROP", Dictionary());
		String backdrop = bd_field.get("0", "backdrop1");
		func_sig = "func _on_backdrop_" + backdrop.replace(" ", "_").to_lower() + "():";
	} else if (opcode == "control_start_as_clone") {
		func_sig = "func _on_clone_start():";
	} else {
		func_sig = "func _on_" + opcode.replace(":", "_") + "():";
	}

	String result = _indent(p_indent) + func_sig + "\n";

	// Walk the chain
	String next_id = hat_blk.get("next", Variant());
	if (next_id.is_empty()) {
		result += _indent(p_indent + 1) + "pass\n";
	} else {
		String cur = next_id;
		while (!cur.is_empty()) {
			result += _block_to_gdscript(cur, p_blocks, p_indent + 1);
			if (!p_blocks.has(cur)) {
				break;
			}
			Dictionary cur_blk = p_blocks[cur];
			Variant nxt = cur_blk.get("next", Variant());
			cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
		}
	}
	return result;
}

String ScratchConverter::_block_to_gdscript(const String &p_block_id, const Dictionary &p_blocks, int p_indent) {
	if (!p_blocks.has(p_block_id)) {
		return String();
	}
	Dictionary blk = p_blocks[p_block_id];
	String opcode = blk.get("opcode", "");
	String ind = _indent(p_indent);

	// --- Motion ---
	if (opcode == "motion_movesteps") {
		String steps = _input_to_expr(p_block_id, "STEPS", p_blocks);
		// rotation (radians) in Godot's Node2D gives the facing direction directly.
		return ind + "position += Vector2(cos(rotation), sin(rotation)) * " + steps + "\n";
	}
	if (opcode == "motion_turnright") {
		String deg = _input_to_expr(p_block_id, "DEGREES", p_blocks);
		return ind + "rotation_degrees += " + deg + "\n";
	}
	if (opcode == "motion_turnleft") {
		String deg = _input_to_expr(p_block_id, "DEGREES", p_blocks);
		return ind + "rotation_degrees -= " + deg + "\n";
	}
	if (opcode == "motion_gotoxy") {
		String x = _input_to_expr(p_block_id, "X", p_blocks);
		String y = _input_to_expr(p_block_id, "Y", p_blocks);
		return ind + "position = Vector2(" + x + ", -(" + y + "))\n";
	}
	if (opcode == "motion_glideto") {
		String secs = _input_to_expr(p_block_id, "SECS", p_blocks);
		String x = _input_to_expr(p_block_id, "X", p_blocks);
		String y = _input_to_expr(p_block_id, "Y", p_blocks);
		return ind + "# glide " + secs + "s to (" + x + ", " + y + ")\n" +
				ind + "create_tween().tween_property(self, \"position\", Vector2(" + x + ", -(" + y + ")), " + secs + ")\n" +
				ind + "await get_tree().create_timer(" + secs + ").timeout\n";
	}
	if (opcode == "motion_setx") {
		String x = _input_to_expr(p_block_id, "X", p_blocks);
		return ind + "position.x = " + x + "\n";
	}
	if (opcode == "motion_sety") {
		String y = _input_to_expr(p_block_id, "Y", p_blocks);
		return ind + "position.y = -(" + y + ")\n";
	}
	if (opcode == "motion_changexby") {
		String dx = _input_to_expr(p_block_id, "DX", p_blocks);
		return ind + "position.x += " + dx + "\n";
	}
	if (opcode == "motion_changeyby") {
		String dy = _input_to_expr(p_block_id, "DY", p_blocks);
		return ind + "position.y -= " + dy + "\n";
	}
	if (opcode == "motion_pointindirection") {
		String dir = _input_to_expr(p_block_id, "DIRECTION", p_blocks);
		return ind + "rotation_degrees = " + dir + " - 90.0\n";
	}
	if (opcode == "motion_goto") {
		String target = _input_to_expr(p_block_id, "TO", p_blocks);
		if (target == "\"_random_\"") {
			return ind + "position = Vector2(randf_range(-240.0, 240.0), randf_range(-180.0, 180.0))\n";
		} else if (target == "\"_mouse_\"") {
			return ind + "position = get_viewport().get_mouse_position()\n";
		}
		return ind + "if has_node(\"../\" + " + target + "):\n" +
				_indent(p_indent + 1) + "position = get_node(\"../\" + " + target + ").position\n";
	}
	if (opcode == "motion_glidesecstoxy") {
		String secs = _input_to_expr(p_block_id, "SECS", p_blocks);
		String tx = _input_to_expr(p_block_id, "X", p_blocks);
		String ty = _input_to_expr(p_block_id, "Y", p_blocks);
		return ind + "create_tween().tween_property(self, \"position\", Vector2(" + tx + ", -(" + ty + ")), " + secs + ")\n" +
				ind + "await get_tree().create_timer(" + secs + ").timeout\n";
	}
	if (opcode == "motion_pointtowards") {
		String target = _input_to_expr(p_block_id, "TOWARDS", p_blocks);
		if (target == "\"_mouse_\"") {
			return ind + "look_at(get_viewport().get_mouse_position())\n";
		}
		return ind + "if has_node(\"../\" + " + target + "):\n" +
				_indent(p_indent + 1) + "look_at(get_node(\"../\" + " + target + ").position)\n";
	}
	if (opcode == "motion_ifonedgebounce") {
		String ind1 = _indent(p_indent + 1);
		return ind + "if position.x < -240.0 or position.x > 240.0 or position.y < -180.0 or position.y > 180.0:\n" +
				ind1 + "rotation_degrees = -rotation_degrees\n" +
				ind1 + "position = position.clamp(Vector2(-240.0, -180.0), Vector2(240.0, 180.0))\n";
	}
	if (opcode == "motion_setrotationstyle") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary style_field = fields.get("STYLE", Dictionary());
		String style = style_field.get("0", "all around");
		return ind + "# set rotation style: " + style + "\n";
	}

	// --- Looks ---
	if (opcode == "looks_say") {
		String msg = _input_to_expr(p_block_id, "MESSAGE", p_blocks);
		return ind + "print(" + msg + ")\n";
	}
	if (opcode == "looks_sayforsecs") {
		String msg = _input_to_expr(p_block_id, "MESSAGE", p_blocks);
		String secs = _input_to_expr(p_block_id, "SECS", p_blocks);
		return ind + "print(" + msg + ")\n" +
				ind + "await get_tree().create_timer(" + secs + ").timeout\n";
	}
	if (opcode == "looks_think" || opcode == "looks_thinkforsecs") {
		String msg = _input_to_expr(p_block_id, "MESSAGE", p_blocks);
		return ind + "print(\"thinking: \", " + msg + ")\n";
	}
	if (opcode == "looks_show") {
		return ind + "visible = true\n";
	}
	if (opcode == "looks_hide") {
		return ind + "visible = false\n";
	}
	if (opcode == "looks_switchcostumeto") {
		String costume = _input_to_expr(p_block_id, "COSTUME", p_blocks);
		return ind + "# switch costume to " + costume + "\n" +
				ind + "_switch_costume(" + costume + ")\n";
	}
	if (opcode == "looks_nextcostume") {
		return ind + "_next_costume()\n";
	}
	if (opcode == "looks_setsizeto") {
		String size = _input_to_expr(p_block_id, "SIZE", p_blocks);
		return ind + "scale = Vector2.ONE * (" + size + " / 100.0)\n";
	}
	if (opcode == "looks_changesizeby") {
		String change = _input_to_expr(p_block_id, "CHANGE", p_blocks);
		return ind + "scale += Vector2.ONE * (" + change + " / 100.0)\n";
	}
	if (opcode == "looks_switchbackdropto") {
		String backdrop = _input_to_expr(p_block_id, "BACKDROP", p_blocks);
		return ind + "if has_node(\"/root/Stage\"):\n" +
				_indent(p_indent + 1) + "get_node(\"/root/Stage\").play(" + backdrop + ")\n";
	}
	if (opcode == "looks_nextbackdrop") {
		return ind + "# looks_nextbackdrop: advance to the next backdrop on the stage\n";
	}
	if (opcode == "looks_changeeffectby") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary eff_field = fields.get("EFFECT", Dictionary());
		String effect = eff_field.get("0", "COLOR");
		String change = _input_to_expr(p_block_id, "CHANGE", p_blocks);
		return ind + "# looks_changeeffectby: " + effect + " by " + change + "\n";
	}
	if (opcode == "looks_seteffectto") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary eff_field = fields.get("EFFECT", Dictionary());
		String effect = eff_field.get("0", "COLOR");
		String val = _input_to_expr(p_block_id, "VALUE", p_blocks);
		return ind + "# looks_seteffectto: " + effect + " = " + val + "\n";
	}
	if (opcode == "looks_cleargraphiceffects") {
		return ind + "# looks_cleargraphiceffects: reset all graphic effects\n";
	}
	if (opcode == "looks_gotofrontback") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary fb_field = fields.get("FRONT_BACK", Dictionary());
		String front_back = fb_field.get("0", "front");
		if (front_back == "front") {
			return ind + "z_index = 100\n";
		}
		return ind + "z_index = -100\n";
	}
	if (opcode == "looks_goforwardbackwardlayers") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary fb_field = fields.get("FORWARD_BACKWARD", Dictionary());
		String dir = fb_field.get("0", "forward");
		String layers = _input_to_expr(p_block_id, "NUM", p_blocks);
		if (dir == "forward") {
			return ind + "z_index += " + layers + "\n";
		}
		return ind + "z_index -= " + layers + "\n";
	}

	// --- Sound ---
	if (opcode == "sound_play") {
		String sound = _input_to_expr(p_block_id, "SOUND_MENU", p_blocks);
		return ind + "# play sound " + sound + "\n" +
				ind + "_play_sound(" + sound + ")\n";
	}
	if (opcode == "sound_playuntildone") {
		String sound = _input_to_expr(p_block_id, "SOUND_MENU", p_blocks);
		return ind + "# play sound until done " + sound + "\n" +
				ind + "await _play_sound_until_done(" + sound + ")\n";
	}
	if (opcode == "sound_stopallsounds") {
		return ind + "AudioServer.set_bus_mute(AudioServer.get_bus_index(\"Master\"), true)\n";
	}
	if (opcode == "sound_changevolumeby") {
		String vol = _input_to_expr(p_block_id, "VOLUME", p_blocks);
		return ind + "AudioServer.set_bus_volume_db(0, AudioServer.get_bus_volume_db(0) + " + vol + ")\n";
	}
	if (opcode == "sound_setvolumeto") {
		String vol = _input_to_expr(p_block_id, "VOLUME", p_blocks);
		return ind + "AudioServer.set_bus_volume_db(0, linear_to_db(" + vol + " * 0.01))\n";
	}
	if (opcode == "sound_changeeffectby") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary eff_field = fields.get("EFFECT", Dictionary());
		String effect = eff_field.get("0", "PITCH");
		String val = _input_to_expr(p_block_id, "VALUE", p_blocks);
		return ind + "# sound_changeeffectby: " + effect + " by " + val + "\n";
	}
	if (opcode == "sound_seteffectto") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary eff_field = fields.get("EFFECT", Dictionary());
		String effect = eff_field.get("0", "PITCH");
		String val = _input_to_expr(p_block_id, "VALUE", p_blocks);
		return ind + "# sound_seteffectto: " + effect + " = " + val + "\n";
	}
	if (opcode == "sound_cleareffects") {
		return ind + "# sound_cleareffects: reset all audio effects\n";
	}

	// --- Control ---
	if (opcode == "control_wait") {
		String secs = _input_to_expr(p_block_id, "DURATION", p_blocks);
		return ind + "await get_tree().create_timer(" + secs + ").timeout\n";
	}
	if (opcode == "control_repeat") {
		String times = _input_to_expr(p_block_id, "TIMES", p_blocks);
		Dictionary inputs = blk.get("inputs", Dictionary());
		String body;
		if (inputs.has("SUBSTACK")) {
			Array substack = inputs["SUBSTACK"];
			if (substack.size() > 1 && substack[1].get_type() == Variant::STRING) {
				String sub_id = substack[1];
				String cur = sub_id;
				while (!cur.is_empty()) {
					body += _block_to_gdscript(cur, p_blocks, p_indent + 1);
					if (!p_blocks.has(cur)) {
						break;
					}
					Dictionary cb = p_blocks[cur];
					Variant nxt = cb.get("next", Variant());
					cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
				}
			}
		}
		if (body.is_empty()) {
			body = _indent(p_indent + 1) + "pass\n";
		}
		return ind + "for _i in range(" + times + "):\n" + body;
	}
	if (opcode == "control_forever") {
		Dictionary inputs = blk.get("inputs", Dictionary());
		String body;
		if (inputs.has("SUBSTACK")) {
			Array substack = inputs["SUBSTACK"];
			if (substack.size() > 1 && substack[1].get_type() == Variant::STRING) {
				String sub_id = substack[1];
				String cur = sub_id;
				while (!cur.is_empty()) {
					body += _block_to_gdscript(cur, p_blocks, p_indent + 1);
					if (!p_blocks.has(cur)) {
						break;
					}
					Dictionary cb = p_blocks[cur];
					Variant nxt = cb.get("next", Variant());
					cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
				}
			}
		}
		if (body.is_empty()) {
			body = _indent(p_indent + 1) + "await get_tree().process_frame\n";
		}
		return ind + "while true:\n" + body;
	}
	if (opcode == "control_if") {
		String cond = _input_to_expr(p_block_id, "CONDITION", p_blocks);
		Dictionary inputs = blk.get("inputs", Dictionary());
		String body;
		if (inputs.has("SUBSTACK")) {
			Array substack = inputs["SUBSTACK"];
			if (substack.size() > 1 && substack[1].get_type() == Variant::STRING) {
				String sub_id = substack[1];
				String cur = sub_id;
				while (!cur.is_empty()) {
					body += _block_to_gdscript(cur, p_blocks, p_indent + 1);
					if (!p_blocks.has(cur)) {
						break;
					}
					Dictionary cb = p_blocks[cur];
					Variant nxt = cb.get("next", Variant());
					cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
				}
			}
		}
		if (body.is_empty()) {
			body = _indent(p_indent + 1) + "pass\n";
		}
		return ind + "if " + cond + ":\n" + body;
	}
	if (opcode == "control_if_else") {
		String cond = _input_to_expr(p_block_id, "CONDITION", p_blocks);
		Dictionary inputs = blk.get("inputs", Dictionary());

		auto collect_body = [&](const String &p_key) -> String {
			String body;
			if (inputs.has(p_key)) {
				Array substack = inputs[p_key];
				if (substack.size() > 1 && substack[1].get_type() == Variant::STRING) {
					String sub_id = substack[1];
					String cur = sub_id;
					while (!cur.is_empty()) {
						body += _block_to_gdscript(cur, p_blocks, p_indent + 1);
						if (!p_blocks.has(cur)) {
							break;
						}
						Dictionary cb = p_blocks[cur];
						Variant nxt = cb.get("next", Variant());
						cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
					}
				}
			}
			return body.is_empty() ? (_indent(p_indent + 1) + "pass\n") : body;
		};

		String then_body = collect_body("SUBSTACK");
		String else_body = collect_body("SUBSTACK2");
		return ind + "if " + cond + ":\n" + then_body + ind + "else:\n" + else_body;
	}
	if (opcode == "control_stop") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary stop_opt = fields.get("STOP_OPTION", Dictionary());
		String stop_what = stop_opt.get("0", "all");
		if (stop_what == "all") {
			return ind + "get_tree().quit()\n";
		}
		return ind + "return\n";
	}
	if (opcode == "control_wait_until") {
		String cond = _input_to_expr(p_block_id, "CONDITION", p_blocks);
		return ind + "while not (" + cond + "):\n" +
				_indent(p_indent + 1) + "await get_tree().process_frame\n";
	}
	if (opcode == "control_repeat_until") {
		String cond = _input_to_expr(p_block_id, "CONDITION", p_blocks);
		Dictionary inputs = blk.get("inputs", Dictionary());
		String body;
		if (inputs.has("SUBSTACK")) {
			Array substack = inputs["SUBSTACK"];
			if (substack.size() > 1 && substack[1].get_type() == Variant::STRING) {
				String sub_id = substack[1];
				String cur = sub_id;
				while (!cur.is_empty()) {
					body += _block_to_gdscript(cur, p_blocks, p_indent + 1);
					if (!p_blocks.has(cur)) {
						break;
					}
					Dictionary cb = p_blocks[cur];
					Variant nxt = cb.get("next", Variant());
					cur = (nxt.get_type() == Variant::STRING) ? String(nxt) : String();
				}
			}
		}
		body += _indent(p_indent + 1) + "await get_tree().process_frame\n";
		return ind + "while not (" + cond + "):\n" + body;
	}
	if (opcode == "control_create_clone_of") {
		String target = _input_to_expr(p_block_id, "CLONE_OPTION", p_blocks);
		if (target == "\"_myself_\"" || target.is_empty()) {
			return ind + "var _clone = duplicate()\n" +
					ind + "get_parent().add_child(_clone)\n";
		}
		return ind + "if has_node(\"../\" + " + target + "):\n" +
				_indent(p_indent + 1) + "var _clone = get_node(\"../\" + " + target + ").duplicate()\n" +
				_indent(p_indent + 1) + "get_parent().add_child(_clone)\n";
	}
	if (opcode == "control_delete_this_clone") {
		return ind + "queue_free()\n";
	}

	// --- Operators ---
	// Operator blocks are expression nodes, not statement nodes.
	// They should be handled by _input_to_expr, but if they appear at top level as no-ops, emit a comment.
	if (opcode.begins_with("operator_")) {
		return ind + "# (operator expression: " + opcode + ")\n";
	}

	// --- Events (broadcast / receive) ---
	if (opcode == "event_broadcast") {
		String msg = _input_to_expr(p_block_id, "BROADCAST_INPUT", p_blocks);
		return ind + "get_tree().call_group(\"scratchtargets\", \"_on_broadcast\", " + msg + ")\n";
	}
	if (opcode == "event_broadcastandwait") {
		String msg = _input_to_expr(p_block_id, "BROADCAST_INPUT", p_blocks);
		return ind + "get_tree().call_group(\"scratchtargets\", \"_on_broadcast\", " + msg + ")\n" +
				ind + "await get_tree().process_frame\n";
	}

	// --- Data (variables) ---
	if (opcode == "data_setvariableto") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary var_field = fields.get("VARIABLE", Dictionary());
		String var_name = var_field.get("0", "variable");
		String val = _input_to_expr(p_block_id, "VALUE", p_blocks);
		return ind + var_name.replace(" ", "_") + " = " + val + "\n";
	}
	if (opcode == "data_changevariableby") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary var_field = fields.get("VARIABLE", Dictionary());
		String var_name = var_field.get("0", "variable");
		String val = _input_to_expr(p_block_id, "VALUE", p_blocks);
		return ind + var_name.replace(" ", "_") + " += " + val + "\n";
	}
	if (opcode == "data_showvariable" || opcode == "data_hidevariable") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary var_field = fields.get("VARIABLE", Dictionary());
		String var_name = var_field.get("0", "variable");
		return ind + "# " + opcode + " " + var_name + "\n";
	}

	// --- Data: lists ---
	if (opcode == "data_addtolist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		String item = _input_to_expr(p_block_id, "ITEM", p_blocks);
		return ind + "_list_" + list_name.replace(" ", "_") + ".append(" + item + ")\n";
	}
	if (opcode == "data_deleteoflist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		String idx = _input_to_expr(p_block_id, "INDEX", p_blocks);
		return ind + "_list_" + list_name.replace(" ", "_") + ".remove_at(int(" + idx + ") - 1)\n";
	}
	if (opcode == "data_deletealloflist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		return ind + "_list_" + list_name.replace(" ", "_") + ".clear()\n";
	}
	if (opcode == "data_insertatlist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		String item = _input_to_expr(p_block_id, "ITEM", p_blocks);
		String idx = _input_to_expr(p_block_id, "INDEX", p_blocks);
		return ind + "_list_" + list_name.replace(" ", "_") + ".insert(int(" + idx + ") - 1, " + item + ")\n";
	}
	if (opcode == "data_replaceitemoflist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		String item = _input_to_expr(p_block_id, "ITEM", p_blocks);
		String idx = _input_to_expr(p_block_id, "INDEX", p_blocks);
		return ind + "_list_" + list_name.replace(" ", "_") + "[int(" + idx + ") - 1] = " + item + "\n";
	}
	if (opcode == "data_showlist" || opcode == "data_hidelist") {
		Dictionary fields = blk.get("fields", Dictionary());
		Dictionary list_field = fields.get("LIST", Dictionary());
		String list_name = list_field.get("0", "list");
		return ind + "# " + opcode + ": " + list_name + "\n";
	}

	// --- Sensing ---
	if (opcode == "sensing_askandwait") {
		String question = _input_to_expr(p_block_id, "QUESTION", p_blocks);
		return ind + "var _answer = await _ask_dialog(" + question + ")\n";
	}
	if (opcode == "sensing_resettimer") {
		return ind + "_timer_start = Time.get_ticks_msec() * 0.001\n";
	}
	if (opcode == "sensing_touchingobject") {
		String obj = _input_to_expr(p_block_id, "TOUCHINGOBJECTMENU", p_blocks);
		return ind + "# sensing_touchingobject: " + obj + " — use overlaps_body() on an Area2D\n";
	}
	if (opcode == "sensing_touchingcolor") {
		return ind + "# sensing_touchingcolor: color collision not supported in Godot\n";
	}
	if (opcode == "sensing_coloristouchingcolor") {
		return ind + "# sensing_coloristouchingcolor: color-to-color collision not supported in Godot\n";
	}

	// --- Fallback ---
	return ind + "# " + opcode + " (not converted)\n";
}

String ScratchConverter::_input_to_expr(const String &p_block_id, const String &p_input_name, const Dictionary &p_blocks) {
	if (!p_blocks.has(p_block_id)) {
		return "0";
	}
	Dictionary blk = p_blocks[p_block_id];
	Dictionary inputs = blk.get("inputs", Dictionary());

	if (!inputs.has(p_input_name)) {
		return "0";
	}

	// An input is [shadow_type, value_or_block_id]
	// value_or_block_id may be:
	//   - an Array like [type, literal_value] for a primitive input
	//   - a String block ID for a reporter block
	Array input_arr = inputs[p_input_name];
	if (input_arr.size() < 2) {
		return "0";
	}

	Variant inner = input_arr[1];
	if (inner.get_type() == Variant::STRING) {
		String inner_str = inner;
		// Could be a block ID (reporter) or could be a literal
		if (p_blocks.has(inner_str)) {
			// It is a reporter block — recurse to get its expression
			Dictionary rep = p_blocks[inner_str];
			String opcode = rep.get("opcode", "");

			if (opcode == "operator_add") {
				String a = _input_to_expr(inner_str, "NUM1", p_blocks);
				String b = _input_to_expr(inner_str, "NUM2", p_blocks);
				return "(" + a + " + " + b + ")";
			}
			if (opcode == "operator_subtract") {
				String a = _input_to_expr(inner_str, "NUM1", p_blocks);
				String b = _input_to_expr(inner_str, "NUM2", p_blocks);
				return "(" + a + " - " + b + ")";
			}
			if (opcode == "operator_multiply") {
				String a = _input_to_expr(inner_str, "NUM1", p_blocks);
				String b = _input_to_expr(inner_str, "NUM2", p_blocks);
				return "(" + a + " * " + b + ")";
			}
			if (opcode == "operator_divide") {
				String a = _input_to_expr(inner_str, "NUM1", p_blocks);
				String b = _input_to_expr(inner_str, "NUM2", p_blocks);
				return "(" + a + " / " + b + ")";
			}
			if (opcode == "operator_equals") {
				String a = _input_to_expr(inner_str, "OPERAND1", p_blocks);
				String b = _input_to_expr(inner_str, "OPERAND2", p_blocks);
				return "(" + a + " == " + b + ")";
			}
			if (opcode == "operator_lt") {
				String a = _input_to_expr(inner_str, "OPERAND1", p_blocks);
				String b = _input_to_expr(inner_str, "OPERAND2", p_blocks);
				return "(" + a + " < " + b + ")";
			}
			if (opcode == "operator_gt") {
				String a = _input_to_expr(inner_str, "OPERAND1", p_blocks);
				String b = _input_to_expr(inner_str, "OPERAND2", p_blocks);
				return "(" + a + " > " + b + ")";
			}
			if (opcode == "operator_and") {
				String a = _input_to_expr(inner_str, "OPERAND1", p_blocks);
				String b = _input_to_expr(inner_str, "OPERAND2", p_blocks);
				return "(" + a + " and " + b + ")";
			}
			if (opcode == "operator_or") {
				String a = _input_to_expr(inner_str, "OPERAND1", p_blocks);
				String b = _input_to_expr(inner_str, "OPERAND2", p_blocks);
				return "(" + a + " or " + b + ")";
			}
			if (opcode == "operator_not") {
				String a = _input_to_expr(inner_str, "OPERAND", p_blocks);
				return "(not " + a + ")";
			}
			if (opcode == "operator_random") {
				String lo = _input_to_expr(inner_str, "FROM", p_blocks);
				String hi = _input_to_expr(inner_str, "TO", p_blocks);
				return "randi_range(int(" + lo + "), int(" + hi + "))";
			}
			if (opcode == "operator_join") {
				String a = _input_to_expr(inner_str, "STRING1", p_blocks);
				String b = _input_to_expr(inner_str, "STRING2", p_blocks);
				return "(str(" + a + ") + str(" + b + "))";
			}
			if (opcode == "operator_length") {
				String s = _input_to_expr(inner_str, "STRING", p_blocks);
				return "len(str(" + s + "))";
			}
			if (opcode == "operator_mathop") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary op_field = rep_fields.get("OPERATOR", Dictionary());
				String math_op = op_field.get("0", "sqrt");
				String num = _input_to_expr(inner_str, "NUM", p_blocks);
				if (math_op == "sqrt") {
					return "sqrt(" + num + ")";
				}
				if (math_op == "abs") {
					return "abs(" + num + ")";
				}
				if (math_op == "floor") {
					return "floor(" + num + ")";
				}
				if (math_op == "ceiling") {
					return "ceil(" + num + ")";
				}
				if (math_op == "round") {
					return "round(" + num + ")";
				}
				if (math_op == "sin") {
					return "sin(deg_to_rad(" + num + "))";
				}
				if (math_op == "cos") {
					return "cos(deg_to_rad(" + num + "))";
				}
				if (math_op == "tan") {
					return "tan(deg_to_rad(" + num + "))";
				}
				if (math_op == "asin") {
					return "rad_to_deg(asin(" + num + "))";
				}
				if (math_op == "acos") {
					return "rad_to_deg(acos(" + num + "))";
				}
				if (math_op == "atan") {
					return "rad_to_deg(atan(" + num + "))";
				}
				if (math_op == "ln") {
					// GDScript log() is the natural logarithm
					return "log(" + num + ")";
				}
				if (math_op == "log") {
					// Scratch "log" is base-10; convert using change-of-base
					return "(log(" + num + ") / log(10.0))";
				}
				if (math_op == "e ^") {
					return "exp(" + num + ")";
				}
				if (math_op == "10 ^") {
					return "pow(10.0, " + num + ")";
				}
				if (math_op == "2 ^") {
					return "pow(2.0, " + num + ")";
				}
				return "(" + num + " /* " + math_op + " */)";
			}
			if (opcode == "operator_letter_of") {
				String letter_idx = _input_to_expr(inner_str, "LETTER", p_blocks);
				String str_val = _input_to_expr(inner_str, "STRING", p_blocks);
				return "(str(" + str_val + ")[clamp(int(" + letter_idx + ") - 1, 0, len(str(" + str_val + ")) - 1)])";
			}
			if (opcode == "operator_contains") {
				String a = _input_to_expr(inner_str, "STRING1", p_blocks);
				String b = _input_to_expr(inner_str, "STRING2", p_blocks);
				return "(str(" + a + ").find(str(" + b + ")) >= 0)";
			}
			if (opcode == "operator_mod") {
				String a = _input_to_expr(inner_str, "NUM1", p_blocks);
				String b = _input_to_expr(inner_str, "NUM2", p_blocks);
				return "fmod(float(" + a + "), float(" + b + "))";
			}
			if (opcode == "operator_round") {
				String n = _input_to_expr(inner_str, "NUM", p_blocks);
				return "round(float(" + n + "))";
			}
			if (opcode == "motion_goto_menu") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary to_field = rep_fields.get("TO", Dictionary());
				String target = to_field.get("0", "_random_");
				return "\"" + target.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
			}
			if (opcode == "motion_pointtowards_menu") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary towards_field = rep_fields.get("TOWARDS", Dictionary());
				String target = towards_field.get("0", "_mouse_");
				return "\"" + target.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
			}
			if (opcode == "looks_backdrops") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary bd_field = rep_fields.get("BACKDROP", Dictionary());
				String backdrop = bd_field.get("0", "backdrop1");
				return "\"" + backdrop.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
			}
			if (opcode == "looks_costume") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary costume_field = rep_fields.get("COSTUME", Dictionary());
				String costume = costume_field.get("0", "costume1");
				return "\"" + costume.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
			}
			if (opcode == "sensing_answer") {
				return "_answer";
			}
			if (opcode == "sensing_loudness") {
				return "0.0 /* microphone input not supported */";
			}
			if (opcode == "sensing_username") {
				return "\"player\"";
			}
			if (opcode == "sensing_dayssince2000") {
				return "(Time.get_unix_time_from_system() / 86400.0 - 10957.0)";
			}
			if (opcode == "sensing_current") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary current_field = rep_fields.get("CURRENTMENU", Dictionary());
				String component = current_field.get("0", "YEAR");
				if (component == "YEAR") {
					return "Time.get_date_dict_from_system()[\"year\"]";
				} else if (component == "MONTH") {
					return "Time.get_date_dict_from_system()[\"month\"]";
				} else if (component == "DATE") {
					return "Time.get_date_dict_from_system()[\"day\"]";
				} else if (component == "DAYOFWEEK") {
					return "Time.get_date_dict_from_system()[\"weekday\"]";
				} else if (component == "HOUR") {
					return "Time.get_time_dict_from_system()[\"hour\"]";
				} else if (component == "MINUTE") {
					return "Time.get_time_dict_from_system()[\"minute\"]";
				} else if (component == "SECOND") {
					return "Time.get_time_dict_from_system()[\"second\"]";
				}
				return "0";
			}
			if (opcode == "sensing_of") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary prop_field = rep_fields.get("PROPERTY", Dictionary());
				String property = prop_field.get("0", "x position");
				String target = _input_to_expr(inner_str, "OBJECT", p_blocks);
				if (property == "x position") {
					return "(get_node_or_null(\"../\" + " + target + ").position.x if has_node(\"../\" + " + target + ") else 0.0)";
				} else if (property == "y position") {
					return "(-get_node_or_null(\"../\" + " + target + ").position.y if has_node(\"../\" + " + target + ") else 0.0)";
				} else if (property == "direction") {
					return "(get_node_or_null(\"../\" + " + target + ").rotation_degrees + 90.0 if has_node(\"../\" + " + target + ") else 0.0)";
				} else if (property == "size") {
					return "(get_node_or_null(\"../\" + " + target + ").scale.x * 100.0 if has_node(\"../\" + " + target + ") else 100.0)";
				}
				return "0 /* sensing_of: " + property + " */";
			}
			if (opcode == "sensing_distanceto") {
				String target = _input_to_expr(inner_str, "DISTANCETOMENU", p_blocks);
				if (target == "\"_mouse_\"") {
					return "position.distance_to(get_viewport().get_mouse_position())";
				}
				return "(position.distance_to(get_node_or_null(\"../\" + " + target + ").position) if has_node(\"../\" + " + target + ") else 0.0)";
			}
			if (opcode == "data_itemoflist") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary list_field = rep_fields.get("LIST", Dictionary());
				String list_name = list_field.get("0", "list");
				String idx = _input_to_expr(inner_str, "INDEX", p_blocks);
				return "_list_" + list_name.replace(" ", "_") + "[int(" + idx + ") - 1]";
			}
			if (opcode == "data_itemnumoflist") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary list_field = rep_fields.get("LIST", Dictionary());
				String list_name = list_field.get("0", "list");
				String item = _input_to_expr(inner_str, "ITEM", p_blocks);
				return "(_list_" + list_name.replace(" ", "_") + ".find(" + item + ") + 1)";
			}
			if (opcode == "data_lengthoflist") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary list_field = rep_fields.get("LIST", Dictionary());
				String list_name = list_field.get("0", "list");
				return "_list_" + list_name.replace(" ", "_") + ".size()";
			}
			if (opcode == "data_listcontainsitem") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary list_field = rep_fields.get("LIST", Dictionary());
				String list_name = list_field.get("0", "list");
				String item = _input_to_expr(inner_str, "ITEM", p_blocks);
				return "_list_" + list_name.replace(" ", "_") + ".has(" + item + ")";
			}
			if (opcode == "sensing_keypressed") {
				String key = _input_to_expr(inner_str, "KEY_OPTION", p_blocks);
				return "Input.is_key_pressed(KEY_" + key.to_upper() + ")";
			}
			if (opcode == "sensing_mousedown") {
				return "Input.is_mouse_button_pressed(MOUSE_BUTTON_LEFT)";
			}
			if (opcode == "sensing_mousex") {
				return "get_viewport().get_mouse_position().x";
			}
			if (opcode == "sensing_mousey") {
				return "(-get_viewport().get_mouse_position().y)";
			}
			if (opcode == "sensing_timer") {
				return "Time.get_ticks_msec() / 1000.0";
			}
			if (opcode == "data_variable") {
				Dictionary rep_fields = rep.get("fields", Dictionary());
				Dictionary var_field = rep_fields.get("VARIABLE", Dictionary());
				String var_name = var_field.get("0", "variable");
				return var_name.replace(" ", "_");
			}
			if (opcode == "motion_xposition") {
				return "position.x";
			}
			if (opcode == "motion_yposition") {
				return "(-position.y)";
			}
			if (opcode == "motion_direction") {
				return "(rotation_degrees + 90.0)";
			}
			// Unknown reporter — emit a comment placeholder
			return "0 /* " + opcode + " */";
		}
		// Treat it as a literal string
		return "\"" + inner_str.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
	}

	if (inner.get_type() == Variant::ARRAY) {
		// [type_id, value] pair — e.g. [4, "10"] for a number literal
		Array val_arr = inner;
		if (val_arr.size() >= 2) {
			Variant raw = val_arr[1];
			if (raw.get_type() == Variant::STRING) {
				String raw_str = raw;
				// Try to detect if numeric
				bool is_num = true;
				for (int ci = 0; ci < raw_str.length(); ci++) {
					char32_t c = raw_str[ci];
					if (!is_digit(c) && c != '.' && c != '-' && c != 'e' && c != 'E' && c != '+') {
						is_num = false;
						break;
					}
				}
				if (is_num && !raw_str.is_empty()) {
					return raw_str;
				}
				return "\"" + raw_str.replace("\\", "\\\\").replace("\"", "\\\"") + "\"";
			}
			return raw.stringify();
		}
	}

	return "0";
}

// ---------------------------------------------------------------------------
// Asset extraction
// ---------------------------------------------------------------------------

Error ScratchConverter::_extract_assets(const String &p_unzip_dir, const String &p_output_dir, const Array &p_costumes, const Array &p_sounds) {
	// Assets were already extracted into p_unzip_dir by convert().
	// This function copies them into the output dir using their human-readable names.
	auto copy_entry = [&](const Dictionary &p_entry) {
		String md5ext = p_entry.get("md5ext", "");
		String name = p_entry.get("name", md5ext);
		String fmt = p_entry.get("dataFormat", "");
		if (md5ext.is_empty()) {
			return;
		}
		String src = p_unzip_dir.path_join(md5ext);
		String dst = p_output_dir.path_join(name + "." + fmt);
		if (src == dst) {
			return;
		}
		Ref<FileAccess> fa_src = FileAccess::open(src, FileAccess::READ);
		if (fa_src.is_null()) {
			return;
		}
		PackedByteArray data = fa_src->get_buffer(fa_src->get_length());
		Ref<FileAccess> fa_dst = FileAccess::open(dst, FileAccess::WRITE);
		if (fa_dst.is_valid()) {
			fa_dst->store_buffer(data);
		}
	};

	for (int i = 0; i < p_costumes.size(); i++) {
		copy_entry(p_costumes[i]);
	}
	for (int i = 0; i < p_sounds.size(); i++) {
		copy_entry(p_sounds[i]);
	}
	return OK;
}

// ---------------------------------------------------------------------------
// Script writer
// ---------------------------------------------------------------------------

Error ScratchConverter::_write_script(const String &p_path, const String &p_script_content) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		print_error("ScratchConverter: cannot write script: " + p_path);
		return ERR_CANT_CREATE;
	}
	f->store_string(p_script_content);
	return OK;
}

// ---------------------------------------------------------------------------
// .tscn generation
// ---------------------------------------------------------------------------

String ScratchConverter::_generate_tscn(const Array &p_targets, const String &p_base_dir) {
	// Count total load steps: scripts + 1 for the scene itself
	int load_steps = 1;
	for (int i = 0; i < p_targets.size(); i++) {
		Dictionary tgt = p_targets[i];
		if (tgt.get("has_script", false)) {
			load_steps++;
		}
	}
	String out;
	out += "[gd_scene load_steps=" + itos(load_steps) + " format=3 uid=\"uid://scratch_converted\"]\n\n";

	// External resource entries for scripts
	int res_idx = 1;
	Vector<int> script_res_ids;
	for (int i = 0; i < p_targets.size(); i++) {
		Dictionary tgt = p_targets[i];
		bool has_script = tgt.get("has_script", false);
		if (has_script) {
			String script_path = tgt.get("script_path", "");
			out += "[ext_resource type=\"Script\" path=\"" + script_path + "\" id=\"" + itos(res_idx) + "\"]\n";
			script_res_ids.push_back(res_idx);
			res_idx++;
		} else {
			script_res_ids.push_back(-1);
		}
	}
	out += "\n";

	// Root node
	out += "[node name=\"ScratchScene\" type=\"Node2D\"]\n\n";

	// Sprite / stage nodes
	for (int i = 0; i < p_targets.size(); i++) {
		Dictionary tgt = p_targets[i];
		String name = tgt.get("safe_name", String("Target") + itos(i));
		bool is_stage = tgt.get("is_stage", false);
		double x = tgt.get("x", 0.0);
		double y = tgt.get("y", 0.0);
		double size = tgt.get("size", 100.0);
		bool visible = tgt.get("visible", true);
		int script_id = script_res_ids[i];

		if (is_stage) {
			out += "[node name=\"" + name + "\" type=\"Node2D\" parent=\".\"]\n";
		} else {
			out += "[node name=\"" + name + "\" type=\"AnimatedSprite2D\" parent=\".\"]\n";
			out += "position = Vector2(" + rtos(x) + ", " + rtos(-y) + ")\n";
			out += "scale = Vector2(" + rtos(size / 100.0) + ", " + rtos(size / 100.0) + ")\n";
			out += "visible = " + String(visible ? "true" : "false") + "\n";
		}
		if (script_id >= 0) {
			out += "script = ExtResource(\"" + itos(script_id) + "\")\n";
		}
		out += "\n";
	}

	return out;
}

#endif // TOOLS_ENABLED
