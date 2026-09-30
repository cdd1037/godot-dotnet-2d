/**************************************************************************/
/*  test_error_string_lifetime.cpp                                                    */
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

TEST_FORCE_LINK(test_error_string_lifetime)

#include "core/error/error_macros.h"
#include "core/string/ustring.h"

namespace TestErrorStringLifetime {

struct Capture {
	String expected;
	String received;
	int calls = 0;

	static void on_error(void *p_user, const char *, const char *, int, const char *p_error, const char *, bool, ErrorHandlerType) {
		Capture *capture = static_cast<Capture *>(p_user);
		// Allocate buffers of the same size before copying the callback argument.
		// The reporting function must still own its UTF-8 data at this point.
		Vector<CharString> pressure;
		const int byte_length = capture->expected.utf8().length();
		for (int i = 0; i < 16; i++) {
			pressure.push_back(String("x").repeat(byte_length).utf8());
		}
		capture->received = String::utf8(p_error);
		capture->calls++;
	}
};

TEST_CASE("[ErrorHandler][SixthFixBatch] ASAP error retains UTF8 through callbacks") {
	Capture capture;
	capture.expected = String::utf8("Lifetime regression: \xce\xbb / \xf0\x9f\x99\x82 / ") + String("payload").repeat(16);
	ErrorHandlerList handler;
	handler.errfunc = Capture::on_error;
	handler.userdata = &capture;
	add_error_handler(&handler);
	_err_print_error_asap(capture.expected, ERR_HANDLER_WARNING);
	remove_error_handler(&handler);
	CHECK(capture.calls == 1);
	CHECK(capture.received == capture.expected);
}

} // namespace TestErrorStringLifetime
