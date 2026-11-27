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

#ifdef __linux__
// dl_iterate_phdr (link.h) is a GNU extension: define before any libc header
// so features.h picks it up.
#define _GNU_SOURCE
#endif

#include "physx_runtime_manager.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>

#include <delayimp.h>
#include <tlhelp32.h>
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
#elif defined(__linux__)
#include <dlfcn.h>
#include <link.h>
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

// Version/pending-swap/file-list markers next to the runtime DLLs (see the
// header).
static const char *VERSION_MARKER_NAME = ".physx-runtime-version";
static const char *PENDING_MARKER_NAME = ".physx-runtime-pending";
// The installed files' expected hashes, written by extract_bundle next to the
// libraries: the offline source for the integrity checks (a repo-staged or
// manually installed runtime has none, and can only be verified once a
// manifest has been fetched).
static const char *FILE_LIST_NAME = ".physx-runtime-files";

static String runtime_exe_dir() {
	return OS::get_singleton()->get_executable_path().get_base_dir();
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

Dictionary PhysXRuntimeManager::get_installed_file_list() {
	Ref<FileAccess> f = FileAccess::open(runtime_exe_dir().path_join(FILE_LIST_NAME), FileAccess::READ);
	if (f.is_null()) {
		return Dictionary();
	}
	JSON json;
	if (json.parse(f->get_as_text()) != OK) {
		return Dictionary();
	}
	const Variant parsed = json.get_data();
	if (parsed.get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	Dictionary list = parsed;
	if (!list.has("files") || list["files"].get_type() != Variant::DICTIONARY) {
		return Dictionary();
	}
	return list;
}

// Session cache of the last parsed manifest's file hashes (the editor plugin
// feeds it after a version check); takes precedence over the installed file
// list until the next manifest is parsed.
static HashMap<String, String> &runtime_manifest_hashes() {
	static HashMap<String, String> cache;
	return cache;
}

void PhysXRuntimeManager::set_manifest_file_hashes(const Dictionary &p_bundle) {
	HashMap<String, String> &cache = runtime_manifest_hashes();
	cache.clear();
	const Array files = p_bundle.get("files", Array());
	for (int i = 0; i < files.size(); i++) {
		const Dictionary entry = files[i];
		const String name = entry.get("name", String());
		const String hash = String(entry.get("sha256", String())).to_lower();
		if (!name.is_empty() && hash.length() == 64) {
			cache[name] = hash;
		}
	}
}

PackedStringArray PhysXRuntimeManager::get_stale_runtime_files() {
	// Present-but-wrong content: the failure class existence checks cannot see
	// (a same-named leftover from another project shadows the real library at
	// load time). Expected hashes come from the manifest parsed this session,
	// or, offline, from the file list the last install wrote.
	PackedStringArray stale;
	HashMap<String, String> want = runtime_manifest_hashes();
	if (want.is_empty()) {
		const Dictionary list = get_installed_file_list();
		if (!list.is_empty()) {
			const Dictionary files = list["files"];
			for (const KeyValue<Variant, Variant> &kv : files) {
				const String hash = String(kv.value).to_lower();
				if (hash.length() == 64) {
					want[String(kv.key)] = hash;
				}
			}
		}
	}
	if (want.is_empty()) {
		return stale; // no known-good hashes: nothing can be verified offline
	}
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	for (const String &name : get_required_runtime_files()) {
		const String *expected = want.getptr(name);
		if (expected == nullptr) {
			continue; // not in the known-good set: unverifiable
		}
		const String path = exe_dir.path_join(name);
		if (!FileAccess::exists(path)) {
			continue; // absent: get_missing_runtime_files() reports that
		}
		const String got = sha256_file(path);
		if (got.is_empty() || got.to_lower() != *expected) {
			stale.push_back(name);
		}
	}
	return stale;
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
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("get_stale_runtime_files"), &PhysXRuntimeManager::get_stale_runtime_files);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("is_runtime_complete"), &PhysXRuntimeManager::is_runtime_complete);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("install_missing_files"), &PhysXRuntimeManager::install_missing_files);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("parse_manifest", "json_text"), &PhysXRuntimeManager::parse_manifest);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("sha256_file", "path"), &PhysXRuntimeManager::sha256_file);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("extract_bundle", "zip_path", "bundle", "target_dir"), &PhysXRuntimeManager::extract_bundle);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("get_installed_runtime_version"), &PhysXRuntimeManager::get_installed_runtime_version);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("get_installed_file_list"), &PhysXRuntimeManager::get_installed_file_list);
	ClassDB::bind_static_method("PhysXRuntimeManager", D_METHOD("complete_pending_swaps"), &PhysXRuntimeManager::complete_pending_swaps);
}

const char *PhysXRuntimeManager::MANIFEST_URL =
		"https://raw.githubusercontent.com/Vlado-VG/redot-physx-runtime/main/manifest.json";

const char *PhysXRuntimeManager::RELEASES_PAGE_URL =
		"https://github.com/Vlado-VG/redot-physx-runtime/releases";

// Keep in lockstep with the runtime repo's manifest "runtime_version" (both
// change together with the vendored SDK under thirdparty/physx): the updater
// offers/adopts only bundles whose runtime_version equals this.
const char *PhysXRuntimeManager::LINKED_RUNTIME_VERSION = "5.12.0";

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
	// Surface the manifest's human version (e.g. "5.12.0") on the bundle: the
	// updater compares it against the installed-version marker. Absent on
	// older manifests -- the updater then skips the comparison.
	bundle["runtime_version"] = String(manifest.get("runtime_version", ""));
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
	// Temporary (.part) names this call wrote, for the failure cleanup below.
	PackedStringArray written;
	// Names whose rename into place failed because the target is in use: they
	// stay as verified .part files and are applied at the next editor start
	// (see complete_pending_swaps()).
	PackedStringArray pending;
	// Extract manifest-listed files one by one, hashing each as it lands so a
	// corrupted download is caught before the editor ever uses it. On any
	// failure the temporary files this call wrote are removed; the previously
	// installed files (and the ones this call skipped as intact) stay.
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

		// Already installed and intact: never touch a library the running
		// editor may have mapped or loaded. Truncating a mapped .so SIGBUSes
		// the process, and a loaded .dll cannot be reopened for writing at
		// all -- the partial-install flow (only some files missing) used to
		// fail on exactly the files it did not need to rewrite.
		if (!want_hash.is_empty() && FileAccess::exists(target) &&
				sha256_file(target).to_lower() == String(want_hash).to_lower()) {
			continue;
		}
		// Everything else lands at a temporary name and is renamed into place
		// after its hash verifies: on Linux the rename swaps the inode
		// atomically (the editor keeps its mapping of the old library), and a
		// crashed run leaves at worst a stale .part behind.
		const String tmp_name = name + ".part";
		const String tmp_target = p_target_dir.path_join(tmp_name);

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

		Ref<FileAccess> out = FileAccess::open(tmp_target, FileAccess::WRITE);
		if (out.is_null()) {
			unzCloseCurrentFile(zip);
			ERR_PRINT(vformat("PhysX: cannot write '%s'.", tmp_target));
			result = ERR_FILE_CANT_WRITE;
			break;
		}
		written.push_back(tmp_name);

		CryptoCore::SHA256Context ctx;
		ctx.start();
		LocalVector<uint8_t> buf;
		buf.resize(1024 * 1024);
		while (true) {
			int n = unzReadCurrentFile(zip, buf.ptr(), (unsigned)buf.size());
			if (n < 0) {
				out.unref();
				dir->remove(tmp_name);
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
		String got_hash = sha256_file(tmp_target);
		if (got_hash.to_lower() != String(want_hash).to_lower()) {
			dir->remove(tmp_name);
			ERR_PRINT(vformat("PhysX: extracted '%s' failed its SHA-256 check; removed.", name));
			result = ERR_FILE_CORRUPT;
			break;
		}

		if (dir->rename(tmp_name, name) != OK) {
			// The target's name is in use -- on Windows a loaded DLL keeps it
			// until its handle closes (the DLLs are delay-loaded, so this only
			// happens when GPU dynamics/Flow already ran this session). Keep
			// the VERIFIED .part and record it: complete_pending_swaps() moves
			// it into place at the next editor start, before any physics has
			// had a chance to load the library. The install itself succeeds --
			// the restart the flow performs is exactly the fresh start needed.
			pending.push_back(name);
			print_line(vformat("PhysX: '%s' is in use -- the update applies at the next editor start.", name));
			continue;
		}
	}

	unzClose(zip);
	if (result != OK) {
		// Remove every temporary file this call wrote so a failed install
		// leaves no half-updated mix of DLLs behind. Previously installed
		// files were never touched (the rename into place is the last step
		// per file), so they stay intact. A pending entry's .part is removed
		// too: the install failed, so no restart-completion is registered.
		for (const String &name : written) {
			dir->remove(name);
		}
		return result;
	}
	// Record the installed files' expected hashes next to them:
	// get_installed_file_list() feeds get_stale_runtime_files() and the
	// loaded-library diagnostic, so a same-named library whose content no
	// longer matches is detected and re-offered instead of silently
	// shadowing the good one at load time.
	{
		Dictionary file_list;
		file_list["runtime_version"] = String(p_bundle.get("runtime_version", ""));
		Dictionary hashes;
		for (int i = 0; i < files.size(); i++) {
			const Dictionary entry = files[i];
			hashes[entry.get("name", String())] = String(entry.get("sha256", String()));
		}
		file_list["files"] = hashes;
		Ref<FileAccess> ff = FileAccess::open(p_target_dir.path_join(FILE_LIST_NAME), FileAccess::WRITE);
		if (ff.is_valid()) {
			ff->store_string(JSON::stringify(file_list, "  "));
		} else {
			WARN_PRINT("PhysX: could not write the runtime file list; offline integrity checks will be unavailable until the next install.");
		}
	}
	if (!pending.is_empty()) {
		// Register the deferred renames; complete_pending_swaps() applies
		// them at the next editor start -- from the executable's directory
		// only, so extracting elsewhere warns instead of stranding a marker
		// that is never read.
		if (p_target_dir.simplify_path() != runtime_exe_dir().simplify_path()) {
			WARN_PRINT(vformat("PhysX: extract_bundle target '%s' is not the editor executable's directory; its pending-swap marker will not be applied automatically at startup.", p_target_dir));
		}
		Ref<FileAccess> pf = FileAccess::open(p_target_dir.path_join(PENDING_MARKER_NAME), FileAccess::WRITE);
		if (pf.is_valid()) {
			for (const String &name : pending) {
				pf->store_line(name);
			}
		} else {
			// The marker could not be written: the pending .part files would
			// never be applied. Clean them instead of stranding them.
			for (const String &name : pending) {
				dir->remove(name + ".part");
			}
			return ERR_FILE_CANT_WRITE;
		}
	}
	return result;
}

// ---------------------------------------------------------------------------
// Installed-version tracking (the updater)
// ---------------------------------------------------------------------------

String PhysXRuntimeManager::get_installed_runtime_version() {
	Ref<FileAccess> f = FileAccess::open(runtime_exe_dir().path_join(VERSION_MARKER_NAME), FileAccess::READ);
	if (f.is_null()) {
		return String();
	}
	return f->get_line().strip_edges();
}

void PhysXRuntimeManager::write_runtime_version_marker(const String &p_version) {
	if (p_version.is_empty()) {
		return; // a manifest without runtime_version leaves the marker alone
	}
	const String path = runtime_exe_dir().path_join(VERSION_MARKER_NAME);
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_valid()) {
		f->store_line(p_version);
	} else {
		// A read-only install directory can never persist the marker, so
		// every session would re-verify the whole bundle by hash. Name the
		// cause instead of silently degrading into a per-session stall.
		ERR_PRINT(vformat("PhysX: cannot write the runtime version marker '%s' (read-only install directory?). The update check will re-verify the installed files each session.", path));
	}
}

void PhysXRuntimeManager::complete_pending_swaps() {
	const String marker_path = runtime_exe_dir().path_join(PENDING_MARKER_NAME);
	if (!FileAccess::exists(marker_path)) {
		return;
	}
	Ref<DirAccess> dir = DirAccess::open(runtime_exe_dir());
	if (dir.is_null()) {
		return; // retried at the next start
	}
	PackedStringArray remaining;
	{
		Ref<FileAccess> f = FileAccess::open(marker_path, FileAccess::READ);
		if (f.is_null()) {
			return;
		}
		while (!f->eof_reached()) {
			const String name = f->get_line().strip_edges();
			if (name.is_empty()) {
				continue;
			}
			// Re-validate on read (the marker is self-written, but a corrupt
			// or hand-edited line must not aim a rename outside this
			// directory -- the same contract extract_bundle enforces when it
			// writes). Invalid entries are dropped from the rewritten marker
			// below, not retried forever. Entries always belong to the most
			// recent install's bundle: extract_bundle rewrites the whole
			// marker, so a restructured runtime cannot resurrect files it no
			// longer ships.
			if (name.contains("/") || name.contains("\\") || name.contains("..")) {
				ERR_PRINT(vformat("PhysX: invalid pending-swap entry '%s'; dropped.", name));
				continue;
			}
			const String part_name = name + ".part";
			if (!FileAccess::exists(runtime_exe_dir().path_join(part_name))) {
				continue; // nothing to apply (already swapped or cleaned up)
			}
			const String target_path = runtime_exe_dir().path_join(name);
			// Move the old image aside first: on Windows a loaded DLL keeps its
			// name until its handle closes (the loader opens with delete-share,
			// so the rename itself succeeds even while mapped -- the leftover
			// .old is removed below, or at a later start).
			const String old_name = name + ".old";
			const bool moved_aside = FileAccess::exists(target_path) && dir->rename(name, old_name) == OK;
			if (dir->rename(part_name, name) != OK) {
				// Roll the aside-move back: without it the library would sit
				// MISSING under its real name for the whole session (the next
				// delay-load would fail until a restart). The verified .part
				// stays and is retried at the next start either way.
				if (moved_aside) {
					dir->rename(old_name, name);
				}
				remaining.push_back(name); // still locked -- retried next start
				continue;
			}
			// Best effort: the .old goes away here, or on a later start.
			dir->remove(old_name);
			print_line(vformat("PhysX: runtime update for '%s' applied -- restart the editor once more if it was in use.", name));
		}
	}
	if (remaining.is_empty()) {
		dir->remove(PENDING_MARKER_NAME);
	} else {
		Ref<FileAccess> w = FileAccess::open(marker_path, FileAccess::WRITE);
		if (w.is_valid()) {
			for (const String &name : remaining) {
				w->store_line(name);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Loaded-runtime-library diagnostic
//
// Logs which file the loader actually resolved each required runtime library
// from, with its hash compared against the known-good hashes when available.
// This is the evidence trail for the shadowing failure class: on Linux a
// stale libNvBlastExtShaders.so on LD_LIBRARY_PATH or in a system directory
// is searched BEFORE the binary's own $ORIGIN folder, and the editor then
// dies at exec with "undefined symbol: NvBlastExtFalloffSubgraphShader" even
// though the correct copy sits next to it. Enumerating + hashing loaded
// libraries is read-only, so the editor plugin runs this on its integrity
// probe thread rather than the main thread.
// ---------------------------------------------------------------------------

#ifdef __linux__
struct RuntimeLibScan {
	const HashSet<String> *wanted;
	PackedStringArray *found;
};

static int runtime_lib_phdr_callback(struct dl_phdr_info *p_info, size_t, void *p_data) {
	RuntimeLibScan *scan = static_cast<RuntimeLibScan *>(p_data);
	if (p_info->dlpi_name == nullptr || p_info->dlpi_name[0] == '\0') {
		return 0;
	}
	const String base = String::utf8(p_info->dlpi_name).get_file();
	if (scan->wanted->has(base)) {
		scan->found->push_back(String::utf8(p_info->dlpi_name));
	}
	return 0;
}

static PackedStringArray loaded_runtime_library_paths(const HashSet<String> &p_wanted) {
	PackedStringArray found;
	RuntimeLibScan scan;
	scan.wanted = &p_wanted;
	scan.found = &found;
	dl_iterate_phdr(runtime_lib_phdr_callback, &scan);
	return found;
}
#elif defined(WINDOWS_ENABLED)
static PackedStringArray loaded_runtime_library_paths(const HashSet<String> &p_wanted) {
	// Windows file names are case-insensitive; delay-loaded DLLs only appear
	// once something has actually loaded them.
	HashSet<String> wanted_lower;
	for (const String &want : p_wanted) {
		wanted_lower.insert(want.to_lower());
	}
	PackedStringArray found;
	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
	if (snapshot == INVALID_HANDLE_VALUE) {
		return found;
	}
	MODULEENTRY32W entry;
	ZeroMemory(&entry, sizeof(entry));
	entry.dwSize = sizeof(entry);
	if (Module32FirstW(snapshot, &entry)) {
		do {
			if (wanted_lower.has(String(entry.szModule).to_lower())) {
				found.push_back(String(entry.szExePath));
			}
		} while (Module32NextW(snapshot, &entry));
	}
	CloseHandle(snapshot);
	return found;
}
#else
static PackedStringArray loaded_runtime_library_paths(const HashSet<String> &) {
	return PackedStringArray();
}
#endif

void PhysXRuntimeManager::log_loaded_runtime_libraries(const HashMap<String, String> &p_expected) {
	const PackedStringArray required = get_required_runtime_files();
	if (required.is_empty()) {
		return; // no runtime libraries staged on this platform/configuration
	}
	HashSet<String> wanted;
	for (const String &name : required) {
		wanted.insert(name);
	}
	const PackedStringArray loaded = loaded_runtime_library_paths(wanted);
	for (const String &path : loaded) {
		const String name = path.get_file();
		const String got = sha256_file(path);
		const String *expected = p_expected.getptr(name);
		if (expected == nullptr) {
			print_line(vformat("PhysX: runtime library '%s' loaded from '%s' (sha256 %s).", name, path, got));
		} else if (!got.is_empty() && got.to_lower() == expected->to_lower()) {
			print_line(vformat("PhysX: runtime library '%s' loaded from '%s' (sha256 verified).", name, path));
		} else {
			WARN_PRINT(vformat("PhysX: runtime library '%s' is loaded from '%s' but its sha256 (%s) does not match the known-good hash (%s) -- it may be stale or shadowed; see the PhysX module README, \"Diagnosing runtime problems\".", name, path, got, *expected));
		}
	}
}
