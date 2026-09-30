/**************************************************************************/
/*  test_class_db_custom_instance.cpp                                     */
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

TEST_FORCE_LINK(test_class_db_custom_instance)

#include "core/crypto/crypto.h"
#include "core/io/dtls_server.h"
#include "core/io/http_client.h"
#include "core/io/packet_peer_dtls.h"
#include "core/io/stream_peer_tls.h"
#include "core/object/class_db.h"

namespace TestClassDBCustomInstance {

class CustomInstanceEnabled : public Object {
	GDCLASS(CustomInstanceEnabled, Object);

protected:
	static void _bind_methods() {
		bind_count++;
		ClassDB::bind_method(D_METHOD("get_value"), &CustomInstanceEnabled::get_value);
	}

	void _notification(int p_what) {
		if (p_what == NOTIFICATION_POSTINITIALIZE) {
			postinitialize_count++;
		}
	}

public:
	inline static int bind_count = 0;
	inline static int hook_count = 0;
	inline static int factory_count = 0;
	inline static int constructor_count = 0;
	inline static int postinitialize_count = 0;
	inline static bool last_notify = false;

	static void register_custom_data_to_otdb() { hook_count++; }
	static Object *create(bool p_notify_postinitialize) {
		factory_count++;
		last_notify = p_notify_postinitialize;
		return ClassDB::creator<CustomInstanceEnabled>(p_notify_postinitialize);
	}
	int get_value() const { return 42; }
	CustomInstanceEnabled() { constructor_count++; }
};

class CustomInstanceDisabled : public Object {
	GDCLASS(CustomInstanceDisabled, Object);

protected:
	static void _bind_methods() { bind_count++; }

public:
	inline static int bind_count = 0;
	inline static int hook_count = 0;
	inline static int factory_count = 0;
	inline static int constructor_count = 0;

	static void register_custom_data_to_otdb() { hook_count++; }
	static Object *create(bool p_notify_postinitialize) {
		factory_count++;
		return ClassDB::creator<CustomInstanceDisabled>(p_notify_postinitialize);
	}
	CustomInstanceDisabled() { constructor_count++; }
};

class CustomInstanceInheritedDisabled : public CustomInstanceDisabled {
	GDCLASS(CustomInstanceInheritedDisabled, CustomInstanceDisabled);

protected:
	static void _bind_methods() { child_bind_count++; }

public:
	inline static int child_bind_count = 0;
};

// The discarded registration must not instantiate the helper body, which would
// try to call this deleted factory through _create_ptr_func<T>().
class CustomInstanceNoFactory : public Object {
	GDCLASS(CustomInstanceNoFactory, Object);

public:
	static Object *create(bool) = delete;
};

} // namespace TestClassDBCustomInstance

// Match the explicit specializations emitted by disabled_class_builder().
// These fixture-only types keep the test independent of the engine build profile.
template <>
struct is_class_enabled<TestClassDBCustomInstance::CustomInstanceDisabled> : std::false_type {};
template <>
struct is_class_enabled<TestClassDBCustomInstance::CustomInstanceNoFactory> : std::false_type {};

namespace TestClassDBCustomInstance {

static_assert(GD_IS_CLASS_ENABLED(CustomInstanceEnabled));
static_assert(!GD_IS_CLASS_ENABLED(CustomInstanceDisabled));
static_assert(!GD_IS_CLASS_ENABLED(CustomInstanceInheritedDisabled));
static_assert(!GD_IS_CLASS_ENABLED(CustomInstanceNoFactory));

TEST_CASE("[ClassDB] Custom instance registration respects disabled classes") {
	GDREGISTER_CUSTOM_INSTANCE_CLASS(CustomInstanceDisabled);
	GDREGISTER_CUSTOM_INSTANCE_CLASS(CustomInstanceInheritedDisabled);
	GDREGISTER_CUSTOM_INSTANCE_CLASS(CustomInstanceNoFactory);

	CHECK_FALSE(ClassDB::class_exists("CustomInstanceDisabled"));
	CHECK_FALSE(ClassDB::class_exists("CustomInstanceInheritedDisabled"));
	CHECK_FALSE(ClassDB::class_exists("CustomInstanceNoFactory"));
	CHECK(CustomInstanceDisabled::get_gdtype_static().get_init_state() == GDType::InitState::UNINITIALIZED);
	CHECK(CustomInstanceInheritedDisabled::get_gdtype_static().get_init_state() == GDType::InitState::UNINITIALIZED);
	CHECK(CustomInstanceNoFactory::get_gdtype_static().get_init_state() == GDType::InitState::UNINITIALIZED);
	CHECK(CustomInstanceDisabled::bind_count == 0);
	CHECK(CustomInstanceInheritedDisabled::child_bind_count == 0);
	CHECK(CustomInstanceDisabled::hook_count == 0);
	CHECK(CustomInstanceDisabled::factory_count == 0);
	CHECK(CustomInstanceDisabled::constructor_count == 0);
}

TEST_CASE("[ClassDB] Custom instance registration preserves enabled factory behavior") {
	// Avoid adding this fixture to the production API inspected by other tests.
	const ClassDB::APIType previous_api = ClassDB::get_current_api();
	ClassDB::set_current_api(ClassDB::API_NONE);
	GDREGISTER_CUSTOM_INSTANCE_CLASS(CustomInstanceEnabled);
	ClassDB::set_current_api(previous_api);

	CHECK(ClassDB::class_exists("CustomInstanceEnabled"));
	CHECK(ClassDB::is_class_exposed("CustomInstanceEnabled"));
	CHECK(ClassDB::is_class_enabled("CustomInstanceEnabled"));
	CHECK(ClassDB::can_instantiate("CustomInstanceEnabled"));
	CHECK(ClassDB::get_api_type("CustomInstanceEnabled") == ClassDB::API_NONE);
	CHECK(CustomInstanceEnabled::get_gdtype_static().get_init_state() != GDType::InitState::UNINITIALIZED);
	CHECK(CustomInstanceEnabled::bind_count == 1);
	CHECK(CustomInstanceEnabled::hook_count == 1);
	CHECK(CustomInstanceEnabled::factory_count == 0);
	CHECK(CustomInstanceEnabled::constructor_count == 0);
	CHECK(ClassDB::get_method("CustomInstanceEnabled", "get_value") != nullptr);

	Object *instance = ClassDB::instantiate("CustomInstanceEnabled");
	REQUIRE(instance != nullptr);
	CHECK(Object::cast_to<CustomInstanceEnabled>(instance) != nullptr);
	CHECK(instance->call("get_value") == Variant(42));
	CHECK(CustomInstanceEnabled::factory_count == 1);
	CHECK(CustomInstanceEnabled::constructor_count == 1);
	CHECK(CustomInstanceEnabled::last_notify);
	CHECK(CustomInstanceEnabled::postinitialize_count == 1);
	memdelete(instance);

	instance = ClassDB::instantiate_without_postinitialization("CustomInstanceEnabled");
	REQUIRE(instance != nullptr);
	CHECK(CustomInstanceEnabled::factory_count == 2);
	CHECK(CustomInstanceEnabled::constructor_count == 2);
	CHECK_FALSE(CustomInstanceEnabled::last_notify);
	CHECK(CustomInstanceEnabled::postinitialize_count == 1);
	memdelete(instance);
}

// Also cover the actual core registration call sites. A build_profile can
// disable these explicitly or via a parent; internal initialization is allowed,
// but a disabled class must not be exposed by custom registration.
template <typename T>
void check_core_custom_instance_class() {
	const StringName name = T::get_class_static();
	INFO("Core class: ", name);
	if constexpr (GD_IS_CLASS_ENABLED(T)) {
		REQUIRE(ClassDB::class_exists(name));
		CHECK(ClassDB::is_class_exposed(name));
	} else if (ClassDB::class_exists(name)) {
		CHECK_FALSE(ClassDB::is_class_exposed(name));
	}
}

TEST_CASE("[ClassDB] Core custom instance classes respect the build profile") {
	check_core_custom_instance_class<HTTPClient>();
	check_core_custom_instance_class<X509Certificate>();
	check_core_custom_instance_class<CryptoKey>();
	check_core_custom_instance_class<HMACContext>();
	check_core_custom_instance_class<Crypto>();
	check_core_custom_instance_class<StreamPeerTLS>();
	check_core_custom_instance_class<PacketPeerDTLS>();
	check_core_custom_instance_class<DTLSServer>();
}

} // namespace TestClassDBCustomInstance
