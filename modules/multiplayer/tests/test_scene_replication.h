/**************************************************************************/
/*  test_scene_replication.h                                              */
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
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

#include "../multiplayer_synchronizer.h"
#include "../scene_replication_config.h"

namespace TestSceneReplication {

class PrecisionDeltaNode : public Node {
	GDCLASS(PrecisionDeltaNode, Node);

	Vector3 v0;
	Vector3 v1;
	float f2 = 0.0f;
	Vector3 v3;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("set_v0", "v"), &PrecisionDeltaNode::set_v0);
		ClassDB::bind_method(D_METHOD("get_v0"), &PrecisionDeltaNode::get_v0);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "v0"), "set_v0", "get_v0");
		ClassDB::bind_method(D_METHOD("set_v1", "v"), &PrecisionDeltaNode::set_v1);
		ClassDB::bind_method(D_METHOD("get_v1"), &PrecisionDeltaNode::get_v1);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "v1"), "set_v1", "get_v1");
		ClassDB::bind_method(D_METHOD("set_f2", "v"), &PrecisionDeltaNode::set_f2);
		ClassDB::bind_method(D_METHOD("get_f2"), &PrecisionDeltaNode::get_f2);
		ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "f2"), "set_f2", "get_f2");
		ClassDB::bind_method(D_METHOD("set_v3", "v"), &PrecisionDeltaNode::set_v3);
		ClassDB::bind_method(D_METHOD("get_v3"), &PrecisionDeltaNode::get_v3);
		ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "v3"), "set_v3", "get_v3");
	}

public:
	void set_v0(const Vector3 &p_v) { v0 = p_v; }
	Vector3 get_v0() const { return v0; }
	void set_v1(const Vector3 &p_v) { v1 = p_v; }
	Vector3 get_v1() const { return v1; }
	void set_f2(float p_v) { f2 = p_v; }
	float get_f2() const { return f2; }
	void set_v3(const Vector3 &p_v) { v3 = p_v; }
	Vector3 get_v3() const { return v3; }
};

static Vector<Variant> _round_trip(const Vector<Variant> &p_values, const Vector<int> &p_precisions) {
	Vector<const Variant *> ptrs;
	ptrs.resize(p_values.size());
	for (int i = 0; i < p_values.size(); i++) {
		ptrs.write[i] = &p_values[i];
	}

	int size = 0;
	Error err = MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), p_precisions.ptr(), ptrs.size(), nullptr, size, false);
	REQUIRE(err == OK);

	Vector<uint8_t> buffer;
	buffer.resize(size);
	int written = 0;
	err = MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), p_precisions.ptr(), ptrs.size(), buffer.ptrw(), written, false);
	REQUIRE(err == OK);
	REQUIRE(written == size);

	Vector<Variant> out;
	out.resize(p_values.size());
	int consumed = 0;
	err = MultiplayerSynchronizer::decode_state_quantized(out, p_precisions.ptr(), buffer.ptr(), size, consumed, false);
	REQUIRE(err == OK);
	REQUIRE(consumed == size);
	return out;
}

TEST_CASE("[Multiplayer][SceneReplication] Half-precision codec round-trips supported types") {
	Vector<Variant> values;
	values.push_back(1.5f);
	values.push_back(Vector2(3.25, -7.5));
	values.push_back(Vector3(10.0, -20.5, 0.125));
	values.push_back(Vector4(2.5, -4.25, 8.0, -0.5));
	values.push_back(Quaternion(0.0, 0.70703125, 0.0, 0.70703125));
	values.push_back(Color(0.5, 0.25, 0.75, 1.0));

	Vector<int> precisions;
	for (int i = 0; i < values.size(); i++) {
		precisions.push_back(SceneReplicationConfig::PRECISION_HALF);
	}

	Vector<Variant> out = _round_trip(values, precisions);

	CHECK(((float)out[0]) == doctest::Approx(1.5f).epsilon(0.001));
	CHECK(((Vector2)out[1]).is_equal_approx(Vector2(3.25, -7.5)));
	CHECK(((Vector3)out[2]).is_equal_approx(Vector3(10.0, -20.5, 0.125)));
	CHECK(((Vector4)out[3]).is_equal_approx(Vector4(2.5, -4.25, 8.0, -0.5)));
	CHECK(((Quaternion)out[4]).is_equal_approx(Quaternion(0.0, 0.70703125, 0.0, 0.70703125)));
	CHECK(((Color)out[5]).is_equal_approx(Color(0.5, 0.25, 0.75, 1.0)));
}

TEST_CASE("[Multiplayer][SceneReplication] Half precision shrinks the encoded size") {
	Vector<Variant> values;
	values.push_back(Vector3(1.0, 2.0, 3.0));
	Vector<const Variant *> ptrs;
	ptrs.resize(1);
	ptrs.write[0] = &values[0];

	Vector<int> half = { SceneReplicationConfig::PRECISION_HALF };
	Vector<int> full = { SceneReplicationConfig::PRECISION_FULL };

	int half_size = 0;
	int full_size = 0;
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), half.ptr(), 1, nullptr, half_size, false);
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), full.ptr(), 1, nullptr, full_size, false);

	CHECK(half_size == 7);
	CHECK(half_size < full_size);
}

TEST_CASE("[Multiplayer][SceneReplication] Half on unsupported type falls back to full") {
	Vector<Variant> values;
	values.push_back(String("hello"));
	values.push_back(Vector3(1.0, 2.0, 3.0));

	Vector<int> precisions = { SceneReplicationConfig::PRECISION_HALF, SceneReplicationConfig::PRECISION_HALF };
	Vector<Variant> out = _round_trip(values, precisions);

	CHECK(((String)out[0]) == String("hello"));
	CHECK(((Vector3)out[1]).is_equal_approx(Vector3(1.0, 2.0, 3.0)));
}

TEST_CASE("[Multiplayer][SceneReplication] Half precision loses out-of-range magnitudes") {
	Vector<Variant> values;
	values.push_back(Vector3(100000.0, -100000.0, 1e-9));
	Vector<int> precisions = { SceneReplicationConfig::PRECISION_HALF };
	Vector<Variant> out = _round_trip(values, precisions);
	Vector3 v = out[0];
	CHECK((Math::is_inf(v.x) || Math::is_nan(v.x)));
	CHECK((Math::is_inf(v.y) || Math::is_nan(v.y)));
	CHECK(v.z == 0.0f);
}

TEST_CASE("[Multiplayer][SceneReplication] Config precision setting persists and round-trips") {
	Ref<SceneReplicationConfig> config;
	config.instantiate();
	NodePath path(".:position");
	config->add_property(path);

	CHECK_FALSE(config->has_reduced_precision());
	CHECK(config->property_get_precision(path) == SceneReplicationConfig::PRECISION_FULL);

	config->property_set_precision(path, SceneReplicationConfig::PRECISION_HALF);
	CHECK(config->property_get_precision(path) == SceneReplicationConfig::PRECISION_HALF);
	CHECK(config->has_reduced_precision());

	CHECK(((int)config->get("properties/0/precision")) == (int)SceneReplicationConfig::PRECISION_HALF);
	config->set("properties/0/precision", (int)SceneReplicationConfig::PRECISION_FULL);
	CHECK(config->property_get_precision(path) == SceneReplicationConfig::PRECISION_FULL);
	CHECK_FALSE(config->has_reduced_precision());
}

TEST_CASE("[Multiplayer][SceneReplication] Mixed full/half properties round-trip") {
	Vector<Variant> values;
	values.push_back(Vector3(100.5, -50.25, 7.125));
	values.push_back(1234567);
	values.push_back(Vector3(-1.0, 2.0, -3.0));
	Vector<int> precisions = {
		SceneReplicationConfig::PRECISION_HALF,
		SceneReplicationConfig::PRECISION_HALF,
		SceneReplicationConfig::PRECISION_FULL,
	};
	Vector<Variant> out = _round_trip(values, precisions);
	CHECK(((Vector3)out[0]).is_equal_approx(Vector3(100.5, -50.25, 7.125)));
	CHECK(((int)out[1]) == 1234567);
	CHECK(((Vector3)out[2]).is_equal_approx(Vector3(-1.0, 2.0, -3.0)));
}

TEST_CASE("[Multiplayer][SceneReplication] Truncated buffer decodes to an error, not a crash") {
	Vector<Variant> values;
	values.push_back(Vector3(1.0, 2.0, 3.0));
	Vector<const Variant *> ptrs;
	ptrs.resize(1);
	ptrs.write[0] = &values[0];
	Vector<int> half = { SceneReplicationConfig::PRECISION_HALF };

	int size = 0;
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), half.ptr(), 1, nullptr, size, false);
	Vector<uint8_t> buffer;
	buffer.resize(size);
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), half.ptr(), 1, buffer.ptrw(), size, false);

	Vector<Variant> out;
	out.resize(1);
	int consumed = 0;
	Error err = MultiplayerSynchronizer::decode_state_quantized(out, half.ptr(), buffer.ptr(), size - 1, consumed, false);
	CHECK(err != OK);
}

TEST_CASE("[Multiplayer][SceneReplication] Half/full precision mismatch is rejected for Vector3") {
	Vector<Variant> values;
	values.push_back(Vector3(12.5, -3.25, 6.75));
	Vector<const Variant *> ptrs;
	ptrs.resize(1);
	ptrs.write[0] = &values[0];
	Vector<int> half = { SceneReplicationConfig::PRECISION_HALF };
	Vector<int> full = { SceneReplicationConfig::PRECISION_FULL };

	int size = 0;
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), half.ptr(), 1, nullptr, size, false);
	Vector<uint8_t> buffer;
	buffer.resize(size);
	MultiplayerSynchronizer::encode_state_quantized(ptrs.ptrw(), half.ptr(), 1, buffer.ptrw(), size, false);

	Vector<Variant> out;
	out.resize(1);
	int consumed = 0;
	Error err = MultiplayerSynchronizer::decode_state_quantized(out, full.ptr(), buffer.ptr(), size, consumed, false);
	CHECK((err != OK || consumed != size));
}

TEST_CASE("[Multiplayer][SceneReplication] Cached precision arrays align with property lists") {
	Ref<SceneReplicationConfig> config;
	config.instantiate();
	NodePath spawn_sync(".:position");
	NodePath watch_only(".:health");
	config->add_property(spawn_sync);
	config->add_property(watch_only);
	config->property_set_precision(spawn_sync, SceneReplicationConfig::PRECISION_HALF);
	config->property_set_spawn(watch_only, false);
	config->property_set_replication_mode(watch_only, SceneReplicationConfig::REPLICATION_MODE_ON_CHANGE);

	const List<NodePath> &sync_props = config->get_sync_properties();
	const Vector<int> &sync_prec = config->get_sync_precisions();
	REQUIRE(sync_props.size() == sync_prec.size());
	REQUIRE(sync_props.size() == 1);
	CHECK(sync_prec[0] == SceneReplicationConfig::PRECISION_HALF);

	const List<NodePath> &watch_props = config->get_watch_properties();
	const Vector<int> &watch_prec = config->get_watch_precisions();
	REQUIRE(watch_props.size() == watch_prec.size());
	REQUIRE(watch_props.size() == 1);
	CHECK(watch_prec[0] == SceneReplicationConfig::PRECISION_FULL);

	const List<NodePath> &spawn_props = config->get_spawn_properties();
	const Vector<int> &spawn_prec = config->get_spawn_precisions();
	REQUIRE(spawn_props.size() == spawn_prec.size());
	REQUIRE(spawn_props.size() == 1);
	CHECK(spawn_prec[0] == SceneReplicationConfig::PRECISION_HALF);
}

TEST_CASE("[Multiplayer][SceneReplication][SceneTree] Delta round-trip preserves order with a mixed-precision subset") {
	GDREGISTER_CLASS(PrecisionDeltaNode);

	Ref<SceneReplicationConfig> cfg;
	cfg.instantiate();
	NodePath pv0(".:v0");
	NodePath pv1(".:v1");
	NodePath pf2(".:f2");
	NodePath pv3(".:v3");
	Vector<NodePath> paths = { pv0, pv1, pf2, pv3 };
	for (const NodePath &p : paths) {
		cfg->add_property(p);
		cfg->property_set_replication_mode(p, SceneReplicationConfig::REPLICATION_MODE_ON_CHANGE);
	}
	cfg->property_set_precision(pv0, SceneReplicationConfig::PRECISION_HALF);
	cfg->property_set_precision(pf2, SceneReplicationConfig::PRECISION_HALF);

	Node *root = SceneTree::get_singleton()->get_root();
	PrecisionDeltaNode *source = memnew(PrecisionDeltaNode);
	root->add_child(source);
	MultiplayerSynchronizer *sync = memnew(MultiplayerSynchronizer);
	sync->set_replication_config(cfg);
	source->add_child(sync);

	source->set_v0(Vector3(1, 2, 3));
	source->set_v1(Vector3(4, 5, 6));
	source->set_f2(7.0);
	source->set_v3(Vector3(8, 9, 10));

	uint64_t indexes = 0;
	sync->get_delta_state(100, 0, indexes);

	source->set_v0(Vector3(100.5, -20.25, 6.125));
	source->set_f2(42.5);
	source->set_v3(Vector3(-3.0, 11.0, 0.5));

	List<Variant> delta = sync->get_delta_state(200, 100, indexes);
	REQUIRE(delta.size() == 3);
	REQUIRE(indexes == (uint64_t)((1 << 0) | (1 << 2) | (1 << 3)));

	Vector<const Variant *> vptr;
	vptr.resize(delta.size());
	int i = 0;
	for (const Variant &v : delta) {
		vptr.write[i++] = &v;
	}
	Vector<int> precisions;
	const Vector<int> &wprec = cfg->get_watch_precisions();
	for (int b = 0; b < wprec.size(); b++) {
		if (indexes & (1ULL << b)) {
			precisions.push_back(wprec[b]);
		}
	}
	REQUIRE(precisions.size() == 3);

	int size = 0;
	MultiplayerSynchronizer::encode_state_quantized(vptr.ptrw(), precisions.ptr(), vptr.size(), nullptr, size, false);
	Vector<uint8_t> buffer;
	buffer.resize(size);
	MultiplayerSynchronizer::encode_state_quantized(vptr.ptrw(), precisions.ptr(), vptr.size(), buffer.ptrw(), size, false);

	List<NodePath> props = sync->get_delta_properties(indexes);
	REQUIRE(props.size() == 3);
	Vector<Variant> out;
	out.resize(props.size());
	int consumed = 0;
	Error err = MultiplayerSynchronizer::decode_state_quantized(out, precisions.ptr(), buffer.ptr(), size, consumed, false);
	REQUIRE(err == OK);
	REQUIRE(consumed == size);

	PrecisionDeltaNode *target = memnew(PrecisionDeltaNode);
	root->add_child(target);
	target->set_v1(Vector3(4, 5, 6));
	err = MultiplayerSynchronizer::set_state(props, target, out);
	REQUIRE(err == OK);

	CHECK(target->get_v0().is_equal_approx(Vector3(100.5, -20.25, 6.125)));
	CHECK(target->get_f2() == doctest::Approx(42.5).epsilon(0.001));
	CHECK(target->get_v3().is_equal_approx(Vector3(-3.0, 11.0, 0.5)));
	CHECK(target->get_v1().is_equal_approx(Vector3(4, 5, 6)));

	source->remove_child(sync);
	memdelete(sync);
	root->remove_child(source);
	memdelete(source);
	root->remove_child(target);
	memdelete(target);
}

} //namespace TestSceneReplication
