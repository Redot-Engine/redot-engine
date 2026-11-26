/**************************************************************************/
/*  editor_progress.h                                                     */
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
 * @file editor_progress.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "core/string/ustring.h"
#include "core/templates/hash_map.h"

class BackgroundProgress;
class ProgressDialog;

/// Tracks progress of long-running editor tasks. Owns the progress logic and
/// the shared "last progress" state, and can be driven either by the editor
/// shell's progress widgets (when an `EditorNode` exists) or by log output
/// (headless / command-line mode, when no shell has registered its UI).
/// This is a plain C++ singleton — not a Godot `Object`/`Node`.
class EditorProgressTracker {
	static EditorProgressTracker *singleton;

	/// Registered shell UI (owned by `EditorNode`); `nullptr` when headless.
	static ProgressDialog *progress_dialog;
	static BackgroundProgress *progress_hb;
	static bool cmdline_mode;

	/// Tracks the total steps for each active task (used for CLI progress output).
	static HashMap<String, int> progress_total_steps;
	static String last_progress_task;
	static String last_progress_state;
	static int last_progress_step;
	static double last_progress_time;

public:
	static EditorProgressTracker *get_singleton() {
		if (!singleton) {
			singleton = memnew(EditorProgressTracker);
		}
		return singleton;
	}

	/// Registers the shell's progress widgets. `EditorNode` calls this in its
	/// constructor after creating both widgets. Either may be `nullptr`.
	static void set_progress_ui(ProgressDialog *p_progress_dialog, BackgroundProgress *p_progress_hb) {
		progress_dialog = p_progress_dialog;
		progress_hb = p_progress_hb;
	}

	/// Sets whether progress is reported to the command-line log instead of the UI.
	static void set_cmdline_mode(bool p_cmdline_mode) {
		cmdline_mode = p_cmdline_mode;
	}

	static void progress_add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel = false);
	/// Advances a progress task to a given step with a new status text; @return `false` if the user cancelled.
	static bool progress_task_step(const String &p_task, const String &p_state, int p_step = -1, bool p_force_refresh = true);
	/// Closes/removes a progress task previously added with `progress_add_task`.
	static void progress_end_task(const String &p_task);

	/// Registers a low-key background progress task shown in the status bar rather than a blocking dialog.
	static void progress_add_task_bg(const String &p_task, const String &p_label, int p_steps);
	/// Advances a background progress task.
	static void progress_task_step_bg(const String &p_task, int p_step = -1);
	/// Ends a background progress task.
	static void progress_end_task_bg(const String &p_task);
};

/// RAII helper that drives a blocking progress dialog task (or a background
/// task when forced) while a scope is alive. Usages stay unchanged:
/// `EditorProgress ep(task, label, steps); ... ep.step(state, step);`
struct EditorProgress {
	String task;
	bool force_background = false;
	bool step(const String &p_state, int p_step = -1, bool p_force_refresh = true);

	EditorProgress(const String &p_task, const String &p_label, int p_amount, bool p_can_cancel = false, bool p_force_background = false);
	~EditorProgress();
};

/// RAII helper that drives a background progress task (status bar) while a
/// scope is alive: `EditorProgressBG bg(task, label, steps); ... bg.step();`
struct EditorProgressBG {
	String task;
	void step(int p_step = -1) { EditorProgressTracker::get_singleton()->progress_task_step_bg(task, p_step); }
	EditorProgressBG(const String &p_task, const String &p_label, int p_amount) {
		EditorProgressTracker::get_singleton()->progress_add_task_bg(p_task, p_label, p_amount);
		task = p_task;
	}
	~EditorProgressBG() { EditorProgressTracker::get_singleton()->progress_end_task_bg(task); }
};
