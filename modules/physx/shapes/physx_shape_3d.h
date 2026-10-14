/**************************************************************************/
/*  physx_shape_3d.h                                                      */
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

/**
 * @file physx_shape_3d.h
 * @brief Abstract base class for collision shape resources.
 *
 * PhysXShape3D is the shared, RID-tracked collision geometry resource (the
 * Godot analog of a CollisionShape3D's shape). It is NOT a PxShape — it is a
 * *blueprint*. When a body attaches this shape, it calls create_shape() to
 * produce a per-actor PxShape instance from the blueprint.
 *
 * Subclasses (box, sphere, capsule, etc.) implement get_physx_geometry() to
 * produce the specific PxGeometry, handling scale where needed.
 *
 * Shape sharing: multiple bodies can reference the same PhysXShape3D. The
 * owners map tracks how many bodies use it so that geometry changes can be
 * propagated to all attached instances via _notify_shape_changed().
 */

#ifndef PHYSX_SHAPE_3D_H
#define PHYSX_SHAPE_3D_H

// --- Godot core ---
#include "core/error/error_macros.h"
#include "core/math/aabb.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"
#include "physx_rid_owner.h"
#include "servers/physics_3d/physics_server_3d.h"

// --- PhysX API ---
#include <PxPhysicsAPI.h>

class PhysXShapedObject3D;

class PhysXShape3D {
public:
	PhysXShape3D() {}
	virtual ~PhysXShape3D();

	// --- Owner tracking (shapes are shared across bodies) ---
	void add_owner(PhysXShapedObject3D *p_owner);
	void remove_owner(PhysXShapedObject3D *p_owner);
	bool has_owner(PhysXShapedObject3D *p_owner) const;
	const HashMap<PhysXShapedObject3D *, int> &get_owners() const { return owners; }

	// --- RID Management ---
	void set_rid(const RID &p_rid) { physx_rid.set_rid(p_rid); }
	RID get_rid() const { return physx_rid.get_rid(); }

	/**
	 * @brief Produces the PxGeometry for this shape, scaled by p_scale.
	 *
	 * Each subclass implements this to generate its specific geometry type
	 * (PxBoxGeometry, PxSphereGeometry, etc.) applying the scale factor.
	 * @return true if geometry generation succeeded.
	 */
	virtual bool get_physx_geometry(physx::PxGeometryHolder &p_geometry_holder, const physx::PxVec3 &p_scale) const = 0;

	/**
	 * @brief Creates a per-actor PxShape from this blueprint.
	 *
	 * If p_material is nullptr, the server's default material is used.
	 * The returned PxShape has refcount 1; the caller must attachShape()
	 * then release() to transfer ownership to the actor.
	 */
	virtual physx::PxShape *create_shape(
			physx::PxPhysics &p_physics,
			const physx::PxVec3 &p_scale,
			const physx::PxMaterial *p_material,
			physx::PxShapeFlags p_flags = physx::PxShapeFlag::eVISUALIZATION | physx::PxShapeFlag::eSCENE_QUERY_SHAPE | physx::PxShapeFlag::eSIMULATION_SHAPE);

	/**
	 * @brief Returns the local alignment pose for geometries that need it.
	 *
	 * For example, PhysX capsules are X-axis aligned but Godot's are Y-axis,
	 * so the capsule subclass returns a 90-degree rotation around Z. Shapes
	 * whose geometry the body scale also scales (heightfields: the
	 * quantization-restore lift; separation rays: the forward half-length
	 * offset) apply the same scale to their pose translation — the consumer
	 * passes the identical scale it gave get_physx_geometry().
	 */
	virtual physx::PxTransform get_local_pose(const physx::PxVec3 &p_scale = physx::PxVec3(1.0f)) const {
		return physx::PxTransform(physx::PxIdentity);
	}

	/**
	 * @brief Translates a raw PhysX face index into the user-facing index.
	 *
	 * Concave shapes cooked with backface collision duplicate every triangle
	 * (reversed winding), so raw PhysX face indices index the DOUBLED mesh;
	 * Godot consumers expect indices into their own faces array. The base
	 * implementation is the identity.
	 */
	virtual int translate_face_index(int p_face_index) const { return p_face_index; }
	/// Whether this shape collides from both sides (ConcavePolygonShape3D's
	/// backface_collision). Queries that must honor Godot's back-face rule
	/// (a back face only counts when the shape allows it AND the query asks
	/// for it) consult this through the shape blueprint.
	virtual bool has_backface_collision() const { return false; }

	// Shape data
	virtual void set_data(const Variant &p_data) = 0;
	virtual Variant get_data() const = 0;

	// Shape metadata
	virtual PhysicsServer3D::ShapeType get_type() const = 0;
	virtual bool is_convex() const = 0;

	// Queries
	virtual AABB get_aabb() const = 0;

	// Margin
	virtual void set_margin(float p_margin);
	float get_margin() const { return margin; }

	/** @brief Detach PxShape from all owners.
	 *
	 * Called during destruction when the shape holds external pointers
	 * (e.g. PxCustomGeometry callbacks) that must not outlive the PxShape
	 * instances. Derived classes override this to perform the detachment.
	 */
	virtual void detach_from_owners();

	virtual float get_solver_bias() const {
		return default_solver_bias;
	}

	virtual void set_solver_bias(float p_bias) {
		if (!Math::is_equal_approx(p_bias, default_solver_bias)) {
			WARN_PRINT_ONCE(
					"Custom solver bias is not supported by the PhysX backend. "
					"The value will be ignored.");
		}
	}

protected:
	void _notify_shape_changed();

	PhysXRIDOwner physx_rid;
	HashMap<PhysXShapedObject3D *, int> owners;
	mutable Mutex owners_mutex;
	// Contact offset (PhysX uses the SUM of both shapes' contact offsets for pair
	// contact generation). The previous default of 0.04 produced a combined zone
	// of 0.08 m — large enough that a sphere moving at even moderate speed (6 m/s
	// → 0.1 m/step at 60 Hz) enters the zone and gets a *speculative* contact
	// (positive separation). PhysX's solver applies restitution to speculative
	// contacts but applies ZERO friction to them, so the sphere bounces without
	// rolling. A smaller offset (0.01 → combined 0.02 m) ensures fast-moving
	// bodies penetrate before the contact fires, producing penetrating contacts
	// that receive full friction. The trade-off is minor: slightly deeper initial
	// penetration per step, corrected by the solver's Baumgarte stabilization.
	float margin = 0.01f;
	float default_solver_bias = 0.0f;
};
#endif // PHYSX_SHAPE_3D_H
