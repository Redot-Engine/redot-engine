/**************************************************************************/
/*  physx_gas_emitter_3d.cpp                                              */
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

#include "physx_gas_emitter_3d.h"

#include "core/object/class_db.h"

void PhysXGasEmitter3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_enabled", "enabled"), &PhysXGasEmitter3D::set_enabled);
	ClassDB::bind_method(D_METHOD("is_enabled"), &PhysXGasEmitter3D::is_enabled);
	ClassDB::bind_method(D_METHOD("set_shape", "shape"), &PhysXGasEmitter3D::set_shape);
	ClassDB::bind_method(D_METHOD("get_shape"), &PhysXGasEmitter3D::get_shape);
	ClassDB::bind_method(D_METHOD("set_radius", "radius"), &PhysXGasEmitter3D::set_radius);
	ClassDB::bind_method(D_METHOD("get_radius"), &PhysXGasEmitter3D::get_radius);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &PhysXGasEmitter3D::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &PhysXGasEmitter3D::get_size);
	ClassDB::bind_method(D_METHOD("set_velocity", "velocity"), &PhysXGasEmitter3D::set_velocity);
	ClassDB::bind_method(D_METHOD("get_velocity"), &PhysXGasEmitter3D::get_velocity);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &PhysXGasEmitter3D::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &PhysXGasEmitter3D::get_density);
	ClassDB::bind_method(D_METHOD("set_divergence", "divergence"), &PhysXGasEmitter3D::set_divergence);
	ClassDB::bind_method(D_METHOD("get_divergence"), &PhysXGasEmitter3D::get_divergence);
	ClassDB::bind_method(D_METHOD("set_swirl", "swirl"), &PhysXGasEmitter3D::set_swirl);
	ClassDB::bind_method(D_METHOD("get_swirl"), &PhysXGasEmitter3D::get_swirl);

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "set_enabled", "is_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shape", PROPERTY_HINT_ENUM, "Sphere,Box"), "set_shape", "get_shape");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "radius", PROPERTY_HINT_RANGE, "0.01,2.0,0.01,suffix:m"), "set_radius", "get_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "size", PROPERTY_HINT_NONE, "suffix:m"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_velocity", "get_velocity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "0.0,4.0,0.01"), "set_density", "get_density");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "divergence", PROPERTY_HINT_RANGE, "-10.0,10.0,0.05,suffix:m/s"), "set_divergence", "get_divergence");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "swirl", PROPERTY_HINT_RANGE, "-10.0,10.0,0.05,suffix:m/s"), "set_swirl", "get_swirl");

	BIND_ENUM_CONSTANT(SHAPE_SPHERE);
	BIND_ENUM_CONSTANT(SHAPE_BOX);
}
