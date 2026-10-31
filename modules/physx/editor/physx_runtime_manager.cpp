/**************************************************************************/
/*  physx_runtime_manager.cpp                                             */
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

#include "physx_runtime_manager.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>

#include <delayimp.h>
#include <cstdio>

// Diagnostics for the download-and-restart flow: the runtime DLLs are
// delay-loaded, and this hook names whichever one fails to load (and when --
// a failure during startup means some static initializer touched a Blast/GPU
// import before the runtime was installed).
static FARPROC WINAPI physx_dli_fail_hook(unsigned p_notify, DelayLoadInfo *p_info) {
	if (p_notify == dliFailLoadLib && p_info && p_info->szDll) {
		fprintf(stderr, "PhysX: delay-load failed for '%s' (runtime files missing?)\n", p_info->szDll);
	}
	return nullptr; // NULL keeps the default behavior: the import raises an error.
}

// Global delay-load fail hook shared by every delay-imported PhysX/Blast DLL.
extern "C" PfnDliHook __pfnDliFailHook2 = physx_dli_fail_hook;
#endif

#ifdef WINDOWS_ENABLED
// The runtime DLLs staged next to the editor executable (see the SCsub
// staging lists), minus what this build never stages: SCsub copies
// PhysXGpu_64.dll in every Windows configuration, but the Blast and nvflow
// sets are staged only when the blast=yes / flow=yes code is compiled in --
// requiring them here would pop the download dialog forever in those builds.
// Keep this list in step with the runtime bundle's manifest "files"
// (redot-physx-runtime).
static const char *required_runtime_files[] = {
	"PhysXGpu_64.dll",
#ifdef GODOT_PHYSX_BLAST
	"NvBlast.dll",
	"NvBlastGlobals.dll",
	"NvBlastExtAuthoring.dll",
	"NvBlastExtShaders.dll",
	"NvBlastExtAssetUtils.dll",
	"NvBlastExtSerialization.dll",
	"NvBlastExtStress.dll",
	"NvBlastExtTkSerialization.dll",
	"NvBlastTk.dll",
#endif
#ifdef GODOT_PHYSX_FLOW
	"nvflow.dll",
	"nvflowext.dll",
#endif
};
#endif

#ifdef __linux__
// The runtime .so files staged next to the editor executable (see the SCsub
// staging lists), minus what this build never stages: libPhysXGpu_64.so only
// goes to bin/ with physx_gpu=yes (the CPU libs' GPU dispatch references
// resolve from the stub instead), and the Blast / nvflow sets follow
// blast=yes / flow=yes. The nine Blast libraries are hard-linked into the
// editor binary ($ORIGIN rpath, no delay-load on Linux): without them the
// engine cannot start at all, so the download offer can only fire when the
// Blast set is present but the lazily-loaded PhysXGpu / Flow runtimes are
// missing -- the bundle otherwise serves manual installs. Keep this list in
// step with the runtime bundle's manifest "files" (redot-physx-runtime).
static const char *required_runtime_files[] = {
#ifdef GODOT_PHYSX_GPU
	"libPhysXGpu_64.so",
#endif
#ifdef GODOT_PHYSX_BLAST
	"libNvBlast.so",
	"libNvBlastGlobals.so",
	"libNvBlastExtAuthoring.so",
	"libNvBlastExtShaders.so",
	"libNvBlastExtAssetUtils.so",
	"libNvBlastExtSerialization.so",
	"libNvBlastExtStress.so",
	"libNvBlastExtTkSerialization.so",
	"libNvBlastTk.so",
#endif
#ifdef GODOT_PHYSX_FLOW
	"libnvflow.so",
	"libnvflowext.so",
#endif
};
#endif

PackedStringArray PhysXRuntimeManager::get_required_runtime_files() {
	PackedStringArray files;
#ifdef WINDOWS_ENABLED
	for (const char *name : required_runtime_files) {
		files.push_back(String(name));
	}
#elif defined(__linux__)
	for (const char *name : required_runtime_files) {
		files.push_back(String(name));
	}
#endif
	return files;
}

PackedStringArray PhysXRuntimeManager::get_missing_runtime_files() {
	PackedStringArray missing;
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	for (const String &name : get_required_runtime_files()) {
		if (!FileAccess::exists(exe_dir.path_join(name))) {
			missing.push_back(name);
		}
	}
	return missing;
}

bool PhysXRuntimeManager::is_runtime_complete() {
	return get_missing_runtime_files().is_empty();
}

Error PhysXRuntimeManager::install_missing_files() {
	// The editor plugin's dialog drives the real flow (manifest -> download ->
	// verify -> extract -> restart). This static entry stays for scripts.
	WARN_PRINT("PhysX: use the editor's 'Download and Restart' flow to install the runtime.");
	return ERR_UNAVAILABLE;
}

void PhysXRuntimeManager::_bind_methods() {
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("get_required_runtime_files"), &PhysXRuntimeManager::get_required_runtime_files);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("get_missing_runtime_files"), &PhysXRuntimeManager::get_missing_runtime_files);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("is_runtime_complete"), &PhysXRuntimeManager::is_runtime_complete);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("install_missing_files"), &PhysXRuntimeManager::install_missing_files);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("parse_manifest", "json_text"), &PhysXRuntimeManager::parse_manifest);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("sha256_file", "path"), &PhysXRuntimeManager::sha256_file);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("extract_bundle", "zip_path", "bundle", "target_dir"), &PhysXRuntimeManager::extract_bundle);
}

const char *PhysXRuntimeManager::MANIFEST_URL =
		"https://raw.githubusercontent.com/Vlado-VG/redot-physx-runtime/main/manifest.json";

const char *PhysXRuntimeManager::RELEASES_PAGE_URL =
		"https://github.com/Vlado-VG/redot-physx-runtime/releases";

// Maps the running OS to the manifest's "platforms" key; empty when the
// platform has no runtime bundle.
static String platform_key() {
#ifdef WINDOWS_ENABLED
	return "windows";
#elif defined(__linux__)
	return "linux";
#else
	return String();
#endif
}

Dictionary PhysXRuntimeManager::parse_manifest(const String &p_json_text) {
	JSON json;
	if (json.parse(p_json_text) != OK) {
		return Dictionary();
	}
	Variant parsed = json.get_data();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	Dictionary manifest = parsed;
	Dictionary platforms = manifest.get("platforms", Dictionary());
	const String key = platform_key();
	if (key.is_empty()) {
		return Dictionary();
	}
	Dictionary platform = platforms.get(key, Dictionary());
	Dictionary bundle = platform.get("bundle", Dictionary());
	if (bundle.is_empty()) {
		return Dictionary();
	}
	// Required well-formed fields: a download URL, the bundle checksum, and
	// the per-file list the installer verifies after extraction.
	if (!bundle.has("url") || !bundle.has("sha256") || !bundle.has("size_bytes") || !bundle.has("files")) {
		return Dictionary();
	}
	if (String(bundle["sha256"]).length() != 64) {
		return Dictionary();
	}
	Array files = bundle["files"];
	if (files.is_empty()) {
		return Dictionary();
	}
	for (int i = 0; i < files.size(); i++) {
		Dictionary entry = files[i];
		if (!entry.has("name") || !entry.has("sha256") || String(entry["sha256"]).length() != 64) {
			return Dictionary();
		}
	}
	return bundle;
}

String PhysXRuntimeManager::sha256_file(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return String();
	}
	CryptoCore::SHA256Context ctx;
	ctx.start();
	const size_t chunk = 1024 * 1024;
	LocalVector<uint8_t> buf;
	buf.resize(chunk);
	while (true) {
		uint64_t n = f->get_buffer(buf.ptr(), chunk);
		if (n == 0) {
			break;
		}
		ctx.update(buf.ptr(), (size_t)n);
		if (f->eof_reached()) {
			break;
		}
	}
	unsigned char hash[32];
	ctx.finish(hash);
	return String::hex_encode_buffer(hash, 32);
}

Error PhysXRuntimeManager::extract_bundle(const String &p_zip_path, const Dictionary &p_bundle, const String &p_target_dir) {
	Array files = p_bundle.get("files", Array());
	if (files.is_empty()) {
		ERR_PRINT("PhysX: bundle manifest has no file list.");
		return ERR_INVALID_DATA;
	}

	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);
	unzFile zip = unzOpen2(p_zip_path.utf8().get_data(), &io);
	if (!zip) {
		ERR_PRINT("PhysX: failed to open the downloaded runtime bundle.");
		return ERR_FILE_CANT_OPEN;
	}

	Ref<DirAccess> dir = DirAccess::open(p_target_dir);
	if (dir.is_null()) {
		// The target may not exist yet (fresh install into a new directory).
		if (DirAccess::make_dir_recursive_absolute(p_target_dir) != OK) {
			unzClose(zip);
			ERR_PRINT(vformat("PhysX: target directory '%s' is not accessible.", p_target_dir));
			return ERR_CANT_OPEN;
		}
		dir = DirAccess::open(p_target_dir);
		if (dir.is_null()) {
			unzClose(zip);
			ERR_PRINT(vformat("PhysX: target directory '%s' is not accessible.", p_target_dir));
			return ERR_CANT_OPEN;
		}
	}

	Error result = OK;
	// Names this call actually wrote, for the failure cleanup below.
	PackedStringArray written;
	// Extract manifest-listed files one by one, hashing each as it lands so a
	// corrupted download is caught before the editor ever uses it. On any
	// failure the files this call wrote are removed; ones it never got to
	// (and working files from an earlier install) stay.
	for (int i = 0; i < files.size() && result == OK; i++) {
		Dictionary entry = files[i];
		String name = entry.get("name", String());
		String want_hash = entry.get("sha256", String());
		// Manifest entries are plain file names next to the executable: a
		// separator or ".." would write wherever the name points.
		if (name.is_empty() || name.contains("/") || name.contains("\\") || name.contains("..")) {
			ERR_PRINT(vformat("PhysX: invalid bundle file name '%s'.", name));
			result = ERR_INVALID_DATA;
			break;
		}
		String target = p_target_dir.path_join(name);

		if (godot_unzip_locate_file(zip, name) != UNZ_OK) {
			ERR_PRINT(vformat("PhysX: bundle is missing file '%s'.", name));
			result = ERR_FILE_NOT_FOUND;
			break;
		}
		if (unzOpenCurrentFile(zip) != UNZ_OK) {
			ERR_PRINT(vformat("PhysX: cannot read '%s' from the bundle.", name));
			result = ERR_FILE_CANT_READ;
			break;
		}

		Ref<FileAccess> out = FileAccess::open(target, FileAccess::WRITE);
		if (out.is_null()) {
			unzCloseCurrentFile(zip);
			ERR_PRINT(vformat("PhysX: cannot write '%s'.", target));
			result = ERR_FILE_CANT_WRITE;
			break;
		}
		written.push_back(name);

		CryptoCore::SHA256Context ctx;
		ctx.start();
		LocalVector<uint8_t> buf;
		buf.resize(1024 * 1024);
		while (true) {
			int n = unzReadCurrentFile(zip, buf.ptr(), (unsigned)buf.size());
			if (n < 0) {
				out.unref();
				dir->remove(name);
				unzCloseCurrentFile(zip);
				ERR_PRINT(vformat("PhysX: read failure while extracting '%s'.", name));
				result = ERR_FILE_CANT_READ;
				break;
			}
			if (n == 0) {
				break;
			}
			out->store_buffer(buf.ptr(), n);
			ctx.update(buf.ptr(), (size_t)n);
		}
		out.unref();
		unzCloseCurrentFile(zip);
		if (result != OK) {
			break;
		}

		// Hash what actually landed on disk and compare with the manifest.
		String got_hash = sha256_file(target);
		if (got_hash.to_lower() != String(want_hash).to_lower()) {
			dir->remove(name);
			ERR_PRINT(vformat("PhysX: extracted '%s' failed its SHA-256 check; removed.", name));
			result = ERR_FILE_CORRUPT;
			break;
		}
	}

	unzClose(zip);
	if (result != OK) {
		// Remove every file this call wrote so a failed install leaves no
		// half-updated mix of DLLs behind. Entries after the failing one were
		// never written, and files that were already there before this call
		// only disappear here if this call had actually replaced them.
		for (const String &name : written) {
			dir->remove(name);
		}
	}
	return result;
}
