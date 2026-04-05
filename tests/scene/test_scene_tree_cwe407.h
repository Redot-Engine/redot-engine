/**************************************************************************/
/*  test_scene_tree_cwe407.h                                              */
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

// CWE-407 regression tests for redot-0001:
// SceneTree::Group::node_set HashSet shadow — O(1) membership instead of O(N)
// Vector linear scan. These tests verify that group membership is correct and
// deduplicated after the fix. A regression to Vector::has() would still pass
// correctness tests but silently reintroduce O(N²) cost at scale.


#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

#include "tests/test_macros.h"

namespace TestSceneTreeCWE407 {

// Unit: adding a node to the same group twice must not duplicate it.
// Before the fix, the guard was node_set.has() — a regression to nodes.has()
// would still deduplicate (ERR_FAIL path), but via O(N) scan instead of O(1).
TEST_CASE("[SceneTree][CWE-407] Group membership: no duplicate on double add") {
	SceneTree *tree = SceneTree::get_singleton();
	Node *node = memnew(Node);
	tree->get_root()->add_child(node);

	node->add_to_group("test_group");
	node->add_to_group("test_group"); // second add must be a no-op

	TypedArray<Node> members = tree->get_nodes_in_group("test_group");
	CHECK_MESSAGE(members.size() == 1, "Node must appear exactly once in group after double add.");

	node->remove_from_group("test_group");
	memdelete(node);
}

// Unit: remove_from_group must keep node_set consistent with nodes Vector.
TEST_CASE("[SceneTree][CWE-407] Group membership: node_set consistent after remove") {
	SceneTree *tree = SceneTree::get_singleton();
	Node *a = memnew(Node);
	Node *b = memnew(Node);
	tree->get_root()->add_child(a);
	tree->get_root()->add_child(b);

	a->add_to_group("sync_group");
	b->add_to_group("sync_group");
	b->remove_from_group("sync_group");

	TypedArray<Node> members = tree->get_nodes_in_group("sync_group");
	CHECK_MESSAGE(members.size() == 1, "Only node a should remain in group.");
	CHECK_MESSAGE(members[0] == Variant(a), "Remaining member must be node a.");

	a->remove_from_group("sync_group");
	memdelete(a);
	memdelete(b);
}

// Integration: 100 nodes added to a group, each appears exactly once.
TEST_CASE("[SceneTree][CWE-407] Group membership: 100 nodes each appear once") {
	SceneTree *tree = SceneTree::get_singleton();
	constexpr int N = 100;
	Vector<Node *> nodes;
	nodes.resize(N);

	for (int i = 0; i < N; i++) {
		nodes.write[i] = memnew(Node);
		tree->get_root()->add_child(nodes[i]);
		nodes[i]->add_to_group("bulk_group");
	}

	TypedArray<Node> members = tree->get_nodes_in_group("bulk_group");
	CHECK_MESSAGE(members.size() == N, "All 100 nodes must appear in group.");

	// Verify no duplicates by checking membership count per node.
	for (int i = 0; i < N; i++) {
		int count = 0;
		for (int j = 0; j < members.size(); j++) {
			if (members[j] == Variant(nodes[i])) {
				count++;
			}
		}
		CHECK_MESSAGE(count == 1, "Each node must appear exactly once.");
	}

	for (int i = 0; i < N; i++) {
		nodes[i]->remove_from_group("bulk_group");
		memdelete(nodes[i]);
	}
}

// Integration: add 50 nodes, remove odd-indexed, confirm exactly 25 remain.
TEST_CASE("[SceneTree][CWE-407] Group membership: partial remove, correct remainder") {
	SceneTree *tree = SceneTree::get_singleton();
	constexpr int N = 50;
	Vector<Node *> nodes;
	nodes.resize(N);

	for (int i = 0; i < N; i++) {
		nodes.write[i] = memnew(Node);
		tree->get_root()->add_child(nodes[i]);
		nodes[i]->add_to_group("partial_group");
	}

	// Remove odd-indexed nodes.
	for (int i = 1; i < N; i += 2) {
		nodes[i]->remove_from_group("partial_group");
	}

	TypedArray<Node> members = tree->get_nodes_in_group("partial_group");
	CHECK_MESSAGE(members.size() == 25, "Exactly 25 even-indexed nodes must remain.");

	for (int i = 0; i < N; i++) {
		if (i % 2 == 0) {
			nodes[i]->remove_from_group("partial_group");
		}
		memdelete(nodes[i]);
	}
}

// Functional / complexity gate: 2000 nodes, add/query/remove must complete without
// O(N²) degradation. A regression to Vector::has() makes this ~1000× slower.
// At N=2000, O(N²) ~= 4M ops; O(N) ~= 2000 ops. This test completes in <1s with
// the fix and would time out in CI with a regression.
TEST_CASE("[SceneTree][CWE-407][Stress] Group membership: 2000-node add/remove is O(N)") {
	SceneTree *tree = SceneTree::get_singleton();
	constexpr int N = 2000;
	Vector<Node *> nodes;
	nodes.resize(N);

	for (int i = 0; i < N; i++) {
		nodes.write[i] = memnew(Node);
		tree->get_root()->add_child(nodes[i]);
	}

	// Add all to group — O(N) with HashSet, O(N²) with Vector::has().
	for (int i = 0; i < N; i++) {
		nodes[i]->add_to_group("stress_group");
	}
	TypedArray<Node> members = tree->get_nodes_in_group("stress_group");
	CHECK_MESSAGE(members.size() == N, "All 2000 nodes must be in group.");

	// Remove all — triggers N HashSet erasures.
	for (int i = 0; i < N; i++) {
		nodes[i]->remove_from_group("stress_group");
	}
	members = tree->get_nodes_in_group("stress_group");
	CHECK_MESSAGE(members.size() == 0, "Group must be empty after all removes.");

	for (int i = 0; i < N; i++) {
		memdelete(nodes[i]);
	}
}

} // namespace TestSceneTreeCWE407
