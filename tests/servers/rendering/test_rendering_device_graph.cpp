/**************************************************************************/
/*  test_rendering_device_graph.cpp                                              */
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

TEST_FORCE_LINK(test_rendering_device_graph)

#include "servers/rendering/rendering_device_graph.h"

// Recording-only fixture: deliberately has no GPU driver, never executes commands.
class TestRenderingDeviceGraphAccessor {
public:
	static void prepare(RenderingDeviceGraph &p_graph, bool p_workaround = false) {
		p_graph.frames.resize(1);
		p_graph.driver_honors_barriers = true;
		p_graph.driver_clears_with_copy_engine = true;
		p_graph.driver_buffers_require_transitions = false;
		p_graph.driver_workarounds.avoid_store_op_dont_care_in_draw_list_with_no_bound_pipeline = p_workaround;
		p_graph.begin();
	}
	static bool has_edge(const RenderingDeviceGraph &p_graph, int p_from, int p_to) {
		const auto *command = reinterpret_cast<const RenderingDeviceGraph::RecordedCommand *>(p_graph.command_data.ptr() + p_graph.command_data_offsets[p_from]);
		for (int i = command->adjacent_command_list_index; i >= 0; i = p_graph.command_list_nodes[i].next_list_index) {
			if (p_graph.command_list_nodes[i].command_index == p_to) {
				return true;
			}
		}
		return false;
	}
	static RDD::AttachmentStoreOp last_store_op(RenderingDeviceGraph &p_graph) {
		auto *command = reinterpret_cast<RenderingDeviceGraph::RecordedDrawListCommand *>(p_graph.command_data.ptr() + p_graph.command_data_offsets[p_graph.command_count - 1]);
		return command->store_ops()[0];
	}
};

namespace TestRenderingDeviceGraph {
using Graph = RenderingDeviceGraph;

TEST_CASE("[RenderingDeviceGraph][SeventhFixBatch] Full texture and slice do not create self edges") {
	Graph graph;
	TestRenderingDeviceGraphAccessor::prepare(graph);
	Graph::ResourceTracker full;
	full.texture_subresources.mipmap_count = 4;
	full.texture_subresources.layer_count = 2;
	full.texture_driver_id = RDD::TextureID(1);
	Graph::ResourceTracker slice;
	slice.parent = &full;
	slice.texture_subresources.mipmap_count = 1;
	slice.texture_subresources.layer_count = 1;
	slice.texture_driver_id = RDD::TextureID(1);
	for (bool slice_first : { false, true }) {
		graph.begin();
		Graph::ResourceTracker *trackers[] = { slice_first ? &slice : &full, slice_first ? &full : &slice };
		Graph::ResourceUsage usages[] = { Graph::RESOURCE_USAGE_TEXTURE_SAMPLE, Graph::RESOURCE_USAGE_TEXTURE_SAMPLE };
		graph.add_driver_callback(nullptr, nullptr, VectorView<Graph::ResourceTracker *>(trackers, 2), VectorView<Graph::ResourceUsage>(usages, 2));
		CHECK_FALSE(TestRenderingDeviceGraphAccessor::has_edge(graph, 0, 0));
		CHECK(full.command_index == 0);
		CHECK(slice.command_index == 0);
		CHECK(full.usage_index == (slice_first ? 1 : 0));
		CHECK(slice.usage_index == (slice_first ? 0 : 1));
		Graph::ResourceTracker *write_trackers[] = { &full };
		Graph::ResourceUsage write_usages[] = { Graph::RESOURCE_USAGE_STORAGE_IMAGE_READ_WRITE };
		graph.add_driver_callback(nullptr, nullptr, VectorView<Graph::ResourceTracker *>(write_trackers, 1), VectorView<Graph::ResourceUsage>(write_usages, 1));
		CHECK(TestRenderingDeviceGraphAccessor::has_edge(graph, 0, 1));
		CHECK_FALSE(TestRenderingDeviceGraphAccessor::has_edge(graph, 1, 1));
	}
	full.reset_if_outdated(full.command_frame + 1);
	CHECK(full.command_index == -1);
	CHECK(full.usage_index == UINT32_MAX);
	graph.finalize();
}

TEST_CASE("[RenderingDeviceGraph][SeventhFixBatch] Empty pipeline workaround changes only required stores") {
	for (bool workaround : { false, true }) {
		Graph graph;
		TestRenderingDeviceGraphAccessor::prepare(graph, workaround);
		Graph::ResourceTracker attachment;
		attachment.is_discardable = true;
		attachment.texture_size = Size2i(16, 16);
		Graph::FramebufferCache framebuffer;
		framebuffer.trackers.push_back(&attachment);
		// Bound -> unbound -> bound also checks reset at every draw-list begin.
		for (bool bound : { true, false, true }) {
			graph.add_draw_list_begin(&framebuffer, Rect2i(0, 0, 16, 16), {}, {}, RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
			if (bound) {
				graph.add_draw_list_bind_pipeline(RDD::PipelineID(1), RDD::PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
			}
			graph.add_draw_list_end();
			const auto expected = workaround && !bound ? RDD::ATTACHMENT_STORE_OP_STORE : RDD::ATTACHMENT_STORE_OP_DONT_CARE;
			CHECK(TestRenderingDeviceGraphAccessor::last_store_op(graph) == expected);
		}
		attachment.is_discardable = false;
		graph.add_draw_list_begin(&framebuffer, Rect2i(0, 0, 16, 16), {}, {}, RDD::PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
		graph.add_draw_list_end();
		CHECK(TestRenderingDeviceGraphAccessor::last_store_op(graph) == RDD::ATTACHMENT_STORE_OP_STORE);
		graph.finalize();
	}
}

} // namespace TestRenderingDeviceGraph
