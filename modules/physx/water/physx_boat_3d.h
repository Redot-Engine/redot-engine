/**************************************************************************/
/*  physx_boat_3d.h                                                       */
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

#include "scene/3d/node_3d.h"

class PhysXBuoyancy3D;
class RigidBody3D;

// Drives its parent RigidBody3D as a boat: the forces it makes itself, on
// top of the PhysXBuoyancy3D (a sibling) that floats it. Forward is +Z.
// - Propeller: throttle pushes from propeller_position, only while it's in
//   the water; steering turns it (an outboard or stern drive), and reverse
//   is weaker.
// - Leg/rudder: a plate at the propeller that the water pushes on sideways
//   as the boat moves past it (and as the propeller's wash runs over it) --
//   it steers with flow, keeps the stern from sliding out, and points the
//   boat along its course.
// - Hull: drag in the boat's own frame -- low along the keel, high sideways
//   and vertically -- plus pitch/yaw/roll damping, all scaled by how much
//   of the hull is in the water.
// Only applies RigidBody3D forces -- works with any physics engine.
class PhysXBoat3D : public Node3D {
	GDCLASS(PhysXBoat3D, Node3D);

	float throttle = 0.0f;
	float steering = 0.0f;
	float max_thrust = 900.0f;
	float reverse_thrust_ratio = 0.4f;
	float max_steer_angle = Math::deg_to_rad(30.0f);
	Vector3 propeller_position = Vector3(0, -0.2f, -2.2f);
	float rudder_strength = 15.0f; // N per (m/s)^2 of flow across the leg
	float propeller_wash_speed = 4.0f; // m/s of flow over the leg at full throttle
	Vector3 hull_drag = Vector3(250.0f, 400.0f, 18.0f); // N per (m/s)^2: x sideways, y vertical, z along the keel
	Vector3 hull_linear_drag = Vector3(60.0f, 200.0f, 8.0f); // N per m/s, same axes
	Vector3 angular_damping = Vector3(300.0f, 500.0f, 150.0f); // N*m per rad/s: x pitch, y yaw, z roll

	ObjectID buoyancy_id;
	float applied_thrust = 0.0f;

	void _physics_step();

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	RigidBody3D *get_body() const;
	PhysXBuoyancy3D *get_buoyancy() const;

	void set_throttle(float p_throttle) { throttle = CLAMP(p_throttle, -1.0f, 1.0f); }
	float get_throttle() const { return throttle; }
	void set_steering(float p_steering) { steering = CLAMP(p_steering, -1.0f, 1.0f); }
	float get_steering() const { return steering; }
	void set_max_thrust(float p_thrust) { max_thrust = MAX(p_thrust, 0.0f); }
	float get_max_thrust() const { return max_thrust; }
	void set_reverse_thrust_ratio(float p_ratio) { reverse_thrust_ratio = CLAMP(p_ratio, 0.0f, 1.0f); }
	float get_reverse_thrust_ratio() const { return reverse_thrust_ratio; }
	void set_max_steer_angle(float p_angle) { max_steer_angle = CLAMP(p_angle, 0.0f, (float)Math::PI * 0.5f); }
	float get_max_steer_angle() const { return max_steer_angle; }
	void set_propeller_position(const Vector3 &p_position) { propeller_position = p_position; }
	Vector3 get_propeller_position() const { return propeller_position; }
	void set_rudder_strength(float p_strength) { rudder_strength = MAX(p_strength, 0.0f); }
	float get_rudder_strength() const { return rudder_strength; }
	void set_propeller_wash_speed(float p_speed) { propeller_wash_speed = MAX(p_speed, 0.0f); }
	float get_propeller_wash_speed() const { return propeller_wash_speed; }
	void set_hull_drag(const Vector3 &p_drag) { hull_drag = p_drag.max(Vector3()); }
	Vector3 get_hull_drag() const { return hull_drag; }
	void set_hull_linear_drag(const Vector3 &p_drag) { hull_linear_drag = p_drag.max(Vector3()); }
	Vector3 get_hull_linear_drag() const { return hull_linear_drag; }
	void set_angular_damping(const Vector3 &p_damping) { angular_damping = p_damping.max(Vector3()); }
	Vector3 get_angular_damping() const { return angular_damping; }

	// Speed along the keel (m/s, negative going astern) and the propeller's
	// thrust this tick (N; 0 while it's out of the water).
	float get_forward_speed() const;
	float get_applied_thrust() const { return applied_thrust; }

	PackedStringArray get_configuration_warnings() const override;
};
