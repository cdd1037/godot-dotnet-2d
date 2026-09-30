/**************************************************************************/
/*  test_third_fix_batch.cpp                                                    */
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

TEST_FORCE_LINK(test_third_fix_batch)

#include "core/io/file_access_memory.h"
#include "core/string/node_path.h"
#include "core/templates/cowdata.h"
#include "scene/resources/resource_format_text.h"

namespace TestThirdFixBatch {

TEST_CASE("[NodePath][ThirdFixBatch] Mutating copies preserves original paths and caches") {
	for (bool cache_first : { false, true }) {
		NodePath original("/Root/Branch/../Leaf:position:x");
		if (cache_first) {
			original.hash();
			CHECK(original.get_concatenated_names() == "Root/Branch/../Leaf");
			CHECK(original.get_concatenated_subnames() == "position:x");
		}
		NodePath copy = original;
		copy.simplify();
		CHECK(String(original) == "/Root/Branch/../Leaf:position:x");
		CHECK(String(copy) == "/Root/Leaf:position:x");
		CHECK(copy.is_absolute());
		CHECK(copy.get_concatenated_subnames() == "position:x");
		CHECK(original.hash() == NodePath("/Root/Branch/../Leaf:position:x").hash());
		CHECK(copy.hash() == NodePath("/Root/Leaf:position:x").hash());
		CHECK(String(original.simplified()) == "/Root/Leaf:position:x");
		CHECK(String(original) == "/Root/Branch/../Leaf:position:x");
	}
	NodePath original("Root/Leaf:position");
	NodePath copy = original;
	copy.prepend_period();
	CHECK(String(original) == "Root/Leaf:position");
	CHECK(String(copy) == "./Root/Leaf:position");
	copy.prepend_period();
	CHECK(String(copy) == "./Root/Leaf:position");
	copy.simplify();
	CHECK(copy == original);
	NodePath empty;
	empty.simplify();
	CHECK(empty.is_empty());
}

TEST_CASE("[CowData][ThirdFixBatch] Explicit empty initializer lists remain empty") {
	CowData<int> empty(std::initializer_list<int>{});
	CHECK(empty.size() == 0);
	CHECK(empty.ptr() == nullptr);
	CHECK(empty.resize(1) == OK);
	empty.set(0, 42);
	CHECK(empty.get(0) == 42);
	CowData<String> strings(std::initializer_list<String>{});
	CHECK(strings.size() == 0);
	CHECK(strings.ptr() == nullptr);
	CowData<int> populated({ 2, 3 });
	CHECK(populated.size() == 2);
	CHECK(populated.get(1) == 3);
}

TEST_CASE("[String][ThirdFixBatch] Empty UTF32 append does not allocate or detach") {
	String empty;
	empty.append_utf32_unchecked(Span<char32_t>());
	CHECK(empty.is_empty());
	CHECK(empty.ptr() == nullptr);
	String original("unchanged");
	String copy = original;
	const char32_t *before = copy.ptr();
	copy.append_utf32_unchecked(Span<char32_t>());
	CHECK(copy == original);
	CHECK(copy.ptr() == before);
	const char32_t suffix[] = U"\U0001F642";
	copy.append_utf32_unchecked(Span<char32_t>(suffix));
	CHECK(copy.length() == original.length() + 1);
	CHECK(copy[copy.length() - 1] == 0x1F642);
	CHECK(original == "unchanged");
}

TEST_CASE("[SceneTree][PackedScene][ThirdFixBatch] Missing scene node names fail cleanly") {
	for (const char *text : {
			 "[gd_scene format=3]\n[node type=\"Node\"]\n",
			 "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n[node type=\"Node\" parent=\".\"]\n" }) {
		Ref<FileAccessMemory> file;
		file.instantiate();
		REQUIRE(file->open_custom(reinterpret_cast<const uint8_t *>(text), strlen(text)) == OK);
		ResourceLoaderText loader;
		loader.open(file);
		ERR_PRINT_OFF;
		const Error error = loader.load();
		ERR_PRINT_ON;
		CHECK(error == ERR_FILE_CORRUPT);
	}
	const char *valid = "[gd_scene format=3]\n[node name=\"Root\" type=\"Node\"]\n";
	Ref<FileAccessMemory> file;
	file.instantiate();
	REQUIRE(file->open_custom(reinterpret_cast<const uint8_t *>(valid), strlen(valid)) == OK);
	ResourceLoaderText loader;
	loader.open(file);
	CHECK(loader.load() == OK);
	Ref<PackedScene> scene = loader.get_resource();
	REQUIRE(scene.is_valid());
	Node *root = scene->instantiate();
	REQUIRE(root != nullptr);
	CHECK(root->get_name() == "Root");
	memdelete(root);
}

} // namespace TestThirdFixBatch
