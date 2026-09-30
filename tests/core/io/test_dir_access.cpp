/**************************************************************************/
/*  test_dir_access.cpp                                                   */
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

TEST_FORCE_LINK(test_dir_access)

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace TestDirAccess {

class RecordingDirAccess : public DirAccess {
	bool in_subdirectory = false;
	int next_index = 0;

public:
	Error listing_error = OK;
	bool fail_subdirectory = false;
	int next_calls = 0;
	Vector<String> removed;

	Error list_dir_begin() override {
		next_index = 0;
		return fail_subdirectory && in_subdirectory ? ERR_CANT_OPEN : listing_error;
	}
	String get_next() override {
		next_calls++;
		if (in_subdirectory || next_index >= 2) {
			return String();
		}
		return next_index++ == 0 ? "folder%2Fother#name" : "file%2Fother#name";
	}
	bool current_is_dir() const override { return next_index == 1; }
	bool current_is_hidden() const override { return false; }
	void list_dir_end() override {}
	int get_drive_count() override { return 0; }
	String get_drive(int p_drive) override { return String(); }
	Error change_dir(String p_dir) override {
		if (p_dir == "folder%2Fother#name") {
			in_subdirectory = true;
			return OK;
		}
		if (p_dir == ".." && in_subdirectory) {
			in_subdirectory = false;
			return OK;
		}
		return ERR_INVALID_PARAMETER;
	}
	String get_current_dir(bool p_include_drive = true) const override {
		return in_subdirectory ? "content://provider/tree/root#folder%252Fother%23name" : "content://provider/tree/root#";
	}
	Error make_dir(String p_dir) override { return ERR_UNAVAILABLE; }
	bool file_exists(String p_file) override { return false; }
	bool dir_exists(String p_dir) override { return false; }
	uint64_t get_space_left() override { return 0; }
	Error rename(String p_from, String p_to) override { return ERR_UNAVAILABLE; }
	Error remove(String p_name) override {
		removed.push_back(p_name);
		return OK;
	}
	bool is_link(String p_file) override { return false; }
	String read_link(String p_file) override { return String(); }
	Error create_link(String p_source, String p_target) override { return ERR_UNAVAILABLE; }
	String get_filesystem_type() const override { return String(); }
};

TEST_CASE("[DirAccess] Recursive erase preserves relative display names") {
	Ref<RecordingDirAccess> da = memnew(RecordingDirAccess);
	CHECK(da->erase_contents_recursive() == OK);
	REQUIRE(da->removed.size() == 2);
	CHECK(da->removed[0] == "folder%2Fother#name");
	CHECK(da->removed[1] == "file%2Fother#name");
	CHECK(da->get_current_dir() == "content://provider/tree/root#");
}

TEST_CASE("[DirAccess] Recursive erase removes only ordinary directory contents") {
	Error create_error = FAILED;
	Ref<DirAccess> da = DirAccess::create_temp("test_dir_access_erase", false, &create_error);
	REQUIRE(create_error == OK);
	REQUIRE(da.is_valid());
	const String root = da->get_current_dir();
	REQUIRE(da->make_dir("nested%2Fdir") == OK);
	Ref<FileAccess> file = FileAccess::open(root.path_join("literal%2F#file.txt"), FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file.unref();
	file = FileAccess::open(root.path_join("nested%2Fdir/child.txt"), FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file.unref();

	Ref<DirAccess> sibling = DirAccess::create_temp("test_dir_access_keep", false, &create_error);
	REQUIRE(create_error == OK);
	REQUIRE(sibling.is_valid());
	const String sentinel = sibling->get_current_dir().path_join("keep.txt");
	file = FileAccess::open(sentinel, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file.unref();

	CHECK(da->erase_contents_recursive() == OK);
	CHECK(da->get_current_dir() == root);
	CHECK(da->get_files().is_empty());
	CHECK(da->get_directories().is_empty());
	CHECK(FileAccess::exists(sentinel));
}

TEST_CASE("[DirAccess] Recursive erase propagates enumeration failures") {
	Ref<RecordingDirAccess> da = memnew(RecordingDirAccess);
	SUBCASE("Initial listing fails") {
		da->listing_error = ERR_CANT_OPEN;
		CHECK(da->erase_contents_recursive() == ERR_CANT_OPEN);
		CHECK(da->next_calls == 0);
	}
	SUBCASE("Nested listing fails") {
		da->fail_subdirectory = true;
		CHECK(da->erase_contents_recursive() == ERR_CANT_OPEN);
		CHECK(da->get_current_dir() == "content://provider/tree/root#");
	}
	CHECK(da->removed.is_empty());
}

} // namespace TestDirAccess
