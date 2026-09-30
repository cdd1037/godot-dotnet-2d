/**************************************************************************/
/*  reflect_corpus.cpp                                                    */
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

#include "thirdparty/spirv-reflect/spirv_reflect.h"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

static const char *name(const char *p_name) { return p_name ? p_name : ""; }
static void block(const SpvReflectBlockVariable &value, int depth = 0) {
	std::cout << "block " << depth << ' ' << name(value.name) << ' ' << value.spirv_id << ' ' << value.offset << ' ' << value.absolute_offset << ' ' << value.size << ' ' << value.padded_size << ' ' << value.decoration_flags << ' ' << value.member_count << ' ' << value.array.dims_count;
	for (uint32_t i = 0; i < value.array.dims_count; i++) std::cout << ' ' << value.array.dims[i];
	std::cout << '\n';
	for (uint32_t i = 0; i < value.member_count; i++) block(value.members[i], depth + 1);
}
static void dump(const SpvReflectShaderModule &m) {
	std::cout << "module " << m.shader_stage << ' ' << name(m.entry_point_name) << ' ' << m.entry_point_count << ' ' << m.descriptor_binding_count << ' ' << m.push_constant_block_count << ' ' << m.spec_constant_count << '\n';
	for (uint32_t i = 0; i < m.entry_point_count; i++) {
		const auto &e = m.entry_points[i];
		std::cout << "entry " << name(e.name) << ' ' << e.id << ' ' << e.local_size.x << ' ' << e.local_size.y << ' ' << e.local_size.z << '\n';
	}
	for (uint32_t i = 0; i < m.descriptor_binding_count; i++) {
		const auto &b = m.descriptor_bindings[i];
		std::cout << "binding " << name(b.name) << ' ' << b.set << ' ' << b.binding << ' ' << b.descriptor_type << ' ' << b.count << '\n';
		block(b.block);
	}
	for (uint32_t i = 0; i < m.push_constant_block_count; i++) block(m.push_constant_blocks[i]);
	for (uint32_t i = 0; i < m.input_variable_count; i++) std::cout << "input " << name(m.input_variables[i]->name) << ' ' << m.input_variables[i]->location << ' ' << m.input_variables[i]->format << '\n';
	for (uint32_t i = 0; i < m.output_variable_count; i++) std::cout << "output " << name(m.output_variables[i]->name) << ' ' << m.output_variables[i]->location << ' ' << m.output_variables[i]->format << '\n';
	for (uint32_t i = 0; i < m.spec_constant_count; i++) std::cout << "spec " << name(m.spec_constants[i].name) << ' ' << m.spec_constants[i].constant_id << '\n';
}
int main(int argc, char **argv) {
	assert(argc > 1);
	for (int i = 1; i < argc; i++) {
		std::ifstream input(argv[i], std::ios::binary | std::ios::ate);
		assert(input);
		const auto size = input.tellg();
		assert(size > 0 && size % 4 == 0);
		std::vector<uint32_t> words(size_t(size) / 4);
		input.seekg(0);
		input.read(reinterpret_cast<char *>(words.data()), size);
		SpvReflectShaderModule module;
		assert(spvReflectCreateShaderModule(size_t(size), words.data(), &module) == SPV_REFLECT_RESULT_SUCCESS);
		dump(module);
		spvReflectDestroyShaderModule(&module);
		const auto start = std::chrono::steady_clock::now();
		for (int repeat = 0; repeat < 100; repeat++) {
			assert(spvReflectCreateShaderModule(size_t(size), words.data(), &module) == SPV_REFLECT_RESULT_SUCCESS);
			spvReflectDestroyShaderModule(&module);
		}
		std::cerr << argv[i] << " 100_parses_us=" << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count() << '\n';
	}
}
