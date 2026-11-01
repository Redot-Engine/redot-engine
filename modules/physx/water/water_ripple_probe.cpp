/**************************************************************************/
/*  water_ripple_probe.cpp                                                */
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

#include "water_ripple_probe.h"

#include "core/object/class_db.h"

void WaterRippleProbe::_bind_methods() {
	ClassDB::bind_method(D_METHOD("has_device"), &WaterRippleProbe::has_device);
	ClassDB::bind_method(D_METHOD("is_available"), &WaterRippleProbe::is_available);
	ClassDB::bind_method(D_METHOD("configure", "grid_resolution", "domain_size", "depth", "damping", "gravity", "water_level"), &WaterRippleProbe::configure);
	ClassDB::bind_method(D_METHOD("configure_ocean", "ocean_grid_resolution", "ocean_domain_size", "wind_speed", "wind_direction", "wave_amplitude"), &WaterRippleProbe::configure_ocean);
	ClassDB::bind_method(D_METHOD("step", "delta"), &WaterRippleProbe::step);
	ClassDB::bind_method(D_METHOD("submit_sphere", "owner", "world_pos", "radius", "strength"), &WaterRippleProbe::submit_sphere);
	ClassDB::bind_method(D_METHOD("clear_sphere", "owner"), &WaterRippleProbe::clear_sphere);
	ClassDB::bind_method(D_METHOD("submit_impulse", "world_pos", "radius", "strength"), &WaterRippleProbe::submit_impulse);
	ClassDB::bind_method(D_METHOD("refresh_height_grid"), &WaterRippleProbe::refresh_height_grid);
	ClassDB::bind_method(D_METHOD("sample_height", "world_x", "world_z"), &WaterRippleProbe::sample_height);
	ClassDB::bind_method(D_METHOD("refresh_ocean_height_grid"), &WaterRippleProbe::refresh_ocean_height_grid);
	ClassDB::bind_method(D_METHOD("get_ocean_height_array"), &WaterRippleProbe::get_ocean_height_array);
	ClassDB::bind_method(D_METHOD("get_ocean_imag_array"), &WaterRippleProbe::get_ocean_imag_array);
	ClassDB::bind_method(D_METHOD("get_ocean_slope_x_array"), &WaterRippleProbe::get_ocean_slope_x_array);
}

bool WaterRippleProbe::has_device() const {
	return solver.has_device();
}

bool WaterRippleProbe::is_available() const {
	return solver.is_available();
}

void WaterRippleProbe::configure(int p_grid_resolution, Vector2 p_domain_size, float p_depth, float p_damping, float p_gravity, float p_water_level) {
	pending_settings.grid_resolution = p_grid_resolution;
	pending_settings.domain_size = p_domain_size;
	pending_settings.depth = p_depth;
	pending_settings.damping = p_damping;
	pending_settings.gravity = p_gravity;
	pending_settings.water_level = p_water_level;
	solver.configure(pending_settings);
}

void WaterRippleProbe::configure_ocean(int p_ocean_grid_resolution, Vector2 p_ocean_domain_size, float p_wind_speed, Vector2 p_wind_direction, float p_wave_amplitude) {
	pending_settings.ocean_grid_resolution = p_ocean_grid_resolution;
	pending_settings.ocean_domain_size = p_ocean_domain_size;
	pending_settings.wind_speed = p_wind_speed;
	pending_settings.wind_direction = p_wind_direction;
	pending_settings.wave_amplitude = p_wave_amplitude;
	solver.configure(pending_settings); // rebuild -- h0 (re)generated here
}

void WaterRippleProbe::step(double p_delta) {
	solver.step(p_delta);
}

void WaterRippleProbe::submit_sphere(int p_owner, Vector3 p_world_pos, float p_radius, float p_strength) {
	solver.submit_sphere((uint64_t)p_owner, p_world_pos, p_radius, p_strength);
}

void WaterRippleProbe::clear_sphere(int p_owner) {
	solver.clear_sphere((uint64_t)p_owner);
}

void WaterRippleProbe::submit_impulse(Vector3 p_world_pos, float p_radius, float p_strength) {
	solver.submit_impulse(p_world_pos, p_radius, p_strength);
}

void WaterRippleProbe::refresh_height_grid() {
	// Force a render-thread sync first -- get_height_grid() otherwise just
	// returns whatever the last async callback landed (fine for real
	// per-tick usage, but this probe's tests want a deterministic "definitely
	// fresh after the preceding step()" read).
	solver.sync_now();
	solver.get_height_grid(cached_height, cached_n, cached_domain_size);
}

float WaterRippleProbe::sample_height(float p_world_x, float p_world_z) const {
	if (cached_height.is_empty() || cached_n <= 0) {
		return 0.0f;
	}
	// Matches water_ripple.glsl's own cell->world mapping: cell (x,y)'s
	// center is at domain center + ((x+0.5)/N*2-1, (y+0.5)/N*2-1) * domain/2.
	const float gx = (p_world_x / (cached_domain_size.x * 0.5f) + 1.0f) * 0.5f * cached_n - 0.5f;
	const float gz = (p_world_z / (cached_domain_size.y * 0.5f) + 1.0f) * 0.5f * cached_n - 0.5f;
	const int x0 = CLAMP((int)floorf(gx), 0, cached_n - 1);
	const int z0 = CLAMP((int)floorf(gz), 0, cached_n - 1);
	const int x1 = CLAMP(x0 + 1, 0, cached_n - 1);
	const int z1 = CLAMP(z0 + 1, 0, cached_n - 1);
	const float fx = CLAMP(gx - x0, 0.0f, 1.0f);
	const float fz = CLAMP(gz - z0, 0.0f, 1.0f);
	const float h00 = cached_height[z0 * cached_n + x0];
	const float h10 = cached_height[z0 * cached_n + x1];
	const float h01 = cached_height[z1 * cached_n + x0];
	const float h11 = cached_height[z1 * cached_n + x1];
	return Math::lerp(Math::lerp(h00, h10, fx), Math::lerp(h01, h11, fx), fz);
}

void WaterRippleProbe::refresh_ocean_height_grid() {
	solver.sync_now();
	int grid_size;
	Vector2 domain;
	solver.get_ocean_height_grid(cached_ocean_height, cached_ocean_imag, grid_size, domain);
	solver.get_ocean_slope_x_grid(cached_ocean_slope_x);
}

PackedFloat32Array WaterRippleProbe::get_ocean_height_array() const {
	PackedFloat32Array arr;
	arr.resize(cached_ocean_height.size());
	for (int i = 0; i < cached_ocean_height.size(); i++) {
		arr.set(i, cached_ocean_height[i]);
	}
	return arr;
}

PackedFloat32Array WaterRippleProbe::get_ocean_imag_array() const {
	PackedFloat32Array arr;
	arr.resize(cached_ocean_imag.size());
	for (int i = 0; i < cached_ocean_imag.size(); i++) {
		arr.set(i, cached_ocean_imag[i]);
	}
	return arr;
}

PackedFloat32Array WaterRippleProbe::get_ocean_slope_x_array() const {
	PackedFloat32Array arr;
	arr.resize(cached_ocean_slope_x.size());
	for (int i = 0; i < cached_ocean_slope_x.size(); i++) {
		arr.set(i, cached_ocean_slope_x[i]);
	}
	return arr;
}
