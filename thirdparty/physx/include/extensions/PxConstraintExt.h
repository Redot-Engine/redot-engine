// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONSTRAINT_EXT_H
#define PX_CONSTRAINT_EXT_H

#include "foundation/PxPreprocessor.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Unique identifiers for extensions classes which implement a constraint based on PxConstraint.

\note Users which want to create their own custom constraint types should choose an ID larger or equal to eNEXT_FREE_ID
and not eINVALID_ID.

\see PxConstraint PxSimulationEventCallback.onConstraintBreak()
*/
struct PxConstraintExtIDs
{
	enum Enum
	{
		eJOINT,
		eVEHICLE_JOINT,
		eNEXT_FREE_ID,
		eINVALID_ID = 0x7fffffff
	};
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
