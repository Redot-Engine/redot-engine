// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_EXTENDED_H
#define PX_EXTENDED_H

// This needs to be included in Foundation just for the debug renderer

#include "PxPhysXConfig.h"
#include "foundation/PxTransform.h"
#include "foundation/PxAssert.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

// This has to be done here since it also changes the top-level "Px" and "Np" APIs
#define PX_BIG_WORLDS

#ifdef PX_BIG_WORLDS
	typedef PxVec3d	PxExtendedVec3;
	typedef	double	PxExtended;
	#define	PX_MAX_EXTENDED	PX_MAX_F64

	PX_FORCE_INLINE PxVec3 toVec3(const PxExtendedVec3& v)
	{
		return PxVec3(float(v.x), float(v.y), float(v.z));
	}

	// Computes the single-precision difference between two extended-precision points
	PX_INLINE	PxVec3	diff(const PxExtendedVec3& p1, const PxExtendedVec3& p0)
	{
		return PxVec3(float(p1.x - p0.x), float(p1.y - p0.y), float(p1.z - p0.z));
	}
#else
	typedef	PxVec3	PxExtendedVec3;
	typedef	float	PxExtended;
	#define	PX_MAX_EXTENDED	PX_MAX_F32

	PX_FORCE_INLINE PxVec3 toVec3(const PxExtendedVec3& v)
	{
		return v;
	}

	// Computes the single-precision difference between two extended-precision points
	PX_INLINE	PxVec3	diff(const PxExtendedVec3& p1, const PxExtendedVec3& p0)
	{
		return p1 - p0;
	}
#endif

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
