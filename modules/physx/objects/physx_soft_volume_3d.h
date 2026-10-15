/**************************************************************************/
/*  physx_soft_volume_3d.h                                                */
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
 * @file physx_soft_volume_3d.h
 * @brief GPU soft body volume — a PhysX 5 PxDeformableVolume (tetrahedral FEM
 * on CUDA) cooked from a SoftBody3D render mesh.
 */

#ifndef PHYSX_SOFT_VOLUME_3D_H
#define PHYSX_SOFT_VOLUME_3D_H

#include "core/math/aabb.h"
#include "core/math/transform_3d.h"
#include "core/templates/local_vector.h"
#include "core/templates/vector.h"

namespace physx {
class PxDeformableVolume;
class PxDeformableVolumeMesh;
class PxDeformableVolumeMaterial;
class PxDeformableAttachment;
class PxRigidActor;
class PxShape;
class PxCudaContextManager;
} // namespace physx

class PhysXSpace3D;

// GPU soft body: a PhysX 5 PxDeformableVolume (tetrahedral FEM on CUDA). Built
// from the SoftBody3D render mesh via a conforming tet cook, so its collision
// mesh surface vertices line up with the render mesh and can be read straight
// back each step. PhysXSoftBody3D owns one of these when the mesh cooks and
// CUDA is available; otherwise it falls back to the CPU XPBD solver.
class PhysXSoftVolume3D {
public:
	struct Params {
		float youngs_modulus = 5.0e5f; // stiffness
		float poisson_ratio = 0.4f; // 0..0.49; near 0.5 == incompressible
		float damping = 0.05f;
		float dynamic_friction = 0.4f;
		float total_mass = 1.0f;
		int solver_iterations = 20;
		float max_speed = 25.0f;
		uint32_t collision_layer = 1;
		uint32_t collision_mask = 1;
		// Soft-body exception slot baked into the collision shape's filter
		// data word2 (0 = none; see spaces/physx_filter_shader.h).
		uint32_t exception_slot = 0;
	};

private:
	PhysXSpace3D *space = nullptr;
	physx::PxCudaContextManager *cuda = nullptr;

	physx::PxDeformableVolume *volume = nullptr;
	physx::PxDeformableVolumeMesh *volume_mesh = nullptr;
	physx::PxDeformableVolumeMaterial *material = nullptr;
	physx::PxShape *shape = nullptr;

	uint32_t coll_vertex_count = 0; // collision tet mesh vertices (readback size)
	void *readback = nullptr; // pinned host PxVec4* : deformed collision positions
	// SOFT-4: persistent pinned scratch buffers for the pin/impulse ops. The
	// old per-call PX_EXT_PINNED_MEMORY_ALLOC/FREE churned the allocator on
	// every script call; these are allocated once per build. The *_valid
	// flags let consecutive calls in one step batch onto a single D2H (the
	// scratch holds the device state as of the last H2D; read_back()
	// invalidates after each solve).
	void *sim_pos_scratch = nullptr; // pinned host PxVec4* (like readback)
	void *sim_vel_scratch = nullptr; // pinned host PxVec4*
	bool pos_scratch_valid = false;
	bool vel_scratch_valid = false;
	bool simulated_once = false;
	float total_mass = 1.0f;
	LocalVector<float> base_inv_mass; // per sim vertex, from updateMass -- for unpin
	LocalVector<physx::PxDeformableAttachment *> attachments; // rigid attachments

	// render (welded) vertex -> nearest collision-mesh vertex, from rest state.
	LocalVector<uint32_t> welded_to_coll;
	LocalVector<Vector3> read_positions; // world space, coll-mesh order
	LocalVector<Vector3> read_normals; // per coll vertex
	LocalVector<int32_t> surface_indices; // welded triangle list, for normals
	AABB bounds;

	void _destroy();
	void _recompute_normals_and_bounds();
	static double _estimate_mesh_volume(const Vector<Vector3> &p_verts, const Vector<int32_t> &p_indices, const Transform3D &p_xform);

public:
	// Returns false if the tet cook or GPU allocation fails -- the caller then
	// uses the CPU solver instead. `p_world_verts`/`p_indices` are the welded
	// render mesh; `p_xform` is the body's world placement.
	bool build(PhysXSpace3D *p_space, const Vector<Vector3> &p_world_verts,
			const Vector<int32_t> &p_indices, const Transform3D &p_xform, const Params &p_params);

	/// Writes the soft-body exception slot into the collision shape's filter
	/// data word2 (0 = no slot; see spaces/physx_filter_shader.h). Called by
	/// PhysXSoftBody3D when it joins its first collision exception.
	void set_exception_slot(uint32_t p_slot);
	void apply_params(const Params &p_params);
	bool is_valid() const { return volume != nullptr; }

	// Called by the space after fetchResults(): pull deformed positions off the GPU.
	void read_back();
	// Deformed world-space position for a welded render vertex.
	Vector3 get_vertex_position(uint32_t p_welded_index) const;
	uint32_t welded_vertex_count() const { return welded_to_coll.size(); }
	Vector3 get_vertex_normal(uint32_t p_welded_index) const;
	AABB get_bounds() const { return bounds; }

	// Pins, indexed by welded render vertex. `p_targets[i]` with a finite x is a
	// world position the pinned vertex is held at; NaN.x pins it wherever it is.
	void set_pins(const Vector<int> &p_welded_indices, const Vector<Vector3> &p_targets);
	void add_central_impulse(const Vector3 &p_impulse);
	void add_point_impulse(uint32_t p_welded_index, const Vector3 &p_impulse);

	// Rigid attachment: pins the given welded render vertices to
	// p_rigid with a PxDeformableAttachment (SDK positional constraints). The
	// attachment is created against the actors' CURRENT poses — the soft body
	// and the rigid must both be in the same scene for it to be active. The
	// volume keeps ownership; release_attachment() or _destroy() frees it.
	/// Returns null when the SDK refuses the attachment (logged).
	physx::PxDeformableAttachment *create_rigid_attachment(const Vector<int> &p_welded_indices, physx::PxRigidActor *p_rigid);
	void release_attachment(physx::PxDeformableAttachment *&p_attachment);

	PhysXSoftVolume3D() {}
	~PhysXSoftVolume3D();
};
#endif // PHYSX_SOFT_VOLUME_3D_H
