/**************************************************************************/
/*  physx_water_spray_3d.h                                                */
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

#include "core/math/random_pcg.h"
#include "scene/3d/node_3d.h"

class GPUParticles3D;
class GPUParticlesCollisionBox3D;
class ParticleProcessMaterial;
class PhysXWaterSurface3D;
class RigidBody3D;

// Spray thrown up by a hull moving through a PhysXWaterSurface3D, drawn by
// ordinary GPUParticles3D nodes authored in the editor -- this node only
// drives them from the body's motion. Make it a child of the RigidBody3D
// (a boat) and point its emitter lists at GPUParticles3D nodes:
// - bow_emitters spray while the hull moves through the water: rate and
//   launch speed rise with forward speed, none where the hull is out of the
//   water.
// - stern_emitters spray with a sibling PhysXBoat3D's propeller thrust.
// - slam_emitters fire once (restart) when the hull drops back onto the
//   water fast enough, scaled with the impact.
// Every tick the emitters are moved to the water surface under where they
// were placed on the hull, level and turned with the hull's heading, so the
// spray leaves the water there whatever the hull's pitch and roll.
// With water_collision, a GPUParticlesCollisionBox3D follows the hull with
// its top on the water around it (a plane fitted to the surface there, wave
// slope included), so spray with collision on lands on the water and skids
// along it instead of falling through. The emitters are then lifted
// collision_margin clear of it: a particle born touching a collider loses
// its speed off the surface at once. The plane is eased over
// collision_smoothing so spray resting on it isn't jolted each time the
// water's heights update.
// With spray_foam, the spray leaves foam where it lands: each tick the
// landing spots are worked out from the emitters' launch points, speeds and
// gravity, and specks of foam are scattered along where the spray lands and
// skids (spray_foam_offset further out), on the sibling PhysXWaterWake3D --
// water stirred by droplets, not churned solid (a slam leaves a patch).
class PhysXWaterSpray3D : public Node3D {
	GDCLASS(PhysXWaterSpray3D, Node3D);

public:
	enum Role {
		ROLE_BOW,
		ROLE_STERN,
		ROLE_SLAM,
	};

private:
	struct Emitter {
		ObjectID id;
		Role role = ROLE_BOW;
		Transform3D rest; // as placed, relative to the body
		Ref<ParticleProcessMaterial> material; // this emitter's own copy
		float base_velocity_min = 0.0f;
		float base_velocity_max = 0.0f;
		float base_ratio = 1.0f;
		float prev_gap = 0.0f; // hull point height above the water, last tick
		float prev_water = 0.0f;
		bool have_prev = false;
		float cooldown = 0.0f;
		// Where the spray leaves the emitter (its local frame), a few spots
		// along it, and the direction it goes: for working out where it lands.
		Vector<Vector3> launch_points;
		Vector<Vector3> launch_dirs;
		float inherit_velocity = 0.0f;
		float gravity = 9.8f;
		float friction = 0.0f; // its collision friction: how far landed spray skids
	};

	NodePath water_surface_path; // empty = the sibling PhysXBuoyancy3D's
	TypedArray<NodePath> bow_emitters;
	TypedArray<NodePath> stern_emitters;
	TypedArray<NodePath> slam_emitters;
	float start_speed = 1.5f;
	float reference_speed = 6.0f;
	float slow_velocity_scale = 0.4f;
	float submerge_depth = 0.25f;
	float slam_speed = 1.5f;
	float slam_full_speed = 4.0f;
	float slam_cooldown = 0.3f;
	bool water_collision = false; // opt-in: a collider for spray with collision on to land on
	float collision_size = 12.0f;
	float collision_fit_radius = 3.0f;
	float collision_margin = 0.08f;
	float collision_smoothing = 0.1f;
	float spray_foam = 0.6f; // how white each speck of landed-spray foam starts
	float spray_foam_density = 0.3f; // chance per launch spot per tick of a speck, at full spray
	float spray_foam_offset = 0.4f; // m further out than the spray lands

	Vector<Emitter> emitters;
	GPUParticlesCollisionBox3D *collider = nullptr; // internal child
	bool have_plane = false; // the collider's top, as last placed
	Vector3 plane_point;
	Vector3 plane_normal;
	// The collider's place at the last two physics ticks: it is moved every
	// frame between them, as spray steps every frame, not every tick.
	Transform3D collider_from;
	Transform3D collider_to;
	bool have_collider_xform = false;
	bool have_smooth = false; // the eased plane: height under the hull, normal
	float smooth_height = 0.0f;
	Vector3 smooth_normal;
	ObjectID water_id;
	ObjectID wake_id; // the sibling PhysXWaterWake3D: the hull's own wake isn't water it clears
	bool built = false;

	void _build();
	void _clear();
	void _physics_step(double p_delta);
	void _add_emitters(const TypedArray<NodePath> &p_paths, Role p_role);
	void _update_collider(const PhysXWaterSurface3D *p_water, const Vector3 &p_center, const Basis &p_level, double p_delta);
	void _deposit_foam(const Emitter &p_emitter, const GPUParticles3D *p_particles, const Vector3 &p_body_velocity, float p_ratio, float p_velocity_scale, double p_delta);
	RandomPCG rng;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	RigidBody3D *get_body() const;
	PhysXWaterSurface3D *get_water_surface() const;

	void set_water_surface_path(const NodePath &p_path);
	NodePath get_water_surface_path() const { return water_surface_path; }
	void set_bow_emitters(const TypedArray<NodePath> &p_paths);
	TypedArray<NodePath> get_bow_emitters() const { return bow_emitters; }
	void set_stern_emitters(const TypedArray<NodePath> &p_paths);
	TypedArray<NodePath> get_stern_emitters() const { return stern_emitters; }
	void set_slam_emitters(const TypedArray<NodePath> &p_paths);
	TypedArray<NodePath> get_slam_emitters() const { return slam_emitters; }
	void set_start_speed(float p_speed) { start_speed = MAX(p_speed, 0.0f); }
	float get_start_speed() const { return start_speed; }
	void set_reference_speed(float p_speed) { reference_speed = MAX(p_speed, 0.01f); }
	float get_reference_speed() const { return reference_speed; }
	void set_slow_velocity_scale(float p_scale) { slow_velocity_scale = CLAMP(p_scale, 0.0f, 1.0f); }
	float get_slow_velocity_scale() const { return slow_velocity_scale; }
	void set_submerge_depth(float p_depth) { submerge_depth = MAX(p_depth, 0.01f); }
	float get_submerge_depth() const { return submerge_depth; }
	void set_slam_speed(float p_speed) { slam_speed = MAX(p_speed, 0.0f); }
	float get_slam_speed() const { return slam_speed; }
	void set_slam_full_speed(float p_speed) { slam_full_speed = MAX(p_speed, 0.01f); }
	float get_slam_full_speed() const { return slam_full_speed; }
	void set_slam_cooldown(float p_seconds) { slam_cooldown = MAX(p_seconds, 0.0f); }
	float get_slam_cooldown() const { return slam_cooldown; }
	void set_water_collision(bool p_enabled);
	bool get_water_collision() const { return water_collision; }
	void set_collision_size(float p_size) { collision_size = MAX(p_size, 0.5f); }
	float get_collision_size() const { return collision_size; }
	void set_collision_fit_radius(float p_radius) { collision_fit_radius = MAX(p_radius, 0.1f); }
	float get_collision_fit_radius() const { return collision_fit_radius; }
	void set_collision_margin(float p_margin) { collision_margin = MAX(p_margin, 0.0f); }
	float get_collision_margin() const { return collision_margin; }
	void set_collision_smoothing(float p_seconds) { collision_smoothing = MAX(p_seconds, 0.0f); }
	float get_collision_smoothing() const { return collision_smoothing; }
	void set_spray_foam(float p_foam) { spray_foam = MAX(p_foam, 0.0f); }
	float get_spray_foam() const { return spray_foam; }
	void set_spray_foam_density(float p_density) { spray_foam_density = CLAMP(p_density, 0.0f, 1.0f); }
	float get_spray_foam_density() const { return spray_foam_density; }
	void set_spray_foam_offset(float p_offset) { spray_foam_offset = p_offset; }
	float get_spray_foam_offset() const { return spray_foam_offset; }

	PackedStringArray get_configuration_warnings() const override;
};
