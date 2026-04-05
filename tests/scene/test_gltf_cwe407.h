/**************************************************************************/
/*  test_gltf_cwe407.h                                                    */
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

// CWE-407 regression tests for redot-0008:
// GLTFState::extensions_used is now HashSet<String> — O(1) insert/has instead
// of O(E) Vector::has() called per node/animation track during import.
// A regression to Vector<String> reintroduces O(E²) cost when many extensions
// are referenced repeatedly across a large GLTF file.
//
// Assertions use get_extensions_required() and has_used_extension() — public
// accessors added alongside this fix. A regression removing the HashSet guard
// would cause duplicate entries in extensions_required and wrong has() results.


#include "modules/gltf/gltf_state.h"

#include "tests/test_macros.h"

namespace TestGLTFCWE407 {

// Unit: add_used_extension with the same name twice must not duplicate in extensions_required.
// With HashSet guard: extensions_required has exactly 1 entry after 3 required adds.
// Without guard (regression): extensions_required would have 3 entries.
TEST_CASE("[GLTFState][CWE-407] extensions_used: required=true adds to extensions_required once") {
	Ref<GLTFState> state;
	state.instantiate();

	// Three calls with required=true — extensions_required must have exactly 1 entry.
	state->add_used_extension("KHR_draco_mesh_compression", true);
	state->add_used_extension("KHR_draco_mesh_compression", true);
	state->add_used_extension("KHR_draco_mesh_compression", true);

	CHECK_MESSAGE(state->get_extensions_required().size() == 1,
			"extensions_required must have exactly 1 entry after 3 duplicate required adds.");
	CHECK_MESSAGE(state->has_used_extension("KHR_draco_mesh_compression"),
			"KHR_draco_mesh_compression must be tracked in extensions_used.");

	// Second required extension — extensions_required grows to 2.
	state->add_used_extension("KHR_mesh_quantization", true);
	state->add_used_extension("KHR_mesh_quantization", true);

	CHECK_MESSAGE(state->get_extensions_required().size() == 2,
			"extensions_required must have exactly 2 entries after adding a second required extension.");
	CHECK_MESSAGE(state->has_used_extension("KHR_mesh_quantization"),
			"KHR_mesh_quantization must be tracked in extensions_used.");

	// Optional extension — extensions_required stays at 2.
	state->add_used_extension("KHR_materials_unlit", false);

	CHECK_MESSAGE(state->get_extensions_required().size() == 2,
			"Optional extension must not appear in extensions_required.");
	CHECK_MESSAGE(state->has_used_extension("KHR_materials_unlit"),
			"KHR_materials_unlit must be tracked in extensions_used despite being optional.");
}

// Integration: multiple distinct extensions, each added once and once re-added.
// Verifies the HashSet correctly deduplicates across 5 unique entries after 10 add calls.
TEST_CASE("[GLTFState][CWE-407] extensions_used: multiple distinct extensions tracked") {
	Ref<GLTFState> state;
	state.instantiate();

	Vector<String> extensions = {
		"KHR_materials_unlit",
		"KHR_texture_transform",
		"KHR_draco_mesh_compression",
		"EXT_mesh_gpu_instancing",
		"KHR_mesh_quantization",
	};
	for (const String &ext : extensions) {
		state->add_used_extension(ext);
	}
	// Re-add all — HashSet must remain at 5 unique entries (no duplicates in required).
	for (const String &ext : extensions) {
		state->add_used_extension(ext);
	}

	// None were added as required — extensions_required must be empty.
	CHECK_MESSAGE(state->get_extensions_required().size() == 0,
			"No extension was added as required — extensions_required must be empty.");

	// All 5 must be tracked in extensions_used.
	for (const String &ext : extensions) {
		CHECK_MESSAGE(state->has_used_extension(ext),
				vformat("%s must be tracked in extensions_used.", ext));
	}

	// Upgrade one to required twice — extensions_required must have exactly 1 entry.
	state->add_used_extension("KHR_draco_mesh_compression", true);
	state->add_used_extension("KHR_draco_mesh_compression", true);

	CHECK_MESSAGE(state->get_extensions_required().size() == 1,
			"extensions_required must have exactly 1 entry after upgrading one extension to required.");

	// Optional re-add must not appear in extensions_required.
	state->add_used_extension("KHR_materials_unlit", false);
	CHECK_MESSAGE(state->get_extensions_required().size() == 1,
			"Optional add must not change extensions_required size.");
}

// Functional / complexity gate: N distinct extensions added, then all re-added.
// With HashSet: O(N) inserts, all O(1). With Vector::has(): O(N²/2) scans.
// At N=500, O(N²) ≈ 125K scans vs O(N) = 500. Regression is measurably slow.
TEST_CASE("[GLTFState][CWE-407][Stress] extensions_used: 500 distinct + re-add, O(N)") {
	Ref<GLTFState> state;
	state.instantiate();

	// Add 500 distinct extension names.
	constexpr int N = 500;
	for (int i = 0; i < N; i++) {
		state->add_used_extension(vformat("EXT_test_%d", i));
	}
	// Re-add all 500 — HashSet insert is O(1) each, total O(N).
	// With Vector::has(), this second pass would be O(N²/2) ≈ 125K scans.
	for (int i = 0; i < N; i++) {
		state->add_used_extension(vformat("EXT_test_%d", i));
	}

	// None were required — extensions_required must be empty.
	CHECK_MESSAGE(state->get_extensions_required().size() == 0,
			"No extension was added as required — extensions_required must be empty.");

	// Spot-check membership at both ends of the range.
	CHECK_MESSAGE(state->has_used_extension("EXT_test_0"),
			"EXT_test_0 must be tracked in extensions_used.");
	CHECK_MESSAGE(state->has_used_extension(vformat("EXT_test_%d", N - 1)),
			"Last extension must be tracked in extensions_used.");
	CHECK_MESSAGE(!state->has_used_extension("EXT_not_added"),
			"Unadded extension must not appear in extensions_used.");

	// Add one required — verify it lands exactly once.
	state->add_used_extension("KHR_materials_unlit", true);
	state->add_used_extension("KHR_materials_unlit", true);
	CHECK_MESSAGE(state->get_extensions_required().size() == 1,
			"extensions_required must have exactly 1 entry after 2 identical required adds.");
}

} // namespace TestGLTFCWE407
