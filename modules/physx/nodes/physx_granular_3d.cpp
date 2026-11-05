/**************************************************************************/
/*  physx_granular_3d.cpp                                                 */
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

#include "physx_granular_3d.h"

#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "core/string/string_name.h"

void PhysXGranular3D::_reconfigure_if_live() {
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	}
}

void PhysXGranular3D::set_friction(float p_deg) {
	friction = CLAMP(p_deg, 1.0f, 55.0f);
	_reconfigure_if_live();
}

void PhysXGranular3D::set_hardness(float p_v) {
	hardness = MAX(p_v, 100.0f);
	_reconfigure_if_live();
}

void PhysXGranular3D::set_grain_cohesion(float p_v) {
	cohesion = MAX(p_v, 0.0f);
	_reconfigure_if_live();
}

void PhysXGranular3D::set_density(float p_v) {
	density = CLAMP(p_v, 50.0f, 4000.0f);
	_reconfigure_if_live();
}

// Hide the fluid-only inspector properties -- this node is always grains.
void PhysXGranular3D::_validate_property(PropertyInfo &p_property) const {
	const StringName &n = p_property.name;
	if (n == SNAME("viscosity") || n == SNAME("surface_tension") || n == SNAME("cohesion") ||
			n == SNAME("vorticity") || n == SNAME("surface_mesh") || n == SNAME("surface_anisotropy") ||
			n == SNAME("foam_enabled") || n == SNAME("foam_particle_count") || n == SNAME("foam_lifetime") ||
			n == SNAME("foam_threshold") || n == SNAME("foam_buoyancy") || n == SNAME("foam_size")) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void PhysXGranular3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_friction", "degrees"), &PhysXGranular3D::set_friction);
	ClassDB::bind_method(D_METHOD("get_friction"), &PhysXGranular3D::get_friction);
	ClassDB::bind_method(D_METHOD("set_hardness", "hardness"), &PhysXGranular3D::set_hardness);
	ClassDB::bind_method(D_METHOD("get_hardness"), &PhysXGranular3D::get_hardness);
	ClassDB::bind_method(D_METHOD("set_grain_cohesion", "cohesion"), &PhysXGranular3D::set_grain_cohesion);
	ClassDB::bind_method(D_METHOD("get_grain_cohesion"), &PhysXGranular3D::get_grain_cohesion);
	ClassDB::bind_method(D_METHOD("set_density", "density"), &PhysXGranular3D::set_density);
	ClassDB::bind_method(D_METHOD("get_density"), &PhysXGranular3D::get_density);

	ADD_GROUP("Granular", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "friction", PROPERTY_HINT_RANGE, "1,55,0.5,degrees"), "set_friction", "get_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "hardness", PROPERTY_HINT_RANGE, "100,200000,1,or_greater,suffix:Pa"), "set_hardness", "get_hardness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "grain_cohesion", PROPERTY_HINT_RANGE, "0,0.2,0.001,or_greater"), "set_grain_cohesion", "get_grain_cohesion");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "density", PROPERTY_HINT_RANGE, "50,4000,10,suffix:kg/m³"), "set_density", "get_density");
}
