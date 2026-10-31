/**************************************************************************/
/*  physx_particle_fluid_3d.cpp                                           */
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

#include "physx_particle_fluid_3d.h"

#include "../objects/physx_gpu_particle_fluid_3d.h"
#include "../particles/mpm_fluid_solver.h"
#include "../physx_server.h"
#include "physx_chunk_emitter_3d.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "scene/3d/physics/animatable_body_3d.h"
#include "scene/3d/physics/collision_object_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/main/viewport.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/primitive_meshes.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/3d/world_boundary_shape_3d.h"
#include "scene/resources/material.h"
#include "servers/rendering/rendering_server.h"

PhysXParticleFluid3D::SolverBackend PhysXParticleFluid3D::_resolved_solver() const {
	if (solver != SOLVER_AUTO) {
		return solver;
	}
	// Granular: the MPM solver is the one that actually piles and holds a slope
	// (PhysX PBD friction cannot), so Auto prefers it even when CUDA is present.
	// Pick PBD granular explicitly for a pure pour / cascade where the grains
	// stay in motion.
	if (_is_granular()) {
		return SOLVER_MPM;
	}
	// Foam is a hard requirement when enabled: NVIDIA's CUDA diffuse-particle
	// allocation is broken through 5.11 (see the maxActiveDiffuseParticles
	// guard in physx_gpu_particle_fluid_3d.cpp), so an Auto fluid with foam on
	// runs the MPM backend, whose Vulkan diffuse layer works on any device.
	// Choose SOLVER_PBD explicitly for the CUDA solver without foam. Revisit
	// when an SDK fixes the allocation.
	if (foam_enabled) {
		return SOLVER_MPM;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	// The PBD particle buffer factory ships only in the Windows SDK libraries
	// (PhysXGPUParticleFluid3D::supports_diffuse_particles); on other platforms
	// there is no CUDA fluid to pick even when a CUDA device is present.
	return (server && server->is_gpu_dynamics_enabled() && server->particle_fluid_supports_diffuse()) ? SOLVER_PBD : SOLVER_MPM;
}

// particle_size inflates the MPM isosurface the way it does on the PBD path
// (which keys the extractor off the particle radius): it widens the SPH scatter
// kernel *and* boosts the per-particle mass, so the same positions mesh as a
// fatter, smoother body of fluid. The iso level stays a fixed fraction of the
// native packed density.
int PhysXParticleFluid3D::_mpm_resolved_grid_res() const {
	if (mpm_grid_resolution > 0) {
		return mpm_grid_resolution;
	}
	return CLAMP((int)Math::round(mpm_domain_size.x / MAX(particle_size * 2.0f, 0.001f)), 12, 96);
}

void PhysXParticleFluid3D::_mpm_surface_params(float &r_iso, float &r_kernel, float &r_boost) const {
	// With the auto grid the particle spacing tracks particle_size, so the SPH
	// scatter mostly just needs a kernel a few cells wide; a small residual boost
	// covers a pinned grid that is finer than particle_size wants.
	const float spacing = MAX(mpm_domain_size.x / MAX(_mpm_resolved_grid_res(), 1), 0.001f) * 0.5f;
	const float rel = particle_size / spacing;
	r_kernel = CLAMP(particle_size, spacing * 2.0f, spacing * 6.0f); // scatter radius (meters)
	r_boost = CLAMP(rel * rel * rel, 1.0f, 24.0f);
	r_iso = 0.5f; // fraction of the native packed kernel density (the solver resolves it)

	if (_mpm_emit_mode) {
		// A poured stream is genuinely sparse in freefall (grid-transfer fluid
		// has no particle cohesion to draw it into a thread the way the PBD path
		// does), so with the normal iso level it reconstructs as disconnected
		// drips. Lower the threshold for the emitter case: fewer accumulated
		// neighbors are needed to cross the surface, so the falling column
		// fuses -- with no change to the scatter-loop cost, and the dense pool
		// (well above any threshold) barely moves.
		r_iso = 0.18f;
	}
}

void PhysXParticleFluid3D::_mpm_apply_surface_params() {
	if (mpm == nullptr) {
		return;
	}
	float iso, kernel, boost;
	_mpm_surface_params(iso, kernel, boost);
	mpm->set_surface_params(iso, kernel, boost);
}

void PhysXParticleFluid3D::_mpm_configure(bool p_prefill) {
	if (mpm == nullptr) {
		return;
	}
	// Set before _mpm_surface_params() below -- it widens the scatter kernel in
	// emit mode.
	_mpm_emit_mode = !p_prefill;

	MPMFluidSolver::Settings s;
	s.particle_target = particle_count;
	s.substeps = mpm_substeps;
	s.stiffness = mpm_stiffness;
	s.viscosity = viscosity;
	s.gravity = Vector3(0, -9.8f, 0) * gravity_scale;
	s.domain = mpm_domain_size;
	s.spawn_region = spawn_region_size;
	s.granular = _is_granular();
	if (s.granular) {
		s.granular_hardness = _granular_hardness();
		s.granular_friction_deg = _granular_friction_deg();
		s.granular_cohesion = _granular_cohesion();
		s.rest_density = _granular_density(); // grain "weight" -- scales the collider coupling
	}

	// The MPM grid and the fluid particle spacing are coupled: too few particles
	// per cell and the sim is noisy. particle_size sets the particle spacing (as
	// on the PBD path), so grid_res follows from it -- ~2 cells per particle --
	// unless the user pinned it. This is what lets a modest particle_count fill a
	// volume: bigger particle_size => coarser grid => fatter particles.
	s.grid_res = _mpm_resolved_grid_res();
	_mpm_surface_params(s.surface_iso, s.surface_kernel, s.surface_boost);

	// Vulkan diffuse (foam/spray/bubble) layer, Ihmsen et al. 2012 potentials
	// auto-normalized on the GPU. The PBD/CUDA path keeps NVIDIA's own
	// diffuse particles (currently behind the maxActiveDiffuseParticles guard --
	// see physx_gpu_particle_fluid_3d.cpp); if a future SDK fixes that
	// allocation, PBD foam returns and this stays the non-CUDA fallback.
	// Granular beds don't foam. The shared foam_threshold property is PBD-scaled
	// (default 300); MPM potential units are normalized so the default maps to
	// 3.0.
	s.foam_enabled = foam_enabled && !_is_granular();
	s.foam_capacity = foam_particle_count;
	s.foam_threshold = MAX(foam_threshold * 0.01f, 0.05f);
	s.foam_lifetime = MAX(foam_lifetime, 0.1f);
	s.foam_buoyancy = CLAMP(foam_buoyancy, 0.0f, 1.0f);

	mpm->configure(s, get_global_transform(), p_prefill);
	_mpm_configured = mpm->is_available();

	// Size the MultiMesh to the buffer capacity (prefill: the seeded slab count;
	// emit: particle_count).
	if (multimesh.is_valid() && mpm->is_available()) {
		RenderingServer *rs = RenderingServer::get_singleton();
		rs->multimesh_allocate_data(multimesh, mpm->get_capacity(), RS::MULTIMESH_TRANSFORM_3D);
		rs->multimesh_set_mesh(multimesh, particle_mesh->get_rid());
		rs->multimesh_set_visible_instances(multimesh, 0);
	}
}

// Resolve a node to an analytic MPM collider. Accepts a CollisionShape3D
// directly, or a CollisionObject3D (uses its first shape). Sphere, box, capsule
// and infinite-plane map exactly; anything else falls back to a bounding sphere.
static bool resolve_mpm_collider(Node3D *p_node, MPMFluidSolver::Collider &out) {
	out.velocity = Vector3();

	Ref<Shape3D> shape;
	Transform3D xf = p_node->get_global_transform();

	if (CollisionShape3D *cshape = Object::cast_to<CollisionShape3D>(p_node)) {
		shape = cshape->get_shape();
	} else if (CollisionObject3D *co = Object::cast_to<CollisionObject3D>(p_node)) {
		List<uint32_t> owners;
		co->get_shape_owners(&owners);
		for (uint32_t owner : owners) {
			if (co->shape_owner_get_shape_count(owner) > 0) {
				xf = xf * co->shape_owner_get_transform(owner);
				shape = co->shape_owner_get_shape(owner, 0);
				break;
			}
		}
	}

	const Vector3 scale = xf.basis.get_scale().abs();
	const float uniform_scale = MAX(scale.x, MAX(scale.y, scale.z));
	out.position = xf.origin;
	out.rotation = xf.basis.get_rotation_quaternion();

	Ref<SphereShape3D> sph = shape;
	if (sph.is_valid()) {
		out.shape = MPMFluidSolver::COLLIDER_SPHERE;
		out.extents = Vector3(MAX(sph->get_radius() * uniform_scale, 0.01f), 0, 0);
		return true;
	}
	Ref<BoxShape3D> box = shape;
	if (box.is_valid()) {
		out.shape = MPMFluidSolver::COLLIDER_BOX;
		out.extents = (box->get_size() * 0.5f) * scale;
		return true;
	}
	Ref<CapsuleShape3D> cap = shape;
	if (cap.is_valid()) {
		const float r = cap->get_radius() * MAX(scale.x, scale.z);
		const float half_h = MAX(cap->get_height() * 0.5f * scale.y - r, 0.0f);
		out.shape = MPMFluidSolver::COLLIDER_CAPSULE;
		out.extents = Vector3(MAX(r, 0.01f), half_h, 0);
		return true;
	}
	Ref<WorldBoundaryShape3D> wb = shape;
	if (wb.is_valid()) {
		const Plane p = xf.xform(wb->get_plane());
		out.shape = MPMFluidSolver::COLLIDER_PLANE;
		out.position = p.normal * p.d; // a point on the plane
		out.extents = p.normal;
		out.rotation = Quaternion();
		return true;
	}

	// Fallback: bounding sphere from the visual bounds.
	float radius = 0.5f;
	if (VisualInstance3D *vi = Object::cast_to<VisualInstance3D>(p_node)) {
		const AABB ab = vi->get_aabb();
		if (ab.has_volume()) {
			radius = ab.get_longest_axis_size() * 0.5f;
		}
	}
	out.shape = MPMFluidSolver::COLLIDER_SPHERE;
	out.position = xf.origin;
	out.extents = Vector3(MAX(radius * uniform_scale, 0.01f), 0, 0);
	return true;
}

// Where a fed MPM collider came from, so the fluid's reaction impulse can be
// routed back. Exactly one of node / chunk_emitter is set.
struct ColliderSource {
	Node3D *node = nullptr;
	PhysXChunkEmitter3D *chunk_emitter = nullptr;
	uint64_t chunk_id = 0; // stable emitter-side id (survives chunk recycles)
};

// PART-5: stable identity for a chunk collider across frames (emitter instance
// id mixed with the emitter-side chunk id).
static uint64_t _collider_id_for_chunk(const PhysXChunkEmitter3D *p_emitter, uint64_t p_chunk_id) {
	return (uint64_t)(uintptr_t)p_emitter ^ (p_chunk_id * 0x9E3779B97F4A7C15ull);
}

void PhysXParticleFluid3D::_mpm_step(double p_delta) {
	if (mpm == nullptr || !mpm->is_available() || multimesh.is_null()) {
		return;
	}
	mpm->set_domain_transform(get_global_transform());

	// Gather coupled bodies as analytic colliders. Budget is the solver's
	// MAX_COLLIDERS (32): explicitly-listed nodes first, then the nearest active
	// debris chunks of any PhysXChunkEmitter3D in the list fill what is left.
	static const int COLLIDER_BUDGET = 32;
	LocalVector<MPMFluidSolver::Collider> cols;
	LocalVector<ColliderSource> col_src;
	HashMap<ObjectID, Vector3> seen_pos;
	LocalVector<PhysXChunkEmitter3D *> chunk_emitters;

	for (int i = 0; i < mpm_colliders.size() && (int)cols.size() < COLLIDER_BUDGET; i++) {
		Node3D *n = Object::cast_to<Node3D>(get_node_or_null(mpm_colliders[i]));
		if (n == nullptr) {
			continue;
		}
		if (PhysXChunkEmitter3D *ce = Object::cast_to<PhysXChunkEmitter3D>(n)) {
			chunk_emitters.push_back(ce);
			continue; // expanded below, once the fixed colliders are counted
		}
		MPMFluidSolver::Collider c;
		if (!resolve_mpm_collider(n, c)) {
			continue;
		}

		RigidBody3D *rb = Object::cast_to<RigidBody3D>(n);
		if (rb != nullptr && !rb->is_freeze_enabled()) {
			c.velocity = rb->get_linear_velocity();
		} else if (p_delta > 0.0) {
			const Vector3 *prev = _mpm_prev_pos.getptr(n->get_instance_id());
			c.velocity = prev ? (c.position - *prev) / (float)p_delta : Vector3();
		}
		seen_pos[n->get_instance_id()] = c.position;
		cols.push_back(c);
		col_src.push_back(ColliderSource{ n, nullptr, 0 });
	}

	// Auto colliders: every non-static body overlapping the domain this step,
	// nearest first, filling whatever the explicit list leaves of the budget.
	if (mpm_auto_colliders && (int)cols.size() < COLLIDER_BUDGET && is_inside_tree()) {
		Ref<World3D> world = get_world_3d();
		PhysicsDirectSpaceState3D *ss = world.is_valid() ? world->get_direct_space_state() : nullptr;
		if (ss != nullptr) {
			if (_mpm_query_shape.is_null()) {
				_mpm_query_shape = PhysicsServer3D::get_singleton()->box_shape_create();
			}
			PhysicsServer3D::get_singleton()->shape_set_data(_mpm_query_shape, mpm_domain_size * 0.5f);

			PhysicsDirectSpaceState3D::ShapeParameters qp;
			qp.shape_rid = _mpm_query_shape;
			qp.transform = get_global_transform();
			qp.collide_with_bodies = true;
			qp.collide_with_areas = false;

			PhysicsDirectSpaceState3D::ShapeResult res[COLLIDER_BUDGET];
			const int hits = ss->intersect_shape(qp, res, COLLIDER_BUDGET);

			struct AutoCand {
				MPMFluidSolver::Collider c;
				Node3D *node;
				float dist2;
			};
			struct Closest {
				bool operator()(const AutoCand &a, const AutoCand &b) const { return a.dist2 < b.dist2; }
			};
			const Vector3 dcentre = get_global_transform().origin;
			LocalVector<AutoCand> acands;
			for (int h = 0; h < hits; h++) {
				Node3D *n = Object::cast_to<Node3D>(res[h].collider);
				if (n == nullptr) {
					continue;
				}
				// Skip immovable bodies (walls / floor -- list those once); keep
				// AnimatableBody3D (moving platforms) and every dynamic body.
				if (Object::cast_to<StaticBody3D>(n) != nullptr && Object::cast_to<AnimatableBody3D>(n) == nullptr) {
					continue;
				}
				if (seen_pos.has(n->get_instance_id())) {
					continue; // already fed from the explicit list
				}
				MPMFluidSolver::Collider c;
				if (!resolve_mpm_collider(n, c)) {
					continue;
				}
				RigidBody3D *rb = Object::cast_to<RigidBody3D>(n);
				if (rb != nullptr && !rb->is_freeze_enabled()) {
					c.velocity = rb->get_linear_velocity();
				} else if (p_delta > 0.0) {
					const Vector3 *prev = _mpm_prev_pos.getptr(n->get_instance_id());
					c.velocity = prev ? (c.position - *prev) / (float)p_delta : Vector3();
				}
				acands.push_back(AutoCand{ c, n, (float)(c.position - dcentre).length_squared() });
			}
			acands.sort_custom<Closest>();
			const int take = MIN((int)acands.size(), COLLIDER_BUDGET - (int)cols.size());
			for (int i = 0; i < take; i++) {
				seen_pos[acands[i].node->get_instance_id()] = acands[i].c.position;
				cols.push_back(acands[i].c);
				col_src.push_back(ColliderSource{ acands[i].node, nullptr, 0 });
			}
		}
	}
	_mpm_prev_pos = seen_pos;

	// Debris chunks: keep only those overlapping the fluid domain, nearest first.
	if (!chunk_emitters.is_empty() && (int)cols.size() < COLLIDER_BUDGET) {
		const Transform3D dxf = get_global_transform();
		const Vector3 dcentre = dxf.origin;
		const Vector3 dhalf = mpm_domain_size * 0.5f + Vector3(0.25f, 0.25f, 0.25f);

		struct Cand {
			MPMFluidSolver::Collider c;
			PhysXChunkEmitter3D *emitter;
			uint64_t chunk_id;
			float dist2;
		};
		struct ClosestChunk {
			bool operator()(const Cand &a, const Cand &b) const { return a.dist2 < b.dist2; }
		};
		LocalVector<Cand> cands;
		LocalVector<PhysXChunkEmitter3D::ChunkBody> bodies;
		for (PhysXChunkEmitter3D *ce : chunk_emitters) {
			ce->get_active_chunk_bodies(bodies);
			for (const PhysXChunkEmitter3D::ChunkBody &b : bodies) {
				const Vector3 rel = b.xform.origin - dcentre;
				const float reach = b.half_extents.length();
				if (Math::abs(rel.x) > dhalf.x + reach || Math::abs(rel.y) > dhalf.y + reach || Math::abs(rel.z) > dhalf.z + reach) {
					continue;
				}
				MPMFluidSolver::Collider c;
				c.shape = b.sphere ? MPMFluidSolver::COLLIDER_SPHERE : MPMFluidSolver::COLLIDER_BOX;
				c.position = b.xform.origin;
				c.rotation = b.xform.basis.get_rotation_quaternion();
				c.extents = b.sphere ? Vector3(MAX(b.half_extents.x, 0.01f), 0, 0) : b.half_extents;
				c.velocity = b.velocity;
				cands.push_back(Cand{ c, ce, b.id, (float)rel.length_squared() });
			}
		}
		cands.sort_custom<ClosestChunk>();
		const int take = MIN((int)cands.size(), COLLIDER_BUDGET - (int)cols.size());
		for (int i = 0; i < take; i++) {
			cols.push_back(cands[i].c);
			col_src.push_back(ColliderSource{ nullptr, cands[i].emitter, cands[i].chunk_id });
		}
	}

	// The isosurface march + triple GPU->CPU readback + ArrayMesh rebuild is by
	// far the most expensive part of a step -- and it dominates when a collider
	// churns the pool and the triangle count spikes. The surface only needs to
	// look continuous, not track every substep, so re-mesh it every 3rd step
	// (~20 Hz) and let the sim run full-rate underneath.
	static const uint32_t SURFACE_EVERY = 3;
	const bool want_surface = surface_mesh && array_mesh.is_valid() && !_is_granular() && (_mpm_surface_tick++ % SURFACE_EVERY == 0);

	if (mpm->get_particle_count() == 0) {
		// Emit mode, nothing spawned yet.
		RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
		if (foam_multimesh.is_valid()) {
			RenderingServer::get_singleton()->multimesh_set_visible_instances(foam_multimesh, 0);
		}
		return;
	}

	// The solver tags async-reaped impulses with the collider id AT SUBMIT
	// TIME: the ids must be on the colliders before the step.
	for (uint32_t i = 0; i < (uint32_t)col_src.size() && (int)i < (int)cols.size(); i++) {
		if (col_src[i].chunk_emitter != nullptr) {
			cols[i].id = _collider_id_for_chunk(col_src[i].chunk_emitter, col_src[i].chunk_id);
		} else if (col_src[i].node != nullptr) {
			cols[i].id = col_src[i].node->get_instance_id();
		}
	}

	LocalVector<MPMFluidSolver::ImpulseHit> impulses;
	// Granular runs async (visuals a frame late, GPU overlapped); fluid syncs so
	// its coupling reaction stays in phase with the rigid bodies.
	mpm->step(p_delta, cols, &impulses, want_surface, _is_granular());

	// PART-5 routing: async reaps return the PREVIOUS frame's impulses, tagged
	// with the collider id that earned them AT SUBMIT TIME. The old code
	// applied impulses[i] to col_src[i] -- this frame's list -- so a reshuffled
	// or resized collider set misrouted reactions between bodies and chunks.
	// Route by id through a source registry that remembers the last two
	// generations of colliders (async lag = 1 frame; two covers the swap).
	// Registry pruning: entries not refreshed within 64 frames drop.
	LocalVector<uint64_t> stale_ids;
	for (const KeyValue<uint64_t, MPMSourceEntry> &E : _mpm_source_registry) {
		if (_mpm_frame - E.value.last_seen > 64) {
			stale_ids.push_back(E.key);
		}
	}
	for (const uint64_t stale : stale_ids) {
		_mpm_source_registry.erase(stale); // erasing while iterating is UB
	}
	for (uint32_t i = 0; i < (uint32_t)col_src.size(); i++) {
		uint64_t id;
		if (col_src[i].chunk_emitter != nullptr) {
			id = _collider_id_for_chunk(col_src[i].chunk_emitter, col_src[i].chunk_id);
		} else if (col_src[i].node != nullptr) {
			id = col_src[i].node->get_instance_id();
		} else {
			continue;
		}
		MPMSourceEntry *entry_ptr = _mpm_source_registry.getptr(id);
		if (entry_ptr == nullptr) {
			_mpm_source_registry.insert(id, MPMSourceEntry());
			entry_ptr = _mpm_source_registry.getptr(id);
		}
		MPMSourceEntry &entry = *entry_ptr;
		entry.node = col_src[i].node;
		entry.chunk_emitter = col_src[i].chunk_emitter;
		entry.chunk_id = col_src[i].chunk_id;
		entry.extents = cols[i].extents;
		entry.last_seen = _mpm_frame;
	}
	_mpm_frame++;

	// Reaction: push the coupled bodies back with the fluid's impulse. Clamp to a
	// sane per-frame velocity change so a solver blow-up can't launch anything
	// across the level.
	for (uint32_t i = 0; i < impulses.size(); i++) {
		const MPMFluidSolver::ImpulseHit &hit = impulses[i];
		MPMSourceEntry *src = _mpm_source_registry.getptr(hit.collider_id);
		if (src == nullptr) {
			continue; // collider left the registry before its (lagged) impulse arrived
		}
		if (src->chunk_emitter != nullptr) {
			Vector3 imp = hit.impulse;
			const float m = MAX(src->extents.x * src->extents.y * src->extents.z * 8.0f * 1200.0f, 0.001f);
			const float cap = m * 20.0f;
			if (imp.length() > cap) {
				imp = imp.normalized() * cap;
			}
			src->chunk_emitter->apply_chunk_impulse(src->chunk_id, imp);
			continue;
		}
		RigidBody3D *rb = Object::cast_to<RigidBody3D>(src->node);
		if (rb == nullptr || rb->is_freeze_enabled()) {
			continue;
		}
		Vector3 imp = hit.impulse;
		// A liquid splash can legitimately shove a body hard; a granular bed can
		// only *support* one. Cap the granular reaction at a few g of holding
		// force (dt-scaled) so a body resting in sand is held up, not launched --
		// grid nodes stopped under a half-buried collider otherwise sum to a
		// large net-up impulse every step.
		const float cap = _is_granular()
				? rb->get_mass() * 35.0f * float(MAX(p_delta, 0.0))
				: rb->get_mass() * 20.0f;
		if (imp.length() > cap) {
			imp = imp.normalized() * cap;
		}
		if (_is_granular() && rb->get_mass() > 0.0) {
			const float vy = rb->get_linear_velocity().y;
			if (imp.y > 0.0f) {
				// The lagged grid state overshoots the support impulse, so a
				// body stood in sand bobs and floats up. The upward reaction
				// may only arrest the body sinking -- never push past zero,
				// nothing at all while it is already rising.
				imp.y = MIN(imp.y, MAX(0.0f, -vy) * rb->get_mass());
			} else if (vy > 0.0f) {
				// Body leaving the bed (a jump): grains cling to the capsule
				// flank and the couple pass's tangential damping would drag the
				// whole jump back down. Let the sand shave the climb, not kill
				// it -- at most a fifth of the upward speed per solve.
				imp.y = MAX(imp.y, -0.2f * vy * rb->get_mass());
			}
		}
		rb->apply_central_impulse(imp);
	}

	_update_mpm_foam();

	RenderingServer *rs = RenderingServer::get_singleton();

	if (want_surface) {
		// The solver marches the isosurface on the GPU; we get back a triangle
		// soup in world space and hand it to an ArrayMesh in this node's space.
		PackedVector3Array verts, normals;
		const int tris = mpm->get_surface_mesh(verts, normals);
		rs->mesh_clear(array_mesh);
		if (tris > 0) {
			const Transform3D inv = get_global_transform().affine_inverse();
			const Basis nb = inv.basis;
			Vector3 *vw = verts.ptrw();
			Vector3 *nw = normals.ptrw();
			for (int i = 0; i < verts.size(); i++) {
				vw[i] = inv.xform(vw[i]);
				nw[i] = nb.xform(nw[i]).normalized();
			}
			Array arrays;
			arrays.resize(RS::ARRAY_MAX);
			arrays[RS::ARRAY_VERTEX] = verts;
			arrays[RS::ARRAY_NORMAL] = normals;
			rs->mesh_add_surface_from_arrays(array_mesh, RS::PRIMITIVE_TRIANGLES, arrays);
			if (water_material.is_valid()) {
				rs->mesh_surface_set_material(array_mesh, 0, water_material->get_rid());
			}
		}
		rs->multimesh_set_visible_instances(multimesh, 0);
		return;
	}
	if (surface_mesh) {
		// Surface path, but this step's re-mesh was skipped for cost -- keep the
		// mesh from the last update and don't fall through to the sphere buffer.
		rs->multimesh_set_visible_instances(multimesh, 0);
		return;
	}

	// The solver packs world-space transforms; the MultiMesh is in node-local
	// space, so shift each instance origin by the node inverse. (Basis stays
	// identity -- the solver ignores domain rotation.)
	// PART-7: write through the solver's own cache (no copy-on-write detach --
	// the old by-value return copied ~2.9 MB per frame just to shift origins).
	PackedFloat32Array &buffer = mpm->get_multimesh_buffer_mut();
	const int n = mpm->get_particle_count();
	const int cap = mpm->get_capacity();
	if (buffer.size() < cap * 12) {
		return;
	}
	const Transform3D inv = get_global_transform().affine_inverse();
	float *b = buffer.ptrw();
	for (int i = 0; i < n; i++) {
		float *t = &b[i * 12];
		const Vector3 local = inv.xform(Vector3(t[3], t[7], t[11]));
		t[3] = local.x;
		t[7] = local.y;
		t[11] = local.z;
	}
	rs->multimesh_set_buffer(multimesh, buffer);
	rs->multimesh_set_visible_instances(multimesh, n);
}

// Push the Vulkan diffuse layer's cached records into the foam sprite
// MultiMesh (world space, same consumption pattern as the PBD path's foam).
// Each record is x,y,z,kind (0 = spray, 1 = foam, 2 = bubble); the kind scales
// the sprite -- spray droplets smaller, bubbles larger. Sprites stay visible
// alongside the MPM isosurface -- there is no MPM foam isosurface layer yet.
void PhysXParticleFluid3D::_update_mpm_foam() {
	if (foam_multimesh.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();
	if (!foam_enabled || mpm == nullptr || !mpm->is_available()) {
		rs->multimesh_set_visible_instances(foam_multimesh, 0);
		return;
	}
	const PackedFloat32Array &foam = mpm->get_foam_data();
	const int fn = MIN(foam.size() / 4, foam_particle_count);
	if (foam_buffer_scratch.size() != foam_particle_count * 12) {
		foam_buffer_scratch.resize(foam_particle_count * 12);
	}
	// PART-7: fill the scratch IN PLACE -- the old by-value copy detached
	// foam_particle_count * 12 floats every tick.
	PackedFloat32Array &foam_buffer = foam_buffer_scratch;
	float *fb = foam_buffer.ptrw();
	const float *fp = foam.ptr();
	static const float kind_scale[3] = { 0.6f, 1.0f, 1.4f };
	for (int i = 0; i < fn; i++) {
		const float *rec = fp + i * 4;
		const float s = kind_scale[CLAMP((int)rec[3], 0, 2)];
		float *t = &fb[i * 12];
		t[0] = s;
		t[1] = 0;
		t[2] = 0;
		t[3] = rec[0];
		t[4] = 0;
		t[5] = s;
		t[6] = 0;
		t[7] = rec[1];
		t[8] = 0;
		t[9] = 0;
		t[10] = s;
		t[11] = rec[2];
	}
	rs->multimesh_set_buffer(foam_multimesh, foam_buffer);
	rs->multimesh_set_visible_instances(foam_multimesh, fn);
}

void PhysXParticleFluid3D::_make_fluid() {
	if (multimesh.is_valid()) {
		return;
	}

	// Freeze the backend for this fluid's lifetime; runtime setting changes
	// re-resolve only on the next free + spawn cycle.
	resolved = _resolved_solver();

	if (resolved == SOLVER_MPM) {
		Ref<SphereMesh> sphere;
		sphere.instantiate();
		sphere->set_radius(particle_size * 0.5);
		sphere->set_height(particle_size);
		sphere->set_radial_segments(6);
		sphere->set_rings(3);
		{
			// The particle spheres get their own opaque material; water_material is
			// left for the isosurface path below (a shared handle shadowed it).
			Ref<StandardMaterial3D> pm;
			pm.instantiate();
			pm->set_albedo(Color(0.16, 0.44, 0.66));
			pm->set_metallic(0.0);
			pm->set_roughness(0.25);
			sphere->set_material(pm);
		}
		particle_mesh = sphere;

		RenderingServer *rs = RenderingServer::get_singleton();
		multimesh = rs->multimesh_create();
		rs->multimesh_allocate_data(multimesh, particle_count, RS::MULTIMESH_TRANSFORM_3D);
		rs->multimesh_set_mesh(multimesh, particle_mesh->get_rid());
		rs->multimesh_set_visible_instances(multimesh, 0);
		set_base(multimesh);

		if (surface_mesh) {
			// Marching-tetrahedra mesh over the solver's density grid, drawn with a
			// translucent water material. The particle MultiMesh stays hidden.
			array_mesh = rs->mesh_create();
			if (water_material.is_null()) {
				Ref<StandardMaterial3D> m;
				m.instantiate();
				m->set_albedo(Color(0.12, 0.42, 0.62, 0.6));
				m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
				m->set_metallic(0.1);
				m->set_roughness(0.06);
				m->set_feature(BaseMaterial3D::FEATURE_REFRACTION, true);
				m->set_refraction(0.05);
				// Marching-tetrahedra output is not guaranteed watertight; render
				// both sides so a stray inward-facing triangle does not read as a
				// hole, and so the surface is still there when viewed from under.
				m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
				water_material = m;
			}
			set_base(array_mesh);
		}

		// Foam/spray sprites: same world-space MultiMesh pattern as the PBD
		// branch -- the Vulkan diffuse layer feeds it from the solver readback.
		if (foam_enabled && get_world_3d().is_valid()) {
			Ref<SphereMesh> foam_sphere;
			foam_sphere.instantiate();
			foam_sphere->set_radius(_effective_foam_size() * 0.5);
			foam_sphere->set_height(_effective_foam_size());
			foam_sphere->set_radial_segments(5);
			foam_sphere->set_rings(2);
			Ref<StandardMaterial3D> foam_material;
			foam_material.instantiate();
			foam_material->set_albedo(Color(1, 1, 1, 0.85));
			foam_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
			foam_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			foam_sphere->set_material(foam_material);
			foam_mesh = foam_sphere;

			foam_multimesh = rs->multimesh_create();
			rs->multimesh_allocate_data(foam_multimesh, foam_particle_count, RS::MULTIMESH_TRANSFORM_3D);
			rs->multimesh_set_mesh(foam_multimesh, foam_mesh->get_rid());
			rs->multimesh_set_visible_instances(foam_multimesh, 0);
			foam_instance = rs->instance_create2(foam_multimesh, get_world_3d()->get_scenario());
			rs->instance_set_transform(foam_instance, Transform3D());
			rs->instance_set_custom_aabb(foam_instance, AABB(Vector3(-100000, -100000, -100000), Vector3(200000, 200000, 200000)));
		}

		mpm = memnew(MPMFluidSolver);
		if (!mpm->has_device()) {
			WARN_PRINT_ONCE("PhysXParticleFluid3D: the MPM compute solver could not start (no RenderingDevice / compute support).");
		}
		return;
	}

	if (fluid.is_valid()) {
		return;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server) {
		WARN_PRINT_ONCE("PhysXParticleFluid3D does nothing unless the 3D physics engine is set to \"PhysX\".");
		return;
	}
	ERR_FAIL_NULL(get_world_3d().ptr());

	fluid = server->particle_fluid_create();
	server->particle_fluid_set_space(fluid, get_world_3d()->get_space());
	server->particle_fluid_set_capacity(fluid, particle_count);
	const bool pbd_granular = _is_granular();
	server->particle_fluid_set_granular(fluid, pbd_granular, 2.0f * Math::tan(Math::deg_to_rad(CLAMP(_granular_friction_deg(), 1.0f, 55.0f))));
	_apply_params();

	// Render the particles as a MultiMesh of small spheres.
	Ref<SphereMesh> sphere;
	sphere.instantiate();
	sphere->set_radius(particle_size * 0.5);
	sphere->set_height(particle_size);
	sphere->set_radial_segments(6);
	sphere->set_rings(3);
	particle_mesh = sphere;

	RenderingServer *rs = RenderingServer::get_singleton();
	multimesh = rs->multimesh_create();
	rs->multimesh_allocate_data(multimesh, particle_count, RS::MULTIMESH_TRANSFORM_3D);
	rs->multimesh_set_mesh(multimesh, particle_mesh->get_rid());
	rs->multimesh_set_visible_instances(multimesh, 0);

	set_base(multimesh);

	// Foam/spray/bubble particles: a second MultiMesh in its own world-space
	// instance (particle positions come back in world space). Used only when the
	// isosurface path is off -- otherwise foam is drawn as its own coarse
	// isosurface layer (foam_array_mesh) so it depth-sorts with the water.
	Ref<SphereMesh> foam_sphere;
	foam_sphere.instantiate();
	foam_sphere->set_radius(_effective_foam_size() * 0.5);
	foam_sphere->set_height(_effective_foam_size());
	foam_sphere->set_radial_segments(5);
	foam_sphere->set_rings(2);
	foam_mesh = foam_sphere;

	Ref<StandardMaterial3D> foam_material;
	foam_material.instantiate();
	foam_material->set_albedo(Color(1, 1, 1, 0.85));
	foam_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	foam_material->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
	foam_sphere->set_material(foam_material);

	foam_multimesh = rs->multimesh_create();
	rs->multimesh_allocate_data(foam_multimesh, foam_particle_count, RS::MULTIMESH_TRANSFORM_3D);
	rs->multimesh_set_mesh(foam_multimesh, foam_mesh->get_rid());
	rs->multimesh_set_visible_instances(foam_multimesh, 0);

	foam_instance = rs->instance_create2(foam_multimesh, get_world_3d()->get_scenario());
	rs->instance_set_transform(foam_instance, Transform3D());
	// World-space instance that never moves; keep it from being culled.
	rs->instance_set_custom_aabb(foam_instance, AABB(Vector3(-100000, -100000, -100000), Vector3(200000, 200000, 200000)));

	_apply_foam();

	// GPU isosurface mesh path: PhysX marching-cubes a triangle mesh we draw as
	// an ArrayMesh with an ordinary water material. Granular grains render as the
	// sphere MultiMesh -- no surface.
	const bool pbd_surface = surface_mesh && !pbd_granular;
	server->particle_fluid_set_surface_mesh(fluid, pbd_surface);
	server->particle_fluid_set_surface_anisotropy(fluid, surface_anisotropy && !pbd_granular);
	if (pbd_surface) {
		array_mesh = rs->mesh_create();
		if (water_material.is_null()) {
			Ref<StandardMaterial3D> m;
			m.instantiate();
			m->set_albedo(Color(0.12, 0.42, 0.62, 0.6));
			m->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			m->set_metallic(0.1);
			m->set_roughness(0.06);
			m->set_feature(BaseMaterial3D::FEATURE_REFRACTION, true);
			m->set_refraction(0.05);
			// Thin fluid features (a stream, a spreading splash) mesh as a thin
			// shell; render both sides so they do not read as a hollow front face.
			m->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
			water_material = m;
		}
		set_base(array_mesh);

		// Foam layer: a coarser isosurface over the diffuse particles, drawn as a
		// whiter, rougher, less transparent skin sitting on the water. Its own
		// world-space instance (the mesh comes back in world space).
		foam_array_mesh = rs->mesh_create();
		if (foam_water_material.is_null()) {
			Ref<StandardMaterial3D> fm;
			fm.instantiate();
			fm->set_albedo(Color(0.95, 0.97, 1.0, 0.85));
			fm->set_transparency(BaseMaterial3D::TRANSPARENCY_ALPHA);
			fm->set_metallic(0.0);
			fm->set_roughness(0.9);
			fm->set_feature(BaseMaterial3D::FEATURE_REFRACTION, true);
			fm->set_refraction(0.02);
			foam_water_material = fm;
		}
		foam_mesh_instance = rs->instance_create2(foam_array_mesh, get_world_3d()->get_scenario());
		rs->instance_set_transform(foam_mesh_instance, Transform3D());
		rs->instance_set_custom_aabb(foam_mesh_instance, AABB(Vector3(-100000, -100000, -100000), Vector3(200000, 200000, 200000)));
	}
}

void PhysXParticleFluid3D::_apply_foam() {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}
	server->particle_fluid_set_foam(fluid, foam_enabled, foam_particle_count, foam_lifetime, foam_threshold, foam_buoyancy, _effective_foam_size());
}

void PhysXParticleFluid3D::_free_fluid() {
	if (mpm != nullptr) {
		memdelete(mpm);
		mpm = nullptr;
	}
	_mpm_configured = false;
	_mpm_emit_mode = false;
	emit_accum = 0.0;
	resolved = SOLVER_AUTO; // backend re-resolves on the next spawn

	if (_mpm_query_shape.is_valid()) {
		PhysicsServer3D::get_singleton()->free(_mpm_query_shape);
		_mpm_query_shape = RID();
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	if (multimesh.is_valid()) {
		set_base(RID());
		rs->free(multimesh);
		multimesh = RID();
	}
	particle_mesh.unref();

	if (foam_instance.is_valid()) {
		rs->free(foam_instance);
		foam_instance = RID();
	}
	if (foam_multimesh.is_valid()) {
		rs->free(foam_multimesh);
		foam_multimesh = RID();
	}
	foam_mesh.unref();

	if (foam_mesh_instance.is_valid()) {
		rs->free(foam_mesh_instance);
		foam_mesh_instance = RID();
	}
	if (foam_array_mesh.is_valid()) {
		rs->free(foam_array_mesh);
		foam_array_mesh = RID();
	}
	foam_water_material.unref();

	if (array_mesh.is_valid()) {
		rs->free(array_mesh);
		array_mesh = RID();
	}
	water_material.unref();

	if (fluid.is_valid()) {
		PhysXServer3D *server = PhysXServer3D::get_singleton();
		if (server) {
			server->free(fluid);
		}
		fluid = RID();
	}
	spawned = false;
}

void PhysXParticleFluid3D::_apply_params() {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_PARTICLE_SIZE, particle_size);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_VISCOSITY, viscosity);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_SURFACE_TENSION, surface_tension);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_COHESION, cohesion);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_VORTICITY, vorticity);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_ADHESION, adhesion);
	server->particle_fluid_set_param(fluid, PhysXGPUParticleFluid3D::PARAM_GRAVITY_SCALE, gravity_scale);
}

void PhysXParticleFluid3D::spawn() {
	_make_fluid();

	if (_mpm_path()) {
		_mpm_configure();
		// Not is_available(): with a separate render thread the build hasn't
		// run yet, and the per-step guards cover the warm-up.
		spawned = _mpm_configured;
		return;
	}

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}

	// Jittered grid filling spawn_region_size, centered on the node. y is the
	// outer loop so a fill that runs out of particles before the region is full
	// spreads across the whole footprint as a shallow bed -- matching the MPM
	// prefill (_seed_block) instead of banking a slab against one wall.
	const Vector3 half = spawn_region_size * 0.5;
	const float spacing = MAX(particle_size, 0.001f);
	// 64-bit cell math: a large region over a small spacing would overflow the
	// int32 product (a 10 m region at 1 mm spacing is 10^12 cells), producing a
	// negative resize count. Per-axis counts are clamped to particle_count --
	// the fill never emits more than that anyway -- and the product is taken
	// before the MIN in 64 bits.
	const int64_t cx = CLAMP(int64_t(spawn_region_size.x / spacing), int64_t(1), int64_t(particle_count));
	const int64_t cy = CLAMP(int64_t(spawn_region_size.y / spacing), int64_t(1), int64_t(particle_count));
	const int64_t cz = CLAMP(int64_t(spawn_region_size.z / spacing), int64_t(1), int64_t(particle_count));
	const int cap = (int)MIN((int64_t)particle_count, cx * cy * cz);
	const Transform3D xf = get_global_transform();

	Vector<Vector3> positions;
	positions.resize(MAX(cap, 0));
	Vector3 *w = positions.ptrw();
	int n = 0;
	const int64_t limit = cap;
	const float jitter = spacing * 0.2;
	for (int64_t iy = 0; iy < cy && n < limit; iy++) {
		for (int64_t iz = 0; iz < cz && n < limit; iz++) {
			for (int64_t ix = 0; ix < cx && n < limit; ix++) {
				Vector3 local(
						-half.x + (ix + 0.5f) * spacing + Math::randf() * jitter,
						-half.y + (iy + 0.5f) * spacing + Math::randf() * jitter,
						-half.z + (iz + 0.5f) * spacing + Math::randf() * jitter);
				w[n++] = xf.xform(local);
			}
		}
	}
	positions.resize(n);

	server->particle_fluid_set_particles(fluid, positions, Vector3());
	spawned = true;
	_update_render();
}

void PhysXParticleFluid3D::_emit_step(double p_delta) {
	_make_fluid();
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}
	emit_accum += emission_rate * p_delta;
	const int count = (int)emit_accum;
	if (count <= 0) {
		return;
	}
	emit_accum -= count;

	const Transform3D xf = get_global_transform();
	const Vector3 origin = xf.origin;
	const Vector3 world_vel = xf.basis.xform(emission_velocity);

	Vector<Vector3> positions;
	positions.resize(count);
	Vector3 *w = positions.ptrw();
	for (int i = 0; i < count; i++) {
		// A jittered point in the emission sphere, nudged along the flow so a
		// whole batch does not spawn on top of itself.
		const Vector3 j(
				Math::randf() * 2.0f - 1.0f,
				Math::randf() * 2.0f - 1.0f,
				Math::randf() * 2.0f - 1.0f);
		w[i] = origin + j * emission_radius + world_vel * (0.004f * (float)i);
	}
	server->particle_fluid_emit(fluid, positions, world_vel);
}

void PhysXParticleFluid3D::_mpm_emit_step(double p_delta) {
	_make_fluid();
	if (mpm == nullptr || !mpm->has_device()) {
		return;
	}
	if (!_mpm_configured) {
		_mpm_configure(false); // empty buffer, fills from the stream
		spawned = _mpm_configured;
	}
	if (!_mpm_configured) {
		return;
	}

	emit_accum += emission_rate * p_delta;
	const int count = (int)emit_accum;
	if (count <= 0) {
		return;
	}
	emit_accum -= count;

	const Transform3D xf = get_global_transform();
	const Vector3 world_vel = xf.basis.xform(emission_velocity);

	LocalVector<MPMFluidSolver::EmittedParticle> batch;
	batch.resize(count);
	for (int i = 0; i < count; i++) {
		const Vector3 j(
				Math::randf() * 2.0f - 1.0f,
				Math::randf() * 2.0f - 1.0f,
				Math::randf() * 2.0f - 1.0f);
		batch[i].position = xf.origin + j * emission_radius + world_vel * (0.004f * (float)i);
		batch[i].velocity = world_vel;
	}
	mpm->emit(batch);
}

bool PhysXParticleFluid3D::is_mpm_available() const {
	return mpm != nullptr && mpm->is_available();
}

void PhysXParticleFluid3D::clear() {
	if (_mpm_path()) {
		if (multimesh.is_valid()) {
			RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
		}
		if (foam_multimesh.is_valid()) {
			// PART-8: the MPM foam layer kept rendering its last frame after a
			// clear -- _update_mpm_foam stops running once spawned=false, so
			// nothing ever hid it.
			RenderingServer::get_singleton()->multimesh_set_visible_instances(foam_multimesh, 0);
		}
		spawned = false;
		_mpm_configured = false; // next spawn()/emit rebuilds the buffer
		emit_accum = 0.0;
		return;
	}

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (server && fluid.is_valid()) {
		server->particle_fluid_clear(fluid);
	}
	if (multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_visible_instances(multimesh, 0);
	}
	if (foam_multimesh.is_valid()) {
		RenderingServer::get_singleton()->multimesh_set_visible_instances(foam_multimesh, 0);
	}
	spawned = false;
}

int PhysXParticleFluid3D::get_live_particle_count() const {
	if (mpm != nullptr) {
		return spawned ? mpm->get_particle_count() : 0;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return 0;
	}
	return server->particle_fluid_get_particle_count(fluid);
}

double PhysXParticleFluid3D::get_mpm_step_msec() const {
	return mpm != nullptr ? mpm->get_last_step_msec() : 0.0;
}

PackedVector3Array PhysXParticleFluid3D::get_particle_positions() const {
	if (mpm != nullptr) {
		return mpm->get_positions();
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return PackedVector3Array();
	}
	return server->particle_fluid_get_positions(fluid);
}

float PhysXParticleFluid3D::get_submersion(const AABB &p_world_aabb) const {
	if (_mpm_path()) {
		// Vulkan path: fraction of cached particle positions inside the query
		// volume, weighted by cell volume -- same semantic as the PBD call.
		return mpm != nullptr ? mpm->get_submersion(p_world_aabb) : 0.0f;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return 0.0f;
	}
	return server->particle_fluid_get_submersion(fluid, p_world_aabb);
}

void PhysXParticleFluid3D::_update_render() {
	if (multimesh.is_null()) {
		return;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}
	RenderingServer *rs = RenderingServer::get_singleton();

	if (surface_mesh && !_is_granular()) {
		// The isosurface mesh draws the fluid -- do not read back every particle
		// position or rebuild the sphere buffer. (Foam is still updated below.)
		rs->multimesh_set_visible_instances(multimesh, 0);
	} else {
		const Vector<Vector3> positions = server->particle_fluid_get_positions(fluid);
		const int n = MIN(positions.size(), particle_count);

		// MultiMesh is in this node's local space; particle positions are world.
		const Transform3D inv = get_global_transform().affine_inverse();

		PackedFloat32Array buffer;
		buffer.resize(particle_count * 12);
		float *b = buffer.ptrw();
		const Vector3 *p = positions.ptr();
		for (int i = 0; i < n; i++) {
			const Vector3 local = inv.xform(p[i]);
			float *t = &b[i * 12];
			t[0] = 1;
			t[1] = 0;
			t[2] = 0;
			t[3] = local.x;
			t[4] = 0;
			t[5] = 1;
			t[6] = 0;
			t[7] = local.y;
			t[8] = 0;
			t[9] = 0;
			t[10] = 1;
			t[11] = local.z;
		}
		rs->multimesh_set_buffer(multimesh, buffer);
		rs->multimesh_set_visible_instances(multimesh, n);
	}

	if (foam_multimesh.is_null()) {
		return;
	}
	// With the isosurface path on, foam is drawn as foam_array_mesh instead.
	if (!foam_enabled || surface_mesh) {
		rs->multimesh_set_visible_instances(foam_multimesh, 0);
		return;
	}
	// Foam instance is world-space, so write particle world positions directly.
	const Vector<Vector3> foam = server->particle_fluid_get_foam_positions(fluid);
	const int fn = MIN(foam.size(), foam_particle_count);
	PackedFloat32Array foam_buffer;
	foam_buffer.resize(foam_particle_count * 12);
	float *fb = foam_buffer.ptrw();
	const Vector3 *fp = foam.ptr();
	for (int i = 0; i < fn; i++) {
		float *t = &fb[i * 12];
		t[0] = 1;
		t[1] = 0;
		t[2] = 0;
		t[3] = fp[i].x;
		t[4] = 0;
		t[5] = 1;
		t[6] = 0;
		t[7] = fp[i].y;
		t[8] = 0;
		t[9] = 0;
		t[10] = 1;
		t[11] = fp[i].z;
	}
	rs->multimesh_set_buffer(foam_multimesh, foam_buffer);
	rs->multimesh_set_visible_instances(foam_multimesh, fn);
}

void PhysXParticleFluid3D::_update_surface_mesh() {
	if (!surface_mesh || array_mesh.is_null()) {
		return;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return;
	}

	{
		PackedVector3Array verts, normals;
		PackedInt32Array indices;
		const int tris = server->particle_fluid_get_surface_mesh(fluid, verts, normals, indices, surface_mesh_version);
		if (tris >= 0) {
			// Keep only the largest connected component: stray marching-cubes
			// debris (a blade, a floating blob) is dropped.
			_commit_iso_mesh(array_mesh, verts, normals, indices, water_material, true, true, particle_size);
		}
	}
	if (foam_mesh_instance.is_valid()) {
		PackedVector3Array verts, normals;
		PackedInt32Array indices;
		const int tris = server->particle_fluid_get_foam_mesh(fluid, verts, normals, indices, foam_surface_mesh_version);
		if (tris >= 0) {
			// Foam is naturally many disconnected clumps -- keep them all.
			_commit_iso_mesh(foam_array_mesh, verts, normals, indices, foam_water_material, false, false, _effective_foam_size());
		}
	}
}

void PhysXParticleFluid3D::_commit_iso_mesh(RID p_mesh, PackedVector3Array &verts, PackedVector3Array &normals, PackedInt32Array &indices, const Ref<Material> &p_material, bool p_to_local, bool p_keep_largest_component, float p_feature_size) {
	RenderingServer *rs = RenderingServer::get_singleton();
	rs->mesh_clear(p_mesh);
	if (verts.is_empty() || indices.is_empty()) {
		return;
	}

	if (p_to_local) {
		// Isosurface vertices come back in world space; the node's instance
		// carries its own transform, so bring them into local space.
		const Transform3D inv = get_global_transform().affine_inverse();
		const Basis nb = inv.basis;
		Vector3 *vw = verts.ptrw();
		Vector3 *nw = normals.ptrw();
		for (int i = 0; i < verts.size(); i++) {
			vw[i] = inv.xform(vw[i]);
			nw[i] = nb.xform(nw[i]).normalized();
		}
	}

	// Drop any triangle stretched well past a grid cell (a stray particle
	// smeared across the level). When p_keep_largest_component, also keep only
	// the biggest connected triangle island (union-find over the index buffer).
	const float max_edge_sq = (p_feature_size * 4.0f) * (p_feature_size * 4.0f);
	AABB local_aabb;
	bool have_aabb = false;
	{
		const int *ip = indices.ptr();
		const Vector3 *vp = verts.ptr();
		const int vcount = verts.size();

		LocalVector<int> parent;
		parent.resize(vcount);
		for (int i = 0; i < vcount; i++) {
			parent[i] = i;
		}
		// Iterative find with path halving.
		auto find = [&](int x) {
			while (parent[x] != x) {
				parent[x] = parent[parent[x]];
				x = parent[x];
			}
			return x;
		};

		PackedInt32Array unstretched;
		unstretched.resize(indices.size());
		int u = 0;
		for (int t = 0; t + 2 < indices.size(); t += 3) {
			const int ia = ip[t], ib = ip[t + 1], ic = ip[t + 2];
			const Vector3 &a = vp[ia];
			const Vector3 &b = vp[ib];
			const Vector3 &c = vp[ic];
			if (a.distance_squared_to(b) > max_edge_sq || b.distance_squared_to(c) > max_edge_sq || c.distance_squared_to(a) > max_edge_sq) {
				continue;
			}
			unstretched.write[u++] = ia;
			unstretched.write[u++] = ib;
			unstretched.write[u++] = ic;
			const int ra = find(ia), rb = find(ib), rc = find(ic);
			parent[rb] = ra;
			parent[find(ic)] = find(ia);
			(void)rc;
		}
		unstretched.resize(u);

		// Tally triangles per connected component. When p_keep_largest_component,
		// drop only genuine speck debris -- a component is kept if it has at least
		// a small fraction of the biggest one's triangles (or a hard floor). This
		// is deliberately not winner-take-all: a scene where the fluid splits into
		// two similar blobs would otherwise flicker as "the largest" flips.
		HashMap<int, int> comp_tris;
		int keep_min = 0;
		int best_root = -1;
		if (p_keep_largest_component) {
			int best_count = 0;
			for (int t = 0; t + 2 < u; t += 3) {
				const int r = find(unstretched[t]);
				const int cnt = (comp_tris[r] += 1);
				if (cnt > best_count) {
					best_count = cnt;
					best_root = r;
				}
			}
			keep_min = MAX(24, (int)(best_count * 0.05f)); // others must clear this; the biggest is always kept
		}

		PackedInt32Array kept;
		kept.resize(u);
		int k = 0;
		const int *up = unstretched.ptr();
		for (int t = 0; t + 2 < u; t += 3) {
			if (p_keep_largest_component) {
				const int r = find(up[t]);
				if (r != best_root && comp_tris[r] < keep_min) {
					continue;
				}
			}
			for (int j = 0; j < 3; j++) {
				const Vector3 &p = vp[up[t + j]];
				kept.write[k++] = up[t + j];
				if (!have_aabb) {
					local_aabb = AABB(p, Vector3());
					have_aabb = true;
				}
				local_aabb.expand_to(p);
			}
		}
		kept.resize(k);
		indices = kept;
	}
	if (indices.is_empty()) {
		return;
	}

	Array arrays;
	arrays.resize(RS::ARRAY_MAX);
	arrays[RS::ARRAY_VERTEX] = verts;
	arrays[RS::ARRAY_NORMAL] = normals;
	arrays[RS::ARRAY_INDEX] = indices;
	rs->mesh_add_surface_from_arrays(p_mesh, RS::PRIMITIVE_TRIANGLES, arrays);
	if (p_material.is_valid()) {
		rs->mesh_surface_set_material(p_mesh, 0, p_material->get_rid());
	}
	rs->mesh_set_custom_aabb(p_mesh, local_aabb.grow(p_feature_size * 4.0f));
}

void PhysXParticleFluid3D::_notification(int p_what) {
	const bool editor = Engine::get_singleton()->is_editor_hint();
	switch (p_what) {
		case NOTIFICATION_ENTER_WORLD: {
			if (editor) {
				_editor_preview_enter();
				set_process_internal(true);
			} else {
				if (spawn_on_ready) {
					spawn();
				}
				// PBD is a physics actor (physics tick); the MPM compute solver is
				// a visual effect and runs per rendered frame with its own
				// substepping, like GPUParticles3D -- stepping it on the fixed
				// physics tick means physics catch-up runs it several times per
				// frame under load, which spirals.
				set_physics_process_internal(true);
				set_process_internal(true);
			}
		} break;
		case NOTIFICATION_EXIT_WORLD: {
			set_physics_process_internal(false);
			set_process_internal(false);
			_editor_preview_exit();
			_free_fluid();
		} break;
		case NOTIFICATION_PREDELETE: {
			// Belt and braces: drop the MPM solver (and its local RenderingDevice)
			// while the rendering subsystem is still alive, in case EXIT_WORLD did
			// not run (e.g. freed without ever being parented).
			_free_fluid();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (editor) {
				_editor_preview_step(get_process_delta_time());
				break;
			}
			// Granular MPM is a pure visual effect at high particle counts, so it
			// runs frame-rate driven (like GPUParticles3D) with its own
			// substepping -- stepping it on the fixed physics tick means physics
			// catch-up re-solves it several times per frame under load. The
			// fluid MPM path stays physics-tick driven so its rigid coupling
			// reaction stays in phase with the bodies it pushes.
			if (_mpm_path() && _is_granular()) {
				_mpm_accum += MIN(get_process_delta_time(), 1.0 / 20.0);
				if (_mpm_accum >= 1.0 / 60.0) {
					// Cap the solved step at ~2 physics ticks -- a stiff granular
					// plasticity solve blows up if a hitch feeds it a big dt.
					const double fdt = MIN(_mpm_accum, 1.0 / 30.0);
					_mpm_accum = 0.0;
					if (emitting) {
						_mpm_emit_step(fdt);
					}
					if (spawned) {
						_mpm_step(fdt);
					}
				}
			}
		} break;
		case NOTIFICATION_INTERNAL_PHYSICS_PROCESS: {
			const bool mpm_here = _mpm_path() && !_is_granular();
			const double pdt = get_physics_process_delta_time();
			if (emitting) {
				if (mpm_here) {
					_mpm_emit_step(pdt);
				} else if (!_mpm_path()) {
					_emit_step(pdt);
				}
			}
			if (mpm_here) {
				if (spawned) {
					_mpm_step(pdt);
				}
			} else if (!_mpm_path() && (spawned || emitting)) {
				_update_render();
				_update_surface_mesh();
			}
		} break;
	}
}

void PhysXParticleFluid3D::_editor_preview_enter() {
	RenderingServer *rs = RenderingServer::get_singleton();

	Ref<SphereMesh> s;
	s.instantiate();
	s->set_radius(particle_size * 0.5);
	s->set_height(particle_size);
	s->set_radial_segments(6);
	s->set_rings(3);
	Ref<StandardMaterial3D> m;
	m.instantiate();
	m->set_albedo(Color(0.3, 0.6, 1.0));
	m->set_shading_mode(BaseMaterial3D::SHADING_MODE_PER_PIXEL);
	s->set_material(m);
	preview_mesh = s;

	const int cap = 400;
	preview_pos.resize(cap);
	preview_vel.resize(cap);
	preview_age.resize(cap);
	for (int i = 0; i < cap; i++) {
		preview_age[i] = Math::randf() * 4.0f; // staggered so they don't all spawn at once
		preview_pos[i] = Vector3();
		preview_vel[i] = Vector3();
	}

	preview_multimesh = rs->multimesh_create();
	rs->multimesh_allocate_data(preview_multimesh, cap, RS::MULTIMESH_TRANSFORM_3D);
	rs->multimesh_set_mesh(preview_multimesh, preview_mesh->get_rid());
	rs->multimesh_set_visible_instances(preview_multimesh, 0);
	set_base(preview_multimesh);
	// GeometryInstance3D::set_gi_mode() already applied INSTANCE_FLAG_USE_
	// DYNAMIC_GI to this node's own instance (the one preview_multimesh just
	// became the base of) the moment the scene loaded, regardless of editor
	// vs runtime. Voxelizing a handful of small drifting preview spheres is
	// what actually triggers VoxelGI's dynamic-object pass to produce a
	// negative-size Rect2i and a garbage compute-dispatch count (reproduced
	// and confirmed upstream on this shared code lineage by isolating the
	// node's gi_mode in an otherwise-empty editor session; the guard was
	// lost in the port here — R-9/VoxelGI regression check) — force it back
	// off while the preview is the base, since a fake preview has nothing
	// real to contribute to GI anyway.
	rs->instance_geometry_set_flag(get_instance(), RS::INSTANCE_FLAG_USE_DYNAMIC_GI, false);
}

void PhysXParticleFluid3D::_editor_preview_exit() {
	if (preview_multimesh.is_valid()) {
		set_base(RID());
		RenderingServer::get_singleton()->free(preview_multimesh);
		preview_multimesh = RID();
	}
	preview_mesh.unref();
	preview_pos.clear();
	preview_vel.clear();
	preview_age.clear();
}

void PhysXParticleFluid3D::_editor_preview_step(double p_delta) {
	if (preview_multimesh.is_null()) {
		return;
	}
	// Throttle to ~24 Hz. A node that processes every frame forces the editor to
	// redraw its viewport continuously (it normally idles); this keeps that cost
	// low, and the preview is only a rough hint anyway.
	preview_throttle += p_delta;
	if (preview_throttle < 1.0 / 24.0) {
		return;
	}
	p_delta = preview_throttle;
	preview_throttle = 0.0;

	const float dt = MIN((float)p_delta, 0.08f);
	const float lifetime = 2.5f;
	const int cap = preview_pos.size();

	// Trickle a steady rate of respawns when emitting; a gentle bubble otherwise.
	const bool jet = emitting && !emission_velocity.is_zero_approx();
	preview_accum += (jet ? 220.0 : 40.0) * p_delta;
	int budget = (int)preview_accum;
	preview_accum -= budget;

	PackedFloat32Array buffer;
	buffer.resize(cap * 12);
	float *b = buffer.ptrw();
	int visible = 0;

	for (int i = 0; i < cap; i++) {
		preview_age[i] += dt;
		bool respawn = preview_age[i] > lifetime || preview_pos[i].y < -spawn_region_size.y * 2.0f - 2.0f;
		if (respawn && budget > 0) {
			budget--;
			preview_age[i] = 0.0f;
			const Vector3 j(Math::randf() * 2 - 1, Math::randf() * 2 - 1, Math::randf() * 2 - 1);
			if (jet) {
				preview_pos[i] = j * emission_radius;
				preview_vel[i] = emission_velocity + j * (emission_velocity.length() * 0.12f);
			} else {
				preview_pos[i] = Vector3(j.x, j.y, j.z) * (spawn_region_size * 0.5f);
				preview_vel[i] = Vector3();
			}
		} else if (respawn) {
			preview_age[i] = lifetime + 1.0f; // park it until there is budget
		}

		if (preview_age[i] <= lifetime) {
			preview_vel[i].y -= 9.8f * dt;
			preview_pos[i] += preview_vel[i] * dt;
			const Vector3 &p = preview_pos[i];
			float *t = &b[visible * 12];
			t[0] = 1;
			t[1] = 0;
			t[2] = 0;
			t[3] = p.x;
			t[4] = 0;
			t[5] = 1;
			t[6] = 0;
			t[7] = p.y;
			t[8] = 0;
			t[9] = 0;
			t[10] = 1;
			t[11] = p.z;
			visible++;
		}
	}

	RenderingServer *rs = RenderingServer::get_singleton();
	rs->multimesh_set_buffer(preview_multimesh, buffer);
	rs->multimesh_set_visible_instances(preview_multimesh, visible);
}

void PhysXParticleFluid3D::set_solver(SolverBackend p_solver) {
	if (solver == p_solver) {
		return;
	}
	const bool was_running = spawned || emitting;
	solver = p_solver;
	if (multimesh.is_valid() || fluid.is_valid()) {
		_free_fluid();
		if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
			if (was_running && spawn_on_ready) {
				spawn();
			}
		}
	}
	update_configuration_warnings();
	notify_property_list_changed();
}

void PhysXParticleFluid3D::set_mpm_domain_size(const Vector3 &p_size) {
	mpm_domain_size = p_size.maxf(0.1);
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	}
	update_gizmos();
}

void PhysXParticleFluid3D::set_mpm_grid_resolution(int p_res) {
	mpm_grid_resolution = p_res <= 0 ? 0 : CLAMP(p_res, 8, 128); // 0 = auto from particle_size
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	}
}

void PhysXParticleFluid3D::set_mpm_substeps(int p_substeps) {
	mpm_substeps = CLAMP(p_substeps, 1, 40);
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	}
}

void PhysXParticleFluid3D::set_mpm_stiffness(float p_stiffness) {
	mpm_stiffness = MAX(p_stiffness, 1.0f);
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	}
}

void PhysXParticleFluid3D::set_mpm_colliders(const TypedArray<NodePath> &p_colliders) {
	mpm_colliders = p_colliders;
	_mpm_prev_pos.clear();
}

void PhysXParticleFluid3D::set_particle_count(int p_count) {
	const int new_count = MAX(p_count, 1);
	if (particle_count == new_count) {
		return;
	}
	particle_count = new_count;
	update_configuration_warnings();
	// The PBD path sizes its MultiMesh and the server-side particle buffer from
	// particle_count at creation. A live count change (e.g. a demo's amount
	// keys) would otherwise make _update_render() write a buffer larger than
	// the MultiMesh allocation ("p_buffer.size() != instances * stride"), so
	// rebuild the fluid for the new capacity. The caller respawns if it wants
	// the volume back (this mirrors clear() + spawn()).
	if (!Engine::get_singleton()->is_editor_hint() && !_mpm_path() && multimesh.is_valid()) {
		_free_fluid();
		_make_fluid();
		spawned = false;
	}
}

void PhysXParticleFluid3D::set_particle_size(float p_size) {
	particle_size = MAX(p_size, 0.001f);
	_apply_params();
	if (foam_size <= 0.0f) {
		_apply_foam(); // foam tracks particle_size when foam_size is auto
	}
	if (mpm != nullptr) {
		Ref<SphereMesh> s = particle_mesh;
		if (s.is_valid()) {
			s->set_radius(particle_size * 0.5);
			s->set_height(particle_size);
		}
		if (spawned && mpm_grid_resolution <= 0) {
			_mpm_configure(!_mpm_emit_mode); // auto grid tracks particle_size -> full rebuild
		} else {
			_mpm_apply_surface_params(); // pinned grid -> just the isosurface params
		}
	}
}

void PhysXParticleFluid3D::set_viscosity(float p_v) {
	viscosity = p_v;
	_apply_params();
}

void PhysXParticleFluid3D::set_adhesion(float p_v) {
	adhesion = MAX(p_v, 0.0f);
	_apply_params();
}

void PhysXParticleFluid3D::set_surface_friction(float p_v) {
	surface_friction = MAX(p_v, 0.0f);
	_apply_params();
}

void PhysXParticleFluid3D::set_render_mode(RenderMode p_mode) {
	const RenderMode before = get_render_mode();
	screen_space_requested = (p_mode == RENDER_MODE_SCREEN_SPACE);
	set_surface_mesh(p_mode == RENDER_MODE_ISOSURFACE);
	if (get_render_mode() != before) {
		notify_property_list_changed();
	}
}

// The solver whose settings the Inspector shows: the authored one, or what
// Auto picks on this machine.
PhysXParticleFluid3D::SolverBackend PhysXParticleFluid3D::_shown_solver() const {
	if (solver != SOLVER_AUTO) {
		return solver;
	}
	// Match _resolved_solver(): Auto never picks PBD for a granular node or
	// one with foam enabled, so the Inspector must not label them "Auto (PBD
	// here)" either (that also hides the mpm_* settings they actually use).
	if (_is_granular() || foam_enabled) {
		return SOLVER_MPM;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	const bool gpu_pbd = server && server->is_gpu_dynamics_enabled() && server->particle_fluid_supports_diffuse();
	return gpu_pbd ? SOLVER_PBD : SOLVER_MPM;
}

void PhysXParticleFluid3D::_validate_property(PropertyInfo &p_property) const {
	// The solver dropdown names what Auto picks here.
	if (p_property.name == StringName("solver")) {
		p_property.hint_string = _shown_solver() == SOLVER_PBD
				? "Auto (PBD here),PBD (CUDA),MPM (compute)"
				: "Auto (MPM here),PBD (CUDA),MPM (compute)";
		return;
	}
	// Each solver's own settings only; hidden values still save.
	const bool pbd = _shown_solver() == SOLVER_PBD;
	static const char *pbd_only[] = { "surface_tension", "cohesion", "vorticity", "adhesion", "surface_friction", "surface_anisotropy", "foam_enabled", "foam_size", nullptr };
	static const char *mpm_only[] = { "mpm_domain_size", "mpm_substeps", "mpm_colliders", nullptr };
	for (int i = 0; pbd_only[i]; i++) {
		if (p_property.name == StringName(pbd_only[i]) && !pbd) {
			p_property.usage = p_property.usage & ~PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
			return;
		}
	}
	for (int i = 0; mpm_only[i]; i++) {
		if (p_property.name == StringName(mpm_only[i]) && pbd) {
			p_property.usage = p_property.usage & ~PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
			return;
		}
	}
}

void PhysXParticleFluid3D::set_gravity_scale(float p_v) {
	gravity_scale = p_v;
	// PBD path: live (material param). MPM path: the solver's gravity is
	// baked at configure, so a live change reconfigures it.
	if (spawned && _mpm_path()) {
		_mpm_configure(!_mpm_emit_mode);
	} else {
		_apply_params();
	}
}

void PhysXParticleFluid3D::set_surface_tension(float p_v) {
	surface_tension = p_v;
	_apply_params();
}

void PhysXParticleFluid3D::set_cohesion(float p_v) {
	cohesion = p_v;
	_apply_params();
}

void PhysXParticleFluid3D::set_vorticity(float p_v) {
	vorticity = p_v;
	_apply_params();
}

void PhysXParticleFluid3D::set_spawn_region_size(const Vector3 &p_size) {
	spawn_region_size = p_size.maxf(0.0);
	if (spawned && _mpm_path() && !_mpm_emit_mode) {
		_mpm_configure(true); // the MPM prefill fills this region
	}
	update_gizmos();
}

void PhysXParticleFluid3D::set_spawn_on_ready(bool p_enable) {
	spawn_on_ready = p_enable;
}

void PhysXParticleFluid3D::set_emitting(bool p_emitting) {
	emitting = p_emitting;
}

void PhysXParticleFluid3D::set_emission_rate(float p_rate) {
	emission_rate = MAX(p_rate, 0.0f);
}

void PhysXParticleFluid3D::set_emission_radius(float p_radius) {
	emission_radius = MAX(p_radius, 0.0f);
}

void PhysXParticleFluid3D::set_emission_velocity(const Vector3 &p_velocity) {
	emission_velocity = p_velocity;
}

void PhysXParticleFluid3D::set_surface_anisotropy(bool p_enabled) {
	if (surface_anisotropy == p_enabled) {
		return;
	}
	surface_anisotropy = p_enabled;
	// Live toggle -- no rebuild; the isosurface callback reads it each step.
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (fluid.is_valid() && server) {
		server->particle_fluid_set_surface_anisotropy(fluid, surface_anisotropy);
	}
	update_configuration_warnings();
}

void PhysXParticleFluid3D::set_surface_mesh(bool p_enabled) {
	if (surface_mesh == p_enabled) {
		return;
	}
	surface_mesh = p_enabled;
	// Rebuild so the isosurface extractor and ArrayMesh come up (or tear down).
	// Same guard as set_solver(): on the MPM path `fluid` is always null and
	// the backend lives behind `multimesh`, so switching render mode on a
	// running MPM fluid never rebuilt anything (the isosurface stayed empty
	// with surface_mesh set, and the MultiMesh stayed hidden after switching
	// back to Particles).
	if (fluid.is_valid() || multimesh.is_valid()) {
		const bool was_spawned = spawned || emitting;
		_free_fluid();
		if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
			_make_fluid();
			if (was_spawned && spawn_on_ready) {
				spawn();
			}
		}
	}
	update_configuration_warnings();
}

void PhysXParticleFluid3D::set_foam_enabled(bool p_enabled) {
	foam_enabled = p_enabled;
	_apply_foam();
	if (_mpm_path() && mpm != nullptr && _mpm_configured) {
		// Runtime toggle flips the solver flag; the slot allocation itself was
		// sized at configure() (a solver configured with foam off has no slots,
		// so enabling stays inert until the next spawn/configure).
		mpm->set_foam_active(p_enabled && !_is_granular());
	}
	// Note: enabling/disabling foam at runtime never flips the backend of a
	// live fluid (the resolution is frozen at creation); on the guarded CUDA
	// path the count simply stays 0 until the fluid is respawned.
}

void PhysXParticleFluid3D::set_foam_particle_count(int p_count) {
	const int new_count = MAX(p_count, 1);
	if (foam_particle_count == new_count) {
		return;
	}
	foam_particle_count = new_count;
	// The foam MultiMesh and the server-side diffuse capacity are sized from
	// foam_particle_count at creation; rebuild a live PBD fluid so the foam
	// buffer write can't outgrow the allocation (same contract as
	// set_particle_count).
	if (!Engine::get_singleton()->is_editor_hint() && !_mpm_path() && fluid.is_valid()) {
		_free_fluid();
		_make_fluid();
		spawned = false;
	}
	if (!Engine::get_singleton()->is_editor_hint() && _mpm_path() && foam_multimesh.is_valid()) {
		// PART-9: on the MPM path the foam MultiMesh must be REALLOCATED to the
		// new count -- _update_mpm_foam sizes its upload from
		// foam_particle_count, so the old allocation mismatched every frame
		// (RS "buffer size != instances*stride" errors) until the next
		// configure.
		RenderingServer::get_singleton()->multimesh_allocate_data(foam_multimesh, foam_particle_count, RenderingServer::MULTIMESH_TRANSFORM_3D);
		if (foam_mesh.is_valid()) {
			RenderingServer::get_singleton()->multimesh_set_mesh(foam_multimesh, foam_mesh->get_rid());
		}
		RenderingServer::get_singleton()->multimesh_set_visible_instances(foam_multimesh, 0);
	}
}

void PhysXParticleFluid3D::set_foam_lifetime(float p_v) {
	foam_lifetime = MAX(p_v, 0.01f);
	_apply_foam();
}

void PhysXParticleFluid3D::set_foam_threshold(float p_v) {
	foam_threshold = MAX(p_v, 0.0f);
	_apply_foam();
}

void PhysXParticleFluid3D::set_foam_buoyancy(float p_v) {
	foam_buoyancy = CLAMP(p_v, 0.0f, 1.0f);
	_apply_foam();
}

void PhysXParticleFluid3D::set_foam_size(float p_v) {
	foam_size = p_v > 0.0f ? MAX(p_v, 0.001f) : 0.0f; // 0 = follow particle_size
	Ref<SphereMesh> s = foam_mesh;
	if (s.is_valid()) {
		s->set_radius(_effective_foam_size() * 0.5);
		s->set_height(_effective_foam_size());
	}
	_apply_foam();
}

PackedVector3Array PhysXParticleFluid3D::get_foam_positions() const {
	if (_mpm_path()) {
		return mpm != nullptr ? mpm->get_foam_positions() : PackedVector3Array();
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return PackedVector3Array();
	}
	return server->particle_fluid_get_foam_positions(fluid);
}

int PhysXParticleFluid3D::get_live_foam_count() const {
	if (_mpm_path()) {
		// Vulkan diffuse layer: count from the solver's last reaped step.
		return mpm != nullptr ? mpm->get_foam_count() : 0;
	}
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	if (!server || fluid.is_null()) {
		return 0;
	}
	return server->particle_fluid_get_foam_count(fluid);
}

AABB PhysXParticleFluid3D::get_aabb() const {
	// Particles roam past the node; a box a few times the spawn region keeps the
	// MultiMesh from being culled without making the editor bounds huge. The
	// isosurface mesh sets its own tight custom AABB when surface_mesh is on.
	const Vector3 e = spawn_region_size.maxf(1.0) * 1.5;
	return AABB(-e, e * 2.0);
}

PackedStringArray PhysXParticleFluid3D::get_configuration_warnings() const {
	PackedStringArray warnings = GeometryInstance3D::get_configuration_warnings();

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	const bool on_mpm = _resolved_solver() == SOLVER_MPM;

	const String engine = GLOBAL_GET("physics/3d/physics_engine");
	if (!on_mpm && engine != "PhysX" && engine != "DEFAULT") {
		warnings.push_back(RTR("The PBD backend only does anything when Project Settings > Physics > 3D > Physics Engine is set to \"PhysX\"."));
	}
	if (!on_mpm && !server) {
		warnings.push_back(RTR("The PhysX physics server is not active. Set the 3D physics engine to \"PhysX\", or set Solver to \"MPM (compute)\"."));
	}
	if (solver == SOLVER_PBD && server && !server->is_gpu_dynamics_enabled()) {
		warnings.push_back(RTR("Solver is \"PBD (CUDA)\" but no CUDA device is available (or Enhanced Determinism is on) -- the fluid will be inert. Use \"Auto\" or \"MPM (compute)\"."));
	}
	if (solver == SOLVER_PBD && server && server->is_gpu_dynamics_enabled() && !server->particle_fluid_supports_diffuse()) {
		warnings.push_back(RTR("Solver is \"PBD (CUDA)\" but this platform's PhysX SDK lacks the CUDA particle buffer factory (Windows only through PhysX 5.11) -- the fluid will be inert. Use \"Auto\" or \"MPM (compute)\"."));
	}
	if (on_mpm && (int)mpm_colliders.size() > 32) {
		warnings.push_back(RTR("MPM coupling uses at most 32 colliders (rigid bodies plus any nearby debris chunks); the rest are ignored."));
	}
	if (surface_anisotropy && !surface_mesh) {
		warnings.push_back(RTR("\"Surface Anisotropy\" has no effect unless \"Surface Mesh\" is enabled."));
	}
	if (surface_anisotropy && emitting) {
		warnings.push_back(RTR("\"Surface Anisotropy\" sharpens the isosurface but can mesh fast particles along the emission stream as thin spikes. It is meant for a settled pool; disable it while emitting."));
	}
	if (particle_count > 200000) {
		warnings.push_back(RTR("Very high particle count. GPU fluids above ~100k particles get expensive."));
	}

	return warnings;
}

void PhysXParticleFluid3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("spawn"), &PhysXParticleFluid3D::spawn);
	ClassDB::bind_method(D_METHOD("clear"), &PhysXParticleFluid3D::clear);
	ClassDB::bind_method(D_METHOD("get_live_particle_count"), &PhysXParticleFluid3D::get_live_particle_count);
	ClassDB::bind_method(D_METHOD("get_mpm_step_msec"), &PhysXParticleFluid3D::get_mpm_step_msec);
	ClassDB::bind_method(D_METHOD("get_particle_positions"), &PhysXParticleFluid3D::get_particle_positions);
	ClassDB::bind_method(D_METHOD("get_submersion", "world_aabb"), &PhysXParticleFluid3D::get_submersion);

	ClassDB::bind_method(D_METHOD("set_solver", "solver"), &PhysXParticleFluid3D::set_solver);
	ClassDB::bind_method(D_METHOD("is_mpm_available"), &PhysXParticleFluid3D::is_mpm_available);
	ClassDB::bind_method(D_METHOD("get_solver"), &PhysXParticleFluid3D::get_solver);
	ClassDB::bind_method(D_METHOD("set_mpm_domain_size", "size"), &PhysXParticleFluid3D::set_mpm_domain_size);
	ClassDB::bind_method(D_METHOD("get_mpm_domain_size"), &PhysXParticleFluid3D::get_mpm_domain_size);
	ClassDB::bind_method(D_METHOD("set_mpm_grid_resolution", "resolution"), &PhysXParticleFluid3D::set_mpm_grid_resolution);
	ClassDB::bind_method(D_METHOD("get_mpm_grid_resolution"), &PhysXParticleFluid3D::get_mpm_grid_resolution);
	ClassDB::bind_method(D_METHOD("set_mpm_substeps", "substeps"), &PhysXParticleFluid3D::set_mpm_substeps);
	ClassDB::bind_method(D_METHOD("get_mpm_substeps"), &PhysXParticleFluid3D::get_mpm_substeps);
	ClassDB::bind_method(D_METHOD("set_mpm_stiffness", "stiffness"), &PhysXParticleFluid3D::set_mpm_stiffness);
	ClassDB::bind_method(D_METHOD("get_mpm_stiffness"), &PhysXParticleFluid3D::get_mpm_stiffness);
	ClassDB::bind_method(D_METHOD("set_mpm_colliders", "colliders"), &PhysXParticleFluid3D::set_mpm_colliders);
	ClassDB::bind_method(D_METHOD("get_mpm_colliders"), &PhysXParticleFluid3D::get_mpm_colliders);
	ClassDB::bind_method(D_METHOD("set_mpm_auto_colliders", "enabled"), &PhysXParticleFluid3D::set_mpm_auto_colliders);
	ClassDB::bind_method(D_METHOD("is_mpm_auto_colliders"), &PhysXParticleFluid3D::is_mpm_auto_colliders);

	ClassDB::bind_method(D_METHOD("set_particle_count", "count"), &PhysXParticleFluid3D::set_particle_count);
	ClassDB::bind_method(D_METHOD("get_particle_count"), &PhysXParticleFluid3D::get_particle_count);
	ClassDB::bind_method(D_METHOD("set_particle_size", "size"), &PhysXParticleFluid3D::set_particle_size);
	ClassDB::bind_method(D_METHOD("get_particle_size"), &PhysXParticleFluid3D::get_particle_size);
	ClassDB::bind_method(D_METHOD("set_viscosity", "viscosity"), &PhysXParticleFluid3D::set_viscosity);
	ClassDB::bind_method(D_METHOD("set_adhesion", "v"), &PhysXParticleFluid3D::set_adhesion);
	ClassDB::bind_method(D_METHOD("get_adhesion"), &PhysXParticleFluid3D::get_adhesion);
	ClassDB::bind_method(D_METHOD("set_surface_friction", "v"), &PhysXParticleFluid3D::set_surface_friction);
	ClassDB::bind_method(D_METHOD("get_surface_friction"), &PhysXParticleFluid3D::get_surface_friction);
	ClassDB::bind_method(D_METHOD("set_render_mode", "mode"), &PhysXParticleFluid3D::set_render_mode);
	ClassDB::bind_method(D_METHOD("get_render_mode"), &PhysXParticleFluid3D::get_render_mode);
	ClassDB::bind_method(D_METHOD("set_gravity_scale", "scale"), &PhysXParticleFluid3D::set_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_gravity_scale"), &PhysXParticleFluid3D::get_gravity_scale);
	ClassDB::bind_method(D_METHOD("get_viscosity"), &PhysXParticleFluid3D::get_viscosity);
	ClassDB::bind_method(D_METHOD("set_surface_tension", "surface_tension"), &PhysXParticleFluid3D::set_surface_tension);
	ClassDB::bind_method(D_METHOD("get_surface_tension"), &PhysXParticleFluid3D::get_surface_tension);
	ClassDB::bind_method(D_METHOD("set_cohesion", "cohesion"), &PhysXParticleFluid3D::set_cohesion);
	ClassDB::bind_method(D_METHOD("get_cohesion"), &PhysXParticleFluid3D::get_cohesion);
	ClassDB::bind_method(D_METHOD("set_vorticity", "vorticity"), &PhysXParticleFluid3D::set_vorticity);
	ClassDB::bind_method(D_METHOD("get_vorticity"), &PhysXParticleFluid3D::get_vorticity);
	ClassDB::bind_method(D_METHOD("set_spawn_region_size", "size"), &PhysXParticleFluid3D::set_spawn_region_size);
	ClassDB::bind_method(D_METHOD("get_spawn_region_size"), &PhysXParticleFluid3D::get_spawn_region_size);
	ClassDB::bind_method(D_METHOD("set_spawn_on_ready", "enable"), &PhysXParticleFluid3D::set_spawn_on_ready);
	ClassDB::bind_method(D_METHOD("get_spawn_on_ready"), &PhysXParticleFluid3D::get_spawn_on_ready);

	ClassDB::bind_method(D_METHOD("set_emitting", "emitting"), &PhysXParticleFluid3D::set_emitting);
	ClassDB::bind_method(D_METHOD("is_emitting"), &PhysXParticleFluid3D::is_emitting);
	ClassDB::bind_method(D_METHOD("set_emission_rate", "rate"), &PhysXParticleFluid3D::set_emission_rate);
	ClassDB::bind_method(D_METHOD("get_emission_rate"), &PhysXParticleFluid3D::get_emission_rate);
	ClassDB::bind_method(D_METHOD("set_emission_radius", "radius"), &PhysXParticleFluid3D::set_emission_radius);
	ClassDB::bind_method(D_METHOD("get_emission_radius"), &PhysXParticleFluid3D::get_emission_radius);
	ClassDB::bind_method(D_METHOD("set_emission_velocity", "velocity"), &PhysXParticleFluid3D::set_emission_velocity);
	ClassDB::bind_method(D_METHOD("get_emission_velocity"), &PhysXParticleFluid3D::get_emission_velocity);
	ClassDB::bind_method(D_METHOD("set_surface_mesh", "enabled"), &PhysXParticleFluid3D::set_surface_mesh);
	ClassDB::bind_method(D_METHOD("is_surface_mesh"), &PhysXParticleFluid3D::is_surface_mesh);
	ClassDB::bind_method(D_METHOD("set_surface_anisotropy", "enabled"), &PhysXParticleFluid3D::set_surface_anisotropy);
	ClassDB::bind_method(D_METHOD("is_surface_anisotropy"), &PhysXParticleFluid3D::is_surface_anisotropy);

	ClassDB::bind_method(D_METHOD("set_foam_enabled", "enabled"), &PhysXParticleFluid3D::set_foam_enabled);
	ClassDB::bind_method(D_METHOD("is_foam_enabled"), &PhysXParticleFluid3D::is_foam_enabled);
	ClassDB::bind_method(D_METHOD("set_foam_particle_count", "count"), &PhysXParticleFluid3D::set_foam_particle_count);
	ClassDB::bind_method(D_METHOD("get_foam_particle_count"), &PhysXParticleFluid3D::get_foam_particle_count);
	ClassDB::bind_method(D_METHOD("set_foam_lifetime", "seconds"), &PhysXParticleFluid3D::set_foam_lifetime);
	ClassDB::bind_method(D_METHOD("get_foam_lifetime"), &PhysXParticleFluid3D::get_foam_lifetime);
	ClassDB::bind_method(D_METHOD("set_foam_threshold", "threshold"), &PhysXParticleFluid3D::set_foam_threshold);
	ClassDB::bind_method(D_METHOD("get_foam_threshold"), &PhysXParticleFluid3D::get_foam_threshold);
	ClassDB::bind_method(D_METHOD("set_foam_buoyancy", "buoyancy"), &PhysXParticleFluid3D::set_foam_buoyancy);
	ClassDB::bind_method(D_METHOD("get_foam_buoyancy"), &PhysXParticleFluid3D::get_foam_buoyancy);
	ClassDB::bind_method(D_METHOD("set_foam_size", "size"), &PhysXParticleFluid3D::set_foam_size);
	ClassDB::bind_method(D_METHOD("get_foam_size"), &PhysXParticleFluid3D::get_foam_size);
	ClassDB::bind_method(D_METHOD("get_live_foam_count"), &PhysXParticleFluid3D::get_live_foam_count);
	ClassDB::bind_method(D_METHOD("get_foam_positions"), &PhysXParticleFluid3D::get_foam_positions);

	BIND_ENUM_CONSTANT(SOLVER_AUTO);
	BIND_ENUM_CONSTANT(RENDER_MODE_PARTICLES);
	BIND_ENUM_CONSTANT(RENDER_MODE_ISOSURFACE);
	BIND_ENUM_CONSTANT(RENDER_MODE_SCREEN_SPACE);
	BIND_ENUM_CONSTANT(SOLVER_PBD);
	BIND_ENUM_CONSTANT(SOLVER_MPM);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "solver", PROPERTY_HINT_ENUM, "Auto,PBD (CUDA),MPM (compute)"), "set_solver", "get_solver");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "particle_count", PROPERTY_HINT_RANGE, "1,262144,1"), "set_particle_count", "get_particle_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "particle_size", PROPERTY_HINT_RANGE, "0.01,1,0.001,suffix:m"), "set_particle_size", "get_particle_size");
	ADD_GROUP("MPM", "mpm_");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "mpm_domain_size", PROPERTY_HINT_NONE, "suffix:m"), "set_mpm_domain_size", "get_mpm_domain_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mpm_grid_resolution", PROPERTY_HINT_RANGE, "0,128,1", PROPERTY_USAGE_DEFAULT), "set_mpm_grid_resolution", "get_mpm_grid_resolution"); // 0 = auto from particle_size
	ADD_PROPERTY(PropertyInfo(Variant::INT, "mpm_substeps", PROPERTY_HINT_RANGE, "1,40,1"), "set_mpm_substeps", "get_mpm_substeps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "mpm_stiffness", PROPERTY_HINT_RANGE, "1,40000,1,or_greater"), "set_mpm_stiffness", "get_mpm_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "mpm_colliders", PROPERTY_HINT_ARRAY_TYPE, "NodePath"), "set_mpm_colliders", "get_mpm_colliders");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "mpm_auto_colliders"), "set_mpm_auto_colliders", "is_mpm_auto_colliders");
	ADD_GROUP("Spawn", "");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "spawn_region_size", PROPERTY_HINT_NONE, "suffix:m"), "set_spawn_region_size", "get_spawn_region_size");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "spawn_on_ready"), "set_spawn_on_ready", "get_spawn_on_ready");
	ADD_GROUP("Emission", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "emitting"), "set_emitting", "is_emitting");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_rate", PROPERTY_HINT_RANGE, "0,50000,1,or_greater,suffix:1/s"), "set_emission_rate", "get_emission_rate");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "emission_radius", PROPERTY_HINT_RANGE, "0,2,0.001,or_greater,suffix:m"), "set_emission_radius", "get_emission_radius");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "emission_velocity", PROPERTY_HINT_NONE, "suffix:m/s"), "set_emission_velocity", "get_emission_velocity");
	ADD_GROUP("Rendering", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "render_mode", PROPERTY_HINT_ENUM, "Particles,Isosurface,Screen Space"), "set_render_mode", "get_render_mode");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "surface_mesh", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NONE), "set_surface_mesh", "is_surface_mesh");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "surface_anisotropy"), "set_surface_anisotropy", "is_surface_anisotropy");
	ADD_GROUP("Foam", "foam_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "foam_enabled"), "set_foam_enabled", "is_foam_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "foam_particle_count", PROPERTY_HINT_RANGE, "1,262144,1"), "set_foam_particle_count", "get_foam_particle_count");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_lifetime", PROPERTY_HINT_RANGE, "0.01,20,0.01,or_greater,suffix:s"), "set_foam_lifetime", "get_foam_lifetime");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_threshold", PROPERTY_HINT_RANGE, "0,2000,1,or_greater"), "set_foam_threshold", "get_foam_threshold");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_buoyancy", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_foam_buoyancy", "get_foam_buoyancy");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "foam_size", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater,suffix:m"), "set_foam_size", "get_foam_size");
	ADD_GROUP("Fluid", "");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "viscosity", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_viscosity", "get_viscosity");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "adhesion", PROPERTY_HINT_RANGE, "0,5,0.01,or_greater"), "set_adhesion", "get_adhesion");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "gravity_scale", PROPERTY_HINT_RANGE, "-4,4,0.01"), "set_gravity_scale", "get_gravity_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "surface_tension", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_surface_tension", "get_surface_tension");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "surface_friction", PROPERTY_HINT_RANGE, "0,2,0.01,or_greater"), "set_surface_friction", "get_surface_friction");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "cohesion", PROPERTY_HINT_RANGE, "0,1,0.001,or_greater"), "set_cohesion", "get_cohesion");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "vorticity", PROPERTY_HINT_RANGE, "0,50,0.01,or_greater"), "set_vorticity", "get_vorticity");
}

PhysXParticleFluid3D::PhysXParticleFluid3D() {
}

PhysXParticleFluid3D::~PhysXParticleFluid3D() {
	if (mpm != nullptr) {
		memdelete(mpm);
		mpm = nullptr;
	}
}
