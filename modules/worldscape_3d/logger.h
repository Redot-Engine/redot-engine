/**************************************************************************/
/*  logger.h                                                              */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

#pragma once

#include "core/variant/variant_utility.h"

/**
 * Prints warnings, errors, and messages to the console.
 * Regular messages are filtered based on the user specified debug level.
 * Warnings and errors always print except in release builds.
 * EXTREME is for continuously called prints like inside snapping.
 * See WorldScape3D::DebugLevel and WorldScape3D::debug_level.
 *
 * Note that in DEBUG mode Godot will crash on quit due to an
 * access violation in editor_log.cpp EditorLog::_process_message().
 * This is most likely caused by us printing messages as Godot is
 * attempting to quit.
 */

// TODO find the push/print UtilityFunctions
#if 0
#define LOG(level, ...)                                                                                 \
	do {                                                                                                \
		if (level == ERROR)                                                                             \
			UtilityFunctions::push_error(__class__, ":", __func__, ":", __LINE__, ": ", __VA_ARGS__);   \
		else if (level == WARN)                                                                         \
			UtilityFunctions::push_warning(__class__, ":", __func__, ":", __LINE__, ": ", __VA_ARGS__); \
		else if (level <= WorldScape3D::debug_level)                                                    \
			UtilityFunctions::print(__class__, ":", __func__, ":", __LINE__, ": ", __VA_ARGS__);        \
	} while (false); // Macro safety
#else
#define LOG(...)
#endif
