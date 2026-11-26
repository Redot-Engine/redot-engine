/**************************************************************************/
/*  editor_progress.cpp                                                   */
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

#include "editor/editor_progress.h"

#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/string/translation_server.h"
#include "editor/gui/progress_dialog.h"

EditorProgressTracker *EditorProgressTracker::singleton = nullptr;

// Static state backing the tracker; defined once here.
ProgressDialog *EditorProgressTracker::progress_dialog = nullptr;
BackgroundProgress *EditorProgressTracker::progress_hb = nullptr;
bool EditorProgressTracker::cmdline_mode = false;
HashMap<String, int> EditorProgressTracker::progress_total_steps;
String EditorProgressTracker::last_progress_task;
String EditorProgressTracker::last_progress_state;
int EditorProgressTracker::last_progress_step = 0;
double EditorProgressTracker::last_progress_time = 0;

void EditorProgressTracker::progress_add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {
	if (cmdline_mode) {
		print_line_rich(vformat("[   0%% ] [color=gray][b]%s[/b] | Started %s (%d steps)[/color]", p_task, p_label, p_steps));
		progress_total_steps[p_task] = p_steps;
	} else if (progress_dialog) {
		progress_dialog->add_task(p_task, p_label, p_steps, p_can_cancel);
	}
}

bool EditorProgressTracker::progress_task_step(const String &p_task, const String &p_state, int p_step, bool p_force_refresh) {
	if (cmdline_mode) {
		double current_time = USEC_TO_SEC(OS::get_singleton()->get_ticks_usec());
		double elapsed_time = current_time - last_progress_time;
		if (p_task != last_progress_task || p_state != last_progress_state || p_step != last_progress_step || elapsed_time >= 1.0) {
			// Only print the progress if it's changed since the last print, or if one second has passed.
			// This prevents multithreaded import from printing the same progress too often, which would bloat the log file.
			const int percent = (p_step / float(progress_total_steps[p_task] + 1)) * 100;
			print_line_rich(vformat("[%4d%% ] [color=gray][b]%s[/b] | %s[/color]", percent, p_task, p_state));
			last_progress_task = p_task;
			last_progress_state = p_state;
			last_progress_step = p_step;
			last_progress_time = current_time;
		}
		return false;
	} else if (progress_dialog) {
		return progress_dialog->task_step(p_task, p_state, p_step, p_force_refresh);
	} else {
		return false;
	}
}

void EditorProgressTracker::progress_end_task(const String &p_task) {
	if (cmdline_mode) {
		progress_total_steps.erase(p_task);
		print_line_rich(vformat("[color=green][ DONE ][/color] [b]%s[/b]\n", p_task));
	} else if (progress_dialog) {
		progress_dialog->end_task(p_task);
	}
}

void EditorProgressTracker::progress_add_task_bg(const String &p_task, const String &p_label, int p_steps) {
	if (progress_hb) {
		progress_hb->add_task(p_task, p_label, p_steps);
	}
}

void EditorProgressTracker::progress_task_step_bg(const String &p_task, int p_step) {
	if (progress_hb) {
		progress_hb->task_step(p_task, p_step);
	}
}

void EditorProgressTracker::progress_end_task_bg(const String &p_task) {
	if (progress_hb) {
		progress_hb->end_task(p_task);
	}
}

bool EditorProgress::step(const String &p_state, int p_step, bool p_force_refresh) {
	if (!force_background && Thread::is_main_thread()) {
		return EditorProgressTracker::progress_task_step(task, p_state, p_step, p_force_refresh);
	} else {
		EditorProgressTracker::progress_task_step_bg(task, p_step);
		return false;
	}
}

EditorProgress::EditorProgress(const String &p_task, const String &p_label, int p_amount, bool p_can_cancel, bool p_force_background) {
	if (!p_force_background && Thread::is_main_thread()) {
		EditorProgressTracker::progress_add_task(p_task, p_label, p_amount, p_can_cancel);
	} else {
		EditorProgressTracker::progress_add_task_bg(p_task, p_label, p_amount);
	}
	task = p_task;
	force_background = p_force_background;
}

EditorProgress::~EditorProgress() {
	if (!force_background && Thread::is_main_thread()) {
		EditorProgressTracker::progress_end_task(task);
	} else {
		EditorProgressTracker::progress_end_task_bg(task);
	}
}
