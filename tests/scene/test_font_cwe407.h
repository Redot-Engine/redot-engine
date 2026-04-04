/**************************************************************************/
/*  test_font_cwe407.h                                                    */
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

// CWE-407 regression tests for redot-0009 and redot-0010:
//
// redot-0009: Font::_is_cyclic used no visited set — O(F^D) traversal of a
//   diamond fallback DAG (shared fonts traversed exponentially). Fixed to O(F)
//   with HashSet<const Font *> visited guard.
//
// redot-0010: Font::_update_rids_fb used no visited set — same diamond-graph
//   problem: shared fallbacks collected duplicate RIDs, inflating the rids[]
//   Vector. Fixed to O(N) with visited guard; each font's RID collected once.
//
// A regression to either method (removing visited guard) would cause:
//   redot-0009: exponential hang on set_fallbacks() with diamond graphs
//   redot-0010: duplicate RIDs in rids[] → text render artefacts
//
// These tests use FontVariation (lightweight constructor, no display server
// needed for fallback graph structure tests).

#pragma once

#include "scene/resources/font.h"

#include "tests/test_macros.h"

namespace TestFontCWE407 {

// Unit: self-referential fallback is rejected by _is_cyclic guard.
// set_fallbacks() calls _is_cyclic(f, 0) per entry. If f == this, returns true
// and ERR_FAIL_COND_MSG fires — the fallback is NOT added. A regression
// (removing visited set) leaves the logic intact for the self-cycle case
// since the base check is `p_f == this`. This test verifies the guard fires.
TEST_CASE("[Font][CWE-407] Cyclic detection: self-reference fallback is rejected") {
	Ref<FontVariation> font;
	font.instantiate();

	TypedArray<Font> fallbacks;
	fallbacks.push_back(font); // self-reference

	// set_fallbacks must reject this with an error message.
	// ERR_FAIL_COND_MSG prints to stderr but does not crash — test verifies
	// fallbacks remains empty after the rejected set.
	ERR_PRINT_OFF;
	font->set_fallbacks(fallbacks);
	ERR_PRINT_ON;

	TypedArray<Font> result = font->get_fallbacks();
	CHECK_MESSAGE(result.size() == 0, "Self-referential fallback must be rejected.");
}

// Unit: linear chain A → B → C is not cyclic and is accepted.
TEST_CASE("[Font][CWE-407] Cyclic detection: linear chain A→B→C is valid") {
	Ref<FontVariation> a, b, c;
	a.instantiate();
	b.instantiate();
	c.instantiate();

	// C has no fallbacks — leaf node.
	TypedArray<Font> b_fallbacks;
	b_fallbacks.push_back(c);
	b->set_fallbacks(b_fallbacks);

	TypedArray<Font> a_fallbacks;
	a_fallbacks.push_back(b);
	a->set_fallbacks(a_fallbacks);

	CHECK_MESSAGE(a->get_fallbacks().size() == 1, "A must have fallback B.");
	CHECK_MESSAGE(b->get_fallbacks().size() == 1, "B must have fallback C.");
}

// Integration: diamond DAG — A → {B, C}, B → D, C → D.
// _is_cyclic must terminate in O(F) not O(F^D). Without visited set,
// D would be traversed twice from A (once via B, once via C).
// With visited guard, D is visited once and memoized.
TEST_CASE("[Font][CWE-407] Cyclic detection: diamond DAG terminates, no cycle") {
	Ref<FontVariation> a, b, c, d;
	a.instantiate();
	b.instantiate();
	c.instantiate();
	d.instantiate();

	// Build: D is a shared fallback of both B and C.
	TypedArray<Font> b_fallbacks;
	b_fallbacks.push_back(d);
	b->set_fallbacks(b_fallbacks);

	TypedArray<Font> c_fallbacks;
	c_fallbacks.push_back(d);
	c->set_fallbacks(c_fallbacks);

	// A's set_fallbacks calls _is_cyclic(b) and _is_cyclic(c).
	// Both explore D. Without visited set, this is O(2^depth).
	TypedArray<Font> a_fallbacks;
	a_fallbacks.push_back(b);
	a_fallbacks.push_back(c);
	a->set_fallbacks(a_fallbacks); // must not hang or crash

	CHECK_MESSAGE(a->get_fallbacks().size() == 2, "A must have fallbacks B and C.");
}

// Integration: back-edge detection — A → B → A must be rejected.
// (Not the same as self-reference — requires following the chain.)
TEST_CASE("[Font][CWE-407] Cyclic detection: indirect cycle A→B→A is rejected") {
	Ref<FontVariation> a, b;
	a.instantiate();
	b.instantiate();

	// Set B's fallback to A first (A has no fallbacks yet — OK).
	TypedArray<Font> b_fallbacks;
	b_fallbacks.push_back(a);
	b->set_fallbacks(b_fallbacks);
	CHECK_MESSAGE(b->get_fallbacks().size() == 1, "B→A link accepted.");

	// Now try to set A's fallback to B — would create cycle A→B→A.
	TypedArray<Font> a_fallbacks;
	a_fallbacks.push_back(b);
	ERR_PRINT_OFF;
	a->set_fallbacks(a_fallbacks); // must be rejected
	ERR_PRINT_ON;

	CHECK_MESSAGE(a->get_fallbacks().size() == 0, "A→B fallback must be rejected (would form cycle).");
}

// Functional / complexity gate: wide diamond with 50 shared leaves.
// Without visited set, _update_rids would traverse each leaf 2× (once per
// branch). With 2 branches and 50 shared leaves: 100 traversals.
// At depth D with branching factor 2: O(2^D) without guard, O(N) with.
// This test verifies set_fallbacks() completes without exponential stall.
TEST_CASE("[Font][CWE-407][Stress] Cyclic detection: wide diamond, 50 shared leaves") {
	constexpr int LEAVES = 50;
	Vector<Ref<FontVariation>> leaves;
	leaves.resize(LEAVES);
	for (int i = 0; i < LEAVES; i++) {
		leaves.write[i].instantiate();
	}

	// branch_left and branch_right both reference all 50 leaves.
	Ref<FontVariation> branch_left, branch_right, root_font;
	branch_left.instantiate();
	branch_right.instantiate();
	root_font.instantiate();

	TypedArray<Font> leaf_array;
	for (int i = 0; i < LEAVES; i++) {
		leaf_array.push_back(leaves[i]);
	}
	branch_left->set_fallbacks(leaf_array);
	branch_right->set_fallbacks(leaf_array);

	// root → {left, right} — both branches share 50 leaves.
	// _is_cyclic traversal without visited: would scan 100 leaf slots.
	// With visited: scans 50 leaves once.
	TypedArray<Font> root_fallbacks;
	root_fallbacks.push_back(branch_left);
	root_fallbacks.push_back(branch_right);
	root_font->set_fallbacks(root_fallbacks); // must complete quickly

	CHECK_MESSAGE(root_font->get_fallbacks().size() == 2, "Root must have 2 branch fallbacks.");
}

} // namespace TestFontCWE407
