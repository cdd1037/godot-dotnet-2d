/**************************************************************************/
/*  test_editor_export_platform.cpp                                       */
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

TEST_FORCE_LINK(test_editor_export_platform)

#ifdef TOOLS_ENABLED

#include "editor/export/editor_export_platform.h"

namespace TestEditorExportPlatform {

class PaddingAccess : public EditorExportPlatform {
public:
	using EditorExportPlatform::_get_pad;
};

TEST_CASE("[EditorExport][FirstFixBatch] PCK padding preserves 64-bit positions") {
	// Exercise the real export helper, without allocating multi-gigabyte files.
	// Include positions immediately before/after signed and unsigned 32-bit wrap.
	const uint64_t bases[] = { 0, (uint64_t(1) << 31) - 16, uint64_t(1) << 31,
		(uint64_t(1) << 32) - 16, uint64_t(1) << 32, uint64_t(1) << 40,
		(uint64_t(1) << 63) - 16 };
	for (const int alignment : { 4, 8, 16 }) {
		for (const uint64_t base : bases) {
			for (int residue = 0; residue < alignment; residue++) {
				const uint64_t position = base + residue;
				const int padding = PaddingAccess::_get_pad(alignment, position);
				CHECK_EQ(padding, residue == 0 ? 0 : alignment - residue);
				CHECK_EQ((position + padding) % alignment, 0);
			}
		}
	}
}

} // namespace TestEditorExportPlatform

#endif // TOOLS_ENABLED
