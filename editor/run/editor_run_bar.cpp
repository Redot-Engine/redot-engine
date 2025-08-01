/**************************************************************************/
/*  editor_run_bar.cpp                                                    */
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

#include "editor_run_bar.h"

#include "core/config/project_settings.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_toaster.h"
#include "editor/run/editor_run_native.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel_container.h"

#ifndef XR_DISABLED
#include "servers/xr_server.h"
#endif // XR_DISABLED

EditorRunBar *EditorRunBar::singleton = nullptr;

void EditorRunBar::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_POSTINITIALIZE: {
			_reset_play_buttons();
		} break;

		case NOTIFICATION_READY: {
			if (Engine::get_singleton()->is_recovery_mode_hint()) {
				recovery_mode_show_dialog();
			}
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			if (Engine::get_singleton()->is_recovery_mode_hint()) {
				main_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadRecoveryMode"), EditorStringName(EditorStyles)));
				recovery_mode_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("RecoveryModeButton"), EditorStringName(EditorStyles)));
				recovery_mode_button->add_theme_style_override("hover", get_theme_stylebox(SNAME("RecoveryModeButton"), EditorStringName(EditorStyles)));

				recovery_mode_button->set_button_icon(get_editor_theme_icon(SNAME("NodeWarning")));
				recovery_mode_reload_button->set_button_icon(get_editor_theme_icon(SNAME("Reload")));

				recovery_mode_button->begin_bulk_theme_override();
				recovery_mode_button->add_theme_color_override("icon_normal_color", Color(0.3, 0.3, 0.3, 1));
				recovery_mode_button->add_theme_color_override("icon_pressed_color", Color(0.4, 0.4, 0.4, 1));
				recovery_mode_button->add_theme_color_override("icon_hover_color", Color(0.6, 0.6, 0.6, 1));
				Color dark_color = get_theme_color("recovery_mode_text_color", EditorStringName(Editor));
				recovery_mode_button->add_theme_color_override(SceneStringName(font_color), dark_color);
				recovery_mode_button->add_theme_color_override("font_pressed_color", dark_color.lightened(0.2));
				recovery_mode_button->add_theme_color_override("font_hover_color", dark_color.lightened(0.4));
				recovery_mode_button->add_theme_color_override("font_hover_pressed_color", dark_color.lightened(0.2));
				recovery_mode_button->end_bulk_theme_override();

				return;
			}

			_update_play_buttons();
			profiler_autostart_indicator->set_button_icon(get_editor_theme_icon(SNAME("ProfilerAutostartWarning")));
			pause_button->set_button_icon(get_editor_theme_icon(SNAME("Pause")));
			stop_button->set_button_icon(get_editor_theme_icon(SNAME("Stop")));

			if (is_movie_maker_enabled()) {
				main_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadMovieMode"), EditorStringName(EditorStyles)));
				write_movie_button->set_theme_type_variation("RunBarButtonMovieMakerEnabled");

				write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonPressed"), EditorStringName(EditorStyles)));
			} else {
				main_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadNormal"), EditorStringName(EditorStyles)));
				write_movie_button->set_theme_type_variation("RunBarButtonMovieMakerDisabled");

				write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonNormal"), EditorStringName(EditorStyles)));
			}

			write_movie_button->set_button_icon(get_editor_theme_icon(SNAME("MainMovieWrite")));

		} break;
	}
}

void EditorRunBar::_reset_play_buttons() {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return;
	}

	play_button->set_pressed(false);
	play_button->set_button_icon(get_editor_theme_icon(SNAME("MainPlay")));
	play_button->set_tooltip_text(TTRC("Play the project."));

	play_scene_button->set_pressed(false);
	play_scene_button->set_button_icon(get_editor_theme_icon(SNAME("PlayScene")));
	play_scene_button->set_tooltip_text(TTRC("Play the edited scene."));

	play_custom_scene_button->set_pressed(false);
	play_custom_scene_button->set_button_icon(get_editor_theme_icon(SNAME("PlayCustom")));
	play_custom_scene_button->set_tooltip_text(TTRC("Play a custom scene."));
}

void EditorRunBar::_update_play_buttons() {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return;
	}

	_reset_play_buttons();
	if (!is_playing()) {
		return;
	}

	Button *active_button = nullptr;
	if (current_mode == RUN_CURRENT) {
		active_button = play_scene_button;
	} else if (current_mode == RUN_CUSTOM) {
		active_button = play_custom_scene_button;
	} else {
		active_button = play_button;
	}

	if (active_button) {
		active_button->set_pressed(true);
		active_button->set_button_icon(get_editor_theme_icon(SNAME("Reload")));
		active_button->set_tooltip_text(TTRC("Reload the played scene."));
	}
}

void EditorRunBar::_movie_maker_item_pressed(int p_id) {
	switch (p_id) {
		case MOVIE_MAKER_TOGGLE: {
			bool new_enabled = !is_movie_maker_enabled();
			set_movie_maker_enabled(new_enabled);
			write_movie_button->get_popup()->set_item_checked(0, new_enabled);
			write_movie_button->set_pressed(new_enabled);
			_write_movie_toggled(new_enabled);
			break;
		}
		case MOVIE_MAKER_OPEN_SETTINGS:
			ProjectSettingsEditor::get_singleton()->popup_project_settings(true);
			ProjectSettingsEditor::get_singleton()->set_general_page("editor/movie_writer");
			break;
	}
}

void EditorRunBar::_write_movie_toggled(bool p_enabled) {
	if (p_enabled) {
		add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadMovieMode"), EditorStringName(EditorStyles)));
		write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonPressed"), EditorStringName(EditorStyles)));
	} else {
		add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("LaunchPadNormal"), EditorStringName(EditorStyles)));
		write_movie_panel->add_theme_style_override(SceneStringName(panel), get_theme_stylebox(SNAME("MovieWriterButtonNormal"), EditorStringName(EditorStyles)));
	}
}

Vector<String> EditorRunBar::_get_xr_mode_play_args(int p_xr_mode_id) {
	Vector<String> play_args;
	if (p_xr_mode_id == 0) {
		// Play in regular mode, xr mode off.
		play_args.push_back("--xr-mode");
		play_args.push_back("off");
	} else if (p_xr_mode_id == 1) {
		// Play in xr mode.
		play_args.push_back("--xr-mode");
		play_args.push_back("on");
	}
	return play_args;
}

void EditorRunBar::_quick_run_selected(const String &p_file_path, int p_id) {
	play_custom_scene(p_file_path, _get_xr_mode_play_args(p_id));
}

void EditorRunBar::_play_custom_pressed(int p_id) {
	if (editor_run.get_status() == EditorRun::STATUS_STOP || current_mode != RunMode::RUN_CUSTOM) {
		stop_playing();

		EditorNode::get_singleton()->get_quick_open_dialog()->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorRunBar::_quick_run_selected).bind(p_id));
		play_custom_scene_button->set_pressed(false);
	} else {
		Vector<String> play_args = _get_xr_mode_play_args(p_id);

		// Reload if already running a custom scene.
		String last_custom_scene = run_custom_filename; // This is necessary to have a copy of the string.
		play_custom_scene(last_custom_scene, play_args);
	}
}

void EditorRunBar::_play_current_pressed(int p_id) {
	Vector<String> play_args = _get_xr_mode_play_args(p_id);

	if (editor_run.get_status() == EditorRun::STATUS_STOP || current_mode != RunMode::RUN_CURRENT) {
		play_current_scene(false, play_args);
	} else {
		// Reload if already running the current scene.
		play_current_scene(true, play_args);
	}
}

void EditorRunBar::_run_scene(const String &p_scene_path, const Vector<String> &p_run_args) {
	ERR_FAIL_COND_MSG(current_mode == RUN_CUSTOM && p_scene_path.is_empty(), "Attempting to run a custom scene with an empty path.");

	if (editor_run.get_status() == EditorRun::STATUS_PLAY) {
		return;
	}

	if (!EditorNode::get_singleton()->validate_custom_directory()) {
		return;
	}

	_reset_play_buttons();

	String write_movie_file;
	if (is_movie_maker_enabled()) {
		if (current_mode == RUN_CURRENT) {
			Node *scene_root = nullptr;
			if (p_scene_path.is_empty()) {
				scene_root = get_tree()->get_edited_scene_root();
			} else {
				int scene_index = EditorNode::get_editor_data().get_edited_scene_from_path(p_scene_path);
				if (scene_index >= 0) {
					scene_root = EditorNode::get_editor_data().get_edited_scene_root(scene_index);
				}
			}

			if (scene_root && scene_root->has_meta("movie_file")) {
				// If the scene file has a movie_file metadata set, use this as file.
				// Quick workaround if you want to have multiple scenes that write to
				// multiple movies.
				write_movie_file = scene_root->get_meta("movie_file");
			}
		}

		if (write_movie_file.is_empty()) {
			write_movie_file = GLOBAL_GET("editor/movie_writer/movie_file");
		}

		if (write_movie_file.is_empty()) {
			// TODO: Provide options to directly resolve the issue with a custom dialog.
			EditorNode::get_singleton()->show_accept(TTR("Movie Maker mode is enabled, but no movie file path has been specified.\nA default movie file path can be specified in the project settings under the Editor > Movie Writer category.\nAlternatively, for running single scenes, a `movie_file` string metadata can be added to the root node,\nspecifying the path to a movie file that will be used when recording that scene."), TTR("OK"));
			return;
		}
	}

	String run_filename;
	switch (current_mode) {
		case RUN_CUSTOM: {
			run_filename = ResourceUID::ensure_path(p_scene_path);
			run_custom_filename = run_filename;
		} break;

		case RUN_CURRENT: {
			if (!p_scene_path.is_empty()) {
				run_filename = p_scene_path;
				run_current_filename = run_filename;
				break;
			}

			Node *scene_root = get_tree()->get_edited_scene_root();
			if (!scene_root) {
				EditorNode::get_singleton()->show_accept(TTR("There is no defined scene to run."), TTR("OK"));
				return;
			}

			if (scene_root->get_scene_file_path().is_empty()) {
				EditorNode::get_singleton()->save_before_run();
				return;
			}

			run_filename = scene_root->get_scene_file_path();
			run_current_filename = run_filename;
		} break;

		default: {
			if (!EditorNode::get_singleton()->ensure_main_scene(false)) {
				return;
			}

			run_filename = GLOBAL_GET("application/run/main_scene");
		} break;
	}

	EditorNode::get_singleton()->try_autosave();
	if (!EditorNode::get_singleton()->call_build()) {
		return;
	}

	EditorDebuggerNode::get_singleton()->start();
	Error error = editor_run.run(run_filename, write_movie_file, p_run_args);
	if (error != OK) {
		EditorDebuggerNode::get_singleton()->stop();
		EditorNode::get_singleton()->show_accept(TTR("Could not start subprocess(es)!"), TTR("OK"));
		return;
	}

	_update_play_buttons();
	stop_button->set_disabled(false);

	emit_signal(SNAME("play_pressed"));
}

void EditorRunBar::_run_native(const Ref<EditorExportPreset> &p_preset) {
	EditorNode::get_singleton()->try_autosave();

	if (run_native->is_deploy_debug_remote_enabled()) {
		stop_playing();

		if (!EditorNode::get_singleton()->call_build()) {
			return; // Build failed.
		}

		EditorDebuggerNode::get_singleton()->start(p_preset->get_platform()->get_debug_protocol());
		emit_signal(SNAME("play_pressed"));
		editor_run.run_native_notify();
	}
}

void EditorRunBar::_profiler_autostart_indicator_pressed() {
	// Switch to the first profiler tab in the bottom panel.
	EditorNode::get_singleton()->get_bottom_panel()->make_item_visible(EditorDebuggerNode::get_singleton(), true);

	if (EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_profiler", false)) {
		EditorDebuggerNode::get_singleton()->get_current_debugger()->switch_to_debugger(3);
	} else if (EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_visual_profiler", false)) {
		EditorDebuggerNode::get_singleton()->get_current_debugger()->switch_to_debugger(4);
	} else {
		// Switch to the network profiler tab.
		EditorDebuggerNode::get_singleton()->get_current_debugger()->switch_to_debugger(8);
	}
}

void EditorRunBar::recovery_mode_show_dialog() {
	recovery_mode_popup->popup_centered();
}

void EditorRunBar::recovery_mode_reload_project() {
	EditorNode::get_singleton()->trigger_menu_option(EditorNode::PROJECT_RELOAD_CURRENT_PROJECT, false);
}

void EditorRunBar::play_main_scene(bool p_from_native) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		EditorToaster::get_singleton()->popup_str(TTR("Recovery Mode is enabled. Disable it to run the project."), EditorToaster::SEVERITY_WARNING);
		return;
	}

	if (p_from_native) {
		run_native->resume_run_native();
	} else {
		stop_playing();

		current_mode = RunMode::RUN_MAIN;
		_run_scene();
	}
}

void EditorRunBar::play_current_scene(bool p_reload, const Vector<String> &p_play_args) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		EditorToaster::get_singleton()->popup_str(TTR("Recovery Mode is enabled. Disable it to run the project."), EditorToaster::SEVERITY_WARNING);
		return;
	}

	String last_current_scene = run_current_filename; // This is necessary to have a copy of the string.

	EditorNode::get_singleton()->save_default_environment();
	stop_playing();

	current_mode = RunMode::RUN_CURRENT;
	if (p_reload) {
		_run_scene(last_current_scene, p_play_args);
	} else {
		_run_scene("", p_play_args);
	}
}

void EditorRunBar::play_custom_scene(const String &p_custom, const Vector<String> &p_play_args) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		EditorToaster::get_singleton()->popup_str(TTR("Recovery Mode is enabled. Disable it to run the project."), EditorToaster::SEVERITY_WARNING);
		return;
	}

	stop_playing();

	current_mode = RunMode::RUN_CUSTOM;
	_run_scene(p_custom, p_play_args);
}

void EditorRunBar::stop_playing() {
	if (editor_run.get_status() == EditorRun::STATUS_STOP) {
		return;
	}

	current_mode = RunMode::STOPPED;
	editor_run.stop();
	EditorDebuggerNode::get_singleton()->stop();

	run_custom_filename.clear();
	run_current_filename.clear();
	stop_button->set_pressed(false);
	stop_button->set_disabled(true);
	_reset_play_buttons();

	emit_signal(SNAME("stop_pressed"));
}

bool EditorRunBar::is_playing() const {
	EditorRun::Status status = editor_run.get_status();
	return (status == EditorRun::STATUS_PLAY || status == EditorRun::STATUS_PAUSED);
}

String EditorRunBar::get_playing_scene() const {
	String run_filename = editor_run.get_running_scene();
	if (run_filename.is_empty() && is_playing()) {
		run_filename = GLOBAL_GET("application/run/main_scene"); // Must be the main scene then.
	}

	return run_filename;
}

Error EditorRunBar::start_native_device(int p_device_id) {
	return run_native->start_run_native(p_device_id);
}

OS::ProcessID EditorRunBar::has_child_process(OS::ProcessID p_pid) const {
	return editor_run.has_child_process(p_pid);
}

void EditorRunBar::stop_child_process(OS::ProcessID p_pid) {
	if (!has_child_process(p_pid)) {
		return;
	}

	editor_run.stop_child_process(p_pid);
	if (!editor_run.get_child_process_count()) { // All children stopped. Closing.
		stop_playing();
	}
}

OS::ProcessID EditorRunBar::get_current_process() const {
	return editor_run.get_current_process();
}

void EditorRunBar::set_movie_maker_enabled(bool p_enabled) {
	movie_maker_enabled = p_enabled;
	write_movie_button->get_popup()->set_item_checked(0, p_enabled);
}

bool EditorRunBar::is_movie_maker_enabled() const {
	return movie_maker_enabled;
}

void EditorRunBar::update_profiler_autostart_indicator() {
	bool profiler_active = EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_profiler", false);
	bool visual_profiler_active = EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_visual_profiler", false);
	bool network_profiler_active = EditorSettings::get_singleton()->get_project_metadata("debug_options", "autostart_network_profiler", false);
	bool any_profiler_active = profiler_active | visual_profiler_active | network_profiler_active;
	any_profiler_active &= !Engine::get_singleton()->is_recovery_mode_hint();
	profiler_autostart_indicator->set_visible(any_profiler_active);
	if (any_profiler_active) {
		String tooltip = TTR("Autostart is enabled for the following profilers, which can have a performance impact:");
		if (profiler_active) {
			tooltip += "\n- " + TTR("Profiler");
		}
		if (visual_profiler_active) {
			tooltip += "\n- " + TTR("Visual Profiler");
		}
		if (network_profiler_active) {
			tooltip += "\n- " + TTR("Network Profiler");
		}
		tooltip += "\n\n" + TTR("Click to open the first profiler for which autostart is enabled.");
		profiler_autostart_indicator->set_tooltip_text(tooltip);
	}
}

HBoxContainer *EditorRunBar::get_buttons_container() {
	return main_hbox;
}

void EditorRunBar::_bind_methods() {
	ADD_SIGNAL(MethodInfo("play_pressed"));
	ADD_SIGNAL(MethodInfo("stop_pressed"));
}

EditorRunBar::EditorRunBar() {
	singleton = this;

	outer_hbox = memnew(HBoxContainer);
	add_child(outer_hbox);

	// Use a button for the indicator since it comes with a background panel and pixel perfect centering of an icon.
	profiler_autostart_indicator = memnew(Button);
	profiler_autostart_indicator->set_tooltip_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	profiler_autostart_indicator->set_icon_alignment(HORIZONTAL_ALIGNMENT_CENTER);
	profiler_autostart_indicator->set_focus_mode(FOCUS_ACCESSIBILITY);
	profiler_autostart_indicator->set_theme_type_variation("ProfilerAutostartIndicator");
	profiler_autostart_indicator->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_profiler_autostart_indicator_pressed));
	outer_hbox->add_child(profiler_autostart_indicator);
	update_profiler_autostart_indicator();

	main_panel = memnew(PanelContainer);
	outer_hbox->add_child(main_panel);

	main_hbox = memnew(HBoxContainer);
	main_panel->add_child(main_hbox);

	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		recovery_mode_popup = memnew(AcceptDialog);
		recovery_mode_popup->set_min_size(Size2(550, 70) * EDSCALE);
		recovery_mode_popup->set_title(TTR("Recovery Mode"));
		recovery_mode_popup->set_text(
				TTR("Godot opened the project in Recovery Mode, which is a special mode that can help recover projects that crash the engine upon initialization. The following features have been temporarily disabled:") +
				String::utf8("\n\n•  ") + TTR("Tool scripts") +
				String::utf8("\n•  ") + TTR("Editor plugins") +
				String::utf8("\n•  ") + TTR("GDExtension addons") +
				String::utf8("\n•  ") + TTR("Automatic scene restoring") +
				String::utf8("\n\n") + TTR("If the project cannot be opened outside of this mode, then it's very likely any of these components is preventing this project from launching. This mode is intended only for basic editing to troubleshoot such issues, and therefore it is not possible to run a project in this mode.") +
				String::utf8("\n\n") + TTR("To disable Recovery Mode, reload the project by pressing the Reload button next to the Recovery Mode banner, or by reopening the project normally."));
		recovery_mode_popup->set_autowrap(true);
		add_child(recovery_mode_popup);

		recovery_mode_reload_button = memnew(Button);
		main_hbox->add_child(recovery_mode_reload_button);
		recovery_mode_reload_button->set_theme_type_variation("RunBarButton");
		recovery_mode_reload_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		recovery_mode_reload_button->set_tooltip_text(TTR("Disable recovery mode and reload the project."));
		recovery_mode_reload_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::recovery_mode_reload_project));

		recovery_mode_panel = memnew(PanelContainer);
		main_hbox->add_child(recovery_mode_panel);

		recovery_mode_button = memnew(Button);
		recovery_mode_panel->add_child(recovery_mode_button);
		recovery_mode_button->set_theme_type_variation("RunBarButton");
		recovery_mode_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		recovery_mode_button->set_text(TTR("Recovery Mode"));
		recovery_mode_button->set_tooltip_text(TTR("Recovery Mode is enabled. Click for more details."));
		recovery_mode_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::recovery_mode_show_dialog));

		return;
	}

	play_button = memnew(Button);
	main_hbox->add_child(play_button);
	play_button->set_theme_type_variation("RunBarButton");
	play_button->set_toggle_mode(true);
	play_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	play_button->set_tooltip_text(TTRC("Run the project's default scene."));
	play_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::play_main_scene).bind(false));

	ED_SHORTCUT_AND_COMMAND("editor/run_project", TTRC("Run Project"), Key::F5);
	ED_SHORTCUT_OVERRIDE("editor/run_project", "macos", KeyModifierMask::META | Key::B);
	play_button->set_shortcut(ED_GET_SHORTCUT("editor/run_project"));

	pause_button = memnew(Button);
	main_hbox->add_child(pause_button);
	pause_button->set_theme_type_variation("RunBarButton");
	pause_button->set_toggle_mode(true);
	pause_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	pause_button->set_tooltip_text(TTRC("Pause the running project's execution for debugging."));
	pause_button->set_disabled(true);

	ED_SHORTCUT("editor/pause_running_project", TTRC("Pause Running Project"), Key::F7);
	ED_SHORTCUT_OVERRIDE("editor/pause_running_project", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::Y);
	pause_button->set_shortcut(ED_GET_SHORTCUT("editor/pause_running_project"));

	stop_button = memnew(Button);
	main_hbox->add_child(stop_button);
	stop_button->set_theme_type_variation("RunBarButton");
	stop_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	stop_button->set_tooltip_text(TTRC("Stop the currently running project."));
	stop_button->set_disabled(true);
	stop_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::stop_playing));

	ED_SHORTCUT("editor/stop_running_project", TTRC("Stop Running Project"), Key::F8);
	ED_SHORTCUT_OVERRIDE("editor/stop_running_project", "macos", KeyModifierMask::META | Key::PERIOD);
	stop_button->set_shortcut(ED_GET_SHORTCUT("editor/stop_running_project"));

	run_native = memnew(EditorRunNative);
	main_hbox->add_child(run_native);
	run_native->connect("native_run", callable_mp(this, &EditorRunBar::_run_native));

	bool add_play_xr_mode_options = false;
#ifndef XR_DISABLED
	if (XRServer::get_xr_mode() == XRServer::XRMODE_ON ||
			(XRServer::get_xr_mode() == XRServer::XRMODE_DEFAULT && GLOBAL_GET("xr/openxr/enabled"))) {
		// If OpenXR is enabled, we turn the `play_scene_button` and
		// `play_custom_scene_button` into MenuButtons to provide the option to start a scene in
		// either regular mode or XR mode.
		add_play_xr_mode_options = true;
	}
#endif // XR_DISABLED

	if (add_play_xr_mode_options) {
		MenuButton *menu_button = memnew(MenuButton);
		PopupMenu *popup = menu_button->get_popup();
		popup->add_item(TTRC("Run Scene in Regular Mode"), 0);
		popup->add_item(TTRC("Run Scene in XR Mode"), 1);
		popup->connect(SceneStringName(id_pressed), callable_mp(this, &EditorRunBar::_play_current_pressed));
		play_scene_button = menu_button;
	} else {
		play_scene_button = memnew(Button);
		play_scene_button->set_toggle_mode(true);
		play_scene_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_play_current_pressed).bind(-1));
	}
	main_hbox->add_child(play_scene_button);
	play_scene_button->set_theme_type_variation("RunBarButton");
	play_scene_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	play_scene_button->set_tooltip_text(TTRC("Run the currently edited scene."));

	ED_SHORTCUT_AND_COMMAND("editor/run_current_scene", TTRC("Run Current Scene"), Key::F6);
	ED_SHORTCUT_OVERRIDE("editor/run_current_scene", "macos", KeyModifierMask::META | Key::R);
	play_scene_button->set_shortcut(ED_GET_SHORTCUT("editor/run_current_scene"));

	if (add_play_xr_mode_options) {
		MenuButton *menu_button = memnew(MenuButton);
		PopupMenu *popup = menu_button->get_popup();
		popup->add_item(TTRC("Run in Regular Mode"), 0);
		popup->add_item(TTRC("Run in XR Mode"), 1);
		popup->connect(SceneStringName(id_pressed), callable_mp(this, &EditorRunBar::_play_custom_pressed));
		play_custom_scene_button = menu_button;
	} else {
		play_custom_scene_button = memnew(Button);
		play_custom_scene_button->set_toggle_mode(true);
		play_custom_scene_button->connect(SceneStringName(pressed), callable_mp(this, &EditorRunBar::_play_custom_pressed).bind(-1));
	}
	main_hbox->add_child(play_custom_scene_button);
	play_custom_scene_button->set_theme_type_variation("RunBarButton");
	play_custom_scene_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	play_custom_scene_button->set_tooltip_text(TTRC("Run a specific scene."));

	ED_SHORTCUT_AND_COMMAND("editor/run_specific_scene", TTRC("Run Specific Scene"), KeyModifierMask::CTRL | KeyModifierMask::SHIFT | Key::F5);
	ED_SHORTCUT_OVERRIDE("editor/run_specific_scene", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::R);
	play_custom_scene_button->set_shortcut(ED_GET_SHORTCUT("editor/run_specific_scene"));

	write_movie_panel = memnew(PanelContainer);
	main_hbox->add_child(write_movie_panel);

	write_movie_button = memnew(MenuButton);
	PopupMenu *write_movie_popup = write_movie_button->get_popup();
	write_movie_popup->add_check_item(TTRC("Enable Movie Maker Mode"), MOVIE_MAKER_TOGGLE);
	write_movie_popup->add_item(TTRC("Open Movie Maker Settings..."), MOVIE_MAKER_OPEN_SETTINGS);
	write_movie_popup->connect(SceneStringName(id_pressed), callable_mp(this, &EditorRunBar::_movie_maker_item_pressed));

	write_movie_panel->add_child(write_movie_button);
	write_movie_button->set_theme_type_variation("RunBarButtonMovieMakerDisabled");
	write_movie_button->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	write_movie_button->set_tooltip_text(TTRC("Enable Movie Maker mode.\nThe project will run at stable FPS and the visual and audio output will be recorded to a video file."));
	write_movie_button->set_accessibility_name(TTRC("Enable Movie Maker Mode"));
}
