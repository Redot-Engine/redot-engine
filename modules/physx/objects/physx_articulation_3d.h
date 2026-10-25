/**************************************************************************/
/*  physx_articulation_3d.h                                               */
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
 * @file physx_articulation_3d.h
 * @brief SKELETON implementation -- reduced-coordinate articulations.
 *
 * Wraps PhysX 5's PxArticulationReducedCoordinate: an articulation owns its
 * links (PxArticulationLink) and the reduced-coordinate joints between them,
 * and is simulated as a single actor inside a PxScene. This skeleton wires
 * the full lifecycle (create / add links / drives / limits / free) plus
 * world-pose readback for rendering. Scope limit: tendons, mimics,
 * aggregates and per-link shape transforms beyond the box placeholder are
 * not exposed (P4 in the module roadmap). Per-link collision filtering and
 * link-level velocity queries ARE exposed (see the API list).
 *
 * API surface (via PhysXServer3D, all indices 0-based):
 *   articulation_create()                       -> RID
 *   articulation_set_space(rid, space)          -> attach to a space
 *   articulation_add_link(rid, parent, parent_frame, child_frame,
 *                         joint_type, density, box_half_extents) -> link index
 *       parent -1 creates the base link (no inbound joint; joint_type ignored).
 *       joint_type: 0 = FIX, 1 = PRISMATIC, 2 = REVOLUTE, 3 = REVOLUTE_UNWRAPPED,
 *                   4 = SPHERICAL (PxArticulationJointType values).
 *       Each link gets a box collision shape of the given half extents; mass
 *       properties are computed from the shape at the given density.
 *   articulation_set_drive(rid, link, axis, stiffness, damping,
 *                          drive_target, drive_velocity, drive_type)
 *       axis: 0 = TWIST, 1 = SWING1, 2 = SWING2, 3 = X, 4 = Y, 5 = Z.
 *       drive_type: 0 = FORCE, 1 = ACCELERATION. Also unlocks the axis motion.
 *   articulation_set_limit(rid, link, axis, low, high)
 *   articulation_wake(rid)
 *   articulation_get_link_count(rid) / articulation_get_link_transform(rid, i)
 *
 * Rendering is the caller's job: poll articulation_get_link_transform and
 * mirror it onto visual meshes (see the lab when this gets a scenario).
 */

#ifndef PHYSX_ARTICULATION_3D_H
#define PHYSX_ARTICULATION_3D_H

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "core/variant/dictionary.h"
#include "physx_rid_owner.h"

namespace physx {
class PxArticulationReducedCoordinate;
class PxArticulationLink;
class PxShape;
} // namespace physx

class PhysXSpace3D;
class PhysXShape3D;

class PhysXArticulation3D : public PhysXRIDOwner {
public:
	PhysXArticulation3D();
	~PhysXArticulation3D();

	void set_space(PhysXSpace3D *p_space);
	PhysXSpace3D *get_space() const { return space; }

	/// Adds a link. parent_index -1 creates the base link. Returns the new
	/// link's index, or -1 on failure. See the file header for conventions.
	int add_link(int p_parent_index,
			const Transform3D &p_parent_frame,
			const Transform3D &p_child_frame,
			int p_joint_type,
			float p_density,
			const Vector3 &p_box_half_extents);

	/// Unlocks the axis, configures the implicit PD drive and sets targets.
	void set_drive(int p_link_index, int p_axis,
			float p_stiffness, float p_damping,
			float p_drive_target, float p_drive_velocity,
			int p_drive_type);

	/// Unlocks the axis and clamps it with a joint limit.
	void set_limit(int p_link_index, int p_axis, float p_low, float p_high);

	void set_fix_base(bool p_fix);
	void wake_up();
	void put_to_sleep();

	int get_link_count() const;
	/// World-space pose of the link (poll for rendering).
	Transform3D get_link_transform(int p_index) const;
	/// True while the articulation is sleeping (all links at rest).
	bool is_sleeping() const;

	/// Replaces the link's collision shape with a per-link instance of the
	/// shared PhysXShape3D shape_blueprint, placed at p_transform. Concave shapes
	/// degrade to query-only on links (links simulate). Mass
	/// properties are recomputed from all attached shapes at the link's
	/// stored density.
	void set_link_shape(int p_link_index, PhysXShape3D *p_shape, const Transform3D &p_transform);

	/// Per-link collision filtering (PxShape filter data word0/word1); the
	/// module's filter shader and pair-callback semantics apply unchanged.
	void set_link_collision_layer(int p_link_index, uint32_t p_layer);
	void set_link_collision_mask(int p_link_index, uint32_t p_mask);
	uint32_t get_link_collision_layer(int p_index) const;
	uint32_t get_link_collision_mask(int p_index) const;

	/// Linear + angular world velocity of the link ("linear"/"angular" keys).
	Dictionary get_link_velocity(int p_index) const;

private:
	/// One link of the articulation: the PhysX link (owned by the
	/// articulation — releasing it releases the links), its collision shape
	/// (a per-link PxShape owned by the link after attach), and the Godot-side
	/// filter/mass state used to (re)configure it.
	struct LinkRecord {
		physx::PxArticulationLink *link = nullptr;
		physx::PxShape *shape = nullptr;
		uint32_t collision_layer = 1;
		uint32_t collision_mask = 1;
		float density = 1.0f;
	};

	PhysXSpace3D *space = nullptr;
	physx::PxArticulationReducedCoordinate *px_articulation = nullptr;
	LocalVector<LinkRecord> links;

	void _destroy();
	physx::PxArticulationLink *_link(int p_index) const;
	LinkRecord *_record(int p_index);
	const LinkRecord *_record(int p_index) const;
	/// Writes the record's layer/mask into its shape's filter data.
	void _apply_link_filter(LinkRecord &p_rec) const;
};

#endif // PHYSX_ARTICULATION_3D_H
