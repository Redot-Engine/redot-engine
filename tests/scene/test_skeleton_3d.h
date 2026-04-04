/**************************************************************************/
/*  test_skeleton_3d.h                                                    */
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

#include "tests/test_macros.h"

#include "scene/3d/skeleton_3d.h"

namespace TestSkeleton3D {

TEST_CASE("[Skeleton3D] Test per-bone meta") {
	Skeleton3D *skeleton = memnew(Skeleton3D);
	skeleton->add_bone("root");
	skeleton->set_bone_rest(0, Transform3D());

	// Adding meta to bone.
	skeleton->set_bone_meta(0, "key1", "value1");
	skeleton->set_bone_meta(0, "key2", 12345);
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Rename bone and check if meta persists.
	skeleton->set_bone_name(0, "renamed_root");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key1") == "value1", "Bone meta missing.");
	CHECK_MESSAGE(skeleton->get_bone_meta(0, "key2") == Variant(12345), "Bone meta missing.");

	// Retrieve list of keys.
	List<StringName> keys;
	skeleton->get_bone_meta_list(0, &keys);
	CHECK_MESSAGE(keys.size() == 2, "Wrong number of bone meta keys.");
	CHECK_MESSAGE(keys.find("key1"), "key1 not found in bone meta list");
	CHECK_MESSAGE(keys.find("key2"), "key2 not found in bone meta list");

	// Removing meta.
	skeleton->set_bone_meta(0, "key1", Variant());
	skeleton->set_bone_meta(0, "key2", Variant());
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key1"), "Bone meta key1 should be deleted.");
	CHECK_MESSAGE(!skeleton->has_bone_meta(0, "key2"), "Bone meta key2 should be deleted.");
	List<StringName> should_be_empty_keys;
	skeleton->get_bone_meta_list(0, &should_be_empty_keys);
	CHECK_MESSAGE(should_be_empty_keys.size() == 0, "Wrong number of bone meta keys.");

	// Deleting non-existing key should succeed.
	skeleton->set_bone_meta(0, "non-existing-key", Variant());
	memdelete(skeleton);
}
// CWE-407 regression tests for redot-0006:
// Skeleton3D::BoneData::child_bones is now HashSet<int> — O(1) has() instead of
// O(B) Vector::find() called inside an O(B) loop, which was O(B²) total.
// A regression to Vector<int> still produces correct results but silently
// reintroduces O(B²) cost in set_bone_parent / _update_process_order.

TEST_CASE("[Skeleton3D][CWE-407] child_bones: set_bone_parent does not duplicate") {
	// _update_process_order inserts into child_bones_set (HashSet) before child_bones (Vector).
	// The HashSet.has() guard prevents duplicates. A regression that removes the guard
	// would produce duplicate entries in get_bone_children().
	// This test calls set_bone_parent twice and verifies exactly 1 child exists.
	Skeleton3D *skeleton = memnew(Skeleton3D);
	skeleton->add_bone("root");    // bone 0
	skeleton->add_bone("child");   // bone 1

	skeleton->set_bone_rest(0, Transform3D());
	skeleton->set_bone_rest(1, Transform3D());

	// Set parent twice — second call must not duplicate.
	skeleton->set_bone_parent(1, 0);
	skeleton->set_bone_parent(1, 0); // triggers _update_process_order again

	Vector<int> children = skeleton->get_bone_children(0);
	CHECK_MESSAGE(children.size() == 1, "Root must have exactly 1 child despite double set_bone_parent.");
	CHECK_MESSAGE(children[0] == 1, "Child bone index must be 1.");

	memdelete(skeleton);
}

TEST_CASE("[Skeleton3D][CWE-407] child_bones: tree of bones, each has correct children") {
	// root → A → B, root → C
	// After process order update: root has children {A, C}, A has child {B}.
	Skeleton3D *skeleton = memnew(Skeleton3D);
	int root = skeleton->add_bone("root");  // 0
	int a    = skeleton->add_bone("A");     // 1
	int b    = skeleton->add_bone("B");     // 2
	int c    = skeleton->add_bone("C");     // 3

	skeleton->set_bone_rest(root, Transform3D());
	skeleton->set_bone_rest(a,    Transform3D());
	skeleton->set_bone_rest(b,    Transform3D());
	skeleton->set_bone_rest(c,    Transform3D());

	skeleton->set_bone_parent(a, root);
	skeleton->set_bone_parent(b, a);
	skeleton->set_bone_parent(c, root);

	Vector<int> root_children = skeleton->get_bone_children(root);
	CHECK_MESSAGE(root_children.size() == 2, "Root must have 2 children.");
	CHECK_MESSAGE(root_children.has(a), "Root children must include A.");
	CHECK_MESSAGE(root_children.has(c), "Root children must include C.");

	Vector<int> a_children = skeleton->get_bone_children(a);
	CHECK_MESSAGE(a_children.size() == 1, "A must have 1 child.");
	CHECK_MESSAGE(a_children[0] == b, "A's child must be B.");

	Vector<int> b_children = skeleton->get_bone_children(b);
	CHECK_MESSAGE(b_children.size() == 0, "B must be a leaf.");

	memdelete(skeleton);
}

TEST_CASE("[Skeleton3D][CWE-407][Stress] child_bones: 500-bone chain, each parent has 1 child") {
	// Linear chain: bone 0 → 1 → 2 → ... → 499.
	// Each set_bone_parent triggers _update_process_order which walks child_bones.
	// With HashSet: O(B) total. With Vector::has(): O(B²) ≈ 250K scans.
	constexpr int N = 500;
	Skeleton3D *skeleton = memnew(Skeleton3D);
	for (int i = 0; i < N; i++) {
		skeleton->add_bone(vformat("bone_%d", i));
		skeleton->set_bone_rest(i, Transform3D());
	}
	for (int i = 1; i < N; i++) {
		skeleton->set_bone_parent(i, i - 1);
	}
	// Verify each bone has exactly 1 child (except the last).
	for (int i = 0; i < N - 1; i++) {
		Vector<int> children = skeleton->get_bone_children(i);
		CHECK_MESSAGE(children.size() == 1, vformat("Bone %d must have exactly 1 child.", i));
	}
	Vector<int> last_children = skeleton->get_bone_children(N - 1);
	CHECK_MESSAGE(last_children.size() == 0, "Last bone must be a leaf.");

	memdelete(skeleton);
}

} // namespace TestSkeleton3D
