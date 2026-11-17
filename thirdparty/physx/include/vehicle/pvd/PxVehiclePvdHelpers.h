// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_PVD_HELPERS_H
#define PX_VEHICLE_PVD_HELPERS_H

#include "foundation/PxSimpleTypes.h"

class OmniPvdWriter;
namespace physx
{
class PxAllocatorCallback;
struct PxVehiclePvdAttributeHandles;
struct PxVehiclePvdObjectHandles;
} 

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Create the attribute handles necessary to reflect vehicles in omnipvd.
\param[in] allocator is used to allocate the memory used to store the attribute handles.
\param[in] omniWriter is used to register the attribute handles with omnipvd.
\see PxVehicleSimulationContext
\see PxVehiclePVDComponent
\see PxVehiclePvdAttributesRelease
*/
PxVehiclePvdAttributeHandles* PxVehiclePvdAttributesCreate
(PxAllocatorCallback& allocator, OmniPvdWriter& omniWriter);

/**
\brief Destory the attribute handles created by PxVehiclePvdAttributesCreate().
\param[in] allocator must be the instance used by PxVehiclePvdObjectCreate().
\param[in] attributeHandles is the PxVehiclePvdAttributeHandles created by PxVehiclePvdAttributesCreate().
\see PxVehiclePvdAttributesCreate
*/
void PxVehiclePvdAttributesRelease
(PxAllocatorCallback& allocator, PxVehiclePvdAttributeHandles& attributeHandles);

/**
\brief Create omnipvd objects that will be used to reflect an individual veicle in omnipvd.
\param[in] nbWheels must be greater than or equal to the number of wheels on the vehicle.
\param[in] nbAntirolls must be greater than or equal to the number of antiroll bars on the vehicle.
\param[in] maxNbPhysxMaterialFrictions must be greater than or equal to the number of PxPhysXMaterialFriction instances associated with any wheel of the vehicle.
\param[in] contextHandle is typically used to associated vehicles with a particular scene or group.
\param[in] allocator is used to allocate the memory used to store handles to the created omnipvd objects.
\note PxVehiclePvdObjectCreate() must be called after PxVehiclePvdAttributesCreate().
\see PxVehicleAxleDescription
\see PxVehicleAntiRollForceParams
\see PxVehiclePhysXMaterialFrictionParams
\see PxVehiclePVDComponent
\see PxVehiclePvdAttributesCreate
*/
PxVehiclePvdObjectHandles* PxVehiclePvdObjectCreate
(const PxU32 nbWheels, const PxU32 nbAntirolls, const PxU32 maxNbPhysxMaterialFrictions,
 const PxU64 contextHandle, PxAllocatorCallback& allocator);

/**
\brief Destroy the PxVehiclePvdObjectHandles instance created by PxVehiclePvdObjectCreate().
\param[in] omniWriter is used to register the attribute handles with omnipvd.
\param[in] allocator must be the instance used by PxVehiclePvdObjectCreate().
\param[in] objectHandles is the PxVehiclePvdObjectHandles that was created by PxVehiclePvdObjectCreate().
*/
void PxVehiclePvdObjectRelease
(OmniPvdWriter& omniWriter, PxAllocatorCallback& allocator, PxVehiclePvdObjectHandles& objectHandles);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
