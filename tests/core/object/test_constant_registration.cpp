/**************************************************************************/
/*  test_constant_registration.cpp                                        */
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

TEST_FORCE_LINK(test_constant_registration)

#include "core/core_constants.h"
#include "core/object/gdtype.h"
#include "core/os/keyboard.h"
#include "core/variant/type_info.h"
#include "core/variant/variant_caster.h"

namespace TestConstantRegistration {

TEST_CASE("[ClassDB][ConstantRegistration] Qualified names and property metadata") {
	using GodotTypeInfo::Internal::enum_qualified_name_to_class_info_name;
	CHECK(enum_qualified_name_to_class_info_name("") == "");
	CHECK(enum_qualified_name_to_class_info_name("Enum") == "Enum");
	CHECK(enum_qualified_name_to_class_info_name("Class::Enum") == "Class.Enum");
	CHECK(enum_qualified_name_to_class_info_name("Namespace::Class::Enum") == "Class.Enum");
	CHECK(enum_qualified_name_to_class_info_name("Outer::Inner::Class::Enum") == "Class.Enum");
	CHECK(enum_qualified_name_to_class_info_name("::Class::Enum") == "Class.Enum");

	const PropertyInfo enum_info = GetTypeInfo<Key>::get_class_info();
	CHECK(enum_info.class_name == StringName("Key"));
	CHECK(enum_info.type == Variant::INT);
	CHECK((enum_info.usage & PROPERTY_USAGE_CLASS_IS_ENUM) != 0);
	const PropertyInfo bitfield_info = GetTypeInfo<BitField<KeyModifierMask>>::get_class_info();
	CHECK(bitfield_info.class_name == StringName("KeyModifierMask"));
	CHECK(bitfield_info.type == Variant::INT);
	CHECK((bitfield_info.usage & PROPERTY_USAGE_CLASS_IS_BITFIELD) != 0);
	CHECK(String(GetTypeInfo<KeyModifierMask>::enum_qualified_name) == GetTypeInfo<BitField<KeyModifierMask>>::enum_qualified_name);
}

TEST_CASE("[ClassDB][ConstantRegistration] Raw binding preserves old GDType maps") {
	GDType original(nullptr, "Original");
	GDType compact(nullptr, "Compact");
	original.initialize();
	compact.initialize();
	struct Binding {
		const char *qualified_enum;
		const char *qualified_value;
		const char *expected_enum;
		const char *expected_value;
		int64_t value;
		bool bitfield;
	};
	const Binding bindings[] = {
		{ "", "PLAIN", "", "PLAIN", INT64_MIN, false },
		{ "Enum", "LOW", "Enum", "LOW", -1, false },
		{ "Class::Enum", "Enum::HIGH", "Class.Enum", "HIGH", INT64_MAX, false },
		{ "Space::Class::Flags", "FLAG", "Class.Flags", "FLAG", INT64_C(1) << 40, true },
		{ "Space::Class::Flags", "Flags::SECOND", "Class.Flags", "SECOND", 2, true },
		// Preserve the pre-existing second-slice rule, rather than silently
		// changing names of multiply-qualified constant expressions.
		{ "Space::Class::Other", "Class::Enum::VALUE", "Class.Other", "Enum", 3, false },
	};
	for (const Binding &binding : bindings) {
		original.bind_integer_constant(binding.expected_enum, binding.expected_value, binding.value, binding.bitfield);
		compact.bind_integer_constant_raw(binding.qualified_enum, binding.qualified_value, binding.value, binding.bitfield);
	}
	CHECK(original.get_integer_constant_map(true).size() == compact.get_integer_constant_map(true).size());
	CHECK(original.get_enum_map(true).size() == compact.get_enum_map(true).size());
	for (const Binding &binding : bindings) {
		const int64_t *value = compact.get_integer_constant_map(true).getptr(binding.expected_value);
		REQUIRE(value != nullptr);
		CHECK(*value == binding.value);
		const GDType::EnumInfo *old_enum = original.get_integer_constant_enum(binding.expected_value, true);
		const GDType::EnumInfo *new_enum = compact.get_integer_constant_enum(binding.expected_value, true);
		if (!old_enum) {
			CHECK(new_enum == nullptr);
			continue;
		}
		REQUIRE(new_enum != nullptr);
		CHECK(new_enum->name == old_enum->name);
		CHECK(new_enum->is_bitfield == old_enum->is_bitfield);
		CHECK(new_enum->values.size() == old_enum->values.size());
		CHECK(new_enum->values[binding.expected_value] == old_enum->values[binding.expected_value]);
	}
	GDType child(&compact, "Child");
	child.initialize();
	CHECK(child.get_integer_constant_map().size() == compact.get_integer_constant_map().size());
	CHECK(child.get_integer_constant_map(true).is_empty());
	CHECK(child.get_integer_constant_enum("FLAG")->is_bitfield);
}

TEST_CASE("[ClassDB][ConstantRegistration] Global values and enum membership") {
	for (int index = 0; index < CoreConstants::get_global_constant_count(); index++) {
		const StringName name(CoreConstants::get_global_constant_name(index));
		CHECK(CoreConstants::is_global_constant(name));
		CHECK(CoreConstants::get_global_constant_index(name) == index);
		const StringName enum_name = CoreConstants::get_global_constant_enum(index);
		if (enum_name != StringName()) {
			CHECK(CoreConstants::is_global_enum(enum_name));
			HashMap<StringName, int64_t> values;
			CoreConstants::get_enum_values(enum_name, &values);
			REQUIRE(values.has(name));
			CHECK(values[name] == CoreConstants::get_global_constant_value(index));
		}
#ifdef DEBUG_ENABLED
		// No current registration uses a NO_VAL macro.
		CHECK_FALSE(CoreConstants::get_ignore_value_in_docs(index));
#endif
	}
	const int plain = CoreConstants::get_global_constant_index("INT64_MIN");
	CHECK(CoreConstants::get_global_constant_value(plain) == INT64_MIN);
	CHECK(CoreConstants::get_global_constant_enum(plain) == StringName());
	const int key = CoreConstants::get_global_constant_index("KEY_ESCAPE");
	CHECK(CoreConstants::get_global_constant_enum(key) == StringName("Key"));
	CHECK(CoreConstants::get_global_constant_value(key) == static_cast<int64_t>(Key::ESCAPE));
#ifdef DEBUG_ENABLED
	CHECK_FALSE(CoreConstants::is_global_constant_bitfield(key));
	CHECK(CoreConstants::is_global_constant_bitfield(CoreConstants::get_global_constant_index("KEY_MASK_SHIFT")));
#endif
	const int custom = CoreConstants::get_global_constant_index("TYPE_NIL");
	CHECK(CoreConstants::get_global_constant_enum(custom) == StringName("Variant.Type"));
	CHECK(CoreConstants::get_global_constant_value(custom) == Variant::NIL);
}

} // namespace TestConstantRegistration
