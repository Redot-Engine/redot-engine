/**************************************************************************/
/*  flow_context_rd.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
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

#pragma once

#ifdef GODOT_PHYSX_FLOW

// NVIDIA Flow's device abstraction (NvFlowContextInterface) implemented on a
// Godot RenderingDevice: Flow's simulation and render passes then run on
// Godot's own GPU device -- whatever driver/API the engine uses -- instead
// of a Vulkan device of Flow's own. This is what lets a fine simulation
// (5 cm cells) render at full resolution: the grid never leaves VRAM, and
// Flow's own ray marcher composites it straight into the scene color buffer.
//
// Plain types only at this boundary (no Flow or Godot headers leak through),
// like flow_simulation.h. Every call must be made on the RenderingDevice's
// thread (in Godot 4 that is the main thread; the effect calls land during
// RenderingServer::draw on the same thread).

#include <cstdint>

class RenderingDevice;
struct NvFlowContextInterface;
struct NvFlowContext;

namespace FlowContextRD {

struct Context;

Context *create(RenderingDevice *p_rd);
void destroy(Context *p_context);

NvFlowContextInterface *get_interface();
NvFlowContext *get_flow_context(Context *p_context);

// Ends a Flow frame: hands acquired resources over, resets per-frame
// transients, schedules this frame's readbacks, advances the frame counter.
void flush(Context *p_context);

// A RenderingDevice texture owned by Godot (scene color / depth), handed to
// Flow as a transient for this frame; Flow never frees it. 2D, p_format is
// an NvFlowFormat.
void *import_texture(Context *p_context, uint64_t p_rid, uint32_t p_width, uint32_t p_height, int p_format);
// The RenderingDevice texture behind a transient Flow returned.
uint64_t transient_texture_rid(void *p_transient);
uint64_t transient_buffer_rid(void *p_transient);

// Diagnostics.
struct Stats {
	int pipelines = 0;
	int pipeline_failures = 0;
	int passes = 0; // compute passes recorded in the last frame
	int buffers = 0;
	int textures = 0;
	uint64_t current_frame = 0;
	uint64_t last_completed_frame = 0;
};
Stats get_stats(Context *p_context);

} // namespace FlowContextRD

#endif // GODOT_PHYSX_FLOW
