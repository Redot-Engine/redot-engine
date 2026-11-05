/**************************************************************************/
/*  physx_flow_collider_3d.h                                              */
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

#include "core/templates/local_vector.h"
#include "scene/3d/node_3d.h"

class CollisionShape3D;

// Explicit fluid collider for a PhysXFlowSimulation3D. Flow's collision model
// is collision-emitters: analytic boxes natively, triangle meshes otherwise
// (NvFlowEmitterBoxParams / NvFlowEmitterMeshParams with isPhysicsCollision).
// This node offers three sources:
//   BOX      -- analytic box from `size` (cheapest, exact);
//   SPHERE   -- cached icosphere triangle mesh from `radius` (Flow has no
//               native sphere collider);
//   SHAPE3D  -- mesh built from a referenced CollisionShape3D's convex or
//               trimesh data (cached until the shape RID changes).
//
// Bodies from the physics server (StaticBody3D/RigidBody3D/...) should use
// the simulation's own `colliders` list instead -- it consumes PhysicsServer3D
// state directly (see PhysXFlowSimulation3D's physics bridge). This node is
// for analytic/kinematic colliders without any physics body, or for pulling a
// specific shape in.
//
// Velocity coupling: when auto_velocity is on (default) linear+angular
// velocity are estimated by finite-differencing the world transform between
// the simulation's steps, so animated colliders push the fluid. Moving a
// collider in the editor therefore also stirs the fluid in editor preview.
class PhysXFlowCollider3D : public Node3D {
	GDCLASS(PhysXFlowCollider3D, Node3D);

public:
	enum Shape {
		SHAPE_BOX,
		SHAPE_SPHERE,
		SHAPE_SHAPE3D,
	};

private:
	friend class PhysXFlowSimulation3D;

	bool enabled = true;
	Shape shape = SHAPE_BOX;
	Vector3 size = Vector3(1.0f, 1.0f, 1.0f); // full size, box
	float radius = 0.5f; // sphere
	NodePath shape_source; // CollisionShape3D for SHAPE_SHAPE3D

	bool auto_velocity = true;
	Vector3 manual_linear_velocity;
	Vector3 manual_angular_velocity;

	// Finite-difference velocity estimate (updated by the simulation's step).
	bool velocity_tracked = false;
	Transform3D last_world_xform;
	Vector3 tracked_linear_velocity;
	Vector3 tracked_angular_velocity;

	// Mesh cache (sphere/shape3d variants): raw triangle soup in local space.
	bool mesh_valid = false;
	uint64_t mesh_cache_key = 0;
	LocalVector<float> mesh_positions; // xyz triples

	void _invalidate_mesh() {
		mesh_valid = false;
		mesh_cache_key = 0;
	}
	bool _ensure_mesh(); // builds mesh_positions for the current shape

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_enabled(bool p_enabled) { enabled = p_enabled; }
	bool get_enabled() const { return enabled; }
	void set_shape(Shape p_shape) {
		if (shape != p_shape) {
			shape = p_shape;
			_invalidate_mesh();
		}
	}
	Shape get_shape() const { return shape; }
	void set_size(const Vector3 &p_size) {
		if (size != p_size) {
			size = p_size.maxf(0.001f);
			_invalidate_mesh();
		}
	}
	Vector3 get_size() const { return size; }
	void set_radius(float p_radius) {
		if (!Math::is_equal_approx(radius, p_radius)) {
			radius = MAX(p_radius, 0.001f);
			_invalidate_mesh();
		}
	}
	float get_radius() const { return radius; }
	void set_shape_source(const NodePath &p_path) {
		shape_source = p_path;
		_invalidate_mesh();
	}
	NodePath get_shape_source() const { return shape_source; }
	void set_auto_velocity(bool p_enabled) { auto_velocity = p_enabled; }
	bool get_auto_velocity() const { return auto_velocity; }
	void set_manual_linear_velocity(const Vector3 &p_v) { manual_linear_velocity = p_v; }
	Vector3 get_manual_linear_velocity() const { return manual_linear_velocity; }
	void set_manual_angular_velocity(const Vector3 &p_v) { manual_angular_velocity = p_v; }
	Vector3 get_manual_angular_velocity() const { return manual_angular_velocity; }

	// Valid after the simulation stepped at least once (finite difference).
	Vector3 get_estimated_linear_velocity() const { return tracked_linear_velocity; }
	Vector3 get_estimated_angular_velocity() const { return tracked_angular_velocity; }

	// Shared icosphere triangle-soup generator (also used by the simulation
	// node's PhysicsServer3D bridge for sphere shapes). Raw xyz triples.
	static void build_icosphere_soup(float p_radius, int p_subdivisions, LocalVector<float> &r_positions);

	PhysXFlowCollider3D() {}

private:
	bool _build_shape3d_mesh(CollisionShape3D *p_cs);
};

VARIANT_ENUM_CAST(PhysXFlowCollider3D::Shape);

#endif // GODOT_PHYSX_FLOW
