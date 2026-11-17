/**************************************************************************/
/*  water_wake_solver.h                                                   */
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

#include "core/math/vector2.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/templates/rid.h"
#include "core/templates/safe_refcount.h"
#include "core/variant/variant.h"

class RenderingDevice;

// GPU side of the moving wake grid (see water_wake.glsl). Methods run on the
// render thread, or synchronously on a private device when headless -- the
// same arrangement as WaterSolverGPU / SkinnedClothSolverGPU.
class WaterWakeSolverGPU : public RefCounted {
	GDSOFTCLASS(WaterWakeSolverGPU, RefCounted);

public:
	static constexpr int MAX_SOURCES = 48;

	RenderingDevice *rd = nullptr;
	bool local = false;
	SafeFlag built;

	RID shader, pipeline;
	RID buf_params, buf_state_a, buf_state_b, buf_sources;
	RID tex_height;
	RID uset_atob, uset_btoa;
	bool a_is_current = true;
	int grid_size = 0;

	// Last readback: heights (m) on the grid, and the grid origin (world xz
	// of cell 0's corner) they were computed at.
	Mutex cache_mtx;
	Vector<float> height_cache;
	Vector2 cache_origin;

	void rt_compile(Ref<WaterWakeSolverGPU> p_self);
	void rt_build(Ref<WaterWakeSolverGPU> p_self, int p_grid_size);
	// p_params: packed as the shader's Params; p_sources: 8 floats per source.
	void rt_step(Ref<WaterWakeSolverGPU> p_self, PackedByteArray p_params, PackedFloat32Array p_sources, Vector2 p_origin, bool p_readback);
	void rt_on_readback(const PackedByteArray &p_data, Ref<WaterWakeSolverGPU> p_self, Vector2 p_origin);
	void rt_free(Ref<WaterWakeSolverGPU> p_self);
	void rt_free_buffers();
};

// Front end: one moving wake grid. Call step() once per physics tick with the
// world position it should be centered on.
class WaterWakeSolver {
	Ref<WaterWakeSolverGPU> gpu;
	void _dispatch(const Callable &p_call) const;

	int grid_size = 128;
	float size = 32.0f;
	bool have_origin = false;
	Vector2i origin_cell; // grid origin in whole cells of the world
	int ticks = 0;

public:
	struct Source {
		Vector2 world_xz;
		float radius = 0.5f;
		float depth = 0.1f;
		float foam = 0.0f; // foam churned in per second at the center (1 = fully white)
		bool drift = false; // foam-only: pushed out from the hull's line with the hull's foam, else stays put
	};

	bool is_available() const { return gpu.is_valid() && gpu->rd != nullptr; }
	bool is_built() const { return is_available() && gpu->built.is_set(); }

	void build(int p_cells, float p_size);
	// p_wave_speed m/s, p_damping and p_border_damping per second.
	// p_foam_persistence: seconds foam takes to fade (to 1/e); p_foam_slope: wave
	// slope past which wake waves foam, p_foam_slope_gain how much per second;
	// foam is pushed out from the bow-stern line at p_foam_spread m/s, fading
	// over p_foam_spread_falloff m. The hull churns p_hull_foam per
	// second (at the bow) into a band just outside p_hull_half_beam.
	void step(double p_delta, const Vector2 &p_center, const Vector<Source> &p_sources, float p_wave_speed, float p_damping, float p_border_damping, float p_border_width, float p_foam_persistence, float p_foam_slope, float p_foam_slope_gain, const Vector2 &p_bow, const Vector2 &p_stern, float p_foam_spread, float p_foam_spread_falloff, float p_hull_half_beam, float p_hull_foam);

	int get_cells() const { return grid_size; }
	float get_size() const { return size; }
	float get_cell_size() const { return size / grid_size; }
	// World xz of the grid's corner (cell 0), as last stepped.
	Vector2 get_origin() const { return Vector2(origin_cell) * get_cell_size(); }
	RID get_height_texture_rd_rid() const;
	// Height (m) at world xz from the last readback (a few frames behind); 0
	// outside the grid. The weight fades to 0 toward the grid's edge.
	float sample_height(const Vector2 &p_world_xz) const;

	WaterWakeSolver();
	~WaterWakeSolver();
};
