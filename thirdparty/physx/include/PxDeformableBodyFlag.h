// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_BODY_FLAGS_H
#define PX_DEFORMABLE_BODY_FLAGS_H

#include "PxPhysXConfig.h"
#include "foundation/PxFlags.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Flags to enable or disable special modes of a PxDeformableBody instance
*/
struct PxDeformableBodyFlag
{
	enum Enum
	{
		eDISABLE_SELF_COLLISION = 1 << 0,	//!< Determines if self collision will be detected and resolved
		eENABLE_SPECULATIVE_CCD = 1 << 1,	//!< Enables support for speculative contact generation, see #PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD
		eKINEMATIC = 1 << 2					//!< Enables support for kinematic motion of the simulation mesh, see #PxRigidBodyFlag::eKINEMATIC
	};
};

typedef PxFlags<PxDeformableBodyFlag::Enum, PxU8> PxDeformableBodyFlags;

#if !PX_DOXYGEN
}
#endif

#endif // PX_DEFORMABLE_BODY_FLAGS_H
