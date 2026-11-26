/**************************************************************************/
/*  import_report.h                                                       */
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
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

/// Collects diagnostics while a project is being imported or opened.
///
/// Every importer reports through this object instead of printing to the
/// console. The editor UI, CLI and tests all consume the same structure.
class ProjectImportReport : public RefCounted {
	GDCLASS(ProjectImportReport, RefCounted);

public:
	enum Severity {
		SEVERITY_INFO,
		SEVERITY_WARNING,
		SEVERITY_ERROR,
	};

	struct Entry {
		Severity severity = SEVERITY_INFO;
		String message;
		String path;
	};

private:
	String engine;
	String source_path;
	String target_path;
	Vector<Entry> entries;
	int converted = 0;
	int skipped = 0;

protected:
	static void _bind_methods();

public:
	void set_engine(const String &p_engine) { engine = p_engine; }
	String get_engine() const { return engine; }

	void set_source_path(const String &p_path) { source_path = p_path; }
	String get_source_path() const { return source_path; }

	void set_target_path(const String &p_path) { target_path = p_path; }
	String get_target_path() const { return target_path; }

	void info(const String &p_message, const String &p_path = String());
	void warning(const String &p_message, const String &p_path = String());
	void error(const String &p_message, const String &p_path = String());

	void add_converted(int p_count = 1) { converted += p_count; }
	void add_skipped(int p_count = 1) { skipped += p_count; }

	int get_converted_count() const { return converted; }
	int get_skipped_count() const { return skipped; }
	int get_error_count() const;
	int get_warning_count() const;
	const Vector<Entry> &get_entries() const { return entries; }

	bool has_errors() const { return get_error_count() > 0; }

	Dictionary to_dictionary() const;
	String to_text() const;
	Error write_markdown(const String &p_path) const;
	Error write_json(const String &p_path) const;
};
