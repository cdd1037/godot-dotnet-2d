/**************************************************************************/
/*  dir_access_jandroid.cpp                                               */
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

#include "dir_access_jandroid.h"

#include "jni_string_utils.h"
#include "thread_jandroid.h"

#include "core/string/print_string.h"

jobject DirAccessJAndroid::dir_access_handler = nullptr;
jclass DirAccessJAndroid::cls = nullptr;
jmethodID DirAccessJAndroid::_dir_open = nullptr;
jmethodID DirAccessJAndroid::_dir_next = nullptr;
jmethodID DirAccessJAndroid::_dir_close = nullptr;
jmethodID DirAccessJAndroid::_dir_is_dir = nullptr;
jmethodID DirAccessJAndroid::_dir_exists = nullptr;
jmethodID DirAccessJAndroid::_file_exists = nullptr;
jmethodID DirAccessJAndroid::_is_readable = nullptr;
jmethodID DirAccessJAndroid::_is_writable = nullptr;
jmethodID DirAccessJAndroid::_get_drive_count = nullptr;
jmethodID DirAccessJAndroid::_get_drive = nullptr;
jmethodID DirAccessJAndroid::_make_dir = nullptr;
jmethodID DirAccessJAndroid::_make_dir_recursive = nullptr;
jmethodID DirAccessJAndroid::_normalize_path = nullptr;
jmethodID DirAccessJAndroid::_get_space_left = nullptr;
jmethodID DirAccessJAndroid::_rename = nullptr;
jmethodID DirAccessJAndroid::_remove = nullptr;
jmethodID DirAccessJAndroid::_current_is_hidden = nullptr;

Error DirAccessJAndroid::list_dir_begin() {
	list_dir_end();
	int res = dir_open(current_dir);
	if (res <= 0) {
		return ERR_CANT_OPEN;
	}

	id = res;

	return OK;
}

String DirAccessJAndroid::get_next() {
	ERR_FAIL_COND_V(id == 0, "");
	if (_dir_next) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, "");
		jstring str = (jstring)env->CallObjectMethod(dir_access_handler, _dir_next, id);
		if (!str) {
			return "";
		}

		String ret = jstring_to_string_utf16((jstring)str, env);
		env->DeleteLocalRef((jobject)str);
		return ret;
	} else {
		return "";
	}
}

bool DirAccessJAndroid::current_is_dir() const {
	if (_dir_is_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(dir_access_handler, _dir_is_dir, id);
	} else {
		return false;
	}
}

bool DirAccessJAndroid::current_is_hidden() const {
	if (_current_is_hidden) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);
		return env->CallBooleanMethod(dir_access_handler, _current_is_hidden, id);
	}
	return false;
}

void DirAccessJAndroid::list_dir_end() {
	if (id == 0) {
		return;
	}

	dir_close(id);
	id = 0;
}

int DirAccessJAndroid::get_drive_count() {
	if (current_dir.begins_with("content://")) {
		return 0; // A document tree has no physical filesystem drives.
	}
	if (_get_drive_count) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		return env->CallIntMethod(dir_access_handler, _get_drive_count, get_access_type());
	} else {
		return 0;
	}
}

String DirAccessJAndroid::get_drive(int p_drive) {
	if (current_dir.begins_with("content://")) {
		return String();
	}
	if (_get_drive) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, "");
		jstring j_drive = (jstring)env->CallObjectMethod(dir_access_handler, _get_drive, get_access_type(), p_drive);
		if (!j_drive) {
			return "";
		}

		String drive = jstring_to_string_utf16(j_drive, env);
		env->DeleteLocalRef(j_drive);
		return drive;
	} else {
		return "";
	}
}

int DirAccessJAndroid::get_current_drive() {
	if (current_dir.begins_with("content://")) {
		return 0;
	}
	return DirAccessUnix::get_current_drive();
}

String DirAccessJAndroid::_get_root_string() const {
	if (current_dir.begins_with("content://")) {
		const int separator = current_dir.find_char('#');
		return separator == -1 ? current_dir + "#" : current_dir.substr(0, separator + 1);
	}
	if (get_access_type() == ACCESS_FILESYSTEM) {
		return "/";
	}
	return DirAccessUnix::_get_root_string();
}

String DirAccessJAndroid::get_current_dir(bool p_include_drive) const {
	if (current_dir.begins_with("content://")) {
		// A content URI must retain its authority and grant anchor, even when
		// p_include_drive is false. Neither is a filesystem drive prefix.
		return current_dir;
	}
	String base = _get_root_path();
	String bd = current_dir;
	if (!base.is_empty()) {
		bd = current_dir.replace_first(base, "");
	}

	String root_string = _get_root_string();
	if (bd.begins_with(root_string)) {
		return bd;
	} else if (bd.begins_with("/")) {
		return root_string + bd.substr(1);
	} else {
		return root_string + bd;
	}
}

Error DirAccessJAndroid::change_dir(String p_dir) {
	String new_dir = get_absolute_path(p_dir);
	if (new_dir.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (new_dir == current_dir && !new_dir.begins_with("content://")) {
		return OK;
	}

	if (!dir_exists(new_dir)) {
		return ERR_INVALID_PARAMETER;
	}

	current_dir = new_dir;
	return OK;
}

String DirAccessJAndroid::fix_path(const String &p_path) const {
	// Unix simplification treats document IDs and URI fragments as physical
	// directories. Expand res:// and user:// without applying it to SAF paths.
	const String path = DirAccess::fix_path(p_path);
	return path.begins_with("content://") ? path : path.simplify_path();
}

String DirAccessJAndroid::get_absolute_path(String p_path) const {
	p_path = DirAccess::fix_path(p_path);
	if (p_path.begins_with("content://") || (current_dir.begins_with("content://") && p_path.is_relative_path())) {
		if (!_normalize_path) {
			return String();
		}
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, String());

		jstring j_current = string_to_jstring_utf16(current_dir, env);
		jstring j_path = string_to_jstring_utf16(p_path, env);
		jstring j_normalized = (jstring)env->CallObjectMethod(dir_access_handler, _normalize_path, j_current, j_path);
		env->DeleteLocalRef(j_current);
		env->DeleteLocalRef(j_path);
		if (!j_normalized) {
			return String();
		}
		const String normalized = jstring_to_string_utf16(j_normalized, env);
		env->DeleteLocalRef(j_normalized);
		return normalized;
	}

	if (p_path.is_relative_path()) {
		p_path = get_current_dir().path_join(p_path);
	}
	return fix_path(p_path);
}

bool DirAccessJAndroid::file_exists(String p_file) {
	if (_file_exists) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		String path = get_absolute_path(p_file);
		if (path.is_empty()) {
			return false;
		}
		jstring j_path = string_to_jstring_utf16(path, env);
		bool result = env->CallBooleanMethod(dir_access_handler, _file_exists, get_access_type(), j_path);
		env->DeleteLocalRef(j_path);
		return result;
	} else {
		return false;
	}
}

bool DirAccessJAndroid::dir_exists(String p_dir) {
	if (_dir_exists) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, false);

		String path = get_absolute_path(p_dir);
		if (path.is_empty()) {
			return false;
		}
		jstring j_path = string_to_jstring_utf16(path, env);
		bool result = env->CallBooleanMethod(dir_access_handler, _dir_exists, get_access_type(), j_path);
		env->DeleteLocalRef(j_path);
		return result;
	} else {
		return false;
	}
}

bool DirAccessJAndroid::is_readable(String p_dir) {
	const String path = get_absolute_path(p_dir);
	if (path.is_empty()) {
		return false;
	}
	if (!path.begins_with("content://")) {
		return DirAccessUnix::is_readable(path);
	}
	if (!_is_readable) {
		return false;
	}

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, false);
	jstring j_path = string_to_jstring_utf16(path, env);
	const bool result = env->CallBooleanMethod(dir_access_handler, _is_readable, get_access_type(), j_path);
	env->DeleteLocalRef(j_path);
	return result;
}

bool DirAccessJAndroid::is_writable(String p_dir) {
	const String path = get_absolute_path(p_dir);
	if (path.is_empty()) {
		return false;
	}
	if (!path.begins_with("content://")) {
		return DirAccessUnix::is_writable(path);
	}
	if (!_is_writable) {
		return false;
	}

	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL_V(env, false);
	jstring j_path = string_to_jstring_utf16(path, env);
	const bool result = env->CallBooleanMethod(dir_access_handler, _is_writable, get_access_type(), j_path);
	env->DeleteLocalRef(j_path);
	return result;
}

Error DirAccessJAndroid::make_dir(String p_dir) {
	// Check if the directory exists already
	if (dir_exists(p_dir)) {
		return ERR_ALREADY_EXISTS;
	}

	if (_make_dir) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		String path = get_absolute_path(p_dir);
		if (path.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		jstring j_dir = string_to_jstring_utf16(path, env);
		bool result = env->CallBooleanMethod(dir_access_handler, _make_dir, get_access_type(), j_dir);
		env->DeleteLocalRef(j_dir);
		if (result) {
			return OK;
		} else {
			return FAILED;
		}
	} else {
		return ERR_UNCONFIGURED;
	}
}

Error DirAccessJAndroid::make_dir_recursive(const String &p_dir) {
	const String path = get_absolute_path(p_dir);
	if (path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (path.begins_with("content://")) {
		if (!_make_dir_recursive) {
			return ERR_UNCONFIGURED;
		}
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);
		jstring j_path = string_to_jstring_utf16(path, env);
		const bool result = env->CallBooleanMethod(dir_access_handler, _make_dir_recursive, get_access_type(), j_path);
		env->DeleteLocalRef(j_path);
		return result ? OK : FAILED;
	}
	Error err = make_dir(p_dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		ERR_FAIL_V_MSG(err, "Could not create directory: " + p_dir);
	}
	return OK;
}

Error DirAccessJAndroid::copy(const String &p_from, const String &p_to, int p_chmod_flags) {
	const String from_path = get_absolute_path(p_from);
	const String to_path = get_absolute_path(p_to);
	if (from_path.is_empty() || to_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (from_path.begins_with("content://") || to_path.begins_with("content://")) {
		// FileAccess has no current directory. Resolve raw relative display names
		// before forwarding the user's explicit file-copy request to it.
		return DirAccess::copy(from_path, to_path, p_chmod_flags);
	}
	return DirAccess::copy(p_from, p_to, p_chmod_flags);
}

Error DirAccessJAndroid::copy_dir(const String &p_from, String p_to, int p_chmod_flags, bool p_copy_links) {
	const String from_path = get_absolute_path(p_from);
	const String to_path = get_absolute_path(p_to);
	if (from_path.is_empty() || to_path.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	if (from_path.begins_with("content://") || to_path.begins_with("content://")) {
		// The generic recursive copier appends raw display names to absolute
		// paths. Those names must be URI-encoded before being used with SAF.
		// Fail before creating or writing anything at the destination.
		return ERR_UNAVAILABLE;
	}
	return DirAccess::copy_dir(p_from, p_to, p_chmod_flags, p_copy_links);
}

Error DirAccessJAndroid::rename(String p_from, String p_to) {
	if (_rename) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		String from_path = get_absolute_path(p_from);
		String to_path = get_absolute_path(p_to);
		if (from_path.is_empty() || to_path.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		if (from_path.begins_with("content://") != to_path.begins_with("content://")) {
			return ERR_UNAVAILABLE;
		}
		jstring j_from = string_to_jstring_utf16(from_path, env);
		jstring j_to = string_to_jstring_utf16(to_path, env);

		bool result = env->CallBooleanMethod(dir_access_handler, _rename, get_access_type(), j_from, j_to);
		env->DeleteLocalRef(j_from);
		env->DeleteLocalRef(j_to);
		if (result) {
			return OK;
		} else {
			return FAILED;
		}
	} else {
		return ERR_UNCONFIGURED;
	}
}

Error DirAccessJAndroid::remove(String p_name) {
	if (_remove) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, ERR_UNCONFIGURED);

		String path = get_absolute_path(p_name);
		if (path.is_empty()) {
			return ERR_INVALID_PARAMETER;
		}
		jstring j_name = string_to_jstring_utf16(path, env);
		bool result = env->CallBooleanMethod(dir_access_handler, _remove, get_access_type(), j_name);
		env->DeleteLocalRef(j_name);
		if (result) {
			return OK;
		} else {
			return FAILED;
		}
	} else {
		return ERR_UNCONFIGURED;
	}
}

String DirAccessJAndroid::read_link(String p_file) {
	const String path = get_absolute_path(p_file);
	if (path.is_empty() || path.begins_with("content://")) {
		return String(); // Document providers do not expose symbolic links.
	}
	return p_file;
}

bool DirAccessJAndroid::is_case_sensitive(const String &p_path) const {
	const String path = get_absolute_path(p_path);
	if (path.is_empty() || path.begins_with("content://")) {
		// SAF lookup matches provider display names exactly; no filesystem
		// case-folding behavior can be inferred from a content URI.
		return true;
	}
	return DirAccessUnix::is_case_sensitive(path);
}

bool DirAccessJAndroid::is_equivalent(const String &p_path_a, const String &p_path_b) const {
	const String path_a = get_absolute_path(p_path_a);
	const String path_b = get_absolute_path(p_path_b);
	if (path_a.is_empty() || path_b.is_empty()) {
		return false;
	}
	if (path_a.begins_with("content://") || path_b.begins_with("content://")) {
		return path_a == path_b;
	}
	return DirAccessUnix::is_equivalent(p_path_a, p_path_b);
}

String DirAccessJAndroid::get_filesystem_type() const {
	if (current_dir.begins_with("content://")) {
		return String(); // SAF does not expose the backing filesystem type.
	}
	return DirAccessUnix::get_filesystem_type();
}

uint64_t DirAccessJAndroid::get_space_left() {
	if (current_dir.begins_with("content://")) {
		return 0; // The app's external-storage capacity does not describe this provider.
	}
	if (_get_space_left) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);
		return env->CallLongMethod(dir_access_handler, _get_space_left, get_access_type());
	} else {
		return 0;
	}
}

void DirAccessJAndroid::setup(jobject p_dir_access_handler) {
	JNIEnv *env = get_jni_env();
	dir_access_handler = env->NewGlobalRef(p_dir_access_handler);

	jclass c = env->GetObjectClass(dir_access_handler);
	cls = (jclass)env->NewGlobalRef(c);

	_dir_open = env->GetMethodID(cls, "dirOpen", "(ILjava/lang/String;)I");
	_dir_next = env->GetMethodID(cls, "dirNext", "(I)Ljava/lang/String;");
	_dir_close = env->GetMethodID(cls, "dirClose", "(I)V");
	_dir_is_dir = env->GetMethodID(cls, "dirIsDir", "(I)Z");
	_dir_exists = env->GetMethodID(cls, "dirExists", "(ILjava/lang/String;)Z");
	_file_exists = env->GetMethodID(cls, "fileExists", "(ILjava/lang/String;)Z");
	_is_readable = env->GetMethodID(cls, "isReadable", "(ILjava/lang/String;)Z");
	_is_writable = env->GetMethodID(cls, "isWritable", "(ILjava/lang/String;)Z");
	_get_drive_count = env->GetMethodID(cls, "getDriveCount", "(I)I");
	_get_drive = env->GetMethodID(cls, "getDrive", "(II)Ljava/lang/String;");
	_make_dir = env->GetMethodID(cls, "makeDir", "(ILjava/lang/String;)Z");
	_make_dir_recursive = env->GetMethodID(cls, "makeDirRecursive", "(ILjava/lang/String;)Z");
	_normalize_path = env->GetMethodID(cls, "normalizePath", "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
	_get_space_left = env->GetMethodID(cls, "getSpaceLeft", "(I)J");
	_rename = env->GetMethodID(cls, "rename", "(ILjava/lang/String;Ljava/lang/String;)Z");
	_remove = env->GetMethodID(cls, "remove", "(ILjava/lang/String;)Z");
	_current_is_hidden = env->GetMethodID(cls, "isCurrentHidden", "(I)Z");
}

void DirAccessJAndroid::terminate() {
	JNIEnv *env = get_jni_env();
	ERR_FAIL_NULL(env);

	env->DeleteGlobalRef(cls);
	env->DeleteGlobalRef(dir_access_handler);
}

DirAccessJAndroid::DirAccessJAndroid() {
}

DirAccessJAndroid::~DirAccessJAndroid() {
	list_dir_end();
}

int DirAccessJAndroid::dir_open(String p_path) {
	if (_dir_open) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL_V(env, 0);

		String path = get_absolute_path(p_path);
		if (path.is_empty()) {
			return 0;
		}
		jstring js = string_to_jstring_utf16(path, env);
		int dirId = env->CallIntMethod(dir_access_handler, _dir_open, get_access_type(), js);
		env->DeleteLocalRef(js);
		return dirId;
	} else {
		return 0;
	}
}

void DirAccessJAndroid::dir_close(int p_id) {
	if (_dir_close) {
		JNIEnv *env = get_jni_env();
		ERR_FAIL_NULL(env);
		env->CallVoidMethod(dir_access_handler, _dir_close, p_id);
	}
}
