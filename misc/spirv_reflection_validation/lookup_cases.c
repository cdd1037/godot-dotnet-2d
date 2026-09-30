/**************************************************************************/
/*  lookup_cases.c                                                    */
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

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static bool fail_lookup_allocation;
static void *controlled_calloc(size_t count, size_t size) {
  if (fail_lookup_allocation && count == 4 && size == sizeof(uint32_t)) return NULL;
  return calloc(count, size);
}
#define calloc controlled_calloc
#include "thirdparty/spirv-reflect/spirv_reflect.c"
#undef calloc

static void check(uint32_t bound, bool fail_optional) {
  uint32_t words[] = {
    SpvMagicNumber, 0x00010300, 0, bound, 0,
    (2u << 16) | SpvOpTypeVoid, 1,
    (3u << 16) | SpvOpTypeFloat, 2, 32,
    (4u << 16) | SpvOpTypeInt, 2, 32, 1,
    (3u << 16) | SpvOpTypeForwardPointer, 3, SpvStorageClassPrivate,
    (4u << 16) | SpvOpTypePointer, 3, SpvStorageClassPrivate, 2
  };
  SpvReflectPrvParser parser = {0};
  fail_lookup_allocation = fail_optional;
  assert(CreateParser(sizeof(words), words, &parser) == SPV_REFLECT_RESULT_SUCCESS);
  SpvReflectResult result = ParseNodes(&parser);
  if (bound == 0) {
    assert(result == SPV_REFLECT_RESULT_ERROR_SPIRV_INVALID_ID_REFERENCE);
  } else {
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    assert((parser.node_index_by_id == NULL) == (fail_optional || bound == UINT32_MAX));
    assert(FindNode(&parser, 0) == NULL);
    assert(FindNode(&parser, bound) == NULL);
    assert(FindNode(&parser, 1)->op == SpvOpTypeVoid);
    assert(FindNode(&parser, 2)->op == SpvOpTypeFloat); // First malformed duplicate wins, as before.
    assert(FindNode(&parser, 3)->op == SpvOpTypePointer); // Forward pointer is replaced.
    assert(FindNode(&parser, bound - 1) != NULL || bound == UINT32_MAX);
  }
  DestroyParser(&parser);
}
int main(void) {
  check(4, false);
  check(4, true);
  check(UINT32_MAX, false);
  check(0, false);
  puts("PASS: dense/sparse IDs, optional allocation failure, duplicate IDs, forward pointers, invalid zero/out-of-range IDs");
}
