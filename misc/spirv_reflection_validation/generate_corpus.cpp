/**************************************************************************/
/*  generate_corpus.cpp                                                    */
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

#include "glslang/Public/ShaderLang.h"
#include "glslang/Public/ResourceLimits.h"
#include "SPIRV/GlslangToSpv.h"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

static void compile(const std::string &directory, const char *name, EShLanguage stage, const std::string &code) {
	glslang::TShader shader(stage);
	const char *source = code.c_str();
	shader.setStrings(&source, 1);
	shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
	const EShMessages messages = EShMessages(EShMsgSpvRules | EShMsgVulkanRules);
	if (!shader.parse(GetDefaultResources(), 450, false, messages)) {
		std::cerr << shader.getInfoLog() << '\n';
		std::abort();
	}
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		std::cerr << program.getInfoLog() << '\n';
		std::abort();
	}
	std::vector<unsigned int> words;
	glslang::SpvOptions options;
	options.disableOptimizer = true;
	glslang::GlslangToSpv(*program.getIntermediate(stage), words, &options);
	std::ofstream binary(directory + "/" + name + ".spv", std::ios::binary);
	binary.write(reinterpret_cast<const char *>(words.data()), words.size() * sizeof(uint32_t));
	std::ofstream text(directory + "/" + name + ".glsl");
	text << code;
	std::cout << name << " words=" << words.size() << '\n';
}
int main(int argc, char **argv) {
	assert(argc == 2);
	assert(glslang::InitializeProcess());
	compile(argv[1], "vertex", EShLangVertex, R"(#version 450
layout(location=0) in vec2 position;
layout(location=1) in vec2 texcoord;
layout(location=0) out vec2 uv;
layout(set=0,binding=0,std140) uniform Camera { mat4 projection; vec4 color; } camera;
void main() { gl_Position=camera.projection*vec4(position,0,1); uv=texcoord; }
)");
	compile(argv[1], "fragment", EShLangFragment, R"(#version 450
layout(location=0) in vec2 uv;
layout(location=0) out vec4 color;
layout(set=1,binding=2) uniform sampler2D textures[3];
layout(push_constant) uniform Params { vec4 tint; float weight; } params;
void main() { color=texture(textures[1],uv)*params.tint*params.weight; }
)");
	compile(argv[1], "compute", EShLangCompute, R"(#version 450
layout(local_size_x=4,local_size_y=2,local_size_z=1) in;
layout(constant_id=7) const int SHIFT=1;
layout(set=0,binding=0,std430) buffer Data { vec4 values[]; } data;
layout(set=0,binding=1) uniform sampler2D image_in;
layout(push_constant) uniform Params { mat4 transform; int index; } params;
void main() { data.values[gl_GlobalInvocationID.x]=params.transform*textureLod(image_in,vec2(0.5),0)+vec4(params.index+SHIFT); }
)");
	std::string large = "#version 450\nlayout(local_size_x=1) in;\nlayout(set=0,binding=0,std140) uniform Large {\n";
	for (int i = 0; i < 256; i++) large += "vec4 field" + std::to_string(i) + ";\n";
	large += "} data;\nlayout(set=0,binding=1,std430) buffer Out { vec4 value; } output_data;\nvoid main() { output_data.value=vec4(0);\n";
	for (int i = 0; i < 256; i++) large += "output_data.value += data.field" + std::to_string(i) + ";\n";
	large += "}\n";
	compile(argv[1], "large_compute", EShLangCompute, large);
	glslang::FinalizeProcess();
}
