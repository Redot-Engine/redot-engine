/**************************************************************************/
/*  engine_importer.h                                                     */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

#include "import_report.h"

/// Base class for every "open or convert a project from engine X" backend.
///
/// Importers are deliberately small: they own engine-specific parsing and
/// delegate shared concerns (reporting, path handling, writing) to this
/// module. The editor UI and the command line both go through the registry,
/// so adding an engine is a single registration.
class EngineImporter : public RefCounted {
	GDCLASS(EngineImporter, RefCounted);

protected:
	static void _bind_methods();

public:
	/// Stable machine-readable identifier, e.g. "unity".
	virtual String get_engine_id() const = 0;
	/// Human-readable name, e.g. "Unity".
	virtual String get_engine_name() const = 0;
	/// True when `p_source_dir` is a project this importer understands.
	virtual bool can_import(const String &p_source_dir) const = 0;

	/// Lightweight description used by the project manager and the UI.
	virtual Dictionary get_project_info(const String &p_source_dir) const;

	/// Bake the project into `p_target_dir` (a Redot project folder).
	/// Returns the report dictionary, which always contains "converted",
	/// "skipped", "warnings" and "errors".
	virtual Dictionary import_project(const String &p_source_dir, const String &p_target_dir, const Dictionary &p_options = Dictionary()) = 0;

	/// Open the project in place, without writing a Redot project. Importers
	/// that support this return true and the editor uses their resource
	/// loaders. Unity implements it through UnityFormatLoader.
	virtual bool supports_direct_open() const { return false; }
};

/// Process-wide list of available importers.
class ProjectImportRegistry {
	static ProjectImportRegistry *singleton;

	Vector<Ref<EngineImporter>> importers;

public:
	static ProjectImportRegistry *get_singleton();

	void add_importer(const Ref<EngineImporter> &p_importer);
	Ref<EngineImporter> find_importer_for(const String &p_source_dir) const;
	Ref<EngineImporter> get_importer(const String &p_engine_id) const;
	const Vector<Ref<EngineImporter>> &get_importers() const { return importers; }
	void clear() { importers.clear(); }
};

/// Shared helpers used by importers and format loaders.
namespace ProjectImportUtils {
Error ensure_dir(const String &p_path);
Error write_text(const String &p_path, const String &p_text);
Error copy_file(const String &p_from, const String &p_to);

String sanitize_identifier(const String &p_name);
String sanitize_node_name(const String &p_name);
String res_path_join(const String &p_base, const String &p_relative);
String relative_path(const String &p_root, const String &p_path);
bool is_ignored_directory(const String &p_name);
bool is_text_yaml(const String &p_path);
} // namespace ProjectImportUtils
