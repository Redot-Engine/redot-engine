/**************************************************************************/
/*  physx_water_wake_3d.h                                                 */
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

#include "water_wake_solver.h"

#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture_rd.h"

class PhysXWaterSurface3D;
class RigidBody3D;
class Texture2DRD;

// A wake that travels with its parent RigidBody3D on a PhysXWaterSurface3D:
// a small ripple simulation centered on the body that the hull pushes down as
// it moves (a row of dips along its keel, deeper with speed), out on open
// water past the surface's simulated square. The grid stays fixed to the
// water as it slides along, so the wake stays where it was made and fades
// out behind; waves reaching the grid's edge are absorbed. Drawn by the
// surface's material and included in its sample_height(), so other floating
// bodies ride it -- everywhere there's water, the shore ripple grid
// included; shore_grid_fade fades it out over that grid instead, for water
// where the body's PhysXBuoyancy3D ripples (disturb_water) drive it.
class PhysXWaterWake3D : public Node3D {
	GDCLASS(PhysXWaterWake3D, Node3D);

	NodePath water_surface_path; // empty = the sibling PhysXBuoyancy3D's
	float size = 48.0f;
	int cells = 192;
	float wave_speed = 2.2f;
	float damping = 0.25f;
	float border_damping = 8.0f;
	float hull_length = 0.0f; // 0 = from the sibling PhysXBuoyancy3D's sample points
	float hull_beam = 0.0f;
	int source_count = 5;
	float rest_depth = 0.02f;
	float wake_depth = 0.18f;
	float reference_speed = 6.0f;
	float shore_grid_fade = 0.0f;
	float bow_foam = 8.0f; // foam churned in per second along the hull's sides at the bow, at reference_speed
	float propeller_foam = 1.0f; // per second at full throttle (needs a sibling PhysXBoat3D)
	float foam_persistence = 4.0f;
	float foam_slope = 0.08f;
	float foam_slope_gain = 4.0f;
	float foam_spread = 1.0f; // m/s foam is pushed out from the hull's line

	WaterWakeSolver *solver = nullptr;
	Vector<WaterWakeSolver::Source> pending_foam; // add_foam() calls since the last step
	Ref<Texture2DRD> height_texture;
	ObjectID water_id;
	int slot = -1;
	float length = 4.0f;
	float beam = 1.2f;
	float bow_z = 2.0f;

	void _build();
	void _clear();
	void _physics_step(double p_delta);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	RigidBody3D *get_body() const;
	PhysXWaterSurface3D *get_water_surface() const;

	void set_water_surface_path(const NodePath &p_path);
	NodePath get_water_surface_path() const { return water_surface_path; }
	void set_size(float p_size);
	float get_size() const { return size; }
	void set_cells(int p_cells);
	int get_cells() const { return cells; }
	void set_wave_speed(float p_speed) { wave_speed = MAX(p_speed, 0.1f); }
	float get_wave_speed() const { return wave_speed; }
	void set_damping(float p_damping) { damping = MAX(p_damping, 0.0f); }
	float get_damping() const { return damping; }
	void set_border_damping(float p_damping) { border_damping = MAX(p_damping, 0.0f); }
	float get_border_damping() const { return border_damping; }
	void set_hull_length(float p_length) { hull_length = MAX(p_length, 0.0f); }
	float get_hull_length() const { return hull_length; }
	void set_hull_beam(float p_beam) { hull_beam = MAX(p_beam, 0.0f); }
	float get_hull_beam() const { return hull_beam; }
	static constexpr int MAX_KEEL_SOURCES = 15;
	void set_source_count(int p_count) { source_count = CLAMP(p_count, 1, MAX_KEEL_SOURCES); }
	int get_source_count() const { return source_count; }
	void set_rest_depth(float p_depth) { rest_depth = MAX(p_depth, 0.0f); }
	float get_rest_depth() const { return rest_depth; }
	void set_wake_depth(float p_depth) { wake_depth = MAX(p_depth, 0.0f); }
	float get_wake_depth() const { return wake_depth; }
	void set_reference_speed(float p_speed) { reference_speed = MAX(p_speed, 0.01f); }
	float get_reference_speed() const { return reference_speed; }
	void set_bow_foam(float p_foam) { bow_foam = MAX(p_foam, 0.0f); }
	float get_bow_foam() const { return bow_foam; }
	void set_propeller_foam(float p_foam) { propeller_foam = MAX(p_foam, 0.0f); }
	float get_propeller_foam() const { return propeller_foam; }
	void set_foam_persistence(float p_seconds) { foam_persistence = MAX(p_seconds, 0.05f); }
	float get_foam_persistence() const { return foam_persistence; }
	void set_foam_slope(float p_slope) { foam_slope = MAX(p_slope, 0.0f); }
	float get_foam_slope() const { return foam_slope; }
	void set_foam_slope_gain(float p_gain) { foam_slope_gain = MAX(p_gain, 0.0f); }
	float get_foam_slope_gain() const { return foam_slope_gain; }
	void set_foam_spread(float p_speed) { foam_spread = MAX(p_speed, 0.0f); }
	float get_foam_spread() const { return foam_spread; }
	void set_shore_grid_fade(float p_fade) { shore_grid_fade = CLAMP(p_fade, 0.0f, 1.0f); }
	float get_shore_grid_fade() const { return shore_grid_fade; }

	// Foam churned into the wake's water around a world position until the
	// next physics step: amount_per_second at the center (1 = fully white),
	// fading out over radius. Call it every tick for a steady source, or once
	// with amount / delta for a one-off patch. With drift it is pushed out
	// from the hull's line as the hull's own foam is (foam_spread), heading
	// out into the wake; without, it stays where it was put.
	void add_foam(const Vector3 &p_world_pos, float p_radius, float p_amount_per_second, bool p_drift = true);

	// Wake height (m) at a world position, from the last readback; 0 outside
	// the grid.
	float sample_height(const Vector3 &p_world_pos) const;
	// The grid's corner (world xz) and side length -- where it is drawn.
	Vector2 get_grid_origin() const;
	Ref<Texture2DRD> get_height_texture() const { return height_texture; }

	PackedStringArray get_configuration_warnings() const override;

	~PhysXWaterWake3D();
};
