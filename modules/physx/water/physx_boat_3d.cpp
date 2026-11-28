/**************************************************************************/
/*  physx_boat_3d.cpp                                                     */
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

#include "physx_boat_3d.h"

#include "physx_buoyancy_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "scene/3d/physics/rigid_body_3d.h"

void PhysXBoat3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_throttle", "throttle"), &PhysXBoat3D::set_throttle);
	ClassDB::bind_method(D_METHOD("get_throttle"), &PhysXBoat3D::get_throttle);
	ClassDB::bind_method(D_METHOD("set_steering", "steering"), &PhysXBoat3D::set_steering);
	ClassDB::bind_method(D_METHOD("get_steering"), &PhysXBoat3D::get_steering);
	ClassDB::bind_method(D_METHOD("set_max_thrust", "thrust"), &PhysXBoat3D::set_max_thrust);
	ClassDB::bind_method(D_METHOD("get_max_thrust"), &PhysXBoat3D::get_max_thrust);
	ClassDB::bind_method(D_METHOD("set_reverse_thrust_ratio", "ratio"), &PhysXBoat3D::set_reverse_thrust_ratio);
	ClassDB::bind_method(D_METHOD("get_reverse_thrust_ratio"), &PhysXBoat3D::get_reverse_thrust_ratio);
	ClassDB::bind_method(D_METHOD("set_max_steer_angle", "angle"), &PhysXBoat3D::set_max_steer_angle);
	ClassDB::bind_method(D_METHOD("get_max_steer_angle"), &PhysXBoat3D::get_max_steer_angle);
	ClassDB::bind_method(D_METHOD("set_propeller_position", "position"), &PhysXBoat3D::set_propeller_position);
	ClassDB::bind_method(D_METHOD("get_propeller_position"), &PhysXBoat3D::get_propeller_position);
	ClassDB::bind_method(D_METHOD("set_rudder_strength", "strength"), &PhysXBoat3D::set_rudder_strength);
	ClassDB::bind_method(D_METHOD("get_rudder_strength"), &PhysXBoat3D::get_rudder_strength);
	ClassDB::bind_method(D_METHOD("set_propeller_wash_speed", "speed"), &PhysXBoat3D::set_propeller_wash_speed);
	ClassDB::bind_method(D_METHOD("get_propeller_wash_speed"), &PhysXBoat3D::get_propeller_wash_speed);
	ClassDB::bind_method(D_METHOD("set_hull_drag", "drag"), &PhysXBoat3D::set_hull_drag);
	ClassDB::bind_method(D_METHOD("get_hull_drag"), &PhysXBoat3D::get_hull_drag);
	ClassDB::bind_method(D_METHOD("set_hull_linear_drag", "drag"), &PhysXBoat3D::set_hull_linear_drag);
	ClassDB::bind_method(D_METHOD("get_hull_linear_drag"), &PhysXBoat3D::get_hull_linear_drag);
	ClassDB::bind_method(D_METHOD("set_angular_damping", "damping"), &PhysXBoat3D::set_angular_damping);
	ClassDB::bind_method(D_METHOD("get_angular_damping"), &PhysXBoat3D::get_angular_damping);
	ClassDB::bind_method(D_METHOD("get_forward_speed"), &PhysXBoat3D::get_forward_speed);
	ClassDB::bind_method(D_METHOD("get_applied_thrust"), &PhysXBoat3D::get_applied_thrust);

	ADD_GROUP("Control", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "throttle", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_throttle", "get_throttle");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "steering", PROPERTY_HINT_RANGE, "-1,1,0.01"), "set_steering", "get_steering");
	ADD_GROUP("Propeller", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_thrust", PROPERTY_HINT_RANGE, "0,100000,1,or_greater,suffix:N"), "set_max_thrust", "get_max_thrust");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "reverse_thrust_ratio", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_reverse_thrust_ratio", "get_reverse_thrust_ratio");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_steer_angle", PROPERTY_HINT_RANGE, "0,90,0.1,radians_as_degrees"), "set_max_steer_angle", "get_max_steer_angle");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "propeller_position", PROPERTY_HINT_NONE, "suffix:m"), "set_propeller_position", "get_propeller_position");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "rudder_strength", PROPERTY_HINT_RANGE, "0,10000,0.1,or_greater"), "set_rudder_strength", "get_rudder_strength");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "propeller_wash_speed", PROPERTY_HINT_RANGE, "0,50,0.1,or_greater,suffix:m/s"), "set_propeller_wash_speed", "get_propeller_wash_speed");
	ADD_GROUP("Hull", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hull_drag"), "set_hull_drag", "get_hull_drag");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "hull_linear_drag"), "set_hull_linear_drag", "get_hull_linear_drag");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "angular_damping"), "set_angular_damping", "get_angular_damping");
}

RigidBody3D *PhysXBoat3D::get_body() const {
	return Object::cast_to<RigidBody3D>(get_parent());
}

PhysXBuoyancy3D *PhysXBoat3D::get_buoyancy() const {
	return ObjectDB::get_instance<PhysXBuoyancy3D>(buoyancy_id);
}

float PhysXBoat3D::get_forward_speed() const {
	RigidBody3D *body = get_body();
	if (body == nullptr) {
		return 0.0f;
	}
	return body->get_linear_velocity().dot(body->get_global_transform().basis.get_column(2).normalized());
}

void PhysXBoat3D::_physics_step() {
	applied_thrust = 0.0f;
	RigidBody3D *body = get_body();
	PhysXBuoyancy3D *buoyancy = get_buoyancy();
	if (body == nullptr || buoyancy == nullptr) {
		return;
	}
	// How much of the hull is in the water: full once half the sample points
	// are under (a hull riding a wave rarely has all of them in at once),
	// nothing when it's airborne off a wave.
	const PackedFloat32Array depths = buoyancy->get_point_depths();
	int wet = 0;
	for (float d : depths) {
		wet += d > 0.0f ? 1 : 0;
	}
	const float in_water = depths.is_empty() ? 0.0f : MIN(2.0f * wet / depths.size(), 1.0f);

	const Transform3D xf = body->get_global_transform();
	const Basis basis = xf.basis.orthonormalized();
	const Vector3 lin = body->get_linear_velocity();
	const Vector3 ang = body->get_angular_velocity();

	// Propeller (and the leg it hangs from), turned by the steering.
	const float steer = -steering * max_steer_angle; // +steering turns left (as VehicleBody3D)
	const Vector3 leg_dir = basis.xform(Vector3(Math::sin(steer), 0.0f, Math::cos(steer)));
	const Vector3 prop_world = xf.xform(propeller_position);
	const Vector3 prop_offset = prop_world - xf.origin;
	const float prop_depth = buoyancy->get_water_height(prop_world) - prop_world.y;
	const float prop_in_water = CLAMP(prop_depth / 0.1f, 0.0f, 1.0f);
	if (prop_in_water > 0.0f) {
		const float effective_throttle = throttle >= 0.0f ? throttle : throttle * reverse_thrust_ratio;
		applied_thrust = effective_throttle * max_thrust * prop_in_water;
		body->apply_force(leg_dir * applied_thrust, prop_offset);

		// The leg is a plate in the water: the flow across it (the boat
		// moving past, plus the propeller's wash along it) pushes it
		// sideways -- steering with flow, and holding the stern on course.
		const Vector3 up = basis.get_column(1);
		Vector3 normal = up.cross(leg_dir);
		normal = normal.normalized();
		const Vector3 v_prop = lin + ang.cross(prop_offset);
		const Vector3 flow = -(v_prop - up * v_prop.dot(up)) - leg_dir * (propeller_wash_speed * MAX(throttle, 0.0f));
		const float across = flow.dot(normal);
		body->apply_force(normal * (rudder_strength * flow.length() * across * prop_in_water), prop_offset);
	}

	if (in_water > 0.0f) {
		// Hull drag in the boat's frame: x sideways, y vertical, z along the
		// keel. Sideways and vertical drag act at the bow and stern ends of
		// the hull (half each), with each end's own velocity: a hull swinging
		// round or pitching pushes its ends through the water, so its length
		// resists turning and pitching the way a keel does.
		float bow = 0.0f;
		float stern = 0.0f;
		const PackedVector3Array pts = buoyancy->get_effective_sample_points();
		for (const Vector3 &p : pts) {
			bow = MAX(bow, p.z);
			stern = MIN(stern, p.z);
		}
		const float ends[2] = { bow, stern };
		for (int e = 0; e < 2; e++) {
			const Vector3 end_offset = basis.xform(Vector3(0.0f, 0.0f, ends[e]));
			Vector3 v_local = basis.xform_inv(lin + ang.cross(end_offset));
			// Vertically, only the hull heaving and pitching: its forward speed
			// seen through a bow-up hull's frame read as both ends sinking, and
			// the drag pushing them up lifted the bow (further ahead of the
			// center of mass than the stern is behind it) harder -- more pitch,
			// more lift, until the boat climbed into a wheelie at speed.
			const Vector3 heave = Vector3(0.0f, lin.y, 0.0f) + ang.cross(end_offset);
			v_local.y = basis.get_column(1).dot(heave);
			Vector3 drag_local;
			for (int i = 0; i < 2; i++) {
				drag_local[i] = -0.5f * (hull_linear_drag[i] * v_local[i] + hull_drag[i] * v_local[i] * Math::abs(v_local[i]));
			}
			body->apply_force(basis.xform(drag_local) * in_water, end_offset);
		}
		const Vector3 v_local = basis.xform_inv(lin);
		const float along = -(hull_linear_drag.z * v_local.z + hull_drag.z * v_local.z * Math::abs(v_local.z));
		body->apply_central_force(basis.get_column(2) * along * in_water);
		const Vector3 w_local = basis.xform_inv(ang);
		body->apply_torque(basis.xform(-w_local * angular_damping) * in_water);
	}
}

void PhysXBoat3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_READY: {
			buoyancy_id = ObjectID();
			if (get_parent() != nullptr) {
				for (int i = 0; i < get_parent()->get_child_count(); i++) {
					PhysXBuoyancy3D *b = Object::cast_to<PhysXBuoyancy3D>(get_parent()->get_child(i));
					if (b != nullptr) {
						buoyancy_id = b->get_instance_id();
						break;
					}
				}
			}
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_physics_process_internal(true);
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			_physics_step();
		} break;
	}
}

PackedStringArray PhysXBoat3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	Node *parent = get_parent();
	if (Object::cast_to<RigidBody3D>(parent) == nullptr) {
		warnings.push_back(RTR("PhysXBoat3D drives a RigidBody3D: make it a child of one."));
	} else {
		bool found = false;
		for (int i = 0; i < parent->get_child_count(); i++) {
			found = found || Object::cast_to<PhysXBuoyancy3D>(parent->get_child(i)) != nullptr;
		}
		if (!found) {
			warnings.push_back(RTR("Add a PhysXBuoyancy3D next to this node (another child of the same RigidBody3D) to float the boat."));
		}
	}
	return warnings;
}
