/**************************************************************************/
/*  preprocessor_runtime.cpp                                             */
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
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

static std::string compact(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); }), s.end());
    return s;
}
static void check(const char *label, const std::string& source, const std::string& expected) {
    glslang::TShader shader(EShLangVertex);
    const char *input = source.c_str();
    shader.setStrings(&input, 1);
    glslang::TShader::ForbidIncluder includer;
    std::string output;
    if (!shader.preprocess(GetDefaultResources(), 450, ECoreProfile, false, false, EShMsgDefault, &output, includer)) {
        std::cerr << label << " preprocess failed:\n" << shader.getInfoLog(); std::abort();
    }
    if (compact(output).find(compact(expected)) == std::string::npos) {
        std::cerr << label << " missing expected expansion: " << expected << "\nOutput:\n" << output; std::abort();
    }
    for (const auto* text : {&source, static_cast<const std::string*>(&output)}) {
        glslang::TShader parsed(EShLangVertex);
        const char *p = text->c_str();
        parsed.setStrings(&p, 1);
        if (!parsed.parse(GetDefaultResources(), 450, false, EShMsgDefault)) {
            std::cerr << label << " parse failed:\n" << parsed.getInfoLog() << "\n" << *text; std::abort();
        }
        glslang::TProgram program;
        program.addShader(&parsed);
        if (!program.link(EShMsgDefault)) {
            std::cerr << label << " link failed:\n" << program.getInfoLog(); std::abort();
        }
    }
    std::cout << "PASS " << label << ": preprocess, expansion, original parse/link, expanded parse/link\n";
}
int main() {
    assert(glslang::InitializeProcess());
    check("punctuation-and-macro-replay", R"(#version 450
#define TWICE(x) ((x) + (x))
#define VALUE 2.0
void main() { float a = TWICE(VALUE); if (a >= 1.0 && a != 0.0) a *= 2.0; gl_Position = vec4(a); }
)", "float a = ((2.0) + (2.0));");
    check("token-paste-identifiers-and-operators", R"(#version 450
#define CAT(a,b) a ## b
void main() { int CAT(my,Var) = 12; bool b = 1 CAT(<,=) 2; bool c = true CAT(&,&) false; gl_Position = vec4(myVar + int(b) + int(c)); }
)", "int myVar = 12; bool b = 1 <= 2; bool c = true && false;");
    check("stringification", R"(#version 450
#extension GL_EXT_debug_printf : enable
#define STRINGIFY(x) #x
void main() { debugPrintfEXT(STRINGIFY(hello + world)); gl_Position = vec4(0.0); }
)", "debugPrintfEXT(\"hello + world\");");
    const std::string longName(1024, 'a');
    check("max-length-token-replay", "#version 450\n#define IDENT " + longName + "\nvoid main() { float IDENT = 1.0; gl_Position = vec4(IDENT); }\n", "float " + longName + " = 1.0;");
    glslang::FinalizeProcess();
}
