// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXC_H
#define PXC_H

#include "foundation/PxPreprocessor.h"

// define API function declaration
#if !defined PX_PHYSX_STATIC_LIB 
	#if PX_WINDOWS_FAMILY
		#if defined PX_PHYSX_COOKING_EXPORTS
			#define PX_PHYSX_COOKING_API __declspec(dllexport)
		#else
			#define PX_PHYSX_COOKING_API __declspec(dllimport)
		#endif
	#elif PX_UNIX_FAMILY
		#define PX_PHYSX_COOKING_API PX_UNIX_EXPORT
	#endif
#endif

#if !defined(PX_PHYSX_COOKING_API)
    #define PX_PHYSX_COOKING_API
#endif

#ifndef PX_C_EXPORT
	#define PX_C_EXPORT extern "C"
#endif

#endif
