/**************************************************************************/
/*  import_report.cpp                                                     */
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

#include "import_report.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

void ProjectImportReport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_engine"), &ProjectImportReport::get_engine);
	ClassDB::bind_method(D_METHOD("get_source_path"), &ProjectImportReport::get_source_path);
	ClassDB::bind_method(D_METHOD("get_target_path"), &ProjectImportReport::get_target_path);
	ClassDB::bind_method(D_METHOD("get_converted_count"), &ProjectImportReport::get_converted_count);
	ClassDB::bind_method(D_METHOD("get_skipped_count"), &ProjectImportReport::get_skipped_count);
	ClassDB::bind_method(D_METHOD("get_error_count"), &ProjectImportReport::get_error_count);
	ClassDB::bind_method(D_METHOD("get_warning_count"), &ProjectImportReport::get_warning_count);
	ClassDB::bind_method(D_METHOD("has_errors"), &ProjectImportReport::has_errors);
	ClassDB::bind_method(D_METHOD("to_dictionary"), &ProjectImportReport::to_dictionary);
	ClassDB::bind_method(D_METHOD("to_text"), &ProjectImportReport::to_text);
}

void ProjectImportReport::info(const String &p_message, const String &p_path) {
	Entry e;
	e.severity = SEVERITY_INFO;
	e.message = p_message;
	e.path = p_path;
	entries.push_back(e);
}

void ProjectImportReport::warning(const String &p_message, const String &p_path) {
	Entry e;
	e.severity = SEVERITY_WARNING;
	e.message = p_message;
	e.path = p_path;
	entries.push_back(e);
	print_verbose(vformat("[%s import] %s%s", engine, p_message, p_path.is_empty() ? String() : " (" + p_path + ")"));
}

void ProjectImportReport::error(const String &p_message, const String &p_path) {
	Entry e;
	e.severity = SEVERITY_ERROR;
	e.message = p_message;
	e.path = p_path;
	entries.push_back(e);
	ERR_PRINT(vformat("[%s import] %s%s", engine, p_message, p_path.is_empty() ? String() : " (" + p_path + ")"));
}

int ProjectImportReport::get_error_count() const {
	int count = 0;
	for (const Entry &e : entries) {
		if (e.severity == SEVERITY_ERROR) {
			count++;
		}
	}
	return count;
}

int ProjectImportReport::get_warning_count() const {
	int count = 0;
	for (const Entry &e : entries) {
		if (e.severity == SEVERITY_WARNING) {
			count++;
		}
	}
	return count;
}

Dictionary ProjectImportReport::to_dictionary() const {
	Array messages;
	for (const Entry &e : entries) {
		Dictionary d;
		d["severity"] = e.severity == SEVERITY_ERROR ? "error" : (e.severity == SEVERITY_WARNING ? "warning" : "info");
		d["message"] = e.message;
		d["path"] = e.path;
		messages.push_back(d);
	}

	Dictionary d;
	d["engine"] = engine;
	d["source_path"] = source_path;
	d["target_path"] = target_path;
	d["converted"] = converted;
	d["skipped"] = skipped;
	d["errors"] = get_error_count();
	d["warnings"] = get_warning_count();
	d["messages"] = messages;
	return d;
}

String ProjectImportReport::to_text() const {
	String text;
	text += vformat("%s import: %s -> %s\n", engine.capitalize(), source_path, target_path);
	text += vformat("Converted: %d, skipped: %d, warnings: %d, errors: %d\n", converted, skipped, get_warning_count(), get_error_count());
	for (const Entry &e : entries) {
		const char *tag = e.severity == SEVERITY_ERROR ? "ERROR" : (e.severity == SEVERITY_WARNING ? "WARN" : "INFO");
		text += vformat("  [%s] %s%s\n", tag, e.message, e.path.is_empty() ? String() : " (" + e.path + ")");
	}
	return text;
}

Error ProjectImportReport::write_markdown(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, vformat("Cannot write import report to '%s'.", p_path));

	f->store_line(vformat("# %s project import report", engine.capitalize()));
	f->store_line("");
	f->store_line(vformat("- Source: `%s`", source_path));
	f->store_line(vformat("- Target: `%s`", target_path));
	f->store_line(vformat("- Converted assets: %d", converted));
	f->store_line(vformat("- Skipped assets: %d", skipped));
	f->store_line(vformat("- Warnings: %d", get_warning_count()));
	f->store_line(vformat("- Errors: %d", get_error_count()));
	f->store_line("");
	f->store_line("## Messages");
	f->store_line("");
	f->store_line("| Severity | Path | Message |");
	f->store_line("| --- | --- | --- |");
	for (const Entry &e : entries) {
		const char *tag = e.severity == SEVERITY_ERROR ? "error" : (e.severity == SEVERITY_WARNING ? "warning" : "info");
		String message = e.message.replace("|", "\\|");
		String path = e.path.replace("|", "\\|");
		f->store_line(vformat("| %s | `%s` | %s |", tag, path, message));
	}
	return OK;
}

Error ProjectImportReport::write_json(const String &p_path) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, vformat("Cannot write import report to '%s'.", p_path));
	f->store_string(JSON::stringify(to_dictionary(), "\t"));
	return OK;
}
