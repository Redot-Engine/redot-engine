/**************************************************************************/
/*  physx_gas_emitter_3d.h                                                */
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

// A data-only placement node for a PhysXGas3D injection point -- add one (or
// several) as a scene node and reference it from PhysXGas3D.emitters, the way
// CollisionShape3D describes a shape without simulating anything itself. Its
// own global transform gives the emitter's world position (and, for
// EMITTER_BOX, orientation is NOT yet honored -- v1 is axis-aligned only,
// see GasSolver's header). No solver of its own; PhysXGas3D reads these
// properties directly each step.
//
// Mirrors NVIDIA Flow's per-emitter parameter block (NvFlowEmitterSphereParams
// / NvFlowEmitterBoxParams) at a fraction of the scope: velocity/divergence/
// density/radius-or-size, like Flow's own fields of the same name, but no
// combustion channels (temperature/fuel/burn -- PhysXGas3D has no such
// channels to feed yet) and no per-emitter couple rates (this solver
// hard-injects, see gas_bs_step.glsl).
class PhysXGasEmitter3D : public Node3D {
	GDCLASS(PhysXGasEmitter3D, Node3D);

public:
	enum Shape { SHAPE_SPHERE,
		SHAPE_BOX };

private:
	bool enabled = true;
	Shape shape = SHAPE_SPHERE;
	float radius = 0.22f; // SHAPE_SPHERE
	Vector3 size = Vector3(0.44f, 0.44f, 0.44f); // SHAPE_BOX, full size (halved internally)
	Vector3 velocity = Vector3(0, 1.4f, 0); // local, rotated (not scaled) by this node's transform
	float density = 1.0f;
	// Outward-radial speed (m/s) from this node's position, added on top of
	// velocity -- an explosion/burst emitter wants every cell pushed away
	// from center, not all pushed the same direction (Flow's own
	// "divergence" field, same name/meaning).
	float divergence = 0.0f;
	// Tangential speed (m/s) around world +Y through this node's position --
	// directly authors rotation instead of relying on vorticity confinement
	// to amplify whatever incidental curl the injection jitter seeds.
	float swirl = 0.0f;

protected:
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool is_enabled() const { return enabled; }
	void set_shape(Shape p_shape) { shape = p_shape; }
	Shape get_shape() const { return shape; }
	void set_radius(float p_radius) { radius = MAX(p_radius, 0.001f); }
	float get_radius() const { return radius; }
	void set_size(const Vector3 &p_size) { size = p_size.maxf(0.002f); }
	Vector3 get_size() const { return size; }
	void set_velocity(const Vector3 &p_velocity) { velocity = p_velocity; }
	Vector3 get_velocity() const { return velocity; }
	void set_density(float p_density) { density = MAX(p_density, 0.0f); }
	float get_density() const { return density; }
	void set_divergence(float p_divergence) { divergence = p_divergence; }
	float get_divergence() const { return divergence; }
	void set_swirl(float p_swirl) { swirl = p_swirl; }
	float get_swirl() const { return swirl; }
};

VARIANT_ENUM_CAST(PhysXGasEmitter3D::Shape);
