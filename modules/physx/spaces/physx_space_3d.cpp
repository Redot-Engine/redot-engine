/**************************************************************************/
/*  physx_space_3d.cpp                                                    */
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

/**
 * @file physx_space_3d.cpp
 * @brief Implementation of PhysXSpace3D — the physics world.
 */

#include "physx_space_3d.h"
#include "../joints/physx_joint_3d.h"
#include "../objects/physx_area_3d.h"
#include "../objects/physx_articulation_3d.h"
#include "../objects/physx_body_3d.h"
#include "../objects/physx_gpu_cloth_3d.h"
#include "../objects/physx_gpu_particle_fluid_3d.h"
#include "../objects/physx_object_3d.h"
#include "../objects/physx_shaped_object_3d.h"
#include "../objects/physx_soft_body_3d.h"
#include "../physx_project_settings.h"
#include "../physx_server.h"
#include "../vehicles/physx_vehicle_server.h"
#include "physx_contact_modify_callback.h"
#include "physx_direct_space_state_3d.h"
#include "physx_filter_shader.h"
#include "physx_pair_filter_callback.h"
#include "physx_simulation_event_callback.h"

#include "PxPhysicsAPI.h" // PhysX SDK
#include "extensions/PxDefaultCpuDispatcher.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

// Refcount of spaces currently recording debug contacts. The simulation filter
// shader is stateless, so it reads a module-global flag; we keep it accurate by
// counting how many spaces have a non-empty debug buffer.
static int s_debug_spaces_refcount = 0;

PhysXSpace3D::PhysXSpace3D() {
	pair_filter_callback = memnew(PhysXPairFilterCallback);
	contact_modify_callback = memnew(PhysXContactModifyCallback);
	event_callback = memnew(PhysXSimulationEventCallback(this));
	_initialize_scene();
	// Only build the query wrapper if the scene was created successfully;
	// otherwise direct_state stays null and queries safely early-out.
	if (px_scene) {
		direct_state = memnew(PhysXDirectSpaceState3D(this));
	}
}

PhysXSpace3D::~PhysXSpace3D() {
	// Detach soft bodies while this space (and its PxScene) is still valid --
	// they may outlive it if their RID is never explicitly freed. Runs before
	// _terminate_scene() so a GPU soft volume can still leave the scene.
	for (PhysXSoftBody3D *sb : soft_bodies) {
		sb->notify_space_destroyed();
	}
	soft_bodies.clear();
	if (direct_state) {
		memdelete(direct_state);
	}
	_terminate_scene();
	if (event_callback) {
		memdelete(event_callback);
		event_callback = nullptr;
	}
	if (pair_filter_callback) {
		memdelete(pair_filter_callback);
		pair_filter_callback = nullptr;
	}
	if (contact_modify_callback) {
		memdelete(contact_modify_callback);
		contact_modify_callback = nullptr;
	}
	// Drop any deferred monitor events; their area/body pointers may be gone
	// by the time anyone could flush them.
	pending_trigger_events.clear();

	// Release this space's claim on the global debug-contacts flag.
	if (!debug_contacts_buffer.is_empty()) {
		debug_contacts_buffer.clear();
		if (--s_debug_spaces_refcount == 0) {
			g_physx_debug_contacts_enabled.store(false, std::memory_order_relaxed);
		}
	}
}

void PhysXSpace3D::_initialize_scene() {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	physx::PxPhysics *physics = server ? server->try_get_physics() : nullptr;
	ERR_FAIL_NULL_MSG(physics, "PhysX: server not initialized before space creation");

	// One CPU dispatcher is shared by every space (owned by the server, sized
	// once at init() from physics/physx_3d/simulation/cpu_worker_threads).
	px_dispatcher = server->get_cpu_dispatcher();
	ERR_FAIL_NULL_MSG(px_dispatcher, "PhysX: no shared CPU dispatcher (server init() failed?)");

	physx::PxSceneDesc scene_desc(physics->getTolerancesScale());

	// Gravity: physics/3d/default_gravity times physics/3d/default_gravity_vector.
	// The product is the world gravity vector, which must match the space's
	// default-area gravity so PhysX's native scene gravity equals the resolved
	// default (PhysXBody3D::on_pre_step applies only the area-override delta
	// on top of it -- zero in the default case).
	const real_t g = GLOBAL_GET("physics/3d/default_gravity");
	const Vector3 g_dir = GLOBAL_GET("physics/3d/default_gravity_vector");
	// Cached for the per-step consumers (PhysXBody3D::on_pre_step delta,
	// CPU soft-body step) — scene gravity is never mutated after creation.
	scene_gravity = Vector3(g_dir.x * g, g_dir.y * g, g_dir.z * g);
	scene_desc.gravity = physx::PxVec3(
			(float)scene_gravity.x,
			(float)scene_gravity.y,
			(float)scene_gravity.z);

	scene_desc.cpuDispatcher = px_dispatcher;

	// Simulation event callback (contacts, triggers, wake/sleep).
	scene_desc.simulationEventCallback = event_callback;

	// Custom filter shader implementing Godot's layer/mask test; requests
	// contact-point notifications for shape pairs where either side carries
	// PHYSX_FILTER_FLAG_CONTACT_NOTIFY (set when a body has
	// max_contacts_reported > 0), and returns eNOTIFY for simulation pairs so
	// the pair filter callback can check collision exceptions.
	scene_desc.filterShader = physx_simulation_filter_shader;

	// Pair filter callback: enforces collision exceptions between bodies.
	// Runs after the filter shader; has access to both actors' userData.
	scene_desc.filterCallback = pair_filter_callback;

	// Contact modify callback: implements Godot's material combiner (absorbent /
	// rough) per-pair. Runs on worker threads during simulate(); reads only the
	// pre-step bounce/friction cached in each actor's userData.
	scene_desc.contactModifyCallback = contact_modify_callback;

	// Solver: PGS (PhysX's classic) by default; TGS is opt-in via
	// physics/physx_3d/simulation/solver_type — it holds joint chains steadier
	// under sustained external forces like wind, at some joint looseness cost
	// in large mixed piles.
	scene_desc.solverType = PhysXProjectSettings::solver_type == 1
			? physx::PxSolverType::eTGS
			: physx::PxSolverType::ePGS;

	// Scene flags required for correct Godot integration:
	//   eENABLE_ACTIVE_ACTORS  - efficient body transform sync back to nodes
	//   eENABLE_CCD            - per-body continuous collision detection
	scene_desc.flags |= physx::PxSceneFlag::eENABLE_ACTIVE_ACTORS;
	scene_desc.flags |= physx::PxSceneFlag::eENABLE_CCD;

	if (PhysXProjectSettings::enhanced_determinism) {
		// Same-binary/same-platform determinism, independent of worker count
		// and API call order (not cross-platform). GPU dynamics is disabled
		// entirely in this mode (see PhysXServer3D::init()).
		scene_desc.flags |= physx::PxSceneFlag::eENABLE_ENHANCED_DETERMINISM;
	}

	// eENABLE_STABILIZATION is CPU-path only; PhysX rejects it alongside GPU
	// dynamics. See physx_project_settings.h for why it defaults to off here.
	if (PhysXProjectSettings::stabilization && !server->is_gpu_dynamics_enabled()) {
		scene_desc.flags |= physx::PxSceneFlag::eENABLE_STABILIZATION;
	}

	// GPU dynamics (GODOT_PHYSX_GPU build + usable CUDA device): the whole
	// scene simulates on the GPU.
	//
	// GPU memory config: PhysX 5.12's defaults are used as-is. The 5.11-era
	// overrides this module carried (4M found/lost pairs, 4M contacts, 256 MB
	// heap and collision stack) predate 5.12's reworked PxGpuDynamicsMemoryConfig
	// and break on it: 5.12 allocates the pinned-host heap, collision stack and
	// broadphase mirrors eagerly at those sizes, and a single 256 MB pinned
	// allocation is refused by Windows (cuMemAllocHost -> CUDA_ERROR_OUT_OF_MEMORY).
	// Once refused, 5.12 latches the CUDA context into a permanent
	// out-of-memory state and every later GPU scene in the process aborts.
	// The defaults keep every pinned allocation at or under 64 MB and grow the
	// heap and collision stack on demand in increments of their capacity.
	// The deformable-surface/volume and particle contact budgets below are
	// the defaults spelled out: non-zero so PxDeformableSurface (cloth) /
	// PxDeformableVolume (GPU soft bodies) can generate contacts -- either
	// touching anything with its budget at 0 silently drops the contacts and
	// the deformable falls through the world.
	if (server->is_gpu_dynamics_enabled()) {
		physx::PxCudaContextManager *cuda = server->get_cuda_context();
		if (cuda) {
			scene_desc.cudaContextManager = cuda;
			scene_desc.flags |= physx::PxSceneFlag::eENABLE_GPU_DYNAMICS;
			scene_desc.broadPhaseType = physx::PxBroadPhaseType::eGPU;
			scene_desc.gpuMaxNumPartitions = 8;
		}
	}

	px_scene = physics->createScene(scene_desc);
	ERR_FAIL_NULL_MSG(px_scene, "PhysX: createScene failed");
}

void PhysXSpace3D::_terminate_scene() {
	if (stepping && px_scene) {
		// The space died with a solve in flight (World3D freed between an
		// async step and its sync). Fetch so nothing releases into a live
		// solve, then let the flush below apply queued mutations.
		px_scene->fetchResults(true);
		stepping = false;
	}
	_flush_actor_ops();
	if (px_scene) {
		px_scene->release();
		px_scene = nullptr;
	}
	// NOTE: px_dispatcher is shared and owned by PhysXServer3D (released in
	// finish()); it is only borrowed here and must not be released per-space.
	px_dispatcher = nullptr;
}

void PhysXSpace3D::set_debug_contacts(int p_amount) {
	const bool was_debugging = !debug_contacts_buffer.is_empty();
	debug_contacts_buffer.resize(p_amount);
	const bool is_debugging = !debug_contacts_buffer.is_empty();

	// Keep the global refcount in sync so the filter shader requests contact
	// notifications exactly while at least one space is debugging.
	if (!was_debugging && is_debugging) {
		if (++s_debug_spaces_refcount == 1) {
			g_physx_debug_contacts_enabled.store(true, std::memory_order_relaxed);
		}
	} else if (was_debugging && !is_debugging) {
		if (--s_debug_spaces_refcount == 0) {
			g_physx_debug_contacts_enabled.store(false, std::memory_order_relaxed);
		}
	}
}

void PhysXSpace3D::step(float p_step) {
	if (!active || !px_scene) {
		return;
	}
	// PhysX forbids calling simulate() while a solve is in flight. A leftover
	// in-flight step can only happen when the async flag was flipped off
	// mid-flight or a sync was skipped — fetch defensively instead of dying.
	if (stepping) {
		WARN_PRINT_ONCE("PhysX: step() called with a solve in flight; fetching it first.");
		px_scene->fetchResults(true);
		stepping = false;
		_finish_step();
	}

	last_step = p_step;

	// Reset the debug-contact buffer for this step (onContact refills it).
	debug_contacts_count = 0;

	// Pre-step: apply constant forces, clear contact buffers, resolve area
	// gravity/damp overrides, and run custom integrators. Iterates the space's
	// own body registration list rather than querying the scene -- no per-step
	// getActors() round-trip, no 1024-actor cap, no userData round-trip.
	for (PhysXBody3D *body : bodies) {
		body->on_pre_step(p_step);
	}

	// Batched multi-fluid isosurface readback: with more than one fluid,
	// onPostSolve kicks each fluid's smoothing kernel WITHOUT syncing, and the
	// sync + outlier clamp + extraction happen in finish_isosurface_extraction()
	// after fetchResults -- syncing inline would serialize N GPU stalls, because
	// PhysX invokes each fluid's onPostSolve in turn. Single-fluid spaces keep
	// the inline path.
	batched_isosurface = fluids.size() > 1;
	for (PhysXGPUParticleFluid3D *fluid : fluids) {
		fluid->set_deferred_extraction(batched_isosurface);
	}

	// Vehicle pre-step: read state from the PhysX actor, apply commands, write
	// state back before simulate so the vehicle2 state is consistent during the step.
	for (PhysXVehicleServer *vehicle : vehicles) {
		vehicle->update(p_step);
	}

	// Synchronous mode (default): simulate + block in the same call. Async
	// mode (physics/physx_3d/simulation/async_step): return with the solve in
	// flight; sync() -- called by the engine at the START of the next tick,
	// before scripts -- does the fetch and the whole post-solve pipeline, so
	// the solve overlaps the rest of the frame. stepping marks the exact
	// simulate->fetch span: pre-step hooks run before it (queries there see
	// the idle scene), and _finish_step runs after it is cleared -- it issues
	// scene queries (CPU soft bodies), and a re-entrant sync() while stepping
	// is still set would fetch-and-recurse.
	stepping = true;
	// Record the simulating thread: ensure_synced() refuses to fetch from any
	// other thread (PhysX requires simulate/fetchResults on one thread).
	stepping_thread = Thread::get_caller_id();
	px_scene->simulate(p_step);
	if (!PhysXServer3D::get_singleton()->is_async_stepping()) {
		px_scene->fetchResults(true);
		stepping = false;
		_finish_step();
	}
}

void PhysXSpace3D::sync() {
	if (!stepping || !px_scene) {
		return;
	}
	px_scene->fetchResults(true);
	// Clear stepping BEFORE _finish_step: the post pipeline issues scene
	// queries (CPU soft bodies), and a re-entrant sync() while stepping is
	// still set would fetch-and-recurse.
	stepping = false;
	_finish_step();
}

// Safety net: a rigid actor being freed must not leave a live
// PxDeformableAttachment pointing at it. Walks the space's soft bodies and
// drops every attachment that references the actor.
void PhysXSpace3D::release_soft_body_attachments_for(physx::PxActor *p_actor) {
	for (PhysXSoftBody3D *sb : soft_bodies) {
		sb->release_attachments_for(p_actor);
	}
}

void PhysXSpace3D::ensure_synced() {
	if (!stepping || !px_scene) {
		return;
	}
	// Only the thread that called simulate() may fetchResults. A query or
	// mutation arriving from another thread must not touch the scene -- it
	// reads the last fetched state instead, and the physics thread fetches at
	// the next tick. The server-tick sync() path is exempt: the WrapMT sync
	// drains the command queue (physics thread idle by construction) before
	// fetching on the main thread.
	if (Thread::get_caller_id() != stepping_thread) {
		WARN_PRINT_ONCE(
				"PhysX: a query/mutation from a non-simulating thread raced an in-flight solve; "
				"using the last fetched state (fetchResults must stay on the simulating thread).");
		return;
	}
	sync();
}

// Everything after fetchResults, shared by the sync and async paths. Runs on
// the server thread with the solve complete.
void PhysXSpace3D::_finish_step() {
	// Mutations queued during the in-flight window (deferred calls, node
	// destruction): apply removes/adds against the now-idle scene and run any
	// deferred releases. Flush BEFORE the readbacks so a mid-flight-destroyed
	// GPU object is out of the picture first.
	_flush_actor_ops();

	// GPU fluid/cloth read-back: copy particle/vertex positions GPU -> host so
	// the nodes can render them. Must run while the scene is still valid (after
	// fetchResults, before the next simulate). In batched mode the finish pass
	// completes each fluid's deferred isosurface extraction first -- every
	// fluid's smoothing kernel was kicked during the solve, so the first sync
	// absorbs the whole batch.
	if (batched_isosurface) {
		for (PhysXGPUParticleFluid3D *fluid : fluids) {
			fluid->finish_isosurface_extraction();
		}
	}
	for (PhysXGPUParticleFluid3D *fluid : fluids) {
		fluid->read_back();
	}
	for (PhysXGPUCloth3D *cloth : cloths) {
		cloth->read_back();
	}
	// GPU soft bodies (PxDeformableVolume) simulated inside px_scene -- pull
	// their deformed state off the device. CPU soft bodies advance here, after
	// the rigid solve, so their per-vertex world query sees this step's final
	// rigid poses.
	for (PhysXSoftBody3D *sb : soft_bodies) {
		sb->read_back();
		Vector3 gravity = scene_gravity;
		if (!sb->is_gpu()) {
			// Reference parity (GodotSoftBody3D::predict_motion): CPU soft
			// bodies resolve area gravity overrides. GPU deformables keep the
			// scene gravity — the solver has no per-body gravity injection.
			gravity = _resolve_soft_body_gravity(*sb);
		}
		sb->step(last_step, gravity);
	}

	// Post-step sync, gated on actual activity: state-sync callbacks
	// and kinematic velocity derivation only run for bodies whose simulation
	// state changed this step -- the active actors, every kinematic (Godot
	// fires their state callback every tick, and AnimatableBody3D's platform
	// velocity must decay to zero when a moving platform stops), and the
	// bodies synced last step that are no longer active (they just fell
	// asleep; one final sync delivers the resting pose to the node).
	// Continuously sleeping bodies cost zero callbacks -- a RigidBody3D's
	// node sync only resumes when the body wakes.
	_finish_sync_bodies.clear();
	_finish_sync_set.clear();
	_finish_now_active.clear();
	LocalVector<PhysXBody3D *> &sync_bodies = _finish_sync_bodies;
	HashSet<PhysXBody3D *> &sync_set = _finish_sync_set;
	auto push_sync = [&](PhysXBody3D *body) {
		if (body && !sync_set.has(body)) {
			sync_set.insert(body);
			sync_bodies.push_back(body);
		}
	};
	LocalVector<PhysXBody3D *> &now_active = _finish_now_active;
	if (px_scene) {
		// Safe window for getSimulationStatistics: the solve is complete.
		physx::PxSimulationStatistics stats;
		px_scene->getSimulationStatistics(stats);
		cached_contact_pairs = (int)stats.nbDiscreteContactPairsTotal;

		physx::PxU32 nb_active = 0;
		physx::PxActor **active_actors = px_scene->getActiveActors(nb_active);
		active_objects = (int)nb_active;
		for (physx::PxU32 i = 0; i < nb_active; i++) {
			if (!active_actors[i] || !active_actors[i]->userData) {
				continue;
			}
			const PhysXActorUserData *ud = static_cast<const PhysXActorUserData *>(active_actors[i]->userData);
			if (ud->object && ud->object->get_type() == PhysXObject3D::OBJECT_TYPE_BODY) {
				PhysXBody3D *body = static_cast<PhysXBody3D *>(ud->object);
				push_sync(body);
				now_active.push_back(body);
			}
		}
	} else {
		active_objects = 0;
		cached_contact_pairs = 0;
	}
	for (PhysXBody3D *body : bodies) {
		if (body->get_mode() == PhysicsServer3D::BODY_MODE_KINEMATIC) {
			push_sync(body);
		}
	}
	for (PhysXBody3D *body : prev_active_bodies) {
		// Newly asleep: fire the callback once more so the node sees the
		// final resting pose. prev_active_bodies is replaced by the CURRENT
		// active set below, so this fires exactly once per sleep transition.
		push_sync(body);
	}
	prev_active_bodies = now_active;

	for (PhysXBody3D *body : sync_bodies) {
		body->on_post_step(last_step);
	}

	// Vehicle post-step (post-step): sync the PhysX actor pose/velocity from
	// vehicle2 state after the simulation step is complete.
	for (PhysXVehicleServer *vehicle : vehicles) {
		vehicle->post_step(last_step);
	}
}

void PhysXSpace3D::set_active(bool p_active) {
	active = p_active;
}

void PhysXSpace3D::add_actor(physx::PxActor *p_actor) {
	ERR_FAIL_NULL(p_actor);
	ERR_FAIL_NULL(px_scene);
	if (stepping) {
		// Async in-flight window: PhysX forbids scene mutation during the
		// solve. Apply right after the fetch (see _flush_actor_ops).
		pending_actor_ops.push_back({ p_actor, PendingActorOp::KIND_ADD });
		return;
	}
	px_scene->addActor(*p_actor);
}

void PhysXSpace3D::remove_actor(physx::PxActor *p_actor) {
	ERR_FAIL_NULL(p_actor);
	// Tolerate a null scene silently: during teardown a body/area destructor
	// may run after the space's PxScene was already released (e.g. the World3D
	// is freed before the body nodes). There's nothing to remove in that case.
	if (!px_scene) {
		return;
	}
	if (stepping) {
		pending_actor_ops.push_back({ p_actor, PendingActorOp::KIND_REMOVE });
		return;
	}
	px_scene->removeActor(*p_actor);
}

// Takes ownership of a PxActor release: while a solve is in flight the
// release is queued until after the fetch (the solve still references the
// actor), otherwise it runs inline. Wrappers that destroy their actor route
// through this so a mid-flight node free never releases into a live solve.
void PhysXSpace3D::release_actor(physx::PxActor *p_actor) {
	ERR_FAIL_NULL(p_actor);
	if (stepping && px_scene) {
		pending_actor_ops.push_back({ p_actor, PendingActorOp::KIND_RELEASE });
		return;
	}
	p_actor->release();
}

void PhysXSpace3D::_flush_actor_ops() {
	for (const PendingActorOp &op : pending_actor_ops) {
		if (op.kind == PendingActorOp::KIND_ADD && px_scene) {
			px_scene->addActor(*op.actor);
		} else if (op.kind == PendingActorOp::KIND_REMOVE && px_scene) {
			px_scene->removeActor(*op.actor);
		} else if (op.kind == PendingActorOp::KIND_RELEASE) {
			op.actor->release();
		}
	}
	pending_actor_ops.clear();
}

void PhysXSpace3D::set_param(PhysicsServer3D::SpaceParameter p_param, double p_value) {
	if (!px_scene) {
		return;
	}

	switch (p_param) {
		case PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS: {
			solver_iteration_count = (int)p_value;
			for (PhysXBody3D *body : bodies) {
				physx::PxRigidActor *actor = body->get_rigid_actor();
				if (actor && actor->is<physx::PxRigidDynamic>()) {
					physx::PxRigidDynamic *dyn = static_cast<physx::PxRigidDynamic *>(actor);
					dyn->setSolverIterationCounts(solver_iteration_count, solver_iteration_count);
				}
			}
		} break;
		case PhysicsServer3D::SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD: {
			sleep_threshold_linear = p_value;
			_refresh_body_sleep_policies();
		} break;
		case PhysicsServer3D::SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD: {
			sleep_threshold_angular = p_value;
			_refresh_body_sleep_policies();
		} break;
		case PhysicsServer3D::SPACE_PARAM_BODY_TIME_TO_SLEEP: {
			time_before_sleep = p_value;
			_refresh_body_sleep_policies();
		} break;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_RECYCLE_RADIUS: {
			// No PhysX counterpart: PhysX's contact-offset/bias machinery
			// supersedes Godot's pair-recycling knob. Cached for get_param.
			param_contact_recycle_radius = p_value;
			WARN_PRINT_ONCE("PhysX: CONTACT_RECYCLE_RADIUS is intentionally unmapped (cached for get_param only).");
		} break;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_SEPARATION: {
			// The PhysX-native knob for contact generation distance is the
			// per-shape contactOffset (driven by the shape margin); a
			// per-space separation has no clean mapping without disturbing
			// it. Cached for get_param only.
			param_contact_max_separation = p_value;
		} break;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION: {
			// Godot: contacts with penetration < allowed produce no
			// positional correction, so bodies rest at `allowed` penetration.
			// PhysX bodies rest at the SUM of a pair's rest offsets — apply
			// half per shape. Only an explicit set maps the parameter; the
			// Godot default (0.01) stays unmapped so default resting
			// behavior is unchanged. Shapes attached afterwards pick the
			// value up (see PhysXShapedObject3D's rest-offset application).
			param_contact_max_allowed_penetration = p_value;
			allowed_penetration_set = true;
			shape_rest_offset = MAX(0.0, p_value) * 0.5;
		} break;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_DEFAULT_BIAS: {
			// No PhysX counterpart: solver bias is a global solver property.
			// Cached for get_param only.
			param_contact_default_bias = p_value;
			WARN_PRINT_ONCE("PhysX: CONTACT_DEFAULT_BIAS is intentionally unmapped (cached for get_param only).");
		} break;
		default: {
			WARN_PRINT_ONCE("PhysX: unknown SpaceParameter, ignored.");
		} break;
	}
}

double PhysXSpace3D::get_param(PhysicsServer3D::SpaceParameter p_param) const {
	switch (p_param) {
		case PhysicsServer3D::SPACE_PARAM_SOLVER_ITERATIONS:
			return static_cast<double>(solver_iteration_count);
		case PhysicsServer3D::SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD:
			return sleep_threshold_linear;
		case PhysicsServer3D::SPACE_PARAM_BODY_ANGULAR_VELOCITY_SLEEP_THRESHOLD:
			return sleep_threshold_angular;
		case PhysicsServer3D::SPACE_PARAM_BODY_TIME_TO_SLEEP:
			return time_before_sleep;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_RECYCLE_RADIUS:
			return param_contact_recycle_radius;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_SEPARATION:
			return param_contact_max_separation;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION:
			return param_contact_max_allowed_penetration;
		case PhysicsServer3D::SPACE_PARAM_CONTACT_DEFAULT_BIAS:
			return param_contact_default_bias;
		default:
			return 0.0;
	}
}

void PhysXSpace3D::_refresh_body_sleep_policies() {
	for (PhysXBody3D *body : bodies) {
		body->refresh_sleep_policy();
	}
}

// Body / Area registration

void PhysXSpace3D::register_body(PhysXBody3D *p_body) {
	ERR_FAIL_NULL(p_body);

	if (bodies.find(p_body) != -1) {
		return;
	}

	bodies.push_back(p_body);

	// Apply cached solver iterations to newly registered bodies.
	physx::PxRigidActor *actor = p_body->get_rigid_actor();
	if (actor && actor->is<physx::PxRigidDynamic>()) {
		physx::PxRigidDynamic *dyn = static_cast<physx::PxRigidDynamic *>(actor);
		dyn->setSolverIterationCounts(solver_iteration_count, solver_iteration_count);
	}
	// Adopt this space's sleep policy (thresholds may differ from the previous
	// space, and the body was created before it had one).
	p_body->refresh_sleep_policy();
}

void PhysXSpace3D::unregister_body(PhysXBody3D *p_body) {
	ERR_FAIL_NULL(p_body);

	for (unsigned int i = 0; i < bodies.size(); i++) {
		if (bodies[i] == p_body) {
			bodies.remove_at(i);
			break;
		}
	}
	// Drop the body from the previous-active list too (it may be about to
	// die): prev_active_bodies must never hold a dangling wrapper pointer.
	const int64_t sync_idx = prev_active_bodies.find(p_body);
	if (sync_idx != -1) {
		prev_active_bodies.remove_at_unordered(sync_idx);
	}
}

void PhysXSpace3D::register_area(PhysXArea3D *p_area) {
	ERR_FAIL_NULL(p_area);

	if (areas.find(p_area) != -1) {
		return;
	}

	areas.push_back(p_area);
}

void PhysXSpace3D::unregister_area(PhysXArea3D *p_area) {
	ERR_FAIL_NULL(p_area);

	for (unsigned int i = 0; i < areas.size(); i++) {
		if (areas[i] == p_area) {
			areas.remove_at(i);
			return;
		}
	}
}

void PhysXSpace3D::register_articulation(PhysXArticulation3D *p_articulation) {
	ERR_FAIL_NULL(p_articulation);

	if (articulations.find(p_articulation) != -1) {
		return;
	}

	articulations.push_back(p_articulation);
}

void PhysXSpace3D::unregister_articulation(PhysXArticulation3D *p_articulation) {
	ERR_FAIL_NULL(p_articulation);

	for (unsigned int i = 0; i < articulations.size(); i++) {
		if (articulations[i] == p_articulation) {
			articulations.remove_at(i);
			return;
		}
	}
}

// Vehicle registration

void PhysXSpace3D::register_vehicle(PhysXVehicleServer *p_vehicle) {
	ERR_FAIL_NULL(p_vehicle);

	if (vehicles.find(p_vehicle) != -1) {
		return;
	}

	vehicles.push_back(p_vehicle);
}

void PhysXSpace3D::unregister_vehicle(PhysXVehicleServer *p_vehicle) {
	ERR_FAIL_NULL(p_vehicle);

	for (unsigned int i = 0; i < vehicles.size(); i++) {
		if (vehicles[i] == p_vehicle) {
			vehicles.remove_at(i);
			return;
		}
	}
}

// GPU fluid / cloth registration

void PhysXSpace3D::register_fluid(PhysXGPUParticleFluid3D *p_fluid) {
	ERR_FAIL_NULL(p_fluid);
	if (fluids.find(p_fluid) != -1) {
		return;
	}
	fluids.push_back(p_fluid);
}

void PhysXSpace3D::unregister_fluid(PhysXGPUParticleFluid3D *p_fluid) {
	ERR_FAIL_NULL(p_fluid);
	for (unsigned int i = 0; i < fluids.size(); i++) {
		if (fluids[i] == p_fluid) {
			fluids.remove_at(i);
			return;
		}
	}
}

void PhysXSpace3D::register_cloth(PhysXGPUCloth3D *p_cloth) {
	ERR_FAIL_NULL(p_cloth);
	if (cloths.find(p_cloth) != -1) {
		return;
	}
	cloths.push_back(p_cloth);
}

void PhysXSpace3D::unregister_cloth(PhysXGPUCloth3D *p_cloth) {
	ERR_FAIL_NULL(p_cloth);
	for (unsigned int i = 0; i < cloths.size(); i++) {
		if (cloths[i] == p_cloth) {
			cloths.remove_at(i);
			return;
		}
	}
}

physx::PxPhysics *PhysXSpace3D::get_px_physics() const {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	return server ? server->try_get_physics() : nullptr;
}

physx::PxCudaContextManager *PhysXSpace3D::get_px_cuda() const {
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	return server ? server->get_cuda_context() : nullptr;
}

// ---------------------------------------------------------------------------
// Deferred monitor/event dispatch.
//
// onTrigger/onContact run inside fetchResults() (physics thread, outside
// Godot's unlocked callback window); Area3D monitor callbacks must fire
// during flush_queries(), so onTrigger records TriggerEvents here and they
// dispatch there. The overlap-list bookkeeping stays in onTrigger -- the
// next step's gravity/damp resolution reads it; only the *Godot callbacks*
// are deferred.
// ---------------------------------------------------------------------------

void PhysXSpace3D::queue_trigger(const TriggerEvent &p_event) {
	pending_trigger_events.push_back(p_event);
}

void PhysXSpace3D::flush_pending_callbacks() {
	// Dispatch the trigger events queued during the last step. Reentrancy guard:
	// a monitor callback could in theory mutate the world, but it cannot re-enter
	// flush_queries() (it runs on the main thread, single-threaded server).
	flushing_callbacks = true;

	for (unsigned int i = 0; i < pending_trigger_events.size(); i++) {
		const TriggerEvent &ev = pending_trigger_events[i];
		// Skip events whose dispatching area was freed between step() and now.
		// The body/area identity is cached in the event, so nothing else is
		// dereferenced here — removal events MUST still dispatch after the
		// body/area has been freed (Godot emits body_exited on removal).
		if (!ev.trigger_area || areas.find(ev.trigger_area) == -1) {
			continue;
		}
		if (ev.is_area_vs_area) {
			ev.trigger_area->dispatch_area_monitor(
					ev.status,
					ev.other_area_rid,
					ev.other_area_id,
					ev.other_shape_idx,
					ev.area_shape_idx);
		} else {
			ev.trigger_area->dispatch_body_monitor(
					ev.status,
					ev.body_rid,
					ev.body_id,
					ev.other_shape_idx,
					ev.area_shape_idx);
		}
	}

	pending_trigger_events.clear();
	flushing_callbacks = false;
}

Vector3 PhysXSpace3D::_resolve_soft_body_gravity(const PhysXSoftBody3D &p_sb) const {
	// Mirror of godot_physics_3d GodotSoftBody3D::predict_motion's gravity
	// resolution: overlapping areas resolve by priority (highest first) with
	// the per-channel override modes, and the space's default area is the
	// additive fallback. The reference tracks true broadphase pairs; here the
	// soft body's world AABB vs each area's shape AABB approximates the overlap.
	const AABB bounds = p_sb.get_bounds();

	LocalVector<const PhysXArea3D *> overlapping;
	for (const PhysXArea3D *area : areas) {
		const physx::PxBounds3 pb = area->get_world_bounds();
		if (pb.isEmpty()) {
			continue;
		}
		const AABB area_bounds(
				Vector3(pb.minimum.x, pb.minimum.y, pb.minimum.z),
				Vector3(pb.maximum.x - pb.minimum.x, pb.maximum.y - pb.minimum.y, pb.maximum.z - pb.minimum.z));
		if (area_bounds.intersects(bounds)) {
			overlapping.push_back(area);
		}
	}

	// Insertion sort by priority ascending; iterate descending (highest first)
	// — same ordering the rigid-body pre-step uses.
	for (int i = 1; i < (int)overlapping.size(); i++) {
		const PhysXArea3D *key = overlapping[i];
		int j = i - 1;
		while (j >= 0 && overlapping[j]->get_priority() > key->get_priority()) {
			overlapping[j + 1] = overlapping[j];
			j--;
		}
		overlapping[j + 1] = key;
	}

	const Vector3 center = bounds.get_center();
	Vector3 gravity;
	bool gravity_done = false;
	for (int i = (int)overlapping.size() - 1; i >= 0 && !gravity_done; i--) {
		const PhysXArea3D *area = overlapping[i];
		gravity_done = physx_apply_area_override(gravity, area->get_gravity_override_mode(),
				[&] { return physx_area_gravity_at(*area, center); });
	}
	if (!gravity_done && default_area) {
		gravity += physx_area_gravity_at(*default_area, center);
	}
	return gravity;
}
