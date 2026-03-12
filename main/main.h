/**************************************************************************/
/*  main.h                                                                */
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

/**
 * @file main.h
 *
 * [Add any documentation that applies to the entire file here!]
 */

#pragma once

#include "core/os/thread.h"
#include "core/typedefs.h"

template <typename T>
class Vector;
struct Color;

class Main {
	enum CLIOptionAvailability {
		CLI_OPTION_AVAILABILITY_EDITOR,
		CLI_OPTION_AVAILABILITY_TEMPLATE_DEBUG,
		CLI_OPTION_AVAILABILITY_TEMPLATE_RELEASE,
		CLI_OPTION_AVAILABILITY_HIDDEN,
	};

	static void print_header(bool p_rich);
	/**
	 * Prints a copyright notice in the command-line help with colored text. A newline is
	 * automatically added at the end.
	 */
	static void print_help_copyright(const char *p_notice);
	/**
	 * Prints a title in the command-line help with colored text. A newline is
	 * automatically added at beginning and at the end.
	 */
	static void print_help_title(const char *p_title);
	/**
	 * Prints an option in the command-line help with colored text. No newline is
	 * added at the end. `p_availability` denotes which build types the argument is
	 * available in. Support in release export templates implies support in debug
	 * export templates and editor. Support in debug export templates implies
	 * support in editor.
	 */
	static void print_help_option(const char *p_option, const char *p_description, CLIOptionAvailability p_availability = CLI_OPTION_AVAILABILITY_TEMPLATE_RELEASE);
	/**
	 * Returns the option string with required and optional arguments colored separately from the rest of the option.
	 * This color replacement must be done *after* calling `rpad()` for the length padding to be done correctly.
	 */
	static String format_help_option(const char *p_option);
	static void print_help(const char *p_binary);
	static uint64_t last_ticks;
	static uint32_t hide_print_fps_attempts;
	static uint32_t frames;
	static uint32_t frame;
	static bool force_redraw_requested;
	static int iterating;

public:
	static bool is_cmdline_tool();
#ifdef TOOLS_ENABLED
	enum CLIScope {
		CLI_SCOPE_TOOL, ///< Editor and project manager.
		CLI_SCOPE_PROJECT,
	};
	static const Vector<String> &get_forwardable_cli_arguments(CLIScope p_scope);
#endif

	static int test_entrypoint(int argc, char *argv[], bool &tests_need_run);
	/**
	 * Engine initialization
	 *
	 * Consists of several methods that are called by each platform's specific main(argc, argv).
	 * To fully understand engine init, one should therefore start from the platform's main and
	 * see how it calls into the Main class' methods.
	 *
	 * The initialization is typically done in 3 steps (with the setup2 step triggered either
	 * automatically by setup, or manually in the platform's main).
	 *
	 * - setup(execpath, argc, argv, p_second_phase) is the main entry point for all platforms,
	 *   responsible for the initialization of all low level singletons and core types, and parsing
	 *   command line arguments to configure things accordingly.
	 *   If p_second_phase is true, it will chain into setup2() (default behavior). This is
	 *   disabled on some platforms (Android, iOS) which trigger the second step in their own time.
	 *
	 * - setup2(p_main_tid_override) registers high level servers and singletons, displays the
	 *   boot splash, then registers higher level types (scene, editor, etc.).
	 *
	 * - start() is the last step and that's where command line tools can run, or the main loop
	 *   can be created eventually and the project settings put into action. That's also where
	 *   the editor node is created, if relevant.
	 *   start() does it own argument parsing for a subset of the command line arguments described
	 *   in help, it's a bit messy and should be globalized with the setup() parsing somehow.
	 */
	static Error setup(const char *execpath, int argc, char *argv[], bool p_second_phase = true);
	static Error setup2(bool p_show_boot_logo = true); // The thread calling setup2() will effectively become the main thread.
	static String get_rendering_driver_name();
	static void setup_boot_logo();
	static Color get_boot_splash_bg_color();
#ifdef TESTS_ENABLED
	/// The order is the same as in `Main::setup()`, only core and some editor types
	/// are initialized here. This also combines `Main::setup2()` initialization.
	static Error test_setup();
	/// The order is the same as in `Main::cleanup()`.
	static void test_cleanup();
#endif
	/// @return Should be EXIT_SUCCESS if we start successfully
	/// and should move on to `OS::run`, and EXIT_FAILURE otherwise for
	/// an early exit with that error code.
	static int start();

	/// @return `false` means iterating further, returning `true` means `OS::run`
	/// will terminate the program. In case of failure, the OS exit code needs
	/// to be set explicitly here (defaults to EXIT_SUCCESS).
	static bool iteration();
	static void force_redraw();

	static bool is_iterating();

	/**
	 * Engine deinitialization
	 *
	 * Responsible for freeing all the memory allocated by previous setup steps,
	 * so that the engine closes cleanly without leaking memory or crashing.
	 * The order matters as some of those steps are linked with each other.
	 */
	static void cleanup(bool p_force = false);
};

/// Test main override is for the testing behavior.
#define TEST_MAIN_OVERRIDE                                         \
	bool run_test = false;                                         \
	int return_code = Main::test_entrypoint(argc, argv, run_test); \
	if (run_test) {                                                \
		return return_code;                                        \
	}

#define TEST_MAIN_PARAM_OVERRIDE(argc, argv)                       \
	bool run_test = false;                                         \
	int return_code = Main::test_entrypoint(argc, argv, run_test); \
	if (run_test) {                                                \
		return return_code;                                        \
	}
