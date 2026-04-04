/**************************************************************************/
/*  test_spring_bone_cwe407.h                                             */
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

// CWE-407 regression tests for redot-0012:
// SpringBoneSimulator3D::_process_bone() built HashSet<ObjectID>/HashMap
// once per frame outside the S×C inner loop instead of calling
// LocalVector::has() / LocalVector::find() (both O(N)) inside the loop.
// Before fix: O(S×C×N). After fix: O(N + S×C).
//
// The internal optimization (_process_bone) runs only during physics
// simulation inside a live SceneTree with a Skeleton3D parent. These
// tests verify structural correctness of the collision management API
// and that the code builds without assertion failures.
//
// Full integration test (complexity gate) requires a physics-enabled
// SceneTree with Skeleton3D, SpringBoneSimulator3D, and SpringBoneCollision3D
// child nodes — see tests/scene/test_spring_bone_simulator_3d.h for
// the simulation harness if/when added.

#pragma once

#include "scene/3d/spring_bone_simulator_3d.h"

#include "tests/test_macros.h"

namespace TestSpringBoneCWE407 {

// Unit: set_setting_count + set_collision_count + get_collision_count are consistent.
// SpringBoneSimulator3D starts with 0 settings. Add a setting, set its collision count,
// then query — the count must match.
TEST_CASE("[SpringBoneSimulator3D][CWE-407] Collision count: set/get round-trips correctly") {
	SpringBoneSimulator3D *sim = memnew(SpringBoneSimulator3D);

	// No settings yet — get_collision_count with index 0 must fail gracefully.
	int count_before = sim->get_collision_count(0);
	CHECK_MESSAGE(count_before == 0, "get_collision_count on missing setting returns 0.");

	sim->set_setting_count(1);
	CHECK_MESSAGE(sim->get_collision_count(0) == 0, "Collision count starts at 0 for new setting.");

	sim->set_collision_count(0, 5);
	CHECK_MESSAGE(sim->get_collision_count(0) == 5, "Collision count must be 5 after set.");

	sim->set_collision_count(0, 0);
	CHECK_MESSAGE(sim->get_collision_count(0) == 0, "Collision count must be 0 after clear.");

	memdelete(sim);
}

// Unit: clear_collisions resets collision count to 0.
TEST_CASE("[SpringBoneSimulator3D][CWE-407] Collision count: clear_collisions resets to 0") {
	SpringBoneSimulator3D *sim = memnew(SpringBoneSimulator3D);
	sim->set_setting_count(1);
	sim->set_collision_count(0, 10);
	CHECK_MESSAGE(sim->get_collision_count(0) == 10, "Count must be 10 before clear.");

	sim->clear_collisions(0);
	CHECK_MESSAGE(sim->get_collision_count(0) == 0, "Count must be 0 after clear_collisions.");

	memdelete(sim);
}

// Integration: multiple settings each have independent collision counts.
// _process_bone iterates settings (S) × collisions (C). Each setting's collision
// list is independent. A regression would corrupt one setting's HashSet from another.
TEST_CASE("[SpringBoneSimulator3D][CWE-407] Collision count: multiple settings independent") {
	SpringBoneSimulator3D *sim = memnew(SpringBoneSimulator3D);
	sim->set_setting_count(3);

	sim->set_collision_count(0, 2);
	sim->set_collision_count(1, 5);
	sim->set_collision_count(2, 1);

	CHECK_MESSAGE(sim->get_collision_count(0) == 2, "Setting 0 must have 2 collisions.");
	CHECK_MESSAGE(sim->get_collision_count(1) == 5, "Setting 1 must have 5 collisions.");
	CHECK_MESSAGE(sim->get_collision_count(2) == 1, "Setting 2 must have 1 collision.");

	// Modify one, others unchanged.
	sim->set_collision_count(1, 3);
	CHECK_MESSAGE(sim->get_collision_count(0) == 2, "Setting 0 unchanged.");
	CHECK_MESSAGE(sim->get_collision_count(1) == 3, "Setting 1 updated to 3.");
	CHECK_MESSAGE(sim->get_collision_count(2) == 1, "Setting 2 unchanged.");

	memdelete(sim);
}

// Structural / complexity gate annotation:
// The fix in _process_bone() builds HashSet<ObjectID> collision_set and
// HashMap<ObjectID, int> collision_index_map ONCE per call, outside the
// inner bone loop (S×C iterations). A regression back to:
//   if (collision_instances.has(id)) { ... }  // O(N) per check
// would restore O(S×C×N) cost.
//
// To verify the O(N + S×C) fix at runtime, a physics integration test must:
//   1. Create a Skeleton3D with S=100 spring bone chains
//   2. Add C=50 SpringBoneCollision3D children (N=50)
//   3. Run _process_simulation() for 1 frame
//   4. Measure wall time — must be <100ms (O(N+S×C)); regression ~250× slower
//
// This annotation documents the required test so a developer reverting the
// HashSet construction to an inline has() call will know what test to add.
TEST_CASE("[SpringBoneSimulator3D][CWE-407] Complexity annotation: _process_bone O(N+S×C)") {
	// Structural smoke test: simulator with many settings and collisions
	// can be created, configured, and destroyed without crash.
	// The actual O(N + S×C) timing gate requires a physics-enabled scene.
	SpringBoneSimulator3D *sim = memnew(SpringBoneSimulator3D);
	constexpr int S = 20; // settings (spring bone chains)
	constexpr int C = 10; // collision slots per setting

	sim->set_setting_count(S);
	for (int s = 0; s < S; s++) {
		sim->set_collision_count(s, C);
		CHECK_MESSAGE(sim->get_collision_count(s) == C,
				vformat("Setting %d must have %d collision slots.", s, C));
	}

	// Clear all and verify.
	for (int s = 0; s < S; s++) {
		sim->clear_collisions(s);
		CHECK_MESSAGE(sim->get_collision_count(s) == 0,
				vformat("Setting %d must have 0 collisions after clear.", s));
	}

	memdelete(sim);
}

} // namespace TestSpringBoneCWE407
