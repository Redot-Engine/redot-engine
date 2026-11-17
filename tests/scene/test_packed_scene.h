/**************************************************************************/
/*  test_packed_scene.h                                                   */
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

#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"

#include "tests/test_macros.h"

namespace TestPackedScene {

TEST_CASE("[PackedScene] Pack Scene and Retrieve State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	const Error err = packed_scene.pack(scene);
	CHECK(err == OK);

	// Retrieve the packed state.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state->get_node_count() == 1);
	CHECK(state->get_node_name(0) == "TestScene");

	memdelete(scene);
}

TEST_CASE("[PackedScene] Signals Preserved when Packing Scene") {
	// Create main scene
	// root
	// `- sub_node (local)
	// `- sub_scene (instance of another scene)
	//    `- sub_scene_node (owned by sub_scene)
	Node *main_scene_root = memnew(Node);
	Node *sub_node = memnew(Node);
	Node *sub_scene_root = memnew(Node);
	Node *sub_scene_node = memnew(Node);

	main_scene_root->add_child(sub_node);
	sub_node->set_owner(main_scene_root);

	sub_scene_root->add_child(sub_scene_node);
	sub_scene_node->set_owner(sub_scene_root);

	main_scene_root->add_child(sub_scene_root);
	sub_scene_root->set_owner(main_scene_root);

	SUBCASE("Signals that should be saved") {
		int main_flags = Object::CONNECT_PERSIST;
		// sub node to a node in main scene
		sub_node->connect("ready", callable_mp(main_scene_root, &Node::is_ready), main_flags);
		// subscene root to a node in main scene
		sub_scene_root->connect("ready", callable_mp(main_scene_root, &Node::is_ready), main_flags);
		//subscene root to subscene root (connected within main scene)
		sub_scene_root->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), main_flags);

		// Pack the scene.
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		const Error err = packed_scene->pack(main_scene_root);
		CHECK(err == OK);

		// Make sure the right connections are in packed scene.
		Ref<SceneState> state = packed_scene->get_state();
		CHECK_EQ(state->get_connection_count(), 3);
	}

	/*
	// FIXME: This subcase requires GH-48064 to be fixed.
	SUBCASE("Signals that should not be saved") {
		int subscene_flags = Object::CONNECT_PERSIST | Object::CONNECT_INHERITED;
		// subscene node to itself
		sub_scene_node->connect("ready", callable_mp(sub_scene_node, &Node::is_ready), subscene_flags);
		// subscene node to subscene root
		sub_scene_node->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), subscene_flags);
		//subscene root to subscene root (connected within sub scene)
		sub_scene_root->connect("ready", callable_mp(sub_scene_root, &Node::is_ready), subscene_flags);

		// Pack the scene.
		Ref<PackedScene> packed_scene;
		packed_scene.instantiate();
		const Error err = packed_scene->pack(main_scene_root);
		CHECK(err == OK);

		// Make sure the right connections are in packed scene.
		Ref<SceneState> state = packed_scene->get_state();
		CHECK_EQ(state->get_connection_count(), 0);
	}
	*/

	memdelete(main_scene_root);
}

TEST_CASE("[PackedScene] Missing properties are preserved when packing") {
	// Regression test for #938: overrides that can't be applied on load because the
	// node's script failed to parse must not be lost when the scene is re-saved.
	// The `_missing_properties` meta is the in-memory holding area that `instantiate()`
	// fills in that case; `pack()` must re-emit its values as regular node properties
	// while never storing the meta key itself.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	Dictionary missing_properties;
	missing_properties["caption"] = "hello";
	missing_properties["amount"] = 42;
	scene->set_meta("_missing_properties", missing_properties);

	PackedScene packed_scene;
	const Error err = packed_scene.pack(scene);
	CHECK(err == OK);

	Ref<SceneState> state = packed_scene.get_state();
	REQUIRE(state.is_valid());
	REQUIRE(state->get_node_count() == 1);

	bool found_caption = false;
	bool found_amount = false;
	bool found_meta = false;
	for (int i = 0; i < state->get_node_property_count(0); i++) {
		const StringName name = state->get_node_property_name(0, i);
		if (name == StringName("caption")) {
			found_caption = true;
			CHECK(state->get_node_property_value(0, i) == Variant("hello"));
		} else if (name == StringName("amount")) {
			found_amount = true;
			CHECK(state->get_node_property_value(0, i) == Variant(42));
		} else if (name == StringName("metadata/_missing_properties")) {
			found_meta = true;
		}
	}

	// The preserved values are stored as regular properties
	CHECK(found_caption);
	CHECK(found_amount);
	// but the holding meta is never written to the scene.
	CHECK_FALSE(found_meta);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Missing node-reference properties are preserved as node paths") {
	// Node-typed exports are applied as deferred NodePaths; when the script fails to
	// parse they must be preserved and re-emitted with the node-path flag so the
	// reference round-trips.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	Dictionary missing_node_properties;
	missing_node_properties["target"] = NodePath("../Target");
	scene->set_meta("_missing_node_properties", missing_node_properties);

	PackedScene packed_scene;
	const Error err = packed_scene.pack(scene);
	CHECK(err == OK);

	Ref<SceneState> state = packed_scene.get_state();
	REQUIRE(state.is_valid());
	REQUIRE(state->get_node_count() == 1);

	bool found_target = false;
	for (int i = 0; i < state->get_node_property_count(0); i++) {
		if (state->get_node_property_name(0, i) == StringName("target")) {
			found_target = true;
			CHECK(state->get_node_property_value(0, i) == Variant(NodePath("../Target")));
		}
	}
	CHECK(found_target);

	// It must be flagged as a node path so it is deferred again on the next load.
	const Vector<String> node_paths = state->get_node_deferred_nodepath_properties(0);
	CHECK(node_paths.has("target"));

	memdelete(scene);
}

TEST_CASE("[PackedScene] Stale missing properties are not restored once the property exists") {
	// If the script is fixed and the recovered property is left at its default, the
	// regular loop omits it; the stale value lingering in the meta must not resurrect.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// "process_mode" exists on the node and is left at its default; the meta holds a
	// different, stale value that must not be written back.
	Dictionary missing_properties;
	missing_properties["process_mode"] = Node::PROCESS_MODE_ALWAYS;
	scene->set_meta("_missing_properties", missing_properties);

	PackedScene packed_scene;
	const Error err = packed_scene.pack(scene);
	CHECK(err == OK);

	Ref<SceneState> state = packed_scene.get_state();
	REQUIRE(state.is_valid());
	REQUIRE(state->get_node_count() == 1);

	bool found_process_mode = false;
	for (int i = 0; i < state->get_node_property_count(0); i++) {
		if (state->get_node_property_name(0, i) == StringName("process_mode")) {
			found_process_mode = true;
		}
	}
	CHECK_FALSE(found_process_mode);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Clear Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Clear the packed scene.
	packed_scene.clear();

	// Check if it has been cleared.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK_FALSE(state->get_node_count() == 1);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Can Instantiate Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Check if the packed scene can be instantiated.
	const bool can_instantiate = packed_scene.can_instantiate();
	CHECK(can_instantiate == true);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Instantiate Packed Scene") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Instantiate the packed scene.
	Node *instance = packed_scene.instantiate();
	CHECK(instance != nullptr);
	CHECK(instance->get_name() == "TestScene");

	memdelete(scene);
	memdelete(instance);
}

TEST_CASE("[PackedScene] Instantiate Packed Scene With Children") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Add persisting child nodes to the scene.
	Node *child1 = memnew(Node);
	child1->set_name("Child1");
	scene->add_child(child1);
	child1->set_owner(scene);

	Node *child2 = memnew(Node);
	child2->set_name("Child2");
	scene->add_child(child2);
	child2->set_owner(scene);

	// Add non persisting child node to the scene.
	Node *child3 = memnew(Node);
	child3->set_name("Child3");
	scene->add_child(child3);

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Instantiate the packed scene.
	Node *instance = packed_scene.instantiate();
	CHECK(instance != nullptr);
	CHECK(instance->get_name() == "TestScene");

	// Validate the child nodes of the instantiated scene.
	CHECK(instance->get_child_count() == 2);
	CHECK(instance->get_child(0)->get_name() == "Child1");
	CHECK(instance->get_child(1)->get_name() == "Child2");
	CHECK(instance->get_child(0)->get_owner() == instance);
	CHECK(instance->get_child(1)->get_owner() == instance);

	memdelete(scene);
	memdelete(instance);
}

TEST_CASE("[PackedScene] Set Path") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Set a new path for the packed scene.
	const String new_path = "NewTestPath";
	packed_scene.set_path(new_path);

	// Check if the path has been set correctly.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state->get_path() == new_path);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Replace State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Create another scene state to replace with.
	Ref<SceneState> new_state = memnew(SceneState);
	new_state->set_path("NewPath");

	// Replace the state.
	packed_scene.replace_state(new_state);

	// Check if the state has been replaced.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state == new_state);

	memdelete(scene);
}

TEST_CASE("[PackedScene] Recreate State") {
	// Create a scene to pack.
	Node *scene = memnew(Node);
	scene->set_name("TestScene");

	// Pack the scene.
	PackedScene packed_scene;
	packed_scene.pack(scene);

	// Recreate the state.
	packed_scene.recreate_state();

	// Check if the state has been recreated.
	Ref<SceneState> state = packed_scene.get_state();
	CHECK(state.is_valid());
	CHECK(state->get_node_count() == 0); // Since the state was recreated, it should be empty.

	memdelete(scene);
}

} // namespace TestPackedScene
