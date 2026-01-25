#include "signalize_dock.h"

#include "editor/editor_node.h"
#include "editor/editor_interface.h"
#include "editor/settings/editor_settings.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/run/editor_run_bar.h"
#include "editor/script/script_editor_plugin.h"
#include "scene/debugger/scene_debugger.h"
#include "scene/gui/tree.h"
#include "core/debugger/engine_debugger.h"
#include "core/object/script_language.h"
#include "core/object/script_instance.h"
#include "core/variant/callable.h"
#include "core/input/input_event.h"
#include "core/object/class_db.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/option_button.h"
#include "scene/resources/style_box_flat.h"
#include "scene/gui/box_container.h"
#include "scene/main/timer.h"
#include "editor/gui/window_wrapper.h"
#include "core/templates/hash_set.h"

// Static member initialization
SignalizeDock *SignalizeDock::singleton_instance = nullptr;
const String SignalizeDock::MESSAGE_SIGNAL_EMITTED = "signal_viewer:signal_emitted";
const String SignalizeDock::MESSAGE_NODE_SIGNAL_DATA = "signal_viewer:node_signal_data";

SignalizeDock::SignalizeDock() {
	// Set singleton instance for global callback access
	singleton_instance = this;
	set_name("Signalize");
	set_h_size_flags(SIZE_EXPAND_FILL);
	set_v_size_flags(SIZE_EXPAND_FILL);

	// Create WindowWrapper (not added yet - only added when floating)
	window_wrapper = memnew(WindowWrapper);
	window_wrapper->set_margins_enabled(true);
	window_wrapper->set_window_title(TTR("Signalize - Signal Viewer"));

	// Create a content container that holds all the UI
	// This container can be reparented between SignalizeDock (docked) and WindowWrapper (floating)
	content_container = memnew(VBoxContainer);
	content_container->set_h_size_flags(SIZE_EXPAND_FILL);
	content_container->set_v_size_flags(SIZE_EXPAND_FILL);
	add_child(content_container);

	// Top bar with search, refresh, and floating button
	HBoxContainer *top_bar = memnew(HBoxContainer);
	content_container->add_child(top_bar);

	title_label = memnew(Label("Signalize"));
	title_label->set_h_size_flags(SIZE_EXPAND_FILL);
	top_bar->add_child(title_label);

	search_box = memnew(LineEdit);
	search_box->set_placeholder("Filter nodes...");
	search_box->set_h_size_flags(SIZE_EXPAND_FILL);
	search_box->connect("text_changed", callable_mp(this, &SignalizeDock::_on_search_changed));
	top_bar->add_child(search_box);

	refresh_button = memnew(Button);
	refresh_button->set_text("Build Graph");
	refresh_button->set_tooltip_text("Rebuild the signal graph from the edited scene");
	refresh_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_refresh_pressed));
	top_bar->add_child(refresh_button);

	// Per-node inspection: Add button to inspect selected remote node
	Button *inspect_button = memnew(Button);
	inspect_button->set_text("Inspect Selected Node");
	// Note: Button doesn't have set_tooltip in Godot 4.x, tooltip is set via theme or custom logic
	inspect_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_inspect_selected_button_pressed));
	top_bar->add_child(inspect_button);

	// Connection color picker
	connection_color_button = memnew(ColorPickerButton);

	// Load saved color from editor settings
	Ref<EditorSettings> editor_settings = EditorSettings::get_singleton();
	if (editor_settings.is_valid()) {
		Variant saved_color = editor_settings->get("signalize/connection_color");
		if (saved_color.get_type() == Variant::COLOR) {
			custom_connection_color = saved_color;
		}

		// Load verbosity level from editor settings
		Variant saved_verbosity = editor_settings->get("signalize/verbosity_level");
		if (saved_verbosity.get_type() == Variant::INT) {
			verbosity_level = saved_verbosity;
		}
	}

	connection_color_button->set_pick_color(custom_connection_color);
	connection_color_button->set_tooltip_text("Connection line color");
	connection_color_button->connect("color_changed", callable_mp(this, &SignalizeDock::_on_connection_color_changed));
	top_bar->add_child(connection_color_button);

	// Settings button
	settings_button = memnew(Button);
	settings_button->set_theme_type_variation(SceneStringName(FlatButton));
	settings_button->set_tooltip_text("Signalize Settings");
	settings_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_settings_pressed));
	top_bar->add_child(settings_button);

	// Make Floating button (using icon like ScriptEditor)
	make_floating_button = memnew(Button);
	make_floating_button->set_theme_type_variation(SceneStringName(FlatButton));
	// Icon will be set in NOTIFICATION_THEME_CHANGED
	make_floating_button->set_tooltip_text("Make Signalize floating (Alt+F)");
	make_floating_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_make_floating_pressed));
	top_bar->add_child(make_floating_button);

	// Graph view
	graph_edit = memnew(GraphEdit);
	graph_edit->set_h_size_flags(SIZE_EXPAND_FILL);
	graph_edit->set_v_size_flags(SIZE_EXPAND_FILL);
	graph_edit->set_zoom(0.8);
	graph_edit->set_show_zoom_label(true);
	content_container->add_child(graph_edit);

	// Connect to play/stop signals to rebuild graph with runtime nodes
	EditorRunBar *run_bar = EditorRunBar::get_singleton();
	if (run_bar) {
		run_bar->connect("play_pressed", callable_mp(this, &SignalizeDock::_on_play_pressed));
		run_bar->connect("stop_pressed", callable_mp(this, &SignalizeDock::_on_stop_pressed));
	} else {
		ERR_PRINT("[Signalize] Could not connect to EditorRunBar signals");
	}

	// Try to connect to debugger for message handling
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (debugger_node) {
		// Debugger node available
	}

	// Create timer to check when game is running and send start_tracking message
	game_start_check_timer = memnew(Timer);
	game_start_check_timer->set_wait_time(0.5); // Check every 0.5 seconds
	game_start_check_timer->set_one_shot(false);
	game_start_check_timer->connect("timeout", callable_mp(this, &SignalizeDock::_on_game_start_check_timer_timeout));
	game_start_check_timer->set_autostart(false);
	content_container->add_child(game_start_check_timer);
	game_start_check_timer->set_process_internal(true); // Make sure timer processes

	// NOTE: Global signal tracking DISABLED by default
	// We'll only enable it when a node is being inspected during gameplay
	tracking_enabled = false;
	was_playing_last_frame = false;
	remote_scene_root_id = ObjectID(); // Initialize to invalid ID

	// Register inspector plugin to detect when nodes are clicked in remote tree
	inspector_plugin = memnew(SignalizeInspectorPlugin);
	inspector_plugin->set_signal_viewer_dock(this);
	EditorInspector::add_inspector_plugin(inspector_plugin);

	// Register message capture to receive signal emissions from game process
	if (EngineDebugger::get_singleton()) {
		EngineDebugger::register_message_capture("signal_viewer", EngineDebugger::Capture(nullptr, _capture_signal_viewer_messages));
		// Also register for "scene" messages to detect node selection in remote tree
		EngineDebugger::register_message_capture("scene", EngineDebugger::Capture(nullptr, _capture_signal_viewer_messages));
	}

	// Build initial graph from edited scene (only works when game is not running)
	_build_graph();
}

void SignalizeDock::_on_test_signal() {
	// Test handler - can be removed in production
}

void SignalizeDock::_on_refresh_pressed() {
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	ScriptEditorDebugger *debugger = nullptr;

	if (debugger_node) {
		debugger = debugger_node->get_current_debugger();
	}

	// If game is running, don't allow full graph refresh
	if (debugger && debugger->is_session_active()) {
		ERR_PRINT("[Signalize] Cannot refresh full graph while game is running. Use 'Inspect Selected Node' instead.");
		return;
	}

	// Clear the existing graph and rebuild from the edited scene
	_clear_inspection();
	_build_graph();
}

void SignalizeDock::_on_make_floating_pressed() {
	if (!window_wrapper || !content_container) {
		return;
	}

	if (!is_floating) {
		// Create a shortcut for toggling floating
		Ref<Shortcut> make_floating_shortcut;
		make_floating_shortcut.instantiate();
		make_floating_shortcut->set_name("signalize/make_floating");

		Ref<InputEventKey> key_event;
		key_event.instantiate();
		key_event->set_keycode(Key::F);
		key_event->set_alt_pressed(true);

		Array events;
		events.push_back(key_event);
		make_floating_shortcut->set_events(events);

		// Reparent content_container from SignalizeDock to WindowWrapper
		remove_child(content_container);
		window_wrapper->set_wrapped_control(content_container, make_floating_shortcut);

		// Add WindowWrapper to the scene tree as a child of SignalizeDock's parent
		if (get_parent()) {
			get_parent()->add_child(window_wrapper);
		}

		// Enable floating mode
		window_wrapper->set_window_enabled(true);
		is_floating = true;
	} else {
		// Disable floating mode first
		window_wrapper->set_window_enabled(false);

		// Reparent content_container back to SignalizeDock
		window_wrapper->release_wrapped_control();
		add_child(content_container);

		// Remove WindowWrapper from scene tree
		if (window_wrapper->get_parent()) {
			window_wrapper->get_parent()->remove_child(window_wrapper);
		}

		is_floating = false;
	}
}

void SignalizeDock::_on_search_changed(const String &p_text) {
	// Show/hide nodes based on search
	String search_lower = p_text.to_lower();

	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		GraphNode *gn = kv.value;
		if (!gn) {
			continue;
		}

		String node_name = gn->get_title();
		bool visible = search_lower.is_empty() || node_name.to_lower().contains(search_lower);
		gn->set_visible(visible);
	}
}

void SignalizeDock::_on_connection_color_changed(const Color &p_color) {
	// Update the custom connection color
	custom_connection_color = p_color;

	// Save to editor settings
	Ref<EditorSettings> editor_settings = EditorSettings::get_singleton();
	if (editor_settings.is_valid()) {
		editor_settings->set("signalize/connection_color", p_color);
		editor_settings->save();
	}

	
	// Note: We don't rebuild the graph here because:
	// 1. Rebuilding triggers the color_changed signal again, creating an infinite loop
	// 2. The new color will be applied automatically when the graph is next rebuilt for any reason
	// 3. Connection highlights during runtime will use the new color immediately
	// The user can force a rebuild by clicking the "Build Graph" button if they want to see the change immediately
}

void SignalizeDock::_on_settings_pressed() {
	// Create settings dialog if it doesn't exist
	if (!settings_dialog) {
		settings_dialog = memnew(AcceptDialog);
		settings_dialog->set_title("Signalize Settings");
		settings_dialog->set_min_size(Size2(300, 100));
		add_child(settings_dialog);

		VBoxContainer *vbox = memnew(VBoxContainer);
		settings_dialog->add_child(vbox);

		// Verbosity setting
		HBoxContainer *verbosity_row = memnew(HBoxContainer);
		vbox->add_child(verbosity_row);

		Label *verbosity_label = memnew(Label("Verbosity Level:"));
		verbosity_label->set_h_size_flags(SIZE_EXPAND_FILL);
		verbosity_row->add_child(verbosity_label);

		OptionButton *verbosity_option = memnew(OptionButton);
		verbosity_option->add_item("Silent (errors only)", 0);
		verbosity_option->add_item("Quiet (graph stats)", 1);
		verbosity_option->add_item("Normal (inspector updates)", 2);
		verbosity_option->add_item("Verbose (full output)", 3);
		verbosity_option->select(verbosity_level);
		verbosity_option->connect("item_selected", callable_mp(this, &SignalizeDock::_on_verbosity_changed));
		verbosity_row->add_child(verbosity_option);

		// Pulse duration setting
		HBoxContainer *duration_row = memnew(HBoxContainer);
		vbox->add_child(duration_row);

		Label *duration_label = memnew(Label("Connection Pulse Duration (seconds):"));
		duration_label->set_h_size_flags(SIZE_EXPAND_FILL);
		duration_row->add_child(duration_label);

		SpinBox *duration_spin = memnew(SpinBox);
		duration_spin->set_min(0.1);
		duration_spin->set_max(10.0);
		duration_spin->set_step(0.1);
		duration_spin->set_value(connection_pulse_duration);
		duration_spin->connect("value_changed", callable_mp(this, &SignalizeDock::_on_pulse_duration_changed));
		duration_row->add_child(duration_spin);
	}

	settings_dialog->popup_centered();
}

void SignalizeDock::_on_pulse_duration_changed(double p_value) {
	connection_pulse_duration = p_value;
}

void SignalizeDock::_on_verbosity_changed(int p_level) {
	verbosity_level = p_level;
	Ref<EditorSettings> editor_settings = EditorSettings::get_singleton();
	if (editor_settings.is_valid()) {
		editor_settings->set("signalize/verbosity_level", verbosity_level);
		editor_settings->save();
	}
}

void SignalizeDock::_on_open_function_button_pressed(ObjectID p_node_id, const String &p_method_name) {
	// Get the node
	Object *obj = ObjectDB::get_instance(p_node_id);
	if (!obj) {
		ERR_PRINT(vformat("[Signalize] Cannot open function: node not found (ID: %s)", String::num_uint64((uint64_t)p_node_id)));
		return;
	}

	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
		ERR_PRINT(vformat("[Signalize] Cannot open function: object is not a Node"));
		return;
	}

	// Get the script attached to this node
	Ref<Script> script = node->get_script();
	if (!script.is_valid()) {
		ERR_PRINT(vformat("[Signalize] Cannot open function: node '%s' has no script", node->get_name()));
		return;
	}

	// Get the script editor
	ScriptEditor *script_editor = ScriptEditor::get_singleton();
	if (!script_editor) {
		ERR_PRINT("[Signalize] Cannot open function: ScriptEditor not available");
		return;
	}

	// First, try to find the method in the direct script
	bool success = script_editor->script_goto_method(script, p_method_name);

	// If not found and the script has a base class, search the base class script
	if (!success) {
		Ref<Script> base_script = script->get_base_script();
		while (base_script.is_valid()) {
			success = script_editor->script_goto_method(base_script, p_method_name);
			if (success) {
				break;
			}

			// Try next base class in the inheritance chain
			base_script = base_script->get_base_script();
		}
	}

	if (!success) {
			}
}

void SignalizeDock::_build_graph() {
	// Save current node positions before clearing
	saved_node_positions.clear();
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		GraphNode *gn = kv.value;
		if (gn) {
			saved_node_positions[kv.key] = gn->get_position_offset();
		}
	}

// Cleanup old runtime tracking connections before rebuilding
	_cleanup_runtime_signal_tracking();

	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		ERR_PRINT("[Signalize] No EditorNode - cannot build graph");
		return;
	}

	Node *scene_root = editor_node->get_edited_scene();
	if (!scene_root) {
		ERR_PRINT("[Signalize] No edited scene - cannot build graph");
		return;
	}

	
	// First pass: collect all nodes that participate in signal connections
	// ONLY include nodes that BOTH emit signals AND have their targets also included
	// This ensures all nodes in the graph will have visible connections
	List<ObjectID> connected_nodes;
	HashMap<ObjectID, int> emitter_connection_counts; // Track how many signals each node emits
	List<Node *> all_nodes;
	_collect_all_nodes(scene_root, all_nodes);

	// Build a set of all nodes in the scene for quick lookup
	HashMap<ObjectID, Node*> node_lookup;
	for (Node *node : all_nodes) {
		node_lookup[node->get_instance_id()] = node;
	}

	// Find all nodes that emit signals to OTHER nodes in the scene
	for (Node *node : all_nodes) {
		List<MethodInfo> signals;
		node->get_signal_list(&signals);

		bool has_connection_to_scene_node = false;
		for (const MethodInfo &sig : signals) {
			List<Object::Connection> conns;
			node->get_signal_connection_list(StringName(sig.name), &conns);

			for (const Object::Connection &conn : conns) {
				Object *target = conn.callable.get_object();
				if (target) {
					Node *target_node = Object::cast_to<Node>(target);
					// Only count if target is a different node (not self-connection)
					if (target_node && target_node != node) {
						// Check if the target is also in our scene
						if (node_lookup.has(target_node->get_instance_id())) {
							has_connection_to_scene_node = true;
							ObjectID emitter_id = node->get_instance_id();

							if (!emitter_connection_counts.has(emitter_id)) {
								emitter_connection_counts[emitter_id] = 0;
							}
							emitter_connection_counts[emitter_id]++;
							break;
						}
					}
				}
			}
			if (has_connection_to_scene_node) {
				break;
			}
		}
	}

	// Now collect all emitters AND their receivers
	HashMap<ObjectID, bool> final_nodes;
	for (Node *node : all_nodes) {
		List<MethodInfo> signals;
		node->get_signal_list(&signals);

		for (const MethodInfo &sig : signals) {
			List<Object::Connection> conns;
			node->get_signal_connection_list(StringName(sig.name), &conns);

			for (const Object::Connection &conn : conns) {
				Object *target = conn.callable.get_object();
				if (target) {
					Node *target_node = Object::cast_to<Node>(target);
					if (target_node && target_node != node && node_lookup.has(target_node->get_instance_id())) {
						// Filter out internal engine connections that aren't user-created
						String method_name = conn.callable.get_method();

						// Check if target has a script with this method
						bool has_script_method = false;
						ScriptInstance *script = target_node->get_script_instance();
						if (script) {
							// Check if this method exists in the script
							has_script_method = script->has_method(StringName(method_name));
						}

						// Only include if it's a real user connection (to a script method)
						if (!has_script_method) {
							// This is an internal engine connection, skip it
							continue;
						}

						// This is a valid user-created connection within the scene
						ObjectID emitter_id = node->get_instance_id();
						ObjectID receiver_id = target_node->get_instance_id();

						// Add both emitter and receiver to final list
						final_nodes[emitter_id] = true;
						final_nodes[receiver_id] = true;
					}
				}
			}
		}
	}

	// Convert set to list
	for (const KeyValue<ObjectID, bool> &KV : final_nodes) {
		connected_nodes.push_back(KV.key);
	}

	// Third pass: create graph nodes only for connected nodes
	int index = 0;
	for (Node *node : all_nodes) {
		if (connected_nodes.find(node->get_instance_id()) != nullptr) {
			_create_graph_node(node, 0, index++);
		}
	}


	// Fourth pass: create the signal connections
	_connect_all_node_signals();

	// Log graph build result (level 1 - Quiet)
	if (should_log(1)) {
		print_line(vformat("[Signalize] Graph built: %d nodes, %d connections",
			node_graph_nodes.size(), connections.size()));
	}

	// Auto-arrange the graph nodes for better visualization
	graph_edit->arrange_nodes();

	// Restore saved positions for nodes that still exist (overriding auto-layout)
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		if (saved_node_positions.has(kv.key)) {
			kv.value->set_position_offset(saved_node_positions[kv.key]);
		}
	}
	}
void SignalizeDock::_collect_all_nodes(Node *p_node, List<Node *> &r_list) {
	if (!p_node) {
		return;
	}

	r_list.push_back(p_node);

	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = Object::cast_to<Node>(p_node->get_child(i));
		if (child) {
			_collect_all_nodes(child, r_list);
		}
	}
}

void SignalizeDock::_build_graph_for_single_node(Node *p_node) {
	if (!p_node) {
		ERR_PRINT("[Signalize] Cannot build graph - node is null");
		return;
	}

	
	// Clear previous graph
	_clear_inspection();

	ObjectID node_id = p_node->get_instance_id();
	String node_name = p_node->get_name();
	String node_class = p_node->get_class();

	// Collect receiver methods (what this node receives)
	HashMap<ObjectID, List<ReceiverMethodInfo>> receiver_methods_list;

	// Collect all signals this node emits
	List<String> emitted_signals;
	List<MethodInfo> signals;
	p_node->get_signal_list(&signals);

	for (const MethodInfo &sig : signals) {
		List<Object::Connection> conns;
		p_node->get_signal_connection_list(StringName(sig.name), &conns);

		// Collect receiver info for connections to script methods only
		for (const Object::Connection &conn : conns) {
			Object *target_obj = conn.callable.get_object();
			if (!target_obj) {
				continue;
			}

			Node *target_node = Object::cast_to<Node>(target_obj);
			if (!target_node) {
				continue;
			}

			String target_method = conn.callable.get_method();
			if (target_method.is_empty()) {
				continue;
			}

			// Filter: Only include connections to script methods
			ScriptInstance *script = target_node->get_script_instance();
			if (!script || !script->has_method(StringName(target_method))) {
				continue; // Skip internal engine connections
			}

			if (emitted_signals.find(sig.name) == nullptr) {
				emitted_signals.push_back(sig.name);
			}

			ObjectID target_id = target_node->get_instance_id();

			if (!receiver_methods_list.has(target_id)) {
				receiver_methods_list[target_id] = List<ReceiverMethodInfo>();
			}

			// Check if already added
			bool already_added = false;
			for (const ReceiverMethodInfo &info : receiver_methods_list[target_id]) {
				if (info.method_name == target_method) {
					already_added = true;
					break;
				}
			}

			if (!already_added) {
				ReceiverMethodInfo info;
				info.target_id = target_id;
				info.method_name = target_method;
				receiver_methods_list[target_id].push_back(info);
			}
		}
	}

	// Also check what this node receives from other nodes
	// We need to scan all nodes in the scene to find signals that connect to this node
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		return;
	}

	Node *scene_root = editor->get_edited_scene();
	if (!scene_root) {
		ERR_PRINT("[Signalize] No edited scene");
		return;
	}

	List<Node *> all_nodes;
	_collect_all_nodes(scene_root, all_nodes);

	// Find all signals that connect TO this node (script methods only)
	for (Node *emitter_node : all_nodes) {
		if (emitter_node == p_node) {
			continue; // Skip self
		}

		List<MethodInfo> emitter_signals;
		emitter_node->get_signal_list(&emitter_signals);

		for (const MethodInfo &sig : emitter_signals) {
			List<Object::Connection> conns;
			emitter_node->get_signal_connection_list(StringName(sig.name), &conns);

			for (const Object::Connection &conn : conns) {
				Object *target_obj = conn.callable.get_object();
				if (!target_obj) {
					continue;
				}

				// Check if this connection targets our node
				if (target_obj == p_node) {
					// Check if this is a connection to a script method
					String target_method = conn.callable.get_method();
					if (target_method.is_empty()) {
						continue;
					}

					// Filter: Only include if p_node has this method in a script
					ScriptInstance *script = p_node->get_script_instance();
					if (!script || !script->has_method(StringName(target_method))) {
						continue; // Skip internal engine connections
					}

					// This emitter sends a signal to our node's script method - add it to the graph
					ObjectID emitter_id = emitter_node->get_instance_id();
					if (!node_graph_nodes.has(emitter_id)) {
						_create_graph_node(emitter_node, 0, node_graph_nodes.size());
					}
					break; // Found a valid connection from this emitter, stop checking other signals
				}
			}
		}
	}

	// Create graph node for the main node
	if (!node_graph_nodes.has(node_id)) {
		_create_graph_node(p_node, 0, node_graph_nodes.size());
	}

	// Create graph nodes for all receivers
	for (const KeyValue<ObjectID, List<ReceiverMethodInfo>> &KV : receiver_methods_list) {
		ObjectID target_id = KV.key;
		if (target_id == node_id) {
			continue; // Skip self
		}

		// Find the target node
		Node *target_node = nullptr;
		for (Node *node : all_nodes) {
			if (node->get_instance_id() == target_id) {
				target_node = node;
				break;
			}
		}

		if (target_node && !node_graph_nodes.has(target_id)) {
			_create_graph_node(target_node, 0, node_graph_nodes.size());
		}
	}

	// Now add all GraphNodes to GraphEdit
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		GraphNode *gn = kv.value;
		if (gn->get_parent() == nullptr) {
			graph_edit->add_child(gn);
		}
	}

	// Now create all the connections and labels
	_connect_all_node_signals();

	// Log single-node graph build result (level 2 - Normal, inspector updates)
	if (should_log(2)) {
		print_line(vformat("[Signalize] Built graph for node %s: %d nodes, %d connections",
			node_name, node_graph_nodes.size(), pending_connections.size()));
	}
}

bool SignalizeDock::_node_has_connections(Node *p_node) {
	if (!p_node) {
		return false;
	}

	// Check if this node emits signals to other nodes
	List<MethodInfo> signals;
	p_node->get_signal_list(&signals);

	for (const MethodInfo &sig : signals) {
		List<Object::Connection> conns;
		p_node->get_signal_connection_list(StringName(sig.name), &conns);

		// Check if this is a user connection (both editor and runtime)
		if (conns.size() > 0) {
			return true;
		}
	}

	// Also check if this node receives signals from other nodes
	// We need to scan all nodes to find connections to this node
	return false; // Will be handled by the emitter check
}

void SignalizeDock::_create_graph_node(Node *p_node, int p_depth, int p_index) {
	if (!p_node) {
		return;
	}

	ObjectID node_id = p_node->get_instance_id();

	// Skip if already created
	if (node_graph_nodes.has(node_id)) {
		return;
	}

	// Create graph node for this node
	GraphNode *gn = memnew(GraphNode);
	String title = vformat("%s(%s)", p_node->get_name(), p_node->get_class());
	gn->set_title(title);
	gn->set_position_offset(Vector2(p_depth * 350, p_index * 150));

	// Get the actual icon color from the editor (matches Scene tree colors)
	Color node_color = _get_editor_node_icon_color(p_node);

	// Set the node's titlebar color using theme override (like Visual Shader does)
	Ref<StyleBoxFlat> sb_colored = gn->get_theme_stylebox("titlebar", "GraphNode")->duplicate();
	sb_colored->set_bg_color(node_color);
	gn->add_theme_style_override("titlebar", sb_colored);

	// Also set the selected state color
	Ref<StyleBoxFlat> sb_colored_selected = gn->get_theme_stylebox("titlebar_selected", "GraphNode")->duplicate();
	sb_colored_selected->set_bg_color(node_color.lightened(0.2));
	gn->add_theme_style_override("titlebar_selected", sb_colored_selected);

	// Set the title text to black
	HBoxContainer *titlebar = gn->get_titlebar_hbox();
	if (titlebar) {
		for (int i = 0; i < titlebar->get_child_count(); i++) {
			Label *title_label = Object::cast_to<Label>(titlebar->get_child(i));
			if (title_label) {
				title_label->add_theme_color_override("font_color", Color(0, 0, 0, 1)); // Black text
				break;
			}
		}
	}

	// Store the color for use with labels
	node_colors[node_id] = node_color;

	// Set a unique name for this graph node
	String graph_name = vformat("GraphNode_%s", String::num_uint64((uint64_t)node_id));
	gn->set_name(graph_name);

	// DON'T add to graph_edit yet! We'll add it after slots are configured
	node_graph_nodes[node_id] = gn;
	node_graph_names[node_id] = graph_name;
}

Color SignalizeDock::_get_editor_node_icon_color(Node *p_node) {
	// Get the actual icon from the editor (same as used in Scene tree)
	EditorNode *editor = EditorNode::get_singleton();
	if (!editor) {
		// Fallback to default color if editor not available
		return Color(0.92, 0.92, 0.92, 1.0);
	}

	Ref<Texture2D> icon = editor->get_object_icon(p_node, "Node");
	if (!icon.is_valid() || icon == nullptr) {
		// No icon available, use default color
		return Color(0.92, 0.92, 0.92, 1.0);
	}

	// Sample the center pixel of the icon to get its representative color
	// This gives us the actual color the editor uses for this node type
	Vector2 icon_size = icon->get_size();
	if (icon_size.x <= 0 || icon_size.y <= 0) {
		return Color(0.92, 0.92, 0.92, 1.0);
	}

	// Get image data from the texture
	Ref<Image> icon_image = icon->get_image();
	if (!icon_image.is_valid()) {
		// Last resort: use default color
		return Color(0.92, 0.92, 0.92, 1.0);
	}

	// Sample a pixel from the center of the icon
	int center_x = icon_image->get_width() / 2;
	int center_y = icon_image->get_height() / 2;
	Color icon_color = icon_image->get_pixel(center_x, center_y);

	// If the sampled pixel is transparent or near-black, try another position
	if (icon_color.a < 0.1) {
		// Try top-left quadrant
		icon_color = icon_image->get_pixel(icon_image->get_width() / 4, icon_image->get_height() / 4);
		if (icon_color.a < 0.1) {
			// Still transparent, use default
			icon_color = Color(0.92, 0.92, 0.92, 1.0);
		}
	}

	return icon_color;
}

Color SignalizeDock::_get_editor_node_icon_color_by_class(const String &p_class_name) {
	// Simple color mapping based on inheritance hierarchy
	// Colors match the editor Scene tree icons

	// Special cases: AnimationPlayer and AnimationTree
	if (p_class_name == "AnimationPlayer" || p_class_name == "AnimationTree") {
		return Color(0.76, 0.56, 0.95, 1.0); // #c38ef1
	}

	// Check inheritance hierarchy using ClassDB
	// Note: Check most specific types first (Control before Node, Node3D before Node)

	// Control (green)
	if (ClassDB::is_parent_class(p_class_name, "Control")) {
		return Color(0.56, 0.94, 0.59, 1.0); // #8eef96
	}

	// Node3D (red)
	if (ClassDB::is_parent_class(p_class_name, "Node3D")) {
		return Color(0.99, 0.5, 0.49, 1.0); // #fc7f7e
	}

	// Node2D (blue)
	if (ClassDB::is_parent_class(p_class_name, "Node2D")) {
		return Color(0.55, 0.65, 0.95, 1.0); // #8da5f3
	}

	// Node (white) - catch-all for anything inheriting from Node
	if (ClassDB::is_parent_class(p_class_name, "Node")) {
		return Color(1.0, 1.0, 1.0, 1.0); // #ffffff
	}

	// Fallback for unknown classes
	return Color(1.0, 1.0, 1.0, 1.0); // #ffffff
}

Color SignalizeDock::_get_node_type_color(const String &p_class_name) {
	// Color mapping based on common node types
	// Pastel colors - softer, lighter, less saturated for better readability
	// 2D nodes - Pastel blues
	if (p_class_name == "Node2D" || p_class_name.contains("2D")) {
		return Color(0.75, 0.85, 0.95, 1.0); // Pastel sky blue
	}
	if (p_class_name == "Sprite2D" || p_class_name == "AnimatedSprite2D") {
		return Color(0.7, 0.8, 0.95, 1.0); // Pastel blue
	}
	if (p_class_name == "Area2D") {
		return Color(0.85, 0.75, 0.95, 1.0); // Pastel periwinkle
	}
	if (p_class_name == "Camera2D") {
		return Color(0.75, 0.9, 0.95, 1.0); // Pastel cyan
	}

	// 3D nodes - Pastel greens
	if (p_class_name == "Node3D" || p_class_name.contains("3D")) {
		return Color(0.75, 0.9, 0.8, 1.0); // Pastel mint green
	}
	if (p_class_name == "MeshInstance3D") {
		return Color(0.7, 0.85, 0.75, 1.0); // Pastel medium green
	}
	if (p_class_name == "Area3D") {
		return Color(0.85, 0.9, 0.7, 1.0); // Pastel lime green
	}
	if (p_class_name == "Camera3D") {
		return Color(0.8, 0.9, 0.85, 1.0); // Pastel light green
	}

	// UI nodes - Pastel oranges/yellows
	if (p_class_name == "Control" || p_class_name.contains("UI")) {
		return Color(1.0, 0.85, 0.7, 1.0); // Pastel peach
	}
	if (p_class_name == "Label" || p_class_name == "RichTextLabel") {
		return Color(1.0, 0.9, 0.75, 1.0); // Pastel cream
	}
	if (p_class_name == "Button" || p_class_name.contains("Button")) {
		return Color(1.0, 0.8, 0.65, 1.0); // Pastel apricot
	}
	if (p_class_name == "Panel" || p_class_name == "Container") {
		return Color(1.0, 0.9, 0.7, 1.0); // Pastel light peach
	}

	// Resource nodes - Pastel pinks
	if (p_class_name == "Resource" || p_class_name == "Timer") {
		return Color(1.0, 0.75, 0.85, 1.0); // Pastel rose
	}

	// Audio nodes - Pastel purples
	if (p_class_name.contains("Audio") || p_class_name.contains("Sound")) {
		return Color(0.85, 0.75, 0.95, 1.0); // Pastel lavender
	}

	// Collision/Physics nodes - Pastel reds
	if (p_class_name.contains("Collision") || p_class_name.contains("Shape")) {
		return Color(1.0, 0.75, 0.75, 1.0); // Pastel coral
	}

	// Light nodes - Pastel yellow
	if (p_class_name.contains("Light")) {
		return Color(1.0, 0.95, 0.8, 1.0); // Pastel light yellow
	}

	// Default - Pastel neutral
	return Color(0.92, 0.92, 0.92, 1.0);
}
void SignalizeDock::_connect_all_node_signals() {
	// First pass: collect all receiver methods for each node
	// Now tracks the actual target object that owns the method
	HashMap<ObjectID, List<ReceiverMethodInfo>> receiver_methods_list;

	// Get the scene
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		return;
	}

	Node *scene_root = editor_node->get_edited_scene();
	if (!scene_root) {
		return;
	}

	// Collect all nodes in the scene
	List<Node *> all_nodes;
	_collect_all_nodes(scene_root, all_nodes);

	// First, find all signals and add their receiver methods to the appropriate graph nodes
	for (Node *emitter_node : all_nodes) {
		if (!emitter_node) {
			continue;
		}

		// Skip if this emitter is not in our graph
		ObjectID emitter_id = emitter_node->get_instance_id();
		if (!node_graph_nodes.has(emitter_id)) {
			continue;
		}

		List<MethodInfo> signals;
		emitter_node->get_signal_list(&signals);

		for (const MethodInfo &sig : signals) {
			List<Object::Connection> conns;
			emitter_node->get_signal_connection_list(StringName(sig.name), &conns);

			for (const Object::Connection &conn : conns) {
				Object *target_obj = conn.callable.get_object();
				if (!target_obj) {
					continue;
				}

				ObjectID target_id = target_obj->get_instance_id();

				// Skip if the target is not in our graph
				if (!node_graph_nodes.has(target_id)) {
					continue;
				}

				String method_name = conn.callable.get_method();
				if (method_name.is_empty()) {
					continue;
				}

				// Add this method to the target's receiver list
				if (!receiver_methods_list.has(target_id)) {
					receiver_methods_list[target_id] = List<ReceiverMethodInfo>();
				}

				// Check if already added
				bool already_added = false;
				for (const ReceiverMethodInfo &info : receiver_methods_list[target_id]) {
					if (info.method_name == method_name) {
						already_added = true;
						break;
					}
				}

				if (!already_added) {
					ReceiverMethodInfo info;
					info.target_id = target_id;  // The object that owns the method
					info.method_name = method_name;
					receiver_methods_list[target_id].push_back(info);
				}
			}
		}
	}

	// Second pass: collect all emitter signals for each node
	HashMap<ObjectID, List<String>> emitter_signals_list;

	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		Object *obj = ObjectDB::get_instance(kv.key);
		if (!obj) {
			continue;
		}

		Node *node = Object::cast_to<Node>(obj);
		if (node) {
			ObjectID node_id = node->get_instance_id();

			List<MethodInfo> signals;
			node->get_signal_list(&signals);

			List<String> node_emitter_signals;
			for (const MethodInfo &sig : signals) {
				List<Object::Connection> conns;
				node->get_signal_connection_list(StringName(sig.name), &conns);

				// Only include this signal if it has at least one connection to a node in our graph
				bool has_connection_in_graph = false;
				for (const Object::Connection &conn : conns) {
					Object *target_obj = conn.callable.get_object();
					if (!target_obj) {
						continue;
					}
					ObjectID target_id = target_obj->get_instance_id();
					if (node_graph_nodes.has(target_id)) {
						has_connection_in_graph = true;
						break;
					}
				}

				if (has_connection_in_graph) {
					node_emitter_signals.push_back(sig.name);
				}
			}

			emitter_signals_list[node_id] = node_emitter_signals;
		}
	}

	// Third pass: add all labels BEFORE adding to GraphEdit
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		ObjectID node_id = kv.key;
		GraphNode *gn = kv.value;

		// Add receiver labels first
		if (receiver_methods_list.has(node_id)) {
			for (const ReceiverMethodInfo &info : receiver_methods_list[node_id]) {
				// Create a horizontal container for label + button
				HBoxContainer *hbox = memnew(HBoxContainer);
				gn->add_child(hbox);

				String function_text = vformat("- %s", info.method_name);
				Label *function_label = memnew(Label(function_text));
				function_label->set_h_size_flags(SIZE_EXPAND_FILL);
				function_label->set_modulate(Color(1, 1, 1, 1)); // Pure white to counteract node tint
				hbox->add_child(function_label);

				Button *open_button = memnew(Button);
				open_button->set_text("Open");
				// Pass the target object ID (which owns the script) and method name
				open_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_open_function_button_pressed).bind(info.target_id, info.method_name));
				hbox->add_child(open_button);
			}
		}

		// Then add emitter labels
		if (emitter_signals_list.has(node_id)) {
			for (const String &sig_name : emitter_signals_list[node_id]) {
				String signal_text = vformat("- %s", sig_name);
				Label *signal_label = memnew(Label(signal_text));
				signal_label->set_modulate(Color(1, 1, 1, 1)); // Pure white to counteract node tint
				gn->add_child(signal_label);
			}
		}
	}

	// Fourth pass: add all GraphNodes to GraphEdit BEFORE configuring slots
	// This is required because set_slot() needs the node to be in the graph first
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		GraphNode *gn = kv.value;
		if (gn->get_parent() == nullptr) {
			graph_edit->add_child(gn);
		}
	}

	// Fifth pass: NOW configure all slots after nodes are in the GraphEdit
	// NOTE: In Godot 4.x, child indices start at 0 (first child we added)
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		ObjectID node_id = kv.key;
		GraphNode *gn = kv.value;

		int current_child_idx = 0;

		// Configure input slots for receiver labels first
		if (receiver_methods_list.has(node_id)) {
			int slot_idx = 0;
			for (const ReceiverMethodInfo &info : receiver_methods_list[node_id]) {
				gn->set_slot(current_child_idx, true, 0, Color(1.0, 0.8, 0.6), false, 0, Color());

				if (!function_to_slot.has(node_id)) {
					function_to_slot[node_id] = HashMap<String, int>();
				}
				function_to_slot[node_id][info.method_name] = slot_idx;
				slot_idx++;
				current_child_idx++;
			}
		}

		// Configure output slots for emitter labels (after receiver labels)
		if (emitter_signals_list.has(node_id)) {
			int slot_idx = 0;
			for (const String &sig_name : emitter_signals_list[node_id]) {
				gn->set_slot(current_child_idx, false, 0, Color(), true, 0, custom_connection_color);

				if (!signal_to_slot.has(node_id)) {
					signal_to_slot[node_id] = HashMap<String, int>();
				}
				signal_to_slot[node_id][sig_name] = slot_idx;
				slot_idx++;
				current_child_idx++;
			}
		}
	}

	// Sixth pass: collect all connections for pending_connections list
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		Object *obj = ObjectDB::get_instance(kv.key);
		if (!obj) {
			continue;
		}

		Node *node = Object::cast_to<Node>(obj);
		if (node) {
			ObjectID node_id = node->get_instance_id();
			List<MethodInfo> signals;
			node->get_signal_list(&signals);

			for (const MethodInfo &sig : signals) {
				List<Object::Connection> conns;
				node->get_signal_connection_list(StringName(sig.name), &conns);

				for (const Object::Connection &conn : conns) {
					Object *target_obj = conn.callable.get_object();
					if (!target_obj) {
						continue;
					}

					String method_name = conn.callable.get_method();
					if (method_name.is_empty()) {
						continue;
					}

					ObjectID target_id = target_obj->get_instance_id();

					if (!signal_to_slot.has(node_id) || !signal_to_slot[node_id].has(sig.name)) {
						continue;
					}
					if (!function_to_slot.has(target_id) || !function_to_slot[target_id].has(method_name)) {
						continue;
					}

					int from_slot = signal_to_slot[node_id][sig.name];
					int to_slot = function_to_slot[target_id][method_name];

					ConnectionSlot conn_slot;
					conn_slot.emitter_id = node_id;
					conn_slot.signal_name = sig.name;
					conn_slot.receiver_id = target_id;
					conn_slot.method_name = method_name;
					conn_slot.from_slot = from_slot;
					conn_slot.to_slot = to_slot;
					pending_connections.push_back(conn_slot);
				}
			}
		}
	}

	// Finally create the visual connections
	call_deferred("_create_visual_connections");

	// NOTE: Runtime signal tracking disabled for performance
	// This would connect to all signals on all nodes to track emissions
	// _connect_runtime_signal_tracking(); // DISABLED: Only enable when live tracking is needed
}

void SignalizeDock::_cleanup_runtime_signal_tracking() {
	// Disconnect all runtime tracking connections
	if (runtime_signal_connections.is_empty()) {
		return; // Nothing to clean up
	}


	// Get the scene root
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		return;
	}

	Node *scene_root = editor_node->get_edited_scene();
	if (!scene_root) {
		// Scene is gone, just clear the tracking data
		runtime_signal_connections.clear();
		return;
	}


	// Disconnect all tracked signals
	for (const KeyValue<ObjectID, HashMap<String, int>> &node_entry : runtime_signal_connections) {
		Object *obj = ObjectDB::get_instance(node_entry.key);
		if (!obj) {
			continue; // Object no longer exists
		}

		Node *node = Object::cast_to<Node>(obj);
		if (!node) {
			continue;
		}

		for (const KeyValue<String, int> &sig_entry : node_entry.value) {
			const String &sig_name = sig_entry.key;

			// Create the callable we used to connect
			Callable callable = callable_mp(this, &SignalizeDock::_on_signal_fired).bind(node, sig_name);

			// Check if we're connected
			if (node->is_connected(sig_name, callable)) {
				node->disconnect(sig_name, callable);
			}
		}
	}

	// Clear the tracking data
	runtime_signal_connections.clear();

	}

void SignalizeDock::_connect_runtime_signal_tracking() {
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		return;
	}

	Node *scene_root = nullptr;
	bool is_runtime = false;

	// Get the edited scene as a reference
	Node *edited_scene = editor_node->get_edited_scene();
	String edited_scene_name = edited_scene ? String(edited_scene->get_name()) : String();
	String edited_scene_class = edited_scene ? edited_scene->get_class() : String();

	// First, try to get the runtime scene (running game)
	SceneTree *scene_tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (scene_tree) {
		Node *root = scene_tree->get_root();
		if (root) {
			// Helper function to recursively search for a matching scene
			std::function<Node*(Node*, int)> search_recursive = [&](Node *node, int depth) -> Node* {
				if (!node || depth > 10) { // Limit recursion depth
					return nullptr;
				}

				// Skip EditorNode
				EditorNode *editor_check = Object::cast_to<EditorNode>(node);
				if (editor_check) {
					return nullptr;
				}

				String node_name = String(node->get_name());
				String node_class = node->get_class();

				// Skip UI dialogs and popups
				if (node_class.contains("Dialog") || node_class.contains("Popup") ||
					node_class.contains("Window") || node_class.contains("Menu") ||
					node_class.contains("Panel") || node_class.contains("Button") ||
					node_name.begins_with("_editor_") || node_name.contains("__editor")) {
					return nullptr;
				}

				// Check if this matches the edited scene class and has signal connections
				if (node_class == edited_scene_class && _node_has_connections(node)) {
					return node;
				}

				// Recursively search children
				for (int i = 0; i < node->get_child_count(); i++) {
					Node *result = search_recursive(node->get_child(i), depth + 1);
					if (result) {
						return result;
					}
				}

				return nullptr;
			};

			// Search recursively through the root's children
			for (int i = 0; i < root->get_child_count(); i++) {
				Node *found = search_recursive(root->get_child(i), 0);
				if (found) {
					scene_root = found;
					is_runtime = true;
					break;
				}
			}
		}
	}

	// If no runtime scene found, use the edited scene
	if (!scene_root) {
		scene_root = edited_scene;
		is_runtime = false;
		if (!scene_root) {
			return;
		}
	}

	// Update the tracking flag
	tracking_runtime_scene = is_runtime;

	// Collect all nodes in the scene
	List<Node *> all_nodes;
	_collect_all_nodes(scene_root, all_nodes);


	// For each node, connect to its signals
	for (Node *node : all_nodes) {
		if (!node) {
			continue;
		}

		ObjectID node_id = node->get_instance_id();

		// Get all signals from this node
		List<MethodInfo> signals;
		node->get_signal_list(&signals);

		for (const MethodInfo &sig : signals) {
			// Check if this signal has any connections (editor or runtime)
			List<Object::Connection> conns;
			node->get_signal_connection_list(StringName(sig.name), &conns);

			if (conns.is_empty()) {
				continue; // Skip signals without connections
			}

			// Connect to this signal for tracking
			// Use bind to pass the node and signal name to our callback
			Callable callable = callable_mp(this, &SignalizeDock::_on_signal_fired).bind(node, sig.name);

			Error err = node->connect(sig.name, callable);
			if (err == OK) {
				// Store the connection info so we can track it
				if (!runtime_signal_connections.has(node_id)) {
					runtime_signal_connections[node_id] = HashMap<String, int>();
				}
				runtime_signal_connections[node_id][sig.name] = 1;

							} else {
				ERR_PRINT(vformat("[Signalize] Failed to connect to signal: %s.%s (error: %d)",
					node->get_name(), sig.name, (int)err));
			}
		}
	}

}

void SignalizeDock::_add_receiver_slots(Node *p_node) {
	if (!p_node) {
		return;
	}

	GraphNode **gn_ptr = node_graph_nodes.getptr(p_node->get_instance_id());
	if (!gn_ptr || !*gn_ptr) {
		return;
	}

	GraphNode *gn = *gn_ptr;
	ObjectID node_id = p_node->get_instance_id();

	// Find all signals where this node is a receiver
	// We need to scan all nodes in the scene to find connections to this node
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		return;
	}

	Node *scene_root = editor_node->get_edited_scene();
	if (!scene_root) {
		return;
	}

	List<Node *> all_nodes;
	_collect_all_nodes(scene_root, all_nodes);

	// Track which methods this node receives, to avoid duplicates
	// Use a list to preserve order
	List<String> receiver_methods;

	for (Node *emitter_node : all_nodes) {
		if (!emitter_node) {
			continue;
		}

		List<MethodInfo> signals;
		emitter_node->get_signal_list(&signals);

		for (const MethodInfo &sig : signals) {
			List<Object::Connection> conns;
			emitter_node->get_signal_connection_list(StringName(sig.name), &conns);

			for (const Object::Connection &conn : conns) {
				Object *target_obj = conn.callable.get_object();
				if (!target_obj) {
					continue;
				}

				// Check if this connection is to our node
				if (target_obj->get_instance_id() != node_id) {
					continue;
				}

				String method_name = conn.callable.get_method();
				if (method_name.is_empty()) {
					continue;
				}

				// Skip if we already added this method
				bool already_added = false;
				for (const String &m : receiver_methods) {
					if (m == method_name) {
						already_added = true;
						break;
					}
				}
				if (already_added) {
					continue;
				}

				receiver_methods.push_back(method_name);
			}
		}
	}

	// Now add the labels in order
	int input_slot_idx = 0;
	for (const String &method_name : receiver_methods) {
		// Add function label with input slot
		String function_text = vformat("- %s", method_name);
		Label *function_label = memnew(Label(function_text));
		function_label->set_modulate(Color(1, 1, 1, 1)); // Pure white to counteract node tint
		gn->add_child(function_label);

		int child_idx = gn->get_child_count() - 1;
		// Use type 0 for all input ports (type is for validation, not indexing)
		gn->set_slot(child_idx, true, 0, Color(1.0, 0.8, 0.6), false, 0, Color());

		// Track the slot for this method
		// The port index is determined by the order of set_slot calls with input enabled
		if (!function_to_slot.has(node_id)) {
			function_to_slot[node_id] = HashMap<String, int>();
		}
		function_to_slot[node_id][method_name] = input_slot_idx;
		input_slot_idx++;
	}
}

void SignalizeDock::_add_emitter_slots(Node *p_node) {
	if (!p_node) {
		return;
	}

	GraphNode **gn_ptr = node_graph_nodes.getptr(p_node->get_instance_id());
	if (!gn_ptr || !*gn_ptr) {
		return;
	}

	GraphNode *gn = *gn_ptr;
	ObjectID node_id = p_node->get_instance_id();

	List<MethodInfo> signals;
	p_node->get_signal_list(&signals);

	// Collect all unique signals from this emitter
	HashSet<String> emitter_signals;
	for (const MethodInfo &sig : signals) {
		List<Object::Connection> conns;
		p_node->get_signal_connection_list(StringName(sig.name), &conns);

		if (conns.size() > 0) {
			emitter_signals.insert(sig.name);
		}
	}

	// Add signal labels with output slots sequentially
	int output_slot_idx = 0;
	for (const String &sig_name : emitter_signals) {
		String signal_text = vformat("- %s", sig_name);
		Label *signal_label = memnew(Label(signal_text));
		signal_label->set_modulate(Color(1, 1, 1, 1)); // Pure white to counteract node tint
		gn->add_child(signal_label);

		int child_idx = gn->get_child_count() - 1;
		// Use type 0 for all output ports (type is for validation, not indexing)
		gn->set_slot(child_idx, false, 0, Color(), true, 0, custom_connection_color);

		// Track the slot for this signal
		// The port index is determined by the order of set_slot calls with output enabled
		if (!signal_to_slot.has(node_id)) {
			signal_to_slot[node_id] = HashMap<String, int>();
		}
		signal_to_slot[node_id][sig_name] = output_slot_idx;
		output_slot_idx++;
	}

	// Now collect all connections from this emitter to build pending_connections
	for (const MethodInfo &sig : signals) {
		List<Object::Connection> conns;
		p_node->get_signal_connection_list(StringName(sig.name), &conns);

		for (const Object::Connection &conn : conns) {
			Object *target_obj = conn.callable.get_object();
			if (!target_obj) {
				continue;
			}

			String method_name = conn.callable.get_method();
			if (method_name.is_empty()) {
				continue;
			}

			ObjectID target_id = target_obj->get_instance_id();

			// Check if both nodes have the required slots
			if (!signal_to_slot.has(node_id) || !signal_to_slot[node_id].has(sig.name)) {
				continue;
			}
			if (!function_to_slot.has(target_id) || !function_to_slot[target_id].has(method_name)) {
				continue;
			}

			// Get slot indices
			int from_slot = signal_to_slot[node_id][sig.name];
			int to_slot = function_to_slot[target_id][method_name];

			// Add to pending connections
			ConnectionSlot conn_slot;
			conn_slot.emitter_id = node_id;
			conn_slot.signal_name = sig.name;
			conn_slot.receiver_id = target_id;
			conn_slot.method_name = method_name;
			conn_slot.from_slot = from_slot;
			conn_slot.to_slot = to_slot;
			pending_connections.push_back(conn_slot);
		}
	}
}

void SignalizeDock::_create_visual_connections() {
	// After all labels and slots are configured, create the visual connections
	for (const ConnectionSlot &conn : pending_connections) {
		String *emitter_name_ptr = node_graph_names.getptr(conn.emitter_id);
		String *receiver_name_ptr = node_graph_names.getptr(conn.receiver_id);

		if (emitter_name_ptr && receiver_name_ptr) {
			graph_edit->connect_node(*emitter_name_ptr, conn.from_slot, *receiver_name_ptr, conn.to_slot);

			// Set default dim state (0.05 = 5% brightness) so the glow (1.0) is very noticeable
			graph_edit->set_connection_activity(*emitter_name_ptr, conn.from_slot, *receiver_name_ptr, conn.to_slot, 0.05);
		}
	}

}

// Simplified callback for runtime signal tracking
void SignalizeDock::_on_signal_fired(Node *p_emitter, const String &p_signal) {
	if (!p_emitter) {
		return;
	}

	// Basic tracking verification - just print to console when a signal fires
	
	// Look up all connections for this signal and log them
	List<Object::Connection> conns;
	p_emitter->get_signal_connection_list(StringName(p_signal), &conns);

	for (const Object::Connection &conn : conns) {
		Object *target_obj = conn.callable.get_object();
		if (!target_obj) {
			continue;
		}

		String method_name = conn.callable.get_method();
		if (method_name.is_empty()) {
			continue;
		}

		// Log each connection that would be triggered
		
		// Also call the detailed callback for counting
		_on_signal_emitted(p_emitter, p_signal, target_obj, method_name);
	}
}

void SignalizeDock::_on_signal_emitted(Node *p_emitter, const String &p_signal, Object *p_target, const String &p_method) {
	if (!p_emitter || !p_target) {
		return;
	}

// Basic tracking verification - just print to console

	// Build key for this specific connection
	String key = vformat("%s|%s|%s|%s",
						String::num_uint64((uint64_t)p_emitter->get_instance_id()),
						p_signal,
						String::num_uint64((uint64_t)p_target->get_instance_id()),
						p_method);

	// Update count if this connection exists
	if (connections.has(key)) {
		connections[key]++;
	} else {
		connections[key] = 1; // Initialize counter if this is the first time we see it
	}
}

void SignalizeDock::_on_runtime_signal_emitted(ObjectID p_emitter_id, const String &p_node_name, const String &p_node_class, const String &p_signal_name, int p_count, const Array &p_connections) {
	// LIVE MODE: Highlight connections when signals fire during gameplay
	// Only process signals for nodes that are currently being inspected
	if (!is_inspecting || p_emitter_id != inspected_node_id) {
				return;
	}

	
	// Update emission count
	if (!node_emits.has(p_emitter_id)) {
		node_emits[p_emitter_id] = HashMap<String, int>();
	}
	node_emits[p_emitter_id][p_signal_name] = node_emits[p_emitter_id].has(p_signal_name) ? node_emits[p_emitter_id][p_signal_name] + p_count : p_count;
	_update_node_emits_label(p_emitter_id);

	// Highlight each connection for this signal
	for (int i = 0; i < p_connections.size(); i++) {
		Array conn_data = p_connections[i];
		if (conn_data.size() < 4) continue;

		ObjectID target_id = conn_data[0];
		String target_method = conn_data[3];

		// Build connection key
		String connection_key = vformat("%s|%s|%s|%s",
			String::num_uint64(p_emitter_id),
			p_signal_name,
			String::num_uint64(target_id),
			target_method);

		// Find the matching ConnectionSlot in pending_connections
		for (const ConnectionSlot &slot : pending_connections) {
			if (slot.emitter_id == p_emitter_id &&
				slot.signal_name == p_signal_name &&
				slot.receiver_id == target_id &&
				slot.method_name == target_method) {

				// Get the graph node names
				String *from_node_name = node_graph_names.getptr(p_emitter_id);
				String *to_node_name = node_graph_names.getptr(target_id);

				if (from_node_name && to_node_name) {
					// Highlight the connection by setting activity to 1.0 (full brightness)
					graph_edit->set_connection_activity(*from_node_name, slot.from_slot, *to_node_name, slot.to_slot, 1.0);


					// Set up timer to fade back to inactive
					// Cancel existing timer if there is one
					if (connection_highlight_timers.has(connection_key)) {
						Timer *old_timer = connection_highlight_timers[connection_key];
						if (old_timer && old_timer->is_inside_tree()) {
							old_timer->queue_free();
						}
					}

					// Create new timer
					Timer *fade_timer = memnew(Timer);
					fade_timer->set_wait_time(connection_pulse_duration); // Use the configurable pulse duration
					fade_timer->set_one_shot(true);
					add_child(fade_timer);

					// Connect timeout to fade function
					fade_timer->connect("timeout", callable_mp(this, &SignalizeDock::_fade_connection_highlight).bind(connection_key));

					connection_highlight_timers[connection_key] = fade_timer;
					fade_timer->start();
				}
				break;
			}
		}
	}
}

void SignalizeDock::_fade_connection_highlight(const String &p_connection_key) {
	// Parse the connection key
	PackedStringArray parts = p_connection_key.split("|");
	if (parts.size() != 4) {
		return;
	}

	ObjectID emitter_id = ObjectID(uint64_t(parts[0].to_int()));
	String signal_name = parts[1];
	ObjectID receiver_id = ObjectID(uint64_t(parts[2].to_int()));
	String method_name = parts[3];

	// Find the matching ConnectionSlot
	for (const ConnectionSlot &slot : pending_connections) {
		if (slot.emitter_id == emitter_id &&
			slot.signal_name == signal_name &&
			slot.receiver_id == receiver_id &&
			slot.method_name == method_name) {

			// Get the graph node names
			String *from_node_name = node_graph_names.getptr(emitter_id);
			String *to_node_name = node_graph_names.getptr(receiver_id);

			if (from_node_name && to_node_name) {
				// Reset to dim state (0.05 = 5% brightness)
				graph_edit->set_connection_activity(*from_node_name, slot.from_slot, *to_node_name, slot.to_slot, 0.05);
			}
			break;
		}
	}

	// Clean up timer
	if (connection_highlight_timers.has(p_connection_key)) {
		Timer *timer = connection_highlight_timers[p_connection_key];
		connection_highlight_timers.erase(p_connection_key);
		if (timer && timer->is_inside_tree()) {
			timer->queue_free();
		}
	}
}

// Engine-level signal tracking implementation
void SignalizeDock::_global_signal_emission_callback(Object *p_emitter, const StringName &p_signal, const Variant **p_args, int p_argcount) {
	// Only track Node objects (skip other Objects like Resources, Refs, etc.)
	Node *emitter_node = Object::cast_to<Node>(p_emitter);
	if (!emitter_node) {
		return; // Not a node, skip
	}

	// Get the signal viewer instance (if it exists and is tracking)
	SignalizeDock *viewer = singleton_instance;
	if (!viewer || !viewer->tracking_enabled) {
		return; // Signal viewer not active or tracking disabled
	}

	// IMPORTANT: Only track signals from the node currently being inspected
	// This prevents tracking all signals in the entire game, which causes lag
	if (viewer->is_inspecting && viewer->inspected_node_id != ObjectID()) {
		ObjectID emitter_id = emitter_node->get_instance_id();
		// Only process if this is the inspected node OR if it's connected to the inspected node
		if (emitter_id != viewer->inspected_node_id) {
			// Check if this emitter has a connection to the inspected node
			bool connects_to_inspected = false;
			List<Object::Connection> conns;
			emitter_node->get_signal_connection_list(p_signal, &conns);

			for (const Object::Connection &conn : conns) {
				Object *target = conn.callable.get_object();
				if (target && target->get_instance_id() == viewer->inspected_node_id) {
					connects_to_inspected = true;
					break;
				}
			}

			if (!connects_to_inspected) {
				return; // Skip - not related to inspected node
			}
		}
	} else {
		// Not inspecting anything, don't track any signals
		return;
	}

	// FILTER OUT ALL GUI/CONTROL CLASSES - Only track gameplay nodes
	// Skip all Control-derived classes (GUI elements)
	Control *as_control = Object::cast_to<Control>(emitter_node);
	if (as_control) {
		return; // Skip ALL Control nodes including VScrollBar, RichTextLabel, etc.
	}

	// Filter out internal engine noise
	String node_name = emitter_node->get_name();
	String node_class = emitter_node->get_class();
	String signal_name = String(p_signal);

	// Skip timer signals from gizmos/updates
	if (signal_name == "timeout" && (
		node_name.contains("Gizmo") || node_name.contains("Update") ||
		(node_name.contains("Timer") && node_class.contains("Editor")))) {
		return; // Skip editor gizmo/update timers
	}

	// Skip skeleton pose updates (fire every frame during animation)
	if (signal_name == "pose_updated" || signal_name == "skeleton_updated") {
		return; // Skip internal animation updates
	}

	// Skip gizmo/editor classes
	if (node_class.contains("Editor") || node_class.contains("Gizmo")) {
		return;
	}

	// Check if this signal has ANY connections
	List<Object::Connection> conns;
	p_emitter->get_signal_connection_list(p_signal, &conns);

	if (conns.is_empty()) {
		return; // No connections at all, skip this signal
	}

	// DEBUG: Log all Area3D signals
	if (String(p_signal) == "body_entered" || String(p_signal) == "body_exited" ||
	    String(p_signal) == "area_entered" || String(p_signal) == "area_exited") {

		// Log all connection targets for debugging
		for (const Object::Connection &conn : conns) {
			Object *target_obj = conn.callable.get_object();
			if (target_obj) {
				String target_name;
				Node *target_node = Object::cast_to<Node>(target_obj);
				if (target_node) {
					target_name = target_node->get_name();
				} else {
					target_name = "<not a node>";
				}
			}
		}
	}

	ObjectID emitter_id = p_emitter->get_instance_id();

	// FILTER STRATEGY:
	// - In editor mode: Only track signals from nodes in our graph (node_graph_nodes)
	// - In gameplay mode: Track ALL signals from game nodes, but skip editor UI nodes
	if (viewer->tracking_runtime_scene) {
		// GAMEPLAY MODE: Track all gameplay signals, filter out editor UI noise
		// Check if ANY connection goes to an editor class - if so, skip this signal
		bool has_editor_target = false;
		for (const Object::Connection &conn : conns) {
			Object *target_obj = conn.callable.get_object();
			if (target_obj) {
				String target_class = target_obj->get_class();
				// If connected to an editor class, this is an editor signal
				if (target_class.contains("Editor") || target_class.contains("SceneTree")) {
					has_editor_target = true;
					break;
				}
			}
		}

		if (has_editor_target) {
			return; // Skip editor signals
		}

		// Also skip common editor UI classes from the emitter
		String node_class = emitter_node->get_class();
		if (node_class.contains("Editor") || node_class.contains("MenuBar") ||
			node_class.contains("Button") || node_class.contains("LineEdit") ||
			node_class.contains("Panel") || node_class.contains("Window") ||
			node_class.contains("Popup") || node_class.contains("Label")) {
			return; // Skip editor UI
		}

		// Track this gameplay signal!
		String node_name = emitter_node->get_name();
	} else {
		// EDITOR MODE: Only track signals from nodes in our graph
		if (!viewer->node_graph_nodes.has(emitter_id)) {
			return; // Not a node we're tracking, skip
		}
	}

	// Track this signal emission
	// Build a unique key for this emission
	String key = vformat("%s|%s", String::num_uint64((uint64_t)p_emitter->get_instance_id()), String(p_signal));

	// Update count
	if (viewer->connections.has(key)) {
		viewer->connections[key]++;
	} else {
		viewer->connections[key] = 1;
	}

	// Print to console for now (Step 1)
	if (!viewer->tracking_runtime_scene) {
			}

	// Look up all connections and log them too
	for (const Object::Connection &conn : conns) {
		Object *target_obj = conn.callable.get_object();
		if (!target_obj) {
			continue;
		}

		String method_name = conn.callable.get_method();
		if (method_name.is_empty()) {
			continue;
		}

			}
}

void SignalizeDock::_enable_signal_tracking() {
	if (tracking_enabled) {
		return; // Already enabled
	}

		Object::set_signal_emission_callback(_global_signal_emission_callback);
	tracking_enabled = true;
}

void SignalizeDock::_disable_signal_tracking() {
	if (!tracking_enabled) {
		return; // Already disabled
	}

		Object::set_signal_emission_callback(nullptr);
	tracking_enabled = false;
}

SignalizeDock::~SignalizeDock() {
// Clean up - disable signal tracking when dock is destroyed
	_disable_signal_tracking();

	// Unregister message capture handlers
	if (EngineDebugger::get_singleton()) {
		EngineDebugger::unregister_message_capture("signal_viewer");
		EngineDebugger::unregister_message_capture("scene");
			}

	// Clear singleton instance to prevent dangling pointer
	singleton_instance = nullptr;
}

void SignalizeDock::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			// Update the MakeFloating icon when theme changes
			if (make_floating_button) {
				make_floating_button->set_button_icon(get_editor_theme_icon(SNAME("MakeFloating")));
			}
			// Update the ColorPicker button icon
			if (connection_color_button) {
				connection_color_button->set_button_icon(get_editor_theme_icon(SNAME("ColorPicker")));
			}
			// Update the Settings button icon
			if (settings_button) {
				settings_button->set_button_icon(get_editor_theme_icon(SNAME("Tools")));
			}
		} break;
	}
}

void SignalizeDock::_update_node_emits_label(ObjectID p_node_id) {
	Label **label_ptr = node_emits_labels.getptr(p_node_id);
	if (!label_ptr || !*label_ptr) {
		return; // No label for this node
	}

	Label *label = *label_ptr;

	// Build the emits string
	HashMap<String, int> *emits_ptr = node_emits.getptr(p_node_id);
	if (!emits_ptr || emits_ptr->is_empty()) {
		label->set_text("Emits: (none)");
		return;
	}

	HashMap<String, int> &emits = *emits_ptr;

	// Collect signal names with their counts into a Vector for sorting
	struct SignalCount {
		String name;
		int count;

		// For sorting: higher count first, then alphabetical
		bool operator<(const SignalCount &p_other) const {
			if (count != p_other.count) {
				return count > p_other.count; // Higher count first (reverse order)
			}
			return name < p_other.name; // Alphabetical for ties
		}
	};

	Vector<SignalCount> signal_list;
	signal_list.resize(emits.size());
	int idx = 0;
	for (const KeyValue<String, int> &kv : emits) {
		signal_list.write[idx].name = kv.key;
		signal_list.write[idx].count = kv.value;
		idx++;
	}

	// Sort using the operator<
	signal_list.sort();

	// Build the string: "Emits: signal1 (5), signal2 (3)"
	String text = "Emits: ";
	int count = 0;
	for (const SignalCount &sc : signal_list) {
		if (count > 0) {
			text += ", ";
		}
		text += vformat("%s (%d)", sc.name, sc.count);
		count++;

		// Limit to 5 signals to keep it readable
		if (count >= 5) {
			int remaining = signal_list.size() - 5;
			if (remaining > 0) {
				text += vformat(" +%d more", remaining);
			}
			break;
		}
	}

	label->set_text(text);
}

void SignalizeDock::_update_node_receives_label(ObjectID p_node_id) {
	Label **label_ptr = node_receives_labels.getptr(p_node_id);
	if (!label_ptr || !*label_ptr) {
		return; // No label for this node
	}

	Label *label = *label_ptr;

	// Build the receives string
	HashMap<String, int> *receives_ptr = node_receives.getptr(p_node_id);
	if (!receives_ptr || receives_ptr->is_empty()) {
		label->set_text("Receives: (none)");
		return;
	}

	HashMap<String, int> &receives = *receives_ptr;

	// Collect method names with their counts into a Vector for sorting
	struct MethodCount {
		String name;
		int count;

		// For sorting: higher count first, then alphabetical
		bool operator<(const MethodCount &p_other) const {
			if (count != p_other.count) {
				return count > p_other.count; // Higher count first (reverse order)
			}
			return name < p_other.name; // Alphabetical for ties
		}
	};

	Vector<MethodCount> method_list;
	method_list.resize(receives.size());
	int idx = 0;
	for (const KeyValue<String, int> &kv : receives) {
		method_list.write[idx].name = kv.key;
		method_list.write[idx].count = kv.value;
		idx++;
	}

	// Sort using the operator<
	method_list.sort();

	// Build the string: "Receives: method1 (5), method2 (3)"
	String text = "Receives: ";
	int count = 0;
	for (const MethodCount &mc : method_list) {
		if (count > 0) {
			text += ", ";
		}
		text += vformat("%s (%d)", mc.name, mc.count);
		count++;

		// Limit to 5 methods to keep it readable
		if (count >= 5) {
			int remaining = method_list.size() - 5;
			if (remaining > 0) {
				text += vformat(" +%d more", remaining);
			}
			break;
		}
	}

	label->set_text(text);
}

void SignalizeDock::_update_connection_labels() {
	// Update all graph node connection labels with current counts
}

// Callbacks for play/stop button presses
void SignalizeDock::_on_play_pressed() {
		// Start timer to periodically check if game is running
	if (game_start_check_timer) {
				game_start_check_timer->start();
			} else {
		ERR_PRINT("[Signalize] ERROR: Timer is null!");
	}
	_on_play_mode_changed(true);
}

void SignalizeDock::_on_game_start_check_timer_timeout() {

	// Check if game is actually running
	EditorInterface *editor_interface = EditorInterface::get_singleton();
	if (!editor_interface) {
		return;
	}

	bool is_playing = editor_interface->is_playing_scene();
	
	if (!is_playing) {
		return; // Game not running yet, wait for next check
	}

	// Game is running! Try to connect via EditorDebuggerNode

	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (debugger_node) {
		
		// Try to get the current debugger session
		ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();

		if (debugger) {

			// Check if session is active before sending
			bool session_active = debugger->is_session_active();
			
			if (!session_active) {
								return; // Keep timer running to retry
			}

			// Send the start_tracking message via ScriptEditorDebugger
			// NOTE: Must use "scene:" prefix because SceneDebugger captures that prefix
			Array args;
			debugger->send_message("scene:signal_viewer:start_tracking", args);
			
			// Stop the timer - we've successfully sent the message
			if (game_start_check_timer) {
				game_start_check_timer->stop();
			}
		} else {
						// Don't stop the timer - keep retrying
			return;
		}
	} else {
		ERR_PRINT("[Signalize] WARNING: No EditorDebuggerNode singleton");
	}

	// Set tracking flag
	tracking_runtime_scene = true;
	
	// Stop the timer - we've detected the game is running
	if (game_start_check_timer) {
		game_start_check_timer->stop();
	}
}

void SignalizeDock::_on_stop_pressed() {
		// Stop the game start check timer
	if (game_start_check_timer) {
		game_start_check_timer->stop();
	}
	_on_play_mode_changed(false);
}

void SignalizeDock::_on_play_mode_changed(bool p_is_playing) {
	
	// Update title label to show (Remote) when game is running
	if (title_label) {
		if (p_is_playing) {
			title_label->set_text("Signalize (Remote)");
		} else {
			title_label->set_text("Signalize");
		}
	}

	// Enable/disable refresh button based on game state
	if (refresh_button) {
		if (p_is_playing) {
			refresh_button->set_disabled(true);
			refresh_button->set_tooltip_text("Disabled During Gameplay");
		} else {
			refresh_button->set_disabled(false);
			refresh_button->set_tooltip_text("Rebuild the signal graph from the edited scene");
		}
	}

	if (p_is_playing) {
		// Game started - switch to Signal Lens style mode
		// Clear the editor graph and wait for manual node inspection
		_clear_inspection();

		// Clear the known ObjectIDs tracker
		known_remote_object_ids.clear();

		
		// Capture the initial remote scene root ID and connect to signals
		EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
		if (debugger_node) {
			ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
			if (debugger) {
				// NOTE: remote_tree_updated signal fires frequently and causes lag
				// Disabled for on-demand inspection - only connect when live tracking is needed
				// debugger->connect("remote_tree_updated", callable_mp(this, &SignalizeDock::_on_remote_tree_updated));
				// 
				// Connect to remote_objects_requested signal to detect node selection in remote tree
				debugger->connect("remote_objects_requested", callable_mp(this, &SignalizeDock::_on_remote_object_selected_in_tree));
				
				// Get the current remote scene root
				const SceneDebuggerTree *remote_tree = debugger->get_remote_tree();
				if (remote_tree && remote_tree->nodes.size() > 0) {
					// The first node in the tree is typically the root
					const List<SceneDebuggerTree::RemoteNode>::Element *first_node = remote_tree->nodes.front();
					if (first_node) {
						remote_scene_root_id = first_node->get().id;
						
						// Track all ObjectIDs in the initial scene (for detecting new nodes in scene transitions)
						for (const List<SceneDebuggerTree::RemoteNode>::Element *E = remote_tree->nodes.front(); E; E = E->next()) {
							known_remote_object_ids.insert(E->get().id);
						}
					}
				}
			}
		}

			} else {
		// Game stopped - disconnect from debugger signals to prevent lag
		EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
		if (debugger_node) {
			ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
			if (debugger) {
				// Disconnect signals to stop receiving updates
				if (debugger->is_connected("remote_objects_requested", callable_mp(this, &SignalizeDock::_on_remote_object_selected_in_tree))) {
					debugger->disconnect("remote_objects_requested", callable_mp(this, &SignalizeDock::_on_remote_object_selected_in_tree));
									}
				// remote_tree_updated is already disabled, but if we ever enable it we should disconnect it too
			}
		}

		// Game stopped - rebuild the editor graph
		_clear_inspection();
		_build_graph();
		remote_scene_root_id = ObjectID(); // Reset root ID
		known_remote_object_ids.clear(); // Clear tracked ObjectIDs
			}
}

void SignalizeDock::_on_remote_tree_updated() {

	// In live mode, we only track new ObjectIDs to detect scene transitions
	// We DON'T auto-regenerate the graph - user must manually click nodes

	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return;
	}

	ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
	if (!debugger) {
		return;
	}

	const SceneDebuggerTree *remote_tree = debugger->get_remote_tree();
	if (!remote_tree || remote_tree->nodes.size() == 0) {
		return;
	}

	// Check for new ObjectIDs we haven't seen before (to detect scene transitions)
	bool has_new_object_ids = false;

	for (const List<SceneDebuggerTree::RemoteNode>::Element *E = remote_tree->nodes.front(); E; E = E->next()) {
		ObjectID obj_id = E->get().id;
		if (!known_remote_object_ids.has(obj_id)) {
			// Found a new ObjectID - scene transition in progress
			has_new_object_ids = true;
			// Add it to our known set
			known_remote_object_ids.insert(obj_id);
		}
	}

	if (has_new_object_ids) {
		// Scene transition detected - update our known ObjectIDs but DON'T clear inspection
		// The user's current inspection is still valid unless the inspected node is actually gone

		// Only clear if the inspected node is no longer in the tree
		if (is_inspecting && inspected_node_id != ObjectID()) {
			bool inspected_node_still_exists = false;
			for (const List<SceneDebuggerTree::RemoteNode>::Element *E = remote_tree->nodes.front(); E; E = E->next()) {
				if (E->get().id == inspected_node_id) {
					inspected_node_still_exists = true;
					break;
				}
			}
			if (!inspected_node_still_exists) {
				_clear_inspection();
			}
		}
	} else {
		// Just property updates, no new nodes - do nothing
	}
}

void SignalizeDock::_on_remote_tree_check_timer_timeout() {
	remote_tree_check_count++;
	
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
				return;
	}

	ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
	if (!debugger) {
				return;
	}

	const SceneDebuggerTree *remote_tree = debugger->get_remote_tree();
	if (!remote_tree) {
		return;
	}

	
	if (remote_tree->nodes.size() == 0) {
		
		// Stop after 10 checks (5 seconds) to avoid infinite loop
		if (remote_tree_check_count >= 10) {
			remote_tree_check_timer->stop();
		}
		return;
	}

	// Tree has nodes! Investigate

	int count = 0;
	for (const SceneDebuggerTree::RemoteNode &remote_node : remote_tree->nodes) {
		if (count >= 5) break; // Only check first 5


		// CRITICAL TEST: Can we get this object via ObjectDB?
		Object *obj = ObjectDB::get_instance(remote_node.id);
		if (obj) {
			Node *node = Object::cast_to<Node>(obj);
			if (node) {

				// Can we get its signals?
				List<MethodInfo> signals;
				node->get_signal_list(&signals);

				// Can we connect to one?
				if (!signals.is_empty()) {
					const MethodInfo &first_signal = signals.front()->get();

					// Try connecting - will this work?!
					obj->connect(first_signal.name, Callable(this, "_on_test_signal"));
				}
			}
		} else {
		}

		count++;
	}

	
	// Stop checking - we got our answer
	remote_tree_check_timer->stop();
}

// Per-node inspection: Button handler to inspect currently selected node
void SignalizeDock::_on_inspect_selected_button_pressed() {
	// Check if game is actually running by checking if there's an active debugger session
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	ScriptEditorDebugger *debugger = nullptr;

	// Only proceed with remote inspection if debugger exists AND has an active session
	if (debugger_node) {
		debugger = debugger_node->get_current_debugger();
	}

	if (debugger && debugger->is_session_active()) {
		// Game IS running - use remote tree inspection
		_inspect_selected_remote_node(debugger);
		return;
	}

	// Game is NOT running - inspect from editor scene tree
	_inspect_selected_editor_node();
}

void SignalizeDock::_inspect_selected_editor_node() {
	EditorNode *editor_node = EditorNode::get_singleton();
	if (!editor_node) {
		ERR_PRINT("[Signalize] No EditorNode available");
		return;
	}

	EditorSelection *editor_selection = editor_node->get_editor_selection();
	if (!editor_selection) {
		return;
	}

	// Get the selected node(s)
	const List<Node *> &selected_nodes = editor_selection->get_top_selected_node_list();
	if (selected_nodes.is_empty()) {
				return;
	}

	// For now, just inspect the first selected node
	Node *selected_node = selected_nodes.front()->get();
	if (!selected_node) {
				return;
	}

	
	// Build graph for just this node
	_build_graph_for_single_node(selected_node);
}

void SignalizeDock::_inspect_selected_remote_node(ScriptEditorDebugger *debugger) {

	// Get the actual Tree widget (not the data structure)
	const Tree *remote_tree_widget = debugger->get_editor_remote_tree();
	if (!remote_tree_widget) {
				return;
	}

	// Get the selected item from the tree
	TreeItem *selected = const_cast<Tree *>(remote_tree_widget)->get_selected();
	if (!selected) {
				return;
	}

	// Debug: Print what we got
	
	// Get the metadata from the selected item
	// EditorDebuggerTree stores ObjectID in column 0 metadata
	Variant metadata = selected->get_metadata(0);
	
	if (metadata.get_type() != Variant::INT) {
		
		// Fallback: Try to match by name from SceneDebuggerTree
		const SceneDebuggerTree *remote_tree_data = debugger->get_remote_tree();
		if (remote_tree_data && !remote_tree_data->nodes.is_empty()) {
			String selected_name = selected->get_text(0);
			
			// Search through all nodes for a matching name
			for (const List<SceneDebuggerTree::RemoteNode>::Element *E = remote_tree_data->nodes.front(); E; E = E->next()) {
				const SceneDebuggerTree::RemoteNode &node = E->get();
				if (node.name == selected_name) {
					
					// Build the path
					String node_path_str = "/root/" + node.name;

					_inspect_remote_node(node.id, node_path_str);
					return;
				}
			}
		}
		return;
	}

	ObjectID node_id = ObjectID(uint64_t(int64_t(metadata)));
	if (node_id.is_null()) {
				return;
	}

	// Get the node name from the tree item
	String node_name = selected->get_text(0);

	// Build the path by walking up the tree hierarchy
	String node_path_str;
	TreeItem *current = selected;
	while (current) {
		String text = current->get_text(0);
		if (!text.is_empty()) {
			if (!node_path_str.is_empty()) {
				node_path_str = text + "/" + node_path_str;
			} else {
				node_path_str = text;
			}
		}
		current = current->get_parent();
	}

	// Prepend /root if needed
	if (!node_path_str.begins_with("/root")) {
		node_path_str = "/root/" + node_path_str;
	}


	// Request signal data for this node from the game process
	_inspect_remote_node(node_id, node_path_str);
}

// Per-node inspection: Send request to game process for signal data
void SignalizeDock::_inspect_remote_node(ObjectID p_node_id, const String &p_node_path) {
	// Check if game is running
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
		return;
	}

	ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
	if (!debugger) {
				return;
	}

	// Send message to game process requesting signal data for this node
	Array args;
	args.append((int64_t)p_node_id);  // Node ID - pass as integer, not string!
	args.append(p_node_path);  // Node path


	// Send with "scene:" prefix to reach SceneDebugger handlers
	// Pass args directly (not wrapped), as the handler expects: [node_id, node_path]
	debugger->_put_msg("scene:signal_viewer_request_node_data", args);


	// Update inspection state
	inspected_node_id = p_node_id;
	inspected_node_path = p_node_path;
	is_inspecting = true;

	// Enable global signal tracking ONLY for this node during gameplay
	// This allows connections to light up when this node's signals fire
	if (!tracking_enabled) {
		_enable_signal_tracking();
			}
}

// Per-node inspection: Handle signal data received from game process
void SignalizeDock::_on_node_signal_data_received(const Array &p_data) {
	// Data format: [node_id, node_name, node_class, [{signal_name, count, [[target_id, target_name, target_class, target_method], ...]}, ...]]
	if (p_data.size() < 4) {
				return;
	}

	
	// Clear previous graph VISUALS but preserve inspection state
	// We need to clear the graph nodes to rebuild them, but keep is_inspecting=true
	List<GraphNode *> nodes_to_delete;
	for (int i = 0; i < graph_edit->get_child_count(); i++) {
		Node *child = graph_edit->get_child(i);
		GraphNode *gn = Object::cast_to<GraphNode>(child);
		if (gn) {
			nodes_to_delete.push_back(gn);
		}
	}

	for (GraphNode *gn : nodes_to_delete) {
		if (gn->get_parent()) {
			gn->get_parent()->remove_child(gn);
		}
		memdelete(gn);
	}

	// Clear all tracking data EXCEPT inspection state
	node_graph_nodes.clear();
	node_graph_names.clear();
	node_colors.clear();
	node_emits_labels.clear();
	node_receives_labels.clear();
	node_emits.clear();
	node_receives.clear();
	signal_to_slot.clear();
	function_to_slot.clear();
	next_emitter_slot_idx.clear();
	next_receiver_slot_idx.clear();
	num_input_ports.clear();
	num_output_ports.clear();
	pending_connections.clear();

	// Parse data
	ObjectID node_id = p_data[0];
	String node_name = p_data[1];
	String node_class = p_data[2];
	Array signal_data = p_data[3];  // Array of signal info dictionaries

	// Track signal emissions for this node
	if (!node_emits.has(node_id)) {
		node_emits[node_id] = HashMap<String, int>();
	}

	// First pass: Collect all receiver methods for each node (like local graph)
	HashMap<ObjectID, List<ReceiverMethodInfo>> receiver_methods_list;

	for (int i = 0; i < signal_data.size(); i++) {
		Array sig_info = signal_data[i];
		if (sig_info.size() < 3) continue;

		String signal_name = sig_info[0];
		int count = sig_info[1];
		Array connections = sig_info[2];

		// Track emission count
		node_emits[node_id][signal_name] = count;

		// Collect all target methods
		for (int j = 0; j < connections.size(); j++) {
			Array conn_data = connections[j];
			if (conn_data.size() < 4) continue;

			ObjectID target_id = conn_data[0];
			String target_name = conn_data[1];
			String target_class = conn_data[2];
			String target_method = conn_data[3];

			// Add to receiver methods list
			if (!receiver_methods_list.has(target_id)) {
				receiver_methods_list[target_id] = List<ReceiverMethodInfo>();
			}

			// Check if already added
			bool already_added = false;
			for (const ReceiverMethodInfo &info : receiver_methods_list[target_id]) {
				if (info.method_name == target_method) {
					already_added = true;
					break;
				}
			}

			if (!already_added) {
				ReceiverMethodInfo info;
				info.target_id = target_id;
				info.method_name = target_method;
				receiver_methods_list[target_id].push_back(info);
			}
		}
	}

	// Second pass: Collect all emitter signals for each node
	HashMap<ObjectID, List<String>> emitter_signals_list;

	// Main node emits signals
	List<String> main_node_signals;
	for (int i = 0; i < signal_data.size(); i++) {
		Array sig_info = signal_data[i];
		if (sig_info.size() < 3) continue;

		String signal_name = sig_info[0];
		Array connections = sig_info[2];

		// Only add if there are connections
		if (connections.size() > 0) {
			main_node_signals.push_back(signal_name);
		}
	}
	emitter_signals_list[node_id] = main_node_signals;

	// Third pass: Create all graph nodes (but don't add to GraphEdit yet)
	// Create main node
	GraphNode *main_node = memnew(GraphNode);
	String main_node_graph_name = vformat("GraphNode_%s", String::num_uint64((uint64_t)node_id));
	main_node->set_title(vformat("%s (%s)", node_name, node_class));
	main_node->set_position_offset(Vector2(100, 100));
	main_node->set_name(main_node_graph_name);

	// Use the same color as editor mode (actual editor icon color)
	Color node_color = _get_editor_node_icon_color_by_class(node_class);
	Ref<StyleBoxFlat> sb_colored = main_node->get_theme_stylebox(SNAME("titlebar"), SNAME("GraphNode"))->duplicate();
	sb_colored->set_bg_color(node_color);
	main_node->add_theme_style_override(SNAME("titlebar"), sb_colored);

	Ref<StyleBoxFlat> sb_colored_selected = main_node->get_theme_stylebox(SNAME("titlebar_selected"), SNAME("GraphNode"))->duplicate();
	sb_colored_selected->set_bg_color(node_color.lightened(0.2));
	main_node->add_theme_style_override(SNAME("titlebar_selected"), sb_colored_selected);

	HBoxContainer *titlebar = main_node->get_titlebar_hbox();
	if (titlebar) {
		for (int i = 0; i < titlebar->get_child_count(); i++) {
			Label *title_label = Object::cast_to<Label>(titlebar->get_child(i));
			if (title_label) {
				title_label->add_theme_color_override("font_color", Color(0, 0, 0, 1));
				break;
			}
		}
	}

	node_colors[node_id] = node_color;
	node_graph_nodes[node_id] = main_node;
	node_graph_names[node_id] = main_node_graph_name;

	// Create target nodes
	for (const KeyValue<ObjectID, List<ReceiverMethodInfo>> &KV : receiver_methods_list) {
		ObjectID target_id = KV.key;

		// Skip if this is the main node (it can have both emits and receives)
		if (target_id == node_id) {
			continue;
		}

		// Find target info from signal data
		String target_name;
		String target_class;
		for (int i = 0; i < signal_data.size(); i++) {
			Array sig_info = signal_data[i];
			if (sig_info.size() < 3) continue;
			Array connections = sig_info[2];
			for (int j = 0; j < connections.size(); j++) {
				Array conn_data = connections[j];
				if (conn_data.size() < 4) continue;
				if (ObjectID(uint64_t(int64_t(conn_data[0]))) == target_id) {
					target_name = conn_data[1];
					target_class = conn_data[2];
					break;
				}
			}
			if (!target_name.is_empty()) break;
		}

		GraphNode *target_gn = memnew(GraphNode);
		String target_graph_name = vformat("GraphNode_%s", String::num_uint64((uint64_t)target_id));
		target_gn->set_title(vformat("%s (%s)", target_name, target_class));
		target_gn->set_name(target_graph_name);
		target_gn->set_position_offset(Vector2(400, 100 + node_graph_nodes.size() * 150));

		Color target_color = _get_editor_node_icon_color_by_class(target_class);
		Ref<StyleBoxFlat> target_sb = target_gn->get_theme_stylebox(SNAME("titlebar"), SNAME("GraphNode"))->duplicate();
		target_sb->set_bg_color(target_color);
		target_gn->add_theme_style_override(SNAME("titlebar"), target_sb);

		Ref<StyleBoxFlat> target_sb_selected = target_gn->get_theme_stylebox(SNAME("titlebar_selected"), SNAME("GraphNode"))->duplicate();
		target_sb_selected->set_bg_color(target_color.lightened(0.2));
		target_gn->add_theme_style_override(SNAME("titlebar_selected"), target_sb_selected);

		HBoxContainer *target_titlebar = target_gn->get_titlebar_hbox();
		if (target_titlebar) {
			for (int i = 0; i < target_titlebar->get_child_count(); i++) {
				Label *title_label = Object::cast_to<Label>(target_titlebar->get_child(i));
				if (title_label) {
					title_label->add_theme_color_override("font_color", Color(0, 0, 0, 1));
					break;
				}
			}
		}

		node_colors[target_id] = target_color;
		node_graph_nodes[target_id] = target_gn;
		node_graph_names[target_id] = target_graph_name;
	}

	// Fourth pass: Add all labels and configure slots BEFORE adding to GraphEdit
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		ObjectID obj_id = kv.key;
		GraphNode *gn = kv.value;

		// Add receiver labels first (input slots on left)
		if (receiver_methods_list.has(obj_id)) {
			for (const ReceiverMethodInfo &info : receiver_methods_list[obj_id]) {
				// Create hbox with label + button
				HBoxContainer *hbox = memnew(HBoxContainer);
				gn->add_child(hbox);

				String function_text = vformat("- %s", info.method_name);
				Label *function_label = memnew(Label(function_text));
				function_label->set_h_size_flags(SIZE_EXPAND_FILL);
				function_label->set_modulate(Color(1, 1, 1, 1));
				hbox->add_child(function_label);

				Button *open_button = memnew(Button);
				open_button->set_text("Open");
				open_button->connect("pressed", callable_mp(this, &SignalizeDock::_on_open_function_button_pressed).bind(info.target_id, info.method_name));
				hbox->add_child(open_button);
			}
		}

		// Then add emitter labels (output slots on right)
		if (emitter_signals_list.has(obj_id)) {
			for (const String &sig_name : emitter_signals_list[obj_id]) {
				String signal_text = vformat("- %s", sig_name);
				Label *signal_label = memnew(Label(signal_text));
				signal_label->set_modulate(Color(1, 1, 1, 1));
				gn->add_child(signal_label);
			}
		}
	}

	// Fifth pass: Configure all slots using child indices
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		ObjectID obj_id = kv.key;
		GraphNode *gn = kv.value;

		int current_child_idx = 0;

		// Configure input slots for receiver labels first
		if (receiver_methods_list.has(obj_id)) {
			int slot_idx = 0;
			for (const ReceiverMethodInfo &info : receiver_methods_list[obj_id]) {
				gn->set_slot(current_child_idx, true, 0, Color(1.0, 0.8, 0.6), false, 0, Color());

				if (!function_to_slot.has(obj_id)) {
					function_to_slot[obj_id] = HashMap<String, int>();
				}
				function_to_slot[obj_id][info.method_name] = slot_idx;
				slot_idx++;
				current_child_idx++;
			}
		}

		// Configure output slots for emitter labels (after receiver labels)
		if (emitter_signals_list.has(obj_id)) {
			int slot_idx = 0;
			for (const String &sig_name : emitter_signals_list[obj_id]) {
				gn->set_slot(current_child_idx, false, 0, Color(), true, 0, custom_connection_color);

				if (!signal_to_slot.has(obj_id)) {
					signal_to_slot[obj_id] = HashMap<String, int>();
				}
				signal_to_slot[obj_id][sig_name] = slot_idx;
				slot_idx++;
				current_child_idx++;
			}
		}
	}

	// Sixth pass: NOW add all GraphNodes to GraphEdit after slots are configured
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		GraphNode *gn = kv.value;
		if (gn->get_parent() == nullptr) {
			graph_edit->add_child(gn);
		}
	}

	// Seventh pass: Create pending connections
	for (int i = 0; i < signal_data.size(); i++) {
		Array sig_info = signal_data[i];
		if (sig_info.size() < 3) continue;

		String signal_name = sig_info[0];
		Array connections = sig_info[2];

		if (!signal_to_slot.has(node_id) || !signal_to_slot[node_id].has(signal_name)) {
			continue;
		}
		int from_slot = signal_to_slot[node_id][signal_name];

		for (int j = 0; j < connections.size(); j++) {
			Array conn_data = connections[j];
			if (conn_data.size() < 4) continue;

			ObjectID target_id = conn_data[0];
			String target_method = conn_data[3];

			if (!function_to_slot.has(target_id) || !function_to_slot[target_id].has(target_method)) {
				continue;
			}
			int to_slot = function_to_slot[target_id][target_method];

			ConnectionSlot conn_slot;
			conn_slot.emitter_id = node_id;
			conn_slot.signal_name = signal_name;
			conn_slot.receiver_id = target_id;
			conn_slot.method_name = target_method;
			conn_slot.from_slot = from_slot;
			conn_slot.to_slot = to_slot;
			pending_connections.push_back(conn_slot);

		}
	}

	// Update labels
	for (const KeyValue<ObjectID, GraphNode *> &kv : node_graph_nodes) {
		ObjectID obj_id = kv.key;
		if (node_emits.has(obj_id)) {
			_update_node_emits_label(obj_id);
		}
		if (node_receives.has(obj_id)) {
			_update_node_receives_label(obj_id);
		}
	}

	// Create visual connections
	call_deferred("_create_visual_connections");

}

// Per-node inspection: Clear current inspection
void SignalizeDock::_clear_inspection() {

	// Clear all graph nodes
	List<GraphNode *> nodes_to_delete;
	for (int i = 0; i < graph_edit->get_child_count(); i++) {
		Node *child = graph_edit->get_child(i);
		GraphNode *gn = Object::cast_to<GraphNode>(child);
		if (gn) {
			nodes_to_delete.push_back(gn);
		}
	}

	for (GraphNode *gn : nodes_to_delete) {
		if (gn->get_parent()) {
			gn->get_parent()->remove_child(gn);
		}
		memdelete(gn);
	}

	// Clear tracking data
	node_graph_nodes.clear();
	node_graph_names.clear();
	node_colors.clear();
	node_emits_labels.clear();
	node_receives_labels.clear();
	node_emits.clear();
	node_receives.clear();
	signal_to_slot.clear();
	function_to_slot.clear();
	next_emitter_slot_idx.clear();
	next_receiver_slot_idx.clear();
	num_input_ports.clear();
	num_output_ports.clear();
	pending_connections.clear();

	// Disable global signal tracking when inspection is cleared
	// This stops tracking all signals when no node is being inspected
	if (tracking_enabled) {
		_disable_signal_tracking();
			}

	// Clear inspection state
	inspected_node_id = ObjectID();
	inspected_node_path = "";
	is_inspecting = false;
}

// Called by inspector plugin when a node is inspected in the Remote tree
void SignalizeDock::_on_node_inspected_in_remote_tree(ObjectID p_node_id, const String &p_node_path) {

	// Don't auto-inspect if we're already manually inspecting a node
	// This prevents automatic inspections (from clicking into the game) from overriding manual inspections
	if (is_inspecting) {
		return;
	}

	// Only auto-inspect if game is running
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
				return;
	}

	ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
	if (!debugger) {
				return;
	}

	// Auto-inspect this node
	_inspect_remote_node(p_node_id, p_node_path);
}

// Called when a node is clicked in the remote tree
void SignalizeDock::_on_remote_object_selected_in_tree(const Array &p_object_ids) {
	
	if (p_object_ids.is_empty()) {
				return;
	}

	// Get the first selected object ID
	ObjectID selected_id = ObjectID(uint64_t(int64_t(p_object_ids[0])));
	
	// Get the node from the ObjectDB
	Object *obj = ObjectDB::get_instance(selected_id);
	if (!obj) {
				return;
	}

	Node *node = Object::cast_to<Node>(obj);
	if (!node) {
				return;
	}

	// Get the node path
	NodePath node_path = node->get_path();
	String node_path_str = node_path.operator String();
	String node_name = node->get_name();


	// Only inspect if game is running
	EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
	if (!debugger_node) {
				return;
	}

	ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
	if (!debugger) {
				return;
	}

	// Inspect this node
		_inspect_remote_node(selected_id, node_path_str);
}

// Message capture handler - receives signal emissions from game
Error SignalizeDock::_capture_signal_viewer_messages(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	// Debug: Print all scene messages to understand the flow
	if (p_msg.begins_with("inspect") || p_msg.contains("selected") || p_msg.contains("remote")) {
			}

	if (p_msg == "signal_viewer:signal_emitted") {
		r_captured = true;

		// Parse the signal emission data from the game
		if (p_args.size() >= 5) {
			// p_args[0] is ObjectID (uint64 as int/Variant)
			ObjectID emitter_id = ObjectID(uint64_t(int64_t(p_args[0])));
			String node_name = p_args[1];
			String node_class = p_args[2];
			String signal_name = p_args[3];
			Array connections = p_args[4];

			// Print to console (Step 1)
			
			// Log connections
			for (int i = 0; i < connections.size(); i++) {
				Array conn_data = connections[i];
				if (conn_data.size() >= 3) {
					String target_class = conn_data[1];
					String method_name = conn_data[2];
									}
			}

			// Update the graph visualization
			SignalizeDock *viewer = singleton_instance;
			if (viewer) {
				viewer->_on_runtime_signal_emitted(emitter_id, node_name, node_class, signal_name, 1, connections);
			}
		}

		return OK;
	}

	if (p_msg == "signal_viewer:node_signal_data") {
		r_captured = true;

		// Handle node signal data response from game
		// Get the singleton instance and call the handler
		SignalizeDock *viewer = singleton_instance;
		if (viewer) {
			viewer->_on_node_signal_data_received(p_args);
		}

		return OK;
	}

	// Capture inspect_objects to detect when nodes are clicked in remote tree
	// (registered for "scene" prefix, so p_msg is "inspect_objects" not "scene:inspect_objects")
	if (p_msg == "inspect_objects") {
		
		// Don't capture this message - let the normal debugger handle it
		// But we can inspect what's being selected
		if (p_args.size() > 0) {
			Array object_ids = p_args[0];
			if (!object_ids.is_empty()) {
				ObjectID selected_id = ObjectID(uint64_t(int64_t(object_ids[0])));

				
				// Get the singleton and trigger inspection
				SignalizeDock *viewer = singleton_instance;
				if (viewer) {
					// Check if game is running (live mode)
					EditorDebuggerNode *debugger_node = EditorDebuggerNode::get_singleton();
					if (debugger_node) {
						ScriptEditorDebugger *debugger = debugger_node->get_current_debugger();
						if (debugger) {
							// Game is running - inspect this node
							// Get node info
							Object *obj = ObjectDB::get_instance(selected_id);
							if (obj) {
								Node *node = Object::cast_to<Node>(obj);
								if (node) {
									NodePath node_path = node->get_path();
									String node_path_str = node_path.operator String();
																		viewer->_inspect_remote_node(selected_id, node_path_str);
								}
							}
						}
					}
				}
			}
		}
	}

	r_captured = false;
	return ERR_UNAVAILABLE;
}
