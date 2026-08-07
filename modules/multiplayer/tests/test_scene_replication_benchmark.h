/**************************************************************************/
/*  test_scene_replication_benchmark.h                                    */
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

#include "core/math/math_funcs.h"
#include "core/os/os.h"

#include "scene/3d/node_3d.h"
#include "scene/main/multiplayer_api.h"

#include "../multiplayer_synchronizer.h"
#include "../scene_replication_config.h"

namespace TestSceneReplicationBenchmark {

static int _encode(const Vector<Variant> &p_vals, const Vector<int> &p_prec, Vector<uint8_t> &r_buf) {
	Vector<const Variant *> ptrs;
	ptrs.resize(p_vals.size());
	for (int i = 0; i < p_vals.size(); i++) {
		ptrs.write[i] = &p_vals[i];
	}
	int size = 0;
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), p_prec.ptr(), ptrs.size(), nullptr, size, false);
	r_buf.resize(size);
	int written = 0;
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), p_prec.ptr(), ptrs.size(), r_buf.ptrw(), written, false);
	return size;
}

TEST_CASE("[SceneReplication][Benchmark] Size / accuracy / speed" * doctest::skip()) {
	Vector<Variant> state;
	state.push_back(Vector3(123.5, 4.25, -67.75));
	state.push_back(Vector3(3.5, 0.0, -2.25));
	state.push_back(1.5707964f);
	state.push_back(87.5f);
	state.push_back(42.0f);

	Vector<int> full;
	Vector<int> half;
	for (int i = 0; i < state.size(); i++) {
		full.push_back(SceneReplicationConfig::PRECISION_FULL);
		half.push_back(SceneReplicationConfig::PRECISION_HALF);
	}

	Vector<uint8_t> fbuf;
	Vector<uint8_t> hbuf;
	const int fsize = _encode(state, full, fbuf);
	const int hsize = _encode(state, half, hbuf);

	print_line("");
	print_line("=== Replication precision benchmark ===");
	print_line(vformat("Snapshot fields: position(Vector3), velocity(Vector3), facing(float), health(float), mana(float)"));
	print_line(vformat("SIZE   full = %d bytes   half = %d bytes   reduction = %.1f%%", fsize, hsize, 100.0 * (fsize - hsize) / fsize));
	CHECK(hsize < fsize);

	print_line("ACCURACY (single Vector3 at increasing magnitude, worst-component abs error):");
	const float mags[] = { 1.0f, 10.0f, 100.0f, 1000.0f, 10000.0f };
	for (float m : mags) {
		Vector<Variant> one;
		one.push_back(Vector3(m * 0.3713 + 0.531, -(m * 0.6127 + 0.208), m * 0.1373 + 0.769));
		Vector<int> hp;
		hp.push_back(SceneReplicationConfig::PRECISION_HALF);
		Vector<uint8_t> b;
		_encode(one, hp, b);
		Vector<Variant> out;
		out.resize(1);
		int consumed = 0;
		MultiplayerSynchronizer::decode_state_quantized(out, hp.ptr(), b.ptr(), b.size(), consumed, false);
		const Vector3 o = one[0];
		const Vector3 d = out[0];
		const float err = MAX(MAX(Math::abs(o.x - d.x), Math::abs(o.y - d.y)), Math::abs(o.z - d.z));
		print_line(vformat("  |coord| ~ %-8.0f   max abs err = %.4f   (%.4f%%)", (double)m, err, 100.0 * err / m));
	}

	const int iters = 300000;
	Vector<const Variant *> ptrs;
	ptrs.resize(state.size());
	for (int i = 0; i < state.size(); i++) {
		ptrs.write[i] = &state[i];
	}
	Vector<uint8_t> buf;
	buf.resize(128);
	Vector<Variant> out;
	out.resize(state.size());

	for (int mode = 0; mode < 2; mode++) {
		const int *prec = mode == 0 ? full.ptr() : half.ptr();
		int warm = 0;
		MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), prec, ptrs.size(), buf.ptrw(), warm, false);

		double sink = 0.0;
		const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
		for (int i = 0; i < iters; i++) {
			int s = 0;
			MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), prec, ptrs.size(), buf.ptrw(), s, false);
			int consumed = 0;
			MultiplayerSynchronizer::decode_state_quantized(out, prec, buf.ptr(), s, consumed, false);
			sink += ((Vector3)out[0]).x;
		}
		const uint64_t t1 = OS::get_singleton()->get_ticks_usec();
		const double us = (double)(t1 - t0);
		print_line(vformat("SPEED  %s   %.1f ns / encode+decode   %.2f M ops/s   (checksum %.1f)",
				mode == 0 ? "full" : "half", 1000.0 * us / iters, iters / us, sink));
	}
	print_line("");
}

TEST_CASE("[SceneReplication][Benchmark] Property-list gather: forced copy vs reference" * doctest::skip()) {
	const int counts[] = { 3, 8 };
	const int iters = 2000000;
	const int rounds = 5;
	print_line("");
	print_line("=== Property-list gather: forced value copy vs reference (min of alternating rounds) ===");
	for (int prop_count : counts) {
		Ref<SceneReplicationConfig> cfg;
		cfg.instantiate();
		for (int i = 0; i < prop_count; i++) {
			cfg->add_property(NodePath(vformat(".:prop_%d", i)));
		}
		cfg->get_sync_properties(); // Prime the internal list.

		uint64_t sink = 0;
		double best_copy = 1e30;
		double best_ref = 1e30;

		for (int round = 0; round < rounds; round++) {
			const bool copy_first = (round % 2) == 0;
			for (int step = 0; step < 2; step++) {
				const bool do_copy = (step == 0) == copy_first;
				const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
				if (do_copy) {
					for (int i = 0; i < iters; i++) {
						const List<NodePath> c = cfg->get_sync_properties(); // Simulate old value-returning API.
						for (const NodePath &p : c) {
							sink += p.get_subname_count();
						}
					}
				} else {
					for (int i = 0; i < iters; i++) {
						const List<NodePath> &r = cfg->get_sync_properties(); // New: reference.
						for (const NodePath &p : r) {
							sink += p.get_subname_count();
						}
					}
				}
				const double ns = 1000.0 * (double)(OS::get_singleton()->get_ticks_usec() - t0) / iters;
				if (do_copy) {
					best_copy = MIN(best_copy, ns);
				} else {
					best_ref = MIN(best_ref, ns);
				}
			}
		}

		print_line(vformat("props=%-2d  copy=%6.1f ns  ref=%6.1f ns  saved=%5.1f ns/gather (%.0f%%)  [sink=%d]",
				prop_count, best_copy, best_ref, best_copy - best_ref,
				best_copy > 0.0 ? 100.0 * (best_copy - best_ref) / best_copy : 0.0, (int64_t)sink));
	}
	print_line("");
}

TEST_CASE("[SceneReplication][Benchmark] Full sync tick (gather+get_state+encode) vs node count" * doctest::skip()) {
	Ref<SceneReplicationConfig> cfg;
	cfg.instantiate();
	cfg->add_property(NodePath(".:position"));
	cfg->add_property(NodePath(".:rotation"));
	cfg->add_property(NodePath(".:scale"));
	cfg->get_sync_properties(); // Prime the internal list.

	const int scales[] = { 100, 1000, 10000, 50000, 100000 };
	const int rounds = 5;
	print_line("");
	print_line("=== Full sync tick: gather+get_state+encode for N synchronizers (min of alternating rounds) ===");
	print_line("    copy = old value-returning gather (~master), ref = new reference gather; socket I/O excluded.");

	for (int n : scales) {
		Vector<Node3D *> nodes;
		nodes.resize(n);
		Node3D **nodep = nodes.ptrw();
		for (int i = 0; i < n; i++) {
			Node3D *nd = memnew(Node3D);
			nd->set_position(Vector3(i * 0.37, i * 0.11, i * 0.53));
			nd->set_rotation(Vector3(i * 0.013, i * 0.021, i * 0.005));
			nd->set_scale(Vector3(1.0 + i * 0.0001, 1.0, 1.0 + i * 0.0002));
			nodep[i] = nd;
		}

		uint64_t sink = 0;
		double best_copy = 1e30;
		double best_ref = 1e30;

		for (int round = 0; round < rounds; round++) {
			const bool copy_first = (round % 2) == 0;
			for (int step = 0; step < 2; step++) {
				const bool do_copy = (step == 0) == copy_first;
				Vector<Variant> vars;
				Vector<const Variant *> varp;
				Vector<uint8_t> buf;
				buf.resize(256);
				const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
				for (int i = 0; i < n; i++) {
					if (do_copy) {
						const List<NodePath> props = cfg->get_sync_properties(); // Simulate old value-returning API.
						MultiplayerSynchronizer::get_state(props, nodep[i], vars, varp);
					} else {
						const List<NodePath> &props = cfg->get_sync_properties(); // New: reference.
						MultiplayerSynchronizer::get_state(props, nodep[i], vars, varp);
					}
					int size = 0;
					MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), nullptr, size);
					if (size > buf.size()) {
						buf.resize(size);
					}
					MultiplayerAPI::encode_and_compress_variants(varp.ptrw(), varp.size(), buf.ptrw(), size);
					sink += size;
				}
				const double ms = (double)(OS::get_singleton()->get_ticks_usec() - t0) / 1000.0;
				if (do_copy) {
					best_copy = MIN(best_copy, ms);
				} else {
					best_ref = MIN(best_ref, ms);
				}
			}
		}

		for (int i = 0; i < n; i++) {
			memdelete(nodep[i]);
		}

		const double saved = best_copy - best_ref;
		print_line(vformat("N=%-6d  copy=%9.3f ms  ref=%9.3f ms  saved=%8.3f ms (%.1f%%)  per-node=%.1f ns  [sink=%d]",
				n, best_copy, best_ref, saved,
				best_copy > 0.0 ? 100.0 * saved / best_copy : 0.0,
				1000000.0 * saved / n, (int64_t)sink));
	}
	print_line("");
}

TEST_CASE("[SceneReplication][Benchmark] Reduced-precision encode tick: per-property find vs cached, vs property count" * doctest::skip()) {
	const int counts[] = { 8, 32, 64 };
	const int iters = 200000;
	const int rounds = 5;
	print_line("");
	print_line("=== Reduced-precision encode tick: per-property find (old) vs cached (new), min of alternating rounds ===");
	for (int prop_count : counts) {
		Ref<SceneReplicationConfig> cfg;
		cfg.instantiate();
		Vector<Variant> values;
		for (int i = 0; i < prop_count; i++) {
			NodePath p = NodePath(vformat(".:prop_%d", i));
			cfg->add_property(p);
			cfg->property_set_precision(p, SceneReplicationConfig::PRECISION_HALF);
			values.push_back(Vector3(i * 0.5, i * 0.25, i * 0.125));
		}
		const List<NodePath> &props = cfg->get_sync_properties();
		Vector<const Variant *> vptr;
		vptr.resize(values.size());
		for (int i = 0; i < values.size(); i++) {
			vptr.write[i] = &values[i];
		}
		Vector<uint8_t> buf;
		buf.resize(prop_count * 8 + 16);

		uint64_t sink = 0;
		double best_old = 1e30;
		double best_new = 1e30;
		for (int round = 0; round < rounds; round++) {
			const bool old_first = (round % 2) == 0;
			for (int step = 0; step < 2; step++) {
				const bool do_old = (step == 0) == old_first;
				const uint64_t t0 = OS::get_singleton()->get_ticks_usec();
				for (int it = 0; it < iters; it++) {
					Vector<int> precisions;
					const int *prec;
					if (do_old) {
						precisions.resize(props.size());
						int *w = precisions.ptrw();
						int j = 0;
						for (const NodePath &p : props) {
							w[j++] = cfg->property_get_precision(p);
						}
						prec = precisions.ptr();
					} else {
						prec = cfg->get_sync_precisions().ptr();
					}
					int size = 0;
					MultiplayerSynchronizer::encode_state_quantized(vptr.ptrw(), prec, vptr.size(), buf.ptrw(), size, false);
					sink += size;
				}
				const double ns = 1000.0 * (double)(OS::get_singleton()->get_ticks_usec() - t0) / iters;
				if (do_old) {
					best_old = MIN(best_old, ns);
				} else {
					best_new = MIN(best_new, ns);
				}
			}
		}
		print_line(vformat("props=%-3d  find+encode=%8.1f ns  cached+encode=%8.1f ns  saved=%7.1f ns/tick (%.0f%%)  [sink=%d]",
				prop_count, best_old, best_new, best_old - best_new,
				best_old > 0.0 ? 100.0 * (best_old - best_new) / best_old : 0.0, (int64_t)sink));
	}
	print_line("");
}

} //namespace TestSceneReplicationBenchmark
