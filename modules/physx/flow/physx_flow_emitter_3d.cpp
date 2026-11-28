/**************************************************************************/
/*  physx_flow_emitter_3d.cpp                                             */
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

#include "physx_flow_emitter_3d.h"

#ifdef GODOT_PHYSX_FLOW

#include "core/object/class_db.h"

void PhysXFlowEmitter3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &PhysXFlowEmitter3D::set_enabled);
	ClassDB::bind_method(D_METHOD("get_enabled"), &PhysXFlowEmitter3D::get_enabled);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysXFlowEmitter3D::set_shape);
	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(SHAPE_BOX);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysXFlowEmitter3D::get_shape);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &PhysXFlowEmitter3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &PhysXFlowEmitter3D::get_radius);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PhysXFlowEmitter3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PhysXFlowEmitter3D::get_size);
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &PhysXFlowEmitter3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &PhysXFlowEmitter3D::get_velocity);
	ClassDB::bind_method(D_METHOD("set_divergence", "divergence"), &PhysXFlowEmitter3D::set_divergence);
	ClassDB::bind_method(D_METHOD("get_divergence"), &PhysXFlowEmitter3D::get_divergence);
	ClassDB::bind_method(D_METHOD("set_temperature", "temperature"), &PhysXFlowEmitter3D::set_temperature);
	ClassDB::bind_method(D_METHOD("get_temperature"), &PhysXFlowEmitter3D::get_temperature);
	ClassDB::bind_method(D_METHOD("set_fuel", "fuel"), &PhysXFlowEmitter3D::set_fuel);
	ClassDB::bind_method(D_METHOD("get_fuel"), &PhysXFlowEmitter3D::get_fuel);
	ClassDB::bind_method(D_METHOD("set_smoke", "smoke"), &PhysXFlowEmitter3D::set_smoke);
	ClassDB::bind_method(D_METHOD("get_smoke"), &PhysXFlowEmitter3D::get_smoke);
	ClassDB::bind_method(D_METHOD("set_couple_rate", "rate"), &PhysXFlowEmitter3D::set_couple_rate);
	ClassDB::bind_method(D_METHOD("get_couple_rate"), &PhysXFlowEmitter3D::get_couple_rate);
	ClassDB::bind_method(D_METHOD("set_sub_steps", "steps"), &PhysXFlowEmitter3D::set_sub_steps);
	ClassDB::bind_method(D_METHOD("get_sub_steps"), &PhysXFlowEmitter3D::get_sub_steps);

	ADD_GROUP("Emitter", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "get_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "Sphere,Box"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.001,8.0,0.01,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "divergence", PROPERTY_HINT_RANGE, "-10.0,10.0,0.01"), "set_divergence", "get_divergence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "temperature", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_temperature", "get_temperature");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "fuel", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_fuel", "get_fuel");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "smoke", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_smoke", "get_smoke");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "couple_rate", PROPERTY_HINT_RANGE, "0.0,20.0,0.1"), "set_couple_rate", "get_couple_rate");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "sub_steps", PROPERTY_HINT_RANGE, "1,8,1"), "set_sub_steps", "get_sub_steps");
}

#endif // GODOT_PHYSX_FLOW
