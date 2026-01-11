/**************************************************************************/
/*  mcp_bridge.cpp                                                        */
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

#include "mcp_bridge.h"

#include "core/crypto/crypto_core.h"
#include "core/input/input.h"
#include "core/input/input_enums.h"
#include "core/input/input_map.h"
#include "core/io/json.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/canvas_item.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/rendering_server.h"
#include <functional>

MCPBridge *MCPBridge::singleton = nullptr;

MCPBridge::MCPBridge() {
	singleton = this;
	server.instantiate();
}

MCPBridge::~MCPBridge() {
	singleton = nullptr;
}

void MCPBridge::_bind_methods() {
	ClassDB::bind_method(D_METHOD("update"), &MCPBridge::update);
}

Error MCPBridge::start_server(int p_port) {
	is_host = true;
	if (p_port == 0) {
		for (int i = 10000; i < 11000; i++) {
			if (server->listen(i) == OK) {
				port = i;
				fprintf(stderr, "[MCP] Bridge server listening on port %d\n", port);
				return OK;
			}
		}
		return ERR_ALREADY_IN_USE;
	} else {
		Error err = server->listen(p_port);
		if (err == OK) {
			port = p_port;
			fprintf(stderr, "[MCP] Bridge server listening on port %d\n", port);
		}
		return err;
	}
}

bool MCPBridge::is_client_connected() const {
	if (connection.is_valid()) {
		connection->poll();
		return connection->get_status() == StreamPeerTCP::STATUS_CONNECTED;
	}
	return false;
}

Error MCPBridge::connect_to_server(const String &p_host, int p_port) {
	is_host = false;
	connection.instantiate();
	port = p_port;
	fprintf(stderr, "[MCP] Game process connecting to bridge at %s:%d\n", p_host.utf8().get_data(), port);
	return connection->connect_to_host(p_host, p_port);
}

Dictionary MCPBridge::send_command(const String &p_action, const Dictionary &p_args) {
	if (!is_client_connected()) {
		Dictionary err;
		err["error"] = "Bridge not connected";
		return err;
	}

	Dictionary cmd;
	cmd["action"] = p_action;
	cmd["args"] = p_args;

	String json = JSON::stringify(cmd);
	CharString utf8 = json.utf8();
	connection->put_data((const uint8_t *)utf8.get_data(), utf8.length());
	connection->put_u8('\n');

	// Wait for response (blocking with timeout)
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();
	String response_str;
	while (OS::get_singleton()->get_ticks_msec() - start_time < 5000) {
		connection->poll();
		if (connection->get_available_bytes() > 0) {
			uint8_t c;
			int read;
			connection->get_partial_data(&c, 1, read);
			if (read > 0) {
				if (c == '\n') {
					break;
				}
				response_str += (char)c;
			}
		} else {
			OS::get_singleton()->delay_usec(1000);
		}
	}

	if (response_str.is_empty()) {
		Dictionary err;
		err["error"] = "Bridge timeout";
		return err;
	}

	Variant res_var = JSON::parse_string(response_str);
	if (res_var.get_type() == Variant::DICTIONARY) {
		return res_var;
	}

	Dictionary err;
	err["error"] = "Bridge invalid response: " + response_str;
	return err;
}

void MCPBridge::update() {
	if (is_host) {
		if (server->is_connection_available()) {
			if (connection.is_valid()) {
				fprintf(stderr, "[MCP] Dropping existing connection for new client\n");
				connection->disconnect_from_host();
			}
			connection = server->take_connection();
			fprintf(stderr, "[MCP] Game process connected to bridge on host side\n");
		}
	} else {
		// Client (Game) side
		if (is_client_connected()) {
			while (connection->get_available_bytes() > 0) {
				uint8_t c;
				int read;
				connection->get_partial_data(&c, 1, read);
				if (read > 0) {
					if (c == '\n') {
						Variant cmd_var = JSON::parse_string(partial_data);
						partial_data = "";
						if (cmd_var.get_type() == Variant::DICTIONARY) {
							Dictionary resp = _process_command(cmd_var);
							String resp_json = JSON::stringify(resp);
							CharString utf8 = resp_json.utf8();
							connection->put_data((const uint8_t *)utf8.get_data(), utf8.length());
							connection->put_u8('\n');
						}
					} else {
						if (partial_data.length() > 1024 * 1024) { // 1MB limit to prevent OOM
							partial_data = "";
							fprintf(stderr, "[MCP] Bridge buffer overflow, clearing\n");
						}
						partial_data += (char)c;
					}
				}
			}
		}
	}
}

void MCPBridge::_trigger_action_event(const StringName &p_action) {
	if (!InputMap::get_singleton()->has_action(p_action)) {
		return;
	}

	const List<Ref<InputEvent>> *events_ptr = InputMap::get_singleton()->action_get_events(p_action);
	if (!events_ptr) {
		return;
	}
	const List<Ref<InputEvent>> &events = *events_ptr;

	for (const Ref<InputEvent> &ev : events) {
		// Try to find a Key or Mouse Button to simulate
		Ref<InputEventKey> key_ev = ev;
		Ref<InputEventMouseButton> mouse_ev = ev;

		if (key_ev.is_valid() || mouse_ev.is_valid()) {
			Ref<InputEvent> press_ev = ev->duplicate();
			Ref<InputEvent> release_ev = ev->duplicate();

			if (key_ev.is_valid()) {
				Ref<InputEventKey> k = press_ev;
				k->set_pressed(true);
				Ref<InputEventKey> k_up = release_ev;
				k_up->set_pressed(false);
			} else if (mouse_ev.is_valid()) {
				Ref<InputEventMouseButton> m = press_ev;
				m->set_pressed(true);
				Ref<InputEventMouseButton> m_up = release_ev;
				m_up->set_pressed(false);
			}

			Input::get_singleton()->parse_input_event(press_ev);
			OS::get_singleton()->delay_usec(50000); // 50ms hold
			Input::get_singleton()->parse_input_event(release_ev);
			return; // Triggered one valid event for the action
		}
	}
}

Dictionary MCPBridge::_process_command(const Dictionary &p_cmd) {
	String action = p_cmd.get("action", "");
	Dictionary args = p_cmd.get("args", Dictionary());
	Dictionary resp;

	fprintf(stderr, "[MCP] Bridge processing command: %s\n", action.utf8().get_data());

	if (action == "capture") {
		SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (st) {
			Window *vp = st->get_root();
			Ref<Image> img = vp->get_texture()->get_image();

			float scale = args.get("scale", 1.0);
			if (scale != 1.0) {
				img->resize(img->get_width() * scale, img->get_height() * scale);
			}

			PackedByteArray png_buffer = img->save_png_to_buffer();
			resp["image_base64"] = CryptoCore::b64_encode_str(png_buffer.ptr(), png_buffer.size());
			resp["format"] = "png";
			resp["width"] = img->get_width();
			resp["height"] = img->get_height();
		} else {
			resp["error"] = "No scene tree found";
		}
	} else if (action == "click") {
		Vector2 pos;
		if (args.has("node_path")) {
			SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
			Node *node = st->get_root()->get_node_or_null(args["node_path"]);
			Control *ctrl = Object::cast_to<Control>(node);
			if (ctrl) {
				pos = ctrl->get_screen_transform().get_origin() + ctrl->get_size() / 2.0;
			} else if (Object::cast_to<Node2D>(node)) {
				pos = Object::cast_to<Node2D>(node)->get_global_position();
			} else {
				resp["error"] = "Node not found or not a 2D element";
				return resp;
			}
		} else {
			pos = Vector2(args.get("x", 0), args.get("y", 0));
		}

		// 1. Move mouse to position
		Ref<InputEventMouseMotion> mm;
		mm.instantiate();
		mm->set_position(pos);
		mm->set_global_position(pos);
		Input::get_singleton()->parse_input_event(mm);

		// 2. Press down
		Ref<InputEventMouseButton> ev_down;
		ev_down.instantiate();
		ev_down->set_position(pos);
		ev_down->set_global_position(pos);
		ev_down->set_button_index(MouseButton::LEFT);
		ev_down->set_button_mask(MouseButtonMask::LEFT);
		ev_down->set_pressed(true);
		Input::get_singleton()->parse_input_event(ev_down);

		// 3. Wait 50ms to ensure engine processes the down state
		OS::get_singleton()->delay_usec(50000);

		// 4. Release
		Ref<InputEventMouseButton> ev_up;
		ev_up.instantiate();
		ev_up->set_position(pos);
		ev_up->set_global_position(pos);
		ev_up->set_button_index(MouseButton::LEFT);
		ev_up->set_button_mask(MouseButtonMask::NONE);
		ev_up->set_pressed(false);
		Input::get_singleton()->parse_input_event(ev_up);

		resp["status"] = "clicked";
		resp["pos"] = pos;
	} else if (action == "inspect_live") {
		SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (st) {
			String path = args.get("path", ".");
			Node *node = (path == "." || path.is_empty()) ? (Node *)st->get_root() : st->get_root()->get_node_or_null(path);
			if (node) {
				bool recursive = args.get("recursive", false);
				int max_depth = args.get("depth", 5);

				std::function<Dictionary(Node *, int)> build_tree = [&](Node *p_node, int p_depth) -> Dictionary {
					Dictionary info;
					info["name"] = p_node->get_name();
					info["type"] = p_node->get_class();

					CanvasItem *ci = Object::cast_to<CanvasItem>(p_node);
					info["visible"] = ci ? ci->is_visible_in_tree() : true;

					if (Control *c = Object::cast_to<Control>(p_node)) {
						Vector2 sp = c->get_screen_transform().get_origin() + c->get_size() / 2.0;
						info["screen_pos"] = sp;
						info["size"] = c->get_size();
					} else if (Node2D *n2 = Object::cast_to<Node2D>(p_node)) {
						info["pos"] = n2->get_global_position();
					}

					if (recursive && p_depth < max_depth) {
						Array children;
						for (int i = 0; i < p_node->get_child_count(); i++) {
							Node *child = p_node->get_child(i);
							// Skip internal nodes to keep context clean
							String cname = child->get_name();
							if (cname.begins_with("@@")) {
								continue;
							}
							children.push_back(build_tree(child, p_depth + 1));
						}
						info["children"] = children;
					}
					return info;
				};

				resp["tree"] = build_tree(node, 0);
			} else {
				resp["error"] = "Node not found";
			}
		}
	} else if (action == "type") {
		String text = args.get("text", "");

		// Smart Fallback: Check if the text matches an Input Action (e.g. "ui_cancel")
		// Only check if it's more than 1 char and doesn't look like a special key [KEY]
		if (text.length() > 1 && !text.begins_with("[") && InputMap::get_singleton()->has_action(text)) {
			_trigger_action_event(text);
			resp["status"] = "triggered_action_fallback";
			return resp;
		}

		for (int i = 0; i < text.length(); i++) {
			String token;
			if (text[i] == '[' && text.find("]", i) != -1) {
				int end = text.find("]", i);
				token = text.substr(i + 1, end - i - 1);
				i = end;
			} else {
				token = String::chr(text[i]);
			}

			uint32_t k_val = (uint32_t)find_keycode(token);
			if (k_val == 0 && token.length() == 1) {
				// Fallback for regular characters
				k_val = (uint32_t)token.to_upper()[0];
			}

			Key code = (Key)(k_val & (uint32_t)KeyModifierMask::CODE_MASK);
			uint32_t mods = k_val & (uint32_t)KeyModifierMask::MODIFIER_MASK;
			char32_t unicode = 0;
			if (token.length() == 1) {
				unicode = token[0];
			}

			// Send Press
			Ref<InputEventKey> ev_down;
			ev_down.instantiate();
			ev_down->set_keycode(code);
			ev_down->set_physical_keycode(code);
			ev_down->set_unicode(unicode);
			ev_down->set_pressed(true);
			ev_down->set_shift_pressed(mods & (uint32_t)KeyModifierMask::SHIFT);
			ev_down->set_ctrl_pressed(mods & (uint32_t)KeyModifierMask::CTRL);
			ev_down->set_alt_pressed(mods & (uint32_t)KeyModifierMask::ALT);
			ev_down->set_meta_pressed(mods & (uint32_t)KeyModifierMask::META);
			Input::get_singleton()->parse_input_event(ev_down);

			// Send Release
			Ref<InputEventKey> ev_up = ev_down->duplicate();
			ev_up->set_pressed(false);
			Input::get_singleton()->parse_input_event(ev_up);
		}
		resp["status"] = "typed";
	} else if (action == "trigger_action") {
		String action_name = args.get("action_name", "");
		if (action_name.is_empty()) {
			resp["error"] = "Missing action_name";
		} else if (!InputMap::get_singleton()->has_action(action_name)) {
			resp["error"] = "Action not found: " + action_name;
		} else {
			_trigger_action_event(action_name);
			resp["status"] = "triggered_action";
		}
	} else if (action == "wait") {
		// Deprecated/No-op on client side, handled by server now
		resp["status"] = "wait_is_server_side";
	}

	return resp;
}
