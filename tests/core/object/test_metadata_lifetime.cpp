/**************************************************************************/
/*  test_metadata_lifetime.cpp                                        */
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

TEST_FORCE_LINK(test_metadata_lifetime)

#include "core/object/method_info.h"
#include "core/string/string_name.h"

namespace TestMetadataLifetime {

TEST_CASE("[StringName][NinthFixBatch] Copies and moves retain interned names") {
	StringName retained;
	uint32_t hash = 0;
	{
		StringName original("ninth_batch_interned_name");
		hash = original.hash();
		StringName copy = original;
		StringName moved(std::move(copy));
		CHECK(copy.is_empty());
		CHECK(moved == original);
		retained = std::move(moved);
		CHECK(moved.is_empty());
		CHECK(retained.hash() == hash);
	}
	CHECK(String(retained) == "ninth_batch_interned_name");
	CHECK(retained.hash() == hash);
	for (int i = 0; i < 64; i++) {
		StringName reinterned("ninth_batch_interned_name");
		CHECK(reinterned == retained);
		StringName temporary = retained;
		temporary = StringName("another_name");
		CHECK(retained == reinterned);
	}
	retained = StringName();
	CHECK(retained.is_empty());
}

TEST_CASE("[PropertyInfo][NinthFixBatch] Copy move and vector relocation preserve property metadata") {
	const PropertyInfo original(Variant::OBJECT, "texture", PROPERTY_HINT_RESOURCE_TYPE, "Texture2D", PROPERTY_USAGE_STORAGE | PROPERTY_USAGE_EDITOR);
	PropertyInfo copy(original);
	CHECK(copy == original);
	PropertyInfo moved(std::move(copy));
	CHECK(moved == original);
	PropertyInfo assigned;
	assigned = original;
	CHECK(assigned == original);
	PropertyInfo move_assigned;
	move_assigned = std::move(assigned);
	CHECK(move_assigned == original);
	Vector<PropertyInfo> properties;
	for (int i = 0; i < 64; i++) {
		properties.push_back(original);
	}
	Vector<PropertyInfo> duplicate = properties;
	duplicate.write[0].name = "changed";
	CHECK(properties[0] == original);
	CHECK(duplicate[0].name == "changed");
	for (const PropertyInfo &property : properties) {
		CHECK(property == original);
	}
}

TEST_CASE("[MethodInfo][NinthFixBatch] Copy move preserves arguments defaults flags and metadata") {
	MethodInfo original(Variant::INT, "metadata_method", PropertyInfo(Variant::STRING, "text"), PropertyInfo(Variant::INT, "count"));
	original.flags = METHOD_FLAG_CONST | METHOD_FLAG_VIRTUAL;
	original.id = 73;
	original.return_val_metadata = 2;
	original.arguments_metadata = { 1, 2 };
	original.default_arguments = { Variant("default"), Variant(INT64_C(1) << 40) };
	const auto check = [&original](const MethodInfo &p_info) {
		CHECK(p_info.name == original.name);
		CHECK(p_info.return_val == original.return_val);
		CHECK(p_info.flags == original.flags);
		CHECK(p_info.id == original.id);
		REQUIRE(p_info.arguments.size() == original.arguments.size());
		for (int i = 0; i < original.arguments.size(); i++) {
			CHECK(p_info.arguments[i] == original.arguments[i]);
		}
		CHECK(p_info.default_arguments == original.default_arguments);
		CHECK(p_info.return_val_metadata == original.return_val_metadata);
		CHECK(p_info.arguments_metadata == original.arguments_metadata);
	};
	MethodInfo copy(original);
	check(copy);
	MethodInfo moved(std::move(copy));
	check(moved);
	MethodInfo assigned;
	assigned = original;
	check(assigned);
	MethodInfo move_assigned;
	move_assigned = std::move(assigned);
	check(move_assigned);
	Vector<MethodInfo> methods;
	for (int i = 0; i < 32; i++) {
		methods.push_back(original);
	}
	for (const MethodInfo &method : methods) {
		check(method);
	}
}

} // namespace TestMetadataLifetime
