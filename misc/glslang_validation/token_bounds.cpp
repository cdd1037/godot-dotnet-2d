/**************************************************************************/
/*  token_bounds.cpp                                                     */
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

#include "glslang/MachineIndependent/preprocessor/PpContext.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>

int main() {
    struct Guarded {
        std::array<unsigned char, 32> before;
        glslang::TPpToken token;
        std::array<unsigned char, 32> after;
    } item;
    item.before.fill(0xAB);
    item.after.fill(0xCD);
    constexpr size_t capacity = sizeof(item.token.name);
    for (size_t length = 0; length <= capacity + 257; ++length) {
        std::string source(length, 'x');
        for (size_t i = 0; i < length; ++i) source[i] = '!' + i % 90;
        std::array<char, capacity> expected{};
        std::snprintf(expected.data(), expected.size(), "%s", source.c_str());
        std::memset(item.token.name, 0xEE, capacity);
        item.token.space = true;
        item.token.fullyExpanded = true;
        item.token.i64val = 0x1122334455667788LL;
        glslang::SetPpTokenName(item.token, source.c_str(), source.size());
        const size_t copied = std::min(length, capacity - 1);
        assert(std::memcmp(expected.data(), item.token.name, copied + 1) == 0);
        assert(item.token.name[copied] == '\0');
        for (size_t i = copied + 1; i < capacity; ++i)
            assert(static_cast<unsigned char>(item.token.name[i]) == 0xEE);
        assert(item.token.space && item.token.fullyExpanded);
        assert(item.token.i64val == 0x1122334455667788LL);
        for (auto b : item.before) assert(b == 0xAB);
        for (auto b : item.after) assert(b == 0xCD);
    }
    // Sources need not be NUL terminated: ASan catches reads past these allocations.
    for (size_t length : {size_t(1), capacity - 1, capacity, capacity + 100}) {
        std::unique_ptr<char[]> source(new char[length]);
        std::memset(source.get(), 'Z', length);
        glslang::SetPpTokenName(item.token, source.get(), length);
        assert(std::strlen(item.token.name) == std::min(length, capacity - 1));
    }
    // Explicit lengths copy only the prefix, and extreme lengths are safely capped.
    glslang::SetPpTokenName(item.token, "abcdef", 3);
    assert(std::strcmp(item.token.name, "abc") == 0);
    std::string large(capacity, 'q');
    glslang::SetPpTokenName(item.token, large.data(), std::numeric_limits<size_t>::max());
    assert(std::strlen(item.token.name) == capacity - 1);
    glslang::SetPpTokenName(item.token, "", 0);
    assert(item.token.name[0] == '\0');
    std::cout << "PASS: " << capacity + 258 << " differential lengths; exact allocations, prefix, SIZE_MAX, guards, metadata and termination\n";
}
