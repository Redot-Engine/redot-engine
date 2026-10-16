// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BVH_33_MIDPHASE_DESC_H
#define PX_BVH_33_MIDPHASE_DESC_H

#include "foundation/PxPreprocessor.h"
#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\deprecated This is only used for BVH33 which is deprecated and will be removed in a future version. Use BVH34 instead.

\brief Enumeration for mesh cooking hints.
*/
struct PX_DEPRECATED PxMeshCookingHint
{
	enum Enum
	{
		eSIM_PERFORMANCE = 0,		//!< Default value. Favors higher quality hierarchy with higher runtime performance over cooking speed.
		eCOOKING_PERFORMANCE = 1	//!< Enables fast cooking path at the expense of somewhat lower quality hierarchy construction.
	};
};

/**
\deprecated BVH33 is deprecated and will be removed in a future version. Use BVH34 instead.

\brief Structure describing parameters affecting BVH33 midphase mesh structure.

\see PxCookingParams, PxMidphaseDesc
*/
struct PX_DEPRECATED PxBVH33MidphaseDesc
{
	/**
	\brief Controls the trade-off between mesh size and runtime performance.

	Using a value of 1.0 will produce a larger cooked mesh with generally higher runtime performance,
	using 0.0 will produce a smaller cooked mesh, with generally lower runtime performance.

	Values outside of [0,1] range will be clamped and cause a warning when any mesh gets cooked.

	<b>Default value:</b> 0.55
	<b>Range:</b> [0.0f, 1.0f]
	*/
	PxF32							meshSizePerformanceTradeOff;

	/**
	\brief Mesh cooking hint. Used to specify mesh hierarchy construction preference.

	<b>Default value:</b> PxMeshCookingHint::eSIM_PERFORMANCE
	*/
	PxMeshCookingHint::Enum			meshCookingHint;

	/**
	\brief Desc initialization to default value.
	*/
    void setToDefault()
    {
	    meshSizePerformanceTradeOff = 0.55f;
		meshCookingHint = PxMeshCookingHint::eSIM_PERFORMANCE;
    }

	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid.
	*/
	bool isValid() const
	{
		if(meshSizePerformanceTradeOff < 0.0f || meshSizePerformanceTradeOff > 1.0f)
			return false;
		return true;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif


#endif

