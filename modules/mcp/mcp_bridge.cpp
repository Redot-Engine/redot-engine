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
#include "core/io/json.h"
#include "core/os/os.h"
#include "scene/2d/node_2d.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/rendering_server.h"

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
		// Try to find an available port
		for (int i = 10000; i < 11000; i++) {
			if (server->listen(i) == OK) {
				port = i;
				return OK;
			}
		}
		return ERR_ALREADY_IN_USE;
	} else {
		Error err = server->listen(p_port);
		if (err == OK) {
			port = p_port;
		}
		return err;
	}
}

bool MCPBridge::is_client_connected() const {
	return connection.is_valid() && connection->get_status() == StreamPeerTCP::STATUS_CONNECTED;
}

Error MCPBridge::connect_to_server(const String &p_host, int p_port) {
	is_host = false;
	connection.instantiate();
	port = p_port;
	return connection->connect_to_host(p_host, p_port);
}

Dictionary MCPBridge::send_command(const String &p_action, const Dictionary &p_args) {
	if (!is_client_connected()) {
		Dictionary err;
		err["error"] = "Not connected";
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
		if (connection->get_available_bytes() > 0) {
			uint8_t c;
			connection->get_data(&c, 1);
			if (c == '\n') {
				break;
			}
			response_str += (char)c;
		} else {
			OS::get_singleton()->delay_usec(1000);
		}
	}

	Variant res_var;
	if (JSON::parse_string(response_str).get_type() == Variant::DICTIONARY) {
		return JSON::parse_string(response_str);
	}

	Dictionary err;
	err["error"] = "Response timeout or invalid format";
	return err;
}

void MCPBridge::update() {
	if (is_host) {
		if (server->is_connection_available()) {
			connection = server->take_connection();
			fprintf(stderr, "[MCP] Game process connected to bridge\n");
		}
	} else {
		// Client (Game) side: process commands
		if (is_client_connected() && connection->get_available_bytes() > 0) {
			String cmd_str;
			while (connection->get_available_bytes() > 0) {
				uint8_t c;
				connection->get_data(&c, 1);
				if (c == '\n') {
					break;
				}
				cmd_str += (char)c;
			}

			Variant cmd_var = JSON::parse_string(cmd_str);
			if (cmd_var.get_type() == Variant::DICTIONARY) {
				Dictionary resp = _process_command(cmd_var);
				String resp_json = JSON::stringify(resp);
				CharString utf8 = resp_json.utf8();
				connection->put_data((const uint8_t *)utf8.get_data(), utf8.length());
				connection->put_u8('\n');
			}
		}
	}
}

Dictionary MCPBridge::_process_command(const Dictionary &p_cmd) {
	String action = p_cmd.get("action", "");
	Dictionary args = p_cmd.get("args", Dictionary());
	Dictionary resp;

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
				pos = ctrl->get_global_position() + ctrl->get_size() / 2.0;
			} else if (Object::cast_to<Node2D>(node)) {
				pos = Object::cast_to<Node2D>(node)->get_global_position();
			} else {
				resp["error"] = "Node not found or not a 2D element";
				return resp;
			}
		} else {
			pos = Vector2(args.get("x", 0), args.get("y", 0));
		}

		Ref<InputEventMouseButton> ev;
		ev.instantiate();
		ev->set_position(pos);
		ev->set_global_position(pos);
		ev->set_button_index(MouseButton::LEFT);

		ev->set_pressed(true);
		Input::get_singleton()->parse_input_event(ev);

		ev->set_pressed(false);
		Input::get_singleton()->parse_input_event(ev);

		resp["status"] = "clicked";
	} else if (action == "type") {
		String text = args.get("text", "");
		for (int i = 0; i < text.length(); i++) {
			char32_t c = text[i];
			Ref<InputEventKey> ev;
			ev.instantiate();
			ev->set_unicode(c);
			ev->set_pressed(true);
			Input::get_singleton()->parse_input_event(ev);
			ev->set_pressed(false);
			Input::get_singleton()->parse_input_event(ev);
		}
		resp["status"] = "typed";
	} else if (action == "wait") {
		float secs = args.get("seconds", 1.0);
		OS::get_singleton()->delay_usec(secs * 1000000);
		resp["status"] = "waited";
	} else if (action == "inspect_live") {
		SceneTree *st = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (st) {
			String path = args.get("path", ".");
			Node *node = (path == "." || path.is_empty()) ? (Node *)st->get_root() : st->get_root()->get_node_or_null(path);
			if (node) {
				Dictionary info;
				info["name"] = node->get_name();
				info["type"] = node->get_class();

				Array children;
				for (int i = 0; i < node->get_child_count(); i++) {
					children.push_back(node->get_child(i)->get_name());
				}
				info["children"] = children;

				Dictionary props;
				List<PropertyInfo> plist;
				node->get_property_list(&plist);
				for (const PropertyInfo &p : plist) {
					if (p.usage & PROPERTY_USAGE_EDITOR) {
						props[p.name] = node->get(p.name);
					}
				}
				info["properties"] = props;
				resp["info"] = info;
			} else {
				resp["error"] = "Node not found";
			}
		}
	}

	return resp;
}
