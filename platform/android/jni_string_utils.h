/**************************************************************************/
/*  jni_string_utils.h                                                    */
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

#pragma once

#include "core/string/ustring.h"

#include <jni.h>

// JNI's UTF-8 methods use modified UTF-8, which differs from Godot's UTF-8
// encoding for supplementary Unicode characters. Use UTF-16 for file and
// directory names so that provider display names can round-trip losslessly.
static inline jstring string_to_jstring_utf16(const String &p_string, JNIEnv *p_env) {
	const Char16String utf16 = p_string.utf16();
	return p_env->NewString(reinterpret_cast<const jchar *>(utf16.get_data()), utf16.length());
}

static inline String jstring_to_string_utf16(jstring p_string, JNIEnv *p_env) {
	if (!p_string) {
		return String();
	}
	const jchar *utf16 = p_env->GetStringChars(p_string, nullptr);
	if (!utf16) {
		return String();
	}
	const int length = p_env->GetStringLength(p_string);
	// Java strings contain native-endian code units, not an encoded byte stream.
	// Keep leading U+FEFF/U+FFFE as literal filename characters: String::utf16()
	// otherwise consumes the first one as a BOM and may change the byte order.
	int prefix_length = 0;
	while (prefix_length < length && (utf16[prefix_length] == 0xfeff || utf16[prefix_length] == 0xfffe)) {
		prefix_length++;
	}
	String prefix;
	if (prefix_length > 0) {
		prefix.resize_uninitialized(prefix_length + 1);
		char32_t *prefix_data = prefix.ptrw();
		for (int i = 0; i < prefix_length; i++) {
			prefix_data[i] = utf16[i];
		}
		prefix_data[prefix_length] = 0;
	}
	const String result = prefix + String::utf16(reinterpret_cast<const char16_t *>(utf16 + prefix_length), length - prefix_length);
	p_env->ReleaseStringChars(p_string, utf16);
	return result;
}
