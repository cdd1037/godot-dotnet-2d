/**************************************************************************/
/*  test_dummy_storage.cpp                                                */
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

TEST_FORCE_LINK(test_dummy_storage)

#ifdef THREADS_ENABLED

#include "core/os/semaphore.h"
#include "core/os/thread.h"
#include "servers/rendering/dummy/storage/material_storage.h"
#include "servers/rendering/dummy/storage/mesh_storage.h"
#include "servers/rendering/dummy/storage/texture_storage.h"

namespace TestDummyStorage {

TEST_CASE("[Rendering][FirstFixBatch] Dummy storage concurrent RID allocation and release") {
	// test_setup installs the dummy renderer. Exercise its real owners without
	// serializing operations through the RenderingServer command queue.
	struct Context {
		RendererDummy::MaterialStorage *materials = RendererDummy::MaterialStorage::get_singleton();
		RendererDummy::MeshStorage *meshes = RendererDummy::MeshStorage::get_singleton();
		RendererDummy::TextureStorage *textures = RendererDummy::TextureStorage::get_singleton();
		Semaphore ready;
		Semaphore start;
		SafeNumeric<uint32_t> failures{ 0 };
	} context;
	REQUIRE(context.materials != nullptr);
	REQUIRE(context.meshes != nullptr);
	REQUIRE(context.textures != nullptr);
	Thread threads[4];
	for (Thread &thread : threads) {
		thread.start([](void *p_data) {
			Context *ctx = static_cast<Context *>(p_data);
			ctx->ready.post();
			ctx->start.wait();
			for (int round = 0; round < 8; round++) {
				RID shaders[256];
				RID materials[256];
				RID meshes[256];
				RID multimeshes[256];
				RID textures[256];
				for (int i = 0; i < 256; i++) {
					shaders[i] = ctx->materials->shader_allocate();
					ctx->materials->shader_initialize(shaders[i], false);
					materials[i] = ctx->materials->material_allocate();
					ctx->materials->material_initialize(materials[i]);
					meshes[i] = ctx->meshes->mesh_allocate();
					ctx->meshes->mesh_initialize(meshes[i]);
					multimeshes[i] = ctx->meshes->multimesh_allocate();
					ctx->meshes->multimesh_initialize(multimeshes[i]);
					textures[i] = ctx->textures->texture_allocate();
				}
				for (int i = 255; i >= 0; i--) {
					if (!ctx->materials->owns_shader(shaders[i]) || !ctx->materials->owns_material(materials[i]) ||
							!ctx->meshes->owns_mesh(meshes[i]) || !ctx->meshes->owns_multimesh(multimeshes[i]) ||
							!ctx->textures->owns_texture(textures[i])) {
						ctx->failures.increment();
					}
					ctx->materials->shader_free(shaders[i]);
					ctx->materials->material_free(materials[i]);
					ctx->meshes->mesh_free(meshes[i]);
					ctx->meshes->multimesh_free(multimeshes[i]);
					ctx->textures->texture_free(textures[i]);
					if (ctx->materials->owns_shader(shaders[i]) || ctx->materials->owns_material(materials[i]) ||
							ctx->meshes->owns_mesh(meshes[i]) || ctx->meshes->owns_multimesh(multimeshes[i]) ||
							ctx->textures->owns_texture(textures[i])) {
						ctx->failures.increment();
					}
				}
			}
		}, &context);
	}
	for (int i = 0; i < 4; i++) {
		context.ready.wait();
	}
	context.start.post(4);
	for (Thread &thread : threads) {
		thread.wait_to_finish();
	}
	CHECK_EQ(context.failures.get(), 0);
}

} // namespace TestDummyStorage

#endif // THREADS_ENABLED
