/**************************************************************************/
/*  physx_soft_body_3d.h                                                  */
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
 * @file physx_soft_body_3d.h
 * @brief Soft body backing the stock SoftBody3D node (PhysicsServer3D
 * soft_body_* API).
 *
 * Each soft body resolves independently to one of two paths:
 *  - GPU: a PhysX PxDeformableVolume (tetrahedral FEM on CUDA, see
 *    physx_soft_volume_3d.h), when the mesh tetrahedralizes and a CUDA device
 *    is available. Volumes also collide with each other.
 *  - CPU: the module's XPBD solver (cloth/xpbd_cloth_solver.h) over the welded
 *    render mesh — edge constraints hold the shape, an optional volume
 *    constraint (pressure) keeps it from collapsing, and a per-vertex scene
 *    query pushes it out of rigid bodies.
 *
 * The path is chosen per body: the physics/physx_3d/soft_body/mode project
 * setting (Auto/CPU/GPU), overridden per body by
 * PhysXServer3D.soft_body_set_solver_mode (-1 project setting, 0 Auto,
 * 1 CPU, 2 GPU). Auto prefers GPU whenever it can build.
 *
 * Known limits (documented, not bugs):
 *  - Query invisibility: soft bodies have no PhysXActorUserData, so they are
 *    never reported by raycasts/overlaps/sweeps and soft_body_set_ray_pickable
 *    is inert (see physx_query_filter_callback.cpp). NOTE: wiring userData was
 *    attempted (2026-09) and REVERTED — with the bridge attached, a raycast
 *    whose ray passes through the tet mesh HANGS the engine (raycasts against
 *    PxTetrahedronMeshGeometry; overlap queries return fine, so the hang is in
 *    the ray-vs-tet traversal of this SDK build). Revisit only with an SDK
 *    upgrade or an SDK patch that fixes deformable ray traversal.
 *  - Collision exceptions: the CPU path enforces them in the per-vertex query
 *    exclude list; the GPU path routes them through the filter-shader slot
 *    registry (word2, spaces/physx_filter_shader.h) — verified working on the
 *    GPU deformable path. Only rigid-body targets are registry-routed.
 *  - Area overrides: the CPU path resolves area gravity overrides exactly
 *    like the reference (PhysXSpace3D::_resolve_soft_body_gravity); GPU
 *    deformables use the scene gravity (the solver has no per-body gravity
 *    injection). Area wind on soft bodies is not implemented (deferred).
 */

#ifndef PHYSX_SOFT_BODY_3D_H
#define PHYSX_SOFT_BODY_3D_H

#include "../cloth/xpbd_cloth_solver.h"
#include "physx_object_3d.h"
#include "physx_soft_volume_3d.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "core/math/aabb.h"
#include "core/math/transform_3d.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"

class PhysicsServer3DRenderingServerHandler;

namespace physx {
class PxActor;
} //namespace physx

class PhysXSoftBody3D : public PhysXObject3D {
public:
	PhysXSoftBody3D() :
			PhysXObject3D(OBJECT_TYPE_SOFT_BODY) {}
	~PhysXSoftBody3D() override;

	// --- Space Management (overrides PhysXObject3D pure virtual) ---
	virtual void set_space(PhysXSpace3D *p_space) override;
	/// Called by the space's destructor for every still-registered soft body.
	/// Detaches without calling back into the (dying) space and drops the GPU
	/// volume while the PxScene is still alive.
	void notify_space_destroyed();
	/// Re-resolves the solver path now (a solver-mode flip must not wait for
	/// an unrelated rebuild trigger); no-op without a space or mesh.
	void request_rebuild();

	// --- Mesh / readiness / bounds ---
	void set_mesh(RID p_mesh);
	RID get_mesh() const { return mesh; }
	bool is_ready() const { return mesh_ready; }
	AABB get_bounds() const { return bounds; }

	// --- Transform ---
	void set_transform(const Transform3D &p_transform);

	// --- Collision exceptions (layer/mask live on PhysXObject3D) ---
	void add_collision_exception(const RID &p_excepted_body) { collision_exceptions.insert(p_excepted_body); }
	void remove_collision_exception(const RID &p_excepted_body) { collision_exceptions.erase(p_excepted_body); }
	void get_collision_exceptions(List<RID> *p_exceptions) const;
	const HashSet<RID> &get_collision_exception_set() const { return collision_exceptions; }

	// --- Ray pickable ---
	void set_ray_pickable(bool p_enable) { ray_pickable = p_enable; }
	bool is_ray_pickable() const override { return ray_pickable; }

	// --- State (transform round-trip; velocity/sleep have no meaning here) ---
	void set_state(PhysicsServer3D::BodyState p_state, const Variant &p_variant);
	Variant get_state(PhysicsServer3D::BodyState p_state) const;

	// --- Stock SoftBody3D parameters, mapped onto the active solver ---
	void set_simulation_precision(int p_precision);
	int get_simulation_precision() const { return simulation_precision; }
	void set_total_mass(real_t p_mass);
	real_t get_total_mass() const { return total_mass; }
	void set_linear_stiffness(real_t p_stiffness);
	real_t get_linear_stiffness() const { return linear_stiffness; }
	void set_shrinking_factor(real_t p_factor);
	real_t get_shrinking_factor() const { return shrinking_factor; }
	void set_pressure_coefficient(real_t p_pressure);
	real_t get_pressure_coefficient() const { return pressure_coefficient; }
	void set_damping_coefficient(real_t p_damping);
	real_t get_damping_coefficient() const { return damping_coefficient; }
	void set_drag_coefficient(real_t p_drag);
	real_t get_drag_coefficient() const { return drag_coefficient; }

	// --- Point / pin operations, indexed by *render* vertex ---
	void move_point(int p_point_index, const Vector3 &p_global_position);
	Vector3 get_point_global_position(int p_point_index) const;
	void pin_point(int p_point_index, bool p_pin);
	bool is_point_pinned(int p_point_index) const;
	void unpin_all();
	void apply_point_impulse(int p_point_index, const Vector3 &p_impulse);
	void apply_point_force(int p_point_index, const Vector3 &p_force, double p_delta);
	void apply_central_impulse(const Vector3 &p_impulse);
	void apply_central_force(const Vector3 &p_force, double p_delta);

	// Rigid attachment (GPU path only): ties a render vertex to a
	// rigid body with a PxDeformableAttachment, so the vertex follows the
	// body exactly instead of being dragged by per-frame forces. The CPU
	// XPBD path has no SDK constraint facility — warn and ignore there.
	void attach_point_to_body(int p_point_index, const RID &p_body);
	void detach_point_from_body(int p_point_index);
	// Called via the space when an attached rigid body is freed (the
	// attachment would otherwise dangle).
	void release_attachments_for(physx::PxActor *p_actor);

	bool is_gpu() const { return using_gpu; }

	/// Solver-path override for THIS body (persisted across rebuilds):
	/// -1 follow project setting, 0 Auto, 1 force CPU, 2 force GPU.
	void set_solver_mode(int p_mode) { mode_override = CLAMP(p_mode, -1, 2); }
	int get_solver_mode() const { return mode_override; }

	/// Stores the soft-body exception slot and pushes it into the GPU volume's
	/// collision shape (filter word2 — see spaces/physx_filter_shader.h). The
	/// CPU path enforces exceptions via the per-vertex query exclude list and
	/// needs no slot.
	void set_exception_slot(uint32_t p_slot);

	// Driven by the space each step: step() advances the CPU path (no-op on
	// GPU); read_back() pulls the GPU volume's deformed state after
	// fetchResults() (no-op on CPU).
	void step(double p_delta, const Vector3 &p_gravity);
	void read_back();
	// Feed deformed positions/normals to the render mesh.
	void update_rendering_server(PhysicsServer3DRenderingServerHandler *p_handler);

protected:
	virtual void _update_shapes() override;

private:
	XPBDClothSolver solver;
	PhysXSoftVolume3D *volume = nullptr; // non-null == GPU path
	bool using_gpu = false;

	RID mesh;
	LocalVector<uint32_t> map_visual_to_physics; // render vertex -> solver vertex
	uint32_t visual_vertex_count = 0;
	HashSet<int> pinned_render_points; // survives set_mesh, like the other backends
	HashMap<int, Vector3> pin_targets; // render index -> world hold pos (or NaN.x)
	LocalVector<Vector3> normals; // per solver vertex, world space
	AABB bounds;
	bool mesh_ready = false;

	Transform3D transform; // initial placement; sim then runs in world space
	bool placed = false; // set once the node hands us its world transform

	HashSet<RID> collision_exceptions;
	bool ray_pickable = true;

	// Stock SoftBody3D properties, kept as given and mapped onto the solver.
	int simulation_precision = 5;
	real_t total_mass = 1.0;
	real_t linear_stiffness = 0.5;
	real_t shrinking_factor = 0.0;
	real_t pressure_coefficient = 0.0;
	real_t damping_coefficient = 0.01;
	real_t drag_coefficient = 0.0;

	RID collision_sphere; // lazily created shape for the per-vertex query
	// Per-vertex world-contact cache, refreshed once per frame and re-projected
	// against every substep (anti-tunnelling).
	LocalVector<Vector3> contact_n;
	LocalVector<Vector3> contact_p;
	LocalVector<uint8_t> contact_hit;
	// Last frame's contact plane per vertex (the anti-tunnelling memory): a
	// vertex squeezed past a thin collider's mid-plane flips rest_info's
	// deepest-hit MTD to the far face, whose plane sits UNDER the vertex --
	// without this memory the resolver would read that as the allowed side
	// and release the vertex through the track. While a vertex is behind its
	// remembered plane, that plane stays authoritative.
	LocalVector<Vector3> contact_n_prev;
	LocalVector<Vector3> contact_p_prev;
	int contact_count = 0; // penetrating vertices at the last refresh

	// Render point index -> live rigid attachment (GPU path).
	HashMap<int, physx::PxDeformableAttachment *> point_attachments;

	void _apply_solver_settings();
	// p_keep_state carries the live CPU sim over when only the mesh handle
	// changed (the node's private-duplicate swap); false re-seeds from rest at
	// the current transform (initial build, placement, teleport, space change).
	void _rebuild_from_mesh(bool p_keep_state = false);
	bool _try_build_gpu(const PackedVector3Array &p_welded, const PackedInt32Array &p_indices);
	PhysXSoftVolume3D::Params _gpu_params() const;

	// Per-body solver-path override (soft_body_set_mode): -1 = follow the
	// physics/physx_3d/soft_body/mode project setting; 0 Auto, 1 CPU, 2 GPU.
	// Replaces the unreachable "physx_soft_mode" metadata hook (soft bodies
	// never received an instance id, so that lookup could never fire).
	int mode_override = -1;
	void _sync_gpu_pins(); // push pinned_render_points / pin_targets to the volume
	void _refresh_contacts();
	void _resolve_contacts();
	void _damp_rigid_drift();
	void _update_normals_and_bounds();
};
#endif // PHYSX_SOFT_BODY_3D_H
