/**************************************************************************/
/*  overflow_ubsan.cpp                                                     */
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

#include "core/math/math_funcs.h"
#include "core/math/vector2i.h"
#include "core/math/vector3i.h"
#include "core/math/vector4i.h"
#include <cassert>
#include <cstdint>

int main() {
	volatile int32_t source32 = INT32_MIN;
	volatile int64_t source64 = INT64_MIN;
	volatile int32_t divisor = -1;
	assert(Math::division_no_overflow(source32, divisor) == INT32_MIN);
	assert(Math::division_no_overflow(source64, int64_t(divisor)) == INT64_MIN);
	assert(Math::modulo_no_overflow(source32, divisor) == 0);
	assert(Math::modulo_no_overflow(source64, int64_t(divisor)) == 0);
	const Vector2i two(source32, source32);
	const Vector3i three(source32, source32, source32);
	const Vector4i four(source32, source32, source32, source32);
	assert(two / divisor == two && two % divisor == Vector2i());
	assert(three / divisor == three && three % divisor == Vector3i());
	assert(four / divisor == four && four % divisor == Vector4i());
}
