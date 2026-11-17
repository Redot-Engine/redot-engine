/**************************************************************************/
/*  physx_granular_3d.h                                                   */
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

#include "physx_particle_fluid_3d.h"

// A GPU granular volume -- sand, gravel, snow. Shares all of
// PhysXParticleFluid3D's machinery (emission, mpm_colliders, domain, spawn
// region, the MultiMesh render, the RID lifecycle) but runs the material as
// Drucker-Prager elastoplastic grains instead of a liquid: it piles, holds a
// slope and gets plowed. No isosurface -- always drawn as the sphere MultiMesh.
//
// The MPM (compute) solver is the one that holds a real angle of repose;
// PhysX's PBD friction cannot pile, so `solver = Auto` always resolves to MPM
// here. Pick PBD explicitly only for a pure pour / cascade where the grains
// stay in motion.
class PhysXGranular3D : public PhysXParticleFluid3D {
	GDCLASS(PhysXGranular3D, PhysXParticleFluid3D);

	float friction = 35.0f; // internal friction angle, degrees -> angle of repose
	float hardness = 150000.0f; // Young's modulus (Pa); softer piles mush, stiffer can jitter
	float cohesion = 0.0f; // 0 = dry sand; small values pack like wet sand / snow
	float density = 900.0f; // grain "weight" (kg/m^3) -- how hard a body ploughs through (snow ~300, sand ~1500)

protected:
	static void _bind_methods();
	void _validate_property(PropertyInfo &p_property) const;

	bool _is_granular() const override { return true; }
	float _granular_friction_deg() const override { return friction; }
	float _granular_hardness() const override { return hardness; }
	float _granular_cohesion() const override { return cohesion; }
	float _granular_density() const override { return density; }

	void _reconfigure_if_live();

public:
	void set_friction(float p_deg);
	float get_friction() const { return friction; }
	void set_hardness(float p_v);
	float get_hardness() const { return hardness; }
	void set_grain_cohesion(float p_v);
	float get_grain_cohesion() const { return cohesion; }
	void set_density(float p_v);
	float get_density() const { return density; }
};
