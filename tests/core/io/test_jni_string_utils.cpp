/**************************************************************************/
/*  test_jni_string_utils.cpp                                             */
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

TEST_FORCE_LINK(test_jni_string_utils)

#ifdef ANDROID_ENABLED

#include "platform/android/jni_string_utils.h"

#include <type_traits>
#include <utility>

namespace TestJNIStringUtils {

struct TestString : _jstring {
	Vector<jchar> data;
};

static jstring JNICALL new_string(JNIEnv *p_env, const jchar *p_chars, jsize p_length) {
	TestString *string = new TestString;
	string->data.resize(p_length + 1);
	for (int i = 0; i < p_length; i++) {
		string->data.write[i] = p_chars[i];
	}
	string->data.write[p_length] = 0;
	return string;
}

static const jchar *JNICALL get_string_chars(JNIEnv *p_env, jstring p_string, jboolean *p_is_copy) {
	return static_cast<TestString *>(p_string)->data.ptr();
}

static jsize JNICALL get_string_length(JNIEnv *p_env, jstring p_string) {
	return static_cast<TestString *>(p_string)->data.size() - 1;
}

static void JNICALL release_string_chars(JNIEnv *p_env, jstring p_string, const jchar *p_chars) {
}

TEST_CASE("[Android][JNI] File path strings preserve Unicode through UTF-16") {
	using JNIFunctions = std::remove_const_t<std::remove_pointer_t<decltype(std::declval<JNIEnv>().functions)>>;
	JNIFunctions functions = {};
	functions.NewString = new_string;
	functions.GetStringChars = get_string_chars;
	functions.GetStringLength = get_string_length;
	functions.ReleaseStringChars = release_string_chars;
	JNIEnv env = {};
	env.functions = &functions;

	const String paths[] = {
		String(),
		"content://provider/tree/primary%3AGodot#",
		"content://provider/tree/root#literal%252F/hash%23name",
		String::utf8("目录/é/🗂️/😀.txt"),
		"back\\slash/space name.txt",
		String::chr(0xfeff),
		String::chr(0xfffe),
		String::chr(0xfeff) + String::chr(0xfffe) + String::chr(0xfeff),
		String::chr(0xfeff) + String::utf8("😀中文"),
		String::chr(0xfffe) + String::utf8("😀中文"),
		String::chr(0xfeff) + String::chr(0xfffe) + String::chr(0xfffe) + String::chr(0xfeff) + "file",
		"middle" + String::chr(0xfeff) + "name",
		"middle" + String::chr(0xfffe) + String::utf8("😀"),
	};
	for (const String &path : paths) {
		jstring encoded = string_to_jstring_utf16(path, &env);
		CHECK(bool(jstring_to_string_utf16(encoded, &env) == path));
		delete static_cast<TestString *>(encoded);
	}
	CHECK(jstring_to_string_utf16(nullptr, &env).is_empty());

	jstring emoji = string_to_jstring_utf16(String::utf8("😀"), &env);
	const TestString *encoded_emoji = static_cast<TestString *>(emoji);
	CHECK(encoded_emoji->data.size() == 3);
	CHECK(encoded_emoji->data[0] == 0xd83d);
	CHECK(encoded_emoji->data[1] == 0xde00);
	delete encoded_emoji;
}

} // namespace TestJNIStringUtils

#endif // ANDROID_ENABLED
