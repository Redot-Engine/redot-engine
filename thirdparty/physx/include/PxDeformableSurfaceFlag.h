// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSICS_DEFORMABLE_SURFACE_FLAGS_H
#define PX_PHYSICS_DEFORMABLE_SURFACE_FLAGS_H

#include "foundation/PxFlags.h"
#include "foundation/PxSimpleTypes.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxDeformableSurfaceFlag
{
	enum Enum
	{
		eUSE_ANISOTROPIC_MODEL		= 1 << 0,	// 0: use isotropic model, 1: use anistropic model
		eENABLE_FLATTENING			= 1 << 1	// 0: query rest bending angle from rest shape, 1: use zero rest bending angle
	};
};

typedef PxFlags<PxDeformableSurfaceFlag::Enum, PxU16> PxDeformableSurfaceFlags;

/**
\brief Identifies input and output buffers for PxDeformableSurface.
*/
struct PxDeformableSurfaceDataFlag
{
	enum Enum
	{
		eNONE						= 0,
		ePOSITION_INVMASS			= 1 << 0,
		eVELOCITY					= 1 << 1,
		eREST_POSITION				= 1 << 2,
		eALL = ePOSITION_INVMASS | eVELOCITY | eREST_POSITION
	};
};

typedef PxFlags<PxDeformableSurfaceDataFlag::Enum, PxU32> PxDeformableSurfaceDataFlags;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_PHYSICS_DEFORMABLE_SURFACE_FLAGS_H
