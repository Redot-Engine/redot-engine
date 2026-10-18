/**************************************************************************/
/*  flow_scatter_rd.h                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
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

#pragma once

#include "flow_simulation.h"

#include "core/math/projection.h"
#include "core/math/vector2i.h"
#include "core/math/vector3.h"
#include "core/templates/rid.h"

class RenderingDevice;

// PhysXFlowSimulation3D's light scattering: point and spot lights glowing in
// the gas. Flow's own renderer lights the gas with one directional light (its
// self-shadow volume), so this is a second pass of our own, run right after
// Flow composites the gas into the frame: it marches each pixel's view ray
// through Flow's sparse density (the same volume and table Flow's ray marcher
// read, see FlowScatterSource), and at each step with smoke adds what the
// nearby lights scatter toward the camera -- each light's color and energy
// with Godot's own distance (and spot cone) falloff, dimmed by the gas between
// it and the point (a short march toward the light) and by the gas between the
// point and the camera. The sum is added onto the scene color.
// (Ported from the upstream godot_physx module's FlowScatterRD.)
class FlowScatterRD {
public:
	static constexpr int MAX_LIGHTS = 16;

	// std430, 160 bytes.
	struct Light {
		float position_range[4] = {}; // meters; w: range
		float color_decay[4] = {}; // linear rgb * energy; w: attenuation exponent
		float direction_cutoff[4] = {}; // spot: direction it points, w: cos(spot angle); omni: w = -2
		float spot[4] = {}; // x: spot attenuation exponent
		// Its shadow in Godot's shadow atlas, as Godot's own volumetric fog reads
		// it (see LightStorage::update_light_buffers), but from world space:
		// omni: world -> light local; spot: world -> atlas projection.
		float shadow_matrix[16] = {};
		float atlas_rect[4] = {};
		float shadow[4] = {}; // x: opacity (0: no shadow), y: bias, zw: omni dual-paraboloid flip offset
	};

	struct Params {
		Projection inv_view_projection; // clip space -> world meters
		Vector3 camera_position; // meters
		float sigma = 4.0f; // extinction per meter per unit smoke (Flow's attenuation)
		float color[3] = { 0.8f, 0.8f, 0.8f }; // the smoke's albedo
		float strength = 1.0f;
		float anisotropy = 0.4f; // Henyey-Greenstein g
		float step_scale = 1.0f; // view step, in cells
		int shadow_steps = 8; // toward each light; 0: no self-shadow
		float shadow_reach = 4.0f; // meters
		int max_steps = 256;
		bool shadows = true; // sample the lights' shadows (scene objects block their glow)
		int resolution_divisor = 4; // 1: every pixel; 2: half resolution each way; 4: quarter
	};

private:
	RID shader;
	RID pipeline;
	RID upsample_shader;
	RID upsample_pipeline;
	// The scattered light at the scattering resolution (rgb) + scene distance (a).
	RID scatter_texture;
	Size2i scatter_size;
	RID linear_sampler;
	RID point_sampler;
	RID shadow_sampler;
	RID params_buffer;
	RID lights_buffer;
	bool failed = false;

	bool _ensure(RenderingDevice *p_rd);

public:
	// After Flow composited into p_color (rgba16f, storage); p_depth is the
	// scene depth it was drawn against.
	// p_shadow_atlas: the frame's shadow atlas (RID() if there is none; lights
	// then glow unshadowed).
	void run(RenderingDevice *p_rd, const FlowScatterSource &p_source, const Params &p_params, const Light *p_lights, int p_light_count, const RID &p_depth, const RID &p_color, const RID &p_shadow_atlas, const Size2i &p_size);
	void free(RenderingDevice *p_rd);
};
