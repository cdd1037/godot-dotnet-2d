/**************************************************************************/
/*  test_scene_resource_remap.cpp                                                 */
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

TEST_FORCE_LINK(test_scene_resource_remap)

#include "scene/resources/packed_scene.h"

namespace TestSceneResourceRemap {

static Ref<Resource> local_resource(const String &p_name) {
	Ref<Resource> result;
	result.instantiate();
	result->set_name(p_name);
	result->set_local_to_scene(true);
	return result;
}

TEST_CASE("[SceneTree][PackedScene][TenthFixBatch] Nested local resources reuse fallback and preserve aliases") {
	Node *scene = memnew(Node);
	Ref<Resource> source = local_resource("source");
	Ref<Resource> fallback = local_resource("fallback");
	Ref<Resource> child = local_resource("child");
	Ref<Resource> child_fallback = local_resource("old_child");
	Ref<Resource> ordinary;
	ordinary.instantiate();
	Dictionary nested;
	nested["local"] = child;
	nested["ordinary"] = ordinary;
	Dictionary old_nested;
	old_nested["local"] = child_fallback;
	old_nested["ordinary"] = ordinary;
	Array typed;
	typed.set_typed(Variant::OBJECT, "Resource", Variant());
	typed.push_back(child);
	typed.push_back(child);
	Array old_typed;
	old_typed.set_typed(Variant::OBJECT, "Resource", Variant());
	old_typed.push_back(child_fallback);
	old_typed.push_back(child_fallback);
	Array values{ nested, typed, nested };
	Array old_values{ old_nested, old_typed, old_nested };
	source->set_meta("nested", values);
	fallback->set_meta("nested", old_values);
	HashMap<Ref<Resource>, Ref<Resource>> initial_scene_resources;
	fallback->configure_for_local_scene(scene, initial_scene_resources);
	CHECK(fallback->get_local_scene() == scene);
	CHECK(child_fallback->get_local_scene() == scene);
	HashMap<Node *, HashMap<Ref<Resource>, Ref<Resource>>> cache;
	Ref<Resource> result = SceneState::get_remap_resource(source, cache, fallback, scene);
	CHECK(result == fallback);
	Array remapped = result->get_meta("nested");
	REQUIRE(remapped.size() == 3);
	CHECK_FALSE(remapped.is_same_instance(values));
	Dictionary first = remapped[0];
	Dictionary repeated = remapped[2];
	CHECK(first.is_same_instance(repeated));
	CHECK_FALSE(first.is_same_instance(nested));
	CHECK(Ref<Resource>(first["local"]) == child_fallback);
	CHECK(Ref<Resource>(first["ordinary"]) == ordinary);
	Array remapped_typed = remapped[1];
	CHECK(remapped_typed.is_same_typed(typed));
	CHECK(Ref<Resource>(remapped_typed[0]) == child_fallback);
	CHECK(Ref<Resource>(remapped_typed[1]) == child_fallback);
	CHECK(Ref<Resource>(nested["local"]) == child);
	CHECK(Ref<Resource>(typed[0]) == child);
	CHECK(child_fallback->get_name() == "child");
	CHECK(child_fallback->get_local_scene() == scene);
	CHECK(cache[scene].getptr(child) != nullptr);
	CHECK_FALSE(cache.has(nullptr));
	memdelete(scene);
}

TEST_CASE("[SceneTree][PackedScene][TenthFixBatch] Typed dictionary fallback compatibility is retained") {
	Node *scene = memnew(Node);
	for (bool compatible : { false, true }) {
		Ref<Resource> source = local_resource("source");
		Ref<Resource> fallback = local_resource("fallback");
		Ref<Resource> child = local_resource("child");
		Ref<Resource> old_child = local_resource("old_child");
		Dictionary values;
		values.set_typed(Variant::STRING, StringName(), Variant(), Variant::OBJECT, "Resource", Variant());
		values["value"] = child;
		Dictionary old_values;
		if (compatible) {
			old_values.set_typed(Variant::STRING, StringName(), Variant(), Variant::OBJECT, "Resource", Variant());
		}
		old_values["value"] = old_child;
		source->set_meta("values", values);
		fallback->set_meta("values", old_values);
		HashMap<Node *, HashMap<Ref<Resource>, Ref<Resource>>> cache;
		Ref<Resource> result = SceneState::get_remap_resource(source, cache, fallback, scene);
		Dictionary remapped = result->get_meta("values");
		CHECK(remapped.is_same_typed(values));
		Ref<Resource> remapped_child = remapped["value"];
		CHECK(remapped_child.is_valid());
		CHECK(remapped_child != child);
		CHECK((remapped_child == old_child) == compatible);
		CHECK(Ref<Resource>(values["value"]) == child);
	}
	memdelete(scene);
}

TEST_CASE("[SceneTree][PackedScene][TenthFixBatch] Reusable fallback remapping terminates resource and container cycles") {
	Node *scene = memnew(Node);
	Ref<Resource> source = local_resource("source");
	Ref<Resource> child = local_resource("child");
	Ref<Resource> fallback = local_resource("fallback");
	Ref<Resource> old_child = local_resource("old_child");
	Array self_array;
	self_array.push_back(self_array);
	Dictionary self_dictionary;
	self_dictionary["self"] = self_dictionary;
	source->set_meta("array", self_array);
	source->set_meta("dictionary", self_dictionary);
	fallback->set_meta("array", Array());
	fallback->set_meta("dictionary", Dictionary());
	source->set_meta("peer", child);
	child->set_meta("peer", source);
	fallback->set_meta("peer", old_child);
	old_child->set_meta("peer", fallback);
	HashMap<Node *, HashMap<Ref<Resource>, Ref<Resource>>> cache;
	Ref<Resource> result = SceneState::get_remap_resource(source, cache, fallback, scene);
	CHECK(result == fallback);
	Array result_array = result->get_meta("array");
	Dictionary result_dictionary = result->get_meta("dictionary");
	CHECK(result_array.is_same_instance(Array(result_array[0])));
	CHECK_FALSE(result_array.is_same_instance(self_array));
	CHECK(result_dictionary.is_same_instance(Dictionary(result_dictionary["self"])));
	CHECK_FALSE(result_dictionary.is_same_instance(self_dictionary));
	CHECK(Ref<Resource>(result->get_meta("peer")) == old_child);
	CHECK(Ref<Resource>(old_child->get_meta("peer")) == fallback);
	// Godot containers/resources are reference-counted, not cycle-collected.
	self_array.clear();
	self_dictionary.clear();
	result_array.clear();
	result_dictionary.clear();
	source->remove_meta("peer");
	child->remove_meta("peer");
	fallback->remove_meta("peer");
	old_child->remove_meta("peer");
	cache.clear();
	memdelete(scene);
}

} // namespace TestSceneResourceRemap
