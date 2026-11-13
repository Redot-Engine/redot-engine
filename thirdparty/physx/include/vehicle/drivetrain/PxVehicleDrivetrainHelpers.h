// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_DRIVETRAIN_HELPERS_H
#define PX_VEHICLE_DRIVETRAIN_HELPERS_H

#include "vehicle/drivetrain/PxVehicleDrivetrainParams.h"
#include "vehicle/drivetrain/PxVehicleDrivetrainStates.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxVehicleWheelRigidBody1dState;

/**
\brief Compute the coupling strength of the clutch.
\param[in] clutchResponseState describes the response of the clutch to the input clutch command.
\param[in] gearboxParams holds the index of neutral gear. 
\param[in] gearboxState describes the current gear.
\note If the gear is in neutral the clutch is fully disengaged and the clutch strength is 0.
\note A clutch response state of 0.0 denotes a fully engaged clutch with maximum strength.
\note A clutch response state of 1.0 denotes a fully disengaged clutch with a strength of 0.0.
*/
PX_FORCE_INLINE PxReal PxVehicleClutchStrengthCompute(const PxVehicleClutchCommandResponseState& clutchResponseState, const PxVehicleGearboxParams& gearboxParams, const PxVehicleGearboxState& gearboxState)
{
	return (gearboxParams.neutralGear != gearboxState.currentGear) ? clutchResponseState.commandResponse : 0.0f;
}

/**
\brief Compute the damping rate of the engine.
\param[in] engineParams describes various damping rates of the engine in different operational states.
\param[in] gearboxParams holds the index of neutral gear.
\param[in] gearboxState describes the current gear.
\param[in] clutchResponseState is the response of the clutch to the clutch command.
\param[in] throttleResponseState is the response of the throttle to the throttle command.
\note Engines typically have different damping rates with clutch engaged and disengaged.
\note Engines typically have different damping rates at different throttle pedal values.
\see PxVehicleClutchStrengthCompute()
\note In neutral gear the clutch is considered to be fully disengaged.
*/
PX_FORCE_INLINE PxReal PxVehicleEngineDampingRateCompute
(const PxVehicleEngineParams& engineParams, 
 const PxVehicleGearboxParams& gearboxParams, const PxVehicleGearboxState& gearboxState,
 const PxVehicleClutchCommandResponseState& clutchResponseState,
 const PxVehicleEngineDriveThrottleCommandResponseState& throttleResponseState)
{
	const PxReal  K = (gearboxParams.neutralGear != gearboxState.currentGear) ? clutchResponseState.normalisedCommandResponse : 0.0f;
	const PxReal zeroThrottleDamping = engineParams.dampingRateZeroThrottleClutchEngaged +
		(1.0f - K)* (engineParams.dampingRateZeroThrottleClutchDisengaged - engineParams.dampingRateZeroThrottleClutchEngaged);
	const PxReal appliedThrottle = throttleResponseState.commandResponse;
	const PxReal fullThrottleDamping = engineParams.dampingRateFullThrottle;
	const PxReal engineDamping = zeroThrottleDamping + (fullThrottleDamping - zeroThrottleDamping)*appliedThrottle;
	return engineDamping;
}

/**
\brief Compute the gear ratio delivered by the gearbox in the current gear.
\param[in] gearboxParams describes the gear ratio of each gear and the final ratio.
\param[in] gearboxState describes the current gear.
\note The gear ratio is the product of the gear ratio of the current gear and the final gear ratio of the gearbox.
*/
PX_FORCE_INLINE PxReal PxVehicleGearRatioCompute
(const PxVehicleGearboxParams& gearboxParams, const PxVehicleGearboxState& gearboxState)
{
	const PxReal gearRatio = gearboxParams.ratios[gearboxState.currentGear] * gearboxParams.finalRatio;
	return gearRatio;
}

/**
\brief Compute the drive torque to deliver to the engine.
\param[in] engineParams describes the profile of maximum available torque across the full range of engine rotational speed.
\param[in] engineState describes the engine rotational speed.
\param[in] throttleCommandResponseState describes the engine's response to input throttle command.
*/
PX_FORCE_INLINE PxReal PxVehicleEngineDriveTorqueCompute
(const PxVehicleEngineParams& engineParams, const PxVehicleEngineState& engineState, const PxVehicleEngineDriveThrottleCommandResponseState& throttleCommandResponseState)
{
	const PxReal appliedThrottle = throttleCommandResponseState.commandResponse;
	const PxReal peakTorque = engineParams.peakTorque;
	const PxVehicleFixedSizeLookupTable<PxReal, PxVehicleEngineParams::eMAX_NB_ENGINE_TORQUE_CURVE_ENTRIES>& torqueCurve = engineParams.torqueCurve;
	const PxReal normalisedRotSpeed = engineState.rotationSpeed/engineParams.maxOmega;
	const PxReal engineDriveTorque = appliedThrottle*peakTorque*torqueCurve.interpolate(normalisedRotSpeed);
	return engineDriveTorque;
}

/**
\deprecated This API was introduced with the new Vehicle API for transition purposes but will be removed in a future version.

\brief Compute the contribution that each wheel makes to the averaged wheel speed at the clutch plate connected to the wheels driven by 
the differential.
\param[in] diffParams describes the wheels coupled to the differential and the operation of the torque split at the differential.
\param[in] nbWheels The number of wheels. Can be larger than the number of wheels connected to the differential.
\param[out] diffAveWheelSpeedContributions describes the contribution that each wheel makes to the averaged wheel speed at the clutch.
            The buffer needs to be sized to be able to hold at least nbWheels entries.
\note Any wheel on an axle connected to the differential could have a non-zero value, depending on the way the differential couples to the wheels.
\note Any wheel on an axle not connected to the differential will have a zero contribution to the averaged wheel speed.
*/
void PX_DEPRECATED PxVehicleLegacyDifferentialWheelSpeedContributionsCompute
(const PxVehicleFourWheelDriveDifferentialLegacyParams& diffParams, 
 const PxU32 nbWheels, PxReal* diffAveWheelSpeedContributions);

/**
\deprecated This API was introduced with the new Vehicle API for transition purposes but will be removed in a future version.

\brief Compute the fraction of available torque that is delivered to each wheel through the differential.
\param[in] diffParams describes the wheels coupled to the differential and the operation of the torque split at the differential.
\param[in] wheelOmegas describes the rotational speeds of the wheels. Is expected to have nbWheels entries.
\param[in] nbWheels The number of wheels. Can be larger than the number of wheels connected to the differential.
\param[out] diffTorqueRatios describes the fraction of available torque delivered to each wheel.
            The buffer needs to be sized to be able to hold at least nbWheels entries.
\note Any wheel on an axle connected to the diff could receive a non-zero ratio, depending on the way the differential couples to the wheels.
\note Any wheel not on an axle connected to the diff will have a zero value.
\note The sum of all the ratios adds to 1.0.
\note Slipping wheels driven by the differential will typically receive less torque than non-slipping wheels in the event that the 
differential has a limited slip configuration.
*/
void PX_DEPRECATED PxVehicleLegacyDifferentialTorqueRatiosCompute
(const PxVehicleFourWheelDriveDifferentialLegacyParams& diffParams,
 const PxVehicleArrayData<const PxVehicleWheelRigidBody1dState>& wheelOmegas, 
 const PxU32 nbWheels, PxReal* diffTorqueRatios);


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
