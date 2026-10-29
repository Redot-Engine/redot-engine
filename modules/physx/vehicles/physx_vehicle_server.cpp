/**************************************************************************/
/*  physx_vehicle_server.cpp                                              */
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
 * @file physx_vehicle_3d.cpp
 * @brief Implementation of PhysXVehicleServer — vehicle2 wrapper.
 *
 * Both archetypes are assembled end-to-end and integrated through the
 * PhysX actor path.
 *   - DirectDrive: per-wheel torque, no gearbox.
 *   - EngineDrive: engine + clutch + gearbox + autobox + differential.
 *
 * vehicle2 component model: each component is a base class we subclass and
 * bind to the vehicle's arrays by overriding getDataFor...Component(...).
 * Every array parameter is passed BY REFERENCE (&), matching the base classes.
 *
 * DirectDrive sequence:
 *   Begin -> DirectDriveCommandResponse -> DirectDriveActuationState
 *   -> RoadGeom -> Suspension -> Tire -> DirectDrivetrain -> Constraint -> End
 * EngineDrive sequence:
 *   Begin -> EngineDriveCommandResponse -> FourWheelDriveDifferentialState
 *   -> EngineDriveActuationState -> RoadGeom -> Suspension -> Tire
 *   -> EngineDrivetrain -> Constraint -> End
 *
 * Hand-offs flagged inline: the gravity/eDISABLE_GRAVITY hand-off, Godot
 * frame calibration, wheel PxShape creation, telemetry readback.
 */

#include "physx_vehicle_server.h"

#include "physx_vehicle_shared.h" // shared frame/gravity/clamp conventions

#include "objects/physx_body_3d.h"
#include "physx_server.h"
#include "shapes/physx_user_data.h"
#include "spaces/physx_query_filter_callback.h"
#include "spaces/physx_space_3d.h"

#include "PxPhysicsAPI.h"
#include "vehicle/PxVehicleAPI.h"
#include "vehicle/physxConstraints/PxVehiclePhysXConstraintHelpers.h"

#include "vehicle/suspension/PxVehicleSuspensionParams.h"
#include "vehicle/suspension/PxVehicleSuspensionStates.h"
#include "vehicle/tire/PxVehicleTireParams.h"
#include "vehicle/wheel/PxVehicleWheelParams.h"

// Per-wheel cap on (ground material -> friction) mappings in the tire grip
// table (vehicle_set_wheel_params "surface_frictions").
static constexpr unsigned MAX_SURFACE_PAIRS = 8;

// ============================================================================
// Helpers
// ============================================================================

static physx::PxTransform _to_px_transform(const Transform3D &p_t) {
	const Quaternion rotation_quaternion = p_t.basis.get_rotation_quaternion();
	return physx::PxTransform(
			physx::PxVec3((float)p_t.origin.x, (float)p_t.origin.y, (float)p_t.origin.z),
			physx::PxQuat((float)rotation_quaternion.x, (float)rotation_quaternion.y, (float)rotation_quaternion.z, (float)rotation_quaternion.w));
}

static void _set_wheel_param_defaults(physx::PxVehicleWheelParams &p_wp,
		physx::PxVehicleSuspensionParams &p_sp,
		physx::PxVehicleSuspensionForceParams &p_sfp,
		physx::PxVehicleTireForceParams &p_tp) {
	p_wp.radius = 0.5f;
	p_wp.halfWidth = 0.2f;
	p_wp.mass = 20.0f;
	p_wp.moi = 0.5f * p_wp.mass * p_wp.radius * p_wp.radius;
	p_wp.dampingRate = 0.25f;

	p_sp.suspensionAttachment = physx::PxTransform(physx::PxIdentity);
	p_sp.suspensionTravelDir = physx::PxVec3(0.0f, -1.0f, 0.0f);
	p_sp.suspensionTravelDist = 1.0f;
	p_sp.wheelAttachment = physx::PxTransform(physx::PxIdentity);

	p_sfp.stiffness = 30000.0f;
	p_sfp.damping = 4500.0f;
	p_sfp.sprungMass = 250.0f;

	p_tp.latStiffX = 2.0f;
	p_tp.latStiffY = 18000.0f;
	p_tp.longStiff = 10000.0f;
	p_tp.camberStiff = 0.0f;
	p_tp.frictionVsSlip[0][0] = 0.0f;
	p_tp.frictionVsSlip[0][1] = 1.0f;
	p_tp.frictionVsSlip[1][0] = 0.1f;
	p_tp.frictionVsSlip[1][1] = 1.0f;
	p_tp.frictionVsSlip[2][0] = 1.0f;
	p_tp.frictionVsSlip[2][1] = 1.0f;
	// Seed from the SCENE gravity when available (VEH-5): the hardcoded 9.81
	// mis-tuned the load-dependent tire stiffness on any scene with custom
	// gravity. The caller (build_direct_drive/build_engine_drive) patches this
	// up with the actual scene value right after the defaults run.
	p_tp.restLoad = p_sfp.sprungMass * 9.81f;
	p_tp.loadFilter[0][0] = 0.0f;
	p_tp.loadFilter[0][1] = 0.0f;
	p_tp.loadFilter[1][0] = 3.0f;
	p_tp.loadFilter[1][1] = 3.0f;
}

// ============================================================================
// Compiler firewall: Vehicle2State
// ============================================================================

struct PhysXVehicleServer::Vehicle2State {
	int nb_wheels = 0;
	Archetype arch = ARCHETYPE_DIRECT_DRIVE;

	// per-wheel config (params)
	physx::PxVehicleWheelParams *wheel_params = nullptr;
	physx::PxVehicleSuspensionParams *suspension_params = nullptr;
	physx::PxVehicleSuspensionForceParams *suspension_force_params = nullptr;
	physx::PxVehicleSuspensionComplianceParams *suspension_compliance_params = nullptr;
	physx::PxVehicleTireForceParams *tire_params = nullptr;
	physx::PxVehiclePhysXMaterialFrictionParams *material_friction_params = nullptr;
	physx::PxVehiclePhysXSuspensionLimitConstraintParams *suspension_limit_params = nullptr;
	physx::PxTransform *wheel_shape_local_poses = nullptr;
	// Anti-roll bars (configured via vehicle_set_anti_roll_params; inactive
	// while nb_anti_roll_bars == 0).
	physx::PxVehicleAntiRollForceParams *anti_roll_params = nullptr;
	int nb_anti_roll_bars = 0;
	physx::PxVehicleAntiRollTorque anti_roll_torque; // accumulated per step (world frame)

	// Per-wheel (ground material -> friction) grip table, wheel-major
	// ([wheel * MAX_SURFACE_PAIRS, + nb)); filled from vehicle_set_wheel_params
	// "surface_frictions".
	physx::PxVehiclePhysXMaterialFriction *surface_pairs = nullptr;
	// Parallel Godot-side table recording WHICH body each surface_pairs entry
	// was resolved from, so entries can be invalidated when that body (and its
	// private PxMaterial) is freed — a raw PxMaterial* here would otherwise
	// dangle the moment the ground body dies (PhysXShapedObject3D releases the
	// material in its destructor).
	RID *surface_pair_bodies = nullptr;

	// per-wheel state (recomputed each step)
	physx::PxVehicleRoadGeometryState *road_geometry_states = nullptr;
	physx::PxVehiclePhysXRoadGeometryQueryState *physx_road_geometry_states = nullptr;
	physx::PxVehicleSuspensionState *suspension_states = nullptr;
	physx::PxVehicleSuspensionComplianceState *suspension_compliance_states = nullptr;
	physx::PxVehicleSuspensionForce *suspension_forces = nullptr;
	physx::PxVehicleTireGripState *tire_grip_states = nullptr;
	physx::PxVehicleTireDirectionState *tire_direction_states = nullptr;
	physx::PxVehicleTireSpeedState *tire_speed_states = nullptr;
	physx::PxVehicleTireSlipState *tire_slip_states = nullptr;
	physx::PxVehicleTireCamberAngleState *tire_camber_states = nullptr;
	physx::PxVehicleTireStickyState *tire_sticky_states = nullptr;
	physx::PxVehicleTireForce *tire_forces = nullptr;
	physx::PxVehicleWheelRigidBody1dState *wheel_rigid_body_1d_states = nullptr;
	physx::PxVehicleWheelActuationState *actuation_states = nullptr;
	physx::PxVehicleWheelLocalPose *wheel_local_poses = nullptr;
	physx::PxReal *steer_response_states = nullptr;
	physx::PxReal *brake_response_states = nullptr;
	physx::PxReal *throttle_response_states = nullptr; // DirectDrive (per-wheel)

	// singletons shared by both archetypes (params)
	physx::PxVehicleAxleDescription axle_description;
	physx::PxVehicleRigidBodyParams rigid_body_params;
	physx::PxVehicleSuspensionStateCalculationParams susp_state_calc_params;
	physx::PxVehiclePhysXActor physx_actor;
	physx::PxVehiclePhysXSteerState physx_steer_state;
	physx::PxVehiclePhysXConstraints physx_constraints;
	physx::PxVehicleCommandState command_state;
	physx::PxVehiclePhysXRoadGeometryQueryParams road_geom_params;
	PhysXQueryFilterCallback *road_geometry_filter = nullptr;
	HashSet<RID> chassis_rid_set;
	physx::PxVehicleDirectDriveThrottleCommandResponseParams throttle_response_params; // DirectDrive
	physx::PxVehicleSteerCommandResponseParams steer_response_params;
	physx::PxVehicleBrakeCommandResponseParams brake_response_params[2];
	int nb_brake_response_params = 2;

	// DirectDrive command/state
	physx::PxVehicleDirectDriveTransmissionCommandState direct_transmission_command;

	// EngineDrive params
	physx::PxVehicleEngineParams engine_params;
	physx::PxVehicleClutchParams clutch_params;
	physx::PxVehicleClutchCommandResponseParams clutch_response_params;
	physx::PxVehicleGearboxParams gearbox_params;
	physx::PxVehicleAutoboxParams autobox_params;
	physx::PxVehicleFourWheelDriveDifferentialParams diff_params;

	// EngineDrive state
	physx::PxVehicleRigidBodyState rigid_body_state; // shared
	physx::PxVehicleEngineState engine_state;
	physx::PxVehicleGearboxState gearbox_state;
	physx::PxVehicleClutchCommandResponseState clutch_response_state;
	physx::PxVehicleDifferentialState diff_state;
	physx::PxVehicleAutoboxState autobox_state;
	physx::PxVehicleClutchSlipState clutch_slip_state;
	physx::PxVehicleWheelConstraintGroupState constraint_group_state;
	physx::PxVehicleEngineDriveThrottleCommandResponseState engine_throttle_response_state;
	physx::PxVehicleEngineDriveTransmissionCommandState engine_transmission_command;

	// component pipeline
	physx::PxVehicleComponentSequence sequence;
	physx::PxVehicleComponent *comp_begin = nullptr;
	physx::PxVehicleComponent *comp_cmd_response = nullptr; // DirectDrive
	physx::PxVehicleComponent *comp_actuation = nullptr; // DirectDrive
	physx::PxVehicleComponent *comp_drivetrain = nullptr; // DirectDrive
	physx::PxVehicleComponent *comp_engine_cmd_response = nullptr; // EngineDrive
	physx::PxVehicleComponent *comp_diff_state = nullptr; // EngineDrive
	physx::PxVehicleComponent *comp_engine_actuation = nullptr; // EngineDrive
	physx::PxVehicleComponent *comp_engine_drivetrain = nullptr; // EngineDrive
	physx::PxVehicleComponent *comp_road_geom = nullptr;
	physx::PxVehicleComponent *comp_suspension = nullptr;
	physx::PxVehicleComponent *comp_tire = nullptr;
	physx::PxVehicleComponent *comp_constraint = nullptr;
	physx::PxVehicleComponent *comp_rigid_body = nullptr;
	physx::PxVehicleComponent *comp_end = nullptr;

	Vehicle2State() = default;
	~Vehicle2State() {
		free_arrays();
		free_components();
	}
	void allocate_arrays(int p_n);
	void free_arrays();
	void free_components();
};

// ============================================================================
// Component subclasses. Each binds its getData to the arrays (arrays by &).
// Begin/End are archetype-aware (engine/gearbox only for EngineDrive).
// ============================================================================

class PhysXVehicleServer::BeginComponent : public physx::PxVehiclePhysXActorBeginComponent {
public:
	explicit BeginComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForPhysXActorBeginComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleCommandState *&commands,
			const physx::PxVehicleEngineDriveTransmissionCommandState *&transmissionCommands,
			const physx::PxVehicleGearboxParams *&gearParams,
			const physx::PxVehicleGearboxState *&gearState,
			const physx::PxVehicleEngineParams *&engineParams,
			physx::PxVehiclePhysXActor *&physxActor,
			physx::PxVehiclePhysXSteerState *&physxSteerState,
			physx::PxVehiclePhysXConstraints *&physxConstraints,
			physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<physx::PxVehicleWheelRigidBody1dState> &wheelRigidBody1dStates,
			physx::PxVehicleEngineState *&engineState) override {
		axleDescription = &m_s->axle_description;
		commands = &m_s->command_state;
		if (m_s->arch == ARCHETYPE_ENGINE_DRIVE) {
			transmissionCommands = &m_s->engine_transmission_command;
			gearParams = &m_s->gearbox_params;
			gearState = &m_s->gearbox_state;
			engineParams = &m_s->engine_params;
			engineState = &m_s->engine_state;
		} else {
			transmissionCommands = nullptr;
			gearParams = nullptr;
			gearState = nullptr;
			engineParams = nullptr;
			engineState = nullptr;
		}
		physxActor = &m_s->physx_actor;
		physxSteerState = &m_s->physx_steer_state;
		physxConstraints = &m_s->physx_constraints;
		rigidBodyState = &m_s->rigid_body_state;
		wheelRigidBody1dStates.setData(m_s->wheel_rigid_body_1d_states);
	}

private:
	Vehicle2State *m_s; // Pointer to the vehicle state data
};

class PhysXVehicleServer::CommandResponseComponent : public physx::PxVehicleDirectDriveCommandResponseComponent {
public:
	explicit CommandResponseComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForDirectDriveCommandResponseComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleSizedArrayData<const physx::PxVehicleBrakeCommandResponseParams> &brakeResponseParams,
			const physx::PxVehicleDirectDriveThrottleCommandResponseParams *&throttleResponseParams,
			const physx::PxVehicleSteerCommandResponseParams *&steerResponseParams,
			physx::PxVehicleSizedArrayData<const physx::PxVehicleAckermannParams> &ackermannParams,
			const physx::PxVehicleCommandState *&commands,
			const physx::PxVehicleDirectDriveTransmissionCommandState *&transmissionCommands,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<physx::PxReal> &brakeResponseStates,
			physx::PxVehicleArrayData<physx::PxReal> &throttleResponseStates,
			physx::PxVehicleArrayData<physx::PxReal> &steerResponseStates) override {
		axleDescription = &m_s->axle_description;
		brakeResponseParams.setDataAndCount(m_s->brake_response_params, m_s->nb_brake_response_params);
		throttleResponseParams = &m_s->throttle_response_params;
		steerResponseParams = &m_s->steer_response_params;
		ackermannParams.setEmpty();
		commands = &m_s->command_state;
		transmissionCommands = &m_s->direct_transmission_command;
		rigidBodyState = &m_s->rigid_body_state;
		brakeResponseStates.setData(m_s->brake_response_states);
		throttleResponseStates.setData(m_s->throttle_response_states);
		steerResponseStates.setData(m_s->steer_response_states);
	}

private:
	Vehicle2State *m_s; // Pointer to the vehicle state data
};

class PhysXVehicleServer::ActuationComponent : public physx::PxVehicleDirectDriveActuationStateComponent {
public:
	explicit ActuationComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForDirectDriveActuationStateComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleArrayData<const physx::PxReal> &brakeResponseStates,
			physx::PxVehicleArrayData<const physx::PxReal> &throttleResponseStates,
			physx::PxVehicleArrayData<physx::PxVehicleWheelActuationState> &actuationStates) override {
		axleDescription = &m_s->axle_description;
		brakeResponseStates.setData(m_s->brake_response_states);
		throttleResponseStates.setData(m_s->throttle_response_states);
		actuationStates.setData(m_s->actuation_states);
	}

private:
	Vehicle2State *m_s; // Pointer to the vehicle state data
};

class PhysXVehicleServer::RoadGeometryComponent : public physx::PxVehiclePhysXRoadGeometrySceneQueryComponent {
public:
	explicit RoadGeometryComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForPhysXRoadGeometrySceneQueryComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehiclePhysXRoadGeometryQueryParams *&roadGeomParams,
			physx::PxVehicleArrayData<const physx::PxReal> &steerResponseStates,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionParams> &suspensionParams,
			physx::PxVehicleArrayData<const physx::PxVehiclePhysXMaterialFrictionParams> &materialFrictionParams,
			physx::PxVehicleArrayData<physx::PxVehicleRoadGeometryState> &roadGeometryStates,
			physx::PxVehicleArrayData<physx::PxVehiclePhysXRoadGeometryQueryState> &physxRoadGeometryStates) override {
		axleDescription = &m_s->axle_description;
		roadGeomParams = &m_s->road_geom_params;
		steerResponseStates.setData(m_s->steer_response_states);
		rigidBodyState = &m_s->rigid_body_state;
		wheelParams.setData(m_s->wheel_params);
		suspensionParams.setData(m_s->suspension_params);
		materialFrictionParams.setData(m_s->material_friction_params);
		roadGeometryStates.setData(m_s->road_geometry_states);
		physxRoadGeometryStates.setData(m_s->physx_road_geometry_states);
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::SuspensionComponent : public physx::PxVehicleSuspensionComponent {
public:
	explicit SuspensionComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForSuspensionComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleRigidBodyParams *&rigidBodyParams,
			const physx::PxVehicleSuspensionStateCalculationParams *&suspensionStateCalculationParams,
			physx::PxVehicleArrayData<const physx::PxReal> &steerResponseStates,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionParams> &suspensionParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionComplianceParams> &suspensionComplianceParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionForceParams> &suspensionForceParams,
			physx::PxVehicleSizedArrayData<const physx::PxVehicleAntiRollForceParams> &antiRollForceParams,
			physx::PxVehicleArrayData<const physx::PxVehicleRoadGeometryState> &wheelRoadGeomStates,
			physx::PxVehicleArrayData<physx::PxVehicleSuspensionState> &suspensionStates,
			physx::PxVehicleArrayData<physx::PxVehicleSuspensionComplianceState> &suspensionComplianceStates,
			physx::PxVehicleArrayData<physx::PxVehicleSuspensionForce> &suspensionForces,
			physx::PxVehicleAntiRollTorque *&antiRollTorque) override {
		axleDescription = &m_s->axle_description;
		rigidBodyParams = &m_s->rigid_body_params;
		suspensionStateCalculationParams = &m_s->susp_state_calc_params;
		steerResponseStates.setData(m_s->steer_response_states);
		rigidBodyState = &m_s->rigid_body_state;
		wheelParams.setData(m_s->wheel_params);
		suspensionParams.setData(m_s->suspension_params);
		suspensionComplianceParams.setData(m_s->suspension_compliance_params);
		suspensionForceParams.setData(m_s->suspension_force_params);
		antiRollForceParams.setDataAndCount(m_s->anti_roll_params, m_s->nb_anti_roll_bars);
		wheelRoadGeomStates.setData(m_s->road_geometry_states);
		suspensionStates.setData(m_s->suspension_states);
		suspensionComplianceStates.setData(m_s->suspension_compliance_states);
		suspensionForces.setData(m_s->suspension_forces);
		antiRollTorque = (m_s->nb_anti_roll_bars > 0) ? &m_s->anti_roll_torque : nullptr;
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::TireComponent : public physx::PxVehicleTireComponent {
public:
	explicit TireComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForTireComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleArrayData<const physx::PxReal> &steerResponseStates,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelActuationState> &actuationStates,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionParams> &suspensionParams,
			physx::PxVehicleArrayData<const physx::PxVehicleTireForceParams> &tireForceParams,
			physx::PxVehicleArrayData<const physx::PxVehicleRoadGeometryState> &roadGeomStates,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionState> &suspensionStates,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionComplianceState> &suspensionComplianceStates,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionForce> &suspensionForces,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelRigidBody1dState> &wheelRigidBody1DStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireGripState> &tireGripStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireDirectionState> &tireDirectionStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireSpeedState> &tireSpeedStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireSlipState> &tireSlipStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireCamberAngleState> &tireCamberAngleStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireStickyState> &tireStickyStates,
			physx::PxVehicleArrayData<physx::PxVehicleTireForce> &tireForces) override {
		axleDescription = &m_s->axle_description;
		steerResponseStates.setData(m_s->steer_response_states);
		rigidBodyState = &m_s->rigid_body_state;
		actuationStates.setData(m_s->actuation_states);
		wheelParams.setData(m_s->wheel_params);
		suspensionParams.setData(m_s->suspension_params);
		tireForceParams.setData(m_s->tire_params);
		roadGeomStates.setData(m_s->road_geometry_states);
		suspensionStates.setData(m_s->suspension_states);
		suspensionComplianceStates.setData(m_s->suspension_compliance_states);
		suspensionForces.setData(m_s->suspension_forces);
		wheelRigidBody1DStates.setData(m_s->wheel_rigid_body_1d_states);
		tireGripStates.setData(m_s->tire_grip_states);
		tireDirectionStates.setData(m_s->tire_direction_states);
		tireSpeedStates.setData(m_s->tire_speed_states);
		tireSlipStates.setData(m_s->tire_slip_states);
		tireCamberAngleStates.setData(m_s->tire_camber_states);
		tireStickyStates.setData(m_s->tire_sticky_states);
		tireForces.setData(m_s->tire_forces);
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::DirectDrivetrainComponent : public physx::PxVehicleDirectDrivetrainComponent {
public:
	explicit DirectDrivetrainComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForDirectDrivetrainComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleArrayData<const physx::PxReal> &brakeResponseStates,
			physx::PxVehicleArrayData<const physx::PxReal> &throttleResponseStates,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelActuationState> &actuationStates,
			physx::PxVehicleArrayData<const physx::PxVehicleTireForce> &tireForces,
			physx::PxVehicleArrayData<physx::PxVehicleWheelRigidBody1dState> &wheelRigidBody1dStates) override {
		axleDescription = &m_s->axle_description;
		brakeResponseStates.setData(m_s->brake_response_states);
		throttleResponseStates.setData(m_s->throttle_response_states);
		wheelParams.setData(m_s->wheel_params);
		actuationStates.setData(m_s->actuation_states);
		tireForces.setData(m_s->tire_forces);
		wheelRigidBody1dStates.setData(m_s->wheel_rigid_body_1d_states);
	}

private:
	Vehicle2State *m_s;
};

// --- EngineDrive components ---

class PhysXVehicleServer::EngineDriveCommandResponseComponent : public physx::PxVehicleEngineDriveCommandResponseComponent {
public:
	explicit EngineDriveCommandResponseComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForEngineDriveCommandResponseComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleSizedArrayData<const physx::PxVehicleBrakeCommandResponseParams> &brakeResponseParams,
			const physx::PxVehicleSteerCommandResponseParams *&steerResponseParams,
			physx::PxVehicleSizedArrayData<const physx::PxVehicleAckermannParams> &ackermannParams,
			const physx::PxVehicleGearboxParams *&gearboxParams,
			const physx::PxVehicleClutchCommandResponseParams *&clutchResponseParams,
			const physx::PxVehicleEngineParams *&engineParams,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			const physx::PxVehicleEngineState *&engineState,
			const physx::PxVehicleAutoboxParams *&autoboxParams,
			const physx::PxVehicleCommandState *&commands,
			const physx::PxVehicleEngineDriveTransmissionCommandState *&transmissionCommands,
			physx::PxVehicleArrayData<physx::PxReal> &brakeResponseStates,
			physx::PxVehicleEngineDriveThrottleCommandResponseState *&throttleResponseState,
			physx::PxVehicleArrayData<physx::PxReal> &steerResponseStates,
			physx::PxVehicleGearboxState *&gearboxResponseState,
			physx::PxVehicleClutchCommandResponseState *&clutchResponseState,
			physx::PxVehicleAutoboxState *&autoboxState) override {
		axleDescription = &m_s->axle_description;
		brakeResponseParams.setDataAndCount(m_s->brake_response_params, m_s->nb_brake_response_params);
		steerResponseParams = &m_s->steer_response_params;
		ackermannParams.setEmpty();
		gearboxParams = &m_s->gearbox_params;
		clutchResponseParams = &m_s->clutch_response_params;
		engineParams = &m_s->engine_params;
		rigidBodyState = &m_s->rigid_body_state;
		engineState = &m_s->engine_state;
		autoboxParams = &m_s->autobox_params;
		commands = &m_s->command_state;
		transmissionCommands = &m_s->engine_transmission_command;
		brakeResponseStates.setData(m_s->brake_response_states);
		throttleResponseState = &m_s->engine_throttle_response_state;
		steerResponseStates.setData(m_s->steer_response_states);
		gearboxResponseState = &m_s->gearbox_state;
		clutchResponseState = &m_s->clutch_response_state;
		autoboxState = &m_s->autobox_state;
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::FourWheelDriveDifferentialStateComponent : public physx::PxVehicleFourWheelDriveDifferentialStateComponent {
public:
	explicit FourWheelDriveDifferentialStateComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForFourWheelDriveDifferentialStateComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleFourWheelDriveDifferentialParams *&differentialParams,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelRigidBody1dState> &wheelRigidbody1dStates,
			physx::PxVehicleDifferentialState *&differentialState,
			physx::PxVehicleWheelConstraintGroupState *&wheelConstraintGroupState) override {
		axleDescription = &m_s->axle_description;
		differentialParams = &m_s->diff_params;
		wheelRigidbody1dStates.setData(m_s->wheel_rigid_body_1d_states);
		differentialState = &m_s->diff_state;
		wheelConstraintGroupState = &m_s->constraint_group_state;
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::EngineDriveActuationStateComponent : public physx::PxVehicleEngineDriveActuationStateComponent {
public:
	explicit EngineDriveActuationStateComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForEngineDriveActuationStateComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleGearboxParams *&gearboxParams,
			physx::PxVehicleArrayData<const physx::PxReal> &brakeResponseStates,
			const physx::PxVehicleEngineDriveThrottleCommandResponseState *&throttleResponseState,
			const physx::PxVehicleGearboxState *&gearboxState,
			const physx::PxVehicleDifferentialState *&differentialState,
			const physx::PxVehicleClutchCommandResponseState *&clutchResponseState,
			physx::PxVehicleArrayData<physx::PxVehicleWheelActuationState> &actuationStates) override {
		axleDescription = &m_s->axle_description;
		gearboxParams = &m_s->gearbox_params;
		brakeResponseStates.setData(m_s->brake_response_states);
		throttleResponseState = &m_s->engine_throttle_response_state;
		gearboxState = &m_s->gearbox_state;
		differentialState = &m_s->diff_state;
		clutchResponseState = &m_s->clutch_response_state;
		actuationStates.setData(m_s->actuation_states);
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::EngineDrivetrainComponent : public physx::PxVehicleEngineDrivetrainComponent {
public:
	explicit EngineDrivetrainComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForEngineDrivetrainComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			const physx::PxVehicleEngineParams *&engineParams,
			const physx::PxVehicleClutchParams *&clutchParams,
			const physx::PxVehicleGearboxParams *&gearboxParams,
			physx::PxVehicleArrayData<const physx::PxReal> &brakeResponseStates,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelActuationState> &actuationStates,
			physx::PxVehicleArrayData<const physx::PxVehicleTireForce> &tireForces,
			const physx::PxVehicleEngineDriveThrottleCommandResponseState *&throttleResponseState,
			const physx::PxVehicleClutchCommandResponseState *&clutchResponseState,
			const physx::PxVehicleDifferentialState *&differentialState,
			const physx::PxVehicleWheelConstraintGroupState *&constraintGroupState,
			physx::PxVehicleArrayData<physx::PxVehicleWheelRigidBody1dState> &wheelRigidBody1dStates,
			physx::PxVehicleEngineState *&engineState,
			physx::PxVehicleGearboxState *&gearboxState,
			physx::PxVehicleClutchSlipState *&clutchState) override {
		axleDescription = &m_s->axle_description;
		wheelParams.setData(m_s->wheel_params);
		engineParams = &m_s->engine_params;
		clutchParams = &m_s->clutch_params;
		gearboxParams = &m_s->gearbox_params;
		brakeResponseStates.setData(m_s->brake_response_states);
		actuationStates.setData(m_s->actuation_states);
		tireForces.setData(m_s->tire_forces);
		throttleResponseState = &m_s->engine_throttle_response_state;
		clutchResponseState = &m_s->clutch_response_state;
		differentialState = &m_s->diff_state;
		constraintGroupState = &m_s->constraint_group_state;
		wheelRigidBody1dStates.setData(m_s->wheel_rigid_body_1d_states);
		engineState = &m_s->engine_state;
		gearboxState = &m_s->gearbox_state;
		clutchState = &m_s->clutch_slip_state;
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::ConstraintComponent : public physx::PxVehiclePhysXConstraintComponent {
public:
	explicit ConstraintComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForPhysXConstraintComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionParams> &suspensionParams,
			physx::PxVehicleArrayData<const physx::PxVehiclePhysXSuspensionLimitConstraintParams> &suspensionLimitParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionState> &suspensionStates,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionComplianceState> &suspensionComplianceStates,
			physx::PxVehicleArrayData<const physx::PxVehicleRoadGeometryState> &wheelRoadGeomStates,
			physx::PxVehicleArrayData<const physx::PxVehicleTireDirectionState> &tireDirectionStates,
			physx::PxVehicleArrayData<const physx::PxVehicleTireStickyState> &tireStickyStates,
			physx::PxVehiclePhysXConstraints *&constraints) override {
		axleDescription = &m_s->axle_description;
		rigidBodyState = &m_s->rigid_body_state;
		suspensionParams.setData(m_s->suspension_params);
		suspensionLimitParams.setData(m_s->suspension_limit_params);
		suspensionStates.setData(m_s->suspension_states);
		suspensionComplianceStates.setData(m_s->suspension_compliance_states);
		wheelRoadGeomStates.setData(m_s->road_geometry_states);
		tireDirectionStates.setData(m_s->tire_direction_states);
		tireStickyStates.setData(m_s->tire_sticky_states);
		constraints = &m_s->physx_constraints;
	}

private:
	Vehicle2State *m_s;
};

// Forward-integrates the rigid body with the accumulated suspension + tire
// forces (plus gravity). Without this component the forces computed by the
// suspension/tire components never reach rigidBodyState, so the actor end
// component writes back an unchanged state and the vehicle never moves.
class PhysXVehicleServer::RigidBodyComponent : public physx::PxVehicleRigidBodyComponent {
public:
	explicit RigidBodyComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForRigidBodyComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleRigidBodyParams *&rigidBodyParams,
			physx::PxVehicleArrayData<const physx::PxVehicleSuspensionForce> &suspensionForces,
			physx::PxVehicleArrayData<const physx::PxVehicleTireForce> &tireForces,
			const physx::PxVehicleAntiRollTorque *&antiRollTorque,
			physx::PxVehicleRigidBodyState *&rigidBodyState) override {
		axleDescription = &m_s->axle_description;
		rigidBodyParams = &m_s->rigid_body_params;
		suspensionForces.setData(m_s->suspension_forces);
		tireForces.setData(m_s->tire_forces);
		antiRollTorque = (m_s->nb_anti_roll_bars > 0) ? &m_s->anti_roll_torque : nullptr;
		rigidBodyState = &m_s->rigid_body_state;
	}

private:
	Vehicle2State *m_s;
};

class PhysXVehicleServer::EndComponent : public physx::PxVehiclePhysXActorEndComponent {
public:
	explicit EndComponent(Vehicle2State *s) :
			m_s(s) {}
	void getDataForPhysXActorEndComponent(
			const physx::PxVehicleAxleDescription *&axleDescription,
			const physx::PxVehicleRigidBodyState *&rigidBodyState,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelParams> &wheelParams,
			physx::PxVehicleArrayData<const physx::PxTransform> &wheelShapeLocalPoses,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelRigidBody1dState> &wheelRigidBody1dStates,
			physx::PxVehicleArrayData<const physx::PxVehicleWheelLocalPose> &wheelLocalPoses,
			const physx::PxVehicleGearboxState *&gearState,
			const physx::PxReal *&throttle,
			physx::PxVehiclePhysXActor *&physxActor) override {
		axleDescription = &m_s->axle_description;
		rigidBodyState = &m_s->rigid_body_state;
		wheelParams.setData(m_s->wheel_params);
		wheelShapeLocalPoses.setData(m_s->wheel_shape_local_poses);
		wheelRigidBody1dStates.setData(m_s->wheel_rigid_body_1d_states);
		wheelLocalPoses.setData(m_s->wheel_local_poses);
		gearState = (m_s->arch == ARCHETYPE_ENGINE_DRIVE) ? &m_s->gearbox_state : nullptr;
		throttle = &m_s->command_state.throttle;
		physxActor = &m_s->physx_actor;
	}

private:
	Vehicle2State *m_s;
};

// ============================================================================
// Vehicle2State array/component lifecycle
// ============================================================================

void PhysXVehicleServer::Vehicle2State::allocate_arrays(int p_n) {
	if (p_n < 0) {
		p_n = 0;
	}
	free_arrays();
	nb_wheels = p_n;
	if (p_n == 0) {
		return;
	}

	wheel_params = new physx::PxVehicleWheelParams[p_n];
	suspension_params = new physx::PxVehicleSuspensionParams[p_n];
	suspension_force_params = new physx::PxVehicleSuspensionForceParams[p_n];
	tire_params = new physx::PxVehicleTireForceParams[p_n];
	wheel_shape_local_poses = new physx::PxTransform[p_n];
	suspension_compliance_params = new physx::PxVehicleSuspensionComplianceParams[p_n]();
	material_friction_params = new physx::PxVehiclePhysXMaterialFrictionParams[p_n]();
	suspension_limit_params = new physx::PxVehiclePhysXSuspensionLimitConstraintParams[p_n]();
	anti_roll_params = new physx::PxVehicleAntiRollForceParams[p_n]();
	surface_pairs = new physx::PxVehiclePhysXMaterialFriction[p_n * MAX_SURFACE_PAIRS]();
	surface_pair_bodies = new RID[p_n * MAX_SURFACE_PAIRS]();

	road_geometry_states = new physx::PxVehicleRoadGeometryState[p_n]();
	physx_road_geometry_states = new physx::PxVehiclePhysXRoadGeometryQueryState[p_n]();
	suspension_states = new physx::PxVehicleSuspensionState[p_n]();
	suspension_compliance_states = new physx::PxVehicleSuspensionComplianceState[p_n]();
	suspension_forces = new physx::PxVehicleSuspensionForce[p_n]();
	tire_grip_states = new physx::PxVehicleTireGripState[p_n]();
	tire_direction_states = new physx::PxVehicleTireDirectionState[p_n]();
	tire_speed_states = new physx::PxVehicleTireSpeedState[p_n]();
	tire_slip_states = new physx::PxVehicleTireSlipState[p_n]();
	tire_camber_states = new physx::PxVehicleTireCamberAngleState[p_n]();
	tire_sticky_states = new physx::PxVehicleTireStickyState[p_n]();
	tire_forces = new physx::PxVehicleTireForce[p_n]();
	wheel_rigid_body_1d_states = new physx::PxVehicleWheelRigidBody1dState[p_n]();
	actuation_states = new physx::PxVehicleWheelActuationState[p_n]();
	wheel_local_poses = new physx::PxVehicleWheelLocalPose[p_n]();
	steer_response_states = new physx::PxReal[p_n]();
	brake_response_states = new physx::PxReal[p_n]();
	throttle_response_states = new physx::PxReal[p_n]();

	for (int i = 0; i < p_n; i++) {
		_set_wheel_param_defaults(wheel_params[i], suspension_params[i],
				suspension_force_params[i], tire_params[i]);
		wheel_shape_local_poses[i] = physx::PxTransform(physx::PxIdentity);
		material_friction_params[i].defaultFriction = 1.0f;
	}
}

void PhysXVehicleServer::Vehicle2State::free_arrays() {
#define _DEL(a) \
	delete[] a; \
	a = nullptr;
	_DEL(wheel_params)
	_DEL(suspension_params)
	_DEL(suspension_force_params)
	_DEL(suspension_compliance_params)
	_DEL(tire_params)
	_DEL(material_friction_params)
	_DEL(suspension_limit_params)
	_DEL(anti_roll_params)
	_DEL(surface_pairs)
	_DEL(surface_pair_bodies)
	_DEL(wheel_shape_local_poses)
	_DEL(road_geometry_states)
	_DEL(physx_road_geometry_states)
	_DEL(suspension_states)
	_DEL(suspension_compliance_states)
	_DEL(suspension_forces)
	_DEL(tire_grip_states)
	_DEL(tire_direction_states)
	_DEL(tire_speed_states)
	_DEL(tire_slip_states)
	_DEL(tire_camber_states)
	_DEL(tire_sticky_states)
	_DEL(tire_forces)
	_DEL(wheel_rigid_body_1d_states)
	_DEL(actuation_states)
	_DEL(wheel_local_poses)
	_DEL(steer_response_states)
	_DEL(brake_response_states)
	_DEL(throttle_response_states)
#undef _DEL
}

void PhysXVehicleServer::Vehicle2State::free_components() {
	delete road_geometry_filter;
	road_geometry_filter = nullptr;
#define _DELC(c) \
	delete c;    \
	c = nullptr;
	_DELC(comp_begin)
	_DELC(comp_cmd_response)
	_DELC(comp_actuation)
	_DELC(comp_drivetrain)
	_DELC(comp_engine_cmd_response)
	_DELC(comp_diff_state)
	_DELC(comp_engine_actuation)
	_DELC(comp_engine_drivetrain)
	_DELC(comp_road_geom)
	_DELC(comp_suspension)
	_DELC(comp_tire)
	_DELC(comp_constraint)
	_DELC(comp_rigid_body)
	_DELC(comp_end)
#undef _DELC
}

// ============================================================================
// Lifecycle: set_space
// ============================================================================

void PhysXVehicleServer::set_space(PhysXSpace3D *p_space) {
	if (space == p_space) {
		return;
	}
	if (space) {
		space->unregister_vehicle(this);
	}
	space = p_space;
	if (space && v2) {
		space->register_vehicle(this);
	}
}

// ============================================================================
// Shared state init (arrays + axle + rigid body + shared singletons)
// ============================================================================

void PhysXVehicleServer::_init_shared_state(Vehicle2State *s, physx::PxRigidDynamic &p_chassis, int p_wheel_count) {
	s->allocate_arrays(p_wheel_count);

	// axle description: all wheels on a single axle (first-pass).
	s->axle_description.setToDefault();
	if (p_wheel_count > 0) {
		physx::PxU32 ids[physx::PxVehicleLimits::eMAX_NB_WHEELS];
		for (int i = 0; i < p_wheel_count; i++) {
			ids[i] = (physx::PxU32)i;
		}
		s->axle_description.addAxle((physx::PxU32)p_wheel_count, ids);
	}

	// rigid body params from the chassis actor.
	s->rigid_body_params.mass = p_chassis.getMass();
	s->rigid_body_params.moi = p_chassis.getMassSpaceInertiaTensor();
	if (s->rigid_body_params.mass <= 0.0f) {
		s->rigid_body_params.mass = 1500.0f;
	}
	if (s->rigid_body_params.moi.isZero()) {
		s->rigid_body_params.moi = physx::PxVec3(1000.0f, 1000.0f, 1000.0f);
	}

	// shared singleton defaults
	s->rigid_body_state.setToDefault();
	s->susp_state_calc_params.suspensionJounceCalculationType =
			physx::PxVehicleSuspensionJounceCalculationType::eRAYCAST;
	s->susp_state_calc_params.limitSuspensionExpansionVelocity = true;
	s->physx_actor.setToDefault();
	s->physx_actor.rigidBody = &p_chassis;
	s->physx_steer_state.setToDefault();
	s->command_state.setToDefault();
	s->road_geom_params.roadGeometryQueryType = physx::PxVehiclePhysXRoadGeometryQueryType::eRAYCAST;
	s->road_geom_params.filterDataEntries = nullptr;
	s->road_geom_params.filterCallback = nullptr;
	s->road_geom_params.defaultFilterData = physx::PxQueryFilterData(
			physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::ePREFILTER);
}

// ============================================================================
// Lifecycle: adopt
// ============================================================================

bool PhysXVehicleServer::adopt(physx::PxRigidDynamic *p_chassis, PhysXBody3D *p_chassis_body) {
	if (!p_chassis) {
		return false;
	}

	// The teardown below destroys vehicle constraints bound to the OLD
	// chassis's scene and writes actor flags on both chassis actors — forbidden
	// while either space has a solve in flight (async stepping). Fetch both.
	if (space) {
		space->ensure_synced();
	}
	if (p_chassis_body && p_chassis_body->get_space() && p_chassis_body->get_space() != space) {
		p_chassis_body->get_space()->ensure_synced();
	}

	// Validate prerequisites BEFORE tearing down the previous configuration:
	// a failed adopt used to leave the new chassis registered (and the old one
	// un-flagged) with no vehicle state attached.
	PhysXServer3D *server = PhysXServer3D::get_singleton();
	physx::PxPhysics *px_physics = server ? server->try_get_physics() : nullptr;
	if (!px_physics) {
		WARN_PRINT_ONCE("PhysX: vehicle adopt skipped -- PhysX not initialized.");
		return false;
	}

	if (space) {
		space->unregister_vehicle(this);
	}
	if (v2) {
		// Release the low-speed tire-model constraints before the state they
		// reference (chassis actor, per-wheel params) goes away.
		physx::PxVehicleConstraintsDestroy(v2->physx_constraints);
	}
	delete v2;
	v2 = nullptr;

	// Clear the old chassis's vehicle flags before adopting a new one.
	// If the old chassis is not freed, it would float (eDISABLE_GRAVITY set)
	// and report is_vehicle_chassis=true without being part of a vehicle.
	if (chassis_actor) {
		chassis_actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
	}
	if (chassis_body) {
		chassis_body->set_is_vehicle_chassis(false);
		chassis_body->remove_chassis_vehicle(this);
	}

	chassis_actor = p_chassis;
	chassis_body = p_chassis_body;
	// Register for space-follow notifications: when the chassis body moves to
	// another space (body_set_space), this vehicle re-homes with it so road
	// geometry, gravity and the step loop come from the chassis's actual scene.
	if (chassis_body) {
		chassis_body->add_chassis_vehicle(this);
	}

	Vehicle2State *state = (archetype == ARCHETYPE_DIRECT_DRIVE)
			? build_direct_drive(*px_physics, *p_chassis, wheel_count)
			: build_engine_drive(*px_physics, *p_chassis, wheel_count);
	if (!state) {
		return false;
	}

	v2 = state;

	// Re-apply cached tuning onto the fresh state (wheel params, engine-drive
	// dicts, anti-roll, gravity-scaled tire seeds) — mirror of _rebuild's
	// re-apply list, so pre-adopt configuration survives the adopt.
	for (int i = 0; i < wheel_count && i < (int)cached_wheel_params.size(); i++) {
		if (!cached_wheel_params[i].is_empty()) {
			apply_wheel_params(i, cached_wheel_params[i]);
		}
	}
	if (archetype == ARCHETYPE_ENGINE_DRIVE) {
		if (!cached_engine_params.is_empty()) {
			set_engine_params(cached_engine_params);
		}
		if (!cached_clutch_params.is_empty()) {
			set_clutch_params(cached_clutch_params);
		}
		if (!cached_gearbox_params.is_empty()) {
			set_gearbox_params(cached_gearbox_params);
		}
		if (!cached_autobox_params.is_empty()) {
			set_autobox_params(cached_autobox_params);
		}
		if (!cached_differential_params.is_empty()) {
			set_differential_params(cached_differential_params);
		}
	}
	_apply_cached_anti_roll();
	_regroup_axles_from_poses();
	_reseed_tire_rest_load();

	// vehicle2 applies gravity through the simulation context, so the chassis
	// must be gravity-disabled. Mark the body as a vehicle chassis so its
	// on_pre_step() skips manual gravity (avoids double-gravity).
	p_chassis->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, true);
	if (chassis_body) {
		chassis_body->set_is_vehicle_chassis(true);
	}

	// Instantiate the low-speed tire-model constraints (excess suspension
	// compression + velocity constraints). The ConstraintComponent in the
	// sequence only fills constraintStates -- without these PhysX-side
	// objects the component had no simulation effect, so vehicles crept and
	// over-compressed their suspension at low speed.
	physx::PxVehicleConstraintsCreate(state->axle_description, *px_physics, *p_chassis,
			state->physx_constraints);

	_update_response_params();

	// Adopt the chassis's space when none was set explicitly. The vehicle
	// must live in the same PhysXSpace3D as its chassis so the space's step
	// loop calls update()/post_step() for it. Without this, the common setup
	// path (vehicle_create -> vehicle_set_chassis_body -> wheel config, no
	// explicit vehicle_set_space) left the vehicle unregistered — the whole
	// subsystem stayed inert because update() was never invoked. A later
	// explicit vehicle_set_space() still overrides this (set_space handles
	// unregister/re-register).
	if (!space && chassis_body) {
		space = chassis_body->get_space();
	}
	if (space) {
		space->register_vehicle(this);
	}
	configured = true;
	return true;
}

// ============================================================================
// Lifecycle: release
// ============================================================================

void PhysXVehicleServer::release() {
	// PxVehicleConstraintsDestroy below releases constraints bound to the
	// chassis's scene — forbidden while a solve is in flight (async stepping).
	// Fetch first. This also guards the server free() path, which calls
	// release().
	if (space) {
		space->ensure_synced();
		space->unregister_vehicle(this);
	}
	if (chassis_body) {
		chassis_body->set_is_vehicle_chassis(false);
		chassis_body->remove_chassis_vehicle(this);
	}
	if (chassis_actor) {
		chassis_actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, false);
	}
	if (v2) {
		physx::PxVehicleConstraintsDestroy(v2->physx_constraints);
	}
	delete v2;
	v2 = nullptr;
	chassis_body = nullptr;
	chassis_actor = nullptr;
	space = nullptr;
	configured = false;
	inputs_dirty = true;
	telemetry_valid = false;
}

// ============================================================================
// Factories
// ============================================================================

PhysXVehicleServer::Vehicle2State *PhysXVehicleServer::build_direct_drive(
		physx::PxPhysics &p_physics,
		physx::PxRigidDynamic &p_chassis,
		int p_wheel_count) {
	(void)p_physics;

	Vehicle2State *s = new Vehicle2State();
	s->arch = ARCHETYPE_DIRECT_DRIVE;
	_init_shared_state(s, p_chassis, p_wheel_count);
	s->direct_transmission_command.setToDefault();

	s->comp_begin = new BeginComponent(s);
	s->comp_cmd_response = new CommandResponseComponent(s);
	s->comp_actuation = new ActuationComponent(s);
	s->comp_road_geom = new RoadGeometryComponent(s);
	s->comp_suspension = new SuspensionComponent(s);
	s->comp_tire = new TireComponent(s);
	s->comp_drivetrain = new DirectDrivetrainComponent(s);
	s->comp_constraint = new ConstraintComponent(s);
	s->comp_rigid_body = new RigidBodyComponent(s);
	s->comp_end = new EndComponent(s);

	s->sequence.add(s->comp_begin);
	s->sequence.add(s->comp_cmd_response);
	s->sequence.add(s->comp_actuation);
	s->sequence.add(s->comp_road_geom);
	s->sequence.add(s->comp_suspension);
	s->sequence.add(s->comp_tire);
	s->sequence.add(s->comp_drivetrain);
	s->sequence.add(s->comp_constraint);
	s->sequence.add(s->comp_rigid_body);
	s->sequence.add(s->comp_end);
	return s;
}

PhysXVehicleServer::Vehicle2State *PhysXVehicleServer::build_engine_drive(
		physx::PxPhysics &p_physics,
		physx::PxRigidDynamic &p_chassis,
		int p_wheel_count) {
	(void)p_physics;

	Vehicle2State *s = new Vehicle2State();
	s->arch = ARCHETYPE_ENGINE_DRIVE;
	_init_shared_state(s, p_chassis, p_wheel_count);

	// --- Engine params (torque curve + MOI + rev range + damping). ---
	s->engine_params.moi = 1.0f;
	s->engine_params.peakTorque = 500.0f;
	s->engine_params.maxOmega = 600.0f; // ~5730 rpm
	s->engine_params.idleOmega = 80.0f;
	s->engine_params.dampingRateFullThrottle = 0.15f;
	s->engine_params.dampingRateZeroThrottleClutchEngaged = 2.0f;
	s->engine_params.dampingRateZeroThrottleClutchDisengaged = 0.35f;
	s->engine_params.torqueCurve.clear();
	s->engine_params.torqueCurve.addPair(0.0f, 0.8f);
	s->engine_params.torqueCurve.addPair(0.33f, 1.0f);
	s->engine_params.torqueCurve.addPair(1.0f, 0.8f);

	// --- Clutch. ---
	s->clutch_params.accuracyMode = physx::PxVehicleClutchAccuracyMode::eBEST_POSSIBLE;
	s->clutch_params.estimateIterations = 4; // unused in eBEST_POSSIBLE but set for safety
	s->clutch_response_params.maxResponse = 40.0f;

	// --- Gearbox: 1 reverse, neutral, 5 forward (ratios must descend). ---
	s->gearbox_params.neutralGear = 1;
	s->gearbox_params.ratios[0] = -4.0f; // reverse
	s->gearbox_params.ratios[1] = 0.0f; // neutral
	s->gearbox_params.ratios[2] = 4.0f; // 1st
	s->gearbox_params.ratios[3] = 2.0f; // 2nd
	s->gearbox_params.ratios[4] = 1.5f; // 3rd
	s->gearbox_params.ratios[5] = 1.1f; // 4th
	s->gearbox_params.ratios[6] = 0.9f; // 5th
	s->gearbox_params.nbRatios = 7;
	s->gearbox_params.finalRatio = 3.5f;
	s->gearbox_params.switchTime = 0.5f;

	// --- Autobox (automatic up/down shift thresholds + latency). ---
	for (int i = 0; i < physx::PxVehicleGearboxParams::eMAX_NB_GEARS; i++) {
		s->autobox_params.upRatios[i] = 0.65f;
		s->autobox_params.downRatios[i] = 0.40f;
	}
	s->autobox_params.latency = 2.0f;

	// --- Differential: open 4-wheel with equal torque split (ratios sum to 1). ---
	s->diff_params.setToDefault();
	if (p_wheel_count > 0) {
		const float equal_wheel_ratio = 1.0f / (float)p_wheel_count;
		for (int i = 0; i < p_wheel_count; i++) {
			s->diff_params.torqueRatios[i] = equal_wheel_ratio;
			s->diff_params.aveWheelSpeedRatios[i] = equal_wheel_ratio;
		}
	}
	// frontWheelIds/rearWheelIds/biases stay 0 -> open diff (not validated).

	// --- EngineDrive states. ---
	s->engine_state.setToDefault();
	s->gearbox_state.setToDefault();
	s->clutch_response_state.setToDefault();
	s->diff_state.setToDefault();
	s->autobox_state.setToDefault();
	s->clutch_slip_state.setToDefault();
	s->constraint_group_state.setToDefault();
	s->engine_throttle_response_state.setToDefault();
	s->engine_transmission_command.setToDefault();

	// --- Components (EngineDrive sequence). ---
	s->comp_begin = new BeginComponent(s);
	s->comp_engine_cmd_response = new EngineDriveCommandResponseComponent(s);
	s->comp_diff_state = new FourWheelDriveDifferentialStateComponent(s);
	s->comp_engine_actuation = new EngineDriveActuationStateComponent(s);
	s->comp_road_geom = new RoadGeometryComponent(s);
	s->comp_suspension = new SuspensionComponent(s);
	s->comp_tire = new TireComponent(s);
	s->comp_engine_drivetrain = new EngineDrivetrainComponent(s);
	s->comp_constraint = new ConstraintComponent(s);
	s->comp_rigid_body = new RigidBodyComponent(s);
	s->comp_end = new EndComponent(s);

	s->sequence.add(s->comp_begin);
	s->sequence.add(s->comp_engine_cmd_response);
	s->sequence.add(s->comp_diff_state);
	s->sequence.add(s->comp_engine_actuation);
	s->sequence.add(s->comp_road_geom);
	s->sequence.add(s->comp_suspension);
	s->sequence.add(s->comp_tire);
	s->sequence.add(s->comp_engine_drivetrain);
	s->sequence.add(s->comp_constraint);
	s->sequence.add(s->comp_rigid_body);
	s->sequence.add(s->comp_end);
	return s;
}

// ============================================================================
// Response-param seeding + re-build
// ============================================================================

void PhysXVehicleServer::_regroup_axles_from_poses() {
	// VEH-4: the build puts ALL wheels on one axle (the old first-pass), which
	// couples wheels that should be independent in the suspension-limit
	// constraint and the axle-relative differential logic. Infer axles from
	// the wheel attachment poses: cluster by the attachment's local
	// longitudinal (frame lng) coordinate — wheels sharing a Z within 5 cm
	// share an axle, clusters ordered front -> back. Identity poses (no
	// local_pose authored) all share one cluster, preserving the old
	// single-axle behavior.
	if (!v2 || wheel_count <= 0) {
		return;
	}
	float z[physx::PxVehicleLimits::eMAX_NB_WHEELS];
	bool any_pose = false;
	const physx::PxVec3 fwd = physx_vehicle_frame().getLngAxis();
	for (int i = 0; i < wheel_count; i++) {
		z[i] = v2->wheel_shape_local_poses[i].p.dot(fwd);
		if (v2->wheel_shape_local_poses[i] != physx::PxTransform(physx::PxIdentity)) {
			any_pose = true;
		}
	}
	if (!any_pose) {
		return;
	}
	float zs[physx::PxVehicleLimits::eMAX_NB_WHEELS];
	int nb_z = 0;
	for (int i = 0; i < wheel_count; i++) {
		bool found = false;
		for (int j = 0; j < nb_z; j++) {
			if (Math::abs(zs[j] - z[i]) < 0.05f) {
				found = true;
				break;
			}
		}
		if (!found && nb_z < (int)physx::PxVehicleLimits::eMAX_NB_AXLES) {
			zs[nb_z++] = z[i];
		}
	}
	if (nb_z <= 1) {
		return;
	}
	for (int i = 0; i < nb_z; i++) {
		for (int j = i + 1; j < nb_z; j++) {
			if (zs[j] < zs[i]) {
				const float z_swap_temp = zs[i];
				zs[i] = zs[j];
				zs[j] = z_swap_temp;
			}
		}
	}
	physx::PxU32 ids[physx::PxVehicleLimits::eMAX_NB_WHEELS];
	physx::PxVehicleAxleDescription fresh;
	fresh.setToDefault();
	for (int a = 0; a < nb_z; a++) {
		int wheels_in_axle = 0;
		for (int i = 0; i < wheel_count; i++) {
			if (Math::abs(zs[a] - z[i]) < 0.05f) {
				ids[wheels_in_axle++] = (physx::PxU32)i;
			}
		}
		fresh.addAxle((physx::PxU32)wheels_in_axle, ids);
	}
	v2->axle_description = fresh;
}

void PhysXVehicleServer::_reseed_tire_rest_load() {
	// VEH-5: the wheel-param defaults hardcode 9.81 for the load-dependent
	// tire stiffness seed (restLoad); a scene with custom gravity got the
	// wrong seed. Patch it from the chassis's actual scene after every build.
	if (!v2 || wheel_count <= 0) {
		return;
	}
	float gravity_mps2 = 9.81f;
	if (space && space->get_px_scene()) {
		const float sg = space->get_px_scene()->getGravity().magnitude();
		if (sg > 0.0f) {
			gravity_mps2 = sg;
		}
	}
	for (int i = 0; i < wheel_count; i++) {
		v2->tire_params[i].restLoad = v2->suspension_force_params[i].sprungMass * gravity_mps2;
	}
}

void PhysXVehicleServer::_update_response_params() {
	if (!v2 || wheel_count <= 0) {
		return;
	}
	delete v2->road_geometry_filter;
	v2->road_geometry_filter = new PhysXQueryFilterCallback();
	v2->road_geometry_filter->collision_mask = chassis_body ? chassis_body->get_collision_mask() : 0xFFFFFFFF;
	v2->road_geometry_filter->collide_with_bodies = true;
	v2->road_geometry_filter->collide_with_areas = false;
	v2->road_geometry_filter->multi_hit = false;
	v2->chassis_rid_set.clear();
	if (chassis_body) {
		v2->chassis_rid_set.insert(chassis_body->get_rid());
	}
	v2->road_geometry_filter->exclude_rids = &v2->chassis_rid_set;
	v2->road_geom_params.filterCallback = v2->road_geometry_filter;

	// Nominal max responses so a freshly-configured vehicle actually moves.
	// Tuned via vehicle_set_response_params(); -1 keeps the built-in default.
	// Steer and both brake channels apply to both archetypes; the throttle
	// response struct is DirectDrive-only (EngineDrive drive torque comes from
	// engine/gearbox params).
	v2->throttle_response_params.maxResponse = (tune_drive_torque >= 0.0f) ? tune_drive_torque : 1000.0f; // DirectDrive: Nm/wheel at full throttle
	v2->steer_response_params.maxResponse = (tune_max_steer_angle >= 0.0f) ? tune_max_steer_angle : 0.6f; // max steer angle (rad)
	v2->brake_response_params[0].maxResponse = (tune_brake_torque >= 0.0f) ? tune_brake_torque : 2000.0f; // brake torque
	v2->brake_response_params[1].maxResponse = (tune_handbrake_torque >= 0.0f) ? tune_handbrake_torque : 3000.0f; // handbrake torque
	// Baseline snapshot (VEH-1): per-wheel commands may RAISE maxResponse to
	// the largest demanded torque (the response contract caps multipliers at
	// 1). Without the snapshot the raised value stuck after the channel
	// returned to scalar mode, silently over-driving the tuned scalar torque.
	scoped_drive_max_response = v2->throttle_response_params.maxResponse;
	scoped_brake_max_response = v2->brake_response_params[0].maxResponse;

	for (int i = 0; i < wheel_count; i++) {
		const WheelFlags &wf = wheel_flags[i];
		v2->throttle_response_params.wheelResponseMultipliers[i] = wf.traction ? 1.0f : 0.0f;
		v2->steer_response_params.wheelResponseMultipliers[i] = wf.steer ? 1.0f : 0.0f;
		v2->brake_response_params[0].wheelResponseMultipliers[i] = wf.brake ? 1.0f : 0.0f;
		v2->brake_response_params[1].wheelResponseMultipliers[i] = 1.0f; // handbrake: all wheels
	}

	// EngineDrive: split differential torque across driven wheels only
	// (RWD/FWD/4WD/6x4 via WheelFlags.traction). torqueRatios/aveWheelSpeedRatios
	// must each sum to 1.0, so driven wheels share equally. DirectDrive ignores
	// the differential (it has no differential component).
	if (archetype == ARCHETYPE_ENGINE_DRIVE) {
		int nb_driven = 0;
		for (int i = 0; i < wheel_count; i++) {
			if (wheel_flags[i].traction) {
				nb_driven++;
			}
		}
		const float driven_torque_ratio = (nb_driven > 0) ? (1.0f / (float)nb_driven) : (1.0f / (float)wheel_count);
		for (int i = 0; i < wheel_count; i++) {
			const bool driven = (nb_driven > 0) ? wheel_flags[i].traction : true;
			v2->diff_params.torqueRatios[i] = driven ? driven_torque_ratio : 0.0f;
			v2->diff_params.aveWheelSpeedRatios[i] = driven ? driven_torque_ratio : 0.0f;
		}
	}
}

void PhysXVehicleServer::_rebuild() {
	if (!chassis_actor || !v2) {
		return;
	}
	const bool was_registered = (space != nullptr);
	if (space) {
		space->unregister_vehicle(this);
	}
	if (v2) {
		physx::PxVehicleConstraintsDestroy(v2->physx_constraints);
	}
	delete v2;
	v2 = nullptr;

	PhysXServer3D *server = PhysXServer3D::get_singleton();
	physx::PxPhysics *px_physics = server ? server->try_get_physics() : nullptr;
	if (!px_physics) {
		configured = false;
		return;
	}

	v2 = (archetype == ARCHETYPE_DIRECT_DRIVE)
			? build_direct_drive(*px_physics, *chassis_actor, wheel_count)
			: build_engine_drive(*px_physics, *chassis_actor, wheel_count);

	// Re-apply the cached tuning: the fresh v2 carries defaults, but the
	// per-wheel and EngineDrive configuration the user applied before the
	// wheel-count change must survive the re-assembly.
	for (int i = 0; i < wheel_count && i < (int)cached_wheel_params.size(); i++) {
		if (!cached_wheel_params[i].is_empty()) {
			apply_wheel_params(i, cached_wheel_params[i]);
		}
	}
	if (archetype == ARCHETYPE_ENGINE_DRIVE && v2) {
		if (!cached_engine_params.is_empty()) {
			set_engine_params(cached_engine_params);
		}
		if (!cached_clutch_params.is_empty()) {
			set_clutch_params(cached_clutch_params);
		}
		if (!cached_gearbox_params.is_empty()) {
			set_gearbox_params(cached_gearbox_params);
		}
		if (!cached_autobox_params.is_empty()) {
			set_autobox_params(cached_autobox_params);
		}
		if (!cached_differential_params.is_empty()) {
			set_differential_params(cached_differential_params);
		}
	}
	_apply_cached_anti_roll();

	physx::PxVehicleConstraintsCreate(v2->axle_description, *px_physics, *chassis_actor,
			v2->physx_constraints);

	_update_response_params();
	_regroup_axles_from_poses();
	_reseed_tire_rest_load();

	if (was_registered && space) {
		space->register_vehicle(this);
	}
	configured = true;
}

// ============================================================================
// Per-step: write_commands — input cache -> vehicle2 command state
// ============================================================================

// 2-wheeler roll-balance assist. Reads the chassis roll (lean) state, applies
// the optional low-speed corrective torque, and computes the steer
// augmentation consumed by write_commands(). A motorcycle is an inverted
// pendulum: it stays up by steering into its own fall, which the PD law below
// emulates. Sign conventions (validated by PHYSX-VEHI-016/017): lean > 0 means
// the chassis tips toward its +X side, and a positive steer command yaws the
// vehicle toward -X — so steering into a +X fall needs a NEGATIVE steer
// contribution, hence the leading minus in the control law.
void PhysXVehicleServer::_update_balance(float p_step) {
	(void)p_step;
	balance_steer_assist = 0.0f;
	if (!balance_enabled || !chassis_actor || wheel_count != 2) {
		balance_lean_angle = 0.0f;
		balance_roll_rate = 0.0f;
		return;
	}

	const physx::PxTransform pose = chassis_actor->getGlobalPose();
	const physx::PxVec3 fwd = pose.q.rotate(physx::PxVec3(0, 0, -1));
	const physx::PxVec3 up_chassis = pose.q.rotate(physx::PxVec3(0, 1, 0));
	const physx::PxVec3 world_up(0, 1, 0);
	physx::PxVec3 right = fwd.cross(world_up);
	if (right.magnitudeSquared() < 1e-8f) {
		return; // chassis stood on end; no meaningful roll axis this frame
	}
	right.normalize();

	// Signed roll: 0 upright, positive = tipped toward the chassis +X side.
	balance_lean_angle = Math::atan2(right.dot(up_chassis), world_up.dot(up_chassis));
	// Roll rate: angular velocity about the forward axis (same sign as lean).
	balance_roll_rate = chassis_actor->getAngularVelocity().dot(fwd);

	// Steer into the fall, capped so the assist can never exceed a sane steer
	// angle. Only meaningful while the contact line can generate lateral force.
	balance_steer_assist = CLAMP(
			-(balance_kp * balance_lean_angle + balance_kd * balance_roll_rate),
			-balance_max_steer_assist, balance_max_steer_assist);

	// Low-speed corrective torque: below ~3 m/s the front wheel's steer
	// authority vanishes (little lateral force available), so a direct
	// righting torque fades in. Scales with the param; 0 disables it.
	if (balance_low_speed_torque > 0.0f) {
		const float speed = chassis_actor->getLinearVelocity().magnitude();
		const float fade = CLAMP(1.0f - speed / 3.0f, 0.0f, 1.0f);
		if (fade > 0.0f) {
			const float corr = -(balance_kp * balance_lean_angle + balance_kd * balance_roll_rate);
			// addTorque does not wake a sleeping actor (VEH-6): a bike that
			// fell asleep leaning gets no righting torque without this.
			chassis_actor->wakeUp();
			chassis_actor->addTorque(fwd * (corr * fade * balance_low_speed_torque));
		}
	}
}

void PhysXVehicleServer::write_commands() {
	if (!v2) {
		return;
	}
	v2->command_state.setToDefault();
	v2->command_state.brakes[0] = in_brake;
	v2->command_state.brakes[1] = in_handbrake;
	v2->command_state.nbBrakes = 2;
	// Effective scalar steer: rider input + balance assist (rad -> normalized
	// against the steer lock).
	const float steer_max = v2->steer_response_params.maxResponse;
	const float steer_norm = (steer_max > 1e-4f)
			? CLAMP(in_steer + balance_steer_assist / steer_max, -1.0f, 1.0f)
			: CLAMP(in_steer, -1.0f, 1.0f);
	v2->command_state.steer = steer_norm;

	if (archetype == ARCHETYPE_DIRECT_DRIVE) {
		// Direct drive: throttle sign selects direction.
		// NOTE (convention audit): the raw direct drive rolls +Z_local with
		// lngAxis = eNegZ (see PhysXVehicle3D's command write for the node
		// stack's flip). THIS API keeps the raw behavior -- its wheel mounts
		// are built caller-side, so direction follows whoever assembles the
		// vehicle; no flip is forced here.
		if (in_throttle > 0.0f) {
			v2->command_state.throttle = in_throttle;
			v2->direct_transmission_command.gear =
					physx::PxVehicleDirectDriveTransmissionCommandState::eFORWARD;
		} else if (in_throttle < 0.0f) {
			v2->command_state.throttle = -in_throttle;
			v2->direct_transmission_command.gear =
					physx::PxVehicleDirectDriveTransmissionCommandState::eREVERSE;
		} else {
			v2->command_state.throttle = 0.0f;
			v2->direct_transmission_command.gear =
					physx::PxVehicleDirectDriveTransmissionCommandState::eNEUTRAL;
		}

		// Per-wheel control: if any per-wheel value is non-zero, that
		// channel switches to per-wheel mode — the scalar command becomes 1.0
		// and the per-wheel value is encoded in the response multipliers
		// (torque = maxResponse * multiplier * command). Per-channel auto-detect
		// lets the scalar and per-wheel paths coexist. When a channel is NOT in
		// per-wheel mode its multipliers are re-seeded from the wheel flags, so
		// a previous per-wheel encoding doesn'wheel_drive_torque stick.
		// Drive
		bool pw_drive = false;
		float drive_sum = 0.0f;
		for (int i = 0; i < wheel_count; i++) {
			if (in_wheel_drive_torque[i] != 0.0f) {
				pw_drive = true;
				drive_sum += in_wheel_drive_torque[i];
			}
		}
		if (pw_drive) {
			const float maxr = v2->throttle_response_params.maxResponse;
			float inv = (maxr > 0.0f) ? (1.0f / maxr) : 1.0f;
			v2->command_state.throttle = 1.0f;
			// The gear flips the sign of the throttle response for the whole
			// vehicle, so it follows the DOMINANT commanded direction (the
			// sign of the torque sum) and each wheel's multiplier carries its
			// own sign relative to that: multiplier = wheel_drive_torque * gearSign * inv.
			// gearSign^2 == 1 cancels out of the applied torque, so every
			// wheel drives in ITS commanded direction -- counter-rotating
			// wheels (skid steering) work instead of silently clamping to
			// the dominant direction.
			const float gear_sign = (drive_sum >= 0.0f) ? 1.0f : -1.0f;
			v2->direct_transmission_command.gear = (drive_sum >= 0.0f)
					? physx::PxVehicleDirectDriveTransmissionCommandState::eFORWARD
					: physx::PxVehicleDirectDriveTransmissionCommandState::eREVERSE;
			for (int i = 0; i < wheel_count; i++) {
				const float wheel_drive_torque = in_wheel_drive_torque[i];
				const float demand = (wheel_drive_torque >= 0.0f) ? wheel_drive_torque : -wheel_drive_torque;
				if (demand * inv > 1.0f) {
					// A wheel torque above maxResponse would encode a multiplier > 1 --
					// outside the response contract. Raise maxResponse to the largest
					// demanded torque instead of clamping (clamping would silently
					// weaken the other wheels' drive).
					v2->throttle_response_params.maxResponse = demand;
					inv = 1.0f / demand;
				}
				v2->throttle_response_params.wheelResponseMultipliers[i] = wheel_drive_torque * gear_sign * inv;
			}
		} else {
			// Scalar mode: restore the TUNED baseline (see the snapshot in
			// _update_response_params) so a past per-wheel demand cannot keep
			// the channel over-driven.
			v2->throttle_response_params.maxResponse = scoped_drive_max_response;
			for (int i = 0; i < wheel_count; i++) {
				v2->throttle_response_params.wheelResponseMultipliers[i] = wheel_flags[i].traction ? 1.0f : 0.0f;
			}
		}
		// Brake (main brake channel; handbrake channel untouched)
		bool pw_brake = false;
		for (int i = 0; i < wheel_count; i++) {
			if (in_wheel_brake_torque[i] != 0.0f) {
				pw_brake = true;
				break;
			}
		}
		if (pw_brake) {
			const float maxr = v2->brake_response_params[0].maxResponse;
			float inv = (maxr > 0.0f) ? (1.0f / maxr) : 1.0f;
			v2->command_state.brakes[0] = 1.0f;
			// Brake magnitude only: a brake torque resists wheel rotation, it
			// has no direction of its own, so the sign of the command is
			// meaningless and |wheel_drive_torque| is the intended semantics.
			for (int i = 0; i < wheel_count; i++) {
				const float wheel_drive_torque = in_wheel_brake_torque[i];
				const float demand = (wheel_drive_torque < 0.0f ? -wheel_drive_torque : wheel_drive_torque);
				if (demand * inv > 1.0f) {
					// Same contract as the drive channel: a brake torque above
					// maxResponse must raise maxResponse, not clamp the multiplier.
					v2->brake_response_params[0].maxResponse = demand;
					inv = 1.0f / demand;
				}
				v2->brake_response_params[0].wheelResponseMultipliers[i] = demand * inv;
			}
		} else {
			// Scalar mode: restore the tuned baseline (VEH-1, mirror of drive).
			v2->brake_response_params[0].maxResponse = scoped_brake_max_response;
			for (int i = 0; i < wheel_count; i++) {
				v2->brake_response_params[0].wheelResponseMultipliers[i] = wheel_flags[i].brake ? 1.0f : 0.0f;
			}
		}
	} else {
		// EngineDrive: throttle in [0,1]; gear from in_target_gear (defaults to
		// eAUTOMATIC_GEAR so the autobox shifts).
		v2->command_state.throttle = in_throttle > 0.0f ? in_throttle : 0.0f;
		v2->engine_transmission_command.clutch = 0.0f; // engaged (no separate clutch input yet)
		v2->engine_transmission_command.targetGear = (physx::PxU32)in_target_gear;
	}

	// --- Steer resolution (shared by both archetypes) ---
	// Priority: per-wheel steer mode > Ackermann geometry > flat scalar.
	// The per-wheel road-wheel angles are cached for vehicle_get_wheel_steer_
	// angles(); multipliers encode them as fractions of the steer lock.
	computed_steer_angles.resize(wheel_count);
	bool pw_steer = false;
	for (int i = 0; i < wheel_count; i++) {
		if (in_wheel_steer_angle[i] != 0.0f) {
			pw_steer = true;
			break;
		}
	}
	const float steer_inv = (steer_max > 1e-4f) ? (1.0f / steer_max) : 1.0f;
	if (pw_steer) {
		v2->command_state.steer = 1.0f;
		for (int i = 0; i < wheel_count; i++) {
			computed_steer_angles[i] = in_wheel_steer_angle[i];
			v2->steer_response_params.wheelResponseMultipliers[i] = in_wheel_steer_angle[i] * steer_inv;
		}
	} else if (ackermann_enabled) {
		// Geometry: wheelbase/track from the tuned values when given, else
		// derived from the suspension attachments (chassis space, forward = -Z
		// per the vehicle frame below: the steered axle is the more negative wheel_local_z).
		float wheelbase = tune_ackermann_wheelbase;
		float track = tune_ackermann_track;
		if (wheelbase <= 0.0f || track <= 0.0f) {
			float z_steer = 0.0f, z_rest = 0.0f;
			int n_steer = 0, n_rest = 0;
			float min_x = 0.0f, max_x = 0.0f;
			bool have_x = false;
			for (int i = 0; i < wheel_count; i++) {
				const float wheel_local_x = v2->wheel_shape_local_poses[i].p.x;
				const float wheel_local_z = v2->wheel_shape_local_poses[i].p.z;
				if (wheel_flags[i].steer) {
					z_steer += wheel_local_z;
					n_steer++;
					if (!have_x || wheel_local_x < min_x) {
						min_x = wheel_local_x;
					}
					if (!have_x || wheel_local_x > max_x) {
						max_x = wheel_local_x;
					}
					have_x = true;
				} else {
					z_rest += wheel_local_z;
					n_rest++;
				}
			}
			if (n_steer > 0 && n_rest > 0) {
				wheelbase = Math::abs(z_steer / n_steer - z_rest / n_rest);
			}
			if (n_steer > 1) {
				track = max_x - min_x;
			}
		}

		const float delta = steer_norm * steer_max; // commanded road-wheel angle (rad)
		if (wheelbase > 1e-4f && track > 1e-4f && Math::abs(delta) > 1e-4f) {
			// Pure Ackermann: both steered wheels point at the same turn center.
			// cot(inner) = cot(delta) - track/(2*wheelbase) (inner steers more).
			// Work on |delta| and re-apply the sign: cot(delta) is negative for
			// a right-hand (negative) command, and atan2(1, negative) returns a
			// second-quadrant angle (~pi) which would steer the wheels clean
			// around. Plain atan(1/wheel_local_x) on positive wheel_local_x stays in the right quadrant;
			// positive-steer results are unchanged (atan2(1, wheel_local_x>0) == atan(1/wheel_local_x)).
			const float adelta = Math::abs(delta);
			const float cot = Math::cos(adelta) / Math::sin(adelta);
			const float half_track_over_L = (0.5f * track) / wheelbase;
			const float inner = Math::atan(1.0f / MAX(cot - half_track_over_L, 0.001f));
			const float outer = Math::atan(1.0f / MAX(cot + half_track_over_L, 0.001f));
			const float ackermann_blend = CLAMP(ackermann_percent, 0.0f, 100.0f) / 100.0f;
			// Turn direction: delta > 0 yaws the vehicle toward its -X side
			// (frame lng = -Z, lat = -X; validated by PHYSX-VEHI-014, which
			// measures the yaw response rather than trusting this comment).
			const bool turn_toward_neg_x = (delta > 0.0f);
			v2->command_state.steer = 1.0f; // signed per-wheel multipliers carry the angle
			for (int i = 0; i < wheel_count; i++) {
				if (wheel_flags[i].steer) {
					const bool wheel_on_neg_x = v2->wheel_shape_local_poses[i].p.x < 0.0f;
					const bool is_inner = (wheel_on_neg_x == turn_toward_neg_x);
					const float target = (delta > 0.0f ? 1.0f : -1.0f) *
							(adelta + ackermann_blend * ((is_inner ? inner : outer) - adelta));
					computed_steer_angles[i] = target;
					v2->steer_response_params.wheelResponseMultipliers[i] = target * steer_inv;
				} else {
					computed_steer_angles[i] = 0.0f;
					v2->steer_response_params.wheelResponseMultipliers[i] = 0.0f;
				}
			}
		} else {
			// Straight (or degenerate geometry): flat steer from the flags.
			v2->command_state.steer = steer_norm;
			for (int i = 0; i < wheel_count; i++) {
				computed_steer_angles[i] = wheel_flags[i].steer ? (steer_norm * steer_max) : 0.0f;
				v2->steer_response_params.wheelResponseMultipliers[i] = wheel_flags[i].steer ? 1.0f : 0.0f;
			}
		}
	} else {
		v2->command_state.steer = steer_norm;
		for (int i = 0; i < wheel_count; i++) {
			computed_steer_angles[i] = wheel_flags[i].steer ? (steer_norm * steer_max) : 0.0f;
			v2->steer_response_params.wheelResponseMultipliers[i] = wheel_flags[i].steer ? 1.0f : 0.0f;
		}
	}

	inputs_dirty = false;
}

// ============================================================================
// Per-step: update — build the PhysX simulation context + step the sequence.
// ============================================================================

void PhysXVehicleServer::update(float p_step) {
	if (!v2 || !configured || !chassis_actor || !space) {
		return;
	}
	// Balance assist first: it reads the freshest chassis roll state and may
	// adjust the steer command consumed by write_commands below.
	_update_balance(p_step);
	// Re-write commands whenever the assist is live (the steer augmentation
	// changes every frame even with constant rider input), not just when the
	// cached inputs changed.
	if (inputs_dirty || balance_enabled || ackermann_enabled) {
		write_commands();
	}

	physx::PxScene *px_scene = space->get_px_scene();
	if (!px_scene) {
		return;
	}

	physx::PxVehiclePhysXSimulationContext ctx;
	ctx.setToDefault();
	ctx.physxScene = px_scene;
	ctx.gravity = px_scene->getGravity();
	// Godot frame: forward -Z, up +Y (lateral -X for a proper right-handed
	// frame). Open item (unverifiable by the current suite, which only drives
	// straight): a positive steer input must curve the chassis toward Godot's
	// +X; flip latAxis if a curved-drive test ever shows the mirror image.
	ctx.frame.lngAxis = physx::PxVehicleAxes::eNegZ;
	ctx.frame.latAxis = physx::PxVehicleAxes::eNegX;
	ctx.frame.vrtAxis = physx::PxVehicleAxes::ePosY;
	ctx.scale.scale = 1.0f;
	ctx.physxActorUpdateMode = physx::PxVehiclePhysXActorUpdateMode::eAPPLY_ACCELERATION;

	// Mark the constraints dirty so the PhysX scene processes them this step
	// (the SDK contract for PxVehicleConstraintsCreate'd objects).
	physx::PxVehicleConstraintsDirtyStateUpdate(v2->physx_constraints);

	// NOTE: gravity is NOT applied manually here. The RigidBodyComponent in
	// the sequence integrates gravity into rigidBodyState, and the actor end
	// component writes the resulting velocity delta back as an acceleration
	// on the chassis (which has eDISABLE_GRAVITY). A manual addForce here
	// would double gravity.

	v2->sequence.update(p_step, ctx);
}

// ============================================================================
// Post-step: read vehicle2 state -> telemetry cache
// ============================================================================

void PhysXVehicleServer::post_step(float p_step) {
	if (!v2) {
		return;
	}
	(void)p_step;

	wheel_telemetry.clear();
	wheel_telemetry.resize(v2->nb_wheels);
	for (int i = 0; i < v2->nb_wheels; i++) {
		WheelTelemetry &wt = wheel_telemetry[i];
		const physx::PxVehicleRoadGeometryState &rg = v2->road_geometry_states[i];
		const physx::PxVehiclePhysXRoadGeometryQueryState &pxrg = v2->physx_road_geometry_states[i];
		wt.in_contact = rg.hitState ? true : false;
		if (wt.in_contact) {
			wt.contact_point = Vector3((real_t)pxrg.hitPosition.x, (real_t)pxrg.hitPosition.y, (real_t)pxrg.hitPosition.z);
			wt.contact_normal = Vector3((real_t)rg.plane.n.x, (real_t)rg.plane.n.y, (real_t)rg.plane.n.z);
			wt.contact_body_rid = resolve_contact_body(pxrg.actor);
		} else {
			wt.contact_point = Vector3();
			wt.contact_normal = Vector3();
			wt.contact_body_rid = RID();
		}
		// Wheel rotation: rad/s -> rpm, plus accumulated angle (rad).
		wt.rpm = v2->wheel_rigid_body_1d_states[i].rotationSpeed * (60.0f / 6.28318530717958647692f);
		wt.rotation = (real_t)v2->wheel_rigid_body_1d_states[i].rotationAngle;
		// Longitudinal/lateral tire slip ratios as the skid indicators.
		wt.skid = (real_t)v2->tire_slip_states[i].slips[physx::PxVehicleTireDirectionModes::eLONGITUDINAL];
		wt.skid_lateral = (real_t)v2->tire_slip_states[i].slips[physx::PxVehicleTireDirectionModes::eLATERAL];
	}

	// EngineDrive telemetry.
	if (archetype == ARCHETYPE_ENGINE_DRIVE) {
		// rad/s -> rpm: rpm = omega * 60 / (2*pi).
		engine_rpm = v2->engine_state.rotationSpeed * (60.0f / 6.28318530717958647692f);
		engine_gear = (int)v2->gearbox_state.currentGear;
		clutch_resp = v2->clutch_response_state.commandResponse;
	}
	telemetry_valid = true;
}

// ============================================================================
// Wheel configuration helpers
// ============================================================================

void PhysXVehicleServer::allocate_wheel_buffers(int p_count) {
	const int old_count = wheel_count;
	wheel_count = p_count;

	// Size the Godot-side per-wheel caches first so _update_response_params()
	// (called inside _rebuild) sees a correctly-sized wheel_flags.
	wheel_flags.resize(p_count);
	in_wheel_drive_torque.resize(p_count);
	in_wheel_brake_torque.resize(p_count);
	in_wheel_steer_angle.resize(p_count);
	// The steer-angle cache must match the wheel count immediately:
	// vehicle_get_wheel_steer_angles() reads it before the first
	// write_commands() has ever run (scripts poll it between
	// vehicle_set_wheel_count() and the first completed step).
	computed_steer_angles.resize(p_count);
	for (float &angle : computed_steer_angles) {
		angle = 0.0f;
	}
	for (int i = old_count; i < p_count; i++) {
		wheel_flags[i] = WheelFlags();
		in_wheel_drive_torque[i] = 0.0f;
		in_wheel_brake_torque[i] = 0.0f;
		in_wheel_steer_angle[i] = 0.0f;
	}

	if (v2) {
		// A wheel-count change requires re-assembling the component sequence
		// (components bind to per-wheel array addresses, which move on realloc).
		_rebuild();
	}
}

void PhysXVehicleServer::apply_wheel_params(int p_idx, const Dictionary &p_params) {
	// Cache for _rebuild re-apply (see cached_wheel_params' own note).
	if (p_idx >= 0) {
		if (p_idx >= (int)cached_wheel_params.size()) {
			cached_wheel_params.resize(p_idx + 1);
		}
		cached_wheel_params[p_idx] = p_params;
	}

	if (!v2 || p_idx < 0 || p_idx >= wheel_count) {
		return;
	}

	physx::PxVehicleWheelParams &wp = v2->wheel_params[p_idx];
	physx::PxVehicleSuspensionParams &sp = v2->suspension_params[p_idx];
	physx::PxVehicleSuspensionForceParams &sfp = v2->suspension_force_params[p_idx];
	physx::PxVehicleTireForceParams &tp = v2->tire_params[p_idx];
	WheelFlags &wf = wheel_flags[p_idx];

	if (p_params.has("radius")) {
		wp.radius = (float)(double)p_params["radius"];
	}
	if (p_params.has("wheel_mass")) {
		wp.mass = (float)(double)p_params["wheel_mass"];
	}
	if (p_params.has("wheel_moi")) {
		wp.moi = (float)(double)p_params["wheel_moi"];
	}
	if (p_params.has("wheel_damping")) {
		wp.dampingRate = (float)(double)p_params["wheel_damping"];
	}

	if (p_params.has("suspension_travel")) {
		sp.suspensionTravelDist = (float)(double)p_params["suspension_travel"];
	}
	if (p_params.has("local_pose")) {
		physx::PxTransform shape_pose = _to_px_transform((Transform3D)p_params["local_pose"]);
		// vehicle2's rigid-body frame is the chassis CENTER-OF-MASS frame,
		// while the caller specifies the wheel attachment in the chassis
		// actor (body) frame. Convert so the suspension raycasts land where
		// the caller positioned them; without this, any chassis whose COM is
		// offset from the body origin (every real vehicle with asymmetric
		// collision shapes) casts its wheel rays from a displaced position
		// and the wheels miss the ground.
		if (chassis_actor) {
			const physx::PxTransform com = chassis_actor->getCMassLocalPose();
			shape_pose = com.getInverse() * shape_pose;
		}
		sp.suspensionAttachment = shape_pose;
		v2->wheel_shape_local_poses[p_idx] = shape_pose;
		v2->wheel_local_poses[p_idx].localPose = shape_pose;
	}

	if (p_params.has("suspension_stiffness")) {
		sfp.stiffness = (float)(double)p_params["suspension_stiffness"];
	}
	if (p_params.has("suspension_damping")) {
		sfp.damping = (float)(double)p_params["suspension_damping"];
	}
	if (p_params.has("suspension_sprung_mass")) {
		sfp.sprungMass = (float)(double)p_params["suspension_sprung_mass"];
	}

	if (p_params.has("tire_friction")) {
		const float tire_friction = (float)(double)p_params["tire_friction"];
		tp.frictionVsSlip[0][0] = 0.0f;
		tp.frictionVsSlip[0][1] = tire_friction;
		tp.frictionVsSlip[1][0] = 0.1f;
		tp.frictionVsSlip[1][1] = tire_friction;
		tp.frictionVsSlip[2][0] = 1.0f;
		tp.frictionVsSlip[2][1] = tire_friction;
	}
	if (p_params.has("tire_long_stiffness")) {
		tp.longStiff = (float)(double)p_params["tire_long_stiffness"];
	}

	if (p_params.has("surface_friction_default")) {
		// Friction used when the ground material has no explicit mapping.
		v2->material_friction_params[p_idx].defaultFriction = (float)(double)p_params["surface_friction_default"];
	}
	if (p_params.has("surface_frictions")) {
		// Per-wheel grip table: { ground_body_rid: friction }, resolved to the
		// ground body's PxMaterial. Surfaces not listed use defaultFriction.
		// The RID of each source body is recorded in the parallel
		// surface_pair_bodies table so invalidate_surface_pairs_for_body() can
		// drop entries when a ground body (and its material) is freed.
		const Dictionary pairs = p_params["surface_frictions"];
		physx::PxVehiclePhysXMaterialFriction *dst = v2->surface_pairs + p_idx * MAX_SURFACE_PAIRS;
		RID *dst_bodies = v2->surface_pair_bodies + p_idx * MAX_SURFACE_PAIRS;
		int surface_pair_count = 0;
		for (const KeyValue<Variant, Variant> &kv : pairs) {
			if (surface_pair_count >= (int)MAX_SURFACE_PAIRS) {
				break;
			}
			PhysXBody3D *ground = PhysXServer3D::get_singleton()->get_body(kv.key);
			ERR_CONTINUE_MSG(!ground, "PhysX: surface_frictions key is not a body RID.");
			dst[surface_pair_count].material = ground->get_shape_material();
			dst[surface_pair_count].friction = (float)(double)kv.value;
			dst_bodies[surface_pair_count] = ground->get_rid();
			surface_pair_count++;
		}
		// Stale RIDs beyond the live count must not resurface: a later call
		// rewrites 0..n-1 wholesale, but invalidate_surface_pairs_for_body()
		// matches on RID and would otherwise compare dead slots.
		for (int i = surface_pair_count; i < (int)MAX_SURFACE_PAIRS; i++) {
			dst_bodies[i] = RID();
		}
		v2->material_friction_params[p_idx].materialFrictions = dst;
		v2->material_friction_params[p_idx].nbMaterialFrictions = surface_pair_count;
	}

	if (p_params.has("steer")) {
		wf.steer = (bool)p_params["steer"];
	}
	if (p_params.has("traction")) {
		wf.traction = (bool)p_params["traction"];
	}
	if (p_params.has("brake")) {
		wf.brake = (bool)p_params["brake"];
	}
	if (p_params.has("front")) {
		wf.front = (bool)p_params["front"];
	}

	// Role flags may have changed -> re-seed the response multipliers.
	_update_response_params();
}

void PhysXVehicleServer::invalidate_surface_pairs_for_body(const PhysXBody3D *p_body) {
	if (!v2 || !p_body) {
		return;
	}
	const RID body_rid = p_body->get_rid();
	for (int w = 0; w < v2->nb_wheels; w++) {
		const int base = w * MAX_SURFACE_PAIRS;
		int nb = v2->material_friction_params[w].nbMaterialFrictions;
		int i = 0;
		while (i < nb) {
			if (v2->surface_pair_bodies[base + i] == body_rid) {
				// Compact the wheel's block, keeping order stable so the grip
				// table stays deterministic across a ground-body free.
				for (int j = i; j < nb - 1; j++) {
					v2->surface_pairs[base + j] = v2->surface_pairs[base + j + 1];
					v2->surface_pair_bodies[base + j] = v2->surface_pair_bodies[base + j + 1];
				}
				v2->surface_pair_bodies[base + nb - 1] = RID();
				nb--;
			} else {
				i++;
			}
		}
		v2->material_friction_params[w].nbMaterialFrictions = nb;
	}
}

// ============================================================================
// Control inputs + EngineDrive config
// ============================================================================

void PhysXVehicleServer::set_control_inputs(float p_throttle, float p_brake, float p_steer, float p_handbrake) {
	in_throttle = p_throttle;
	in_brake = p_brake;
	in_steer = p_steer;
	in_handbrake = p_handbrake;
	inputs_dirty = true;
}

void PhysXVehicleServer::set_gear_command(int p_gear) {
	in_target_gear = p_gear;
	inputs_dirty = true;
}

// Tuned command-response limits; a negative entry reverts that channel to the
// built-in default (see _update_response_params).
void PhysXVehicleServer::set_response_params(const Dictionary &p_params) {
	if (p_params.has("drive_torque")) {
		tune_drive_torque = (float)(double)p_params["drive_torque"];
	}
	if (p_params.has("max_steer_angle")) {
		tune_max_steer_angle = (float)(double)p_params["max_steer_angle"];
	}
	if (p_params.has("brake_torque")) {
		tune_brake_torque = (float)(double)p_params["brake_torque"];
	}
	if (p_params.has("handbrake_torque")) {
		tune_handbrake_torque = (float)(double)p_params["handbrake_torque"];
	}
	_update_response_params();
}

// Anti-roll bars: "wheel_ids" is a flattened list of wheel-index pairs and
// "stiffness" holds one stiffness per pair (unit: mass / time^2); an empty
// "stiffness" disables anti-roll. Like the other vehicle2 params, the
// configuration lives in the vehicle2 state and resets on rebuild.
void PhysXVehicleServer::set_anti_roll_params(const Dictionary &p_params) {
	if (!p_params.has("stiffness")) {
		return;
	}
	// Cache FIRST (VEH-2): the configuration must survive wheel-count rebuilds
	// like every other tuning dictionary, and buffering here also makes
	// pre-adopt calls (v2 == nullptr) take effect at adopt time instead of
	// being silently dropped.
	{
		// Deep-copy the arrays: the caller's Dictionary may mutate after return.
		Dictionary copy;
		copy["wheel_ids"] = p_params.get("wheel_ids", PackedInt32Array());
		copy["stiffness"] = p_params["stiffness"];
		cached_anti_roll_params = copy;
	}
	if (!v2) {
		return;
	}
	PackedFloat32Array stiffness = p_params["stiffness"];
	PackedInt32Array wheel_ids = p_params.get("wheel_ids", PackedInt32Array());
	int nb_bars = wheel_ids.size() / 2 < stiffness.size() ? wheel_ids.size() / 2 : stiffness.size();
	if (nb_bars > wheel_count) {
		nb_bars = wheel_count;
	}
	for (int i = 0; i < nb_bars; i++) {
		const int w0 = wheel_ids[i * 2];
		const int w1 = wheel_ids[i * 2 + 1];
		ERR_FAIL_INDEX_MSG(w0, wheel_count, "PhysX: anti-roll wheel id out of range.");
		ERR_FAIL_INDEX_MSG(w1, wheel_count, "PhysX: anti-roll wheel id out of range.");
		v2->anti_roll_params[i].wheel0 = (physx::PxU32)w0;
		v2->anti_roll_params[i].wheel1 = (physx::PxU32)w1;
		v2->anti_roll_params[i].stiffness = stiffness[i];
	}
	v2->nb_anti_roll_bars = nb_bars;
}

void PhysXVehicleServer::_apply_cached_anti_roll() {
	// Re-apply the cached anti-roll configuration to the (fresh) v2 state.
	// Called from _rebuild/adopt; a no-op without a cache or state.
	if (!v2 || cached_anti_roll_params.is_empty() || !cached_anti_roll_params.has("stiffness")) {
		return;
	}
	PackedFloat32Array stiffness = cached_anti_roll_params["stiffness"];
	PackedInt32Array wheel_ids = cached_anti_roll_params.get("wheel_ids", PackedInt32Array());
	int nb_bars = wheel_ids.size() / 2 < stiffness.size() ? wheel_ids.size() / 2 : stiffness.size();
	if (nb_bars > wheel_count) {
		nb_bars = wheel_count;
	}
	for (int i = 0; i < nb_bars; i++) {
		const int w0 = wheel_ids[i * 2];
		const int w1 = wheel_ids[i * 2 + 1];
		if (w0 < 0 || w0 >= wheel_count || w1 < 0 || w1 >= wheel_count) {
			continue;
		}
		v2->anti_roll_params[i].wheel0 = (physx::PxU32)w0;
		v2->anti_roll_params[i].wheel1 = (physx::PxU32)w1;
		v2->anti_roll_params[i].stiffness = stiffness[i];
	}
	v2->nb_anti_roll_bars = nb_bars;
}

// Live-state getters (mirror set_anti_roll_params / set_response_params):
// return what the vehicle2 state is ACTUALLY using right now, so tests can
// assert that configuration survived a rebuild rather than trusting caches.
Dictionary PhysXVehicleServer::get_anti_roll_params() const {
	Dictionary out;
	if (!v2) {
		out["nb_bars"] = 0;
		return out;
	}
	PackedInt32Array wheel_ids;
	PackedFloat32Array stiffness;
	for (int i = 0; i < v2->nb_anti_roll_bars; i++) {
		wheel_ids.push_back((int)v2->anti_roll_params[i].wheel0);
		wheel_ids.push_back((int)v2->anti_roll_params[i].wheel1);
		stiffness.push_back(v2->anti_roll_params[i].stiffness);
	}
	out["nb_bars"] = v2->nb_anti_roll_bars;
	out["wheel_ids"] = wheel_ids;
	out["stiffness"] = stiffness;
	return out;
}

Dictionary PhysXVehicleServer::get_response_params() const {
	Dictionary out;
	if (!v2) {
		return out;
	}
	out["drive_max_response"] = (double)v2->throttle_response_params.maxResponse;
	out["steer_max_response"] = (double)v2->steer_response_params.maxResponse;
	out["brake_max_response"] = (double)v2->brake_response_params[0].maxResponse;
	out["handbrake_max_response"] = (double)v2->brake_response_params[1].maxResponse;
	return out;
}

// Ackermann steering geometry. "percent" blends between parallel steer (0) and
// pure Ackermann (100); "wheelbase"/"track" override the auto-derived geometry
// (negative = derive from the wheel suspension attachments).
void PhysXVehicleServer::set_ackermann_params(const Dictionary &p_params) {
	if (p_params.has("enabled")) {
		ackermann_enabled = (bool)p_params["enabled"];
	}
	if (p_params.has("percent")) {
		ackermann_percent = (float)(double)p_params["percent"];
	}
	if (p_params.has("wheelbase")) {
		tune_ackermann_wheelbase = (float)(double)p_params["wheelbase"];
	}
	if (p_params.has("track")) {
		tune_ackermann_track = (float)(double)p_params["track"];
	}
	inputs_dirty = true;
}

PackedFloat32Array PhysXVehicleServer::get_wheel_steer_angles() const {
	PackedFloat32Array out;
	out.resize(wheel_count);
	for (int i = 0; i < wheel_count; i++) {
		out.write[i] = (real_t)computed_steer_angles[i];
	}
	return out;
}

// 2-wheeler roll-balance assist (see _update_balance for the control law).
void PhysXVehicleServer::set_balance_params(const Dictionary &p_params) {
	if (p_params.has("enabled")) {
		balance_enabled = (bool)p_params["enabled"];
	}
	if (p_params.has("kp")) {
		balance_kp = MAX((float)(double)p_params["kp"], 0.0f);
	}
	if (p_params.has("kd")) {
		balance_kd = MAX((float)(double)p_params["kd"], 0.0f);
	}
	if (p_params.has("max_steer_assist")) {
		balance_max_steer_assist = MAX((float)(double)p_params["max_steer_assist"], 0.0f);
	}
	if (p_params.has("low_speed_torque")) {
		balance_low_speed_torque = MAX((float)(double)p_params["low_speed_torque"], 0.0f);
	}
	inputs_dirty = true;
}

Dictionary PhysXVehicleServer::get_balance_state() const {
	Dictionary state;
	state["lean_angle"] = (real_t)balance_lean_angle;
	state["roll_rate"] = (real_t)balance_roll_rate;
	state["steer_assist"] = (real_t)balance_steer_assist;
	return state;
}

void PhysXVehicleServer::set_wheel_drive_torque(int p_idx, float p_torque) {
	if (p_idx < 0 || p_idx >= wheel_count) {
		return;
	}
	in_wheel_drive_torque[p_idx] = p_torque;
	inputs_dirty = true;
}

void PhysXVehicleServer::set_wheel_brake_torque(int p_idx, float p_torque) {
	if (p_idx < 0 || p_idx >= wheel_count) {
		return;
	}
	in_wheel_brake_torque[p_idx] = p_torque;
	inputs_dirty = true;
}

void PhysXVehicleServer::set_wheel_steer_angle(int p_idx, float p_angle) {
	if (p_idx < 0 || p_idx >= wheel_count) {
		return;
	}
	in_wheel_steer_angle[p_idx] = p_angle;
	inputs_dirty = true;
}

void PhysXVehicleServer::set_engine_params(const Dictionary &p_params) {
	if (archetype != ARCHETYPE_ENGINE_DRIVE) {
		return;
	}
	cached_engine_params = p_params;
	if (!v2) {
		return; // pre-adopt calls replay from the cache at adopt
	}
	if (p_params.has("moi")) {
		v2->engine_params.moi = (float)(double)p_params["moi"];
	}
	if (p_params.has("peak_torque")) {
		v2->engine_params.peakTorque = (float)(double)p_params["peak_torque"];
	}
	if (p_params.has("idle_omega")) {
		v2->engine_params.idleOmega = (float)(double)p_params["idle_omega"];
	}
	if (p_params.has("max_omega")) {
		v2->engine_params.maxOmega = (float)(double)p_params["max_omega"];
	}
	if (p_params.has("damping_full_throttle")) {
		v2->engine_params.dampingRateFullThrottle = (float)(double)p_params["damping_full_throttle"];
	}
	if (p_params.has("damping_zero_throttle_clutch_engaged")) {
		v2->engine_params.dampingRateZeroThrottleClutchEngaged = (float)(double)p_params["damping_zero_throttle_clutch_engaged"];
	}
	if (p_params.has("damping_zero_throttle_clutch_disengaged")) {
		v2->engine_params.dampingRateZeroThrottleClutchDisengaged = (float)(double)p_params["damping_zero_throttle_clutch_disengaged"];
	}
	if (p_params.has("torque_curve")) {
		// Interleaved [x0,y0, x1,y1, ...] of (normalized omega, normalized torque).
		v2->engine_params.torqueCurve.clear();
		PackedFloat32Array pts = p_params["torque_curve"];
		const float *r = pts.ptr();
		const int curve_point_count = pts.size() / 2;
		for (int i = 0; i < curve_point_count; i++) {
			v2->engine_params.torqueCurve.addPair(r[i * 2], r[i * 2 + 1]);
		}
	}
}

void PhysXVehicleServer::set_clutch_params(const Dictionary &p_params) {
	if (archetype != ARCHETYPE_ENGINE_DRIVE) {
		return;
	}
	cached_clutch_params = p_params;
	if (!v2) {
		return; // pre-adopt calls replay from the cache at adopt
	}
	if (p_params.has("max_response")) {
		v2->clutch_response_params.maxResponse = (float)(double)p_params["max_response"];
	}
	if (p_params.has("estimate_iterations")) {
		v2->clutch_params.estimateIterations = (int)p_params["estimate_iterations"];
	}
	if (p_params.has("accuracy_mode")) {
		v2->clutch_params.accuracyMode = ((int)p_params["accuracy_mode"] == 1)
				? physx::PxVehicleClutchAccuracyMode::eBEST_POSSIBLE
				: physx::PxVehicleClutchAccuracyMode::eESTIMATE;
	}
}

void PhysXVehicleServer::set_gearbox_params(const Dictionary &p_params) {
	if (archetype != ARCHETYPE_ENGINE_DRIVE) {
		return;
	}
	cached_gearbox_params = p_params;
	if (!v2) {
		return; // pre-adopt calls replay from the cache at adopt
	}
	if (p_params.has("neutral_gear")) {
		v2->gearbox_params.neutralGear = (int)p_params["neutral_gear"];
	}
	if (p_params.has("final_ratio")) {
		v2->gearbox_params.finalRatio = (float)(double)p_params["final_ratio"];
	}
	if (p_params.has("switch_time")) {
		v2->gearbox_params.switchTime = (float)(double)p_params["switch_time"];
	}
	if (p_params.has("ratios")) {
		PackedFloat32Array ratios = p_params["ratios"];
		const float *r = ratios.ptr();
		int gear_ratio_count = ratios.size();
		const int cap = (int)physx::PxVehicleGearboxParams::eMAX_NB_GEARS;
		if (gear_ratio_count > cap) {
			gear_ratio_count = cap;
		}
		for (int i = 0; i < gear_ratio_count; i++) {
			v2->gearbox_params.ratios[i] = r[i];
		}
		v2->gearbox_params.nbRatios = gear_ratio_count;
	}
}

void PhysXVehicleServer::set_autobox_params(const Dictionary &p_params) {
	if (archetype != ARCHETYPE_ENGINE_DRIVE) {
		return;
	}
	cached_autobox_params = p_params;
	if (!v2) {
		return; // pre-adopt calls replay from the cache at adopt
	}
	const int cap = (int)physx::PxVehicleGearboxParams::eMAX_NB_GEARS;
	if (p_params.has("up_ratio")) {
		const float up_shift_ratio = (float)(double)p_params["up_ratio"];
		for (int i = 0; i < cap; i++) {
			v2->autobox_params.upRatios[i] = up_shift_ratio;
		}
	}
	if (p_params.has("down_ratio")) {
		const float up_shift_ratio = (float)(double)p_params["down_ratio"];
		for (int i = 0; i < cap; i++) {
			v2->autobox_params.downRatios[i] = up_shift_ratio;
		}
	}
	if (p_params.has("latency")) {
		v2->autobox_params.latency = (float)(double)p_params["latency"];
	}
}

void PhysXVehicleServer::set_differential_params(const Dictionary &p_params) {
	if (archetype != ARCHETYPE_ENGINE_DRIVE) {
		return;
	}
	cached_differential_params = p_params;
	if (!v2) {
		return; // pre-adopt calls replay from the cache at adopt
	}
	// Optional 4-wheel limited-slip biases/targets (0 = open). The per-wheel
	// drive split (RWD/FWD/6x4) is handled in _update_response_params() from
	// WheelFlags.traction, independent of these biases.
	if (p_params.has("front_bias")) {
		v2->diff_params.frontBias = (float)(double)p_params["front_bias"];
	}
	if (p_params.has("front_target")) {
		v2->diff_params.frontTarget = (float)(double)p_params["front_target"];
	}
	if (p_params.has("rear_bias")) {
		v2->diff_params.rearBias = (float)(double)p_params["rear_bias"];
	}
	if (p_params.has("rear_target")) {
		v2->diff_params.rearTarget = (float)(double)p_params["rear_target"];
	}
	if (p_params.has("center_bias")) {
		v2->diff_params.centerBias = (float)(double)p_params["center_bias"];
	}
	if (p_params.has("center_target")) {
		v2->diff_params.centerTarget = (float)(double)p_params["center_target"];
	}
	if (p_params.has("rate")) {
		v2->diff_params.rate = (float)(double)p_params["rate"];
	}
	if (p_params.has("front_wheel_ids")) {
		PackedInt32Array ids = p_params["front_wheel_ids"];
		const int32_t *wheel_ids_ptr = ids.ptr();
		const int driven_wheel_count = ids.size() < 2 ? ids.size() : 2;
		for (int i = 0; i < driven_wheel_count; i++) {
			v2->diff_params.frontWheelIds[i] = (physx::PxU32)wheel_ids_ptr[i];
		}
	}
	if (p_params.has("rear_wheel_ids")) {
		PackedInt32Array ids = p_params["rear_wheel_ids"];
		const int32_t *wheel_ids_ptr = ids.ptr();
		const int driven_wheel_count = ids.size() < 2 ? ids.size() : 2;
		for (int i = 0; i < driven_wheel_count; i++) {
			v2->diff_params.rearWheelIds[i] = (physx::PxU32)wheel_ids_ptr[i];
		}
	}
}

// ============================================================================
// Utility
// ============================================================================

RID PhysXVehicleServer::resolve_contact_body(const physx::PxRigidActor *p_hit) const {
	if (!p_hit) {
		return RID();
	}
	// The hit actor's userData is a PhysXActorUserData (set by every body/area
	// the module creates). Resolve it back to the Godot RID.
	const PhysXActorUserData *ud = static_cast<const PhysXActorUserData *>(p_hit->userData);
	if (!ud || !ud->object) {
		return RID();
	}
	return ud->rid;
}

PhysXVehicleServer::Archetype PhysXVehicleServer::int_to_archetype(int p_kind) {
	return (p_kind == 1) ? ARCHETYPE_ENGINE_DRIVE : ARCHETYPE_DIRECT_DRIVE;
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

PhysXVehicleServer::PhysXVehicleServer() = default;

PhysXVehicleServer::~PhysXVehicleServer() {
	release();
}
