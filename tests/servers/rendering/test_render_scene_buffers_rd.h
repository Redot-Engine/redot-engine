/**************************************************************************/
/*  test_render_scene_buffers_rd.h                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
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

#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/rendering_server.h"

#include "tests/test_macros.h"

// The luminance multiplier contract compositor effects build their brightness
// thresholds on: a buffer that can hold HDR values reports 1.0 (Forward+, or
// the Mobile renderer with HDR 2D enabled), a buffer that squeezes HDR into a
// UNORM format reports 2.0 (the Mobile renderer's default).
TEST_CASE("[RenderSceneBuffersRD] Luminance multiplier") {
	Ref<RenderSceneBuffersRD> buffers;
	buffers.instantiate();

	CHECK(buffers->get_luminance_multiplier() == 1.0);

	buffers->set_can_be_storage(false); // The Mobile renderer's configuration.
	CHECK(buffers->get_luminance_multiplier() == 2.0);

	buffers->set_can_be_storage(true);
	CHECK(buffers->get_luminance_multiplier() == 1.0);
}
