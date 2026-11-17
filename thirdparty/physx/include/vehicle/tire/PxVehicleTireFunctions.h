// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_TIRE_FUNCTIONS_H
#define PX_VEHICLE_TIRE_FUNCTIONS_H

#include "foundation/PxSimpleTypes.h"
#include "vehicle/PxVehicleParams.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleSuspensionParams;
struct PxVehicleRoadGeometryState;
struct PxVehicleRigidBodyState;
struct PxVehicleTireDirectionState;
struct PxVehicleSuspensionComplianceState;
struct PxVehicleWheelParams;
struct PxVehicleTireSpeedState;
struct PxVehicleWheelActuationState;
struct PxVehicleWheelRigidBody1dState;
struct PxVehicleTireSlipState;
struct PxVehicleSuspensionState;
struct PxVehicleTireCamberAngleState;
struct PxVehicleTireGripState;
struct PxVehicleTireForceParams;
struct PxVehicleSuspensionForce;
struct PxVehicleTireForce;
struct PxVehicleTireStickyState;

/**
\deprecated This API was introduced with the new Vehicle API for transition purposes but will be removed in a future version.

\brief Compute the longitudinal and lateral tire directions in the ground plane.
\param[in] suspensionParams describes the frame of the suspension and wheel. 
\param[in] steerAngle is the steer angle in radians to be applied to the wheel.
\param[in] roadGeometryState describes the plane of the road geometry under the wheel
\param[in] rigidBodyState describes the current pose of the vehicle's rigid body in the world frame.
\param[in] frame is a description of the vehicle's lateral and longitudinal axes.
\param[out] tireDirectionState is the computed tire longitudinal and lateral directions in the world frame. 
\note PxVehicleTireDirsLegacyUpdate replicates the tire direction calculation of PhysX 5.0 and earlier.
*/
PX_DEPRECATED void PxVehicleTireDirsLegacyUpdate
(const PxVehicleSuspensionParams& suspensionParams,
 const PxReal steerAngle, const PxVehicleRoadGeometryState& roadGeometryState, const PxVehicleRigidBodyState& rigidBodyState,
 const PxVehicleFrame& frame,
 PxVehicleTireDirectionState& tireDirectionState);

/**
\brief Compute the longitudinal and lateral tire directions in the ground plane.
\param[in] suspensionParams describes the frame of the suspension and wheel.
\param[in] steerAngle is the steer angle in radians to be applied to the wheel.
\param[in] groundNormal describes the plane normal of the road geometry under the wheel.
\param[in] isWheelOnGround defines whether the wheel touches the road geometry.
\param[in] complianceState is a description of the camber and toe angle that arise from suspension compliance.
\param[in] rigidBodyState describes the current pose of the vehicle's rigid body in the world frame.
\param[in] frame is a description of the vehicle's lateral and longitudinal axes.
\param[out] tireDirectionState is the computed tire longitudinal and lateral directions in the world frame.
\note The difference between PxVehicleTireDirsUpdate and PxVehicleTireDirsLegacyUpdate is that 
PxVehicleTireDirsUpdate accounts for suspension compliance while PxVehicleTireDirsLegacyUpdate does not.
*/
void PxVehicleTireDirsUpdate
(const PxVehicleSuspensionParams& suspensionParams,
 const PxReal steerAngle, const PxVec3& groundNormal, bool isWheelOnGround,
 const PxVehicleSuspensionComplianceState& complianceState,
 const PxVehicleRigidBodyState& rigidBodyState,
 const PxVehicleFrame& frame,
 PxVehicleTireDirectionState& tireDirectionState);

/**
\brief Project the rigid body velocity at the tire  contact point along the tire longitudinal directions.
\param[in] wheelParams is a description of the wheel's radius and half-width.
\param[in] suspensionParams describes the frame of the suspension and wheel.
\param[in] steerAngle is the steer angle in radians to be applied to the wheel.
\param[in] suspensionStates is the current suspension compression state.
\param[in] tireDirectionState is the tire's longitudinal and lateral directions in the ground plane.
\param[in] rigidBodyState describes the current pose and velocity of the vehicle's rigid body in the world frame.
\param[in] roadGeometryState describes the current velocity of the road geometry under each wheel.
\param[in] frame is a description of the vehicle's lateral and longitudinal axes.
\param[out] tireSpeedState is the components of rigid body velocity at the tire contact point along the 
tire's longitudinal and lateral axes.
\see PxVehicleTireDirsUpdate
\see PxVehicleTireDirsLegacyUpdate
*/
void PxVehicleTireSlipSpeedsUpdate
(const PxVehicleWheelParams& wheelParams, const PxVehicleSuspensionParams& suspensionParams,
 const PxF32 steerAngle, const PxVehicleSuspensionState& suspensionStates, const PxVehicleTireDirectionState& tireDirectionState,
 const PxVehicleRigidBodyState& rigidBodyState, const PxVehicleRoadGeometryState& roadGeometryState,
 const PxVehicleFrame& frame,
 PxVehicleTireSpeedState& tireSpeedState);

/**
\brief Compute a tire's longitudinal and lateral slip angles. 
\param[in] wheelParams describes the radius of the wheel.
\param[in] tireSlipParams describes how to manage small longitudinal speeds by setting minimum values on the 
denominator of the quotients used to calculate lateral and longitudinal slip.
\param[in] actuationState describes whether a wheel is to be driven by a drive torque or not.
\param[in] tireSpeedState is the component of rigid body velocity at the tire contact point projected along the 
tire's longitudinal and lateral axes.
\param[in] wheelRigidBody1dState is the wheel rotation speed.
\param[out] tireSlipState is the computed tire longitudinal and lateral slips. 
\note Longitudinal slip angle has the following theoretical form: (wheelRotationSpeed*wheelRadius - longitudinalSpeed)/|longitudinalSpeed|
\note Lateral slip angle has the following theoretical form: atan(lateralSpeed/|longitudinalSpeed|)
\note The calculation of both longitudinal and lateral slip angles avoid a zero denominator using minimum values for the denominator set in
tireSlipParams.
*/
void PxVehicleTireSlipsUpdate
(const PxVehicleWheelParams& wheelParams,
 const PxVehicleTireSlipParams& tireSlipParams,
 const PxVehicleWheelActuationState& actuationState, PxVehicleTireSpeedState& tireSpeedState, const PxVehicleWheelRigidBody1dState& wheelRigidBody1dState,
 PxVehicleTireSlipState& tireSlipState);

/**
\deprecated This API was introduced with the new Vehicle API for transition purposes but will be removed in a future version.

\brief Compute a tire's longitudinal and lateral slip angles.
\param[in] wheelParams describes the radius of the wheel.
\param[in] tireSlipParams describes how to manage small longitudinal speeds by setting minimum values on the
denominator of the quotients used to calculate lateral and longitudinal slip.
\param[in] actuationState describes whether a wheel is to be driven by a drive torque or not.
\param[in] tireSpeedState is the component of rigid body velocity at the tire contact point projected along the
tire's longitudinal and lateral axes.
\param[in] wheelRigidBody1dState is the wheel rotation speed.
\param[out] tireSlipState is the computed tire longitudinal and lateral slips.
\note Longitudinal slip angle has the following theoretical form: (wheelRotationSpeed*wheelRadius - longitudinalSpeed)/|longitudinalSpeed|
\note Lateral slip angle has the following theoretical form: atan(lateralSpeed/|longitudinalSpeed|)
\note The calculation of both longitudinal and lateral slip angles avoid a zero denominator using minimum values for the denominator set in
tireSlipParams.
*/
void PX_DEPRECATED PxVehicleTireSlipsLegacyUpdate
(const PxVehicleWheelParams& wheelParams,
	const PxVehicleTireSlipParams& tireSlipParams,
	const PxVehicleWheelActuationState& actuationState, PxVehicleTireSpeedState& tireSpeedState, const PxVehicleWheelRigidBody1dState& wheelRigidBody1dState,
	PxVehicleTireSlipState& tireSlipState);


/**
\brief Compute the camber angle of  the wheel
\param[in] suspensionParams describes the frame of the suspension and wheel.
\param[in] steerAngle is the steer angle in radians to be applied to the wheel.
\param[in] groundNormal describes the plane normal of the road geometry under the wheel.
\param[in] isWheelOnGround defines whether the wheel touches the road geometry.
\param[in] complianceState is a description of the camber and toe angle that arise from suspension compliance.
\param[in] rigidBodyState describes the current pose of the vehicle's rigid body in the world frame.
\param[in] frame is a description of the vehicle's lateral and longitudinal axes.
\param[out] tireCamberAngleState is the computed camber angle of the tire expressed in radians. 
*/
void PxVehicleTireCamberAnglesUpdate
(const PxVehicleSuspensionParams& suspensionParams,
 const PxReal steerAngle, const PxVec3& groundNormal, bool isWheelOnGround,
 const PxVehicleSuspensionComplianceState& complianceState,
 const PxVehicleRigidBodyState& rigidBodyState,
 const PxVehicleFrame& frame,
 PxVehicleTireCamberAngleState& tireCamberAngleState);

/**
\brief Compute the load and friction experienced by the tire.
\param[in] tireForceParams describes the tire's friction response to longitudinal lip angle and its load response.
\param[in] frictionCoefficient describes the friction coefficient for the tire and road geometry pair.
\param[in] isWheelOnGround defines whether the wheel touches the road geometry.
\param[in] suspensionForce is the force that the suspension exerts on the sprung mass of the suspension.
\param[in] tireSlipState is the tire longitudinal and lateral slip angles.
\param[out] tireGripState is the computed load and friction experienced by the tire.
\note If the suspension cannot place the wheel on the ground the tire load and friction will be 0.0.
*/
void PxVehicleTireGripUpdate
(const PxVehicleTireForceParams& tireForceParams,
 PxReal frictionCoefficient, bool isWheelOnGround, const PxVehicleSuspensionForce& suspensionForce,
 const PxVehicleTireSlipState& tireSlipState,
 PxVehicleTireGripState& tireGripState);

/**
\brief When a tire has been at a very low speed for a threshold time without application of drive torque, a
secondary tire model is applied to bring the tire to rest using velocity constraints that asymptotically approach zero speed
along the tire's lateral and longitudinal directions.  This secondary tire model is referred to as the sticky tire model and the
tire is considered to be in the sticky tire state when the speed and time conditions are satisfied.  The purpose of 
PxVehicleTireStickyStateUpdate is to compute the target speeds of the sticky state and to record whether sticky state is
active or not.
\param[in] axleDescription is the axles of the vehicle and the wheels on each axle.
\param[in] wheelParams describes the radius of the wheel
\param[in] tireStickyParams describe the threshold speeds and times for the lateral and longitudinal sticky states.
\param[in] actuationStates describes whether each wheel experiences a drive torque.
\param[in] tireGripState is the load and friction experienced by the tire.
\param[in] tireSpeedState is the component of rigid body velocity at the tire contact point projected along the
tire's longitudinal and lateral axes.
\param[in] wheelRigidBody1dState is the wheel rotation speed.
\param[in] dt is the simulation time that has lapsed since the last call to PxVehicleTireStickyStateUpdate
\param[out] tireStickyState is a description of the sticky state of the tire in the longitudinal and lateral directions.
\note The velocity constraints are maintained through integration with the PhysX scene using the function 
PxVehiclePhysXConstraintStatesUpdate. Alternative implementations independent of PhysX are possible. 
\see PxVehiclePhysXConstraintStatesUpdate
\see PxVehicleTireSlipsAccountingForStickyStatesUpdate
*/
void PxVehicleTireStickyStateUpdate
(const PxVehicleAxleDescription& axleDescription, const PxVehicleWheelParams& wheelParams,
 const PxVehicleTireStickyParams& tireStickyParams,
 const PxVehicleArrayData<const PxVehicleWheelActuationState>& actuationStates, 
 const PxVehicleTireGripState& tireGripState, const PxVehicleTireSpeedState& tireSpeedState, const PxVehicleWheelRigidBody1dState& wheelRigidBody1dState,
 const PxReal dt,
 PxVehicleTireStickyState& tireStickyState);

/**
\brief Set the tire longitudinal and lateral slip values to 0.0 in the event that the tire has entred tire sticky state. This is 
necessary to avoid both tire models being simultaneously active and interfering with each other.
\param[in] tireStickyState is a description of the sticky state of the tire in the longitudinal and lateral directions.
\param[out] tireSlipState is the updated lateral and longudinal slip with either set to 0.0 in the event that the correspoinding
sticky state is active.
\note This function should not be invoked if there is no subsequent component to implement the sticky tire model.
*/
void PxVehicleTireSlipsAccountingForStickyStatesUpdate
(const PxVehicleTireStickyState& tireStickyState,
 PxVehicleTireSlipState& tireSlipState);

/**
\brief Compute the longitudinal and lateral forces in the world frame that develop on the tire as a consequence of 
the tire's slip angles, friction and load.
\param[in] wheelParams describes the radius and half-width of the wheel.
\param[in] suspensionParams describes the frame of the suspension and wheel.
\param[in] tireForceParams describes the conversion of slip angle, friction and load to a force along the longitudinal 
and lateral directions of the tire.
\param[in] complianceState is a description of the camber and toe angle that arise from suspension compliance.
\param[out] tireGripState is the load and friction experienced by the tire.
\param[in] tireDirectionState is the tire's longitudinal and lateral directions in the ground plane.
\param[in] tireSlipState is the longitudinal and lateral slip angles.
\param[in] tireCamberAngleState is the camber angle of the tire expressed in radians.
\param[in] rigidBodyState describes the current pose of the vehicle's rigid body in the world frame.
\param[out] tireForce is the computed tire forces in the world frame.
*/
void PxVehicleTireForcesUpdate
(const PxVehicleWheelParams& wheelParams, const PxVehicleSuspensionParams& suspensionParams,
 const PxVehicleTireForceParams& tireForceParams,
 const PxVehicleSuspensionComplianceState& complianceState,
 const PxVehicleTireGripState& tireGripState, const PxVehicleTireDirectionState& tireDirectionState, 
 const PxVehicleTireSlipState& tireSlipState, const PxVehicleTireCamberAngleState& tireCamberAngleState,
 const PxVehicleRigidBodyState& rigidBodyState,
 PxVehicleTireForce& tireForce);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
