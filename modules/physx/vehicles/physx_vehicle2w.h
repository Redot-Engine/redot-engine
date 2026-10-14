/**************************************************************************/
/*  physx_vehicle2w.h                                                     */
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

// A single direct-drive 2-wheel vehicle (motorcycle-style: one front steering
// wheel, one rear driven wheel) -- same composition pattern as Vehicle4W
// (vehicle/physx_vehicle4w.h), reduced to 2 wheels instead of 4.
//
// Vehicle4W's own composition can't represent this: configure_vehicle4w()
// hard-requires exactly 2 steering + 2 non-steering wheels (its Ackermann
// correction and anti-roll bars are both defined over a wheel *pair* on an
// axle), and every one of its per-wheel arrays is fixed at size 4. A 2-wheel
// vehicle has one wheel per axle -- no pair to correct or roll-couple -- so
// this is a separate, smaller composition rather than a variant of Vehicle4W.
//
// PxVehicle2 itself provides no balancing mechanism for a 2-wheeled vehicle
// (unlike Jolt's dedicated MotorcycleController, which adds an active lean
// spring) -- confirmed by reading the whole vehicle/ SDK header tree, zero
// Motorcycle/Lean/Balance-related types anywhere in it. This class is purely
// the same per-wheel suspension/tire/drivetrain composition Vehicle4W uses;
// staying upright is left to the caller (PhysXMotorcycle3D exposes
// apply_torque_impulse() so a script-side lean controller can apply a
// balancing torque every tick, the same role Jolt's lean spring plays, just
// implemented at the node/script layer instead of inside the SDK).
class Vehicle2W : public PxVehicleRigidBodyComponent,
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
	static constexpr PxU32 WHEEL_FRONT = 0;
	static constexpr PxU32 WHEEL_REAR = 1;

	// --- Base (BaseVehicleParams/State) ------------------------------------
	PxVehicleAxleDescription axleDescription;
	PxVehicleFrame frame;
	PxVehicleScale scale;
	PxVehicleSuspensionStateCalculationParams suspensionStateCalculationParams;
	PxVehicleBrakeCommandResponseParams brakeResponseParams[1];
	PxVehicleSteerCommandResponseParams steerResponseParams;
	PxVehicleSuspensionParams suspensionParams[2];
	PxVehicleSuspensionComplianceParams suspensionComplianceParams[2];
	PxVehicleSuspensionForceParams suspensionForceParams[2];
	PxVehicleTireForceParams tireForceParams[2];
	PxVehicleWheelParams wheelParams[2];
	PxVehicleRigidBodyParams rigidBodyParams;

	PxReal brakeCommandResponseStates[2] = {};
	PxReal steerCommandResponseStates[2] = {};
	PxVehicleWheelActuationState actuationStates[2];
	PxVehicleRoadGeometryState roadGeomStates[2];
	PxVehicleSuspensionState suspensionStates[2];
	PxVehicleSuspensionComplianceState suspensionComplianceStates[2];
	PxVehicleSuspensionForce suspensionForces[2];
	PxVehicleTireGripState tireGripStates[2];
	PxVehicleTireDirectionState tireDirectionStates[2];
	PxVehicleTireSpeedState tireSpeedStates[2];
	PxVehicleTireSlipState tireSlipStates[2];
	PxVehicleTireCamberAngleState tireCamberAngleStates[2];
	PxVehicleTireStickyState tireStickyStates[2];
	PxVehicleTireForce tireForces[2];
	PxVehicleWheelRigidBody1dState wheelRigidBody1dStates[2];
	PxVehicleWheelLocalPose wheelLocalPoses[2];
	PxVehicleRigidBodyState rigidBodyState;

	// --- Direct drive (DirectDrivetrainParams/State) -----------------------
	PxVehicleDirectDriveThrottleCommandResponseParams directDriveThrottleResponseParams;
	PxReal directDriveThrottleResponseStates[2] = {};
	PxVehicleCommandState commandState;
	PxVehicleDirectDriveTransmissionCommandState transmissionCommandState;

	// --- PhysX integration (PhysXIntegrationParams/State) ------------------
	PxVehiclePhysXRoadGeometryQueryParams physxRoadGeometryQueryParams;
	// The road-geometry query params' filterCallback points here, so it must
	// live as long as the vehicle composition.
	PhysXWheelQueryFilter wheel_query_filter;
	PxVehiclePhysXMaterialFrictionParams physxMaterialFrictionParams[2];
	PxVehiclePhysXSuspensionLimitConstraintParams physxSuspensionLimitConstraintParams[2];
	PxTransform physxActorCMassLocalPose;
	PxVec3 physxActorBoxShapeHalfExtents;
	PxTransform physxActorBoxShapeLocalPose;
	PxTransform physxWheelShapeLocalPoses[2];
	PxVehiclePhysXActor physxActor;
	PxVehiclePhysXSteerState physxSteerState;
	PxVehiclePhysXConstraints physxConstraints;

	PxVehicleComponentSequence componentSequence;
	PxU8 componentSequenceSubstepGroupHandle = 0;

	void setToDefault() {
		for (PxU32 i = 0; i < 2; i++) {
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
		outAntiRollTorque = nullptr; // no anti-roll bar -- meaningless with one wheel per axle
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
		outAckermannParams.setEmpty(); // no wheel pair to Ackermann-correct
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

	void step(PxReal dt, const PxVehicleSimulationContext &context) {
		componentSequence.update(dt, context);
	}

	void destroy() {
		PxVehicleConstraintsDestroy(physxConstraints);
		PxVehiclePhysXActorDestroy(physxActor);
	}
};

// Everything a caller can tune about ONE wheel of a direct-drive 2-wheel
// vehicle, in Godot units/conventions -- same field set/meaning as
// Vehicle4WWheelConfig (see that struct's own doc comment for `position`'s
// hardpoint convention).
struct Vehicle2WWheelConfig {
	Vector3 position;
	// The wheel node's own local orientation (relative to the vehicle body) --
	// identity means straight-down suspension travel and an unrotated axle,
	// matching every existing scene. A non-identity basis angles the fork
	// (or, on a car, gives a wheel real camber/caster) -- same convention
	// stock Godot's own VehicleWheel3D already uses (it derives suspension
	// travel direction from -basis.column(1) and axle from basis.column(0)
	// of the wheel node's own local transform; see vehicle_body_3d.cpp
	// VehicleWheel3D::_notification()). PxVehicleSuspensionParams.
	// suspensionAttachment/suspensionTravelDir accept exactly this: any unit
	// direction "in the frame of the rigid body", so mirroring stock's own
	// convention needed no new PhysX capability, just wiring it through.
	Basis basis;
	real_t radius = 0.30f;
	real_t half_width = 0.08f;
	real_t wheel_mass = 8.0f;
	real_t wheel_moment_of_inertia = 0.4f;
	real_t damping_rate = 0.15f;

	real_t suspension_travel = 0.12f;
	real_t suspension_stiffness = 25000.0f;
	real_t suspension_damping = 2200.0f;

	real_t tire_lateral_stiffness = 12000.0f;
	real_t tire_longitudinal_stiffness = 12000.0f;
	real_t tire_camber_stiffness = 0.0f;
	real_t tire_friction = 1.0f;
	real_t tire_rest_grip = 0.9f;
	real_t tire_slide_grip = 0.7f;
};

// Everything a caller can tune about a direct-drive 2-wheel vehicle, in Godot
// units/conventions. front = steering only, rear = driven only (real
// motorcycle convention -- no front-wheel-drive option, unlike Vehicle4WConfig's
// per-wheel use_as_traction, since a 2-wheel direct-drive vehicle only ever
// has one meaningful drivetrain split).
struct Vehicle2WConfig {
	real_t mass = 220.0f;
	Vector3 moment_of_inertia = Vector3(40.0f, 55.0f, 20.0f);

	// Chassis collision box stands in for the bike's frame/engine mass --
	// deliberately small and low, the wheels themselves (and the visual mesh
	// a scene author parents under each wheel node) carry the real silhouette.
	Vector3 chassis_half_extents = Vector3(0.18f, 0.25f, 0.45f);
	Vector3 chassis_box_center_local = Vector3(0.0f, 0.55f, 0.0f);
	Vector3 chassis_com_local = Vector3(0.0f, 0.5f, 0.0f);

	Vehicle2WWheelConfig front_wheel;
	Vehicle2WWheelConfig rear_wheel;

	real_t max_engine_torque = 250.0f;
	real_t max_brake_torque = 1200.0f;
	real_t max_steer_angle = 0.5f; // radians

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

// Same role as configure_vehicle4w() (see that function's own doc comment) --
// fills every param struct on v from cfg, builds the real PxRigidDynamic
// chassis+wheel shapes, the suspension-limit/sticky-tire constraints, the
// component sequence, and out_context. Does NOT add the actor to the scene or
// set its start pose. Returns false (with an ERR_PRINT) on any real
// validation failure.
inline bool configure_vehicle2w(Vehicle2W &v, const Vehicle2WConfig &cfg, PxPhysics &physics, PxScene &scene, PxVehiclePhysXSimulationContext &out_context) {
	v.setToDefault();

	// Right-handed triple (lng x lat = vrt), shared with the 4W/server stacks.
	// The old (eNegZ, ePosX, ePosY) was LEFT-handed: PxVehicleFrame::isValid()
	// builds a quaternion from the basis, which a mirrored basis cannot satisfy
	// -- the frame was illegal and the direct drive rolled the wrong way,
	// papered over by negating every throttle command. With the legal frame the
	// drive rolls -Z natively and the negations are gone.
	v.frame = physx_vehicle_frame();
	v.scale.scale = 1.0f;

	const PxU32 frontWheel[1] = { Vehicle2W::WHEEL_FRONT };
	const PxU32 rearWheel[1] = { Vehicle2W::WHEEL_REAR };
	v.axleDescription.setToDefault();
	v.axleDescription.addAxle(1, frontWheel);
	v.axleDescription.addAxle(1, rearWheel);

	v.suspensionStateCalculationParams.suspensionJounceCalculationType = PxVehicleSuspensionJounceCalculationType::eRAYCAST;
	v.suspensionStateCalculationParams.limitSuspensionExpansionVelocity = false;

	// Single brake response entry: both wheels get full brake response
	// (no separate handbrake convention for a 2-wheel vehicle).
	v.brakeResponseParams[0].maxResponse = (PxReal)cfg.max_brake_torque;
	v.brakeResponseParams[0].wheelResponseMultipliers[Vehicle2W::WHEEL_FRONT] = 1.0f;
	v.brakeResponseParams[0].wheelResponseMultipliers[Vehicle2W::WHEEL_REAR] = 1.0f;

	v.steerResponseParams.maxResponse = (PxReal)cfg.max_steer_angle;
	v.steerResponseParams.wheelResponseMultipliers[Vehicle2W::WHEEL_FRONT] = 1.0f;
	v.steerResponseParams.wheelResponseMultipliers[Vehicle2W::WHEEL_REAR] = 0.0f;

	v.directDriveThrottleResponseParams.maxResponse = (PxReal)cfg.max_engine_torque;
	v.directDriveThrottleResponseParams.wheelResponseMultipliers[Vehicle2W::WHEEL_FRONT] = 0.0f;
	v.directDriveThrottleResponseParams.wheelResponseMultipliers[Vehicle2W::WHEEL_REAR] = 1.0f;

	v.rigidBodyParams.mass = (PxReal)cfg.mass;
	v.rigidBodyParams.moi = godot_to_px(cfg.moment_of_inertia);

	const Vehicle2WWheelConfig *wheel_cfgs[2] = { &cfg.front_wheel, &cfg.rear_wheel };
	for (PxU32 i = 0; i < 2; i++) {
		const Vehicle2WWheelConfig &w = *wheel_cfgs[i];

		v.wheelParams[i].radius = (PxReal)w.radius;
		v.wheelParams[i].halfWidth = (PxReal)w.half_width;
		v.wheelParams[i].mass = (PxReal)w.wheel_mass;
		v.wheelParams[i].moi = (PxReal)w.wheel_moment_of_inertia;
		v.wheelParams[i].dampingRate = (PxReal)w.damping_rate;

		v.suspensionForceParams[i].stiffness = (PxReal)w.suspension_stiffness;
		v.suspensionForceParams[i].damping = (PxReal)w.suspension_damping;
		// Sprung mass: half the vehicle's weight per wheel (front/rear split,
		// same estimate Vehicle4W uses per-axle -- exact static balance isn't
		// critical here since a real motorcycle's rider/frame layout varies
		// widely anyway).
		v.suspensionForceParams[i].sprungMass = v.rigidBodyParams.mass * 0.5f;

		// Same attachment-vs-rest-position backing-out Vehicle4W's own
		// configure_vehicle4w() does -- see that function's doc comment for
		// why w.position has to mean "rest position", not "attachment". The
		// travel direction is no longer hardcoded straight down -- it's the
		// wheel node's own local -Y (matching stock Godot's VehicleWheel3D
		// convention, see Vehicle2WWheelConfig::basis's own doc comment), so
		// the same backing-out math has to project along that direction
		// instead of assuming +Y is "up" relative to the suspension.
		const Vector3 travel_dir_local = w.basis.xform(Vector3(0.0f, -1.0f, 0.0f)).normalized();
		const PxReal restLoadEstimate = v.suspensionForceParams[i].sprungMass * 9.81f;
		const PxReal jounceAtRest = (w.suspension_stiffness > 0.0) ? PxClamp(restLoadEstimate / (PxReal)w.suspension_stiffness, 0.0f, (PxReal)w.suspension_travel) : 0.0f;
		const Vector3 attachment_local = (w.position - cfg.chassis_com_local) - travel_dir_local * (real_t)((PxReal)w.suspension_travel - jounceAtRest);
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
		ERR_PRINT("PhysX motorcycle: invalid axle description.");
		return false;
	}
	if (!v.rigidBodyParams.isValid()) {
		ERR_PRINT("PhysX motorcycle: invalid rigid body params.");
		return false;
	}

	v.physxRoadGeometryQueryParams.roadGeometryQueryType = PxVehiclePhysXRoadGeometryQueryType::eRAYCAST;
	// Wheels hit only non-area shapes on a layer in the vehicle's
	// collision_mask, like a Godot raycast — never an Area3D.
	v.physxRoadGeometryQueryParams.defaultFilterData = PxQueryFilterData(PxFilterData(0, 0, 0, 0), PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER);
	v.physxRoadGeometryQueryParams.filterCallback = &v.wheel_query_filter;
	v.wheel_query_filter.collision_mask = (PxU32)cfg.collision_mask;
	v.physxRoadGeometryQueryParams.filterDataEntries = nullptr;

	v.physxActorCMassLocalPose = PxTransform(godot_to_px(cfg.chassis_com_local));
	v.physxActorBoxShapeHalfExtents = godot_to_px(cfg.chassis_half_extents);
	v.physxActorBoxShapeLocalPose = PxTransform(godot_to_px(cfg.chassis_box_center_local));

	PxMaterial *wheel_material = physics.createMaterial((PxReal)cfg.front_wheel.tire_friction, (PxReal)cfg.front_wheel.tire_friction, 0.1f);
	// Same rationale as configure_vehicle4w(): the chassis box is a real
	// simulation shape (so other bodies can hit it) with its own material,
	// not the wheels' tire-friction one. The contact-modify combiner
	// overrides it on the CPU path (chassis actor userData, from
	// cfg.chassis_friction/bounce); the GPU path solves straight from it.
	PxMaterial *chassis_material = physics.createMaterial(
			(PxReal)(cfg.chassis_friction < 0.0f ? -cfg.chassis_friction : cfg.chassis_friction),
			(PxReal)(cfg.chassis_friction < 0.0f ? -cfg.chassis_friction : cfg.chassis_friction),
			(PxReal)CLAMP(cfg.chassis_bounce, 0.0f, 1.0f));
	if (!wheel_material || !chassis_material) {
		if (wheel_material) {
			wheel_material->release();
		}
		if (chassis_material) {
			chassis_material->release();
		}
		ERR_PRINT("PhysX motorcycle: failed to create material.");
		return false;
	}
	PxCookingParams cookingParams(physics.getTolerancesScale());

	{
		// Same eSIMULATION_SHAPE-only, non-scene-query chassis convention as
		// configure_vehicle4w() -- see that function's own comment on why
		// eSCENE_QUERY_SHAPE would break the wheels' own road-geometry raycasts.
		const PxFilterData chassisFilterData((PxU32)cfg.collision_layer, (PxU32)cfg.collision_mask, 0, 0);
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
		ERR_PRINT("PhysX motorcycle: PxVehiclePhysXActorCreate failed.");
		return false;
	}

	// See configure_vehicle4w(): explicit chassis gravity-off, so Vehicle2's
	// context-applied gravity is the only gravity.
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
