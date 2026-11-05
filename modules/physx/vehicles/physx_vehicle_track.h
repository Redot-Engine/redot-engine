/**************************************************************************/
/*  physx_vehicle_track.h                                                 */
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

#include "../physx_conversions.h"
#include "physx_wheel_query_filter.h"

#include "core/error/error_macros.h"
#include "core/math/basis.h"
#include "core/math/vector3.h"
#include "core/typedefs.h"

#include <PxPhysicsAPI.h>
#include <vehicle/PxVehicleAPI.h>

#include "physx_vehicle_shared.h" // frame/gravity/clamp conventions shared with the server stack

using namespace physx;

// A direct-drive N-wheel tracked (tank-style) vehicle: no steering wheels at
// all, driven entirely by skid-steer (independent left/right track speed).
// Neither Vehicle4W nor Vehicle2W can represent this: both assume a steering
// axle, and both are fixed at a specific wheel count. PxVehicle2 itself has
// no track/tank primitive whatsoever (confirmed: zero track/tank-related
// types anywhere in the whole vehicle/ SDK header tree -- PhysX 5 dropped
// the old PhysX-3.4-era PxVehicleDriveTank class entirely when it moved to
// this component-based system). Jolt, by contrast, ships a real, tested
// TrackedVehicleController (thirdparty/jolt_physics/Jolt/Physics/Vehicle/
// TrackedVehicleController.h) -- but wrapping that would mean new code in
// modules/jolt_physics, breaking this project's standing "vanilla engine +
// one extra module" line, so this is a from-scratch composition entirely
// inside godot_physx instead, built from the same raw per-wheel suspension/
// tire/drivetrain components Vehicle4W/Vehicle2W already use.
//
// Skid-steer mechanism: PxVehicle2's own per-wheel throttle response
// multiplier (already used by Vehicle2W/Vehicle4W to route drive torque to
// specific wheels) is set PER TICK here, not once at configure time -- each
// wheel's multiplier IS the signed drive command for its own track
// (left_ratio/right_ratio, each in [-1, 1], sign = direction, magnitude =
// speed), with commandState.throttle itself held at a constant 1.0. This
// sidesteps PxVehicleDirectDriveTransmissionCommandState's single
// vehicle-wide forward/reverse gear entirely -- a real pivot turn needs the
// two tracks spinning in OPPOSITE directions at the same time, which a
// shared gear can't express, but two independently-signed per-wheel
// multipliers can.
//
// PxVehicleLimits.h's own eMAX_NB_WHEELS is 20; MAX_WHEELS here is a smaller
// practical cap comfortably covering a real tank's road-wheel count (up to
// 8 per side / 16 total) while keeping every per-wheel array a plain fixed
// C array, matching Vehicle2W/Vehicle4W's own convention.
class VehicleTrack : public PxVehicleRigidBodyComponent,
					 public PxVehicleSuspensionComponent,
					 public PxVehicleTireComponent,
					 public PxVehicleWheelComponent,
					 public PxVehiclePhysXActorBeginComponent,
					 public PxVehiclePhysXActorEndComponent,
					 public PxVehiclePhysXConstraintComponent,
					 public PxVehiclePhysXRoadGeometrySceneQueryComponent,
					 public PxVehicleDirectDriveCommandResponseComponent,
					 public PxVehicleDirectDriveActuationStateComponent,
					 public PxVehicleDirectDrivetrainComponent {
public:
	static constexpr PxU32 MAX_WHEELS = 16;

	PxU32 numWheels = 0;
	// True = this wheel is on the left track, false = right. Populated by
	// configure_vehicle_track() from each wheel's local X sign -- see that
	// function's own doc comment.
	bool wheelIsLeftTrack[MAX_WHEELS] = {};
	// How many wheels share each track -- setDriverInput() divides by this so
	// maxEngineTorque means a real per-TRACK torque budget shared across
	// every wheel on it, not (maxResponse * multiplier) applied
	// INDEPENDENTLY at every wheel. Found via a real repro: with all wheels
	// at multiplier +-1.0 simultaneously, an 8-wheel tank hit ~36 m/s in 2.5
	// seconds and fell through the floor during a pivot -- roughly 8x the
	// intended drive torque, not the configured per-vehicle max.
	PxU32 numLeftWheels = 0;
	PxU32 numRightWheels = 0;

	// --- Base (BaseVehicleParams/State) ------------------------------------
	PxVehicleAxleDescription axleDescription;
	PxVehicleFrame frame;
	PxVehicleScale scale;
	PxVehicleSuspensionStateCalculationParams suspensionStateCalculationParams;
	PxVehicleBrakeCommandResponseParams brakeResponseParams[1];
	PxVehicleSteerCommandResponseParams steerResponseParams;
	PxVehicleSuspensionParams suspensionParams[MAX_WHEELS];
	PxVehicleSuspensionComplianceParams suspensionComplianceParams[MAX_WHEELS];
	PxVehicleSuspensionForceParams suspensionForceParams[MAX_WHEELS];
	PxVehicleTireForceParams tireForceParams[MAX_WHEELS];
	PxVehicleWheelParams wheelParams[MAX_WHEELS];
	PxVehicleRigidBodyParams rigidBodyParams;

	PxReal brakeCommandResponseStates[MAX_WHEELS] = {};
	PxReal steerCommandResponseStates[MAX_WHEELS] = {};
	PxVehicleWheelActuationState actuationStates[MAX_WHEELS];
	PxVehicleRoadGeometryState roadGeomStates[MAX_WHEELS];
	PxVehicleSuspensionState suspensionStates[MAX_WHEELS];
	PxVehicleSuspensionComplianceState suspensionComplianceStates[MAX_WHEELS];
	PxVehicleSuspensionForce suspensionForces[MAX_WHEELS];
	PxVehicleTireGripState tireGripStates[MAX_WHEELS];
	PxVehicleTireDirectionState tireDirectionStates[MAX_WHEELS];
	PxVehicleTireSpeedState tireSpeedStates[MAX_WHEELS];
	PxVehicleTireSlipState tireSlipStates[MAX_WHEELS];
	PxVehicleTireCamberAngleState tireCamberAngleStates[MAX_WHEELS];
	PxVehicleTireStickyState tireStickyStates[MAX_WHEELS];
	PxVehicleTireForce tireForces[MAX_WHEELS];
	PxVehicleWheelRigidBody1dState wheelRigidBody1dStates[MAX_WHEELS];
	PxVehicleWheelLocalPose wheelLocalPoses[MAX_WHEELS];
	PxVehicleRigidBodyState rigidBodyState;

	// --- Direct drive (DirectDrivetrainParams/State) -----------------------
	PxVehicleDirectDriveThrottleCommandResponseParams directDriveThrottleResponseParams;
	PxReal directDriveThrottleResponseStates[MAX_WHEELS] = {};
	PxVehicleCommandState commandState;
	PxVehicleDirectDriveTransmissionCommandState transmissionCommandState;

	// --- PhysX integration (PhysXIntegrationParams/State) ------------------
	PxVehiclePhysXRoadGeometryQueryParams physxRoadGeometryQueryParams;
	// The road-geometry query params' filterCallback points here, so it must
	// live as long as the vehicle composition.
	PhysXWheelQueryFilter wheel_query_filter;
	PxVehiclePhysXMaterialFrictionParams physxMaterialFrictionParams[MAX_WHEELS];
	PxVehiclePhysXSuspensionLimitConstraintParams physxSuspensionLimitConstraintParams[MAX_WHEELS];
	PxTransform physxActorCMassLocalPose;
	PxVec3 physxActorBoxShapeHalfExtents;
	PxTransform physxActorBoxShapeLocalPose;
	PxTransform physxWheelShapeLocalPoses[MAX_WHEELS];
	PxVehiclePhysXActor physxActor;
	PxVehiclePhysXSteerState physxSteerState;
	PxVehiclePhysXConstraints physxConstraints;

	PxVehicleComponentSequence componentSequence;
	PxU8 componentSequenceSubstepGroupHandle = 0;

	void setToDefault() {
		for (PxU32 i = 0; i < numWheels; i++) {
			actuationStates[i].setToDefault();
			roadGeomStates[i].setToDefault();
			suspensionStates[i].setToDefault();
			suspensionComplianceStates[i].setToDefault();
			suspensionForces[i].setToDefault();
			tireGripStates[i].setToDefault();
			tireDirectionStates[i].setToDefault();
			tireSpeedStates[i].setToDefault();
			tireSlipStates[i].setToDefault();
			tireCamberAngleStates[i].setToDefault();
			tireStickyStates[i].setToDefault();
			tireForces[i].setToDefault();
			wheelRigidBody1dStates[i].setToDefault();
			wheelLocalPoses[i].setToDefault();
		}
		rigidBodyState.setToDefault();
		commandState.setToDefault();
		transmissionCommandState.gear = PxVehicleDirectDriveTransmissionCommandState::eNEUTRAL;
		physxActor.setToDefault();
		physxSteerState.setToDefault();
		physxConstraints.setToDefault();
	}

	// getDataForRigidBodyComponent (PxVehicleRigidBodyComponent)
	virtual void getDataForRigidBodyComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehicleRigidBodyParams *&outRigidBodyParams,
			PxVehicleArrayData<const PxVehicleSuspensionForce> &outSuspensionForces,
			PxVehicleArrayData<const PxVehicleTireForce> &outTireForces,
			const PxVehicleAntiRollTorque *&outAntiRollTorque,
			PxVehicleRigidBodyState *&outRigidBodyState) override {
		outAxleDescription = &axleDescription;
		outRigidBodyParams = &rigidBodyParams;
		outSuspensionForces.setData(suspensionForces);
		outTireForces.setData(tireForces);
		outAntiRollTorque = nullptr; // no anti-roll bar -- one wheel per axle
		outRigidBodyState = &rigidBodyState;
	}

	// getDataForSuspensionComponent (PxVehicleSuspensionComponent)
	virtual void getDataForSuspensionComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehicleRigidBodyParams *&outRigidBodyParams,
			const PxVehicleSuspensionStateCalculationParams *&outSuspensionStateCalculationParams,
			PxVehicleArrayData<const PxReal> &outSteerResponseStates,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxVehicleSuspensionParams> &outSuspensionParams,
			PxVehicleArrayData<const PxVehicleSuspensionComplianceParams> &outSuspensionComplianceParams,
			PxVehicleArrayData<const PxVehicleSuspensionForceParams> &outSuspensionForceParams,
			PxVehicleSizedArrayData<const PxVehicleAntiRollForceParams> &outAntiRollForceParams,
			PxVehicleArrayData<const PxVehicleRoadGeometryState> &outWheelRoadGeomStates,
			PxVehicleArrayData<PxVehicleSuspensionState> &outSuspensionStates,
			PxVehicleArrayData<PxVehicleSuspensionComplianceState> &outSuspensionComplianceStates,
			PxVehicleArrayData<PxVehicleSuspensionForce> &outSuspensionForces,
			PxVehicleAntiRollTorque *&outAntiRollTorque) override {
		outAxleDescription = &axleDescription;
		outRigidBodyParams = &rigidBodyParams;
		outSuspensionStateCalculationParams = &suspensionStateCalculationParams;
		outSteerResponseStates.setData(steerCommandResponseStates);
		outRigidBodyState = &rigidBodyState;
		outWheelParams.setData(wheelParams);
		outSuspensionParams.setData(suspensionParams);
		outSuspensionComplianceParams.setData(suspensionComplianceParams);
		outSuspensionForceParams.setData(suspensionForceParams);
		outAntiRollForceParams.setEmpty();
		outWheelRoadGeomStates.setData(roadGeomStates);
		outSuspensionStates.setData(suspensionStates);
		outSuspensionComplianceStates.setData(suspensionComplianceStates);
		outSuspensionForces.setData(suspensionForces);
		outAntiRollTorque = nullptr;
	}

	// getDataForTireComponent (PxVehicleTireComponent)
	virtual void getDataForTireComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			PxVehicleArrayData<const PxReal> &outSteerResponseStates,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<const PxVehicleWheelActuationState> &outActuationStates,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxVehicleSuspensionParams> &outSuspensionParams,
			PxVehicleArrayData<const PxVehicleTireForceParams> &outTireForceParams,
			PxVehicleArrayData<const PxVehicleRoadGeometryState> &outRoadGeomStates,
			PxVehicleArrayData<const PxVehicleSuspensionState> &outSuspensionStates,
			PxVehicleArrayData<const PxVehicleSuspensionComplianceState> &outSuspensionComplianceStates,
			PxVehicleArrayData<const PxVehicleSuspensionForce> &outSuspensionForces,
			PxVehicleArrayData<const PxVehicleWheelRigidBody1dState> &outWheelRigidBody1DStates,
			PxVehicleArrayData<PxVehicleTireGripState> &outTireGripStates,
			PxVehicleArrayData<PxVehicleTireDirectionState> &outTireDirectionStates,
			PxVehicleArrayData<PxVehicleTireSpeedState> &outTireSpeedStates,
			PxVehicleArrayData<PxVehicleTireSlipState> &outTireSlipStates,
			PxVehicleArrayData<PxVehicleTireCamberAngleState> &outTireCamberAngleStates,
			PxVehicleArrayData<PxVehicleTireStickyState> &outTireStickyStates,
			PxVehicleArrayData<PxVehicleTireForce> &outTireForces) override {
		outAxleDescription = &axleDescription;
		outSteerResponseStates.setData(steerCommandResponseStates);
		outRigidBodyState = &rigidBodyState;
		outActuationStates.setData(actuationStates);
		outWheelParams.setData(wheelParams);
		outSuspensionParams.setData(suspensionParams);
		outTireForceParams.setData(tireForceParams);
		outRoadGeomStates.setData(roadGeomStates);
		outSuspensionStates.setData(suspensionStates);
		outSuspensionComplianceStates.setData(suspensionComplianceStates);
		outSuspensionForces.setData(suspensionForces);
		outWheelRigidBody1DStates.setData(wheelRigidBody1dStates);
		outTireGripStates.setData(tireGripStates);
		outTireDirectionStates.setData(tireDirectionStates);
		outTireSpeedStates.setData(tireSpeedStates);
		outTireSlipStates.setData(tireSlipStates);
		outTireCamberAngleStates.setData(tireCamberAngleStates);
		outTireStickyStates.setData(tireStickyStates);
		outTireForces.setData(tireForces);
	}

	// getDataForWheelComponent (PxVehicleWheelComponent)
	virtual void getDataForWheelComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			PxVehicleArrayData<const PxReal> &outSteerResponseStates,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxVehicleSuspensionParams> &outSuspensionParams,
			PxVehicleArrayData<const PxVehicleWheelActuationState> &outActuationStates,
			PxVehicleArrayData<const PxVehicleSuspensionState> &outSuspensionStates,
			PxVehicleArrayData<const PxVehicleSuspensionComplianceState> &outSuspensionComplianceStates,
			PxVehicleArrayData<const PxVehicleTireSpeedState> &outTireSpeedStates,
			PxVehicleArrayData<PxVehicleWheelRigidBody1dState> &outWheelRigidBody1dStates,
			PxVehicleArrayData<PxVehicleWheelLocalPose> &outWheelLocalPoses) override {
		outAxleDescription = &axleDescription;
		outSteerResponseStates.setData(steerCommandResponseStates);
		outWheelParams.setData(wheelParams);
		outSuspensionParams.setData(suspensionParams);
		outActuationStates.setData(actuationStates);
		outSuspensionStates.setData(suspensionStates);
		outSuspensionComplianceStates.setData(suspensionComplianceStates);
		outTireSpeedStates.setData(tireSpeedStates);
		outWheelRigidBody1dStates.setData(wheelRigidBody1dStates);
		outWheelLocalPoses.setData(wheelLocalPoses);
	}

	// getDataForPhysXActorBeginComponent (PxVehiclePhysXActorBeginComponent)
	virtual void getDataForPhysXActorBeginComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehicleCommandState *&outCommands,
			const PxVehicleEngineDriveTransmissionCommandState *&outTransmissionCommands,
			const PxVehicleGearboxParams *&outGearParams,
			const PxVehicleGearboxState *&outGearState,
			const PxVehicleEngineParams *&outEngineParams,
			PxVehiclePhysXActor *&outPhysxActor,
			PxVehiclePhysXSteerState *&outPhysxSteerState,
			PxVehiclePhysXConstraints *&outPhysxConstraints,
			PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<PxVehicleWheelRigidBody1dState> &outWheelRigidBody1dStates,
			PxVehicleEngineState *&outEngineState) override {
		outAxleDescription = &axleDescription;
		outCommands = &commandState;
		outPhysxActor = &physxActor;
		outPhysxSteerState = &physxSteerState;
		outPhysxConstraints = &physxConstraints;
		outRigidBodyState = &rigidBodyState;
		outWheelRigidBody1dStates.setData(wheelRigidBody1dStates);
		outTransmissionCommands = nullptr;
		outGearParams = nullptr;
		outGearState = nullptr;
		outEngineParams = nullptr;
		outEngineState = nullptr;
	}

	// getDataForPhysXActorEndComponent (PxVehiclePhysXActorEndComponent)
	virtual void getDataForPhysXActorEndComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxTransform> &outWheelShapeLocalPoses,
			PxVehicleArrayData<const PxVehicleWheelRigidBody1dState> &outWheelRigidBody1dStates,
			PxVehicleArrayData<const PxVehicleWheelLocalPose> &outWheelLocalPoses,
			const PxVehicleGearboxState *&outGearState,
			const PxReal *&outThrottle,
			PxVehiclePhysXActor *&outPhysxActor) override {
		outAxleDescription = &axleDescription;
		outRigidBodyState = &rigidBodyState;
		outWheelParams.setData(wheelParams);
		outWheelShapeLocalPoses.setData(physxWheelShapeLocalPoses);
		outWheelRigidBody1dStates.setData(wheelRigidBody1dStates);
		outWheelLocalPoses.setData(wheelLocalPoses);
		outPhysxActor = &physxActor;
		outGearState = nullptr;
		outThrottle = &commandState.throttle;
	}

	// getDataForPhysXConstraintComponent (PxVehiclePhysXConstraintComponent)
	virtual void getDataForPhysXConstraintComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<const PxVehicleSuspensionParams> &outSuspensionParams,
			PxVehicleArrayData<const PxVehiclePhysXSuspensionLimitConstraintParams> &outSuspensionLimitParams,
			PxVehicleArrayData<const PxVehicleSuspensionState> &outSuspensionStates,
			PxVehicleArrayData<const PxVehicleSuspensionComplianceState> &outSuspensionComplianceStates,
			PxVehicleArrayData<const PxVehicleRoadGeometryState> &outWheelRoadGeomStates,
			PxVehicleArrayData<const PxVehicleTireDirectionState> &outTireDirectionStates,
			PxVehicleArrayData<const PxVehicleTireStickyState> &outTireStickyStates,
			PxVehiclePhysXConstraints *&outConstraints) override {
		outAxleDescription = &axleDescription;
		outRigidBodyState = &rigidBodyState;
		outSuspensionParams.setData(suspensionParams);
		outSuspensionLimitParams.setData(physxSuspensionLimitConstraintParams);
		outSuspensionStates.setData(suspensionStates);
		outSuspensionComplianceStates.setData(suspensionComplianceStates);
		outWheelRoadGeomStates.setData(roadGeomStates);
		outTireDirectionStates.setData(tireDirectionStates);
		outTireStickyStates.setData(tireStickyStates);
		outConstraints = &physxConstraints;
	}

	// getDataForPhysXRoadGeometrySceneQueryComponent (PxVehiclePhysXRoadGeometrySceneQueryComponent)
	virtual void getDataForPhysXRoadGeometrySceneQueryComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			const PxVehiclePhysXRoadGeometryQueryParams *&outRoadGeomParams,
			PxVehicleArrayData<const PxReal> &outSteerResponseStates,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxVehicleSuspensionParams> &outSuspensionParams,
			PxVehicleArrayData<const PxVehiclePhysXMaterialFrictionParams> &outMaterialFrictionParams,
			PxVehicleArrayData<PxVehicleRoadGeometryState> &outRoadGeometryStates,
			PxVehicleArrayData<PxVehiclePhysXRoadGeometryQueryState> &outPhysxRoadGeometryStates) override {
		outAxleDescription = &axleDescription;
		outRoadGeomParams = &physxRoadGeometryQueryParams;
		outSteerResponseStates.setData(steerCommandResponseStates);
		outRigidBodyState = &rigidBodyState;
		outWheelParams.setData(wheelParams);
		outSuspensionParams.setData(suspensionParams);
		outMaterialFrictionParams.setData(physxMaterialFrictionParams);
		outRoadGeometryStates.setData(roadGeomStates);
		outPhysxRoadGeometryStates.setEmpty();
	}

	// getDataForDirectDriveCommandResponseComponent (PxVehicleDirectDriveCommandResponseComponent)
	virtual void getDataForDirectDriveCommandResponseComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			PxVehicleSizedArrayData<const PxVehicleBrakeCommandResponseParams> &outBrakeResponseParams,
			const PxVehicleDirectDriveThrottleCommandResponseParams *&outThrottleResponseParams,
			const PxVehicleSteerCommandResponseParams *&outSteerResponseParams,
			PxVehicleSizedArrayData<const PxVehicleAckermannParams> &outAckermannParams,
			const PxVehicleCommandState *&outCommands,
			const PxVehicleDirectDriveTransmissionCommandState *&outTransmissionCommands,
			const PxVehicleRigidBodyState *&outRigidBodyState,
			PxVehicleArrayData<PxReal> &outBrakeResponseStates,
			PxVehicleArrayData<PxReal> &outThrottleResponseStates,
			PxVehicleArrayData<PxReal> &outSteerResponseStates) override {
		outAxleDescription = &axleDescription;
		outBrakeResponseParams.setDataAndCount(brakeResponseParams, 1);
		outThrottleResponseParams = &directDriveThrottleResponseParams;
		outSteerResponseParams = &steerResponseParams;
		outAckermannParams.setEmpty(); // no steering wheels at all
		outCommands = &commandState;
		outTransmissionCommands = &transmissionCommandState;
		outRigidBodyState = &rigidBodyState;
		outBrakeResponseStates.setData(brakeCommandResponseStates);
		outThrottleResponseStates.setData(directDriveThrottleResponseStates);
		outSteerResponseStates.setData(steerCommandResponseStates);
	}

	// getDataForDirectDriveActuationStateComponent (PxVehicleDirectDriveActuationStateComponent)
	virtual void getDataForDirectDriveActuationStateComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			PxVehicleArrayData<const PxReal> &outBrakeResponseStates,
			PxVehicleArrayData<const PxReal> &outThrottleResponseStates,
			PxVehicleArrayData<PxVehicleWheelActuationState> &outActuationStates) override {
		outAxleDescription = &axleDescription;
		outBrakeResponseStates.setData(brakeCommandResponseStates);
		outThrottleResponseStates.setData(directDriveThrottleResponseStates);
		outActuationStates.setData(actuationStates);
	}

	// getDataForDirectDrivetrainComponent (PxVehicleDirectDrivetrainComponent)
	virtual void getDataForDirectDrivetrainComponent(
			const PxVehicleAxleDescription *&outAxleDescription,
			PxVehicleArrayData<const PxReal> &outBrakeResponseStates,
			PxVehicleArrayData<const PxReal> &outThrottleResponseStates,
			PxVehicleArrayData<const PxVehicleWheelParams> &outWheelParams,
			PxVehicleArrayData<const PxVehicleWheelActuationState> &outActuationStates,
			PxVehicleArrayData<const PxVehicleTireForce> &outTireForces,
			PxVehicleArrayData<PxVehicleWheelRigidBody1dState> &outWheelRigidBody1dStates) override {
		outAxleDescription = &axleDescription;
		outBrakeResponseStates.setData(brakeCommandResponseStates);
		outThrottleResponseStates.setData(directDriveThrottleResponseStates);
		outWheelParams.setData(wheelParams);
		outActuationStates.setData(actuationStates);
		outTireForces.setData(tireForces);
		outWheelRigidBody1dStates.setData(wheelRigidBody1dStates);
	}

	void initComponentSequence() {
		componentSequence.add(static_cast<PxVehiclePhysXActorBeginComponent *>(this));
		componentSequence.add(static_cast<PxVehicleDirectDriveCommandResponseComponent *>(this));
		componentSequence.add(static_cast<PxVehicleDirectDriveActuationStateComponent *>(this));
		componentSequence.add(static_cast<PxVehiclePhysXRoadGeometrySceneQueryComponent *>(this));

		componentSequenceSubstepGroupHandle = componentSequence.beginSubstepGroup(3);
		componentSequence.add(static_cast<PxVehicleSuspensionComponent *>(this));
		componentSequence.add(static_cast<PxVehicleTireComponent *>(this));
		componentSequence.add(static_cast<PxVehiclePhysXConstraintComponent *>(this));
		componentSequence.add(static_cast<PxVehicleDirectDrivetrainComponent *>(this));
		componentSequence.add(static_cast<PxVehicleRigidBodyComponent *>(this));
		componentSequence.endSubstepGroup();

		componentSequence.add(static_cast<PxVehicleWheelComponent *>(this));
		componentSequence.add(static_cast<PxVehiclePhysXActorEndComponent *>(this));
	}

	// Skid-steer drive command -- see this class's own doc comment for the
	// mechanism. left_ratio/right_ratio are each [-1, 1]: sign is direction,
	// magnitude is speed. brake is [0, 1], shared by both tracks (same
	// convention Jolt's own TrackedVehicleController::SetDriverInput uses).
	// Must be called once per tick BEFORE step() -- it writes the throttle
	// response params step() itself reads.
	void setDriverInput(PxReal left_ratio, PxReal right_ratio, PxReal brake) {
		commandState.throttle = 1.0f;
		commandState.brakes[0] = brake;
		commandState.nbBrakes = 1;
		// Divided by that track's own wheel count -- see numLeftWheels/
		// numRightWheels's own doc comment for why: maxEngineTorque is a
		// per-TRACK budget shared across every wheel on it, not applied in
		// full at each wheel independently.
		// Positive ratios drive -Z (nose-first): with the right-handed frame
		// the direct drive rolls -Z natively, so no sign flip here (the old
		// negation compensated the left-handed frame; it is gone).
		const PxReal leftPerWheel = numLeftWheels > 0 ? physx_vehicle_clamp_sym(left_ratio) / (PxReal)numLeftWheels : 0.0f;
		const PxReal rightPerWheel = numRightWheels > 0 ? physx_vehicle_clamp_sym(right_ratio) / (PxReal)numRightWheels : 0.0f;
		for (PxU32 i = 0; i < numWheels; i++) {
			directDriveThrottleResponseParams.wheelResponseMultipliers[i] = wheelIsLeftTrack[i] ? leftPerWheel : rightPerWheel;
		}
	}

	void step(PxReal dt, const PxVehicleSimulationContext &context) {
		componentSequence.update(dt, context);
	}

	void destroy() {
		PxVehicleConstraintsDestroy(physxConstraints);
		PxVehiclePhysXActorDestroy(physxActor);
	}
};

// Everything a caller can tune about ONE road wheel of a tracked vehicle, in
// Godot units/conventions -- same field set/meaning as Vehicle2WWheelConfig
// (see that struct's own doc comment for `position`'s hardpoint convention
// and `basis`'s suspension-travel-direction convention). No use_as_steering/
// use_as_traction flags -- a tank has neither; every wheel is driven, none
// steer, and which TRACK a wheel belongs to is inferred from `position.x`'s
// sign by configure_vehicle_track(), not set explicitly here (see that
// function's own doc comment for why).
struct VehicleTrackWheelConfig {
	Vector3 position;
	Basis basis;
	real_t radius = 0.35f;
	real_t half_width = 0.12f;
	real_t wheel_mass = 15.0f;
	real_t wheel_moment_of_inertia = 1.2f;
	real_t damping_rate = 0.15f;

	real_t suspension_travel = 0.15f;
	real_t suspension_stiffness = 60000.0f;
	real_t suspension_damping = 5000.0f;

	real_t tire_lateral_stiffness = 30000.0f;
	real_t tire_longitudinal_stiffness = 40000.0f;
	real_t tire_camber_stiffness = 0.0f;
	// Real tank tracks grip very differently front-to-back than a tire does
	// (a track is a continuous belt, not a point contact), but PxVehicle2's
	// tire model has no track-specific primitive to reach for (see this
	// file's own top-of-file doc comment) -- tire_friction and the two
	// stiffness values above are the whole available knob set, same as any
	// other wheel in this module.
	real_t tire_friction = 1.4f;
	real_t tire_rest_grip = 0.9f;
	real_t tire_slide_grip = 0.8f;
};

// Everything a caller can tune about a tracked vehicle, in Godot units/
// conventions. wheels' size is the real wheel count (2 to
// VehicleTrack::MAX_WHEELS) -- unlike Vehicle2WConfig/Vehicle4WWheelConfig's
// fixed front/rear or 4-element layout, since a real tank's road-wheel count
// varies by design.
struct VehicleTrackConfig {
	real_t mass = 4000.0f;
	Vector3 moment_of_inertia = Vector3(8000.0f, 9000.0f, 9000.0f);

	// Chassis collision box stands in for the hull -- deliberately close to
	// the real hull silhouette, unlike Vehicle2W/Vehicle4W's undersized
	// stand-in boxes, since a tank's hull is most of its visible mass and
	// the wheels don't carry the same "real silhouette" role a car's do.
	Vector3 chassis_half_extents = Vector3(1.2f, 0.6f, 3.0f);
	Vector3 chassis_box_center_local = Vector3(0.0f, 0.9f, 0.0f);
	Vector3 chassis_com_local = Vector3(0.0f, 0.7f, 0.0f);

	LocalVector<VehicleTrackWheelConfig> wheels;

	real_t max_engine_torque = 4000.0f;
	real_t max_brake_torque = 8000.0f;

	uint32_t collision_layer = 1;
	uint32_t collision_mask = 1;

	// Chassis contact material from the node's physics_material_override:
	// SIGNED Godot-style values (negative = rough / absorbent). The
	// contact-modify combiner reads the chassis actor's userData; the
	// chassis PxMaterial created from these is the GPU-path fallback.
	// Defaults match the combiner's no-userData behavior (1.0 / 0.0).
	float chassis_friction = 1.0f;
	float chassis_bounce = 0.0f;
};

// Same role as configure_vehicle2w()/configure_vehicle4w() (see either
// function's own doc comment) -- fills every param struct on v from config,
// builds the real PxRigidDynamic chassis+wheel shapes, the suspension-limit/
// sticky-tire constraints, the component sequence, and out_context. Does NOT
// add the actor to the scene or set its start pose. Returns false (with an
// ERR_PRINT) on any real validation failure.
//
// Track-side assignment: each wheel's own local X position sign decides
// which track it belongs to (negative = left, positive = right) -- the same
// "the vehicle inspects its own wheel children" pattern PhysXMotorcycle3D
// already uses for front/rear classification, just keyed on position instead
// of an explicit flag, since a tank wheel has no equivalent of
// use_as_steering to key on (see VehicleTrackWheelConfig's own doc comment
// for why no new per-wheel property was added instead). A wheel sitting
// exactly at X=0 is rejected -- a real tank never has a wheel on the
// centerline, so this is treated as a real configuration error, not silently
// guessed.
inline bool configure_vehicle_track(VehicleTrack &v, const VehicleTrackConfig &config, PxPhysics &physics, PxScene &scene, PxVehiclePhysXSimulationContext &out_context) {
	const uint32_t wheel_count = config.wheels.size();
	if (wheel_count < 2 || wheel_count > VehicleTrack::MAX_WHEELS) {
		ERR_PRINT(vformat("PhysX tank: wheel count must be between 2 and %d (got %d).", VehicleTrack::MAX_WHEELS, wheel_count));
		return false;
	}

	v.numWheels = wheel_count;
	v.setToDefault();

	// Right-handed triple -- same fix as configure_vehicle2w(): the old
	// left-handed (eNegZ, ePosX, ePosY) was an illegal frame whose drive came
	// out mirrored, compensated by negating the per-track multipliers below.
	v.frame = physx_vehicle_frame();
	v.scale.scale = 1.0f;

	v.axleDescription.setToDefault();
	v.numLeftWheels = 0;
	v.numRightWheels = 0;
	for (uint32_t i = 0; i < wheel_count; i++) {
		if (Math::is_zero_approx(config.wheels[i].position.x)) {
			ERR_PRINT(vformat("PhysX tank: wheel %d sits exactly on the centerline (local X = 0) -- can't tell which track it belongs to.", i));
			return false;
		}
		v.wheelIsLeftTrack[i] = config.wheels[i].position.x < 0.0f;
		if (v.wheelIsLeftTrack[i]) {
			v.numLeftWheels++;
		} else {
			v.numRightWheels++;
		}
		const PxU32 wheelId[1] = { (PxU32)i };
		v.axleDescription.addAxle(1, wheelId);
	}

	v.suspensionStateCalculationParams.suspensionJounceCalculationType = PxVehicleSuspensionJounceCalculationType::eRAYCAST;
	v.suspensionStateCalculationParams.limitSuspensionExpansionVelocity = false;

	// Single brake response entry: every wheel gets full brake response --
	// same "no separate handbrake convention" choice Vehicle2W makes.
	v.brakeResponseParams[0].maxResponse = (PxReal)config.max_brake_torque;
	for (uint32_t i = 0; i < wheel_count; i++) {
		v.brakeResponseParams[0].wheelResponseMultipliers[i] = 1.0f;
	}

	// No steering wheels at all -- steerResponseParams still has to be a
	// valid struct (the command-response component reads it unconditionally),
	// but every multiplier stays at 0 and commandState.steer is never set.
	v.steerResponseParams.maxResponse = 0.0f;
	for (uint32_t i = 0; i < wheel_count; i++) {
		v.steerResponseParams.wheelResponseMultipliers[i] = 0.0f;
	}

	// Real per-wheel multipliers are set every tick by setDriverInput(), not
	// here -- maxResponse alone scales the [-1, 1] signed command into real
	// torque.
	v.directDriveThrottleResponseParams.maxResponse = (PxReal)config.max_engine_torque;
	for (uint32_t i = 0; i < wheel_count; i++) {
		v.directDriveThrottleResponseParams.wheelResponseMultipliers[i] = 0.0f;
	}

	v.rigidBodyParams.mass = (PxReal)config.mass;
	v.rigidBodyParams.moi = godot_to_px(config.moment_of_inertia);

	for (uint32_t i = 0; i < wheel_count; i++) {
		const VehicleTrackWheelConfig &w = config.wheels[i];

		v.wheelParams[i].radius = (PxReal)w.radius;
		v.wheelParams[i].halfWidth = (PxReal)w.half_width;
		v.wheelParams[i].mass = (PxReal)w.wheel_mass;
		v.wheelParams[i].moi = (PxReal)w.wheel_moment_of_inertia;
		v.wheelParams[i].dampingRate = (PxReal)w.damping_rate;

		v.suspensionForceParams[i].stiffness = (PxReal)w.suspension_stiffness;
		v.suspensionForceParams[i].damping = (PxReal)w.suspension_damping;
		// Even split across every road wheel -- same per-wheel static-load
		// estimate Vehicle2W/Vehicle4W use, just divided N ways instead of 2
		// or 4.
		v.suspensionForceParams[i].sprungMass = v.rigidBodyParams.mass / (PxReal)wheel_count;

		// Same attachment-vs-rest-position backing-out every other
		// configure_vehicleNW() in this module does -- see
		// configure_vehicle2w()'s own doc comment for why w.position has to
		// mean "rest position", not "attachment".
		const Vector3 travel_dir_local = w.basis.xform(Vector3(0.0f, -1.0f, 0.0f)).normalized();
		const PxReal restLoadEstimate = v.suspensionForceParams[i].sprungMass * 9.81f;
		const PxReal jounceAtRest = (w.suspension_stiffness > 0.0) ? PxClamp(restLoadEstimate / (PxReal)w.suspension_stiffness, 0.0f, (PxReal)w.suspension_travel) : 0.0f;
		const Vector3 attachment_local = (w.position - config.chassis_com_local) - travel_dir_local * (real_t)((PxReal)w.suspension_travel - jounceAtRest);
		v.suspensionParams[i].suspensionAttachment = PxTransform(godot_to_px(attachment_local), godot_to_px(w.basis.get_rotation_quaternion()));
		v.suspensionParams[i].suspensionTravelDir = godot_to_px(travel_dir_local);
		v.suspensionParams[i].suspensionTravelDist = (PxReal)w.suspension_travel;
		v.suspensionParams[i].wheelAttachment = PxTransform(PxIdentity);

		v.suspensionComplianceParams[i] = PxVehicleSuspensionComplianceParams();

		v.tireForceParams[i].latStiffX = 0.01f;
		v.tireForceParams[i].latStiffY = (PxReal)w.tire_lateral_stiffness;
		v.tireForceParams[i].longStiff = (PxReal)w.tire_longitudinal_stiffness;
		v.tireForceParams[i].camberStiff = (PxReal)w.tire_camber_stiffness;
		v.tireForceParams[i].restLoad = v.suspensionForceParams[i].sprungMass * 9.81f;
		v.tireForceParams[i].frictionVsSlip[0][0] = 0.0f;
		v.tireForceParams[i].frictionVsSlip[0][1] = (PxReal)w.tire_friction * (PxReal)w.tire_rest_grip;
		v.tireForceParams[i].frictionVsSlip[1][0] = 0.1f;
		v.tireForceParams[i].frictionVsSlip[1][1] = (PxReal)w.tire_friction;
		v.tireForceParams[i].frictionVsSlip[2][0] = 1.0f;
		v.tireForceParams[i].frictionVsSlip[2][1] = (PxReal)w.tire_friction * (PxReal)w.tire_slide_grip;
		v.tireForceParams[i].loadFilter[0][0] = 0.0f;
		v.tireForceParams[i].loadFilter[0][1] = 0.23f;
		v.tireForceParams[i].loadFilter[1][0] = 3.0f;
		v.tireForceParams[i].loadFilter[1][1] = 3.0f;

		v.physxMaterialFrictionParams[i].defaultFriction = (PxReal)w.tire_friction;
		v.physxMaterialFrictionParams[i].materialFrictions = nullptr;
		v.physxMaterialFrictionParams[i].nbMaterialFrictions = 0;

		v.physxSuspensionLimitConstraintParams[i].restitution = 0.0f;
		v.physxSuspensionLimitConstraintParams[i].directionForSuspensionLimitConstraint = PxVehiclePhysXSuspensionLimitConstraintParams::eROAD_GEOMETRY_NORMAL;

		v.physxWheelShapeLocalPoses[i] = PxTransform(PxIdentity);
	}

	if (!v.axleDescription.isValid()) {
		ERR_PRINT("PhysX tank: invalid axle description.");
		return false;
	}
	if (!v.rigidBodyParams.isValid()) {
		ERR_PRINT("PhysX tank: invalid rigid body params.");
		return false;
	}

	v.physxRoadGeometryQueryParams.roadGeometryQueryType = PxVehiclePhysXRoadGeometryQueryType::eRAYCAST;
	// Wheels hit only non-area shapes on a layer in the vehicle's
	// collision_mask, like a Godot raycast — never an Area3D.
	v.physxRoadGeometryQueryParams.defaultFilterData = PxQueryFilterData(PxFilterData(0, 0, 0, 0), PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER);
	v.physxRoadGeometryQueryParams.filterCallback = &v.wheel_query_filter;
	v.wheel_query_filter.collision_mask = (PxU32)config.collision_mask;
	v.physxRoadGeometryQueryParams.filterDataEntries = nullptr;

	v.physxActorCMassLocalPose = PxTransform(godot_to_px(config.chassis_com_local));
	v.physxActorBoxShapeHalfExtents = godot_to_px(config.chassis_half_extents);
	v.physxActorBoxShapeLocalPose = PxTransform(godot_to_px(config.chassis_box_center_local));

	PxMaterial *wheel_material = physics.createMaterial((PxReal)config.wheels[0].tire_friction, (PxReal)config.wheels[0].tire_friction, 0.1f);
	// Same rationale as configure_vehicle2w()/configure_vehicle4w(): the
	// chassis box is a real simulation shape with its own material, not the
	// wheels' tire-friction one. The contact-modify combiner overrides it on
	// the CPU path (chassis actor userData, from config.chassis_friction/bounce);
	// the GPU path solves straight from it.
	PxMaterial *chassis_material = physics.createMaterial(
			(PxReal)(config.chassis_friction < 0.0f ? -config.chassis_friction : config.chassis_friction),
			(PxReal)(config.chassis_friction < 0.0f ? -config.chassis_friction : config.chassis_friction),
			(PxReal)CLAMP(config.chassis_bounce, 0.0f, 1.0f));
	if (!wheel_material || !chassis_material) {
		if (wheel_material) {
			wheel_material->release();
		}
		if (chassis_material) {
			chassis_material->release();
		}
		ERR_PRINT("PhysX tank: failed to create material.");
		return false;
	}
	PxCookingParams cookingParams(physics.getTolerancesScale());

	{
		// Same eSIMULATION_SHAPE-only, non-scene-query chassis convention as
		// configure_vehicle2w()/configure_vehicle4w() -- see either
		// function's own comment on why eSCENE_QUERY_SHAPE would break the
		// wheels' own road-geometry raycasts.
		const PxFilterData chassisFilterData((PxU32)config.collision_layer, (PxU32)config.collision_mask, 0, 0);
		const PxShapeFlags chassisShapeFlags(PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eVISUALIZATION);
		const PxVehiclePhysXRigidActorParams actorParams(v.rigidBodyParams, nullptr);
		const PxBoxGeometry boxGeom(v.physxActorBoxShapeHalfExtents);
		const PxVehiclePhysXRigidActorShapeParams actorShapeParams(boxGeom, v.physxActorBoxShapeLocalPose, *chassis_material, chassisShapeFlags, chassisFilterData, chassisFilterData);
		const PxVehiclePhysXWheelParams wheelParams(v.axleDescription, v.wheelParams);
		const PxVehiclePhysXWheelShapeParams wheelShapeParams(*wheel_material, PxShapeFlags(0), PxFilterData(), PxFilterData());

		PxVehiclePhysXActorCreate(
				v.frame,
				actorParams, v.physxActorCMassLocalPose, actorShapeParams,
				wheelParams, wheelShapeParams,
				physics, cookingParams,
				v.physxActor);
	}
	wheel_material->release();
	chassis_material->release();

	if (!v.physxActor.rigidBody) {
		ERR_PRINT("PhysX tank: PxVehiclePhysXActorCreate failed.");
		return false;
	}

	// See configure_vehicle4w(): explicit chassis gravity-off.
	physx_vehicle_chassis_gravity_off(*v.physxActor.rigidBody);

	PxVehicleConstraintsCreate(v.axleDescription, physics, *v.physxActor.rigidBody, v.physxConstraints);

	v.initComponentSequence();
	v.transmissionCommandState.gear = PxVehicleDirectDriveTransmissionCommandState::eFORWARD;

	out_context.setToDefault();
	out_context.frame = v.frame;
	out_context.scale = v.scale;
	out_context.gravity = physx_vehicle_scene_gravity(scene, v.frame);
	out_context.physxScene = &scene;
	out_context.physxUnitCylinderSweepMesh = nullptr;

	return true;
}
