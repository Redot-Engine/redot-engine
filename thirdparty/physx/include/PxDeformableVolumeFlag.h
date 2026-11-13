// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_VOLUME_FLAGS_H
#define PX_DEFORMABLE_VOLUME_FLAGS_H

#include "PxPhysXConfig.h"
#include "foundation/PxFlags.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Flags to enable or disable special modes of a PxDeformableVolume instance
*/
struct PxDeformableVolumeFlag
{
	enum Enum
	{
		eCOMPUTE_STRESS_TENSOR = 1 << 0,				//!< Enables computation of a Cauchy stress tensor for every tetrahedron in the simulation mesh. The tensors can be accessed through the deformable volume direct API
		ePARTIALLY_KINEMATIC = 1 << 1					//!< Enables partially kinematic motion of the collision and simulation mesh.
	};
};

typedef PxFlags<PxDeformableVolumeFlag::Enum, PxU16> PxDeformableVolumeFlags;

/**
\brief Identifies the buffers of a PxDeformableVolume instance.

\see PxDeformableVolume::markDirty()
*/
struct PxDeformableVolumeDataFlag
{
	enum Enum
	{
		eNONE = 0,

		ePOSITION_INVMASS = 1 << 0,             //!< The collision mesh's positions
		eSIM_POSITION_INVMASS = 1 << 1,         //!< The simulation mesh's positions and inverse masses
		eSIM_VELOCITY = 1 << 2,                 //!< The simulation mesh's velocities
		eREST_POSITION_INVMASS = 1 << 3,        //!< The collision mesh's rest positions

		eALL = ePOSITION_INVMASS | eSIM_POSITION_INVMASS | eSIM_VELOCITY | eREST_POSITION_INVMASS
	};
};

typedef PxFlags<PxDeformableVolumeDataFlag::Enum, PxU32> PxDeformableVolumeDataFlags;

#if !PX_DOXYGEN
}
#endif

#endif // PX_DEFORMABLE_VOLUME_FLAGS_H
