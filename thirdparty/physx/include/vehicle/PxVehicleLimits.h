// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_VEHICLE_LIMITS_H
#define PX_VEHICLE_LIMITS_H

#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
struct PxVehicleLimits
{
	enum Enum
	{
		eMAX_NB_WHEELS = 20,
		eMAX_NB_AXLES = eMAX_NB_WHEELS
	};
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

