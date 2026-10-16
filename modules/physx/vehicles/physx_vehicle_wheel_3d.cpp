/**************************************************************************/
/*  physx_vehicle_wheel_3d.cpp                                            */
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

#include "physx_vehicle_wheel_3d.h"

#include "physx_motorcycle_3d.h"
#include "physx_tank_3d.h"
#include "physx_vehicle_3d.h"

#include "core/object/class_db.h"

void PhysXVehicleWheel3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Capture the authored hardpoint BEFORE the parent vehicle ever
			// simulates (it starts owning this transform once live).
			_authored_position = get_position();
			_authored_basis = get_transform().basis;
			_authored_valid = true;
			if (PhysXVehicle3D *v = Object::cast_to<PhysXVehicle3D>(get_parent())) {
				vehicle = v;
				v->wheels.push_back(this);
				v->_rebuild_if_live();
			} else if (PhysXMotorcycle3D *m = Object::cast_to<PhysXMotorcycle3D>(get_parent())) {
				motorcycle = m;
				m->wheels.push_back(this);
				m->_rebuild_if_live();
			} else if (PhysXTank3D *t = Object::cast_to<PhysXTank3D>(get_parent())) {
				tank = t;
				t->wheels.push_back(this);
				t->_rebuild_if_live();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			if (vehicle) {
				vehicle->wheels.erase(this);
				vehicle->_rebuild_if_live();
				vehicle = nullptr;
			} else if (motorcycle) {
				motorcycle->wheels.erase(this);
				motorcycle->_rebuild_if_live();
				motorcycle = nullptr;
			} else if (tank) {
				tank->wheels.erase(this);
				tank->_rebuild_if_live();
				tank = nullptr;
			}
		} break;
	}
}

void PhysXVehicleWheel3D::_rebuild_parent_if_live() {
	if (vehicle) {
		vehicle->_rebuild_if_live();
	} else if (motorcycle) {
		motorcycle->_rebuild_if_live();
	} else if (tank) {
		tank->_rebuild_if_live();
	}
}

void PhysXVehicleWheel3D::set_radius(real_t p_v) {
	radius = p_v;
	update_gizmos();
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_half_width(real_t p_v) {
	half_width = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_wheel_mass(real_t p_v) {
	wheel_mass = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_wheel_moment_of_inertia(real_t p_v) {
	wheel_moment_of_inertia = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_damping_rate(real_t p_v) {
	damping_rate = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_suspension_travel(real_t p_v) {
	suspension_travel = p_v;
	update_gizmos();
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_suspension_stiffness(real_t p_v) {
	suspension_stiffness = p_v;
	update_gizmos();
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_suspension_damping(real_t p_v) {
	suspension_damping = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_tire_lateral_stiffness(real_t p_v) {
	tire_lateral_stiffness = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_tire_longitudinal_stiffness(real_t p_v) {
	tire_longitudinal_stiffness = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_tire_camber_stiffness(real_t p_v) {
	tire_camber_stiffness = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_tire_friction(real_t p_v) {
	tire_friction = MAX(p_v, 0.0f);
	// LIVE, not a rebuild: tire friction is gameplay-tunable (wet/ice/gravel
	// surface changes mid-drive). The 4W vehicle can rewrite the built
	// composition's per-wheel road-friction default + tire force curve in
	// place; the motorcycle/tank compositions just rebuild.
	if (vehicle) {
		vehicle->_update_live_wheel_tire_friction(this);
	} else {
		_rebuild_parent_if_live();
	}
}
void PhysXVehicleWheel3D::set_tire_rest_grip(real_t p_v) {
	tire_rest_grip = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_tire_slide_grip(real_t p_v) {
	tire_slide_grip = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_use_as_steering(bool p_v) {
	use_as_steering = p_v;
	_rebuild_parent_if_live();
}
void PhysXVehicleWheel3D::set_use_as_traction(bool p_v) {
	use_as_traction = p_v;
	_rebuild_parent_if_live();
}

PackedStringArray PhysXVehicleWheel3D::get_configuration_warnings() const {
	PackedStringArray warnings = Node3D::get_configuration_warnings();
	if (!Object::cast_to<PhysXVehicle3D>(get_parent()) && !Object::cast_to<PhysXMotorcycle3D>(get_parent()) && !Object::cast_to<PhysXTank3D>(get_parent())) {
		warnings.push_back(RTR("PhysXVehicleWheel3D serves to provide a wheel to a PhysXVehicle3D, PhysXMotorcycle3D, or PhysXTank3D. Please use it as a child of one of those."));
	}
	return warnings;
}

void PhysXVehicleWheel3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_radius", "value"), &PhysXVehicleWheel3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &PhysXVehicleWheel3D::get_radius);
	ClassDB::bind_method(D_METHOD("set_half_width", "value"), &PhysXVehicleWheel3D::set_half_width);
	ClassDB::bind_method(D_METHOD("get_half_width"), &PhysXVehicleWheel3D::get_half_width);
	ClassDB::bind_method(D_METHOD("set_wheel_mass", "value"), &PhysXVehicleWheel3D::set_wheel_mass);
	ClassDB::bind_method(D_METHOD("get_wheel_mass"), &PhysXVehicleWheel3D::get_wheel_mass);
	ClassDB::bind_method(D_METHOD("set_wheel_moment_of_inertia", "value"), &PhysXVehicleWheel3D::set_wheel_moment_of_inertia);
	ClassDB::bind_method(D_METHOD("get_wheel_moment_of_inertia"), &PhysXVehicleWheel3D::get_wheel_moment_of_inertia);
	ClassDB::bind_method(D_METHOD("set_damping_rate", "value"), &PhysXVehicleWheel3D::set_damping_rate);
	ClassDB::bind_method(D_METHOD("get_damping_rate"), &PhysXVehicleWheel3D::get_damping_rate);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.05,2,0.01,or_greater"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "half_width", PROPERTY_HINT_RANGE, "0.01,1,0.01,or_greater"), "set_half_width", "get_half_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_mass", PROPERTY_HINT_RANGE, "0.1,200,0.1,or_greater"), "set_wheel_mass", "get_wheel_mass");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "wheel_moment_of_inertia", PROPERTY_HINT_RANGE, "0.01,50,0.01,or_greater"), "set_wheel_moment_of_inertia", "get_wheel_moment_of_inertia");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping_rate", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), "set_damping_rate", "get_damping_rate");

	ClassDB::bind_method(D_METHOD("set_suspension_travel", "value"), &PhysXVehicleWheel3D::set_suspension_travel);
	ClassDB::bind_method(D_METHOD("get_suspension_travel"), &PhysXVehicleWheel3D::get_suspension_travel);
	ClassDB::bind_method(D_METHOD("set_suspension_stiffness", "value"), &PhysXVehicleWheel3D::set_suspension_stiffness);
	ClassDB::bind_method(D_METHOD("get_suspension_stiffness"), &PhysXVehicleWheel3D::get_suspension_stiffness);
	ClassDB::bind_method(D_METHOD("set_suspension_damping", "value"), &PhysXVehicleWheel3D::set_suspension_damping);
	ClassDB::bind_method(D_METHOD("get_suspension_damping"), &PhysXVehicleWheel3D::get_suspension_damping);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_travel", PROPERTY_HINT_RANGE, "0.01,1,0.01,or_greater"), "set_suspension_travel", "get_suspension_travel");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_stiffness", PROPERTY_HINT_RANGE, "1000,200000,100,or_greater"), "set_suspension_stiffness", "get_suspension_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "suspension_damping", PROPERTY_HINT_RANGE, "0,20000,10,or_greater"), "set_suspension_damping", "get_suspension_damping");

	ClassDB::bind_method(D_METHOD("set_tire_lateral_stiffness", "value"), &PhysXVehicleWheel3D::set_tire_lateral_stiffness);
	ClassDB::bind_method(D_METHOD("get_tire_lateral_stiffness"), &PhysXVehicleWheel3D::get_tire_lateral_stiffness);
	ClassDB::bind_method(D_METHOD("set_tire_longitudinal_stiffness", "value"), &PhysXVehicleWheel3D::set_tire_longitudinal_stiffness);
	ClassDB::bind_method(D_METHOD("get_tire_longitudinal_stiffness"), &PhysXVehicleWheel3D::get_tire_longitudinal_stiffness);
	ClassDB::bind_method(D_METHOD("set_tire_camber_stiffness", "value"), &PhysXVehicleWheel3D::set_tire_camber_stiffness);
	ClassDB::bind_method(D_METHOD("get_tire_camber_stiffness"), &PhysXVehicleWheel3D::get_tire_camber_stiffness);
	ClassDB::bind_method(D_METHOD("set_tire_friction", "value"), &PhysXVehicleWheel3D::set_tire_friction);
	ClassDB::bind_method(D_METHOD("get_tire_friction"), &PhysXVehicleWheel3D::get_tire_friction);
	ClassDB::bind_method(D_METHOD("set_tire_rest_grip", "value"), &PhysXVehicleWheel3D::set_tire_rest_grip);
	ClassDB::bind_method(D_METHOD("get_tire_rest_grip"), &PhysXVehicleWheel3D::get_tire_rest_grip);
	ClassDB::bind_method(D_METHOD("set_tire_slide_grip", "value"), &PhysXVehicleWheel3D::set_tire_slide_grip);
	ClassDB::bind_method(D_METHOD("get_tire_slide_grip"), &PhysXVehicleWheel3D::get_tire_slide_grip);
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_lateral_stiffness", PROPERTY_HINT_RANGE, "1000,100000,100,or_greater"), "set_tire_lateral_stiffness", "get_tire_lateral_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_longitudinal_stiffness", PROPERTY_HINT_RANGE, "1000,100000,100,or_greater"), "set_tire_longitudinal_stiffness", "get_tire_longitudinal_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_camber_stiffness", PROPERTY_HINT_RANGE, "0,100000,100,or_greater"), "set_tire_camber_stiffness", "get_tire_camber_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_friction", PROPERTY_HINT_RANGE, "0.1,3,0.01,or_greater"), "set_tire_friction", "get_tire_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_rest_grip", PROPERTY_HINT_RANGE, "0,1.5,0.01"), "set_tire_rest_grip", "get_tire_rest_grip");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tire_slide_grip", PROPERTY_HINT_RANGE, "0,1.5,0.01"), "set_tire_slide_grip", "get_tire_slide_grip");

	ClassDB::bind_method(D_METHOD("set_use_as_steering", "value"), &PhysXVehicleWheel3D::set_use_as_steering);
	ClassDB::bind_method(D_METHOD("is_used_as_steering"), &PhysXVehicleWheel3D::is_used_as_steering);
	ClassDB::bind_method(D_METHOD("set_use_as_traction", "value"), &PhysXVehicleWheel3D::set_use_as_traction);
	ClassDB::bind_method(D_METHOD("is_used_as_traction"), &PhysXVehicleWheel3D::is_used_as_traction);
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_as_steering"), "set_use_as_steering", "is_used_as_steering");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_as_traction"), "set_use_as_traction", "is_used_as_traction");
}
