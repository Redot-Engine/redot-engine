/**************************************************************************/
/*  physx_export_plugin.cpp                                               */
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

#include "physx_export_plugin.h"

#include "physx_license.gen.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/version.h"
#include "editor/export/editor_export_platform.h"
#include "editor/export/editor_export_preset.h"
#include "editor/file_system/editor_paths.h"

namespace {

struct PhysXRuntimeLibrary {
	const char *windows_name;
	const char *linux_name;
	// What the exported game loses without it, shown in the export warning.
	const char *missing_effect;
};

} //namespace

void PhysXExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	const bool windows = p_features.has("windows");
	const bool is_linux = p_features.has("linux");
	if (!windows && !is_linux) {
		return; // The module's runtime libraries are only built for Windows and Linux.
	}

	LocalVector<PhysXRuntimeLibrary> libraries;
#ifdef GODOT_PHYSX_BLAST
	const char *blast_effect = "Blast destruction (PhysXDestructible3D) cannot work and the game may fail to start";
	libraries.push_back({ "NvBlast.dll", "libNvBlast.so", blast_effect });
	libraries.push_back({ "NvBlastGlobals.dll", "libNvBlastGlobals.so", blast_effect });
	libraries.push_back({ "NvBlastExtAssetUtils.dll", "libNvBlastExtAssetUtils.so", blast_effect });
	libraries.push_back({ "NvBlastExtAuthoring.dll", "libNvBlastExtAuthoring.so", blast_effect });
	libraries.push_back({ "NvBlastExtSerialization.dll", "libNvBlastExtSerialization.so", blast_effect });
	libraries.push_back({ "NvBlastExtShaders.dll", "libNvBlastExtShaders.so", blast_effect });
	libraries.push_back({ "NvBlastExtStress.dll", "libNvBlastExtStress.so", blast_effect });
	libraries.push_back({ "NvBlastExtTkSerialization.dll", "libNvBlastExtTkSerialization.so", blast_effect });
	libraries.push_back({ "NvBlastTk.dll", "libNvBlastTk.so", blast_effect });
#endif
#ifdef GODOT_PHYSX_GPU
	libraries.push_back({ "PhysXGpu_64.dll", "libPhysXGpu_64.so", "PhysX runs on the CPU without GPU dynamics" });
#endif
#ifdef GODOT_PHYSX_FLOW
	const char *flow_effect = "the NVIDIA Flow nodes (PhysXFlowSimulation3D, emitters) stay inert";
	libraries.push_back({ "nvflow.dll", "libnvflow.so", flow_effect });
	libraries.push_back({ "nvflowext.dll", "libnvflowext.so", flow_effect });
#endif

	// The libraries must come from the same build as the export template, for
	// the template's platform: the custom template's folder, else this editor
	// version's templates folder.
	String template_dir;
	const Ref<EditorExportPreset> preset = get_export_preset();
	if (preset.is_valid()) {
		const String custom = preset->get(p_debug ? "custom_template/debug" : "custom_template/release");
		if (!custom.is_empty()) {
			template_dir = custom.get_base_dir();
		}
	}
	if (template_dir.is_empty()) {
		template_dir = EditorPaths::get_singleton()->get_export_templates_dir().path_join(GODOT_VERSION_FULL_CONFIG);
	}

	// One warning per effect (Blast has nine libraries), naming the missing
	// files, what the game loses and where they belong.
	const Ref<EditorExportPlatform> platform = get_export_platform();
	LocalVector<String> missing_effects;
	LocalVector<PackedStringArray> missing_files;
	for (const PhysXRuntimeLibrary &library : libraries) {
		const String path = template_dir.path_join(windows ? library.windows_name : library.linux_name);
		if (FileAccess::exists(path)) {
			add_shared_object(path, Vector<String>(), String());
			continue;
		}
		int64_t effect_index = missing_effects.find(String(library.missing_effect));
		if (effect_index < 0) {
			missing_effects.push_back(String(library.missing_effect));
			missing_files.push_back(PackedStringArray());
			effect_index = missing_effects.size() - 1;
		}
		missing_files[effect_index].push_back(path.get_file());
	}
	if (platform.is_valid()) {
		for (uint32_t i = 0; i < missing_effects.size(); i++) {
			const bool plural = missing_files[i].size() > 1;
			platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_WARNING, "PhysX",
					vformat("Not found next to the export template, so not shipped: %s. Without %s, %s. Copy %s into '%s' from the build that produced the template (see the PhysX module README, \"Exporting a game\").",
							String(", ").join(missing_files[i]),
							plural ? "them" : "it",
							missing_effects[i],
							plural ? "them" : "it",
							template_dir));
		}
	}

	// The PhysX SDK is compiled into the template and NvBlast / PhysXGpu /
	// nvflow ship as binaries: BSD-3-Clause requires the notice to go with
	// them. Written to the cache dir so an installed editor without a source
	// tree can still ship it.
	const String license_path = EditorPaths::get_singleton()->get_cache_dir().path_join("physx").path_join("PHYSX-LICENSE.txt");
	DirAccess::make_dir_recursive_absolute(license_path.get_base_dir());
	Ref<FileAccess> license_file = FileAccess::open(license_path, FileAccess::WRITE);
	if (license_file.is_valid()) {
		license_file->store_string(String::utf8(PHYSX_LICENSE_TEXT));
		license_file.unref();
		add_shared_object(license_path, Vector<String>(), String());
	}
}
