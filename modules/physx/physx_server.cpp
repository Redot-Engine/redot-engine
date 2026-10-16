/**************************************************************************/
/*  physx_server.cpp                                                      */
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
 * @file physx_server.cpp
 * @brief Implementation of PhysXServer3D —the PhysX-backed PhysicsServer3D.
 */

#if defined(__linux__) && defined(GODOT_PHYSX_GPU)
#include <dlfcn.h> // Godot patch: preload libcuda so the GPU runtime's cu* symbols resolve (see the GPU init below).
#endif

#include "joints/physx_joint_3d.h"
#include "objects/physx_area_3d.h"
#include "objects/physx_articulation_3d.h"
#include "objects/physx_body_3d.h"
#include "objects/physx_direct_body_state_3d.h"
#include "objects/physx_gpu_cloth_3d.h"
#include "objects/physx_gpu_particle_fluid_3d.h"
#include "objects/physx_soft_body_3d.h"
#include "physx_project_settings.h"
#include "physx_server.h"
#include "shapes/physx_box_shape_3d.h"
#include "shapes/physx_capsule_shape_3d.h"
#include "shapes/physx_concave_polygon_shape_3d.h"
#include "shapes/physx_convex_polygon_shape_3d.h"
#include "shapes/physx_custom_shape_type.h"
#include "shapes/physx_cylinder_shape_3d.h"
#include "shapes/physx_heightmap_shape_3d.h"
#include "shapes/physx_separation_ray_shape_3d.h"
#include "shapes/physx_shape_3d.h"
#include "shapes/physx_sphere_shape_3d.h"
#include "shapes/physx_world_boundary_shape_3d.h"
#include "spaces/physx_direct_space_state_3d.h"
#include "spaces/physx_filter_shader.h"
#include "spaces/physx_space_3d.h"
#include "vehicles/physx_vehicle_server.h"

#include "PxPhysics.h"
#include "common/PxTolerancesScale.h"
#include "extensions/PxDefaultCpuDispatcher.h"
#include "extensions/PxExtensionsAPI.h"
#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxErrorCallback.h"
#include "foundation/PxErrors.h"
#include "foundation/PxFoundation.h"
#include "vehicle/PxVehicleAPI.h"

#ifdef GODOT_PHYSX_GPU
#include "cudamanager/PxCudaContextManager.h"
#endif

#include "core/config/project_settings.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

#ifdef DEBUG_ENABLED
#include "pvd/PxPvd.h"
#include "pvd/PxPvdTransport.h"
#endif

// ============================================================================
// _bind_methods —expose the vehicle2 API to C# / GDScript.
//
// These are PhysX-specific methods (PhysicsServer3D has no vehicle virtuals),
// so they must be explicitly bound to ClassDB here to be reachable from script.
// C# bindings regenerate on the next C# build (module_csharp_enabled=yes).
//
// IMPORTANT (C# / ECS):
//   Call these on the INNER server via PhysXServer3D.GetSingleton(), NOT on
//   PhysicsServer3D.GetSingleton() —the latter returns the
//   PhysicsServer3DWrapMT wrapper (register_types.cpp), which lacks these
//   methods. singleton_ptr is the real PhysXServer3D (set in its ctor).
//
// THREADING:
//   These ClassDB-bound methods bypass PhysicsServer3DWrapMT (scripts call
//   the inner server directly from the main thread), so every method below
//   takes the server's api_mutex first; the physics pipeline (step / sync /
//   flush_queries / free) takes the same lock, and the WrapMT pump task's
//   marshaled-command batches are bracketed with it via
//   command_flush_begin/end. This serializes module API calls against the
//   actual simulation, callback dispatch, RID frees and marshaled commands —
//   including under physics/physx_3d/simulation/async_step and with
//   run_on_separate_thread=true. Residual caveat: with async stepping on a
//   separate thread, a module mutation from the main thread cannot fetch a
//   solve the physics thread kicked (PhysX requires simulate/fetchResults on
//   one thread) and proceeds against the last fetched state (_warn_module_
//   api_separate_thread tripwires it once).
//
// USAGE (per frame): write control inputs in _PhysicsProcess (pre-step); read
//   telemetry (vehicle_get_wheel_states / vehicle_get_engine_state) afterwards
//   —it reflects the last completed step.
// ============================================================================
void PhysXServer3D::_bind_methods() {
	// Singleton (static) —lets script reach the inner server.
	ClassDB::bind_static_method("PhysXServer3D", D_METHOD("get_singleton"), &PhysXServer3D::get_singleton);
	ClassDB::bind_method(D_METHOD("is_gpu_dynamics_enabled"), &PhysXServer3D::is_gpu_dynamics_enabled);
	ClassDB::bind_method(D_METHOD("get_gpu_device_name"), &PhysXServer3D::get_gpu_device_name);

	// Lifecycle.
	ClassDB::bind_method(D_METHOD("soft_body_set_solver_mode", "soft_body", "mode"), &PhysXServer3D::soft_body_set_solver_mode);
	ClassDB::bind_method(D_METHOD("soft_body_attach_point_to_body", "soft_body", "point_index", "body"), &PhysXServer3D::soft_body_attach_point_to_body);
	ClassDB::bind_method(D_METHOD("soft_body_detach_point_from_body", "soft_body", "point_index"), &PhysXServer3D::soft_body_detach_point_from_body);
	ClassDB::bind_method(D_METHOD("soft_body_get_solver_mode", "soft_body"), &PhysXServer3D::soft_body_get_solver_mode);
	// The cloth RID API (like vehicle/particle_fluid) is exposed to scripts so
	// tests and tools can drive it without the PhysXCloth3D node.
	ClassDB::bind_method(D_METHOD("cloth_create"), &PhysXServer3D::cloth_create);
	ClassDB::bind_method(D_METHOD("cloth_set_space", "cloth", "space"), &PhysXServer3D::cloth_set_space);
	ClassDB::bind_method(D_METHOD("cloth_set_params", "cloth", "thickness", "density", "stretch", "bend", "damping", "collision_mask"), &PhysXServer3D::cloth_set_params);
	ClassDB::bind_method(D_METHOD("cloth_set_collision_layer_and_mask", "cloth", "layer", "mask"), &PhysXServer3D::cloth_set_collision_layer_and_mask);
	ClassDB::bind_method(D_METHOD("cloth_add_collision_exception", "cloth", "body"), &PhysXServer3D::cloth_add_collision_exception);
	ClassDB::bind_method(D_METHOD("cloth_remove_collision_exception", "cloth", "body"), &PhysXServer3D::cloth_remove_collision_exception);
	ClassDB::bind_method(D_METHOD("cloth_build", "cloth", "positions", "indices", "transform"), &PhysXServer3D::cloth_build);
	ClassDB::bind_method(D_METHOD("cloth_set_pinned", "cloth", "pinned"), &PhysXServer3D::cloth_set_pinned);
	ClassDB::bind_method(D_METHOD("cloth_set_pin_targets", "cloth", "targets"), &PhysXServer3D::cloth_set_pin_targets);
	ClassDB::bind_method(D_METHOD("cloth_apply_wind", "cloth", "wind", "drag", "lift", "delta"), &PhysXServer3D::cloth_apply_wind);
	ClassDB::bind_method(D_METHOD("cloth_is_ready", "cloth"), &PhysXServer3D::cloth_is_ready);
	ClassDB::bind_method(D_METHOD("vehicle_create", "archetype"), &PhysXServer3D::vehicle_create);
	ClassDB::bind_method(D_METHOD("vehicle_set_chassis_body", "vehicle", "body"), &PhysXServer3D::vehicle_set_chassis_body);
	ClassDB::bind_method(D_METHOD("vehicle_set_space", "vehicle", "space"), &PhysXServer3D::vehicle_set_space);

	// Wheels.
	ClassDB::bind_method(D_METHOD("vehicle_get_wheel_count", "vehicle"), &PhysXServer3D::vehicle_get_wheel_count);
	ClassDB::bind_method(D_METHOD("vehicle_set_wheel_count", "vehicle", "count"), &PhysXServer3D::vehicle_set_wheel_count);
	ClassDB::bind_method(D_METHOD("vehicle_add_wheel", "vehicle"), &PhysXServer3D::vehicle_add_wheel);
	ClassDB::bind_method(D_METHOD("vehicle_set_wheel_params", "vehicle", "idx", "params"), &PhysXServer3D::vehicle_set_wheel_params);

	// Control (both archetypes) + EngineDrive config.
	ClassDB::bind_method(D_METHOD("vehicle_set_control_inputs", "vehicle", "throttle", "brake", "steer", "handbrake"), &PhysXServer3D::vehicle_set_control_inputs);
	ClassDB::bind_method(D_METHOD("vehicle_set_gear_command", "vehicle", "gear"), &PhysXServer3D::vehicle_set_gear_command);
	ClassDB::bind_method(D_METHOD("vehicle_set_response_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_response_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_anti_roll_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_anti_roll_params);
	ClassDB::bind_method(D_METHOD("vehicle_get_anti_roll_params", "vehicle"), &PhysXServer3D::vehicle_get_anti_roll_params);
	ClassDB::bind_method(D_METHOD("vehicle_get_response_params", "vehicle"), &PhysXServer3D::vehicle_get_response_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_engine_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_engine_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_clutch_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_clutch_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_gearbox_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_gearbox_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_autobox_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_autobox_params);
	ClassDB::bind_method(D_METHOD("vehicle_set_differential_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_differential_params);

	// DirectDrive per-wheel control.
	ClassDB::bind_method(D_METHOD("vehicle_set_wheel_drive_torque", "vehicle", "idx", "torque"), &PhysXServer3D::vehicle_set_wheel_drive_torque);
	ClassDB::bind_method(D_METHOD("vehicle_set_wheel_brake_torque", "vehicle", "idx", "torque"), &PhysXServer3D::vehicle_set_wheel_brake_torque);
	ClassDB::bind_method(D_METHOD("vehicle_set_wheel_steer_angle", "vehicle", "idx", "angle"), &PhysXServer3D::vehicle_set_wheel_steer_angle);

	// Telemetry (polled; valid after the step).
	ClassDB::bind_method(D_METHOD("vehicle_get_wheel_states", "vehicle"), &PhysXServer3D::vehicle_get_wheel_states);
	ClassDB::bind_method(D_METHOD("vehicle_get_engine_state", "vehicle"), &PhysXServer3D::vehicle_get_engine_state);

	// Ackermann steering + 2-wheeler balance assist.
	ClassDB::bind_method(D_METHOD("vehicle_set_ackermann_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_ackermann_params);
	ClassDB::bind_method(D_METHOD("vehicle_get_wheel_steer_angles", "vehicle"), &PhysXServer3D::vehicle_get_wheel_steer_angles);
	ClassDB::bind_method(D_METHOD("vehicle_set_balance_params", "vehicle", "params"), &PhysXServer3D::vehicle_set_balance_params);
	ClassDB::bind_method(D_METHOD("vehicle_get_balance_state", "vehicle"), &PhysXServer3D::vehicle_get_balance_state);

	// Reduced-coordinate articulations (module-defined API; the same
	// inner-server caveat as the vehicle block applies — call these on
	// PhysXServer3D.GetSingleton(), not PhysicsServer3D.GetSingleton()).
	ClassDB::bind_method(D_METHOD("articulation_create"), &PhysXServer3D::articulation_create);
	ClassDB::bind_method(D_METHOD("articulation_set_space", "articulation", "space"), &PhysXServer3D::articulation_set_space);
	ClassDB::bind_method(D_METHOD("articulation_add_link", "articulation", "parent_index", "parent_frame", "child_frame", "joint_type", "density", "box_half_extents"), &PhysXServer3D::articulation_add_link);
	ClassDB::bind_method(D_METHOD("articulation_set_drive", "articulation", "link_index", "axis", "stiffness", "damping", "drive_target", "drive_velocity", "drive_type"), &PhysXServer3D::articulation_set_drive);
	ClassDB::bind_method(D_METHOD("articulation_set_limit", "articulation", "link_index", "axis", "low", "high"), &PhysXServer3D::articulation_set_limit);
	ClassDB::bind_method(D_METHOD("articulation_set_fix_base", "articulation", "fix"), &PhysXServer3D::articulation_set_fix_base);
	ClassDB::bind_method(D_METHOD("articulation_wake", "articulation"), &PhysXServer3D::articulation_wake);
	ClassDB::bind_method(D_METHOD("articulation_sleep", "articulation"), &PhysXServer3D::articulation_sleep);
	ClassDB::bind_method(D_METHOD("articulation_get_link_count", "articulation"), &PhysXServer3D::articulation_get_link_count);
	ClassDB::bind_method(D_METHOD("articulation_get_link_transform", "articulation", "link_index"), &PhysXServer3D::articulation_get_link_transform);
	ClassDB::bind_method(D_METHOD("articulation_is_sleeping", "articulation"), &PhysXServer3D::articulation_is_sleeping);
	ClassDB::bind_method(D_METHOD("articulation_set_link_shape", "articulation", "link_index", "shape", "transform"), &PhysXServer3D::articulation_set_link_shape);
	ClassDB::bind_method(D_METHOD("articulation_set_link_collision_layer", "articulation", "link_index", "layer"), &PhysXServer3D::articulation_set_link_collision_layer);
	ClassDB::bind_method(D_METHOD("articulation_set_link_collision_mask", "articulation", "link_index", "mask"), &PhysXServer3D::articulation_set_link_collision_mask);
	ClassDB::bind_method(D_METHOD("articulation_get_link_collision_layer", "articulation", "link_index"), &PhysXServer3D::articulation_get_link_collision_layer);
	ClassDB::bind_method(D_METHOD("articulation_get_link_collision_mask", "articulation", "link_index"), &PhysXServer3D::articulation_get_link_collision_mask);
	ClassDB::bind_method(D_METHOD("articulation_get_link_velocity", "articulation", "link_index"), &PhysXServer3D::articulation_get_link_velocity);

	// GPU particle fluid (module extension; drives the PhysXParticleFluid3D
	// node from C++, bound here so GDScript/C# tools like the debug lab can
	// build fluid volumes through the raw server API).
	ClassDB::bind_method(D_METHOD("particle_fluid_create"), &PhysXServer3D::particle_fluid_create);
	ClassDB::bind_method(D_METHOD("particle_fluid_supports_diffuse"), &PhysXServer3D::particle_fluid_supports_diffuse);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_space", "fluid", "space"), &PhysXServer3D::particle_fluid_set_space);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_param", "fluid", "param", "value"), &PhysXServer3D::particle_fluid_set_param);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_capacity", "fluid", "max"), &PhysXServer3D::particle_fluid_set_capacity);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_particles", "fluid", "positions", "initial_velocity"), &PhysXServer3D::particle_fluid_set_particles);
	ClassDB::bind_method(D_METHOD("particle_fluid_emit", "fluid", "positions", "velocity"), &PhysXServer3D::particle_fluid_emit);
	ClassDB::bind_method(D_METHOD("particle_fluid_clear", "fluid"), &PhysXServer3D::particle_fluid_clear);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_foam", "fluid", "enabled", "capacity", "lifetime", "threshold", "buoyancy", "size"), &PhysXServer3D::particle_fluid_set_foam);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_positions", "fluid"), &PhysXServer3D::particle_fluid_get_positions);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_foam_positions", "fluid"), &PhysXServer3D::particle_fluid_get_foam_positions);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_particle_count", "fluid"), &PhysXServer3D::particle_fluid_get_particle_count);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_foam_count", "fluid"), &PhysXServer3D::particle_fluid_get_foam_count);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_surface_mesh", "fluid", "enabled"), &PhysXServer3D::particle_fluid_set_surface_mesh);
	ClassDB::bind_method(D_METHOD("particle_fluid_set_surface_anisotropy", "fluid", "enabled"), &PhysXServer3D::particle_fluid_set_surface_anisotropy);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_surface_triangle_count", "fluid"), &PhysXServer3D::particle_fluid_get_surface_triangle_count);
	ClassDB::bind_method(D_METHOD("soft_body_is_gpu", "soft_body"), &PhysXServer3D::soft_body_is_gpu);
	ClassDB::bind_method(D_METHOD("particle_fluid_get_submersion", "fluid", "world_aabb"), &PhysXServer3D::particle_fluid_get_submersion);
}

PhysXServer3D *PhysXServer3D::singleton_ptr = nullptr;
// ---------------------------------------------------------------------------
// PhysX allocator / error callbacks.
//
// PhysX requires these to route its internal allocations and diagnostics.
// They are declared as static locals inside init() so they outlive
// px_foundation (PhysX holds raw pointers to them).
// ---------------------------------------------------------------------------

// Bridges PhysX allocations to Memory::alloc_static(). The ALIGNED variant
// is mandatory: PhysX SIMD paths require >=16-byte alignment, and the
// matching deallocate() must pass the same flag.
class PhysXAllocatorCallback : public physx::PxAllocatorCallback {
public:
	virtual ~PhysXAllocatorCallback() override {}

	virtual void *allocate(
			size_t size,
			const char *typeName,
			const char *filename,
			int line) override {
		return Memory::alloc_static(size, true);
	}

	virtual void deallocate(void *ptr) override {
		Memory::free_static(ptr, true);
	}
};
// Routes PhysX errors/warnings to Godot's logging macros (no-op body in
// release builds @DASH@ PhysX still requires the object to exist). Debug
// builds map each error code to the matching Godot log level.
class PhysXErrorCallback : public physx::PxErrorCallback {
public:
	virtual ~PhysXErrorCallback() override {}

	virtual void reportError(
			physx::PxErrorCode::Enum code,
			const char *message,
			const char *file,
			int line) override {
#ifdef DEBUG_ENABLED
		String formatted_message =
				" in " + String(file) + ", line " + itos(line) + ": " + String(message);

		switch (code) {
			case physx::PxErrorCode::eDEBUG_INFO:
				print_line("PhysX Info" + formatted_message);
				break;

			case physx::PxErrorCode::eDEBUG_WARNING:
				WARN_PRINT("PhysX Warning" + formatted_message);
				break;

			case physx::PxErrorCode::ePERF_WARNING:
				WARN_PRINT("PhysX Performance Warning" + formatted_message);
				break;

			case physx::PxErrorCode::eINVALID_PARAMETER:
				ERR_PRINT("PhysX Error (Invalid Parameter)" + formatted_message);
				break;

			case physx::PxErrorCode::eINVALID_OPERATION:
				ERR_PRINT("PhysX Error (Invalid Operation)" + formatted_message);
				break;

			case physx::PxErrorCode::eOUT_OF_MEMORY:
				ERR_PRINT("PhysX Error (Out of Memory)" + formatted_message);
				break;

			case physx::PxErrorCode::eINTERNAL_ERROR:
				ERR_PRINT("PhysX Error (Internal Error)" + formatted_message);
				break;

			case physx::PxErrorCode::eABORT:
				ERR_PRINT("PhysX Error (Abort)" + formatted_message);
				break;

			default:
				ERR_PRINT("PhysX Error Code " + itos((int)code) + formatted_message);
				break;
		}
#endif
	}
};

physx::PxPhysics &PhysXServer3D::get_physics() const {
	CRASH_COND_MSG(!px_physics,
			"PhysX: px_physics is null. Likely get_singleton() returned nullptr due to WrapMT wrapper.");
	return *px_physics;
}

template <typename T>
RID PhysXServer3D::_make_shape() {
	T *shape = memnew(T);
	RID rid = shape_owner.make_rid(shape);
	shape->set_rid(rid);
	return rid;
}

RID PhysXServer3D::world_boundary_shape_create() {
	return _make_shape<PhysXWorldBoundaryShape3D>();
}
RID PhysXServer3D::separation_ray_shape_create() {
	return _make_shape<PhysXSeparationRayShape3D>();
}
RID PhysXServer3D::sphere_shape_create() {
	return _make_shape<PhysXSphereShape3D>();
}
RID PhysXServer3D::box_shape_create() {
	return _make_shape<PhysXBoxShape3D>();
}
RID PhysXServer3D::capsule_shape_create() {
	return _make_shape<PhysXCapsuleShape3D>();
}
RID PhysXServer3D::cylinder_shape_create() {
	return _make_shape<PhysXCylinderShape3D>();
}
RID PhysXServer3D::convex_polygon_shape_create() {
	return _make_shape<PhysXConvexPolygonShape3D>();
}
RID PhysXServer3D::concave_polygon_shape_create() {
	return _make_shape<PhysXConcavePolygonShape3D>();
}
RID PhysXServer3D::heightmap_shape_create() {
	return _make_shape<PhysXHeightMapShape3D>();
}
RID PhysXServer3D::custom_shape_create() {
	return _make_shape<PhysXCustomShapeType>();
}

void PhysXServer3D::shape_set_data(RID p_shape, const Variant &p_data) {
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_data(p_data);
}

Variant PhysXServer3D::shape_get_data(RID p_shape) const {
	const PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, Variant());

	return shape->get_data();
}

void PhysXServer3D::shape_set_margin(RID p_shape, real_t p_margin) {
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_margin((float)p_margin);
}

real_t PhysXServer3D::shape_get_margin(RID p_shape) const {
	const PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, 0.04f);

	return shape->get_margin();
}

PhysicsServer3D::ShapeType PhysXServer3D::shape_get_type(RID p_shape) const {
	const PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, PhysicsServer3D::SHAPE_CUSTOM);

	return shape->get_type();
}

void PhysXServer3D::shape_set_custom_solver_bias(RID p_shape, real_t p_bias) {
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);

	shape->set_solver_bias((float)p_bias);
}
real_t PhysXServer3D::shape_get_custom_solver_bias(RID p_shape) const {
	const PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_V(shape, 0.0);

	return shape->get_solver_bias();
}

String PhysXServer3D::get_gpu_device_name() const {
#ifdef GODOT_PHYSX_GPU
	if (px_cuda_context) {
		return String(px_cuda_context->getDeviceName());
	}
#endif
	return String();
}

RID PhysXServer3D::space_create() {
	// Same degraded-boot discipline as body_create/area_create —
	// the space's default area (created below) needs PxPhysics.
	ERR_FAIL_NULL_V_MSG(px_physics, RID(),
			"PhysX: space_create() called before PhysX initialization. "
			"Check that init() succeeded and that PhysX DLLs/libs are available.");
	PhysXSpace3D *space = memnew(PhysXSpace3D);
	RID rid = space_owner.make_rid(space);
	space->set_rid(rid);
	_space_list.push_back(rid);

	// Create the space's default area (priority -1). It provides the
	// world-default gravity/damp and is the additive fallback for any
	// gravity/damp channel not resolved by user areas.
	RID area_id = area_create();
	PhysXArea3D *def = area_owner.get_or_null(area_id);
	ERR_FAIL_NULL_V(def, rid);
	def->set_space(space);
	def->set_param(PhysicsServer3D::AREA_PARAM_PRIORITY, -1);
	// Seed with project-setting defaults so it matches the old scene gravity.
	const real_t g = GLOBAL_GET("physics/3d/default_gravity");
	def->set_param(PhysicsServer3D::AREA_PARAM_GRAVITY, g);
	// Seed the direction from the project setting too -- the old hardcoded
	// (0, -1, 0) fought a customized physics/3d/default_gravity_vector until
	// World3D's area_set_param overwrote it a frame later.
	def->set_param(PhysicsServer3D::AREA_PARAM_GRAVITY_VECTOR,
			(Vector3)GLOBAL_GET("physics/3d/default_gravity_vector"));
	space->set_default_area(def);
	return rid;
}

void PhysXServer3D::space_set_param(RID p_space, PhysicsServer3D::SpaceParameter p_param, real_t p_value) {
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);
	space->set_param(p_param, p_value);
}

real_t PhysXServer3D::space_get_param(RID p_space, PhysicsServer3D::SpaceParameter p_param) const {
	const PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, 0.0);
	return space->get_param(p_param);
}

PhysicsDirectSpaceState3D *PhysXServer3D::space_get_direct_state(RID p_space) {
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, nullptr);
	return space->get_direct_state();
}

void PhysXServer3D::space_set_debug_contacts(RID p_space, int p_max_contacts) {
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);
	space->set_debug_contacts(p_max_contacts);
}

PackedVector3Array PhysXServer3D::space_get_contacts(RID p_space) const {
	const PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, PackedVector3Array());
	// Only the first `count` entries are valid this step.
	const Vector<Vector3> &contacts = space->get_debug_contacts();
	const int count = space->get_debug_contact_count();
	PackedVector3Array packed;
	packed.resize(count);
	for (int i = 0; i < count; i++) {
		packed.write[i] = contacts[i];
	}
	return packed;
}

int PhysXServer3D::space_get_contact_count(RID p_space) const {
	const PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL_V(space, 0);
	return space->get_debug_contact_count();
}

PhysXArea3D *PhysXServer3D::get_area(RID p_rid) const {
	return area_owner.get_or_null(p_rid);
}

PhysXShape3D *PhysXServer3D::get_shape(RID p_rid) const {
	return shape_owner.get_or_null(p_rid);
}

RID PhysXServer3D::area_create() {
	// Degrade gracefully like body_create() — the area constructor
	// needs PxPhysics to create its kinematic actor, so constructing one
	// before init() (or after a failed init) would crash.
	ERR_FAIL_NULL_V_MSG(px_physics, RID(),
			"PhysX: area_create() called before PhysX initialization. "
			"Check that init() succeeded and that PhysX DLLs/libs are available.");
	PhysXArea3D *area = memnew(PhysXArea3D);
	RID rid = area_owner.make_rid(area);
	area->set_rid(rid);
	// The actor was created with an invalid RID in userData (set_rid ran after
	// the ctor); refresh now so query/callback results resolve correctly.
	area->refresh_user_data();
	return rid;
}

void PhysXServer3D::area_set_space(RID p_area, RID p_space) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	PhysXSpace3D *space = p_space.is_valid() ? space_owner.get_or_null(p_space) : nullptr;
	if (p_space.is_valid()) {
		ERR_FAIL_NULL_MSG(space, "PhysX: area_set_space passed an invalid space RID.");
	}
	area->set_space(space);
}

RID PhysXServer3D::area_get_space(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());
	return area->get_space() ? area->get_space()->get_rid() : RID();
}

void PhysXServer3D::area_add_shape(RID p_area, RID p_shape, const Transform3D &p_transform, bool p_disabled) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	area->add_shape(shape, p_transform, p_disabled);
}

void PhysXServer3D::area_set_shape(RID p_area, int p_shape_idx, RID p_shape) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	ERR_FAIL_INDEX(p_shape_idx, area->get_shape_count());
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	area->set_shape(p_shape_idx, shape);
}

RID PhysXServer3D::area_get_shape(RID p_area, int p_shape_idx) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, RID());
	return area->get_shape_rid(p_shape_idx);
}

void PhysXServer3D::area_set_shape_transform(RID p_area, int p_shape_idx, const Transform3D &p_transform) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_shape_transform(p_shape_idx, p_transform);
}

Transform3D PhysXServer3D::area_get_shape_transform(RID p_area, int p_shape_idx) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());
	return area->get_shape_transform(p_shape_idx);
}

void PhysXServer3D::area_set_shape_disabled(RID p_area, int p_shape_idx, bool p_disabled) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_shape_disabled(p_shape_idx, p_disabled);
}

int PhysXServer3D::area_get_shape_count(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_shape_count();
}

void PhysXServer3D::area_remove_shape(RID p_area, int p_shape_idx) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->remove_shape(p_shape_idx);
}

void PhysXServer3D::area_clear_shapes(RID p_area) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->clear_shapes();
}

void PhysXServer3D::area_attach_object_instance_id(RID p_area, ObjectID p_id) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_instance_id(p_id);
	area->refresh_user_data();
}

ObjectID PhysXServer3D::area_get_object_instance_id(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, ObjectID());
	return area->get_instance_id();
}

void PhysXServer3D::area_set_param(RID p_area, PhysicsServer3D::AreaParameter p_param, const Variant &p_value) {
	// Godot's World3D calls area_set_param(SPACE_RID, ...) to set world-default
	// gravity/damp. The space RID isn't in the area owner, so resolve it to the
	// space's default area first —matches GodotPhysicsServer3D.
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	if (!area) {
		PhysXSpace3D *as_space = space_owner.get_or_null(p_area);
		if (as_space) {
			area = as_space->get_default_area();
		}
	}
	ERR_FAIL_NULL(area);
	area->set_param(p_param, p_value);
}

Variant PhysXServer3D::area_get_param(RID p_area, PhysicsServer3D::AreaParameter p_param) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	if (!area) {
		const PhysXSpace3D *as_space = space_owner.get_or_null(p_area);
		if (as_space) {
			area = as_space->get_default_area();
		}
	}
	ERR_FAIL_NULL_V(area, Variant());
	return area->get_param(p_param);
}

void PhysXServer3D::area_set_transform(RID p_area, const Transform3D &p_transform) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_transform(p_transform);
}

Transform3D PhysXServer3D::area_get_transform(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, Transform3D());
	return area->get_transform();
}

void PhysXServer3D::area_set_collision_layer(RID p_area, uint32_t p_layer) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_collision_layer(p_layer);
}

uint32_t PhysXServer3D::area_get_collision_layer(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_collision_layer();
}

void PhysXServer3D::area_set_collision_mask(RID p_area, uint32_t p_mask) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_collision_mask(p_mask);
}

uint32_t PhysXServer3D::area_get_collision_mask(RID p_area) const {
	const PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL_V(area, 0);
	return area->get_collision_mask();
}

void PhysXServer3D::area_set_monitorable(RID p_area, bool p_monitorable) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_monitorable(p_monitorable);
}

void PhysXServer3D::area_set_ray_pickable(RID p_area, bool p_enable) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_ray_pickable(p_enable);
}

void PhysXServer3D::area_set_monitor_callback(RID p_area, const Callable &p_callback) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_monitor_callback(p_callback);
}

void PhysXServer3D::area_set_area_monitor_callback(RID p_area, const Callable &p_callback) {
	PhysXArea3D *area = area_owner.get_or_null(p_area);
	ERR_FAIL_NULL(area);
	area->set_area_monitor_callback(p_callback);
}

PhysXBody3D *PhysXServer3D::get_body(RID p_rid) const {
	return body_owner.get_or_null(p_rid);
}

PhysXJoint3D *PhysXServer3D::get_joint(RID p_rid) const {
	return joint_owner.get_or_null(p_rid);
}

PhysXVehicleServer *PhysXServer3D::get_vehicle(RID p_rid) const {
	return vehicle_owner.get_or_null(p_rid);
}

RID PhysXServer3D::body_create() {
	ERR_FAIL_NULL_V_MSG(px_physics, RID(),
			"PhysX: body_create() called before PhysX initialization. "
			"Check that init() succeeded and that PhysX DLLs/libs are available.");
	PhysXBody3D *body = memnew(PhysXBody3D);
	RID rid = body_owner.make_rid(body);
	body->set_rid(rid);
	// The actor was created with an invalid RID in userData (set_rid ran after
	// the ctor); refresh now so query/callback results resolve correctly.
	body->refresh_user_data();
	return rid;
}

RID PhysXServer3D::body_get_space(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());
	return body->get_space() ? body->get_space()->get_rid() : RID();
}

void PhysXServer3D::body_set_space(RID p_body, RID p_space) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	PhysXSpace3D *space = p_space.is_valid() ? space_owner.get_or_null(p_space) : nullptr;
	if (p_space.is_valid()) {
		ERR_FAIL_NULL_MSG(space, "PhysX: body_set_space passed an invalid space RID.");
	}
	body->set_space(space);
}

PhysicsServer3D::BodyMode PhysXServer3D::body_get_mode(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, PhysicsServer3D::BODY_MODE_STATIC);
	return body->get_mode();
}

void PhysXServer3D::body_set_mode(RID p_body, PhysicsServer3D::BodyMode p_mode) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_mode(p_mode);
}

void PhysXServer3D::body_add_shape(RID p_body, RID p_shape, const Transform3D &p_transform, bool p_disabled) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	body->add_shape(shape, p_transform, p_disabled);
}

void PhysXServer3D::body_set_shape(RID p_body, int p_shape_idx, RID p_shape) {
	// Godot's body_set_shape replaces the shape resource at an index while
	// preserving the slot's transform and disabled state. PhysX shapes are
	// immutable once attached, so this detaches and re-attaches; the body
	// wrapper SWAP-restores the original index.
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	ERR_FAIL_INDEX(p_shape_idx, body->get_shape_count());
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL(shape);
	body->set_shape(p_shape_idx, shape);
}

RID PhysXServer3D::body_get_shape(RID p_body, int p_shape_idx) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, RID());
	return body->get_shape_rid(p_shape_idx);
}

Transform3D PhysXServer3D::body_get_shape_transform(RID p_body, int p_shape_idx) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Transform3D());
	return body->get_shape_transform(p_shape_idx);
}

void PhysXServer3D::body_set_shape_transform(RID p_body, int p_shape_idx, const Transform3D &p_transform) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_shape_transform(p_shape_idx, p_transform);
}

void PhysXServer3D::body_set_shape_disabled(RID p_body, int p_shape_idx, bool p_disabled) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_shape_disabled(p_shape_idx, p_disabled);
}

int PhysXServer3D::body_get_shape_count(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_shape_count();
}

void PhysXServer3D::body_remove_shape(RID p_body, int p_shape_idx) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->remove_shape(p_shape_idx);
}

void PhysXServer3D::body_clear_shapes(RID p_body) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	while (body->get_shape_count() > 0) {
		body->remove_shape(0);
	}
}

void PhysXServer3D::body_attach_object_instance_id(RID p_body, ObjectID p_id) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	// Silently ignore an invalid RID: body_create() may have returned an empty
	// RID during a fresh-compile editor boot when PhysX wasn't initialized yet,
	// and the node still calls attach on it. ERR_FAIL_NULL would spam the log.
	if (!body) {
		return;
	}
	body->set_instance_id(p_id);
	// Keep the actor's userData in sync.
	body->refresh_user_data();
}

ObjectID PhysXServer3D::body_get_object_instance_id(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, ObjectID());
	return body->get_instance_id();
}

void PhysXServer3D::body_set_enable_continuous_collision_detection(RID p_body, bool p_enable) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_continuous_collision_detection(p_enable);
}

bool PhysXServer3D::body_is_continuous_collision_detection_enabled(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->is_continuous_collision_detection_enabled();
}

void PhysXServer3D::body_set_collision_layer(RID p_body, uint32_t p_layer) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_collision_layer(p_layer);
}

uint32_t PhysXServer3D::body_get_collision_layer(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_collision_layer();
}

void PhysXServer3D::body_set_collision_mask(RID p_body, uint32_t p_mask) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_collision_mask(p_mask);
}

uint32_t PhysXServer3D::body_get_collision_mask(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_collision_mask();
}

void PhysXServer3D::body_set_collision_priority(RID p_body, real_t p_priority) {
	// Stored and round-tripped; weighting test-motion recovery by it (upstream
	// Godot behavior) is not implemented -- see C-10.
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_collision_priority(p_priority);
}

real_t PhysXServer3D::body_get_collision_priority(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	return body ? body->get_collision_priority() : 1.0;
}

void PhysXServer3D::body_set_user_flags(RID p_body, uint32_t p_flags) {
	// Stored and round-tripped (no PhysX equivalent).
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_user_flags(p_flags);
}

uint32_t PhysXServer3D::body_get_user_flags(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	return body ? body->get_user_flags() : 0;
}

void PhysXServer3D::body_set_param(RID p_body, PhysicsServer3D::BodyParameter p_param, const Variant &p_value) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_param(p_param, p_value);
}

Variant PhysXServer3D::body_get_param(RID p_body, PhysicsServer3D::BodyParameter p_param) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());
	return body->get_param(p_param);
}

void PhysXServer3D::body_reset_mass_properties(RID p_body) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->reset_mass_properties();
}

void PhysXServer3D::body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_state(p_state, p_value);
}

Variant PhysXServer3D::body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Variant());
	return body->get_state(p_state);
}

void PhysXServer3D::body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_central_impulse(p_impulse);
}

void PhysXServer3D::body_apply_impulse(RID p_body, const Vector3 &p_impulse, const Vector3 &p_position) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_impulse(p_impulse, p_position);
}

void PhysXServer3D::body_apply_torque_impulse(RID p_body, const Vector3 &p_impulse) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_torque_impulse(p_impulse);
}

void PhysXServer3D::body_apply_central_force(RID p_body, const Vector3 &p_force) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_central_force(p_force);
}

void PhysXServer3D::body_apply_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_force(p_force, p_position);
}

void PhysXServer3D::body_apply_torque(RID p_body, const Vector3 &p_torque) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->apply_torque(p_torque);
}

void PhysXServer3D::body_add_constant_central_force(RID p_body, const Vector3 &p_force) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_central_force(p_force);
}

void PhysXServer3D::body_add_constant_force(RID p_body, const Vector3 &p_force, const Vector3 &p_position) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_force(p_force, p_position);
}

void PhysXServer3D::body_add_constant_torque(RID p_body, const Vector3 &p_torque) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->add_constant_torque(p_torque);
}

void PhysXServer3D::body_set_constant_force(RID p_body, const Vector3 &p_force) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_constant_force(p_force);
}

Vector3 PhysXServer3D::body_get_constant_force(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());
	return body->get_constant_force();
}

void PhysXServer3D::body_set_constant_torque(RID p_body, const Vector3 &p_torque) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_constant_torque(p_torque);
}

Vector3 PhysXServer3D::body_get_constant_torque(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, Vector3());
	return body->get_constant_torque();
}

void PhysXServer3D::body_set_axis_velocity(RID p_body, const Vector3 &p_axis_velocity) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_axis_velocity(p_axis_velocity);
}

void PhysXServer3D::body_set_axis_lock(RID p_body, PhysicsServer3D::BodyAxis p_axis, bool p_lock) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_axis_lock(p_axis, p_lock);
}

bool PhysXServer3D::body_is_axis_locked(RID p_body, PhysicsServer3D::BodyAxis p_axis) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->is_axis_locked(p_axis);
}

void PhysXServer3D::body_add_collision_exception(RID p_body, RID p_excepted_body) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	// Everything below mutates state the solver reads mid-flight: the word2
	// slot registry (read by the GPU filter shader on the sim threads) and
	// PxShape::setSimulationFilterData via refresh_collision_filters —
	// forbidden while a solve is in flight (async stepping). Fetch from both
	// bodies' spaces first.
	if (body->get_space()) {
		body->get_space()->ensure_synced();
	}
	PhysXBody3D *other = body_owner.get_or_null(p_excepted_body);
	if (other && other->get_space() && other->get_space() != body->get_space()) {
		other->get_space()->ensure_synced();
	}

	body->add_collision_exception(p_excepted_body);

	// GPU path: the simulation filter shader cannot see actors/userData (it
	// runs stateless on the sim thread, and GPU pair filtering does not invoke
	// the pair filter callback), so route the exception through the same word2
	// slot registry soft-body exceptions use. The CPU PhysXPairFilterCallback
	// keeps enforcing the set from the wrappers as well — both kill the same
	// pair, whichever path a pair takes.
	if (!other) {
		return;
	}
	const uint32_t slot_a = body->get_or_alloc_exception_slot();
	const uint32_t slot_b = other->get_or_alloc_exception_slot();
	body->refresh_collision_filters();
	other->refresh_collision_filters();
	g_physx_soft_exceptions.add(slot_a, slot_b);
}

void PhysXServer3D::body_remove_collision_exception(RID p_body, RID p_excepted_body) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);

	// Same mid-flight discipline as body_add_collision_exception: the registry
	// entry is read by the GPU filter shader during the solve.
	if (body->get_space()) {
		body->get_space()->ensure_synced();
	}
	PhysXBody3D *other = body_owner.get_or_null(p_excepted_body);
	if (other && other->get_space() && other->get_space() != body->get_space()) {
		other->get_space()->ensure_synced();
	}

	body->remove_collision_exception(p_excepted_body);
	if (other && body->get_exception_slot() != 0 && other->get_exception_slot() != 0) {
		g_physx_soft_exceptions.remove(body->get_exception_slot(), other->get_exception_slot());
	}
}

void PhysXServer3D::body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->get_collision_exceptions(p_exceptions);
}

void PhysXServer3D::body_set_max_contacts_reported(RID p_body, int p_amount) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_max_contacts_reported(p_amount);
}

int PhysXServer3D::body_get_max_contacts_reported(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, 0);
	return body->get_max_contacts_reported();
}

void PhysXServer3D::body_set_contacts_reported_depth_threshold(RID p_body, real_t p_threshold) {
	// Stored and round-tripped (no PhysX equivalent).
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_contacts_depth_threshold(p_threshold);
}

real_t PhysXServer3D::body_get_contacts_reported_depth_threshold(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	return body ? body->get_contacts_depth_threshold() : 0.0;
}

void PhysXServer3D::body_set_omit_force_integration(RID p_body, bool p_enable) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_omit_force_integration(p_enable);
}

bool PhysXServer3D::body_is_omitting_force_integration(RID p_body) const {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	return body->is_omitting_force_integration();
}

void PhysXServer3D::body_set_state_sync_callback(RID p_body, const Callable &p_callable) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_state_sync_callback(p_callable);
}

void PhysXServer3D::body_set_force_integration_callback(RID p_body, const Callable &p_callable, const Variant &p_userdata) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_force_integration_callback(p_callable, p_userdata);
}

void PhysXServer3D::body_set_ray_pickable(RID p_body, bool p_enable) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(body);
	body->set_ray_pickable(p_enable);
}

bool PhysXServer3D::body_test_motion(RID p_body, const MotionParameters &p_parameters, MotionResult *r_result) {
	const PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, false);
	PhysXSpace3D *space = body->get_space();
	if (!space || !space->get_direct_state()) {
		return false;
	}
	return space->get_direct_state()->body_test_motion(*body, p_parameters, r_result);
}

PhysicsDirectBodyState3D *PhysXServer3D::body_get_direct_state(RID p_body) {
	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(body, nullptr);
	return body->get_direct_state();
}

void PhysXServer3D::_warn_module_api_separate_thread() {
	if ((bool)GLOBAL_GET("physics/3d/run_on_separate_thread")) {
		WARN_PRINT_ONCE(
				"PhysX: module extension APIs (vehicles, GPU fluids/cloths, articulations) are serialized "
				"against the physics pipeline, but with physics/physx_3d/simulation/async_step a module "
				"mutation from the main thread cannot fetch a solve the physics thread kicked and proceeds "
				"against the last fetched state. Keep async stepping off for projects that mutate module "
				"objects from scripts.");
	}
}

RID PhysXServer3D::vehicle_create(int p_archetype) {
	MutexLock lock(api_mutex);
	_warn_module_api_separate_thread();
	ERR_FAIL_NULL_V_MSG(px_physics, RID(),
			"PhysX: vehicle_create() called before PhysX initialization.");
	PhysXVehicleServer *vehicle = memnew(PhysXVehicleServer);
	RID rid = vehicle_owner.make_rid(vehicle);
	vehicle->set_rid(rid);
	vehicle->archetype = PhysXVehicleServer::int_to_archetype(p_archetype);
	return rid;
}

void PhysXServer3D::vehicle_set_chassis_body(RID p_vehicle, RID p_body) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);

	PhysXBody3D *body = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_MSG(body, "PhysX: vehicle_set_chassis_body: body RID not found.");

	// Vehicles require a simulated dynamic chassis. Static/kinematic bodies are
	// rejected because (a) PxVehicle drives a PxRigidDynamic that is integrated
	// by the solver, and (b) a later mode change recreates the underlying actor
	// (PhysXBody3D::set_mode, physx_body_3d.cpp:178-257), which would invalidate
	// the chassis pointer the vehicle borrowed. Kinematic bodies share the same
	// PxRigidDynamic C++ type, so the mode must be checked explicitly.
	if (body->get_mode() != PhysicsServer3D::BODY_MODE_RIGID) {
		ERR_FAIL_MSG("PhysX: vehicle chassis must be a dynamic (BODY_MODE_RIGID) body.");
	}

	physx::PxRigidDynamic *dyn = body->get_px_dynamic();
	ERR_FAIL_NULL_MSG(dyn, "PhysX: vehicle chassis dynamic actor is null.");

	vehicle->adopt(dyn, body);
}

void PhysXServer3D::vehicle_set_space(RID p_vehicle, RID p_space) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);

	// Resolve the space RID; an invalid RID clears the space membership, mirroring
	// body_set_space (physx_server.cpp:509).
	PhysXSpace3D *space = p_space.is_valid() ? space_owner.get_or_null(p_space) : nullptr;
	if (p_space.is_valid()) {
		ERR_FAIL_NULL_MSG(space, "PhysX: vehicle_set_space passed an invalid space RID.");
	}

	vehicle->set_space(space);
}

int PhysXServer3D::vehicle_get_wheel_count(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	const PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, 0);
	return vehicle->get_wheel_count();
}

void PhysXServer3D::vehicle_set_wheel_count(RID p_vehicle, int p_count) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	ERR_FAIL_COND_MSG(p_count < 0, "PhysX: vehicle wheel count must be >= 0.");
	ERR_FAIL_COND_MSG(p_count > (int)physx::PxVehicleLimits::eMAX_NB_WHEELS,
			"PhysX: vehicle wheel count exceeds PxVehicleLimits::eMAX_NB_WHEELS.");
	// Per-wheel param arrays live in the vehicle2 state, which is created when
	// the chassis is attached. Require a chassis before configuring wheels.
	ERR_FAIL_NULL_MSG(vehicle->get_chassis_body(),
			"PhysX: attach a chassis with vehicle_set_chassis_body before configuring wheels.");
	vehicle->allocate_wheel_buffers(p_count);
}

int PhysXServer3D::vehicle_add_wheel(RID p_vehicle) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, -1);
	ERR_FAIL_NULL_V_MSG(vehicle->get_chassis_body(), -1,
			"PhysX: attach a chassis with vehicle_set_chassis_body before configuring wheels.");
	const int idx = vehicle->get_wheel_count();
	ERR_FAIL_COND_V_MSG(idx + 1 > (int)physx::PxVehicleLimits::eMAX_NB_WHEELS, -1,
			"PhysX: vehicle wheel count exceeds PxVehicleLimits::eMAX_NB_WHEELS.");
	vehicle->allocate_wheel_buffers(idx + 1);
	return idx;
}

void PhysXServer3D::vehicle_set_wheel_params(RID p_vehicle, int p_idx, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	ERR_FAIL_INDEX(p_idx, vehicle->get_wheel_count());
	vehicle->apply_wheel_params(p_idx, p_params);
}

void PhysXServer3D::vehicle_set_control_inputs(RID p_vehicle, real_t p_throttle, real_t p_brake, real_t p_steer, real_t p_handbrake) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_control_inputs((float)p_throttle, (float)p_brake, (float)p_steer, (float)p_handbrake);
}

void PhysXServer3D::vehicle_set_gear_command(RID p_vehicle, int p_gear) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_gear_command(p_gear);
}

void PhysXServer3D::vehicle_set_response_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_response_params(p_params);
}

void PhysXServer3D::vehicle_set_anti_roll_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_anti_roll_params(p_params);
}

void PhysXServer3D::vehicle_set_engine_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_engine_params(p_params);
}

void PhysXServer3D::vehicle_set_clutch_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_clutch_params(p_params);
}

void PhysXServer3D::vehicle_set_gearbox_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_gearbox_params(p_params);
}

void PhysXServer3D::vehicle_set_autobox_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_autobox_params(p_params);
}

void PhysXServer3D::vehicle_set_differential_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_differential_params(p_params);
}

void PhysXServer3D::vehicle_set_wheel_drive_torque(RID p_vehicle, int p_idx, real_t p_torque) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	ERR_FAIL_INDEX(p_idx, vehicle->get_wheel_count());
	vehicle->set_wheel_drive_torque(p_idx, (float)p_torque);
}

void PhysXServer3D::vehicle_set_wheel_brake_torque(RID p_vehicle, int p_idx, real_t p_torque) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	ERR_FAIL_INDEX(p_idx, vehicle->get_wheel_count());
	vehicle->set_wheel_brake_torque(p_idx, (float)p_torque);
}

void PhysXServer3D::vehicle_set_wheel_steer_angle(RID p_vehicle, int p_idx, real_t p_angle) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	ERR_FAIL_INDEX(p_idx, vehicle->get_wheel_count());
	vehicle->set_wheel_steer_angle(p_idx, (float)p_angle);
}

Array PhysXServer3D::vehicle_get_wheel_states(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	Array out;
	const PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, out);
	if (!vehicle->get_telemetry_valid()) {
		return out;
	}
	const LocalVector<WheelTelemetry> &wts = vehicle->get_wheel_telemetry();
	for (int i = 0; i < (int)wts.size(); i++) {
		Dictionary d;
		d["in_contact"] = wts[i].in_contact;
		d["contact_point"] = wts[i].contact_point;
		d["contact_normal"] = wts[i].contact_normal;
		d["contact_body_rid"] = wts[i].contact_body_rid;
		d["rpm"] = wts[i].rpm;
		d["skid"] = wts[i].skid;
		d["skid_lateral"] = wts[i].skid_lateral;
		d["rotation"] = wts[i].rotation;
		out.push_back(d);
	}
	return out;
}

Dictionary PhysXServer3D::vehicle_get_engine_state(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	Dictionary d;
	const PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, d);
	d["rpm"] = vehicle->get_engine_rpm();
	d["gear"] = vehicle->get_engine_gear();
	d["clutch"] = vehicle->get_clutch_resp();
	return d;
}

void PhysXServer3D::vehicle_set_ackermann_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_ackermann_params(p_params);
}

PackedFloat32Array PhysXServer3D::vehicle_get_wheel_steer_angles(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	const PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, PackedFloat32Array());
	return vehicle->get_wheel_steer_angles();
}

void PhysXServer3D::vehicle_set_balance_params(RID p_vehicle, const Dictionary &p_params) {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL(vehicle);
	vehicle->set_balance_params(p_params);
}

Dictionary PhysXServer3D::vehicle_get_balance_state(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	const PhysXVehicleServer *vehicle = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(vehicle, Dictionary());
	return vehicle->get_balance_state();
}

// ---------------------------------------------------------------------------
// ARTICULATION SKELETON (see objects/physx_articulation_3d.h)
// ---------------------------------------------------------------------------

Dictionary PhysXServer3D::vehicle_get_anti_roll_params(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *v = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(v, Dictionary());
	return v->get_anti_roll_params();
}

Dictionary PhysXServer3D::vehicle_get_response_params(RID p_vehicle) const {
	MutexLock lock(api_mutex);
	PhysXVehicleServer *v = vehicle_owner.get_or_null(p_vehicle);
	ERR_FAIL_NULL_V(v, Dictionary());
	return v->get_response_params();
}

RID PhysXServer3D::articulation_create() {
	MutexLock lock(api_mutex);
	_warn_module_api_separate_thread();
	ERR_FAIL_NULL_V_MSG(px_physics, RID(),
			"PhysX: articulation_create() called before PhysX initialization.");
	PhysXArticulation3D *articulation = memnew(PhysXArticulation3D);
	RID rid = articulation_owner.make_rid(articulation);
	articulation->set_rid(rid);
	return rid;
}

void PhysXServer3D::articulation_set_space(RID p_articulation, RID p_space) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	PhysXSpace3D *space = p_space.is_valid() ? space_owner.get_or_null(p_space) : nullptr;
	if (p_space.is_valid()) {
		ERR_FAIL_NULL_MSG(space, "PhysX: articulation_set_space passed an invalid space RID.");
	}
	articulation->set_space(space);
}

int PhysXServer3D::articulation_add_link(RID p_articulation, int p_parent_index,
		const Transform3D &p_parent_frame, const Transform3D &p_child_frame,
		int p_joint_type, float p_density, const Vector3 &p_box_half_extents) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, -1);
	return articulation->add_link(p_parent_index, p_parent_frame, p_child_frame,
			p_joint_type, p_density, p_box_half_extents);
}

void PhysXServer3D::articulation_set_drive(RID p_articulation, int p_link_index, int p_axis,
		float p_stiffness, float p_damping, float p_drive_target,
		float p_drive_velocity, int p_drive_type) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->set_drive(p_link_index, p_axis, p_stiffness, p_damping,
			p_drive_target, p_drive_velocity, p_drive_type);
}

void PhysXServer3D::articulation_set_limit(RID p_articulation, int p_link_index, int p_axis,
		float p_low, float p_high) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->set_limit(p_link_index, p_axis, p_low, p_high);
}

void PhysXServer3D::articulation_set_fix_base(RID p_articulation, bool p_fix) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->set_fix_base(p_fix);
}

void PhysXServer3D::articulation_wake(RID p_articulation) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->wake_up();
}

void PhysXServer3D::articulation_sleep(RID p_articulation) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->put_to_sleep();
}

int PhysXServer3D::articulation_get_link_count(RID p_articulation) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, 0);
	return articulation->get_link_count();
}

Transform3D PhysXServer3D::articulation_get_link_transform(RID p_articulation, int p_link_index) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, Transform3D());
	return articulation->get_link_transform(p_link_index);
}

bool PhysXServer3D::articulation_is_sleeping(RID p_articulation) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, true);
	return articulation->is_sleeping();
}

void PhysXServer3D::articulation_set_link_shape(RID p_articulation, int p_link_index, RID p_shape, const Transform3D &p_transform) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	PhysXShape3D *shape = shape_owner.get_or_null(p_shape);
	ERR_FAIL_NULL_MSG(shape, "PhysX: articulation_set_link_shape passed an invalid shape RID.");
	articulation->set_link_shape(p_link_index, shape, p_transform);
}

void PhysXServer3D::articulation_set_link_collision_layer(RID p_articulation, int p_link_index, uint32_t p_layer) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->set_link_collision_layer(p_link_index, p_layer);
}

void PhysXServer3D::articulation_set_link_collision_mask(RID p_articulation, int p_link_index, uint32_t p_mask) {
	MutexLock lock(api_mutex);
	PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL(articulation);
	articulation->set_link_collision_mask(p_link_index, p_mask);
}

uint32_t PhysXServer3D::articulation_get_link_collision_layer(RID p_articulation, int p_link_index) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, 0);
	return articulation->get_link_collision_layer(p_link_index);
}

uint32_t PhysXServer3D::articulation_get_link_collision_mask(RID p_articulation, int p_link_index) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, 0);
	return articulation->get_link_collision_mask(p_link_index);
}

Dictionary PhysXServer3D::articulation_get_link_velocity(RID p_articulation, int p_link_index) const {
	MutexLock lock(api_mutex);
	const PhysXArticulation3D *articulation = articulation_owner.get_or_null(p_articulation);
	ERR_FAIL_NULL_V(articulation, Dictionary());
	return articulation->get_link_velocity(p_link_index);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SOFT BODY (see objects/physx_soft_body_3d.h)
// ---------------------------------------------------------------------------

RID PhysXServer3D::soft_body_create() {
	PhysXSoftBody3D *soft_body = memnew(PhysXSoftBody3D);
	RID rid = soft_body_owner.make_rid(soft_body);
	soft_body->set_rid(rid);
	return rid;
}

void PhysXServer3D::soft_body_set_solver_mode(RID p_body, int p_mode) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_solver_mode(p_mode);
	// Re-resolve the path now: a mode flip on a built body must not wait for
	// an unrelated rebuild trigger (same contract as set_space/set_mesh).
	soft_body->set_space(soft_body->get_space());
}

int PhysXServer3D::soft_body_get_solver_mode(RID p_body) const {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, -1);
	return soft_body->get_solver_mode();
}

void PhysXServer3D::soft_body_attach_point_to_body(RID p_body, int p_point_index, RID p_rigid_body) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	// Attachment creation mutates the deformable's constraint set — a
	// mid-solve create is not legal (async stepping). Fetch first.
	if (soft_body->get_space()) {
		soft_body->get_space()->ensure_synced();
	}
	soft_body->attach_point_to_body(p_point_index, p_rigid_body);
}

void PhysXServer3D::soft_body_detach_point_from_body(RID p_body, int p_point_index) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	if (soft_body->get_space()) {
		soft_body->get_space()->ensure_synced();
	}
	soft_body->detach_point_from_body(p_point_index);
}

void PhysXServer3D::soft_body_update_rendering_server(RID p_body, PhysicsServer3DRenderingServerHandler *p_rendering_server_handler) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->update_rendering_server(p_rendering_server_handler);
}

void PhysXServer3D::soft_body_set_space(RID p_body, RID p_space) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	if (p_space.is_valid()) {
		ERR_FAIL_NULL_MSG(space, "PhysX: soft_body_set_space passed an invalid space RID.");
	}
	soft_body->set_space(space);
}

RID PhysXServer3D::soft_body_get_space(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, RID());
	return soft_body->get_space() ? soft_body->get_space()->get_rid() : RID();
}

void PhysXServer3D::soft_body_set_ray_pickable(RID p_body, bool p_enable) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_ray_pickable(p_enable);
}

void PhysXServer3D::soft_body_set_collision_layer(RID p_body, uint32_t p_layer) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_collision_layer(p_layer);
}

uint32_t PhysXServer3D::soft_body_get_collision_layer(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, 0);
	return soft_body->get_collision_layer();
}

void PhysXServer3D::soft_body_set_collision_mask(RID p_body, uint32_t p_mask) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_collision_mask(p_mask);
}

uint32_t PhysXServer3D::soft_body_get_collision_mask(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, 0);
	return soft_body->get_collision_mask();
}

void PhysXServer3D::soft_body_add_collision_exception(RID p_body, RID p_excepted_body) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->add_collision_exception(p_excepted_body);

	// GPU path: route the exception through the filter-shader registry — the
	// deformable solver consults no per-vertex query filters, so without this
	// the exception is inert on GPU bodies. Only rigid-body targets
	// participate (soft-vs-soft exceptions stay CPU-path only).
	PhysXBody3D *other = body_owner.get_or_null(p_excepted_body);
	if (!other) {
		return;
	}
	const uint32_t soft_slot = soft_body->get_or_alloc_exception_slot();
	const uint32_t body_slot = other->get_or_alloc_exception_slot();
	other->refresh_collision_filters();
	soft_body->set_exception_slot(soft_slot);
	g_physx_soft_exceptions.add(soft_slot, body_slot);
}

void PhysXServer3D::soft_body_remove_collision_exception(RID p_body, RID p_excepted_body) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->remove_collision_exception(p_excepted_body);
	PhysXBody3D *other = body_owner.get_or_null(p_excepted_body);
	if (other && soft_body->get_exception_slot() != 0 && other->get_exception_slot() != 0) {
		g_physx_soft_exceptions.remove(soft_body->get_exception_slot(), other->get_exception_slot());
	}
}

void PhysXServer3D::soft_body_get_collision_exceptions(RID p_body, List<RID> *p_exceptions) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->get_collision_exceptions(p_exceptions);
}

void PhysXServer3D::soft_body_set_state(RID p_body, PhysicsServer3D::BodyState p_state, const Variant &p_value) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_state(p_state, p_value);
}

Variant PhysXServer3D::soft_body_get_state(RID p_body, PhysicsServer3D::BodyState p_state) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, Variant());
	return soft_body->get_state(p_state);
}

void PhysXServer3D::soft_body_set_transform(RID p_body, const Transform3D &p_transform) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_transform(p_transform);
}

void PhysXServer3D::soft_body_apply_point_impulse(RID p_body, int p_point_index, const Vector3 &p_impulse) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->apply_point_impulse(p_point_index, p_impulse);
}

void PhysXServer3D::soft_body_apply_point_force(RID p_body, int p_point_index, const Vector3 &p_force) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	const double dt = soft_body->get_space() ? soft_body->get_space()->get_last_step() : 0.0;
	soft_body->apply_point_force(p_point_index, p_force, dt);
}

void PhysXServer3D::soft_body_apply_central_impulse(RID p_body, const Vector3 &p_impulse) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->apply_central_impulse(p_impulse);
}

void PhysXServer3D::soft_body_apply_central_force(RID p_body, const Vector3 &p_force) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	const double dt = soft_body->get_space() ? soft_body->get_space()->get_last_step() : 0.0;
	soft_body->apply_central_force(p_force, dt);
}

bool PhysXServer3D::soft_body_is_gpu(RID p_body) const {
	MutexLock lock(api_mutex);
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, false);
	return soft_body->is_gpu();
}

void PhysXServer3D::soft_body_set_simulation_precision(RID p_body, int p_precision) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_simulation_precision(p_precision);
}

int PhysXServer3D::soft_body_get_simulation_precision(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, 0);
	return soft_body->get_simulation_precision();
}

void PhysXServer3D::soft_body_set_total_mass(RID p_body, real_t p_total_mass) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_total_mass(p_total_mass);
}

real_t PhysXServer3D::soft_body_get_total_mass(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_total_mass();
}

void PhysXServer3D::soft_body_set_linear_stiffness(RID p_body, real_t p_coefficient) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_linear_stiffness(p_coefficient);
}

real_t PhysXServer3D::soft_body_get_linear_stiffness(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_linear_stiffness();
}

void PhysXServer3D::soft_body_set_shrinking_factor(RID p_body, real_t p_shrinking_factor) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_shrinking_factor(p_shrinking_factor);
}

real_t PhysXServer3D::soft_body_get_shrinking_factor(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_shrinking_factor();
}

void PhysXServer3D::soft_body_set_pressure_coefficient(RID p_body, real_t p_coefficient) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_pressure_coefficient(p_coefficient);
}

real_t PhysXServer3D::soft_body_get_pressure_coefficient(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_pressure_coefficient();
}

void PhysXServer3D::soft_body_set_damping_coefficient(RID p_body, real_t p_coefficient) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_damping_coefficient(p_coefficient);
}

real_t PhysXServer3D::soft_body_get_damping_coefficient(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_damping_coefficient();
}

void PhysXServer3D::soft_body_set_drag_coefficient(RID p_body, real_t p_coefficient) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_drag_coefficient(p_coefficient);
}

real_t PhysXServer3D::soft_body_get_drag_coefficient(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, real_t());
	return soft_body->get_drag_coefficient();
}

void PhysXServer3D::soft_body_set_mesh(RID p_body, RID p_mesh) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->set_mesh(p_mesh);
}

AABB PhysXServer3D::soft_body_get_bounds(RID p_body) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, AABB());
	return soft_body->get_bounds();
}

void PhysXServer3D::soft_body_move_point(RID p_body, int p_point_index, const Vector3 &p_global_position) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->move_point(p_point_index, p_global_position);
}

Vector3 PhysXServer3D::soft_body_get_point_global_position(RID p_body, int p_point_index) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, Vector3());
	return soft_body->get_point_global_position(p_point_index);
}

void PhysXServer3D::soft_body_remove_all_pinned_points(RID p_body) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->unpin_all();
}

void PhysXServer3D::soft_body_pin_point(RID p_body, int p_point_index, bool p_pin) {
	PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(soft_body);
	soft_body->pin_point(p_point_index, p_pin);
}

bool PhysXServer3D::soft_body_is_point_pinned(RID p_body, int p_point_index) const {
	const PhysXSoftBody3D *soft_body = soft_body_owner.get_or_null(p_body);
	ERR_FAIL_NULL_V(soft_body, false);
	return soft_body->is_point_pinned(p_point_index);
}

/* PARTICLE FLUID */

bool PhysXServer3D::particle_fluid_supports_diffuse() const {
	return PhysXGPUParticleFluid3D::supports_diffuse_particles();
}

RID PhysXServer3D::particle_fluid_create() {
	MutexLock lock(api_mutex);
	_warn_module_api_separate_thread();
	PhysXGPUParticleFluid3D *fluid = memnew(PhysXGPUParticleFluid3D);
	RID rid = fluid_owner.make_rid(fluid);
	fluid->set_self(rid);
	return rid;
}

void PhysXServer3D::particle_fluid_set_space(RID p_fluid, RID p_space) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	fluid->set_space(space);
}

void PhysXServer3D::particle_fluid_set_param(RID p_fluid, int p_param, real_t p_value) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	ERR_FAIL_INDEX(p_param, PhysXGPUParticleFluid3D::PARAM_MAX);
	fluid->set_param((PhysXGPUParticleFluid3D::Param)p_param, p_value);
}

void PhysXServer3D::particle_fluid_set_capacity(RID p_fluid, int p_max) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_capacity(p_max > 0 ? (uint32_t)p_max : 1);
}

void PhysXServer3D::particle_fluid_set_granular(RID p_fluid, bool p_enabled, real_t p_friction) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_granular(p_enabled, p_friction);
}

void PhysXServer3D::particle_fluid_set_particles(RID p_fluid, const Vector<Vector3> &p_positions, const Vector3 &p_initial_velocity) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_particles(p_positions, p_initial_velocity);
}

void PhysXServer3D::particle_fluid_emit(RID p_fluid, const Vector<Vector3> &p_positions, const Vector3 &p_velocity) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->emit(p_positions, p_velocity);
}

void PhysXServer3D::particle_fluid_clear(RID p_fluid) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->clear();
}

void PhysXServer3D::particle_fluid_set_foam(RID p_fluid, bool p_enabled, int p_capacity, real_t p_lifetime, real_t p_threshold, real_t p_buoyancy, real_t p_size) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_foam_capacity(p_capacity > 0 ? (uint32_t)p_capacity : 1);
	fluid->set_foam_lifetime(p_lifetime);
	fluid->set_foam_threshold(p_threshold);
	fluid->set_foam_buoyancy(p_buoyancy);
	fluid->set_foam_size(p_size);
	fluid->set_foam_enabled(p_enabled);
}

Vector<Vector3> PhysXServer3D::particle_fluid_get_foam_positions(RID p_fluid) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, Vector<Vector3>());
	const LocalVector<Vector3> &src = fluid->get_foam_positions();
	Vector<Vector3> out;
	out.resize(src.size());
	if (src.size() > 0) {
		memcpy(out.ptrw(), src.ptr(), src.size() * sizeof(Vector3));
	}
	return out;
}

int PhysXServer3D::particle_fluid_get_foam_count(RID p_fluid) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0);
	return (int)fluid->get_foam_count();
}

void PhysXServer3D::particle_fluid_set_surface_mesh(RID p_fluid, bool p_enabled) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_surface_mesh_enabled(p_enabled);
}

int PhysXServer3D::particle_fluid_get_surface_triangle_count(RID p_fluid) const {
	MutexLock lock(api_mutex);
	const PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0);
	return (int)fluid->get_surface_triangle_count();
}

void PhysXServer3D::particle_fluid_set_surface_anisotropy(RID p_fluid, bool p_enabled) {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL(fluid);
	fluid->set_surface_anisotropy_enabled(p_enabled);
}

int PhysXServer3D::particle_fluid_get_surface_mesh(RID p_fluid, PackedVector3Array &r_vertices, PackedVector3Array &r_normals, PackedInt32Array &r_indices, uint32_t &r_version) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0);
	LocalVector<Vector3> v, n;
	LocalVector<int32_t> idx;
	const uint32_t tris = fluid->copy_surface_mesh(v, n, idx, r_version);
	if (tris == UINT32_MAX) {
		return -1; // unchanged; caller keeps its current mesh
	}
	r_vertices.resize(v.size());
	r_normals.resize(n.size());
	r_indices.resize(idx.size());
	if (v.size() > 0) {
		memcpy(r_vertices.ptrw(), v.ptr(), v.size() * sizeof(Vector3));
	}
	if (n.size() > 0) {
		memcpy(r_normals.ptrw(), n.ptr(), n.size() * sizeof(Vector3));
	}
	if (idx.size() > 0) {
		memcpy(r_indices.ptrw(), idx.ptr(), idx.size() * sizeof(int32_t));
	}
	return (int)tris;
}

int PhysXServer3D::particle_fluid_get_foam_mesh(RID p_fluid, PackedVector3Array &r_vertices, PackedVector3Array &r_normals, PackedInt32Array &r_indices, uint32_t &r_version) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0);
	LocalVector<Vector3> v, n;
	LocalVector<int32_t> idx;
	const uint32_t tris = fluid->copy_foam_mesh(v, n, idx, r_version);
	if (tris == UINT32_MAX) {
		return -1;
	}
	r_vertices.resize(v.size());
	r_normals.resize(n.size());
	r_indices.resize(idx.size());
	if (v.size() > 0) {
		memcpy(r_vertices.ptrw(), v.ptr(), v.size() * sizeof(Vector3));
	}
	if (n.size() > 0) {
		memcpy(r_normals.ptrw(), n.ptr(), n.size() * sizeof(Vector3));
	}
	if (idx.size() > 0) {
		memcpy(r_indices.ptrw(), idx.ptr(), idx.size() * sizeof(int32_t));
	}
	return (int)tris;
}

Vector<Vector3> PhysXServer3D::particle_fluid_get_positions(RID p_fluid) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, Vector<Vector3>());
	const LocalVector<Vector3> &src = fluid->get_positions();
	Vector<Vector3> out;
	out.resize(src.size());
	if (src.size() > 0) {
		memcpy(out.ptrw(), src.ptr(), src.size() * sizeof(Vector3));
	}
	return out;
}

int PhysXServer3D::particle_fluid_get_particle_count(RID p_fluid) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0);
	return (int)fluid->get_particle_count();
}

real_t PhysXServer3D::particle_fluid_get_submersion(RID p_fluid, const AABB &p_world_aabb) const {
	MutexLock lock(api_mutex);
	PhysXGPUParticleFluid3D *fluid = fluid_owner.get_or_null(p_fluid);
	ERR_FAIL_NULL_V(fluid, 0.0);
	return fluid->get_submersion(p_world_aabb);
}

/* GPU CLOTH */

RID PhysXServer3D::cloth_create() {
	MutexLock lock(api_mutex);
	_warn_module_api_separate_thread();
	if (!px_cuda_context) {
		return RID(); // no CUDA -> the node uses its CPU fallback
	}
	PhysXGPUCloth3D *cloth = memnew(PhysXGPUCloth3D);
	RID rid = cloth_owner.make_rid(cloth);
	cloth->set_self(rid);
	return rid;
}

void PhysXServer3D::cloth_set_space(RID p_cloth, RID p_space) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->set_space(space_owner.get_or_null(p_space));
}

void PhysXServer3D::cloth_set_params(RID p_cloth, real_t p_thickness, real_t p_density, real_t p_stretch, real_t p_bend, real_t p_damping, uint32_t p_collision_mask) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->set_params(p_thickness, p_density, p_stretch, p_bend, p_damping, p_collision_mask);
}

void PhysXServer3D::cloth_set_collision_layer_and_mask(RID p_cloth, uint32_t p_layer, uint32_t p_mask) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->set_collision_layer(p_layer);
	// Mask rides the same filter push; keep the stored value in sync for the
	// next rebuild.
	cloth->set_collision_mask(p_mask);
}

void PhysXServer3D::cloth_add_collision_exception(RID p_cloth, RID p_body) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	PhysXBody3D *other = body_owner.get_or_null(p_body);
	ERR_FAIL_NULL(other);
	// Same slot-registry routing as rigid-body exceptions: word2 slots on both
	// sides, enforced by the filter shader (the deformable surface's shape has
	// no userData, so the CPU pair-filter path cannot see it).
	const uint32_t slot_a = cloth->get_or_alloc_exception_slot();
	const uint32_t slot_b = other->get_or_alloc_exception_slot();
	other->refresh_collision_filters();
	g_physx_soft_exceptions.add(slot_a, slot_b);
}

void PhysXServer3D::cloth_remove_collision_exception(RID p_cloth, RID p_body) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	PhysXBody3D *other = body_owner.get_or_null(p_body);
	if (!other || cloth->get_exception_slot() == 0 || other->get_exception_slot() == 0) {
		return;
	}
	g_physx_soft_exceptions.remove(cloth->get_exception_slot(), other->get_exception_slot());
}

void PhysXServer3D::cloth_build(RID p_cloth, const Vector<Vector3> &p_positions, const Vector<int32_t> &p_indices, const Transform3D &p_xform) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->build(p_positions, p_indices, p_xform);
}

void PhysXServer3D::cloth_set_pinned(RID p_cloth, const Vector<int32_t> &p_pinned) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->set_pinned(p_pinned);
}

void PhysXServer3D::cloth_set_pin_targets(RID p_cloth, const Vector<Vector3> &p_world_targets) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->set_pin_targets(p_world_targets);
}

void PhysXServer3D::cloth_apply_wind(RID p_cloth, const Vector3 &p_wind, real_t p_drag, real_t p_lift, real_t p_dt) {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL(cloth);
	cloth->apply_wind(p_wind, p_drag, p_lift, p_dt);
}

bool PhysXServer3D::cloth_is_ready(RID p_cloth) const {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	return cloth && cloth->is_ready();
}

int PhysXServer3D::cloth_get_mesh(RID p_cloth, PackedVector3Array &r_positions, PackedInt32Array &r_indices, uint32_t &r_version) const {
	MutexLock lock(api_mutex);
	PhysXGPUCloth3D *cloth = cloth_owner.get_or_null(p_cloth);
	ERR_FAIL_NULL_V(cloth, 0);
	LocalVector<Vector3> p;
	LocalVector<int32_t> idx;
	const uint32_t tris = cloth->copy_mesh(p, idx, r_version);
	if (tris == UINT32_MAX) {
		return -1;
	}
	r_positions.resize(p.size());
	r_indices.resize(idx.size());
	if (p.size() > 0) {
		memcpy(r_positions.ptrw(), p.ptr(), p.size() * sizeof(Vector3));
	}
	if (idx.size() > 0) {
		memcpy(r_indices.ptrw(), idx.ptr(), idx.size() * sizeof(int32_t));
	}
	return (int)tris;
}

RID PhysXServer3D::joint_create() {
	PhysXJoint3D *joint = memnew(PhysXJoint3D());
	RID rid = joint_owner.make_rid(joint);
	joint->set_rid(rid);
	return rid;
}

// Joint creation inserts a PxConstraint into the owning scene(s) (and
// joint_make_* first releases the previous joint's constraint) — both are
// forbidden while a solve is in flight (async stepping). Resolved purely from
// RIDs and called BEFORE any pointer is cached, so everything below resolves
// fresh state after the fetch and needs no re-validation.
static void _physx_sync_spaces_for_joint(RID p_body_a, RID p_body_b) {
	for (const RID &rid : { p_body_a, p_body_b }) {
		if (!rid.is_valid()) {
			continue;
		}
		PhysXBody3D *body = PhysXServer3D::get_singleton()->get_body(rid);
		if (body && body->get_space()) {
			body->get_space()->ensure_synced();
		}
	}
}

void PhysXServer3D::joint_clear(RID p_joint) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->release();
}

void PhysXServer3D::joint_make_pin(RID p_joint, RID p_body_a, const Vector3 &p_local_a, RID p_body_b, const Vector3 &p_local_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);

	// Resolve both bodies. body_a is required; body_b may be invalid, in which
	// case the joint is anchored to the world frame (PhysX accepts a NULL actor
	// for either side). This matches Godot's contract: a single-body joint pins
	// the body to a fixed point in world space.
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: pin joint body_a is not a rigid body.");
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	physx::PxTransform local_a(physx::PxVec3(p_local_a.x, p_local_a.y, p_local_a.z));
	physx::PxTransform local_b(physx::PxVec3(p_local_b.x, p_local_b.y, p_local_b.z));

	joint->make(PhysicsServer3D::JOINT_TYPE_PIN, PhysXJoint3D::JOINT_KIND_PIN, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::pin_joint_set_param(RID p_joint, PhysicsServer3D::PinJointParam p_param, real_t p_value) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_pin_param(p_param, p_value);
}

real_t PhysXServer3D::pin_joint_get_param(RID p_joint, PhysicsServer3D::PinJointParam p_param) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, real_t());
	return joint->get_pin_param(p_param);
}

void PhysXServer3D::pin_joint_set_local_a(RID p_joint, const Vector3 &p_local_a) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_local_a(p_local_a);
}

Vector3 PhysXServer3D::pin_joint_get_local_a(RID p_joint) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, Vector3());
	return joint->get_local_a();
}

void PhysXServer3D::pin_joint_set_local_b(RID p_joint, const Vector3 &p_local_b) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_local_b(p_local_b);
}

Vector3 PhysXServer3D::pin_joint_get_local_b(RID p_joint) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, Vector3());
	return joint->get_local_b();
}

// godot_physics_3d hinge axis (frame local Z) -> PhysX revolute axis (frame local X).
static const physx::PxQuat HINGE_Z_TO_X(-physx::PxHalfPi, physx::PxVec3(0.0f, 1.0f, 0.0f));

void PhysXServer3D::joint_make_hinge(RID p_joint, RID p_body_a, const Transform3D &p_hinge_a, RID p_body_b, const Transform3D &p_hinge_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: hinge joint body_a is not a rigid body.");
	// body_b optional —anchors to the world frame when invalid.
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	// godot_physics_3d (the reference contract) constrains the transform-
	// variant hinge about the joint frame's local Z axis; PhysX revolute
	// joints rotate about the frame's local X. Composing this rotation into
	// both frames maps Godot's hinge axis onto PhysX's without touching the
	// pivot geometry. -90deg about +Y maps the PhysX frame X axis onto the
	// Godot frame +Z, preserving the right-hand sign of limits and motor
	// velocities (verified: a +4 motor target spins +Z, not -Z).
	physx::PxTransform local_a = PhysXShapedObject3D::to_physx_transform(p_hinge_a) * physx::PxTransform(HINGE_Z_TO_X);
	physx::PxTransform local_b = PhysXShapedObject3D::to_physx_transform(p_hinge_b) * physx::PxTransform(HINGE_Z_TO_X);

	joint->make(PhysicsServer3D::JOINT_TYPE_HINGE, PhysXJoint3D::JOINT_KIND_HINGE, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::joint_make_hinge_simple(RID p_joint, RID p_body_a, const Vector3 &p_pivot_a, const Vector3 &p_axis_a, RID p_body_b, const Vector3 &p_pivot_b, const Vector3 &p_axis_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: hinge joint body_a is not a rigid body.");
	// body_b optional —anchors to the world frame when invalid.
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	// The simple variant's axis lands directly on the PhysX revolute X
	// (compute_joint_frame builds the frame with the axis on X), so no Z->X
	// remap is needed here -- unlike the transform variant above.
	physx::PxVec3 pivot_a(p_pivot_a.x, p_pivot_a.y, p_pivot_a.z);
	physx::PxVec3 pivot_b(p_pivot_b.x, p_pivot_b.y, p_pivot_b.z);
	physx::PxVec3 axis_a(p_axis_a.x, p_axis_a.y, p_axis_a.z);
	physx::PxVec3 axis_b(p_axis_b.x, p_axis_b.y, p_axis_b.z);

	physx::PxTransform local_a = PhysXJoint3D::compute_joint_frame(pivot_a, axis_a);
	physx::PxTransform local_b = PhysXJoint3D::compute_joint_frame(pivot_b, axis_b);

	joint->make(PhysicsServer3D::JOINT_TYPE_HINGE, PhysXJoint3D::JOINT_KIND_HINGE, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::hinge_joint_set_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param, real_t p_value) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_hinge_param(p_param, p_value);
}

real_t PhysXServer3D::hinge_joint_get_param(RID p_joint, PhysicsServer3D::HingeJointParam p_param) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, real_t());
	return joint->get_hinge_param(p_param);
}

void PhysXServer3D::hinge_joint_set_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag, bool p_enabled) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_hinge_flag(p_flag, p_enabled);
}

bool PhysXServer3D::hinge_joint_get_flag(RID p_joint, PhysicsServer3D::HingeJointFlag p_flag) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);
	return joint->get_hinge_flag(p_flag);
}

void PhysXServer3D::joint_make_slider(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: slider joint body_a is not a rigid body.");
	// body_b optional —anchors to the world frame when invalid.
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	physx::PxTransform local_a = PhysXShapedObject3D::to_physx_transform(p_local_ref_a);
	physx::PxTransform local_b = PhysXShapedObject3D::to_physx_transform(p_local_ref_b);

	joint->make(PhysicsServer3D::JOINT_TYPE_SLIDER, PhysXJoint3D::JOINT_KIND_SLIDER, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::slider_joint_set_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param, real_t p_value) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_slider_param(p_param, p_value);
}

real_t PhysXServer3D::slider_joint_get_param(RID p_joint, PhysicsServer3D::SliderJointParam p_param) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, real_t());
	return joint->get_slider_param(p_param);
}

void PhysXServer3D::joint_make_cone_twist(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: cone/twist joint body_a is not a rigid body.");
	// body_b optional —anchors to the world frame when invalid.
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	physx::PxTransform local_a = PhysXShapedObject3D::to_physx_transform(p_local_ref_a);
	physx::PxTransform local_b = PhysXShapedObject3D::to_physx_transform(p_local_ref_b);

	joint->make(PhysicsServer3D::JOINT_TYPE_CONE_TWIST, PhysXJoint3D::JOINT_KIND_CONE_TWIST, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::cone_twist_joint_set_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param, real_t p_value) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_cone_twist_param(p_param, p_value);
}

real_t PhysXServer3D::cone_twist_joint_get_param(RID p_joint, PhysicsServer3D::ConeTwistJointParam p_param) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, real_t());
	return joint->get_cone_twist_param(p_param);
}

void PhysXServer3D::joint_make_generic_6dof(RID p_joint, RID p_body_a, const Transform3D &p_local_ref_a, RID p_body_b, const Transform3D &p_local_ref_b) {
	// Fetch any in-flight solve BEFORE resolving pointers (see helper comment).
	_physx_sync_spaces_for_joint(p_body_a, p_body_b);

	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	PhysXBody3D *body_a = body_owner.get_or_null(p_body_a);
	ERR_FAIL_NULL_MSG(body_a, "PhysX: 6DOF joint body_a is not a rigid body.");
	// body_b optional —anchors to the world frame when invalid.
	PhysXBody3D *body_b = p_body_b.is_valid() ? body_owner.get_or_null(p_body_b) : nullptr;

	physx::PxTransform local_a = PhysXShapedObject3D::to_physx_transform(p_local_ref_a);
	physx::PxTransform local_b = PhysXShapedObject3D::to_physx_transform(p_local_ref_b);

	// Godot semantics: a fresh 6DOF joint has every axis FREE (limits apply
	// only where the enable flags say so) -- the wrapper's _apply_params()
	// pushes that from the cached flags, both now and after any rebuild.
	joint->make(PhysicsServer3D::JOINT_TYPE_6DOF, PhysXJoint3D::JOINT_KIND_6DOF, body_a, local_a, body_b, local_b);
}

void PhysXServer3D::generic_6dof_joint_set_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param, real_t p_value) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_g6dof_param(p_axis, p_param, p_value);
}

real_t PhysXServer3D::generic_6dof_joint_get_param(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisParam p_param) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, real_t());
	return joint->get_g6dof_param(p_axis, p_param);
}

void PhysXServer3D::generic_6dof_joint_set_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag, bool p_enable) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_g6dof_flag(p_axis, p_flag, p_enable);
}

bool PhysXServer3D::generic_6dof_joint_get_flag(RID p_joint, Vector3::Axis p_axis, PhysicsServer3D::G6DOFJointAxisFlag p_flag) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);
	return joint->get_g6dof_flag(p_axis, p_flag);
}

PhysicsServer3D::JointType PhysXServer3D::joint_get_type(RID p_joint) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, PhysicsServer3D::JointType());
	switch (joint->get_kind()) {
		case PhysXJoint3D::JOINT_KIND_PIN:
			return PhysicsServer3D::JOINT_TYPE_PIN;
		case PhysXJoint3D::JOINT_KIND_HINGE:
			return PhysicsServer3D::JOINT_TYPE_HINGE;
		case PhysXJoint3D::JOINT_KIND_SLIDER:
			return PhysicsServer3D::JOINT_TYPE_SLIDER;
		case PhysXJoint3D::JOINT_KIND_CONE_TWIST:
			return PhysicsServer3D::JOINT_TYPE_CONE_TWIST;
		case PhysXJoint3D::JOINT_KIND_6DOF:
			return PhysicsServer3D::JOINT_TYPE_6DOF;
		default:
			return PhysicsServer3D::JOINT_TYPE_MAX;
	}
}

void PhysXServer3D::joint_set_solver_priority(RID p_joint, int p_priority) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_solver_priority(p_priority);
}

int PhysXServer3D::joint_get_solver_priority(RID p_joint) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, 0);
	return joint->get_solver_priority();
}

void PhysXServer3D::joint_disable_collisions_between_bodies(RID p_joint, bool p_disable) {
	PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL(joint);
	joint->set_disable_collisions(p_disable);
}

bool PhysXServer3D::joint_is_disabled_collisions_between_bodies(RID p_joint) const {
	const PhysXJoint3D *joint = joint_owner.get_or_null(p_joint);
	ERR_FAIL_NULL_V(joint, false);
	return joint->is_disabled_collisions();
}

// Frees a RID and its wrapper: dispatches on the owning RID table. Branch
// comments below carry each kind's teardown ordering.
void PhysXServer3D::free(RID p_rid) {
	MutexLock lock(api_mutex);
	// Freeing a null/invalid RID is a no-op (standard Godot convention —the
	// scene tree may free a RID that body_create() returned empty because PhysX
	// wasn't initialized yet during a fresh-compile editor boot, or a RID that
	// was already freed during a scene reload). Don't spam the log for these.
	if (!p_rid.is_valid()) {
		return;
	}
	if (shape_owner.owns(p_rid)) {
		PhysXShape3D *s = shape_owner.get_or_null(p_rid);

		// Remove the shape from all bodies/areas that reference it.
		// This prevents dangling PxShape->userData pointers when queries or
		// callbacks try to recover the shape after it's been freed.
		while (s->get_owners().size()) {
			PhysXShapedObject3D *so = const_cast<PhysXShapedObject3D *>(s->get_owners().begin()->key);
			so->remove_shape(s);
		}

		shape_owner.free(p_rid);
		memdelete(s);
		return;
	}
	if (body_owner.owns(p_rid)) {
		PhysXBody3D *b = body_owner.get_or_null(p_rid);

		// Fetch an in-flight solve before teardown: everything below releases
		// the actor, its joints and its vehicles — structural scene mutations
		// that are forbidden mid-solve (async stepping). The fetch fires
		// state-sync callbacks, so re-validate the RID in case one of them
		// freed this body re-entrantly.
		if (b->get_space()) {
			b->get_space()->ensure_synced();
		}
		if (!body_owner.owns(p_rid)) {
			return;
		}
		b = body_owner.get_or_null(p_rid);

		// Release any soft-body rigid attachments referencing this body BEFORE
		// the space detach/actor release: a live PxDeformableAttachment
		// must never dangle on a freed rigid.
		if (b->get_rigid_actor() && b->get_space()) {
			b->get_space()->release_soft_body_attachments_for(b->get_rigid_actor());
		}

		// Detach from the space first (godot-physics parity): unregisters the
		// body, removes the actor from the scene, and queues body_exited
		// events for overlapping areas while the body is still alive.
		b->set_space(nullptr);

		// Vehicles referencing this body as chassis are released here; joints
		// are notified from ~PhysXBody3D (body_removed) so their wrappers --
		// and the RIDs the Joint3D nodes hold -- stay valid.
		release_vehicles_for_body(b);
		// Drop grip-table references from surviving vehicles: memdelete(b) below
		// releases the body's PxMaterial, which those tables hold raw pointers to.
		invalidate_vehicles_surface_pairs(b);
		// Drop the body's word2 exception-slot registry entries: the slot dies
		// with the body and slots are never reused, but the registry must not
		// accumulate dead keys (mirrors the soft-body free path).
		if (b->get_exception_slot() != 0) {
			g_physx_soft_exceptions.remove_soft(b->get_exception_slot());
		}

		body_owner.free(p_rid);
		memdelete(b);
		return;
	}
	if (area_owner.owns(p_rid)) {
		PhysXArea3D *a = area_owner.get_or_null(p_rid);

		// Same mid-solve concern as the body branch: ~PhysXArea3D detaches its
		// shapes (broadphase mutation) and releases the trigger actor, and the
		// fetch may fire callbacks that free this area re-entrantly.
		if (a->get_space()) {
			a->get_space()->ensure_synced();
		}
		if (!area_owner.owns(p_rid)) {
			return;
		}
		a = area_owner.get_or_null(p_rid);

		area_owner.free(p_rid);
		// If this was a space's default area, clear the space's pointer: the
		// space-free branch frees the default area via get_rid(), and without
		// this it would dereference the freed wrapper and double-free the RID
		// when the space itself is torn down later (finish()'s area loop runs
		// before its space loop, so a leaked space's default area is freed
		// here first).
		if (a->get_space() && a->get_space()->get_default_area() == a) {
			a->get_space()->set_default_area(nullptr);
		}
		memdelete(a);
		return;
	}
	if (soft_body_owner.owns(p_rid)) {
		PhysXSoftBody3D *sb = soft_body_owner.get_or_null(p_rid);
		if (sb->get_exception_slot() != 0) {
			g_physx_soft_exceptions.remove_soft(sb->get_exception_slot());
		}
		sb->set_space(nullptr);
		soft_body_owner.free(p_rid);
		memdelete(sb);
		return;
	}
	// articulation_owner — articulations are freed here.
	if (articulation_owner.owns(p_rid)) {
		PhysXArticulation3D *a = articulation_owner.get_or_null(p_rid);
		a->set_space(nullptr);
		articulation_owner.free(p_rid);
		memdelete(a);
		return;
	}
	if (space_owner.owns(p_rid)) {
		PhysXSpace3D *sp = space_owner.get_or_null(p_rid);

		// Tearing a space down mid-solve: the default-area teardown below
		// detaches shapes INLINE (its space pointer is already cleared by the
		// detach loop, so its own gate cannot fire), and ~PhysXSpace3D releases
		// the PxScene. Fetch the in-flight solve up front; the queued actor ops
		// are applied by that fetch, making every teardown step below legal.
		sp->ensure_synced();

		// Detach all bodies from this space before deleting it,
		// because their destructors may reference the space.
		while (!sp->get_bodies().is_empty()) {
			const_cast<PhysXBody3D *>(sp->get_bodies()[0])->set_space(nullptr);
		}

		// Detach all areas from this space before deleting it.
		while (!sp->get_areas().is_empty()) {
			const_cast<PhysXArea3D *>(sp->get_areas()[0])->set_space(nullptr);
		}

		// Detach all vehicles from this space before deleting it.
		while (!sp->get_vehicles().is_empty()) {
			const_cast<PhysXVehicleServer *>(sp->get_vehicles()[0])->set_space(nullptr);
		}

		// Detach all GPU fluids/cloths from this space before deleting it.
		while (!sp->get_fluids().is_empty()) {
			const_cast<PhysXGPUParticleFluid3D *>(sp->get_fluids()[0])->set_space(nullptr);
		}
		while (!sp->get_cloths().is_empty()) {
			const_cast<PhysXGPUCloth3D *>(sp->get_cloths()[0])->set_space(nullptr);
		}

		// Detach all articulations from this space before deleting it. Their
		// wrappers survive the space (script-held RIDs) and must not keep a
		// dangling space/scene pointer: the PxScene dies with the space, and a
		// later articulation call would dereference freed memory.
		while (!sp->get_articulations().is_empty()) {
			const_cast<PhysXArticulation3D *>(sp->get_articulations()[0])->set_space(nullptr);
		}

		// Free the default area (which belongs to this space).
		PhysXArea3D *default_area = sp->get_default_area();
		if (default_area) {
			free(default_area->get_rid());
		}

		space_owner.free(p_rid);
		memdelete(sp);
		for (uint32_t i = 0; i < _space_list.size(); i++) {
			if (_space_list[i] == p_rid) {
				_space_list.remove_at(i);
				break;
			}
		}
		return;
	}
	// joint_owner —joints are freed here.
	if (joint_owner.owns(p_rid)) {
		PhysXJoint3D *j = joint_owner.get_or_null(p_rid);
		joint_owner.free(p_rid);
		memdelete(j);
		return;
	}
	// vehicle_owner —vehicles are freed here.
	if (vehicle_owner.owns(p_rid)) {
		PhysXVehicleServer *v = vehicle_owner.get_or_null(p_rid);
		v->release();
		vehicle_owner.free(p_rid);
		memdelete(v);
		return;
	}
	// fluid_owner —GPU particle fluids are freed here.
	if (fluid_owner.owns(p_rid)) {
		PhysXGPUParticleFluid3D *f = fluid_owner.get_or_null(p_rid);
		f->set_space(nullptr);
		fluid_owner.free(p_rid);
		memdelete(f);
		return;
	}
	// cloth_owner —GPU cloths are freed here.
	if (cloth_owner.owns(p_rid)) {
		PhysXGPUCloth3D *c = cloth_owner.get_or_null(p_rid);
		c->set_space(nullptr);
		cloth_owner.free(p_rid);
		memdelete(c);
		return;
	}
	ERR_PRINT(vformat("PhysX: attempted to free unknown RID (id %d)", p_rid.get_id()));
}

void PhysXServer3D::space_set_active(RID p_space, bool p_active) {
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	ERR_FAIL_NULL(space);
	space->set_active(p_active);
}

bool PhysXServer3D::space_is_active(RID p_space) const {
	PhysXSpace3D *space = space_owner.get_or_null(p_space);
	return space && space->is_active();
}
// Bootstraps the PhysX SDK. Ordering constraints:
//  - the allocator/error callbacks must outlive the foundation (they are
//    static for that reason);
//  - PVD must exist before PxCreatePhysics, which takes the pvd pointer.
void PhysXServer3D::init() {
	// Static so the callbacks outlive px_foundation (PhysX holds raw pointers).
	static PhysXAllocatorCallback s_allocator;
	static PhysXErrorCallback s_error;
	px_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, s_allocator, s_error);
	ERR_FAIL_NULL_MSG(px_foundation, "PhysX: PxCreateFoundation failed.");

	physx::PxTolerancesScale scale; // Godot units are meters

#ifdef DEBUG_ENABLED
	// PVD must be connected BEFORE PxCreatePhysics, which takes the pvd pointer.
	// Opt-in via the project setting: connecting unconditionally made
	// every debug build dial localhost:5425 with eALL instrumentation.
	if (GLOBAL_GET("physics/physx_3d/debug/pvd")) {
		px_pvd_transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
		if (px_pvd_transport) {
			px_debugger = physx::PxCreatePvd(*px_foundation);
			if (px_debugger) {
				px_debugger->connect(*px_pvd_transport, physx::PxPvdInstrumentationFlag::eALL);
			}
		}
	}
#endif

	px_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *px_foundation, scale, false, px_debugger);
	ERR_FAIL_NULL_MSG(px_physics, "PhysX: PxCreatePhysics failed.");

	// Extensions provide joints, the default CPU dispatcher, and PVD helpers.
	extensions_initialized = PxInitExtensions(*px_physics, px_debugger);

	// Vehicle2 SDK - must be initialized after extensions (it uses the same
	// allocator) and before the default material is created.
	vehicle_sdk_initialized = physx::PxInitVehicleExtension(*px_foundation);

	// --- Module project settings (read once, before any PxScene exists) ---
	PhysXProjectSettings::read_settings();

#ifdef GODOT_PHYSX_GPU
	// GPU dynamics. Never deterministic (GPU solver scheduling varies run to
	// run), so enhanced_determinism forces the CPU path even when a device is
	// present. A GPU build without a usable CUDA device falls back to CPU.
	if (!PhysXProjectSettings::enhanced_determinism) {
#if defined(__linux__)
		// The GPU runtime is linked into the engine and staged next to it by
		// the module SCsub (physx_gpu=yes only); this dlopen acquires its
		// handle to fetch the entry point through. Chain: preload the CUDA
		// driver (its cu* symbols must sit in the global namespace before
		// the runtime's deferred cu* references resolve), then the runtime,
		// then fetch the GPU entry point. Any failure falls back to CPU
		// simulation.
		void *cuda_driver = dlopen("libcuda.so.1", RTLD_NOW | RTLD_GLOBAL);
		if (cuda_driver == nullptr) {
			WARN_PRINT(vformat("PhysX: libcuda.so.1 could not be loaded (%s); falling back to CPU simulation.", dlerror()));
		} else {
			// The shipped layout places the runtime next to the engine binary;
			// a bare-name dlopen does not search the executable's directory, so
			// try the binary-relative path first and the system search path last.
			const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
			const String runtime_paths[2] = {
				exe_dir.path_join("libPhysXGpu_64.so"),
				"libPhysXGpu_64.so",
			};
			void *gpu_runtime = nullptr;
			String load_error;
			for (const String &runtime_path : runtime_paths) {
				gpu_runtime = dlopen(runtime_path.utf8().get_data(), RTLD_NOW | RTLD_GLOBAL);
				if (gpu_runtime != nullptr) {
					break;
				}
				const char *load_err = dlerror();
				load_error = load_err != nullptr ? String::utf8(load_err) : String();
			}
			if (gpu_runtime == nullptr) {
				WARN_PRINT(vformat("PhysX: libPhysXGpu_64.so could not be loaded (%s); falling back to CPU simulation.", load_error));
			} else {
				using PxCudaContextManagerCreateFn = physx::PxCudaContextManager *(*)(const physx::PxFoundation &, const physx::PxCudaContextManagerDesc &, physx::PxProfilerCallback *);
				PxCudaContextManagerCreateFn create_cuda_context =
						reinterpret_cast<PxCudaContextManagerCreateFn>(dlsym(gpu_runtime, "PxCreateCudaContextManager"));
				if (create_cuda_context == nullptr) {
					WARN_PRINT(vformat("PhysX: PxCreateCudaContextManager missing from libPhysXGpu_64.so (%s); falling back to CPU simulation.", dlerror()));
				} else {
					physx::PxCudaContextManagerDesc cuda_desc;
					px_cuda_context = create_cuda_context(*px_foundation, cuda_desc, PxGetProfilerCallback());
					if (px_cuda_context && !px_cuda_context->contextIsValid()) {
						px_cuda_context->release();
						px_cuda_context = nullptr;
					}
					if (px_cuda_context) {
						print_line(vformat("PhysX: CUDA context ready on device '%s' -> GPU dynamics available.", px_cuda_context->getDeviceName()));
						gpu_dynamics_enabled = true;
					} else {
						WARN_PRINT("PhysX: no usable CUDA device; falling back to CPU simulation.");
					}
				}
			}
		}
#else
		{
			physx::PxCudaContextManagerDesc cuda_desc;
			px_cuda_context = PxCreateCudaContextManager(*px_foundation, cuda_desc, PxGetProfilerCallback());
			if (px_cuda_context && !px_cuda_context->contextIsValid()) {
				px_cuda_context->release();
				px_cuda_context = nullptr;
			}
			if (px_cuda_context) {
				print_line(vformat("PhysX: CUDA context ready on device '%s' -> GPU dynamics available.", px_cuda_context->getDeviceName()));
				gpu_dynamics_enabled = true;
			} else {
				WARN_PRINT("PhysX: no usable CUDA device; falling back to CPU simulation.");
			}
		}
#endif
	} else {
		print_verbose("PhysX: enhanced_determinism is set -> GPU dynamics disabled (the GPU solver is not deterministic).");
	}
#endif

	// --- Shared CPU dispatcher (one pool for every PxScene) ---
	// The CPU solver scales with worker count, so give it most of the machine
	// (leaving a core for the main thread + render). With GPU dynamics the CPU
	// mostly waits on the GPU each step, so extra workers only add coordination
	// overhead —keep that pool small. physics/physx_3d/simulation/
	// cpu_worker_threads overrides (0 = auto).
	const int cpu_count = OS::get_singleton()->get_processor_count();
	uint32_t worker_threads;
	if (PhysXProjectSettings::cpu_worker_threads > 0) {
		worker_threads = (uint32_t)PhysXProjectSettings::cpu_worker_threads;
	} else {
		worker_threads = gpu_dynamics_enabled
				? (uint32_t)CLAMP(cpu_count / 4, 2, 4)
				: (uint32_t)CLAMP(cpu_count - 1, 1, 16);
	}
	px_cpu_dispatcher = physx::PxDefaultCpuDispatcherCreate(worker_threads);
	ERR_FAIL_NULL_MSG(px_cpu_dispatcher, "PhysX: PxDefaultCpuDispatcherCreate failed.");
	print_verbose(vformat("PhysX: CPU dispatcher using %d worker threads (of %d)%s.",
			worker_threads, cpu_count, gpu_dynamics_enabled ? " [GPU path]" : ""));

	print_verbose(vformat("PhysX %d.%d.%d initialized%s.",
			PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX,
			gpu_dynamics_enabled ? " [GPU]" : ""));

	px_cooking_params = physx::PxCookingParams(scale);

	// Weld near-coincident vertices so imported meshes (which routinely carry
	// duplicated vertices) don't produce internal degenerate triangles that
	// upset the narrowphase.
	px_cooking_params.meshWeldTolerance = 0.001f;
	px_cooking_params.meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
#ifdef GODOT_PHYSX_GPU
	// Cooked meshes must carry GPU data or the GPU solver cannot collide
	// against them (silent fall-through on the GPU path otherwise).
	px_cooking_params.buildGPUData = true;
#endif

	// Populate the custom-shape factory registry ("cone", ...).
	PhysXCustomShapeType::register_builtin_shapes();

	// Default material used by every shape that doesn't specify one.
	// Static/dynamic friction 0.5, restitution 0.1 —conservative defaults.
	px_default_material = px_physics->createMaterial(0.5f, 0.5f, 0.1f);
	ERR_FAIL_NULL(px_default_material);
}
// Tears down the PhysX SDK in reverse init order; see the ordering comment
// at the top of the body for why each kind precedes the next.
void PhysXServer3D::finish() {
	// Free wrappers before releasing PxPhysics, so their PxActors are removed
	// from still-valid scenes. Order: joints -> vehicles -> bodies -> areas ->
	// shapes -> spaces. Vehicles come before bodies because they borrow the
	// chassis PxRigidDynamic, and before spaces because release() calls
	// space->unregister_vehicle(). Bodies/areas need their spaces alive when
	// their destructors call _destroy_actor which accesses space->remove_actor().
	for (const RID &r : joint_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : vehicle_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : body_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : area_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : soft_body_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : articulation_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : fluid_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : cloth_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : shape_owner.get_owned_list()) {
		free(r);
	}
	for (const RID &r : space_owner.get_owned_list()) {
		free(r);
	}
	_space_list.clear(); // each free() above already removed its entry

	if (px_default_material) {
		px_default_material->release();
		px_default_material = nullptr;
	}
#ifdef GODOT_PHYSX_GPU
	// Calling release() requires the complete PxCudaContextManager type, which
	// only exists when the SDK headers have GPU support enabled (a CPU build
	// defines DISABLE_CUDA_PHYSX, and px_cuda_context can only be non-null in
	// a GPU build anyway).
	if (px_cuda_context) {
		px_cuda_context->release();
		px_cuda_context = nullptr;
		gpu_dynamics_enabled = false;
	}
#endif
	if (px_cpu_dispatcher) {
		px_cpu_dispatcher->release();
		px_cpu_dispatcher = nullptr;
	}
	if (vehicle_sdk_initialized) {
		physx::PxCloseVehicleExtension();
		vehicle_sdk_initialized = false;
	}
	if (extensions_initialized) {
		PxCloseExtensions();
		extensions_initialized = false;
	}
	if (px_physics) {
		px_physics->release(); // releases shapes/actors still owned by it
		px_physics = nullptr;
	}
#ifdef DEBUG_ENABLED
	if (px_debugger) {
		px_debugger->release();
		px_debugger = nullptr;
	}
	if (px_pvd_transport) {
		px_pvd_transport->release();
		px_pvd_transport = nullptr;
	}
#endif
	if (px_foundation) {
		px_foundation->release();
		px_foundation = nullptr;
	}
}
// Server-global simulation gate, INDEPENDENT of per-space activation: the
// editor calls set_active(false) once at startup so physics stays frozen in
// the viewport; the running game uses a fresh server with active=true
// (mirrors Jolt/godot_physics_3d). Do NOT propagate this into per-space
// state @DASH@ World3D::get_space() re-enables individual spaces as worlds
// are (re)created, which must not override the global freeze.
void PhysXServer3D::set_active(bool p_active) {
	active = p_active;
}
// Advances every active space by p_step. With async stepping off, each
// space blocks in simulate()+fetchResults(); with it on, step() only kicks
// the solve and the next tick's sync() fetches (see PhysXSpace3D::step).
void PhysXServer3D::step(real_t p_step) {
	MutexLock lock(api_mutex);
	if (!active) {
		return;
	}
	// Read live (once per step, deliberately uncached) so tests and tools can
	// toggle async stepping at runtime. The static StringName removes the
	// per-call string hashing; the setting itself stays live by design.
	static const StringName async_step_sn("physics/physx_3d/simulation/async_step");
	async_stepping = GLOBAL_GET(async_step_sn);
	for (const RID &rid : _space_list) {
		PhysXSpace3D *space = space_owner.get_or_null(rid);
		if (space && space->is_active()) {
			space->step((float)p_step);
		}
	}
}

void PhysXServer3D::sync() {
	MutexLock lock(api_mutex);
	// The engine calls sync() at the start of every physics tick, before
	// scripts. In async stepping mode this fetches the solve that step()
	// kicked last tick (per-space no-op when nothing is in flight); with the
	// flag off the fetch already happened inline in step() and this is a no-op.
	for (const RID &rid : _space_list) {
		PhysXSpace3D *space = space_owner.get_or_null(rid);
		if (space) {
			space->sync();
		}
	}
}

void PhysXServer3D::end_sync() {
	// Marks the sync window closed. The fetch already happened in sync().
}

void PhysXServer3D::flush_queries() {
	MutexLock lock(api_mutex);
	// Dispatch the Area3D monitor callbacks (body_entered / area_entered / ...)
	// that were queued during the last step's onTrigger. Godot's contract is
	// that these fire in the flush_queries() window —after sync(), with the
	// world unlocked —so onTrigger records events instead of dispatching inline.
	if (!active) {
		return;
	}
	for (const RID &rid : _space_list) {
		PhysXSpace3D *space = space_owner.get_or_null(rid);
		if (space) {
			space->flush_pending_callbacks();
		}
	}
}

void PhysXServer3D::release_vehicles_for_body(PhysXBody3D *p_body) const {
	// Release any vehicles that have the given body as chassis before
	// freeing it —the vehicle's chassis pointer would dangle.
	LocalVector<RID> vehicles = vehicle_owner.get_owned_list();
	for (int i = (int)vehicles.size() - 1; i >= 0; i--) {
		PhysXVehicleServer *v = vehicle_owner.get_or_null(vehicles[i]);
		if (v->get_chassis_body() == p_body) {
			v->release();
			vehicle_owner.free(vehicles[i]);
			memdelete(v);
		}
	}
}

void PhysXServer3D::invalidate_vehicles_surface_pairs(PhysXBody3D *p_body) const {
	// Walk every surviving vehicle (release_vehicles_for_body already removed
	// the chassis ones) and drop grip-table entries resolved from p_body's
	// material. Called by free() BEFORE the body is deleted, which releases
	// that material — the stored raw pointers would otherwise dangle and be
	// consumed by the vehicle's per-step suspension update.
	LocalVector<RID> vehicles = vehicle_owner.get_owned_list();
	for (int i = (int)vehicles.size() - 1; i >= 0; i--) {
		PhysXVehicleServer *v = vehicle_owner.get_or_null(vehicles[i]);
		if (v) {
			v->invalidate_surface_pairs_for_body(p_body);
		}
	}
}

bool PhysXServer3D::is_flushing_queries() const {
	for (const RID &rid : _space_list) {
		const PhysXSpace3D *space = space_owner.get_or_null(rid);
		if (space && space->is_flushing_callbacks()) {
			return true;
		}
	}
	return false;
}

int PhysXServer3D::get_process_info(PhysicsServer3D::ProcessInfo p_process_info) {
	switch (p_process_info) {
		case PhysicsServer3D::INFO_ACTIVE_OBJECTS: {
			// Sum the per-space counts from the last completed solve (each
			// space counts in its _finish_step, right after fetchResults).
			int total = 0;
			for (const RID &rid : _space_list) {
				const PhysXSpace3D *space = space_owner.get_or_null(rid);
				if (space) {
					total += space->get_active_objects();
				}
			}
			return total;
		}
		case PhysicsServer3D::INFO_COLLISION_PAIRS: {
			// Narrow-phase pair count of the last completed step, aggregated
			// over active spaces. PxScene::getSimulationStatistics must not be
			// called while a solve runs (it is silently ignored mid-solve and
			// races under separate-thread physics), so each space caches the
			// count in its _finish_step — the same pattern as
			// INFO_ACTIVE_OBJECTS — and this poll only reads the caches.
			int pairs = 0;
			for (const RID &rid : _space_list) {
				const PhysXSpace3D *space = space_owner.get_or_null(rid);
				if (space) {
					pairs += space->get_cached_contact_pairs();
				}
			}
			return pairs;
		}
		case PhysicsServer3D::INFO_ISLAND_COUNT:
			// PxSimulationStatistics exposes no island count (verified against
			// the vendored SDK: only pair/body/shape/constraint counters), and
			// PhysX does not expose its islands externally. Reported as 0.
			return 0;
		default:
			return 0;
	}
}
