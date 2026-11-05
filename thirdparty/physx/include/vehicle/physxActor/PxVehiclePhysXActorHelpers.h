// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PHYSX_ACTOR_HELPERS_H
#define PX_VEHICLE_PHYSX_ACTOR_HELPERS_H

#include "PxFiltering.h"
#include "PxShape.h"

#if !PX_DOXYGEN
namespace physx
{

class PxGeometry;
class PxMaterial;
struct PxCookingParams;

#endif

struct PxVehicleRigidBodyParams;
struct PxVehicleAxleDescription;
struct PxVehicleWheelParams;
struct PxVehiclePhysXActor;
struct PxVehicleFrame;
struct PxVehicleSuspensionParams;

class PxVehiclePhysXRigidActorParams
{
	PX_NOCOPY(PxVehiclePhysXRigidActorParams)

public:

	PxVehiclePhysXRigidActorParams(const PxVehicleRigidBodyParams& _physxActorRigidBodyParams, const char* _physxActorName)
		: rigidBodyParams(_physxActorRigidBodyParams),
		  physxActorName(_physxActorName)
	{
	}

	const PxVehicleRigidBodyParams& rigidBodyParams;
	const char* physxActorName;
};

class PxVehiclePhysXRigidActorShapeParams
{
	PX_NOCOPY(PxVehiclePhysXRigidActorShapeParams)

public:

	PxVehiclePhysXRigidActorShapeParams
	(const PxGeometry& _geometry, const PxTransform& _localPose, const PxMaterial& _material, 
	 const PxShapeFlags _flags, const PxFilterData& _simulationFilterData, const PxFilterData& _queryFilterData)
		: geometry(_geometry),
		  localPose(_localPose),
		  material(_material),
		  flags(_flags),
		  simulationFilterData(_simulationFilterData),
		  queryFilterData(_queryFilterData)
	{
	}

	const PxGeometry& geometry;
	const PxTransform& localPose;
	const PxMaterial& material;
	PxShapeFlags flags;
	PxFilterData simulationFilterData;
	PxFilterData queryFilterData;
};

class PxVehiclePhysXWheelParams
{
	PX_NOCOPY(PxVehiclePhysXWheelParams)

public:

	PxVehiclePhysXWheelParams(const PxVehicleAxleDescription& _axleDescription, const PxVehicleWheelParams* _wheelParams)
		: axleDescription(_axleDescription),
		  wheelParams(_wheelParams)
	{
	}

	const PxVehicleAxleDescription& axleDescription;
	const PxVehicleWheelParams* wheelParams;
};

class PxVehiclePhysXWheelShapeParams
{
	PX_NOCOPY(PxVehiclePhysXWheelShapeParams)

public:

	PxVehiclePhysXWheelShapeParams(const PxMaterial& _material, const PxShapeFlags _flags, const PxFilterData _simulationFilterData, const PxFilterData _queryFilterData)
		: material(_material),
		  flags(_flags),
		  simulationFilterData(_simulationFilterData),
		  queryFilterData(_queryFilterData)
	{
	}

	const PxMaterial& material;
	PxShapeFlags flags;
	PxFilterData simulationFilterData;
	PxFilterData queryFilterData;
};

/**
\brief Create a PxRigidDynamic instance, instantiate it with desired properties and populate it with PxShape instances.
\param[in] vehicleFrame describes the frame of the vehicle.
\param[in] rigidActorParams describes the mass and moment of inertia of the rigid body.
\param[in] rigidActorCmassLocalPose specifies the mapping between actor and rigid body frame.
\param[in] rigidActorShapeParams describes the collision geometry associated with the rigid body.
\param[in] wheelParams describes the radius and half-width of the wheels.
\param[in] wheelShapeParams describes the PxMaterial and PxShapeFlags to apply to the wheel shapes.
\param[in] physics is a PxPhysics instance.
\param[in] params is a PxCookingParams instance
\param[in] vehiclePhysXActor is a record of the PxRigidDynamic and PxShape instances instantiated.
\note This is an alternative to PxVehiclePhysXArticulationLinkCreate.
\note PxVehiclePhysXActorCreate primarily serves as an illustration of the instantiation of the PhysX class instances
required to simulate a vehicle with a PxRigidDynamic.
\see PxVehiclePhysXActorDestroy
*/
void PxVehiclePhysXActorCreate
(const PxVehicleFrame& vehicleFrame,
 const PxVehiclePhysXRigidActorParams& rigidActorParams, const PxTransform& rigidActorCmassLocalPose,
 const PxVehiclePhysXRigidActorShapeParams& rigidActorShapeParams,
 const PxVehiclePhysXWheelParams& wheelParams, const PxVehiclePhysXWheelShapeParams& wheelShapeParams,
 PxPhysics& physics, const PxCookingParams& params,
 PxVehiclePhysXActor& vehiclePhysXActor);

/**
\brief Configure an actor so that it is ready for vehicle simulation.
\param[in] rigidActorParams describes the mass and moment of inertia of the rigid body.
\param[in] rigidActorCmassLocalPose specifies the mapping between actor and rigid body frame.
\param[out] rigidBody is the body to be prepared for simulation.
*/
void PxVehiclePhysXActorConfigure
(const PxVehiclePhysXRigidActorParams& rigidActorParams, const PxTransform& rigidActorCmassLocalPose,
 PxRigidBody& rigidBody);

/**
\brief Create a PxArticulationReducedCoordinate and a single PxArticulationLink, 
instantiate the PxArticulationLink with desired properties and populate it with PxShape instances.
\param[in] vehicleFrame describes the frame of the vehicle.
\param[in] rigidActorParams describes the mass and moment of inertia of the rigid body.
\param[in] rigidActorCmassLocalPose specifies the mapping between actor and rigid body frame.
\param[in] rigidActorShapeParams describes the collision geometry associated with the rigid body.
\param[in] wheelParams describes the radius and half-width of the wheels.
\param[in] wheelShapeParams describes the PxMaterial and PxShapeFlags to apply to the wheel shapes.
\param[in] physics is a PxPhysics instance.
\param[in] params is a PxCookingParams instance
\param[in] vehiclePhysXActor is a record of the PxArticulationReducedCoordinate, PxArticulationLink and PxShape instances instantiated.
\note This is an alternative to PxVehiclePhysXActorCreate.  
\note PxVehiclePhysXArticulationLinkCreate primarily serves as an illustration of the instantiation of the PhysX class instances 
required to simulate a vehicle as part of an articulated ensemble.
\see PxVehiclePhysXActorDestroy
*/
void PxVehiclePhysXArticulationLinkCreate
(const PxVehicleFrame& vehicleFrame,
 const PxVehiclePhysXRigidActorParams& rigidActorParams, const PxTransform& rigidActorCmassLocalPose,
 const PxVehiclePhysXRigidActorShapeParams& rigidActorShapeParams,
 const PxVehiclePhysXWheelParams& wheelParams, const PxVehiclePhysXWheelShapeParams& wheelShapeParams,
 PxPhysics& physics, const PxCookingParams& params,
 PxVehiclePhysXActor& vehiclePhysXActor);

/**
\brief Release the PxRigidDynamic, PxArticulationReducedCoordinate, PxArticulationLink and PxShape instances 
instantiated by PxVehiclePhysXActorCreate or PxVehiclePhysXArticulationLinkCreate.
\param[in] vehiclePhysXActor is a description of the PhysX instances to be released.
*/
void PxVehiclePhysXActorDestroy(PxVehiclePhysXActor& vehiclePhysXActor);



#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
