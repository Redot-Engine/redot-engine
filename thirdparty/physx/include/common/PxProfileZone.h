// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PROFILE_ZONE_H
#define PX_PROFILE_ZONE_H

#include "foundation/PxProfiler.h"
#include "foundation/PxFoundation.h"

#if PX_DEBUG || PX_CHECKED || PX_PROFILE
	#define PX_PROFILE_ZONE(x, y)										\
		physx::PxProfileScoped PX_CONCAT(_scoped, __LINE__)(PxGetProfilerCallback(), x, false, y)
	#define PX_PROFILE_START_CROSSTHREAD(x, y)							\
		if(PxGetProfilerCallback())										\
			PxGetProfilerCallback()->zoneStart(x, true, y)
	#define PX_PROFILE_STOP_CROSSTHREAD(x, y)							\
		if(PxGetProfilerCallback())										\
			PxGetProfilerCallback()->zoneEnd(NULL, x, true, y)
	#define PX_PROFILE_VALUE(x, y, z)									\
		if(PxGetProfilerCallback())										\
			PxGetProfilerCallback()->recordData(x, y, z)
	#define PX_PROFILE_FRAME(x, y)                                                                                                         \
		if(PxGetProfilerCallback())                                                                                                        \
			PxGetProfilerCallback()->recordFrame(x, y)
#else
	#define PX_PROFILE_ZONE(x, y)
	#define PX_PROFILE_START_CROSSTHREAD(x, y)
	#define PX_PROFILE_STOP_CROSSTHREAD(x, y)
	#define PX_PROFILE_VALUE(x, y, z)
	#define PX_PROFILE_FRAME(x, y)
#endif

#endif
