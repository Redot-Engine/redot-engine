/**************************************************************************/
/*  physx_space_3d.h                                                      */
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
 * @file physx_space_3d.h
 * @brief A physics world — owns one PxScene and its supporting infrastructure:
 * the simulation event / pair filter / contact modify callbacks and the
 * PhysXDirectSpaceState3D query interface.
 */

#ifndef PHYSX_SPACE_3D_H
#define PHYSX_SPACE_3D_H
#include "core/math/math_funcs.h"
#include "core/os/thread.h"
#include "core/templates/hash_set.h"
#include "core/templates/local_vector.h"
#include "servers/physics_3d/physics_server_3d.h"
namespace physx {
class PxScene;
class PxActor;
class PxPhysics;
class PxDefaultCpuDispatcher;
class PxCudaContextManager;
} //namespace physx

class PhysXArea3D;
class PhysXObject3D;
class PhysXBody3D;
class PhysXJoint3D;
class PhysXDirectSpaceState3D;
class PhysXVehicleServer;
class PhysXSimulationEventCallback;
class PhysXShapedObject3D;
class PhysXSoftBody3D;
class PhysXPairFilterCallback;
class PhysXContactModifyCallback;
class PhysXGPUParticleFluid3D;
class PhysXGPUCloth3D;
class PhysXArticulation3D;

class PhysXSpace3D {
public:
	PhysXSpace3D();
	~PhysXSpace3D();

	// Async stepping (physics/physx_3d/simulation/async_step): step() kicks
	// PxScene::simulate() and returns; sync() — which the engine calls at the
	// start of the next tick, BEFORE scripts — does the fetchResults and the
	// whole post-solve pipeline (_finish_step). With the flag off, step()
	// fetches inline and sync() is a no-op.
	void step(float p_step);
	void sync();
	/// Fetches an in-flight solve immediately (no-op when idle) so structural
	/// scene mutations become legal again: actor/joint/articulation add-remove-
	/// release, shape attach/detach and scene queries are all forbidden while
	/// simulate() is outstanding. Mutation entry points that cannot be deferred
	/// call this first — mutating frames lose the async overlap by design.
	///
	/// Thread discipline: only the thread that called simulate() may fetch.
	/// A call from another thread (frame-thread queries against a space whose
	/// step runs on the WrapMT physics thread) is refused with a one-shot
	/// warning and reads the last fetched state instead — PhysX requires
	/// simulate/fetchResults on one thread. The server-tick sync() below is
	/// exempt: the WrapMT sync drains the command queue (the physics thread is
	/// idle by construction) before fetching on the main thread.
	void ensure_synced();
	bool is_stepping() const { return stepping; }
	float get_last_step() const { return last_step; }
	/// Active-actor count of the last completed solve (INFO_ACTIVE_OBJECTS).
	int get_active_objects() const { return active_objects; }
	/// Narrow-phase pair count of the last completed solve
	/// (INFO_COLLISION_PAIRS). Sampled in _finish_step — must not be queried
	/// mid-solve (getSimulationStatistics is silently ignored there and races
	/// under separate-thread physics).
	int get_cached_contact_pairs() const { return cached_contact_pairs; }
	/// World gravity the PxScene was created with (zero until initialized).
	Vector3 get_scene_gravity() const { return scene_gravity; }

	RID get_rid() const { return rid; }
	void set_rid(const RID &p_rid) { rid = p_rid; }

	bool is_active() const { return active; }
	void set_active(bool p_active);

	// Godot Space Parameters
	double get_param(PhysicsServer3D::SpaceParameter p_param) const;
	void set_param(PhysicsServer3D::SpaceParameter p_param, double p_value);

	int get_solver_iteration_count() const { return solver_iteration_count; }

	// Space-mapped shape rest offset from SPACE_PARAM_CONTACT_MAX_ALLOWED_
	// PENETRATION: Godot lets contacts rest at `allowed` penetration; PhysX
	// bodies rest at the SUM of a pair's rest offsets, so each shape carries
	// half. 0.0 unless the value was set explicitly — the Godot default
	// (0.01) deliberately stays unmapped so default resting behavior is
	// unchanged. Changing the value mid-simulation applies to subsequently
	// attached shapes only.
	real_t get_shape_rest_offset() const { return shape_rest_offset; }

	// Godot SpaceParameters (separate linear/angular velocity thresholds plus
	// time-to-sleep) mapped onto PhysX's mass-normalized kinetic-energy sleep
	// threshold + wake counter.
	real_t get_sleep_threshold_linear() const { return sleep_threshold_linear; }
	real_t get_sleep_threshold_angular() const { return sleep_threshold_angular; }
	real_t get_time_before_sleep() const { return time_before_sleep; }
	real_t get_sleep_energy_threshold() const {
		return 0.5 * (sleep_threshold_linear * sleep_threshold_linear + sleep_threshold_angular * sleep_threshold_angular);
	}

	PhysXDirectSpaceState3D *get_direct_state() { return direct_state; }

	// While a solve is in flight (async mode), scene mutations are queued and
	// applied right after the fetch — PhysX forbids add/remove while the
	// simulation runs. The mid-flight window is real: deferred engine calls
	// and node destruction run between step() and the next sync().
	// release_actor lets wrappers transfer PxActor ownership to the space so
	// a freed body's actor is released only after the fetch, never mid-solve.
	void add_actor(physx::PxActor *p_actor);
	void remove_actor(physx::PxActor *p_actor);
	void release_actor(physx::PxActor *p_actor);

	physx::PxScene *get_px_scene() const { return px_scene; }

	PhysXArea3D *get_default_area() const { return default_area; }
	void set_default_area(PhysXArea3D *p_area) { default_area = p_area; }

	/// Releases every soft-body rigid attachment referencing p_actor — called
	/// from the server's body-free path BEFORE the actor is released, so a
	/// live PxDeformableAttachment never dangles on a freed rigid.
	void release_soft_body_attachments_for(physx::PxActor *p_actor);

	// Debug contacts — drives the "Visible Collision Shapes" contact overlay.
	// set_debug_contacts() allocates the buffer; onContact appends each step;
	// enabling it on any space flips a module-global flag so the stateless
	// simulation filter shader requests contact-point notifications for all
	// pairs (not just those with max_contacts_reported > 0). Matches
	// godot_physics_3d's debug-contact semantics.
	void set_debug_contacts(int p_amount);
	_FORCE_INLINE_ bool is_debugging_contacts() const { return !debug_contacts_buffer.is_empty(); }
	_FORCE_INLINE_ void add_debug_contact(const Vector3 &p_contact) {
		if (debug_contacts_count < (int)debug_contacts_buffer.size()) {
			debug_contacts_buffer.write[debug_contacts_count++] = p_contact;
		}
	}
	_FORCE_INLINE_ Vector<Vector3> get_debug_contacts() const { return debug_contacts_buffer; }
	_FORCE_INLINE_ int get_debug_contact_count() const { return debug_contacts_count; }

	// onTrigger/onContact run inside fetchResults(), outside Godot's "safe"
	// callback window: Area3D monitor callbacks must fire during
	// PhysicsServer3D::flush_queries() (after sync, world unlocked), so
	// onTrigger records events here and flush_pending_callbacks() dispatches
	// them in the flush_queries() window.
	/// A deferred area/body monitor event (from onTrigger).
	struct TriggerEvent {
		PhysXArea3D *trigger_area = nullptr; ///< The trigger area (dispatches the callback).
		PhysXBody3D *body = nullptr; ///< The other body, if this is a body-monitor event.
		PhysXArea3D *other_area = nullptr; ///< The other area, if this is an area-vs-area event.
		PhysicsServer3D::AreaBodyStatus status = PhysicsServer3D::AREA_BODY_ADDED;
		int other_shape_idx = 0; ///< Shape index of the other actor.
		int area_shape_idx = 0; ///< Shape index of this area.
		bool is_area_vs_area = false; ///< Selects area_monitor vs body_monitor dispatch.
		/// Identity is cached at queue time: the body/area may be freed before
		/// flush_queries() dispatches (body_exited must still fire), so the
		/// dispatch must never dereference body/other_area.
		RID body_rid;
		ObjectID body_id;
		RID other_area_rid;
		ObjectID other_area_id;
	};

	/// A deferred contact event (from onContact); reserves the hook for
	/// state-sync dispatch at flush time.
	struct ContactEvent {
		PhysXBody3D *body = nullptr;
	};

	void queue_trigger(const TriggerEvent &p_event);
	void flush_pending_callbacks();

	bool is_flushing_callbacks() const { return flushing_callbacks; }

	// --- Registration (unregister drops the back-pointer; see the member
	// comments below for why each list is tracked) ---
	void register_body(PhysXBody3D *p_body);
	void unregister_body(PhysXBody3D *p_body);

	void register_area(PhysXArea3D *p_area);
	void unregister_area(PhysXArea3D *p_area);

	void register_vehicle(PhysXVehicleServer *p_vehicle);
	void unregister_vehicle(PhysXVehicleServer *p_vehicle);

	const LocalVector<PhysXVehicleServer *> &get_vehicles() const {
		return vehicles;
	}

	void register_fluid(PhysXGPUParticleFluid3D *p_fluid);
	void unregister_fluid(PhysXGPUParticleFluid3D *p_fluid);
	void register_cloth(PhysXGPUCloth3D *p_cloth);
	void unregister_cloth(PhysXGPUCloth3D *p_cloth);

	const LocalVector<PhysXGPUParticleFluid3D *> &get_fluids() const {
		return fluids;
	}
	const LocalVector<PhysXGPUCloth3D *> &get_cloths() const {
		return cloths;
	}

	void register_soft_body(PhysXSoftBody3D *p_sb) {
		if (soft_bodies.find(p_sb) == -1) {
			soft_bodies.push_back(p_sb);
		}
	}
	void unregister_soft_body(PhysXSoftBody3D *p_sb) {
		int idx = soft_bodies.find(p_sb);
		if (idx != -1) {
			soft_bodies.remove_at(idx);
		}
	}

	// Articulations are tracked so freeing the space can detach them: their
	// wrappers survive the space and must not keep a dangling scene pointer.
	void register_articulation(PhysXArticulation3D *p_articulation);
	void unregister_articulation(PhysXArticulation3D *p_articulation);

	const LocalVector<PhysXArticulation3D *> &get_articulations() const {
		return articulations;
	}

	// Convenience accessors for the GPU object types (resolve via the server).
	physx::PxPhysics *get_px_physics() const;
	physx::PxCudaContextManager *get_px_cuda() const;

	const LocalVector<PhysXBody3D *> &get_bodies() const {
		return bodies;
	}

	// Bodies PhysX reported moving in the last completed step (the
	// eENABLE_ACTIVE_ACTORS snapshot _finish_step leaves in
	// prev_active_bodies): interpolation/streaming consumers walk this
	// instead of diffing every body's transform.
	const LocalVector<PhysXBody3D *> &get_moved_bodies() const { return prev_active_bodies; }

	const LocalVector<PhysXArea3D *> &get_areas() const {
		return areas;
	}

private:
	physx::PxScene *px_scene = nullptr;
	physx::PxDefaultCpuDispatcher *px_dispatcher = nullptr;

	PhysXSimulationEventCallback *event_callback = nullptr;

	PhysXPairFilterCallback *pair_filter_callback = nullptr;

	// Godot material combiner: absorbent/rough.
	PhysXContactModifyCallback *contact_modify_callback = nullptr;

	PhysXDirectSpaceState3D *direct_state = nullptr;
	RID rid;
	bool active = false;
	bool stepping = false;
	// Thread that called simulate() for the in-flight solve (0 = none).
	// ensure_synced() refuses to fetch from any other thread.
	Thread::ID stepping_thread = 0;
	bool batched_isosurface = false;
	float last_step = 0.0f;
	int active_objects = 0;
	int cached_contact_pairs = 0;

	// Queued scene mutations from the async in-flight window, applied in
	// order at fetch time (see add_actor/remove_actor/release_actor).
	struct PendingActorOp {
		enum Kind {
			KIND_ADD,
			KIND_REMOVE,
			KIND_RELEASE,
		};
		physx::PxActor *actor = nullptr;
		Kind kind = KIND_ADD;
	};
	LocalVector<PendingActorOp> pending_actor_ops;
	void _flush_actor_ops();
	int solver_iteration_count = 8; // Godot default (physics/3d/solver/solver_iterations)

	// Cached SpaceParameter values so get_param round-trips all eight. The
	// contact_* defaults mirror Godot's project settings, but only an
	// explicit space_set_param maps CONTACT_MAX_ALLOWED_PENETRATION onto
	// shape rest offsets (CONTACT_RECYCLE_RADIUS / CONTACT_DEFAULT_BIAS have
	// no PhysX counterpart at all and are cached for round-trip only).
	real_t param_contact_recycle_radius = 0.01;
	real_t param_contact_max_separation = 0.05;
	real_t param_contact_max_allowed_penetration = 0.01;
	real_t param_contact_default_bias = 0.2;
	bool allowed_penetration_set = false;
	real_t shape_rest_offset = 0.0;

	// Godot sleep policy defaults (physics/3d/sleep_threshold_linear etc.).
	real_t sleep_threshold_linear = 0.1;
	real_t sleep_threshold_angular = Math::deg_to_rad(8.0);
	real_t time_before_sleep = 0.5;

	// Provides world-default gravity/damp.
	PhysXArea3D *default_area = nullptr;

	// Scene gravity cached at creation (the module never mutates it after);
	// serves the per-step consumers without a virtual PxScene::getGravity()
	// round-trip per body per step.
	Vector3 scene_gravity;

	LocalVector<PhysXBody3D *> bodies;
	LocalVector<PhysXArea3D *> areas;
	LocalVector<PhysXVehicleServer *> vehicles;
	LocalVector<PhysXGPUParticleFluid3D *> fluids;
	LocalVector<PhysXGPUCloth3D *> cloths;
	LocalVector<PhysXSoftBody3D *> soft_bodies;
	LocalVector<PhysXArticulation3D *> articulations;

	// Filled during step, drained in flush_queries.
	LocalVector<TriggerEvent> pending_trigger_events;
	bool flushing_callbacks = false;

	// Bodies that were ACTIVE during the previous step (sync gating):
	// bodies in this list that are no longer active just fell asleep and get
	// one final sync. Tracking the *active* set (not the synced set) is what
	// makes the final sync happen exactly once. Cleaned in unregister_body().
	LocalVector<PhysXBody3D *> prev_active_bodies;

	// Reused per-step buffers for the post-step sync set (built in
	// _finish_step). Members instead of locals so a steady-state step does
	// not allocate.
	LocalVector<PhysXBody3D *> _finish_sync_bodies;
	HashSet<PhysXBody3D *> _finish_sync_set;
	LocalVector<PhysXBody3D *> _finish_now_active;

	// Reset (count = 0) at the start of each step; filled by onContact.
	Vector<Vector3> debug_contacts_buffer;
	int debug_contacts_count = 0;

	void _initialize_scene();
	void _terminate_scene();
	/// Shared tail of step()/sync(): everything after fetchResults — queued
	/// mutations, GPU readbacks, CPU soft-body step, post-step hooks, active
	/// actor count. Runs on the server thread.
	void _finish_step();
	/// CPU soft bodies only: resolves the effective gravity from overlapping
	/// areas (AABB approximation of the reference's broadphase pairs) with the
	/// default area as the additive fallback — mirrors godot_physics_3d's
	/// GodotSoftBody3D::predict_motion. GPU deformables keep the scene gravity
	/// (no per-body gravity injection path).
	Vector3 _resolve_soft_body_gravity(const PhysXSoftBody3D &p_sb) const;
	/// Re-applies the sleep policy to every registered body (called when a
	/// sleep-related SpaceParameter changes).
	void _refresh_body_sleep_policies();
};
#endif // PHYSX_SPACE_3D_H
