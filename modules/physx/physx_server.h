/**************************************************************************/
/*  physx_server.h                                                        */
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
 * @file physx_server.h
 * @brief The PhysX-backed PhysicsServer3D implementation.
 *
 * PhysXServer3D is the central hub that bridges Godot's PhysicsServer3D API
 * to NVIDIA PhysX 5.x. It owns the PhysX foundation/physics/PVD singletons,
 * the default material, and the RID owners that map Godot RIDs to internal
 * PhysX wrapper objects (PhysXShape3D, PhysXBody3D, PhysXSpace3D).
 *
 * Threading: PhysXServer3D itself is not thread-safe. When run on a separate
 * thread, it is wrapped by PhysicsServer3DWrapMT which serializes calls via
 * a command queue. The server does not need to know which mode it is in.
 * NOTE: the ClassDB-bound module APIs (the vehicle, articulation,
 * particle-fluid and cloth method families) bypass the wrapper's queue —
 * scripts call them directly on this inner server from the main thread.
 * They, the physics pipeline (step/sync/flush_queries/free) and the WrapMT
 * pump task's marshaled-command batches are all serialized by api_mutex
 * (the pump brackets via command_flush_begin/end); see the note above
 * api_mutex below and _bind_methods in physx_server.cpp.
 */

#ifndef PHYSX_SERVER_H
#define PHYSX_SERVER_H

#include "physx_rid_owner.h"
#include "servers/physics_3d/physics_server_3d.h"

#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include <PxPhysicsAPI.h>
#include <core/templates/rid_owner.h>

class PhysXShape3D;
class PhysXBody3D;
class PhysXSpace3D;
class PhysXArea3D;
class PhysXJoint3D;
class PhysXVehicleServer;
class PhysXSoftBody3D;
class PhysXGPUCloth3D;
class PhysXGPUParticleFluid3D;
class PhysXArticulation3D;
namespace physx {
class PxFoundation;
class PxPhysics;
class PxPvd;
class PxPvdTransport;
class PxDefaultCpuDispatcher;
class PxCudaContextManager;
} //namespace physx

class PhysXServer3D : public PhysicsServer3D {
	GDCLASS(PhysXServer3D, PhysicsServer3D);

	static PhysXServer3D *singleton_ptr;
	// ------------------------------------------------------------------
	// RID Owners — map Godot RIDs to internal wrapper objects.
	// The `true` template parameter enables thread-safe atomics.
	// ------------------------------------------------------------------
	mutable RID_PtrOwner<PhysXShape3D, true> shape_owner;
	mutable RID_PtrOwner<PhysXBody3D, true> body_owner{ 65536, 1048576 };
	mutable RID_PtrOwner<PhysXSpace3D, true> space_owner;
	mutable RID_PtrOwner<PhysXArea3D, true> area_owner;
	mutable RID_PtrOwner<PhysXJoint3D, true> joint_owner;
	mutable RID_PtrOwner<PhysXVehicleServer, true> vehicle_owner;
	mutable RID_PtrOwner<PhysXSoftBody3D, true> soft_body_owner;
	mutable RID_PtrOwner<PhysXGPUParticleFluid3D, true> fluid_owner;
	mutable RID_PtrOwner<PhysXGPUCloth3D, true> cloth_owner;
	mutable RID_PtrOwner<PhysXArticulation3D, true> articulation_owner;
	// ------------------------------------------------------------------
	// API guard — serializes the ClassDB-bound module APIs (called directly
	// on this inner server from the main thread, bypassing WrapMT) against
	// the physics pipeline (step/sync/flush_queries/free) AND against the
	// WrapMT pump task's marshaled-command batches (bracketed via
	// command_flush_begin/end). This fork's Mutex is recursive: step/flush
	// dispatch script Callables that may legally call back into these APIs
	// on the same thread.
	// ------------------------------------------------------------------
	mutable Mutex api_mutex;
	// ------------------------------------------------------------------
	// PhysX SDK singletons (created in init(), released in finish()).
	// ------------------------------------------------------------------
	physx::PxCookingParams px_cooking_params{ physx::PxTolerancesScale() };
	physx::PxFoundation *px_foundation = nullptr;
	physx::PxPhysics *px_physics = nullptr;
	physx::PxPvd *px_debugger = nullptr;
	physx::PxPvdTransport *px_pvd_transport = nullptr;
	physx::PxMaterial *px_default_material = nullptr;
	// One CPU dispatcher shared by every PxScene the server creates (PhysX's
	// recommended setup — per-scene dispatchers multiply worker threads).
	physx::PxDefaultCpuDispatcher *px_cpu_dispatcher = nullptr;
	// Valid only when the build has GPU support (GODOT_PHYSX_GPU) AND a usable
	// CUDA device was found at init(). null otherwise.
	physx::PxCudaContextManager *px_cuda_context = nullptr;
	bool gpu_dynamics_enabled = false;

	bool extensions_initialized = false;
	bool vehicle_sdk_initialized = false;

	// Server-global simulation gate. Independent of per-space activation
	// (space_set_active): the editor sets this to false on startup so physics
	// stays frozen until the game runs. Mirrors Jolt/godot_physics_3d.
	bool active = true;

	// Optional async stepping (physics/physx_3d/simulation/async_step, read
	// live per server step): step() kicks the solve, sync() fetches it.
	bool async_stepping = false;

private:
	/** @brief Creates a shape of type T, registers it, and returns its RID. */
	template <typename T>
	RID _make_shape();

	static void _bind_methods();

	/// Live list of space RIDs, maintained on space create/free. The per-frame
	/// server loops (step/sync/flush_queries/process info) used to call
	/// space_owner.get_owned_list(), which returns a LocalVector by value —
	/// one heap copy per call, several per frame.
	LocalVector<RID> _space_list;

	/// One-shot warning tripwire: module extension APIs (vehicles, GPU
	/// fluids/cloths, articulations, water/flow probes) are serialized against
	/// the pipeline and the WrapMT marshaled-command batches by api_mutex (see
	/// command_flush_begin), but under physics/physx_3d/simulation/async_step
	/// a module mutation from the main thread cannot fetch a solve the pump
	/// thread kicked (PhysX requires simulate/fetchResults on one thread) —
	/// it proceeds against the last fetched state and warns once. Keep async
	/// stepping off for projects that mutate module objects from scripts.
	/// Called from the module API creation entry points.
	void _warn_module_api_separate_thread();

public:
	PhysXServer3D() { singleton_ptr = this; }
	~PhysXServer3D() = default;

	static PhysXServer3D *get_singleton() { return singleton_ptr; }

	/// Raw space lookup by RID (World3D::get_space()) for scene-level nodes
	/// that need this module's PxScene/PxPhysics directly -- the node-level
	/// vehicle stack borrows the simulation this way (PhysXVehicle3D & co).
	/// Returns null when p_space_rid is not a PhysX space (i.e. another
	/// physics backend is active).
	PhysXSpace3D *get_space(RID p_space_rid) const { return space_owner.get_or_null(p_space_rid); }

	// --- PhysX SDK accessors (used by shapes, bodies, spaces) ---
	physx::PxPhysics &get_physics() const;
	physx::PxPhysics *try_get_physics() const { return px_physics; }
	const physx::PxCookingParams &get_cooking_params() const { return px_cooking_params; }
	physx::PxMaterial *get_default_material() const { return px_default_material; }
	/// Shared CPU dispatcher handed to every PxScene (null before init()).
	physx::PxDefaultCpuDispatcher *get_cpu_dispatcher() const { return px_cpu_dispatcher; }
	/// CUDA context when GPU dynamics is available (GODOT_PHYSX_GPU build +
	/// usable device + determinism not forced); null otherwise.
	physx::PxCudaContextManager *get_cuda_context() const { return px_cuda_context; }
	bool is_gpu_dynamics_enabled() const { return gpu_dynamics_enabled; }

	// ------------------------------------------------------------------
	// SHAPE API — collision geometry resources (shared, not per-body).
	// Each create() returns a RID registered in shape_owner.
	// ------------------------------------------------------------------
	virtual RID world_boundary_shape_create() override;
	virtual RID separation_ray_shape_create() override;
	virtual RID sphere_shape_create() override;
	virtual RID box_shape_create() override;
	virtual RID capsule_shape_create() override;
	virtual RID cylinder_shape_create() override;
	virtual RID convex_polygon_shape_create() override;
	virtual RID concave_polygon_shape_create() override;
	virtual RID heightmap_shape_create() override;
	virtual RID custom_shape_create() override;

	virtual void shape_set_data(RID p_shape, const Variant &p_data) override;
	virtual Variant shape_get_data(RID p_shape) const override;

	virtual void shape_set_margin(RID p_shape, real_t p_margin) override;
	virtual real_t shape_get_margin(RID p_shape) const override;

	virtual PhysicsServer3D::ShapeType shape_get_type(RID p_shape) const override;

	virtual void shape_set_custom_solver_bias(RID p_shape, real_t p_bias) override;
	virtual real_t shape_get_custom_solver_bias(RID p_shape) const override;

	// ------------------------------------------------------------------
	// SPACE API — a physics world (one PxScene per space).
	// ------------------------------------------------------------------
	virtual RID space_create() override;

	virtual void space_set_active(RID p_space, bool p_active) override;
	virtual bool space_is_active(RID p_space) const override;

	virtual void space_set_param(RID p_space, PhysicsServer3D::SpaceParameter p_param, real_t p_value) override;
	virtual real_t space_get_param(RID p_space, PhysicsServer3D::SpaceParameter p_param) const override;

	virtual PhysicsDirectSpaceState3D *space_get_direct_state(RID p_space) override;

	virtual void space_set_debug_contacts(RID p_space, int p_max_contacts) override;
	virtual PackedVector3Array space_get_contacts(RID p_space) const override;
	virtual int space_get_contact_count(RID p_space) const override;

	// ------------------------------------------------------------------
	// AREA API — trigger volumes (kinematic actor + trigger shape [+ detection
	// shape when monitorable — see REG-0011]).
	// ------------------------------------------------------------------
	virtual RID area_create() override;

	virtual void area_set_space(RID p_area, RID p_space) override;
	virtual RID area_get_space(RID p_area) const override;

	virtual void area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform, bool p_disabled) override;

	virtual void area_set_shape(RID p_area, int p_shape_idx, RID p_shape) override;
	virtual RID area_get_shape(RID p_area, int p_shape_idx) const override;

	virtual void area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) override;
	virtual Transform3D area_get_shape_transform(RID p_area, int p_shape_idx) const override;

	virtual void area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) override;

	virtual int area_get_shape_count(RID p_area) const override;

	virtual void area_remove_shape(RID p_area, int p_shape_idx) override;
	virtual void area_clear_shapes(RID p_area) override;

	virtual void area_attach_object_instance_id(RID p_area, ObjectID p_id) override;
	virtual ObjectID area_get_object_instance_id(RID p_area) const override;

	virtual void area_set_param(RID p_area, PhysicsServer3D::AreaParameter p_param, const Variant &p_value) override;
	virtual Variant area_get_param(RID p_area, PhysicsServer3D::AreaParameter p_param) const override;

	virtual void area_set_transform(RID p_area, const Transform3D &p_transform) override;
	virtual Transform3D area_get_transform(RID p_area) const override;

	virtual void area_set_collision_layer(RID p_area, uint32_t p_layer) override;
	virtual uint32_t area_get_collision_layer(RID p_area) const override;

	virtual void area_set_collision_mask(RID p_area, uint32_t p_mask) override;
	virtual uint32_t area_get_collision_mask(RID p_area) const override;

	virtual void area_set_monitorable(RID p_area, bool p_monitorable) override;

	virtual void area_set_ray_pickable(RID p_area, bool p_enable) override;

	virtual void area_set_monitor_callback(RID p_area, const Callable &p_callback) override;
	virtual void area_set_area_monitor_callback(RID p_area, const Callable &p_callback) override;

	// ------------------------------------------------------------------
	// BODY API — dynamic/static/kinematic rigid bodies.
	// Each setter resolves the RID → PhysXBody3D* and forwards the call.
	// ------------------------------------------------------------------
	PhysXBody3D *get_body(RID p_rid) const;
	PhysXArea3D *get_area(RID p_rid) const;
	PhysXShape3D *get_shape(RID p_rid) const;
	PhysXJoint3D *get_joint(RID p_rid) const;
	PhysXVehicleServer *get_vehicle(RID p_rid) const;
	virtual RID body_create() override;

	virtual void body_set_space(RID p_body, RID p_space) override;
	virtual RID body_get_space(RID p_body) const override;

	virtual void body_set_mode(RID p_body, PhysicsServer3D::BodyMode p_mode) override;
	virtual PhysicsServer3D::BodyMode body_get_mode(RID p_body) const override;

	virtual void body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform, bool p_disabled) override;

	virtual void body_set_shape(RID p_body, int p_shape_idx, RID p_shape) override;
	virtual RID body_get_shape(RID p_body, int p_shape_idx) const override;

	virtual void body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) override;
	virtual Transform3D body_get_shape_transform(RID p_body, int p_shape_idx) const override;

	virtual void body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) override;

	virtual int body_get_shape_count(RID p_body) const override;

	virtual void body_remove_shape(RID p_body, int p_shape_idx) override;
	virtual void body_clear_shapes(RID p_body) override;

	virtual void body_attach_object_instance_id(RID p_body, ObjectID p_id) override;
	virtual ObjectID body_get_object_instance_id(RID p_body) const override;

	virtual void body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) override;
	virtual bool body_is_continuous_collision_detection_enabled(RID p_body) const override;

	virtual void body_set_collision_layer(RID p_body, uint32_t p_layer) override;
	virtual uint32_t body_get_collision_layer(RID p_body) const override;

	virtual void body_set_collision_mask(RID p_body, uint32_t p_mask) override;
	virtual uint32_t body_get_collision_mask(RID p_body) const override;

	virtual void body_set_collision_priority(RID p_body, real_t p_priority) override;
	virtual real_t body_get_collision_priority(RID p_body) const override;

	virtual void body_set_user_flags(RID p_body, uint32_t p_flags) override;
	virtual uint32_t body_get_user_flags(RID p_body) const override;

	virtual void body_set_param(RID p_body, PhysicsServer3D::BodyParameter p_param, const Variant &p_value) override;
	virtual Variant body_get_param(RID p_body, PhysicsServer3D::BodyParameter p_param) const override;

	virtual void body_reset_mass_properties(RID p_body) override;

	virtual void body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) override;
	virtual Variant body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const override;

	virtual void body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) override;
	virtual void body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position) override;
	virtual void body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) override;

	virtual void body_apply_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void body_apply_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_add_constant_central_force(RID p_body, const Vector3 &p_force) override;
	virtual void body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) override;
	virtual void body_add_constant_torque(RID p_body, const Vector3 &p_torque) override;

	virtual void body_set_constant_force(RID p_body, const Vector3 &p_force) override;
	virtual Vector3 body_get_constant_force(RID p_body) const override;

	virtual void body_set_constant_torque(RID p_body, const Vector3 &p_torque) override;
	virtual Vector3 body_get_constant_torque(RID p_body) const override;

	virtual void body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) override;

	virtual void body_set_axis_lock(RID p_body, PhysicsServer3D::BodyAxis p_axis, bool p_lock) override;
	virtual bool body_is_axis_locked(RID p_body, PhysicsServer3D::BodyAxis p_axis) const override;

	virtual void body_add_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void body_remove_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void body_set_max_contacts_reported(RID p_body, int p_amount) override;
	virtual int body_get_max_contacts_reported(RID p_body) const override;

	virtual void body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) override;
	virtual real_t body_get_contacts_reported_depth_threshold(RID p_body) const override;

	virtual void body_set_omit_force_integration(RID p_body, bool p_enable) override;
	virtual bool body_is_omitting_force_integration(RID p_body) const override;

	virtual void body_set_state_sync_callback(RID p_body, const Callable &p_callable) override;
	virtual void body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_userdata) override;

	virtual void body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual bool body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result) override;

	virtual PhysicsDirectBodyState3D *body_get_direct_state(RID p_body) override;

	// ------------------------------------------------------------------
	// VEHICLE API — vehicle2 wrapper.
	//   vehicle_create           make a vehicle RID of the given archetype.
	//   vehicle_set_chassis_body attach a dynamic body as the chassis.
	//   vehicle_set_space        set/change the vehicle's space membership.
	// Teardown is handled by free(RID) (release + memdelete) and finish()
	// (frees vehicles before bodies — they borrow the chassis PxRigidDynamic).
	// ------------------------------------------------------------------
	// Module extension: per-body soft-body solver-path override
	// (-1 follow project setting, 0 Auto, 1 CPU, 2 GPU).
	void soft_body_set_solver_mode(RID p_body, int p_mode);
	int soft_body_get_solver_mode(RID p_body) const;

	// Module extension: rigid attachment of a soft-body render vertex to a
	// rigid body (PxDeformableAttachment, GPU solver path — GAP-15).
	void soft_body_attach_point_to_body(RID p_body, int p_point_index, RID p_rigid_body);
	void soft_body_detach_point_from_body(RID p_body, int p_point_index);

	RID vehicle_create(int p_archetype);
	void vehicle_set_chassis_body(RID p_vehicle, RID p_body);
	void vehicle_set_space(RID p_vehicle, RID p_space);

	// Wheel configuration. Wheels are configured AFTER the chassis is attached
	// (the per-wheel param arrays live in the vehicle2 state created at adopt()).
	int vehicle_get_wheel_count(RID p_vehicle) const;
	void vehicle_set_wheel_count(RID p_vehicle, int p_count);
	int vehicle_add_wheel(RID p_vehicle);
	void vehicle_set_wheel_params(RID p_vehicle, int p_idx, const Dictionary &p_params);

	// Control + EngineDrive config (Phase 8).
	void vehicle_set_control_inputs(RID p_vehicle, real_t p_throttle, real_t p_brake, real_t p_steer, real_t p_handbrake);
	void vehicle_set_gear_command(RID p_vehicle, int p_gear);
	void vehicle_set_response_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_anti_roll_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_engine_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_clutch_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_gearbox_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_autobox_params(RID p_vehicle, const Dictionary &p_params);
	void vehicle_set_differential_params(RID p_vehicle, const Dictionary &p_params);

	// DirectDrive per-wheel control (Phase 9).
	void vehicle_set_wheel_drive_torque(RID p_vehicle, int p_idx, real_t p_torque);
	void vehicle_set_wheel_brake_torque(RID p_vehicle, int p_idx, real_t p_torque);
	void vehicle_set_wheel_steer_angle(RID p_vehicle, int p_idx, real_t p_angle);

	// Telemetry readback (Phase 10). Polled; valid after the step.
	Array vehicle_get_wheel_states(RID p_vehicle) const;
	Dictionary vehicle_get_engine_state(RID p_vehicle) const;

	// Ackermann steering geometry (percent blend + optional wheelbase/track
	// override) and the per-wheel road-wheel steer angles it resolves to (rad).
	void vehicle_set_ackermann_params(RID p_vehicle, const Dictionary &p_params);
	PackedFloat32Array vehicle_get_wheel_steer_angles(RID p_vehicle) const;

	// 2-wheeler roll-balance assist (lean stabilization) + its telemetry.
	void vehicle_set_balance_params(RID p_vehicle, const Dictionary &p_params);
	Dictionary vehicle_get_balance_state(RID p_vehicle) const;
	Dictionary vehicle_get_anti_roll_params(RID p_vehicle) const;
	Dictionary vehicle_get_response_params(RID p_vehicle) const;

	// ------------------------------------------------------------------
	// ARTICULATION API — SKELETON for PhysX reduced-coordinate articulations.
	// See objects/physx_articulation_3d.h for the enum conventions and the
	// not-yet-exposed feature list (tendons, mimics, link velocity queries).
	// ------------------------------------------------------------------
	RID articulation_create();
	void articulation_set_space(RID p_articulation, RID p_space);
	/// parent_index -1 creates the base link; returns the new link index.
	int articulation_add_link(RID p_articulation, int p_parent_index,
			const Transform3D &p_parent_frame, const Transform3D &p_child_frame,
			int p_joint_type, float p_density, const Vector3 &p_box_half_extents);
	void articulation_set_drive(RID p_articulation, int p_link_index, int p_axis,
			float p_stiffness, float p_damping, float p_drive_target,
			float p_drive_velocity, int p_drive_type);
	void articulation_set_limit(RID p_articulation, int p_link_index, int p_axis,
			float p_low, float p_high);
	void articulation_set_fix_base(RID p_articulation, bool p_fix);
	void articulation_wake(RID p_articulation);
	void articulation_sleep(RID p_articulation);
	int articulation_get_link_count(RID p_articulation) const;
	Transform3D articulation_get_link_transform(RID p_articulation, int p_link_index) const;
	bool articulation_is_sleeping(RID p_articulation) const;

	/// Replaces a link's collision shape with a per-link instance of the
	/// shared PhysXShape3D blueprint (concave shapes degrade to query-only
	/// per REG-0014; mass recomputed at the link's stored density).
	void articulation_set_link_shape(RID p_articulation, int p_link_index, RID p_shape, const Transform3D &p_transform);
	/// Per-link collision filtering (shape filter data word0/word1).
	void articulation_set_link_collision_layer(RID p_articulation, int p_link_index, uint32_t p_layer);
	void articulation_set_link_collision_mask(RID p_articulation, int p_link_index, uint32_t p_mask);
	uint32_t articulation_get_link_collision_layer(RID p_articulation, int p_link_index) const;
	uint32_t articulation_get_link_collision_mask(RID p_articulation, int p_link_index) const;
	/// Linear + angular world velocity of a link ("linear"/"angular" keys).
	Dictionary articulation_get_link_velocity(RID p_articulation, int p_link_index) const;

	// ------------------------------------------------------------------
	// SOFT BODY API — stock SoftBody3D. Each body resolves to a GPU
	// PxDeformableVolume (CUDA) or the CPU XPBD solver — see
	// objects/physx_soft_body_3d.h.
	// ------------------------------------------------------------------
	virtual RID soft_body_create() override;

	virtual void soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) override;

	virtual void soft_body_set_space(RID p_body, RID p_space) override;
	virtual RID soft_body_get_space(RID p_body) const override;

	virtual void soft_body_set_ray_pickable(RID p_body, bool p_enable) override;

	virtual void soft_body_set_collision_layer(RID p_body, uint32_t p_layer) override;
	virtual uint32_t soft_body_get_collision_layer(RID p_body) const override;

	virtual void soft_body_set_collision_mask(RID p_body, uint32_t p_mask) override;
	virtual uint32_t soft_body_get_collision_mask(RID p_body) const override;

	virtual void soft_body_add_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void soft_body_remove_collision_exception(RID p_body, RID p_excepted_body) override;
	virtual void soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) override;

	virtual void soft_body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) override;
	virtual Variant soft_body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const override;

	virtual void soft_body_set_transform(RID p_body, const Transform3D &p_transform) override;

	virtual void soft_body_apply_point_impulse(RID p_body, int p_point_index, const Vector3 &p_impulse) override;
	virtual void soft_body_apply_point_force(RID p_body, int p_point_index, const Vector3 &p_force) override;
	virtual void soft_body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) override;
	virtual void soft_body_apply_central_force(RID p_body, const Vector3 &p_force) override;

	/// True when this soft body runs the GPU deformable path (and is therefore
	/// queryable); false on the CPU XPBD path.
	bool soft_body_is_gpu(RID p_body) const;

	virtual void soft_body_set_simulation_precision(RID p_body, int p_precision) override;
	virtual int soft_body_get_simulation_precision(RID p_body) const override;

	virtual void soft_body_set_total_mass(RID p_body, real_t p_total_mass) override;
	virtual real_t soft_body_get_total_mass(RID p_body) const override;

	virtual void soft_body_set_linear_stiffness(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_linear_stiffness(RID p_body) const override;

	virtual void soft_body_set_shrinking_factor(RID p_body, real_t p_shrinking_factor) override;
	virtual real_t soft_body_get_shrinking_factor(RID p_body) const override;

	virtual void soft_body_set_pressure_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_pressure_coefficient(RID p_body) const override;

	virtual void soft_body_set_damping_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_damping_coefficient(RID p_body) const override;

	virtual void soft_body_set_drag_coefficient(RID p_body, real_t p_coefficient) override;
	virtual real_t soft_body_get_drag_coefficient(RID p_body) const override;

	virtual void soft_body_set_mesh(RID p_body, RID p_mesh) override;

	virtual AABB soft_body_get_bounds(RID p_body) const override;

	virtual void soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) override;

	virtual Vector3 soft_body_get_point_global_position(RID p_body, int p_point_index) const override;

	virtual void soft_body_remove_all_pinned_points(RID p_body) override;

	virtual void soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) override;
	virtual bool soft_body_is_point_pinned(RID p_body, int p_point_index) const override;

	// ------------------------------------------------------------------
	// PARTICLE FLUID API — module extension (not part of PhysicsServer3D).
	// GPU PBD fluid simulated by PhysXParticleFluid3D via get_singleton().
	// Inert unless the build has GPU support and a CUDA device is present.
	// ------------------------------------------------------------------
	RID particle_fluid_create();
	/// True when this platform's SDK can create the CUDA PBD particle buffer.
	/// The diffuse-particle factory ships only in the GPU-enabled Windows
	/// libraries through PhysX 5.11 (stubbed elsewhere, see
	/// objects/physx_extgpu_stub_linux.cpp), so false means the
	/// PhysXParticleFluid3D node's Auto rule resolves to its MPM solver and an
	/// explicitly pinned PBD fluid stays inert.
	bool particle_fluid_supports_diffuse() const;
	void particle_fluid_set_space(RID p_fluid, RID p_space);
	void particle_fluid_set_param(RID p_fluid, int p_param, real_t p_value);
	void particle_fluid_set_capacity(RID p_fluid, int p_max);
	// Runs the PBD system as Drucker-Prager grains instead of a liquid
	// (PhysXGranular3D's explicit PBD mode). p_friction is a friction
	// coefficient (~tan of the repose angle).
	void particle_fluid_set_granular(RID p_fluid, bool p_enabled, real_t p_friction);
	void particle_fluid_set_particles(RID p_fluid, const Vector<Vector3> &p_positions, const Vector3 &p_initial_velocity);
	void particle_fluid_emit(RID p_fluid, const Vector<Vector3> &p_positions, const Vector3 &p_velocity);
	void particle_fluid_clear(RID p_fluid);
	void particle_fluid_set_foam(RID p_fluid, bool p_enabled, int p_capacity, real_t p_lifetime, real_t p_threshold, real_t p_buoyancy, real_t p_size);
	Vector<Vector3> particle_fluid_get_positions(RID p_fluid) const;
	Vector<Vector3> particle_fluid_get_foam_positions(RID p_fluid) const;
	int particle_fluid_get_particle_count(RID p_fluid) const;
	int particle_fluid_get_foam_count(RID p_fluid) const;

	void particle_fluid_set_surface_mesh(RID p_fluid, bool p_enabled);
	/// Triangle count of the fluid's latest isosurface (script-facing probe).
	int particle_fluid_get_surface_triangle_count(RID p_fluid) const;
	void particle_fluid_set_surface_anisotropy(RID p_fluid, bool p_enabled);
	// Fills Godot arrays with the latest GPU isosurface; returns the triangle count.
	// r_version: pass the last-seen version; returns -1 (leaving r_* untouched) if
	// the mesh is unchanged, else the triangle count and the new version.
	int particle_fluid_get_surface_mesh(RID p_fluid, PackedVector3Array &r_vertices, PackedVector3Array &r_normals, PackedInt32Array &r_indices, uint32_t &r_version) const;
	// Same, for the coarse foam isosurface layer (diffuse particles).
	int particle_fluid_get_foam_mesh(RID p_fluid, PackedVector3Array &r_vertices, PackedVector3Array &r_normals, PackedInt32Array &r_indices, uint32_t &r_version) const;
	real_t particle_fluid_get_submersion(RID p_fluid, const AABB &p_world_aabb) const;

	// ------------------------------------------------------------------
	// GPU CLOTH API — module extension. PhysX 5 PxDeformableSurface on CUDA.
	// Returns RID() when no CUDA device is available — the PhysXCloth3D node
	// then uses its CPU XPBD fallback.
	// ------------------------------------------------------------------
	RID cloth_create();
	void cloth_set_space(RID p_cloth, RID p_space);
	void cloth_set_params(RID p_cloth, real_t p_thickness, real_t p_density, real_t p_stretch, real_t p_bend, real_t p_damping, uint32_t p_collision_mask);
	void cloth_set_collision_layer_and_mask(RID p_cloth, uint32_t p_layer, uint32_t p_mask);
	void cloth_add_collision_exception(RID p_cloth, RID p_body);
	void cloth_remove_collision_exception(RID p_cloth, RID p_body);
	void cloth_build(RID p_cloth, const Vector<Vector3> &p_positions, const Vector<int32_t> &p_indices, const Transform3D &p_xform);
	void cloth_set_pinned(RID p_cloth, const Vector<int32_t> &p_pinned);
	void cloth_set_pin_targets(RID p_cloth, const Vector<Vector3> &p_world_targets);
	void cloth_apply_wind(RID p_cloth, const Vector3 &p_wind, real_t p_drag, real_t p_lift, real_t p_dt);
	bool cloth_is_ready(RID p_cloth) const;
	int cloth_get_mesh(RID p_cloth, PackedVector3Array &r_positions, PackedInt32Array &r_indices, uint32_t &r_version) const;

	// ------------------------------------------------------------------
	// JOINT API — constraints between bodies
	// ------------------------------------------------------------------
	virtual RID joint_create() override;

	virtual void joint_clear(RID p_joint) override;

	virtual void joint_make_pin(RID p_joint, RID p_body_a, const Vector3 &p_local_a, RID p_body_b, const Vector3 &p_local_b) override;

	virtual void pin_joint_set_param(RID p_joint, PhysicsServer3D::PinJointParam p_param, real_t p_value) override;
	virtual real_t pin_joint_get_param(RID p_joint, PhysicsServer3D::PinJointParam p_param) const override;

	virtual void pin_joint_set_local_a(RID p_joint, const Vector3 &p_local_a) override;
	virtual Vector3 pin_joint_get_local_a(RID p_joint) const override;

	virtual void pin_joint_set_local_b(RID p_joint, const Vector3 &p_local_b) override;
	virtual Vector3 pin_joint_get_local_b(RID p_joint) const override;

	virtual void joint_make_hinge(RID p_joint, RID p_body_a, const Transform3D &p_hinge_a, RID p_body_b, const Transform3D &p_hinge_b) override;

	virtual void joint_make_hinge_simple(RID p_joint, RID p_body_a, const Vector3 &p_pivot_a, const Vector3 &p_axis_a, RID p_body_b, const Vector3 &p_pivot_b, const Vector3 &p_axis_b) override;

	virtual void hinge_joint_set_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param, real_t p_value) override;
	virtual real_t hinge_joint_get_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param) const override;

	virtual void hinge_joint_set_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled) override;
	virtual bool hinge_joint_get_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag) const override;

	virtual void joint_make_slider(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void slider_joint_set_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param, real_t p_value) override;
	virtual real_t slider_joint_get_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param) const override;

	virtual void joint_make_cone_twist(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void cone_twist_joint_set_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param, real_t p_value) override;
	virtual real_t cone_twist_joint_get_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param) const override;

	virtual void joint_make_generic_6dof(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) override;

	virtual void generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) override;
	virtual real_t generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const override;

	virtual void generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) override;
	virtual bool generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const override;

	virtual PhysicsServer3D::JointType joint_get_type(RID p_joint) const override;

	virtual void joint_set_solver_priority(RID p_joint, int p_priority) override;
	virtual int joint_get_solver_priority(RID p_joint) const override;

	virtual void joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) override;
	virtual bool joint_is_disabled_collisions_between_bodies(RID p_joint) const override;

	// ------------------------------------------------------------------
	// LIFECYCLE & SIMULATION — init/finish PhysX, step all spaces.
	// ------------------------------------------------------------------
	virtual void free(RID p_rid) override;

	// --- Internal helpers (called by bodies/joints/vehicles during lifecycle) ---
	/// Releases all vehicles that have the given body as chassis before
	/// freeing it. Called by free() to prevent dangling chassis pointers.
	void release_vehicles_for_body(PhysXBody3D *p_body) const;
	/// Drops all vehicles' "surface_frictions" grip-table entries that were
	/// resolved from the given body's material. Called by free() before the
	/// body (and its PxMaterial) is destroyed — non-chassis ground bodies are
	/// referenced by the grip table without any other lifetime link.
	void invalidate_vehicles_surface_pairs(PhysXBody3D *p_body) const;

	virtual void set_active(bool p_active) override;

	/// Whether step() defers the solve fetch to sync() (async stepping).
	bool is_async_stepping() const { return async_stepping; }

	virtual void init() override;
	virtual void finish() override;

	virtual void step(real_t p_step) override;

	virtual void sync() override;
	virtual void end_sync() override;

	virtual void flush_queries() override;
	virtual bool is_flushing_queries() const override;

	/// Serializes the WrapMT pump task's marshaled-command batches against the
	/// ClassDB-bound module APIs (which take api_mutex on the main thread).
	/// Together with the api_mutex that step/sync/flush_queries and the module
	/// APIs already hold, this closes the last unsynchronized path: previously
	/// marshaled commands could interleave with a module API mid-call under
	/// physics/3d/run_on_separate_thread. The recursive mutex keeps script
	/// callbacks fired from flush_queries() legal.
	virtual void command_flush_begin() override { api_mutex.lock(); }
	virtual void command_flush_end() override { api_mutex.unlock(); }

	virtual int get_process_info(PhysicsServer3D::ProcessInfo p_process_info) override;
};
#endif // PHYSX_SERVER_H
