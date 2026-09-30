/**************************************************************************/
/*  test_class_db_defaults.cpp                                            */
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

TEST_FORCE_LINK(test_class_db_defaults)

#ifdef THREADS_ENABLED

#include "core/object/class_db.h"
#include "core/os/semaphore.h"
#include "core/os/thread.h"

namespace TestClassDBDefaults {

class ConcurrentDefaultsObject : public Object {
	GDCLASS(ConcurrentDefaultsObject, Object);

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_value"), &ConcurrentDefaultsObject::get_value);
		ADD_PROPERTY(PropertyInfo(Variant::INT, "value"), "", "get_value");
	}

public:
	inline static SafeNumeric<uint32_t> constructor_count{ 0 };
	inline static SafeNumeric<uint32_t> nested_lookup_failures{ 0 };
	int get_value() const { return 73; }
	ConcurrentDefaultsObject() {
		constructor_count.increment();
		// Default-property construction may itself query another class's defaults.
		// This reenters the ClassDB write lock without recursing into this class.
		bool valid = false;
		Variant nested = ClassDB::class_get_default_property_value("Resource", "resource_local_to_scene", &valid);
		if (!valid || nested.get_type() != Variant::BOOL || bool(nested)) {
			nested_lookup_failures.increment();
		}
	}
};

TEST_CASE("[ClassDB][FirstFixBatch] Concurrent cold default cache initialization") {
	const ClassDB::APIType previous_api = ClassDB::get_current_api();
	ClassDB::set_current_api(ClassDB::API_NONE);
	ClassDB::register_class<ConcurrentDefaultsObject>();
	ClassDB::set_current_api(previous_api);
	REQUIRE_EQ(ConcurrentDefaultsObject::constructor_count.get(), 0);
	struct Context {
		Semaphore ready;
		Semaphore start;
		StringName class_name = "ConcurrentDefaultsObject";
		StringName property_name = "value";
		StringName missing_property = "missing_property";
		SafeNumeric<uint32_t> failures{ 0 };
	} context;
	Thread threads[4];
	for (Thread &thread : threads) {
		thread.start([](void *p_data) {
			Context *ctx = static_cast<Context *>(p_data);
			ctx->ready.post();
			ctx->start.wait();
			for (int iteration = 0; iteration < 256; iteration++) {
				bool valid = false;
				Variant value = ClassDB::class_get_default_property_value(ctx->class_name, ctx->property_name, &valid);
				if (!valid || value.get_type() != Variant::INT || int(value) != 73) {
					ctx->failures.increment();
				}
				valid = true;
				value = ClassDB::class_get_default_property_value(ctx->class_name, ctx->missing_property, &valid);
				if (valid || value.get_type() != Variant::NIL) {
					ctx->failures.increment();
				}
			}
		}, &context);
	}
	for (int i = 0; i < 4; i++) {
		context.ready.wait();
	}
	context.start.post(4);
	for (Thread &thread : threads) {
		thread.wait_to_finish();
	}
	CHECK_EQ(context.failures.get(), 0);
	CHECK_EQ(ConcurrentDefaultsObject::constructor_count.get(), 1);
	CHECK_EQ(ConcurrentDefaultsObject::nested_lookup_failures.get(), 0);
}

} // namespace TestClassDBDefaults

#endif // THREADS_ENABLED
