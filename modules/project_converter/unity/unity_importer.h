/**************************************************************************/
/*  unity_importer.h                                                      */
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

#include "../import/engine_importer.h"

/// Unity backend for the project converter.
///
/// Two modes share the same parsing code:
///  * Direct-open: the editor treats the Unity folder as the project and the
///    format loader builds scenes on demand (see unity_format_loader.h).
///  * Baking: `import_project()` writes a complete Redot project, converting
///    scenes, prefabs, materials, shaders and scripts.
class UnityImporter : public EngineImporter {
	GDCLASS(UnityImporter, EngineImporter);

protected:
	static void _bind_methods() {}

public:
	String get_engine_id() const override { return "unity"; }
	String get_engine_name() const override { return "Unity"; }
	bool can_import(const String &p_source_dir) const override;
	Dictionary get_project_info(const String &p_source_dir) const override;
	Dictionary import_project(const String &p_source_dir, const String &p_target_dir, const Dictionary &p_options = Dictionary()) override;
	bool supports_direct_open() const override { return true; }
};
