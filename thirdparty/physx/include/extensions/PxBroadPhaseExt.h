// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BROAD_PHASE_EXT_H
#define PX_BROAD_PHASE_EXT_H

#include "PxPhysXConfig.h"
#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
class PxBounds3;
#endif

class PxBroadPhaseExt
{
public:

	/**
	\brief Creates regions for PxSceneDesc, from a global box.

	This helper simply subdivides the given global box into a 2D grid of smaller boxes. Each one of those smaller boxes
	is a region of interest for the broadphase. There are nbSubdiv*nbSubdiv regions in the 2D grid. The function does not
	subdivide along the given up axis.

	This is the simplest setup one can use with PxBroadPhaseType::eMBP. A more sophisticated setup would try to cover
	the game world with a non-uniform set of regions (i.e. not just a grid).

	\param[out]	regions			Regions computed from the input global box
	\param[in]	globalBounds	World-space box covering the game world
	\param[in]	nbSubdiv		Grid subdivision level. The function will create nbSubdiv*nbSubdiv regions.
	\param[in]	upAxis			Up axis (0 for X, 1 for Y, 2 for Z).
	\return		number of regions written out to the 'regions' array

	\see PxSceneDesc PxBroadPhaseType
	*/
	static	PxU32	createRegionsFromWorldBounds(PxBounds3* regions, const PxBounds3& globalBounds, PxU32 nbSubdiv, PxU32 upAxis=1);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
