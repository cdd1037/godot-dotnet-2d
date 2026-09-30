/**************************************************************************/
/*  test_shader_language.cpp                                              */
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

TEST_FORCE_LINK(test_shader_language)

#include "servers/rendering/shader_language.h"

namespace TestShaderLanguage {

TEST_CASE("[ShaderLanguage][FirstFixBatch] Constant mat4 vector multiplication") {
	ShaderLanguage language;
	ShaderLanguage::ShaderCompileInfo info;
	info.shader_types.insert("canvas_item");
	const String code = R"(
shader_type canvas_item;
const mat4 matrix = mat4(vec4(1.0, 2.0, 3.0, 4.0), vec4(5.0, 6.0, 7.0, 8.0),
                        vec4(9.0, 10.0, 11.0, 12.0), vec4(13.0, 14.0, 15.0, 16.0));
const vec4 vector = vec4(2.0, 3.0, 5.0, 7.0);
const vec4 matrix_vector = matrix * vector;
const vec4 vector_matrix = vector * matrix;
)";
	REQUIRE_MESSAGE(language.compile(code, info) == OK, language.get_error_text());
	const char *names[] = { "matrix_vector", "vector_matrix" };
	const double expected[2][4] = { { 153.0, 170.0, 187.0, 204.0 }, { 51.0, 119.0, 187.0, 255.0 } };
	for (int product = 0; product < 2; product++) {
		const ShaderLanguage::ShaderNode::Constant *constant = language.get_shader()->constants.getptr(names[product]);
		REQUIRE(constant != nullptr);
		REQUIRE(constant->initializer != nullptr);
		// Folded operator nodes retain their syntax kind and expose cached values.
		const Vector<ShaderLanguage::Scalar> values = constant->initializer->get_values();
		REQUIRE_EQ(values.size(), 4);
		for (int component = 0; component < 4; component++) {
			CHECK(values[component].real == doctest::Approx(expected[product][component]));
		}
	}
}

#ifdef DEBUG_ENABLED
// Release builds return zero for Memory::get_mem_usage(), so they cannot prove reclamation.
TEST_CASE("[ShaderLanguage][FirstFixBatch] Constant array nodes are reclaimed") {
	ShaderLanguage::ShaderCompileInfo info;
	info.shader_types.insert("canvas_item");
	const String code = "shader_type canvas_item; const float values[3] = float[3](1.0, 2.0, 3.0);";
	// Warm up parser-wide caches before measuring retained engine allocations.
	{
		ShaderLanguage language;
		REQUIRE_MESSAGE(language.compile(code, info) == OK, language.get_error_text());
	}
	const uint64_t memory_before = Memory::get_mem_usage();
	for (int iteration = 0; iteration < 8; iteration++) {
		ShaderLanguage language;
		REQUIRE_MESSAGE(language.compile(code, info) == OK, language.get_error_text());
		const ShaderLanguage::ShaderNode::Constant *constant = language.get_shader()->constants.getptr("values");
		REQUIRE(constant != nullptr);
		CHECK_EQ(constant->array_size, 3);
	}
	CHECK_EQ(Memory::get_mem_usage(), memory_before);
}

#endif // DEBUG_ENABLED

} // namespace TestShaderLanguage
