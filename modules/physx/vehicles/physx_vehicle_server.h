/**************************************************************************/
/*  physx_vehicle_server.h                                                */
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
 * @file physx_vehicle_3d.h
 * @brief Wrapper for PhysX vehicle2 - owns vehicle state, borrows chassis body.
 *
 * PhysXVehicleServer is the core vehicle2 wrapper. It owns all vehicle2 state
 * (component sequences, parameter arrays, telemetry states) and borrows a
 * chassis PhysXBody3D* (the PxRigidBody that represents the vehicle in the
 * PhysX scene). It exposes the vehicle archetype (DirectDrive or EngineDrive)
 * and provides build_/adopt/release/update/post_step lifecycle methods.
 *
 * vehicle2 compile/link surface (verification gate):
 *   Compile-probe symbol: PxVehicleAPI.h (exists at thirdparty/physx/include/vehicle/PxVehicleAPI.h).
 *   SDK init/shutdown: PxInitVehicleExtension(foundation) / PxCloseVehicleExtension().
 *   DirectDrive component classes: PxVehicleDrivetrainStates, PxVehicleDrivetrainParams,
 *     PxVehicleDrivetrainFunctions, PxVehicleDrivetrainComponents.
 *   EngineDrive component classes: PxVehicleDrivetrainStates, PxVehicleDrivetrainParams,
 *     PxVehicleDrivetrainFunctions, PxVehicleDrivetrainComponents (shared drivetrain).
 *   PhysX-integration components: PxVehiclePhysXActorStates,
 *     PxVehiclePhysXActorFunctions, PxVehiclePhysXActorComponents,
 *     PxVehiclePhysXConstraintStates, PxVehiclePhysXConstraintFunctions,
 *     PxVehiclePhysXConstraintComponents, PxVehiclePhysXRoadGeometryState,
 *     PxVehiclePhysXRoadGeometryFunctions, PxVehiclePhysXRoadGeometryComponents.
 *   Batched-query setup: PxVehiclePhysXRoadGeometrySceneQueryComponent::getDataForPhysXRoadGeometrySceneQueryComponent().
 *   Telemetry state arrays: PxVehiclePvdFunctions::PxVehiclePvdRigidBodyRegister/Write,
 *     PxVehiclePvdSuspensionStateCalculationParamsRegister/Write,
 *     PxVehiclePvdCommandResponseRegister/Write,
 *     PxVehiclePvdWheelAttachmentsRegister/Write,
 *     PxVehiclePvdAntiRollsRegister/Write,
 *     PxVehiclePvdDirectDrivetrainRegister/Write,
 *     PxVehiclePvdEngineDrivetrainRegister/Write,
 *     PxVehiclePvdPhysXWheelAttachmentRegister/Write,
 *     PxVehiclePvdPhysXRigidActorRegister/Write,
 *     PxVehiclePvdPhysXSteerStateRegister/Write.
 */

#ifndef PHYSX_VEHICLE_3D_H
#define PHYSX_VEHICLE_3D_H

#include "core/string/ustring.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"
#include "objects/physx_body_3d.h"
#include "physx_rid_owner.h"
#include "spaces/physx_space_3d.h"

namespace physx {
class PxPhysics;
class PxRigidDynamic;
class PxRigidActor;
} //namespace physx

class PhysXServer3D;
class PhysXSpace3D;

// OPTIONAL, deliberately unimplemented — read before considering it:
// Batched suspension (road-geometry) queries. This is an extremely niche
// optimization. Each vehicle already issues its own per-wheel scene raycasts
// through the vehicle2 road-geometry component, which is negligible for the
// vehicle counts any real project runs (1 player vehicle, a handful of AI).
// Batching only pays off with dozens of simultaneously fully-simulated
// vehicles, and even then the industry-standard answer is vehicle LOD
// (simulate the player + nearest few; approximate or kinematic-drive the
// rest), not query batching. Do NOT implement this speculatively: it adds a
// shared query pipeline whose physics output is identical to what already
// exists, and it would have to be re-validated against determinism and
// filtering. Only revisit it if profiling of an actual game shows suspension
// raycasts dominating the step AND the project genuinely runs that many full
// vehicles — and prefer the LOD approach first.

/**
 * @brief Telemetry data for a single wheel.
 *
 * Filled by post_step() and read by server getters.
 */
struct WheelTelemetry {
	bool in_contact = false;
	Vector3 contact_point; // world space
	Vector3 contact_normal; // world space
	RID contact_body_rid; // resolved from hit actor's PhysXActorUserData
	float rpm = 0.f;
	float skid = 0.f; // longitudinal tire slip ratio
	float skid_lateral = 0.f; // lateral tire slip ratio
	float rotation = 0.f; // wheel angular position (for render)
};

/**
 * @brief Per-wheel drivetrain/role flags.
 *
 * Stored Godot-side because the vehicle2 param structs don't model them.
 * Consumed by the axle description and the command, brake
 * and steer response components.
 */
struct WheelFlags {
	bool steer = false; // this wheel receives steering input.
	bool traction = true; // this wheel is driven (engine torque / direct drive).
	bool brake = true; // this wheel receives brake input.
	bool front = false; // front axle (affects default steer/drive assignment).
};

class PhysXVehicleServer : public PhysXRIDOwner {
public:
	PhysXVehicleServer();
	~PhysXVehicleServer();

	/** Vehicle archetype - determines the drivetrain type. */
	enum Archetype {
		ARCHETYPE_DIRECT_DRIVE,
		ARCHETYPE_ENGINE_DRIVE,
	};

	/**
	 * @brief Adopt an existing PhysX actor as the vehicle chassis.
	 *
	 * Links the chassis PhysXBody3D and its PxRigidDynamic to this vehicle,
	 * assembles the vehicle2 state, and registers in the current space (if one
	 * is set). May be called again to re-attach a different chassis; the
	 * previous assembly is torn down first.
	 */
	bool adopt(physx::PxRigidDynamic *p_chassis, PhysXBody3D *p_chassis_body);

	/**
	 * @brief Change the vehicle's space membership (register/unregister).
	 *
	 * Mirrors PhysXBody3D::set_space. The vehicle registers in the new space
	 * only once assembled (chassis attached via adopt); if the chassis is
	 * attached afterwards, adopt() registers at that point.
	 */
	void set_space(PhysXSpace3D *p_space);

	// --- Public getters (called by server) ---
	Archetype get_archetype() const { return archetype; }
	int get_wheel_count() const { return wheel_count; }
	const LocalVector<WheelTelemetry> &get_wheel_telemetry() const { return wheel_telemetry; }
	bool get_telemetry_valid() const { return telemetry_valid; }

	/** Get engine RPM (EngineDrive only). */
	float get_engine_rpm() const { return engine_rpm; }
	/** Get engine gear (EngineDrive only). */
	int get_engine_gear() const { return engine_gear; }
	/** Get clutch response (EngineDrive only). */
	float get_clutch_resp() const { return clutch_resp; }

	/** Get chassis body (non-owning). */
	PhysXBody3D *get_chassis_body() const { return chassis_body; }

private:
	friend class PhysXServer3D;
	friend class PhysXSpace3D;

	// ============================================================
	// (1) PRIVATE VARIABLES — plain Godot/POD state
	// ============================================================
	// VEH-1: tuned per-channel baselines snapshotted in
	// _update_response_params; per-wheel command modes may raise the live
	// maxResponse above these, and the scalar restore needs the original.
	float scoped_drive_max_response = 1000.0f;
	float scoped_brake_max_response = 2000.0f;
	// VEH-2: anti-roll configuration cache — survives wheel-count rebuilds
	// like every other tuning dictionary.
	Dictionary cached_anti_roll_params;
	Archetype archetype = ARCHETYPE_DIRECT_DRIVE;
	bool configured = false;
	bool inputs_dirty = true;
	bool telemetry_valid = false;
	int wheel_count = 0;

	// --- input cache: written by ECS via server, consumed in update() ---
	//   EngineDrive set (used iff archetype == ENGINE_DRIVE):
	float in_throttle = 0.f;
	float in_brake = 0.f;
	float in_steer = 0.f;
	float in_handbrake = 0.f;
	int in_target_gear = 0xff; // eAUTOMATIC_GEAR — autobox shifts by default; override via set_gear_command()

	//   DirectDrive set (used iff archetype == DIRECT_DRIVE), per-wheel [wheel_count]:
	LocalVector<float> in_wheel_drive_torque;
	LocalVector<float> in_wheel_brake_torque;
	LocalVector<float> in_wheel_steer_angle;

	//   Per-wheel role flags (steer / traction / brake / front).
	LocalVector<WheelFlags> wheel_flags;

	// --- rebuild-surviving configuration cache ---
	// v2-resident tuning (per-wheel params, EngineDrive drivetrain params) is
	// destroyed with v2 on _rebuild (a wheel-count change). These Godot-side
	// caches hold the last-applied Dictionaries so _rebuild can re-apply the
	// configuration instead of silently resetting it to defaults.
	LocalVector<Dictionary> cached_wheel_params;
	Dictionary cached_engine_params;
	Dictionary cached_clutch_params;
	Dictionary cached_gearbox_params;
	Dictionary cached_autobox_params;
	Dictionary cached_differential_params;

	// --- tuning cache: written by server (vehicle_set_response_params), applied
	// in _update_response_params(); -1 = built-in default ---
	float tune_drive_torque = -1.0f; // DirectDrive: per-wheel drive torque at full throttle (Nm).
	float tune_max_steer_angle = -1.0f; // steer lock (rad).
	float tune_brake_torque = -1.0f; // main brake channel torque (Nm).
	float tune_handbrake_torque = -1.0f; // handbrake channel torque (Nm).

	// --- Ackermann steering (vehicle_set_ackermann_params) ---
	// When enabled, the per-wheel steer angles fed to vehicle2 are computed
	// from the scalar steer command through the Ackermann geometry (inner
	// wheel of a turn steers more than the outer). Geometry derives from the
	// wheel suspension attachments unless overridden. Applied to steer-flagged
	// wheels in write_commands(); per-wheel steer mode bypasses it.
	bool ackermann_enabled = false;
	float ackermann_percent = 100.0f; // 0 = parallel steer, 100 = pure Ackermann.
	float tune_ackermann_wheelbase = -1.0f; // <0 = derive from wheel attachments.
	float tune_ackermann_track = -1.0f; // <0 = derive from steered-axle pair.

	// --- 2-wheeler balance assist (vehicle_set_balance_params) ---
	// Dynamic roll stabilization for motorcycles: a PD controller on the
	// chassis roll (lean) angle and roll rate augments the steer command so
	// the bike steers into its own fall, plus an optional low-speed corrective
	// torque for when steering has no lateral authority (near standstill).
	bool balance_enabled = false;
	float balance_kp = 10.0f; // lean-angle gain (steer rad per lean rad).
	float balance_kd = 1.5f; // roll-rate gain (steer rad per rad/s).
	float balance_max_steer_assist = 0.3f; // cap on the steer augmentation (rad).
	float balance_low_speed_torque = 0.0f; // corrective torque scale (Nm/rad) below ~3 m/s; 0 = off.
	float balance_lean_angle = 0.0f; // telemetry: signed chassis roll (rad, + = tips right).
	float balance_roll_rate = 0.0f; // telemetry: roll rate (rad/s).
	float balance_steer_assist = 0.0f; // telemetry: steer augmentation this step (rad).

	// --- telemetry cache: filled in post_step(), read by server getters ---
	LocalVector<WheelTelemetry> wheel_telemetry;
	// Per-wheel road-wheel steer angles the last write_commands() resolved to
	// (rad; Ackermann-aware). Read via vehicle_get_wheel_steer_angles().
	LocalVector<float> computed_steer_angles;

	//   EngineDrive-only telemetry:
	float engine_rpm = 0.f;
	int engine_gear = 0;
	float clutch_resp = 0.f;

	// ============================================================
	// (2) PRIVATE OBJECTS — non-trivial owned/borrowed handles
	// ============================================================
	// --- chassis: NON-owning. Owned by PhysXBody3D; borrowed here. ---
	PhysXBody3D *chassis_body = nullptr;
	physx::PxRigidDynamic *chassis_actor = nullptr;

	/// Drops every "surface_frictions" grip-table entry that was resolved from
	/// p_body's material — called by the server when that body is freed, before
	/// its PxMaterial is released (the stored raw pointers would dangle).
	void invalidate_surface_pairs_for_body(const PhysXBody3D *p_body);

	// --- vehicle2 guts (compiler firewall; defined in .cpp) ---
	struct Vehicle2State;
	Vehicle2State *v2 = nullptr;

	// Component subclasses (defined in the .cpp; bind getData to the
	// Vehicle2State arrays). Forward-declared here so they can be defined
	// out-of-line as nested classes.
	class BeginComponent;
	class CommandResponseComponent;
	class ActuationComponent;
	class RoadGeometryComponent;
	class SuspensionComponent;
	class TireComponent;
	class DirectDrivetrainComponent;
	class ConstraintComponent;
	class RigidBodyComponent;
	class EndComponent;
	class EngineDriveCommandResponseComponent;
	class FourWheelDriveDifferentialStateComponent;
	class EngineDriveActuationStateComponent;
	class EngineDrivetrainComponent;

	// --- space membership ---
	PhysXSpace3D *space = nullptr;

	// ============================================================
	// (3) PRIVATE FUNCTIONS
	// ============================================================
	// --- lifecycle (mirror PhysXJoint3D::create_px_joint / adopt / release) ---
	static void _init_shared_state(Vehicle2State *p_state, physx::PxRigidDynamic &p_chassis, int p_wheel_count);
	static Vehicle2State *build_direct_drive(physx::PxPhysics &p_physics,
			physx::PxRigidDynamic &p_chassis,
			int p_wheel_count);
	static Vehicle2State *build_engine_drive(physx::PxPhysics &p_physics,
			physx::PxRigidDynamic &p_chassis,
			int p_wheel_count);
	void release();
	// Re-seed throttle/steer/brake response params from WheelFlags (drive/steer
	// intent). Called after build, after a wheel-count change, and after
	// apply_wheel_params (flags may have changed).
	void _update_response_params();
	void _reseed_tire_rest_load();
	void _regroup_axles_from_poses();
	void _apply_cached_anti_roll();
	// Tear down v2 and re-run build with the current archetype/chassis/wheel
	// count. Used when the wheel count changes after adopt (the component
	// sequence binds to array addresses, so it must be re-assembled).
	void _rebuild();

	// --- per-step (called by PhysXSpace3D; see plan B orchestration) ---
	// update() runs in the pre-step window so the vehicle2 forces are applied
	// to the chassis PxRigidDynamic before simulate(). The wrapper fetches
	// gravity + the scene context from its owning space internally.
	void write_commands();
	void update(float p_step);
	void post_step(float p_step);

	// Balance assist: reads the chassis pose/angular velocity, computes the
	// lean telemetry and (when enabled) the steer augmentation + optional
	// low-speed corrective torque. Runs inside update() before write_commands.
	void _update_balance(float p_step);

	// --- wheel configuration helpers (used by server's vehicle_set_wheel_*) ---
	void allocate_wheel_buffers(int p_count);
	void apply_wheel_params(int p_idx, const Dictionary &p_params);

	// --- control inputs ---
	void set_control_inputs(float p_throttle, float p_brake, float p_steer, float p_handbrake);
	void set_gear_command(int p_gear);
	// Tuned command-response limits (steer + brake channels on both
	// archetypes, drive torque on DirectDrive); -1 entries keep defaults.
	void set_response_params(const Dictionary &p_params);
	// --- Anti-roll bars (wheel-id pairs + per-bar stiffness; empty = disabled).
	void set_anti_roll_params(const Dictionary &p_params);
	Dictionary get_anti_roll_params() const; // LIVE v2 state (test/diagnostic)
	Dictionary get_response_params() const; // LIVE v2 state (test/diagnostic)
	// Ackermann steering geometry ("enabled", "percent", "wheelbase", "track").
	void set_ackermann_params(const Dictionary &p_params);
	// Per-wheel road-wheel steer angles the current command resolves to (rad).
	// Ackermann-aware; 0 for non-steering wheels. Updated each write_commands.
	PackedFloat32Array get_wheel_steer_angles() const;
	// 2-wheeler roll-balance assist ("enabled", "kp", "kd", "max_steer_assist",
	// "low_speed_torque").
	void set_balance_params(const Dictionary &p_params);
	// Balance telemetry: lean_angle / roll_rate / steer_assist.
	Dictionary get_balance_state() const;

	// --- EngineDrive config; no-op unless archetype == ENGINE_DRIVE ---
	void set_engine_params(const Dictionary &p_params);
	void set_clutch_params(const Dictionary &p_params);
	void set_gearbox_params(const Dictionary &p_params);
	void set_autobox_params(const Dictionary &p_params);
	void set_differential_params(const Dictionary &p_params);

	// --- DirectDrive per-wheel control ---
	void set_wheel_drive_torque(int p_idx, float p_torque);
	void set_wheel_brake_torque(int p_idx, float p_torque);
	void set_wheel_steer_angle(int p_idx, float p_angle);

	// --- utility ---
	RID resolve_contact_body(const physx::PxRigidActor *p_hit) const;
	static Archetype int_to_archetype(int p_kind);
};

#endif // PHYSX_VEHICLE_3D_H
