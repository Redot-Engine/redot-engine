// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_COMPONENT_H
#define PX_VEHICLE_COMPONENT_H

#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
struct PxVehicleSimulationContext;

class PxVehicleComponent
{
public:

	virtual ~PxVehicleComponent() {}

	/**
	\brief Update function for a vehicle component.

	\param[in] dt The timestep size to use for the update step.
	\param[in] context Vehicle simulation context holding global data or data that usually applies to a
	                   large group of vehicles.
	\return True if subsequent components in a sequence should get updated, false if the sequence should
	        be aborted.

	\see PxVehicleComponentSequence
	*/
	virtual bool update(const PxReal dt, const PxVehicleSimulationContext& context) = 0;

};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
