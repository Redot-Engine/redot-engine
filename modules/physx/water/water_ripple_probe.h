/**************************************************************************/
/*  water_ripple_probe.h                                                  */
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

#include "water_solver.h"

#include "core/object/ref_counted.h"

// Thin RefCounted bridge exposing WaterSolver to GDScript, same precedent as
// GodotPhysXVehicleProbe/GodotPhysXBlastProbe -- exists purely so a real
// windowed GDScript test can drive the ripple layer end to end before
// PhysXWaterSurface3D (the real node) is built. Not a real node itself.
class WaterRippleProbe : public RefCounted {
	GDCLASS(WaterRippleProbe, RefCounted);

protected:
	static void _bind_methods();

private:
	WaterSolver solver;

public:
	// Device present (independent of whether configure() has finished
	// building on the render thread yet) -- check this before the first
	// configure() call.
	bool has_device() const;
	// Device present AND the render thread has finished building for the
	// current configure() -- ready to step(). Async dispatch means this can
	// take a frame or two to flip true after configure() returns.
	bool is_available() const;
	void configure(int p_grid_resolution, Vector2 p_domain_size, float p_depth, float p_damping, float p_gravity, float p_water_level);
	// Ocean (FFT) layer settings -- must be called before the first step() to
	// take effect (rebuilds ocean buffers via a fresh WaterSolver::configure()
	// call under the hood).
	void configure_ocean(int p_ocean_grid_resolution, Vector2 p_ocean_domain_size, float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude);
	void step(double p_delta);
	void submit_sphere(int p_owner, Vector3 p_world_pos, float p_radius, float p_strength);
	void clear_sphere(int p_owner);
	void submit_impulse(Vector3 p_world_pos, float p_radius, float p_strength);

	// Bilinear sample of the last get_height_grid() readback -- call
	// refresh_height_grid() first each time you want a fresh readback (it's a
	// blocking GPU sync, deliberately not automatic every call).
	void refresh_height_grid();
	float sample_height(float p_world_x, float p_world_z) const;

	// Raw ocean (FFT) layer readback -- a probe-only accessor. The imag array
	// is the choppy x displacement, the slope array dh/dx straight from the
	// FFT (see water_spectrum_evolve.glsl).
	void refresh_ocean_height_grid();
	PackedFloat32Array get_ocean_height_array() const;
	PackedFloat32Array get_ocean_imag_array() const;
	PackedFloat32Array get_ocean_slope_x_array() const;

private:
	WaterSolver::Settings pending_settings;
	Vector<float> cached_height;
	int cached_n = 0;
	Vector2 cached_domain_size;
	Vector<float> cached_ocean_height;
	Vector<float> cached_ocean_imag;
	Vector<float> cached_ocean_slope_x;
};
