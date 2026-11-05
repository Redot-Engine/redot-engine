/**************************************************************************/
/*  physx_blast_runtime.h                                                 */
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

// Blast runtime availability (RUNTIME-1).
//
// The editor starts WITHOUT the Blast runtime libraries: Windows
// delay-loads the DLLs, and the Linux release binary's NEEDED entries are
// removed at release time (every Blast import is a lazily-bound function,
// so nothing binds eagerly). The runtime manager installs the libraries
// next to the editor executable when a project uses them; until then Blast
// is "disabled": every path that reaches NvBlast must call
// blast_runtime_load() first and bail out gracefully when it reports false
// -- an unguarded call would terminate the process with "symbol lookup
// error" (Linux) or a delay-load exception (Windows).
//
// The check is idempotent and retryable: it re-attempts on every call until
// it succeeds, so installing the runtime takes effect for Blast without an
// editor restart.

#ifdef GODOT_PHYSX_BLAST

#include "core/io/file_access.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED

// The DLLs are delay-loaded: their presence next to the executable is all
// the CRT delay helper needs to resolve the imports on first use. (Global
// is not checked -- nothing imports it directly; the other DLLs load it as
// their own dependency.)
inline bool blast_runtime_load() {
	static bool loaded = false;
	static bool warned = false;
	if (loaded) {
		return true;
	}
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	loaded = FileAccess::exists(exe_dir.path_join("NvBlast.dll")) &&
			FileAccess::exists(exe_dir.path_join("NvBlastExtAuthoring.dll")) &&
			FileAccess::exists(exe_dir.path_join("NvBlastExtShaders.dll"));
	if (!loaded && !warned) {
		warned = true;
		ERR_PRINT("PhysX: the Blast runtime DLLs are not installed next to the editor executable -- Blast is disabled until the PhysX runtime is installed.");
	}
	return loaded;
}

#else // WINDOWS_ENABLED

// Linux/BSD: the runtime libraries are dlopen'ed RTLD_GLOBAL from the
// executable's directory. The module's NvBlast calls are lazily-bound PLT
// entries, so they resolve against the loaded libraries from then on (the
// same mechanism the GPU runtime's libcuda preload relies on). Their
// transitive dependencies (globals, Tk, serialization, stress) ride along
// through their own sonames and the vendored $ORIGIN rpath.
#include <dlfcn.h>

inline bool blast_runtime_load() {
	static bool loaded = false;
	static bool warned = false;
	if (loaded) {
		return true;
	}
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	loaded = true;
	for (const char *lib : { "libNvBlast.so", "libNvBlastGlobals.so", "libNvBlastExtAuthoring.so", "libNvBlastExtShaders.so" }) {
		if (dlopen(exe_dir.path_join(lib).utf8().get_data(), RTLD_NOW | RTLD_GLOBAL) == nullptr) {
			loaded = false;
			break;
		}
	}
	if (!loaded && !warned) {
		warned = true;
		ERR_PRINT("PhysX: the Blast runtime libraries are not installed next to the editor executable -- Blast is disabled until the PhysX runtime is installed.");
	}
	return loaded;
}

#endif // WINDOWS_ENABLED

#endif // GODOT_PHYSX_BLAST
