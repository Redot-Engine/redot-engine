/**************************************************************************/
/*  physx_runtime_manager.h                                               */
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

#include "core/object/object.h"

// Tracks the PhysX runtime binaries that must sit next to the editor
// executable (PhysXGpu_64.dll and the hard-imported NvBlast DLLs on Windows).
// The editor binary is built with these DLLs delay-loaded, so it starts
// without them; the module uses this manager to detect the gap and offer the
// download-and-restart flow. The download itself is not implemented yet --
// install_missing_files() is a stub.
class PhysXRuntimeManager : public Object {
	GDCLASS(PhysXRuntimeManager, Object);

public:
	// Manifest source for the download-and-restart flow. When the Redot team
	// adopts the runtime repo, only this URL changes.
	static const char *MANIFEST_URL;

	// Runtime files that must sit next to the editor executable, per
	// platform. Names only: the directory is always the executable's.
	static PackedStringArray get_required_runtime_files();

	// The subset of get_required_runtime_files() that is not present in the
	// executable's directory.
	static PackedStringArray get_missing_runtime_files();

	static bool is_runtime_complete();

	// Downloads and installs the missing runtime files. Not implemented yet:
	// the download-and-restart flow is being added incrementally.
	static Error install_missing_files();

	// --- Install helpers (used by the editor plugin's install flow) ---

	// Parses manifest JSON text and returns the Windows bundle dictionary
	// {url, sha256, size_bytes, files} -- empty Dictionary when the manifest
	// is missing entries or malformed.
	static Dictionary parse_manifest(const String &p_json_text);

	// Lowercase hex SHA-256 of a file; empty String on failure.
	static String sha256_file(const String &p_path);

	// Extracts the bundle's manifest-listed files from the zip into
	// p_target_dir, verifying each file's SHA-256 while writing. Fails
	// without leaving partial files behind.
	static Error extract_bundle(const String &p_zip_path, const Dictionary &p_bundle, const String &p_target_dir);

protected:
	static void _bind_methods();
};
