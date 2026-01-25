#pragma once

#include "scene/gui/graph_edit.h"
#include "scene/gui/button.h"
#include "scene/gui/line_edit.h"
#include "scene/main/node.h"

#include "core/object/object_id.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/hash_set.h"

// Forward declarations
class SignalizeInspectorPlugin;
class WindowWrapper;
class ScriptEditorDebugger;
class EditorDebuggerNode;
class ColorPickerButton;
class AcceptDialog;

class SignalizeDock : public VBoxContainer {
	GDCLASS(SignalizeDock, VBoxContainer);

	friend class SignalizeInspectorPlugin; // Allow inspector plugin to access private methods

private:
	GraphEdit *graph_edit = nullptr;
	Button *refresh_button = nullptr;
	LineEdit *search_box = nullptr;
	Node *selected_node = nullptr;
	Button *tool_button = nullptr; // Reference to the bottom panel button
	Button *make_floating_button = nullptr; // Button to make window floating
	WindowWrapper *window_wrapper = nullptr; // Wrapper for floating window support
	ColorPickerButton *connection_color_button = nullptr; // Color picker for connection lines
	Color custom_connection_color = Color(0.5, 0.1, 0.1); // Default dark red
	Button *settings_button = nullptr; // Settings button
	float connection_pulse_duration = 0.3; // Duration of connection highlight in seconds
	AcceptDialog *settings_dialog = nullptr; // Settings popup dialog

	// Verbosity control: 0=Silent, 1=Quiet, 2=Normal, 3=Verbose
	int verbosity_level = 0; // Default to Silent (no console spam)

	// Helper to check if we should log at a given verbosity level
	bool should_log(int p_level) const { return verbosity_level >= p_level; }

	VBoxContainer *content_container = nullptr; // Holds the actual UI, can be reparented
	bool is_floating = false; // Track floating state
	Label *title_label = nullptr; // Reference to title label to update with (Remote)

	// Track graph nodes and their positions
	HashMap<ObjectID, GraphNode *> node_graph_nodes;
	HashMap<ObjectID, String> node_graph_names;
	HashMap<ObjectID, Color> node_colors;  // Track color for each node
	HashMap<ObjectID, Vector2> saved_node_positions;  // Track manually positioned nodes

	// Track labels for each node to update signal info
	HashMap<ObjectID, Label *> node_emits_labels;  // Shows "Emits: signal1 (5), signal2 (3)"
	HashMap<ObjectID, Label *> node_receives_labels; // Shows "Receives: method1 (2), method2 (1)"

	// Track which signals/methods each node has (for updating labels)
	HashMap<ObjectID, HashMap<String, int>> node_emits;     // node_id -> signal_name -> count
	HashMap<ObjectID, HashMap<String, int>> node_receives;  // node_id -> method_name -> count

	// Track signal emissions using String key: "emitter_id|signal|target_id|method" -> count
	HashMap<String, int> connections;

	// Track which slot index corresponds to each signal/method
	HashMap<ObjectID, HashMap<String, int>> signal_to_slot; // emitter_id -> signal_name -> slot_idx
	HashMap<ObjectID, HashMap<String, int>> function_to_slot; // receiver_id -> method_name -> slot_idx

	// Track next available port slot index for each node (sequential from 0)
	HashMap<ObjectID, int> next_emitter_slot_idx;
	HashMap<ObjectID, int> next_receiver_slot_idx;
	HashMap<ObjectID, int> num_input_ports;  // Track how many input ports each node has
	HashMap<ObjectID, int> num_output_ports; // Track how many output ports each node has

	// Track connections to make: emitter_id, signal_name, target_id, method_name -> from_slot, to_slot
	struct ConnectionSlot {
		ObjectID emitter_id;
		String signal_name;
		ObjectID receiver_id;
		String method_name;
		int from_slot;
		int to_slot;
	};
	List<ConnectionSlot> pending_connections;

	// Track receiver method with its actual target object (for opening scripts)
	struct ReceiverMethodInfo {
		ObjectID target_id;  // The object that actually owns the method/script
		String method_name;
	};

	void _build_graph();
	void _collect_all_nodes(Node *p_node, List<Node *> &r_list);
	bool _node_has_connections(Node *p_node);
	void _create_graph_node(Node *p_node, int p_depth, int p_index);
	Color _get_editor_node_icon_color(Node *p_node); // Get actual icon color from editor
	Color _get_editor_node_icon_color_by_class(const String &p_class_name); // Get icon color by class name
	Color _get_node_type_color(const String &p_class_name);
	void _connect_all_node_signals();
	void _cleanup_runtime_signal_tracking(); // STEP 1: Disconnect old tracking connections
	void _connect_runtime_signal_tracking(); // STEP 1: Connect to signals for runtime tracking
	void _add_receiver_slots(Node *p_node);
	void _add_emitter_slots(Node *p_node);
	void _create_visual_connections();

	// Track which functions we've added to each receiver to avoid duplicates
	HashMap<ObjectID, HashSet<String>> receiver_functions;

	// Track runtime signal connections so we can disconnect them later
	HashMap<ObjectID, HashMap<String, int>> runtime_signal_connections; // emitter_id -> signal_name -> connection_count

	// Track which scene tree we're connected to (editor or runtime)
	bool tracking_runtime_scene = false;

	// STEP 1: Engine-level signal tracking hook
	static void _global_signal_emission_callback(Object *p_emitter, const StringName &p_signal, const Variant **p_args, int p_argcount);
	void _enable_signal_tracking();
	void _disable_signal_tracking();
	bool tracking_enabled = false;

	// Static instance pointer for the global callback to access
	static SignalizeDock *singleton_instance;

	// Track play mode state to detect changes
	bool was_playing_last_frame = false;

	// Track remote scene root to detect actual scene changes (not just property updates)
	ObjectID remote_scene_root_id;

	// Track all ObjectIDs we've seen in the remote scene tree
	// When new ObjectIDs appear (new nodes instantiated), clear and regenerate
	HashSet<ObjectID> known_remote_object_ids;

	// Per-node inspection: Track currently inspected node
	ObjectID inspected_node_id;
	String inspected_node_path;
	bool is_inspecting = false;

	// STEP 1: Timer for retrying start_tracking message until game is running
	Timer *game_start_check_timer = nullptr;
	void _on_game_start_check_timer_timeout();

	// STEP 1: Timer for checking remote tree population
	Timer *remote_tree_check_timer = nullptr;
	void _on_remote_tree_check_timer_timeout();
	int remote_tree_check_count = 0;

	// Highlight timers for fading signal connection highlights
	HashMap<String, Timer *> connection_highlight_timers;
	void _fade_connection_highlight(const String &p_connection_key);

	// Runtime tracking callback (simplified for Step 1)
	void _on_signal_fired(Node *p_emitter, const String &p_signal);
	void _on_signal_emitted(Node *p_emitter, const String &p_signal, Object *p_target, const String &p_method);
	void _on_test_signal(); // Test handler for investigating remote node access
	void _on_refresh_pressed();
	void _on_make_floating_pressed(); // Toggle floating window
	void _on_inspect_selected_button_pressed(); // Inspect currently selected remote node
	void _on_search_changed(const String &p_text);
	void _on_connection_color_changed(const Color &p_color); // Connection color picker changed
	void _on_settings_pressed(); // Open settings dialog
	void _on_pulse_duration_changed(double p_value); // Handle pulse duration spinbox change
	void _on_verbosity_changed(int p_level); // Handle verbosity dropdown change
	void _on_open_function_button_pressed(ObjectID p_node_id, const String &p_method_name); // Open script to function
	void _update_node_emits_label(ObjectID p_node_id); // Update the "Emits" label with signal counts
	void _update_node_receives_label(ObjectID p_node_id); // Update the "Receives" label with method counts
	void _update_connection_labels();

	// Per-node inspection methods
	void _inspect_selected_editor_node(); // Inspect selected node in editor (game not running)
	void _inspect_selected_remote_node(ScriptEditorDebugger *debugger); // Inspect selected node in remote tree (game running)
	void _inspect_remote_node(ObjectID p_node_id, const String &p_node_path); // Request signal data for a node
	void _clear_inspection(); // Clear current inspection and graph
	void _on_node_inspected_in_remote_tree(ObjectID p_node_id, const String &p_node_path); // Called by inspector plugin when node is inspected

	void _build_graph_for_single_node(Node *p_node); // Build graph for a single node in editor mode
	void _on_remote_object_selected_in_tree(const Array &p_object_ids); // Called when node is clicked in remote tree

	// Inspector plugin instance
	SignalizeInspectorPlugin *inspector_plugin = nullptr;

	// STEP 1: Handle play mode changes to rebuild graph with remote nodes
	void _on_play_pressed();
	void _on_stop_pressed();
	void _on_play_mode_changed(bool p_is_playing);
	void _on_remote_tree_updated(); // Called when remote scene tree changes

	// STEP 1: Debugger message capture handler - receives signal emissions from game
	static Error _capture_signal_viewer_messages(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured);

	// Message constants
	static const String MESSAGE_SIGNAL_EMITTED;
	static const String MESSAGE_NODE_SIGNAL_DATA;

	// Destructor to ensure we clean up tracking
	~SignalizeDock();

	void _notification(int p_what); // Handle theme changes to update icon

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_on_play_mode_changed", "is_playing"), &SignalizeDock::_on_play_mode_changed);
		ClassDB::bind_method(D_METHOD("_on_refresh_pressed"), &SignalizeDock::_on_refresh_pressed);
		ClassDB::bind_method(D_METHOD("_on_make_floating_pressed"), &SignalizeDock::_on_make_floating_pressed);
		ClassDB::bind_method(D_METHOD("_on_search_changed", "text"), &SignalizeDock::_on_search_changed);
		ClassDB::bind_method(D_METHOD("_on_connection_color_changed", "color"), &SignalizeDock::_on_connection_color_changed);
		ClassDB::bind_method(D_METHOD("_on_settings_pressed"), &SignalizeDock::_on_settings_pressed);
		ClassDB::bind_method(D_METHOD("_on_pulse_duration_changed", "value"), &SignalizeDock::_on_pulse_duration_changed);
		ClassDB::bind_method(D_METHOD("_on_verbosity_changed", "level"), &SignalizeDock::_on_verbosity_changed);
		ClassDB::bind_method(D_METHOD("_on_signal_fired", "emitter", "signal"), &SignalizeDock::_on_signal_fired);
		ClassDB::bind_method(D_METHOD("_on_signal_emitted", "emitter", "signal", "target", "method"), &SignalizeDock::_on_signal_emitted);
		ClassDB::bind_method(D_METHOD("_on_test_signal"), &SignalizeDock::_on_test_signal);
		ClassDB::bind_method(D_METHOD("_on_remote_tree_updated"), &SignalizeDock::_on_remote_tree_updated);
		ClassDB::bind_method(D_METHOD("_on_remote_object_selected_in_tree", "object_ids"), &SignalizeDock::_on_remote_object_selected_in_tree);
		ClassDB::bind_method(D_METHOD("_create_visual_connections"), &SignalizeDock::_create_visual_connections);
		ClassDB::bind_method(D_METHOD("_fade_connection_highlight", "connection_key"), &SignalizeDock::_fade_connection_highlight);
	}

public:
	// Singleton accessor for receiving runtime signal updates
	static SignalizeDock *get_singleton() { return singleton_instance; }

	// Set reference to the bottom panel button (for updating icon/badge if needed)
	void set_tool_button(Button *p_button) { tool_button = p_button; }

	// Called when a runtime signal is emitted from the game process
	void _on_runtime_signal_emitted(ObjectID p_emitter_id, const String &p_node_name, const String &p_node_class, const String &p_signal_name, int p_count, const Array &p_connections);

	// Called when signal data is received from game process (made public for ScriptEditorDebugger)
	void _on_node_signal_data_received(const Array &p_data);

	SignalizeDock();
};

#include "editor/inspector/editor_inspector.h"

// Inspector plugin to detect when nodes are inspected in the Remote tree
// This allows us to automatically show signal data when user double-clicks a node
class SignalizeInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(SignalizeInspectorPlugin, EditorInspectorPlugin);

private:
	SignalizeDock *signal_viewer_dock = nullptr;

public:
	void set_signal_viewer_dock(SignalizeDock *p_dock) {
		signal_viewer_dock = p_dock;
	}

	// This is called when any object is inspected in the editor (including Remote tree)
	virtual bool can_handle(Object *p_object) override {
		if (!signal_viewer_dock) {
			return false;
		}

		// Only handle Node objects
		Node *node = Object::cast_to<Node>(p_object);
		if (!node) {
			return false;
		}

		// Check if this node has a "Node/path" property (indicates it's from Remote tree)
		// Actually, let's just handle ALL nodes and check in parse_begin
		return true;
	}

	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide = false) override {
		// We don't want to modify property display, just detect when a node is being inspected
		return false;
	}

	// Called when parsing of an object begins (node is being inspected)
	virtual void parse_begin(Object *p_object) override {
		if (!signal_viewer_dock || !p_object) {
			return;
		}

		Node *node = Object::cast_to<Node>(p_object);
		if (!node) {
			return;
		}

		// Get node info
		ObjectID node_id = node->get_instance_id();
		NodePath node_path_obj = node->get_path();
		String node_path = node_path_obj.operator String();
		String node_name = node->get_name();

		// Log inspector update (level 2 - Normal) with shortened path
		if (signal_viewer_dock->should_log(2)) {
			// Strip editor UI hierarchy, show only scene path
			String short_path = node_path;
			if (node_path.contains("/@EditorNode@")) {
				// Extract just the scene portion after @SubViewport@
				int subview_idx = node_path.find("/@SubViewport@");
				if (subview_idx != -1) {
					short_path = node_path.substr(subview_idx + 15); // Skip "/@SubViewport@"
				}
			}
			print_line(vformat("[Signalize Inspector] Node inspected: %s (path: %s)", node_name, short_path));
		}

		// Notify the dock to inspect this node
		signal_viewer_dock->_on_node_inspected_in_remote_tree(node_id, node_path);
	}

protected:
	static void _bind_methods() {}
};
