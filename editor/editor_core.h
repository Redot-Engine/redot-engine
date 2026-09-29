/**************************************************************************/
/*  editor_core.h                                                         */
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

// #include "core/object/print.h"
#include "core/object/script_language.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/pair.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"
#include "editor/editor_data.h"
#include "editor/editor_scene_manager.h"
#include "editor/settings/editor_folding.h"
#include "scene/main/node.h"

class ColorPicker;
class DependencyEditor;
class DependencyErrorDialog;
class EditorCommandPalette;
class EditorExport;
class EditorFileDialog;
class EditorFileSystem;
class EditorLog;
class EditorNativeShaderSourceVisualizer;
class EditorPluginManager;
class EditorQuickOpenDialog;
class EditorResourcePreview;
class EditorSelection;
class EditorSettingsDialog;
class EditorShell;
class FBXImporterManager;
class FileDialog;
class Node;
class Object;
class ProjectExportDialog;
class ProjectSettingsEditor;
// class Ref;
class Resource;
class Script;
class SubViewport;
class Theme;
class Texture2D;
class Translation;
class EditorSceneManager;

/**
 * Owns editor-wide services and state that must exist independently of a
 * concrete editor shell.
 *
 * `EditorCore` is intentionally a `Node` so owned service nodes participate in
 * the scene tree and receive normal lifecycle processing. Dialogs owned by the
 * core are initially unparented; a concrete shell is responsible for attaching
 * them to its UI root.
 */
class EditorCore : public Node {
	GDCLASS(EditorCore, Node);

	// friend class EditorNode;

	static EditorCore *singleton;
	EditorSceneManager *editor_scene_manager = nullptr;

	/// Owns the plugin registry, addon state, and resource-conversion plugins.
	/// Created in `_initialize_services()`, destroyed in the destructor.
	EditorPluginManager *editor_plugin_manager = nullptr;

	/// Pointer to the active editor shell. Set by the concrete shell
	/// (e.g. `EditorNode`) during construction; `nullptr` until then.
	EditorShell *shell = nullptr;

	/// Shared data about edited resources, scenes, selection undo history, and
	/// editor-specific resource metadata.
	EditorData editor_data;

	/// Tracks folding and other persistent editor presentation state.
	EditorFolding editor_folding;

	/// Tracks navigation between objects inspected by the editor.
	EditorSelectionHistory editor_history;

	/// Current editor selection. This is an `Object`, not a `Node`, and is
	/// manually deleted by `EditorCore`.
	EditorSelection *editor_selection = nullptr;

	/// Command palette. Lazily-created singleton owned by core, parented by the
	/// shell.
	EditorCommandPalette *command_palette = nullptr;

	/// Generates and caches resource previews used by editor UI and filesystem
	/// operations.
	EditorResourcePreview *resource_preview = nullptr;

	/// Project export service. This is a service node rather than part of a
	/// specific shell layout.
	EditorExport *editor_export = nullptr;

	/// Project-wide filesystem index and scanner.
	EditorFileSystem *editor_file_system = nullptr;

	/// Project export dialog. It remains unparented until a shell attaches it.
	ProjectExportDialog *project_export = nullptr;

	/// Project settings editor. Constructed with `editor_data` so filesystem
	/// autoload handling can use it before a shell finishes initializing.
	ProjectSettingsEditor *project_settings_editor = nullptr;

	/// Editor settings dialog. It remains unparented until a shell attaches it.
	EditorSettingsDialog *editor_settings_dialog = nullptr;

	/// Shader source visualizer. It remains unparented until a shell attaches it.
	EditorNativeShaderSourceVisualizer *native_shader_source_visualizer = nullptr;

	/// Generic quick-open dialog owned independently of the shell.
	EditorQuickOpenDialog *quick_open_dialog = nullptr;

	/// Quick-open color palette dialog owned independently of the shell.
	EditorQuickOpenDialog *quick_open_color_palette = nullptr;

#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	/// FBX importer configuration UI. It remains unparented until a shell
	/// attaches it.
	FBXImporterManager *fbx_importer_manager = nullptr;
#endif

	/// The editor's `Theme` resource. Rebuilt by the shell (`_update_theme`),
	/// read by core via `get_editor_theme()`.
	Ref<Theme> theme;

	/// Unix timestamp (seconds) captured when the editor becomes ready.
	uint64_t started_timestamp = 0;

	/// Cache mapping class/resource type names to their editor icons.
	HashMap<String, Ref<Texture2D>> icon_type_cache;
	/// Cache mapping (class, script) pairs to their resolved editor icons.
	HashMap<Pair<String, String>, Ref<Texture2D>> class_icon_cache;

	/// Broken dependencies recorded while loading resources, keyed by the
	/// resource path. Surfaced in a dialog by `load_resource`.
	HashMap<String, HashSet<String>> dependency_errors;

	/// The resource currently being saved via a "Save As" dialog (core state,
	/// read by the shell when the file dialog is confirmed).
	Ref<Resource> saving_resource;
	/// Resources currently in the middle of a `save_resource_in_path` call, so
	/// the global save callback does not double-process them.
	HashSet<Ref<Resource>> saving_resources_in_path;

	/// Translation resources tracked for the active preview locale.
	HashSet<Ref<Translation>> tracked_translations;
	/// `true` while a deferred translation-changed notification is pending.
	bool pending_translation_notification = false;
	/// `true` once setting overrides changed and the shell must notify.
	bool settings_overrides_changed = false;

	/// File dialogs registered for theme/display-mode tracking.
	HashSet<FileDialog *> file_dialogs;
	HashSet<EditorFileDialog *> editor_file_dialogs;

	/// The print handler installed on the engine for the editor's log.
	PrintHandlerList print_handler;

	/// File extensions treated as text files / opened externally by
	/// `load_resource`.
	HashSet<String> textfile_extensions;
	HashSet<String> other_file_extensions;

	/// Callback that opens the quick-open color palette when a color picker
	/// requests it; set by `setup_color_picker`, consumed by the shell.
	Callable palette_file_selected_callback;

	/// Applies editor-specific engine and resource-loading behavior that is not
	/// owned by a shell.
	void _initialize_engine();

	/// Constructs service nodes and unparented dialogs owned by the core.
	void _initialize_services();

	/// Registers global engine callbacks (print handler, resource save/load,
	/// file-close error, dependency-error) owned by the core.
	void _register_global_callbacks();

	/// Unregisters the global engine callbacks owned by the core.
	void _unregister_global_callbacks();

	/// Refreshes editor state after GDExtension reloads (inspector, script
	/// editor, documentation).
	void _gdextensions_reloaded();

	/// Resolves an icon for a class name, falling back through script icons and
	/// theme defaults.
	Ref<Texture2D> _get_class_or_script_icon(const String &p_class, const String &p_script_path, const String &p_fallback = "Object", bool p_fallback_script_to_theme = false);

	/// Records that an edited subresource (or its subresources) changed and
	/// saves it. @return `true` if the resource or a subresource changed.
	bool _find_and_save_resource(Ref<Resource> p_res, HashMap<Ref<Resource>, bool> &processed, int32_t flags);
	/// Finds modified subresources inside an edited object and saves them.
	bool _find_and_save_edited_subresources(Object *obj, HashMap<Ref<Resource>, bool> &processed, int32_t flags);
	/// Walks a scene saving any modified subresources found on its nodes.
	void _save_edited_subresources(Node *scene, HashMap<Ref<Resource>, bool> &processed, int32_t flags);

	/// Returns the icon a `FileDialog` should show for the given path.
	static Ref<Texture2D> _file_dialog_get_icon(const String &p_path);
	static void _file_dialog_register(FileDialog *p_dialog);
	static void _file_dialog_unregister(FileDialog *p_dialog);
	static void _editor_file_dialog_register(EditorFileDialog *p_dialog);
	static void _editor_file_dialog_unregister(EditorFileDialog *p_dialog);

	/// Routes file-access close errors into the shell's error surface (deferred
	/// to the main thread).
	static void _file_access_close_error_notify(const String &p_str);
	static void _file_access_close_error_notify_impl(const String &p_str);

	/// Global callback invoked when a resource load fails; routes the error to
	/// the shell's IO error surface (deferred to the main thread).
	static void _resource_load_error_notify(const String &p_error);
	static void _resource_load_error_notify_impl(const String &p_error);

	/// Global callback invoked when scene instantiation produces a warning;
	/// routes the warning to the shell and marks the current scene as unsaved
	/// (deferred to the main thread).
	static void _instantiation_warning_notify(const String &p_warning);
	static void _instantiation_warning_notify_impl(const String &p_warning);

	/// Engine print callback installed on the editor; forwards output to the log.
	static void _print_handler(void *p_this, const String &p_string, bool p_error, bool p_rich);
	/// Implementation that appends printed text to the editor's log.
	static void _print_handler_impl(const String &p_string, bool p_error, bool p_rich);

	/// Global callback invoked whenever a resource is saved.
	static void _resource_saved(Ref<Resource> p_resource, const String &p_path);
	/// Global callback invoked whenever a resource is loaded.
	static void _resource_loaded(Ref<Resource> p_resource, const String &p_path);

	/// Records a broken dependency for a file being loaded so it can be shown
	/// in a dialog.
	static void _dependency_error_report(const String &p_path, const String &p_dep, const String &p_type);

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	static EditorCore *get_singleton() { return singleton; }

	/// Returns the scene-session manager owned by the core.
	/// Valid after initialize() has run; guaranteed for the editor's lifetime.
	EditorSceneManager *get_scene_manager() const { return editor_scene_manager; }

	/// Sets the active editor shell. Called by the concrete shell during
	/// construction (e.g. `EditorNode`'s constructor). Also wires the shell's
	/// back-pointers to this core, the scene manager, and the plugin manager.
	void set_shell(EditorShell *p_shell);

	/// @returns The active editor shell, or `nullptr` if none has been set yet.
	EditorShell *get_shell() const { return shell; }

	/// @returns The editor plugin manager, or `nullptr` before initialization.
	EditorPluginManager *get_editor_plugin_manager() const { return editor_plugin_manager; }

	/// Constructs dialogs and other Node services that depend on the shell's
	/// theme/EditorNode singleton. Called from EditorNode's constructor.
	void initialize_dialogs();

	/// Removes the print handler to prevent EditorLog from receiving messages
	/// during shutdown. Call before destroying the EditorNode.

	EditorData &get_editor_data() { return editor_data; } ///< @return The editor's data store (open scenes, edited object, undo history, etc.).
	EditorFolding &get_editor_folding() { return editor_folding; } ///< @return The helper that saves/restores scene folding and collapse state.
	EditorSelectionHistory *get_editor_selection_history() { return &editor_history; }
	EditorSelection *get_editor_selection() { return editor_selection; }
	EditorResourcePreview *get_resource_preview() { return resource_preview; }
	EditorExport *get_editor_export() { return editor_export; }
	EditorFileSystem *get_editor_file_system() { return editor_file_system; }
	EditorLog *get_log() { return log; }
	EditorCommandPalette *get_command_palette() { return command_palette; }

	/// @returns the editor's `Theme` resource.
	Ref<Theme> get_editor_theme() const { return theme; }
	/// Sets the editor's `Theme` resource (the shell rebuilds it via its
	/// `_update_theme` and writes the result back here).
	void set_editor_theme(const Ref<Theme> &p_theme) { theme = p_theme; }
	/// @returns the callback that opens the quick-open color palette.
	Callable get_palette_file_selected_callback() const { return palette_file_selected_callback; }
	/// @returns the file dialogs registered for theme/display-mode tracking.
	const HashSet<FileDialog *> &get_file_dialogs() const { return file_dialogs; }
	/// @returns the editor file dialogs registered for theme/display-mode tracking.
	const HashSet<EditorFileDialog *> &get_editor_file_dialogs() const { return editor_file_dialogs; }

	// ── Resource operations ─────────────────────────────────────────────────

	/// Loads a resource file (e.g. a script or material) into the editor.
	Error load_resource(const String &p_resource, bool p_ignore_broken_deps = false);
	/// Opens a file whether it's a scene or a plain resource.
	Error load_scene_or_resource(const String &p_file, bool p_ignore_broken_deps = false, bool p_change_scene_tab_if_already_open = true);
	/// Saves a resource into the given path in the project.
	void save_resource_in_path(const Ref<Resource> &p_resource, const String &p_path);
	/// Saves a resource to its existing path (or prompts if it has none).
	void save_resource(const Ref<Resource> &p_resource);
	/// Saves a resource under a new path ("Save As"), optionally suggesting a location.
	void save_resource_as(const Ref<Resource> &p_resource, const String &p_at_path = String());
	/// Saves the default environment settings of the edited scene.
	void save_default_environment();
	/// @returns `true` if the resource cannot be written to (built-in, foreign, etc.).
	bool is_resource_read_only(Ref<Resource> p_resource, bool p_foreign_resources_are_writable = false);
	/// Re-reads the text-file / other file extension lists from editor settings.
	/// @returns `true` if either set changed (the filesystem should rescan).
	bool update_file_extension_caches();
	/// Replaces all uses of the source resources with the target resources inside the given object.
	void replace_resources_in_object(Object *p_object, const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource);
	/// Replaces all uses of the source resources with the target resources across all open scenes.
	void replace_resources_in_scenes(const Vector<Ref<Resource>> &p_source_resources, const Vector<Ref<Resource>> &p_target_resource);
	/// Opens the dependency fixer dialog for a file with broken/missing dependencies.
	void fix_dependencies(const String &p_for_file);
	/// Runs an editor script (a Script with a _run method) inside the editor.
	void run_editor_script(const Ref<Script> &p_script);

	// ── Object type / icon queries ──────────────────────────────────────────

	/// @returns The base script of an object's custom (script-defined) type.
	Ref<Script> get_object_custom_type_base(const Object *p_object) const;
	/// @returns The display name of an object's custom (script-defined) type.
	StringName get_object_custom_type_name(const Object *p_object) const;
	/// @returns `true` if the object's type matches the given (possibly custom) class.
	bool is_object_of_custom_type(const Object *p_object, const StringName &p_class);
	/// @returns The icon for an object (custom script icon, class icon, or fallback).
	Ref<Texture2D> get_object_icon(const Object *p_object, const String &p_fallback = "Object");
	/// @return The editor icon for a class name, with a fallback if none exists.
	Ref<Texture2D> get_class_icon(const String &p_class, const String &p_fallback = "");
	/// Resolves an icon suitable for native OS menus (theme-aware, dark/light).
	Ref<Texture2D> _get_editor_theme_native_menu_icon(const StringName &p_name, bool p_global_menu, bool p_dark_mode) const;

	// ── Translation / preview locale ────────────────────────────────────────

	/// @returns The locale used for previewing translations in the editor.
	String get_preview_locale() const;
	/// Sets the locale used to preview translations in the running editor.
	void set_preview_locale(const String &p_locale);
	/// Updates the theme of editor preview windows to the given mode.
	void update_preview_themes(int p_mode);
	/// Notifies the editor that setting overrides changed so UI can refresh.
	void notify_settings_overrides_changed();

	// ── Naming / misc ───────────────────────────────────────────────────────

	enum SceneNameCasing {
		SCENE_NAME_CASING_AUTO,
		SCENE_NAME_CASING_PASCAL_CASE,
		SCENE_NAME_CASING_SNAKE_CASE,
		SCENE_NAME_CASING_KEBAB_CASE,
		SCENE_NAME_CASING_CAMEL_CASE,
	};

	/// Adjusts a suggested scene file name to match the project's naming convention.
	static String adjust_scene_name_casing(const String &p_root_name);
	/// Adjusts a suggested script file name to match the configured casing style.
	static String adjust_script_name_casing(const String &p_file_name, ScriptLanguage::ScriptNameCasing p_auto_casing);
	/// Given full file paths, produces file names that are unique enough to tell apart.
	static void disambiguate_filenames(const Vector<String> p_full_paths, Vector<String> &r_filenames);
	/// @returns `true` if the editor is allowed to open floating/sub windows.
	bool is_multi_window_enabled() const;
	/// @returns Tooltip text explaining whether multi-window support is available.
	String get_multiwindow_support_tooltip_text() const;
	/// Applies editor-standard configuration to a ColorPicker control.
	void setup_color_picker(ColorPicker *p_picker);
	/// Opens the quick-open color palette dialog (ColorPicker quick-open callback).
	void _quick_open_color_palette();
	/// @returns the Unix timestamp captured when the editor became ready.
	uint64_t get_started_timestamp() const { return started_timestamp; }
	/// @returns the cache mapping (class, script) pairs to resolved editor icons.
	HashMap<Pair<String, String>, Ref<Texture2D>> &get_class_icon_cache() { return class_icon_cache; }
	/// @returns broken dependencies recorded while loading resources.
	HashMap<String, HashSet<String>> &get_dependency_errors() { return dependency_errors; }
	/// @returns the resource currently being saved via a "Save As" dialog.
	Ref<Resource> get_saving_resource() const { return saving_resource; }
	/// Sets the resource currently being saved via a "Save As" dialog.
	void set_saving_resource(const Ref<Resource> &p_resource) { saving_resource = p_resource; }

	void remove_print_handler_early();
	/// @returns `true` and clears the flag once setting overrides changed.
	bool take_settings_overrides_changed() {
		bool changed = settings_overrides_changed;
		settings_overrides_changed = false;
		return changed;
	}

	ProjectExportDialog *get_project_export() const { return project_export; }

	ProjectSettingsEditor *get_project_settings_editor() const { return project_settings_editor; }
	EditorSettingsDialog *get_editor_settings_dialog() const { return editor_settings_dialog; }
	EditorNativeShaderSourceVisualizer *get_native_shader_source_visualizer() const { return native_shader_source_visualizer; }
	EditorQuickOpenDialog *get_quick_open_dialog() const { return quick_open_dialog; }
	EditorQuickOpenDialog *get_quick_open_color_palette() const { return quick_open_color_palette; }

	/// Unparents the quick-open dialogs if a shell has parented them, so the
	/// shell's subtree teardown does not free them before `~EditorCore` does.
	void unparent_quick_open_dialogs();
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	FBXImporterManager *get_fbx_importer_manager() const { return fbx_importer_manager; }
#endif

	/**
	 * Initializes engine behavior and constructs core-owned services.
	 *
	 * Call exactly once after adding the core to the scene root and before
	 * constructing any editor shell.
	 */
	void initialize();

	/// Editor log (output panel). Owned by core, parented by the shell
	/// (bottom-panel item in the default shell).
	EditorLog *log = nullptr;

	/// Reloads editor translation resources and applies the current locale.
	void _update_translations();
	/// Re-subscribes to the translation resources for the active preview locale.
	void _translation_resources_changed();
	/// Queues a deferred "translations changed" notification to the scene root.
	void _queue_translation_notification();
	/// Delivers the queued translation notification through the scene root.
	void _propagate_translation_notification();

	/// Populates the cache mapping class/resource type names to their editor icons.
	void _build_icon_type_cache();

	/// @returns A formatted string of OS/hardware info (for Help > Copy
	/// System Info).
	String _get_system_info() const;

	EditorCore();
	~EditorCore() override;
};
