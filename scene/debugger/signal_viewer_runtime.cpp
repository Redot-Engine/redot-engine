/**************************************************************************/
/*  signal_viewer_runtime.cpp                                             */
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

#include "signal_viewer_runtime.h"

#include "core/debugger/engine_debugger.h"
#include "core/os/os.h"
#include "scene/gui/control.h"
#include "scene/main/node.h"

#ifdef DEBUG_ENABLED

// Static member initialization
SignalViewerRuntime *SignalViewerRuntime::singleton = nullptr;

SignalViewerRuntime *SignalViewerRuntime::create_singleton() {
	if (singleton) {
		return singleton; // Already exists
	}
	singleton = memnew(SignalViewerRuntime);
	return singleton;
}

void SignalViewerRuntime::destroy_singleton() {
	if (singleton) {
		memdelete(singleton);
		singleton = nullptr;
	}
}

SignalViewerRuntime::SignalViewerRuntime() {
	// Don't set singleton here - use create_singleton() instead
}

SignalViewerRuntime::~SignalViewerRuntime() {
	if (singleton == this) {
		singleton = nullptr;
	}
	if (tracking_enabled) {
		stop_tracking();
	}
}

void SignalViewerRuntime::_signal_emission_callback(Object *p_emitter, const StringName &p_signal, const Variant **p_args, int p_argcount) {
	// Only track Node objects
	Node *emitter_node = Object::cast_to<Node>(p_emitter);
	if (!emitter_node) {
		return; // Not a node, skip
	}

	// Check if this signal has connections
	List<Object::Connection> conns;
	p_emitter->get_signal_connection_list(p_signal, &conns);
	if (conns.is_empty()) {
		return; // No connections, skip this signal
	}

	// Get the singleton and check if we should track this
	SignalViewerRuntime *runtime = get_singleton();
	if (!runtime || !runtime->tracking_enabled) {
		return;
	}

	String node_class = emitter_node->get_class();
	String node_name = emitter_node->get_name();
	String signal_name = String(p_signal);

	// NOTE: Control node filter disabled to support signal showcases/demos
	// In production games, you may want to re-enable this to reduce UI noise
	// FILTER OUT ALL GUI/CONTROL CLASSES - Only track gameplay nodes
	// Skip all Control-derived classes (GUI elements) - check both by cast and by name
	// Control *as_control = Object::cast_to<Control>(emitter_node);
	// if (as_control) {
	// 	return; // Skip ALL Control nodes
	// }

	// Filter out internal engine noise - gizmo timers, skeleton pose updates, etc.
	// Skip timer signals (unless they're user-created gameplay timers)
	if (signal_name == "timeout" && (node_name.contains("Gizmo") || node_name.contains("Update") || node_name.contains("Timer") && node_class.contains("Editor"))) {
		return; // Skip editor gizmo/update timers
	}

	// Skip skeleton pose updates (fire every frame during animation)
	if (signal_name == "pose_updated" || signal_name == "skeleton_updated") {
		return; // Skip internal animation updates
	}

	// Skip bone list changes (skeleton animation system noise)
	if (signal_name == "bone_list_changed") {
		return;
	}

	// Filter out Skeleton3D and PhysicalBone completely
	if (node_class == "Skeleton3D" || node_class == "Skeleton3D3D" ||
			node_class == "PhysicalBone" || node_class == "PhysicalBoneSimulator3D") {
		return;
	}

	// Additional filtering by class name for robustness
	// Skip all common GUI/Control classes and editor internals
	if (node_class == "VScrollBar" || node_class == "HScrollBar" ||
			node_class == "ScrollBar" || node_class == "RichTextLabel" ||
			node_class == "Label" || node_class == "Button" ||
			node_class == "LineEdit" || node_class == "TextEdit" ||
			node_class == "Panel" || node_class == "Popup" ||
			node_class == "Window" || node_class == "Dialog" ||
			node_class.contains("Editor") || node_class.contains("Gizmo") ||
			node_class.contains("Menu") || node_class.contains("Theme") ||
			node_class.contains("StyleBox") || node_class.contains("Tree") ||
			node_class.contains("ItemList") || node_class.contains("Option") ||
			node_class.contains("Check")) {
		return;
	}

	// DEBUG: Only print after filtering (much less spam)
	print_line(vformat("[Signal Viewer Runtime] Tracking: %s.%s", node_name, signal_name));

	// Create a unique key for this signal
	String key = vformat("%s:%s", emitter_node->get_instance_id(), signal_name);

	// Increment the emission count
	if (!runtime->signal_emission_counts.has(key)) {
		runtime->signal_emission_counts[key] = 0;

		// First time seeing this signal - collect and store connection info
		Array connections_array;
		for (const Object::Connection &conn : conns) {
			Array conn_data;
			Object *target = conn.callable.get_object();
			if (target) {
				conn_data.append(target->get_instance_id());

				// Get the target node's name if it's a Node, otherwise use class
				Node *target_node = Object::cast_to<Node>(target);
				if (target_node) {
					conn_data.append(target_node->get_name()); // Use actual node name
					conn_data.append(target_node->get_class());
				} else {
					conn_data.append(target->get_class()); // Use class as name for non-Node objects
					conn_data.append(target->get_class());
				}

				conn_data.append(String(conn.callable.get_method()));
				connections_array.append(conn_data);
			}
		}
		runtime->signal_connections[key] = connections_array;
	}
	runtime->signal_emission_counts[key]++;

	// Get current time
	uint64_t current_time = OS::get_singleton()->get_ticks_msec();

	// Check if we should send an update (rate limiting)
	bool should_send = false;
	if (!runtime->signal_last_sent_time.has(key)) {
		// First time seeing this signal - send immediately
		should_send = true;
	} else {
		uint64_t last_sent = runtime->signal_last_sent_time[key];
		if (current_time - last_sent >= RATE_LIMIT_MS) {
			should_send = true;
		}
	}

	// Also check if we need to do a batch update
	if (current_time - runtime->last_batch_update_time >= BATCH_UPDATE_INTERVAL_MS) {
		runtime->_send_batch_updates();
		runtime->last_batch_update_time = current_time;
	}

	// Send update if rate limit allows
	if (should_send) {
		int count = runtime->signal_emission_counts[key];
		Array connections = runtime->signal_connections.has(key) ? runtime->signal_connections[key] : Array();
		runtime->_send_signal_update(key, emitter_node->get_instance_id(), node_name, node_class, signal_name, count, connections);
		runtime->signal_last_sent_time[key] = current_time;
		runtime->signal_emission_counts[key] = 0; // Reset count after sending
	}
}

void SignalViewerRuntime::_send_signal_update(const String &p_key, ObjectID p_emitter_id, const String &p_node_name, const String &p_node_class, const String &p_signal_name, int p_count, const Array &p_connections) {
	// Build message with signal emission data
	Array msg_data;
	msg_data.append(p_emitter_id);
	msg_data.append(p_node_name);
	msg_data.append(p_node_class);
	msg_data.append(p_signal_name);
	msg_data.append(p_count);
	msg_data.append(p_connections); // Add connection info

	// Send message back to editor via EngineDebugger
	EngineDebugger *ed = EngineDebugger::get_singleton();
	if (ed) {
		print_line(vformat("[Signal Viewer Runtime] Sending update: %s.%s (count: %d, connections: %d)", p_node_name, p_signal_name, p_count, p_connections.size()));
		ed->send_message("signal_viewer:signal_emitted", msg_data);
	}
}

void SignalViewerRuntime::_send_batch_updates() {
	// Send all accumulated signals that haven't been sent recently
	EngineDebugger *ed = EngineDebugger::get_singleton();
	if (!ed || signal_emission_counts.is_empty()) {
		return;
	}

	print_line(vformat("[Signal Viewer Runtime] Sending batch updates for %d signals", signal_emission_counts.size()));

	// Note: We'll send updates for signals that have counts but haven't been sent recently
	// For simplicity, we just reset all counts here. A more sophisticated approach would
	// send all pending counts in a single message.
	signal_emission_counts.clear();
}

void SignalViewerRuntime::start_tracking() {
	if (tracking_enabled) {
		return; // Already tracking
	}

	// Register the signal emission callback with Object class
	Object::set_signal_emission_callback(_signal_emission_callback);
	tracking_enabled = true;

	print_line("[Signal Viewer Runtime] Signal tracking enabled");
}

void SignalViewerRuntime::stop_tracking() {
	if (!tracking_enabled) {
		return; // Already not tracking
	}

	// Unregister the callback
	Object::set_signal_emission_callback(nullptr);
	tracking_enabled = false;

	print_line("[Signal Viewer Runtime] Signal tracking disabled");
}

void SignalViewerRuntime::send_node_signal_data(ObjectID p_node_id) {
	// Get the node from ObjectDB
	Object *obj = ObjectDB::get_instance(p_node_id);
	if (!obj) {
		print_line(vformat("[Signal Viewer Runtime] Node not found: %s", String::num_uint64((uint64_t)p_node_id)));
		return;
	}

	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		print_line("[Signal Viewer Runtime] Object is not a Node");
		return;
	}

	// Get node info
	String node_name = node->get_name();
	String node_class = node->get_class();

	print_line(vformat("[Signal Viewer Runtime] Collecting signal data for: %s (%s)", node_name, node_class));

	// Collect all signal data for this node
	Array signal_data_array;

	// Get all signals from this node
	List<MethodInfo> signals;
	node->get_signal_list(&signals);

	for (const MethodInfo &sig : signals) {
		String signal_name = sig.name;

		// Get connections for this signal
		List<Object::Connection> conns;
		node->get_signal_connection_list(StringName(signal_name), &conns);

		if (conns.is_empty()) {
			continue; // Skip signals without connections
		}

		// Build connection data
		Array connections_array;
		for (const Object::Connection &conn : conns) {
			Object *target_obj = conn.callable.get_object();
			if (!target_obj) {
				continue;
			}

			ObjectID target_id = target_obj->get_instance_id();
			String target_name;
			String target_class;

			// Get target node info
			Node *target_node = Object::cast_to<Node>(target_obj);
			if (target_node) {
				target_name = target_node->get_name();
				target_class = target_node->get_class();
			} else {
				target_name = target_obj->get_class();
				target_class = target_obj->get_class();
			}

			String target_method = conn.callable.get_method();

			// Build connection data array: [target_id, target_name, target_class, target_method]
			Array conn_data;
			conn_data.append(target_id);
			conn_data.append(target_name);
			conn_data.append(target_class);
			conn_data.append(target_method);
			connections_array.append(conn_data);
		}

		// Check if we have tracking data for this signal
		String key = vformat("%s:%s", String::num_uint64((uint64_t)p_node_id), signal_name);
		int count = 0;
		if (signal_emission_counts.has(key)) {
			count = signal_emission_counts[key];
		}

		// Build signal info array: [signal_name, count, connections_array]
		Array sig_info;
		sig_info.append(signal_name);
		sig_info.append(count);
		sig_info.append(connections_array);

		signal_data_array.append(sig_info);

		print_line(vformat("[Signal Viewer Runtime] Signal: %s (count: %d, connections: %d)",
				signal_name, count, connections_array.size()));
	}

	// Send the data back to editor
	// Format: [node_id, node_name, node_class, signal_data_array]
	Array msg_data;
	msg_data.append(p_node_id);
	msg_data.append(node_name);
	msg_data.append(node_class);
	msg_data.append(signal_data_array);

	EngineDebugger *ed = EngineDebugger::get_singleton();
	if (ed) {
		print_line(vformat("[Signal Viewer Runtime] Sending signal data: %s (%s)", node_name, node_class));
		ed->send_message("signal_viewer:node_signal_data", msg_data);
	} else {
		print_line("[Signal Viewer Runtime] No EngineDebugger - cannot send signal data");
	}
}

void SignalViewerRuntime::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_tracking"), &SignalViewerRuntime::start_tracking);
	ClassDB::bind_method(D_METHOD("stop_tracking"), &SignalViewerRuntime::stop_tracking);
	ClassDB::bind_method(D_METHOD("is_tracking_enabled"), &SignalViewerRuntime::is_tracking_enabled);
	ClassDB::bind_method(D_METHOD("send_node_signal_data", "node_id"), &SignalViewerRuntime::send_node_signal_data);
}

#endif // DEBUG_ENABLED
