/**************************************************************************/
/*  signal_viewer_runtime.h                                               */
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

#pragma once

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

class SignalViewerRuntime : public Object {
	GDCLASS(SignalViewerRuntime, Object);

private:
	static SignalViewerRuntime *singleton;
	bool tracking_enabled = false;

	// Verbosity control: 0=Silent, 1=Quiet, 2=Normal, 3=Verbose
	int verbosity_level = 0;

	// Track which nodes we're monitoring to avoid duplicate messages
	HashSet<ObjectID> monitored_nodes;

	// Track signal emission counts and last sent time for rate limiting
	// Key: "emitter_id:signal_name" -> count
	HashMap<String, int> signal_emission_counts;
	// Key: "emitter_id:signal_name" -> last sent timestamp (uint64_t)
	HashMap<String, uint64_t> signal_last_sent_time;
	// Key: "emitter_id:signal_name" -> connections_array (targets and methods)
	HashMap<String, Array> signal_connections;

	// Minimum time between sending messages for the same signal (in milliseconds)
	static const uint64_t RATE_LIMIT_MS = 1000; // 1 second

	// Timer for periodic batch updates
	uint64_t last_batch_update_time = 0;
	static const uint64_t BATCH_UPDATE_INTERVAL_MS = 2000; // Send batch updates every 2 seconds

	// Signal emission callback - called by Object::emit_signal() in the game process
	static void _signal_emission_callback(Object *p_emitter, const StringName &p_signal, const Variant **p_args, int p_argcount);

	// Send accumulated signal counts to editor
	void _send_signal_update(const String &p_key, ObjectID p_emitter_id, const String &p_node_name, const String &p_node_class, const String &p_signal_name, int p_count, const Array &p_connections);

	// Send all accumulated signals as a batch update
	void _send_batch_updates();

public:
	SignalViewerRuntime();
	~SignalViewerRuntime();

	static SignalViewerRuntime *get_singleton() { return singleton; }

	// Create and set the singleton (for use by SceneDebugger)
	static SignalViewerRuntime *create_singleton();

	// Destroy the singleton
	static void destroy_singleton();

	// Start tracking signal emissions
	void start_tracking();

	// Stop tracking signal emissions
	void stop_tracking();

	// Check if tracking is enabled
	bool is_tracking_enabled() const { return tracking_enabled; }

	// Set verbosity level (0=Silent, 1=Quiet, 2=Normal, 3=Verbose)
	void set_verbosity(int p_level) { verbosity_level = CLAMP(p_level, 0, 3); }

	// Get current verbosity level
	int get_verbosity() const { return verbosity_level; }

	// Helper to check if we should log at a given verbosity level
	bool should_log(int p_level) const { return verbosity_level >= p_level; }

	// Handle request for node signal data (per-node inspection)
	// Made public so SceneDebugger can call it
	void send_node_signal_data(ObjectID p_node_id);

protected:
	static void _bind_methods();
};
