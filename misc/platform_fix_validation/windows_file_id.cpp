// Portable control-flow test of the verbatim Windows implementation with mocked Win32 calls.
// This is not a Windows filesystem or network-drive integration test.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <functional>

using ULONGLONG = uint64_t;
using HANDLE = int;
using LPCWSTR = const wchar_t *;
using FILE_INFO_BY_HANDLE_CLASS = int;
constexpr int INVALID_HANDLE_VALUE = -1;
constexpr int FILE_READ_ATTRIBUTES = 1, FILE_SHARE_READ = 2, FILE_SHARE_WRITE = 4;
constexpr int FILE_SHARE_DELETE = 8, OPEN_EXISTING = 1, FILE_FLAG_BACKUP_SEMANTICS = 2;
struct BY_HANDLE_FILE_INFORMATION {
	uint32_t dwVolumeSerialNumber = 1, nFileIndexLow = 2, nFileIndexHigh = 3;
};
struct Record {
	bool open_success = true, ex_success = true, legacy_success = true, open = false;
	uint64_t volume = 1, low = 2, high = 3;
	BY_HANDLE_FILE_INFORMATION legacy;
};
static Record records[2];
static int fallback_calls = 0;
static bool fallback_result = false;
struct String {
	std::wstring value;
	String(const wchar_t *p_value) : value(p_value) {}
	const String &utf16() const { return *this; }
	const wchar_t *get_data() const { return value.c_str(); }
};
struct DirAccess {
	bool is_equivalent(const String &, const String &) const {
		fallback_calls++;
		return fallback_result;
	}
};
struct DirAccessWindows : DirAccess {
	String fix_path(const String &p_path) const { return p_path; }
	bool is_equivalent(const String &, const String &) const;
};
HANDLE CreateFileW(LPCWSTR path, int, int, void *, int, int, void *) {
	const int index = path[0] == L'a' ? 0 : 1;
	if (!records[index].open_success) {
		return INVALID_HANDLE_VALUE;
	}
	records[index].open = true;
	return index + 1;
}
bool CloseHandle(HANDLE handle) {
	assert(records[handle - 1].open);
	records[handle - 1].open = false;
	return true;
}
bool GetFileInformationByHandleEx(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS, void *out, size_t size) {
	const Record &record = records[handle - 1];
	assert(record.open);
	if (handle == 2) {
		assert(records[0].open); // Both handles stay open while IDs are queried.
	}
	if (!record.ex_success) {
		return false;
	}
	const uint64_t data[] = { record.volume, record.low, record.high };
	assert(size == sizeof(data));
	memcpy(out, data, size);
	return true;
}
bool GetFileInformationByHandle(HANDLE handle, BY_HANDLE_FILE_INFORMATION *out) {
	const Record &record = records[handle - 1];
	assert(record.open);
	if (!record.legacy_success) {
		return false;
	}
	*out = record.legacy;
	return true;
}

#include "windows_file_id_impl.inc"

static void run(const std::function<void()> &configure, bool expected, int expected_fallbacks = 0) {
	records[0] = Record();
	records[1] = Record();
	fallback_calls = 0;
	fallback_result = false;
	configure();
	assert(DirAccessWindows().is_equivalent(String(L"a"), String(L"b")) == expected);
	assert(fallback_calls == expected_fallbacks);
	assert(!records[0].open && !records[1].open);
}
int main() {
	run([] {}, true);
	run([] { records[1].volume++; }, false);
	run([] { records[1].low++; }, false);
	run([] { records[1].high++; }, false);
	run([] { records[0].ex_success = records[1].ex_success = false; }, true);
	run([] { records[0].ex_success = records[1].ex_success = false; records[1].legacy.dwVolumeSerialNumber++; }, false);
	run([] { records[0].ex_success = records[1].ex_success = false; records[1].legacy.nFileIndexLow++; }, false);
	run([] { records[0].ex_success = records[1].ex_success = false; records[1].legacy.nFileIndexHigh++; }, false);
	run([] { records[0].ex_success = false; }, false);
	run([] { records[0].open_success = false; fallback_result = true; }, true, 1);
	run([] { records[1].open_success = false; fallback_result = true; }, true, 1);
	run([] { records[0].ex_success = records[0].legacy_success = false; }, false, 1);
	run([] { records[1].ex_success = records[1].legacy_success = false; fallback_result = true; }, true, 1);
}
