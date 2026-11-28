// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_FOUNDATION_CONFIG_H
#define PX_FOUNDATION_CONFIG_H

#include "foundation/PxPreprocessor.h"


#if defined PX_PHYSX_STATIC_LIB
	#define PX_FOUNDATION_API
#else
	#if PX_WINDOWS_FAMILY && !PX_CUDA_COMPILER
		#if defined PX_PHYSX_FOUNDATION_EXPORTS
			#define PX_FOUNDATION_API __declspec(dllexport)
		#else
			#define PX_FOUNDATION_API __declspec(dllimport)
		#endif
	#elif PX_UNIX_FAMILY
		#define PX_FOUNDATION_API PX_UNIX_EXPORT
	#else
		#define PX_FOUNDATION_API
	#endif
#endif 


#endif 
