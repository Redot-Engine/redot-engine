/**************************************************************************/
/*  physx_flow_emitter_3d.h                                               */
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

#ifdef GODOT_PHYSX_FLOW

#include "scene/3d/node_3d.h"

// Placement marker for one PhysXFlowSimulation3D injection point -- a
// data-only node in the spirit of CollisionShape3D: it does nothing on its
// own, a PhysXFlowSimulation3D that lists it in `emitters` reads its
// properties every step. The property set mirrors NvFlowEmitterSphereParams
// / NvFlowEmitterBoxParams (the two emitter shapes the Flow SDK supports
// natively -- the module's older PhysXGasEmitter3D was already shaped after
// these same SDK structs).
//
// Channels: velocity pushes fluid, divergence adds radial expansion
// ("nuke plume"), and the combustion trio drives the solver's real
// combustion -- temperature ignites fuel (above the layer's ignition
// threshold), burning fuel raises temperature further (rising plume) and
// generates smoke. A pure smoke emitter sets temperature/fuel to 0 and
// smoke > 0; a torch sets temperature+fuel and leaves smoke to combustion.
class PhysXFlowEmitter3D : public Node3D {
	GDCLASS(PhysXFlowEmitter3D, Node3D);

public:
	enum Shape {
		SHAPE_SPHERE,
		SHAPE_BOX,
	};

private:
	friend class PhysXFlowSimulation3D; // reads all fields each step

	bool enabled = true;
	Shape shape = SHAPE_SPHERE;
	float radius = 0.5f;
	Vector3 size = Vector3(1.0f, 1.0f, 1.0f); // full size, box only
	Vector3 velocity = Vector3(0, 2.0f, 0); // node-local, rotated by transform
	float divergence = 0.0f;
	float temperature = 0.5f;
	float fuel = 0.8f;
	float smoke = 0.0f;
	// Master scale for the SDK's per-channel coupleRate* fields (how hard
	// the emitter pushes its values into the grid; 2.0 is the SDK default).
	float couple_rate = 2.0f;
	int sub_steps = 1;

protected:
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool get_enabled() const { return enabled; }
	void set_shape(Shape p_shape) { shape = p_shape; }
	Shape get_shape() const { return shape; }
	void set_radius(float p_radius) { radius = MAX(p_radius, 0.001f); }
	float get_radius() const { return radius; }
	void set_size(const Vector3 &p_size) { size = p_size.maxf(0.001f); }
	Vector3 get_size() const { return size; }
	void set_velocity(const Vector3 &p_velocity) { velocity = p_velocity; }
	Vector3 get_velocity() const { return velocity; }
	void set_divergence(float p_divergence) { divergence = p_divergence; }
	float get_divergence() const { return divergence; }
	void set_temperature(float p_temperature) { temperature = MAX(p_temperature, 0.0f); }
	float get_temperature() const { return temperature; }
	void set_fuel(float p_fuel) { fuel = MAX(p_fuel, 0.0f); }
	float get_fuel() const { return fuel; }
	void set_smoke(float p_smoke) { smoke = MAX(p_smoke, 0.0f); }
	float get_smoke() const { return smoke; }
	void set_couple_rate(float p_rate) { couple_rate = MAX(p_rate, 0.0f); }
	float get_couple_rate() const { return couple_rate; }
	void set_sub_steps(int p_steps) { sub_steps = CLAMP(p_steps, 1, 8); }
	int get_sub_steps() const { return sub_steps; }

	PhysXFlowEmitter3D() {}
};

VARIANT_ENUM_CAST(PhysXFlowEmitter3D::Shape);

#endif // GODOT_PHYSX_FLOW
