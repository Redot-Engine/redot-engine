// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BVH_BUILD_STRATEGY_H
#define PX_BVH_BUILD_STRATEGY_H

#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Desired build strategy for bounding-volume hierarchies
*/
struct PxBVHBuildStrategy
{
	enum Enum
	{
		eFAST = 0,		//!< Fast build strategy. Fast build speed, good runtime performance in most cases. Recommended for runtime cooking.
		eDEFAULT = 1,	//!< Default build strategy. Medium build speed, good runtime performance in all cases.
		eSAH = 2,		//!< SAH build strategy. Slower builds, slightly improved runtime performance in some cases.

		eLAST
	};
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
