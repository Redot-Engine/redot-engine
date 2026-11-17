/**************************************************************************/
/*  scratch_converter.h                                                   */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

// Converts a Scratch 3 (.sb3) project file to a Godot scene (.tscn) with
// GDScript files representing each sprite's block scripts.
class ScratchConverter {
public:
	// Convert a .sb3 file to a Godot .tscn scene.
	// p_sb3_path: path to the .sb3 file
	// p_output_dir: directory to write extracted assets and the .tscn
	// Returns OK on success
	static Error convert(const String &p_sb3_path, const String &p_output_dir);

private:
	// Parse the blocks JSON and generate GDScript code for all hat chains
	static String _blocks_to_gdscript(const Dictionary &p_blocks, const String &p_target_name);
	// Convert a single top-level hat block chain to a GDScript function
	static String _chain_to_gdscript(const String &p_hat_id, const Dictionary &p_blocks, int p_indent);
	// Convert a single block to GDScript statement(s)
	static String _block_to_gdscript(const String &p_block_id, const Dictionary &p_blocks, int p_indent);
	// Get input value (literal or reporter block) as a GDScript expression
	static String _input_to_expr(const String &p_block_id, const String &p_input_name, const Dictionary &p_blocks);
	// Write a .gd file for a target's scripts
	static Error _write_script(const String &p_path, const String &p_script_content);
	// Copy costume and sound asset files out of the unzipped directory
	static Error _extract_assets(const String &p_unzip_dir, const String &p_output_dir, const Array &p_costumes, const Array &p_sounds);
	// Generate .tscn scene text from converted target data
	static String _generate_tscn(const Array &p_targets, const String &p_base_dir);
};

#endif // TOOLS_ENABLED
