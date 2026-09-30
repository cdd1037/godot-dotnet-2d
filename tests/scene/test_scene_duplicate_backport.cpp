/**************************************************************************/
/*  test_scene_duplicate_backport.cpp                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_scene_duplicate_backport)

#include "scene/2d/node_2d.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_utils.h"

namespace TestSceneDuplicateBackport {

TEST_CASE("[SceneTree][Node][SceneDuplicateBackport] Reordered scene descendants retain their properties") {
	Node2D *source = memnew(Node2D);
	source->set_name("Scene");
	for (const char *branch_name : { "A", "B" }) {
		Node2D *branch = memnew(Node2D);
		branch->set_name(branch_name);
		source->add_child(branch);
		branch->set_owner(source);
		for (const char *child_name : { "Left", "Right" }) {
			Node2D *child = memnew(Node2D);
			child->set_name(child_name);
			branch->add_child(child);
			child->set_owner(source);
			Node2D *leaf = memnew(Node2D);
			leaf->set_name("Leaf");
			child->add_child(leaf);
			leaf->set_owner(source);
		}
	}

	Ref<PackedScene> packed;
	packed.instantiate();
	REQUIRE(packed->pack(source) == OK);
	memdelete(source);
	// Keep the scene cached so duplication can instantiate it without writing a fixture to disk.
	packed->set_path(TestUtils::get_temp_path("scene_duplicate_backport.tscn"));
	Node *instance = packed->instantiate();
	REQUIRE(instance != nullptr);
	instance->move_child(instance->get_node(NodePath("B")), 0);
	for (const char *branch_name : { "A", "B" }) {
		Node *branch = instance->get_node(NodePath(branch_name));
		branch->move_child(branch->get_node(NodePath("Right")), 0);
	}

	const char *paths[] = { ".", "A", "A/Left", "A/Left/Leaf", "A/Right", "A/Right/Leaf", "B", "B/Left", "B/Left/Leaf", "B/Right", "B/Right/Leaf" };
	int marker = 0;
	for (const char *path : paths) {
		Node2D *node = Object::cast_to<Node2D>(instance->get_node(NodePath(path)));
		REQUIRE(node != nullptr);
		marker++;
		node->set_position(Vector2(marker, marker * 10));
		node->set_meta("marker", marker);
	}

	Node *root = instance;
	int flags = Node::DUPLICATE_SIGNALS | Node::DUPLICATE_GROUPS | Node::DUPLICATE_SCRIPTS | Node::DUPLICATE_USE_INSTANTIATION;
	SUBCASE("Duplicate an instance root") {
	}
	SUBCASE("Duplicate an instance below a plain parent") {
		root = memnew(Node);
		root->set_name("Parent");
		root->add_child(instance);
	}
	SUBCASE("Duplicate without instantiation keeps index-based copying") {
		flags &= ~Node::DUPLICATE_USE_INSTANTIATION;
	}
	SUBCASE("Duplicate without scripts still remaps properties") {
		flags &= ~Node::DUPLICATE_SCRIPTS;
	}

	Node *copy = root->duplicate(flags);
	REQUIRE(copy != nullptr);
	Node *copy_instance = root == instance ? copy : copy->get_node(NodePath("Scene"));
	REQUIRE(copy_instance != nullptr);
	for (const char *path : paths) {
		Node2D *original_node = Object::cast_to<Node2D>(instance->get_node(NodePath(path)));
		Node2D *copied_node = Object::cast_to<Node2D>(copy_instance->get_node(NodePath(path)));
		REQUIRE(copied_node != nullptr);
		CHECK(copied_node != original_node);
		CHECK(copied_node->get_position() == original_node->get_position());
		CHECK(copied_node->get_meta("marker") == original_node->get_meta("marker"));
	}

	memdelete(copy);
	memdelete(root);
}

} // namespace TestSceneDuplicateBackport
