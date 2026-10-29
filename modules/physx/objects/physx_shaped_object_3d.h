/**************************************************************************/
/*  physx_shaped_object_3d.h                                              */
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
 * @file physx_shaped_object_3d.h
 * @brief Base class for objects that own collision shapes (bodies, areas).
 *
 * Vocabulary: PhysXShape3D is the shared shape blueprint (one per RID, many
 * owners); each attachment creates this object's own PxShape shape instance;
 * areas may add a second non-trigger "detection shape" instance (see
 * AttachedShape). After attachShape() the module releases its PxShape
 * reference (refcount 1, owned by the actor) — detaching or releasing the
 * actor destroys the instance.
 */

#ifndef PHYSX_SHAPED_OBJECT_3D_H
#define PHYSX_SHAPED_OBJECT_3D_H

#include "core/math/transform_3d.h"
#include "core/templates/local_vector.h"
#include "physx_object_3d.h"
#include "shapes/physx_shape_3d.h"

#include <PxPhysicsAPI.h>

class PhysXSpace3D;

/// Applies the space-mapped rest offset (SPACE_PARAM_CONTACT_MAX_ALLOWED_
/// PENETRATION) to a freshly created PxShape: PhysX bodies rest at the SUM
/// of a pair's rest offsets, so each shape carries half the allowed
/// penetration. restOffset must stay below contactOffset (SDK validation),
/// so the contact generation distance is raised to keep the margin as the
/// gap above it. An unset value (0.0 — the default) leaves the PhysX
/// defaults untouched, so default resting behavior is unchanged.
void physx_apply_space_rest_offset(physx::PxShape *p_px_shape, float p_margin, const PhysXSpace3D *p_space);

class PhysXShapedObject3D : public PhysXObject3D {
public:
	explicit PhysXShapedObject3D(ObjectType p_type);
	virtual ~PhysXShapedObject3D();

	static physx::PxTransform to_physx_transform(const Transform3D &p_transform);

	// Godot's server contract carries per-shape scale inside the shape
	// transform basis (the standard idiom: a 1x1 trimesh quad whose shape
	// offset scales it (50,1,50) into a floor), and PhysX requires that scale
	// to be baked into the geometry itself. Mirrored (negative) scales are
	// absorbed with abs(), since PhysX geometries reject negative scales.
	Vector3 _shape_geometry_scale(const Transform3D &p_shape_transform) const;

	// Signed variant for geometries that support mirroring (convex polygon
	// meshes): uses Godot's Basis::get_scale() convention (determinant sign
	// applied to all axes), which pairs exactly with get_rotation_quaternion()'s
	// own (-1,-1,-1) absorption. A degenerate (zero-determinant) basis falls
	// back to the absolute scale.
	Vector3 _shape_geometry_scale_signed(const Transform3D &p_shape_transform) const;

	// Dispatch by shape type: only convex polygon meshes consume the signed
	// (mirror-capable) scale; every other geometry keeps the absolute scale
	// (PhysX heightfields require positive scales; primitive geometries consume
	// max-abs components; concave meshes keep Godot's "negative scale
	// unsupported" restriction). The convex shape bakes a negative determinant
	// by cooking a point-reflected hull — PxMeshScale itself rejects negative
	// components on convex geometries.
	Vector3 _shape_geometry_scale_for(const PhysXShape3D *p_shape, const Transform3D &p_shape_transform) const;

	// Called by PhysXShape3D::_notify_shape_changed when a blueprint's geometry
	// is modified (e.g. user changes box size in the editor): recreates the
	// PxGeometry of every attached instance of that blueprint.
	virtual void shape_changed(PhysXShape3D *p_shape);

	// Called when the owner's transform scale changes (body_set_state
	// TRANSFORM / area_set_transform). PhysX actor poses carry no scale, so
	// godot_physics' "the whole body transform scales its shapes" behavior is
	// produced by baking the node scale into each shape's geometry and local
	// pose (offsets scale too, like children of a scaled node).
	void refresh_shape_scaling();

	// Called by the PhysXShape3D destructor so the attached pointer is
	// nullified instead of dangling when the shape dies before its owner.
	void nullify_shape(PhysXShape3D *p_shape);

	// Called by PhysXShape3D::detach_from_owners() for custom-geometry shapes:
	// the instances must die with the blueprint because they hold its external
	// pointers (e.g. PxCustomGeometry callbacks).
	void detach_shape(PhysXShape3D *p_shape);

	// --- Shape Management ---
	void add_shape(PhysXShape3D *p_shape, const Transform3D &p_transform, bool p_disabled);
	void remove_shape(int p_index);
	void remove_shape(PhysXShape3D *p_shape);
	void set_shape_transform(int p_index, const Transform3D &p_transform);

	int get_shape_count() const { return (int)attached_shapes.size(); }
	/// Attached PxShapes (instance + detection) whose custom geometry holds
	/// p_callbacks: the reference count a PxCustomGeometry callback instance
	/// needs before it may be freed.
	int count_shapes_with_callback(const void *p_callbacks) const;
	RID get_shape_rid(int p_index) const;
	Transform3D get_shape_transform(int p_index) const;

	// Maps a trigger/contact shape back to the body-local index. PxShape::
	// userData points at the shared blueprint, not a per-instance index, so
	// the owning object's records are the only source.
	int find_shape_index(const physx::PxShape *p_px_shape) const;

	// --- PhysX Access ---
	physx::PxRigidActor *get_rigid_actor() const { return px_actor; }

	/// Public wrapper used by the server after PhysXObject3D::exception_slot changes.
	void refresh_collision_filters() { update_shapes_collision_filter(); }

protected:
	void update_shapes_collision_filter();

	/// word3 module flags (contact-notify / IS_AREA) for this object's shapes.
	uint32_t shape_filter_flags() const;

	/// Arming writes PHYSX_FILTER_FLAG_CONTACT_NOTIFY into PxFilterData.word3.
	void set_contact_notify(bool p_enable) { contact_notify = p_enable; }

	// The base returns the server's shared default material; the body override
	// lazily owns a private one so per-body bounce/friction mutations don't
	// leak across bodies sharing the default.
	virtual physx::PxMaterial *_get_shape_material();

	// Recreates every shape instance against the current px_actor. Required
	// after the actor was replaced (static<->dynamic mode switch): releasing
	// the old PxRigidActor released its PxShapes, dangling the records.
	void rebuild_shapes();

	// --- Shape lifecycle hooks (overridden by derived classes like PhysXBody3D) ---
	virtual void _on_shape_added();
	virtual void _on_shape_removed();
	virtual void _on_shape_geometry_changed();
	virtual void _on_shape_transform_changed();

	/// Shared by bodies and areas so they never shadow separate actor members.
	physx::PxRigidActor *px_actor = nullptr;

	/// nullptr for objects using the server default; set by _get_shape_material().
	physx::PxMaterial *px_material = nullptr;

	/// Protected so derived classes (areas) can access shape fields for
	/// trigger flag configuration.
	struct AttachedShape {
		PhysXShape3D *shape_blueprint = nullptr;
		physx::PxShape *shape_instance = nullptr;
		/// Optional second, non-trigger simulation shape (areas only):
		/// PhysX 5 does not report trigger-vs-trigger pairs, so a monitorable
		/// area needs a plain simulation shape for other areas' triggers to
		/// fire against. nullptr for bodies and for non-monitorable areas.
		physx::PxShape *detection_shape = nullptr;
		Transform3D relative_transform; ///< Relative to the body origin.
		bool disabled = false;
		bool trigger = false;
	};

	LocalVector<AttachedShape> attached_shapes;

	/// Baked into shape geometry + local pose origins (see refresh_shape_scaling).
	Vector3 body_scale = Vector3(1, 1, 1);

public:
	/// The node scale baked into every attached shape's geometry (motion-query
	/// ray bases scale their length with it).
	Vector3 get_body_scale() const { return body_scale; }

private:
	bool contact_notify = false;
};
#endif // PHYSX_SHAPED_OBJECT_3D_H
