/**************************************************************************/
/*  engine_importer.cpp                                                   */
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

#include "engine_importer.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

void EngineImporter::_bind_methods() {
}

Dictionary EngineImporter::get_project_info(const String &p_source_dir) const {
	Dictionary info;
	info["engine"] = get_engine_id();
	info["engine_name"] = get_engine_name();
	info["path"] = p_source_dir;
	info["name"] = p_source_dir.get_file();
	info["direct_open"] = supports_direct_open();
	return info;
}

ProjectImportRegistry *ProjectImportRegistry::singleton = nullptr;

ProjectImportRegistry *ProjectImportRegistry::get_singleton() {
	static ProjectImportRegistry registry;
	return &registry;
}

void ProjectImportRegistry::add_importer(const Ref<EngineImporter> &p_importer) {
	ERR_FAIL_COND(p_importer.is_null());
	for (const Ref<EngineImporter> &existing : importers) {
		if (existing.is_valid() && existing->get_engine_id() == p_importer->get_engine_id()) {
			return;
		}
	}
	importers.push_back(p_importer);
}

Ref<EngineImporter> ProjectImportRegistry::find_importer_for(const String &p_source_dir) const {
	for (const Ref<EngineImporter> &importer : importers) {
		if (importer.is_valid() && importer->can_import(p_source_dir)) {
			return importer;
		}
	}
	return Ref<EngineImporter>();
}

Ref<EngineImporter> ProjectImportRegistry::get_importer(const String &p_engine_id) const {
	for (const Ref<EngineImporter> &importer : importers) {
		if (importer.is_valid() && importer->get_engine_id() == p_engine_id) {
			return importer;
		}
	}
	return Ref<EngineImporter>();
}

namespace ProjectImportUtils {

Error ensure_dir(const String &p_path) {
	if (p_path.is_empty()) {
		return OK;
	}
	Ref<DirAccess> d = DirAccess::create_for_path(p_path);
	ERR_FAIL_COND_V_MSG(d.is_null(), ERR_CANT_CREATE, vformat("Cannot create directory access for '%s'.", p_path));
	return d->make_dir_recursive(p_path);
}

Error write_text(const String &p_path, const String &p_text) {
	Error err = ensure_dir(p_path.get_base_dir());
	ERR_FAIL_COND_V(err != OK, err);
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err != OK ? err : ERR_CANT_CREATE, vformat("Cannot write '%s'.", p_path));
	f->store_string(p_text);
	return OK;
}

Error copy_file(const String &p_from, const String &p_to) {
	Error err = ensure_dir(p_to.get_base_dir());
	ERR_FAIL_COND_V(err != OK, err);

	Ref<FileAccess> src = FileAccess::open(p_from, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(src.is_null(), err != OK ? err : ERR_FILE_CANT_OPEN, vformat("Cannot read '%s'.", p_from));

	Ref<FileAccess> dst = FileAccess::open(p_to, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(dst.is_null(), err != OK ? err : ERR_FILE_CANT_WRITE, vformat("Cannot write '%s'.", p_to));

	const uint64_t chunk = 1 << 16;
	uint64_t remaining = src->get_length();
	while (remaining > 0) {
		PackedByteArray buffer = src->get_buffer(MIN(remaining, chunk));
		if (buffer.is_empty()) {
			break;
		}
		dst->store_buffer(buffer);
		remaining -= buffer.size();
	}
	return OK;
}

String sanitize_identifier(const String &p_name) {
	String out;
	for (int i = 0; i < p_name.length(); i++) {
		const char32_t c = p_name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
			out += c;
		} else {
			out += '_';
		}
	}
	if (out.is_empty()) {
		out = "Imported";
	}
	if (out[0] >= '0' && out[0] <= '9') {
		out = "_" + out;
	}
	return out;
}

String sanitize_node_name(const String &p_name) {
	String out = p_name.strip_edges();
	if (out.is_empty()) {
		return "Node";
	}
	static const String invalid = ".:@/\"%";
	for (int i = 0; i < invalid.length(); i++) {
		out = out.replace(invalid.substr(i, 1), "_");
	}
	return out;
}

String res_path_join(const String &p_base, const String &p_relative) {
	String base = p_base;
	if (!base.ends_with("/")) {
		base += "/";
	}
	return base + p_relative.replace_char('\\', '/');
}

String relative_path(const String &p_root, const String &p_path) {
	String root = p_root.replace_char('\\', '/');
	String path = p_path.replace_char('\\', '/');
	if (!root.ends_with("/")) {
		root += "/";
	}
	if (path.begins_with(root)) {
		return path.substr(root.length());
	}
	return path;
}

bool is_ignored_directory(const String &p_name) {
	static const char *ignored[] = {
		"Library", "Temp", "Logs", "obj", "obj.meta", "Build", "Builds", "UserSettings",
		".git", ".vs", ".idea", ".vscode", "node_modules", "PackageCache", "Recordings"
	};
	for (const char *name : ignored) {
		if (p_name == name) {
			return true;
		}
	}
	return p_name.begins_with(".");
}

bool is_text_yaml(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	String head = f->get_buffer(8).get_string_from_utf8();
	return head.begins_with("%YAML") || head.begins_with("---");
}

} // namespace ProjectImportUtils
