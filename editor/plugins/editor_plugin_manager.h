/**************************************************************************/
/*  editor_plugin_manager.h                                               */
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

/**
 * @file editor_plugin_manager.h
 *
 * Owns the editor-plugin registry, addon state, and resource-conversion
 * plugin list. Extracted from `EditorNode` to decouple plugin lifecycle
 * management from the shell.
 */

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"

class ConfigFile;
class EditorCore;
class EditorPlugin;
class EditorPluginList;
class EditorResourceConversionPlugin;
class EditorShell;
class Resource;

typedef void (*EditorNodeInitCallback)();
typedef void (*EditorPluginInitializeCallback)();
typedef bool (*EditorBuildCallback)();

/// Manages the registry of `EditorPlugin` instances, addon enable/disable
/// state, and resource-conversion plugins.
///
/// This is a `RefCounted` singleton (not a `Node`); its lifetime is owned
/// by `EditorCore`. It holds raw, non-owning pointers to `EditorCore` and
/// `EditorShell` for service access.
class EditorPluginManager : public RefCounted {
	GDCLASS(EditorPluginManager, RefCounted);

	static EditorPluginManager *singleton;

	// ── Service pointers (non-owning, set by `EditorCore`) ───────────────
	EditorCore *core = nullptr;
	EditorShell *shell = nullptr;

	// ── Plugin dispatch lists (owned) ─────────────────────────────────────
	EditorPluginList *editor_plugins_force_input_forwarding = nullptr;
	EditorPluginList *editor_plugins_force_over = nullptr;
	EditorPluginList *editor_plugins_over = nullptr;

	// ── Plugin / addon state ──────────────────────────────────────────────
	Vector<EditorPlugin *> editor_plugins;
	bool _initializing_plugins = false;
	HashMap<String, EditorPlugin *> addon_name_to_plugin;
	LocalVector<String> pending_addons;
	HashMap<ObjectID, HashSet<EditorPlugin *>> active_plugins;
	bool is_main_screen_editing = false;

	// ── Resource-conversion plugins ───────────────────────────────────────
	Vector<Ref<EditorResourceConversionPlugin>> resource_conversion_plugins;

	// ── Callback registries ───────────────────────────────────────────────
	enum {
		MAX_INIT_CALLBACKS = 128,
		MAX_BUILD_CALLBACKS = 128,
	};

	static EditorBuildCallback build_callbacks[MAX_BUILD_CALLBACKS];
	static EditorPluginInitializeCallback plugin_init_callbacks[MAX_INIT_CALLBACKS];
	static int build_callback_count;
	static int plugin_init_callback_count;


	// ── Internal helpers ──────────────────────────────────────────────────
	void _remove_plugin_from_enabled(const String &p_name);
	void _update_addon_config();
	void _enable_pending_addons();

public:
	EditorPluginManager();

	static EditorPluginManager *get_singleton() {
		return singleton;
	}

	void set_core(EditorCore *p_core) { core = p_core; }
	void set_shell(EditorShell *p_shell) { shell = p_shell; }

	// ── Getters ───────────────────────────────────────────────────────────
	EditorPluginList *get_editor_plugins_force_input_forwarding() { return editor_plugins_force_input_forwarding; }
	EditorPluginList *get_editor_plugins_force_over() { return editor_plugins_force_over; }
	EditorPluginList *get_editor_plugins_over() { return editor_plugins_over; }
	Vector<EditorPlugin *> &get_editor_plugins() { return editor_plugins; }
	HashMap<ObjectID, HashSet<EditorPlugin *>> &get_active_plugins() { return active_plugins; }
	HashMap<String, EditorPlugin *> &get_addon_name_to_plugin() { return addon_name_to_plugin; }
	LocalVector<String> &get_pending_addons() { return pending_addons; }
	bool is_main_screen_editing_flag() const { return is_main_screen_editing; }
	void set_main_screen_editing(bool p_editing) { is_main_screen_editing = p_editing; }

	// ── Plugin lifecycle ──────────────────────────────────────────────────
	void init_plugins(); ///< Registers editor plugins and runs their initialization routines.
	void add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false); ///< Registers an `EditorPlugin` instance with the editor.
	void remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed = false); ///< Unregisters / deactivates an `EditorPlugin` instance.
	void add_extension_editor_plugin(const StringName &p_class_name); ///< Instantiates and registers an editor plugin class by name.
	void remove_extension_editor_plugin(const StringName &p_class_name); ///< Removes an extension-provided editor plugin by class name.

	/// Global callback for `GDExtensionEditorPlugins` to add an extension editor plugin.
	static void _gdextension_add_plugin(const StringName &p_class_name);
	/// Global callback for `GDExtensionEditorPlugins` to remove an extension editor plugin.
	static void _gdextension_remove_plugin(const StringName &p_class_name);
	void unload_editor_addons(); ///< Unloads (removes and frees) all addon plugins.
	void set_addon_plugin_enabled(const String &p_addon, bool p_enabled, bool p_config_changed = false); ///< Enables or disables an addon plugin.
	bool is_addon_plugin_enabled(const String &p_addon) const; ///< @return `true` if the named addon plugin is currently enabled.
	void _on_plugin_ready(Object *p_script, const String &p_activate_name); ///< Handles the plugin config dialog "ready" signal.

	// ── Resource-conversion plugins ───────────────────────────────────────
	void add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin); ///< Registers a plugin that can convert one resource type to another.
	void remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin); ///< Unregisters a resource-conversion plugin.
	Vector<Ref<EditorResourceConversionPlugin>> find_resource_conversion_plugin_for_resource(const Ref<Resource> &p_for_resource); ///< Finds conversion plugins capable of converting the given resource.
	Vector<Ref<EditorResourceConversionPlugin>> find_resource_conversion_plugin_for_type_name(const String &p_type); ///< Finds conversion plugins that target the named resource type.

	// ── Callbacks ─────────────────────────────────────────────────────────
	static void add_plugin_init_callback(EditorPluginInitializeCallback p_callback); ///< Registers a callback invoked when each plugin is initialized.
	static void add_init_callback(EditorNodeInitCallback p_callback) { _init_callbacks.push_back(p_callback); } ///< Registers a callback invoked once when the editor starts up.
	static void add_build_callback(EditorBuildCallback p_callback); ///< Registers a callback invoked to check whether the project "builds" before running.
	bool call_build(); ///< Runs all registered build callbacks; returns `false` if any of them fail.

	// ── Scene-session UI hooks (delegated from `EditorShell`) ─────────────
	/// Restores plugin-specific per-scene state from a scene's config data.
	void _load_editor_plugin_states_from_config(const Ref<ConfigFile> &p_config_file);
	/// Handles a plugin starting to edit an object (dispatched by the shell).
	void _plugin_over_edit(EditorPlugin *p_plugin, Object *p_object);
	/// Registers a plugin as the active screen plugin when it owns the main screen.
	void _plugin_over_self_own(EditorPlugin *p_plugin);

	// ── Shutdown ──────────────────────────────────────────────────────────
	static void cleanup(); ///< Clears registered init callbacks.

	static Vector<EditorNodeInitCallback> _init_callbacks;

	~EditorPluginManager() override;
};
