// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSX_COMMON_CONFIG_H
#define PX_PHYSX_COMMON_CONFIG_H

#include "foundation/PxSimpleTypes.h"

#if PX_DEBUG
    //Fills almost all allocated (host and device memory) with 0xcdcdcdcd (=3452816845)
    #define PX_STOMP_ALLOCATED_MEMORY 1
#else
    #define PX_STOMP_ALLOCATED_MEMORY 0
#endif

/*Disable support for VS2017 prior version 15.5.1 for windows platform, because of a compiler bug:
https://developercommunity.visualstudio.com/content/problem/66047/possible-compiler-bug.html
*/
#if (PX_VC == 15) && PX_WINDOWS && (_MSC_FULL_VER < 191225830)
#error Visual studio 2017 prior to 15.5.1 is not supported because of a compiler bug.
#endif

// define API function declaration (public API only needed because of extensions)
#if defined PX_PHYSX_STATIC_LIB
	#define PX_PHYSX_CORE_API
#else
	#if PX_WINDOWS_FAMILY
		#if defined PX_PHYSX_CORE_EXPORTS
			#define PX_PHYSX_CORE_API __declspec(dllexport)
		#else
			#define PX_PHYSX_CORE_API __declspec(dllimport)
		#endif
	#elif PX_UNIX_FAMILY
		#define PX_PHYSX_CORE_API PX_UNIX_EXPORT
    #else
		#define PX_PHYSX_CORE_API
    #endif
#endif

#if PX_SUPPORT_GPU_PHYSX
// define API function declaration
#if defined PX_PHYSX_GPU_STATIC
	#define PX_PHYSX_GPU_API
#else
	#if PX_WINDOWS 
	#if defined PX_PHYSX_GPU_EXPORTS
	#define PX_PHYSX_GPU_API __declspec(dllexport)
	#else
	#define PX_PHYSX_GPU_API __declspec(dllimport)
	#endif
	#elif PX_UNIX_FAMILY
	#define PX_PHYSX_GPU_API PX_UNIX_EXPORT
	#else
	#define PX_PHYSX_GPU_API
	#endif
#endif

#else // PX_SUPPORT_GPU_PHYSX
#define PX_PHYSX_GPU_API
#endif // PX_SUPPORT_GPU_PHYSX

#if defined PX_PHYSX_STATIC_LIB
	#define PX_PHYSX_COMMON_API
#else
	#if PX_WINDOWS_FAMILY && !PX_CUDA_COMPILER
		#if defined PX_PHYSX_COMMON_EXPORTS
			#define PX_PHYSX_COMMON_API __declspec(dllexport)
		#else
			#define PX_PHYSX_COMMON_API __declspec(dllimport)
		#endif
	#elif PX_UNIX_FAMILY
		#define PX_PHYSX_COMMON_API PX_UNIX_EXPORT
	#else
		#define PX_PHYSX_COMMON_API
	#endif
#endif 

// PT: typical "invalid" value in various CD algorithms
#define	PX_INVALID_U32		0xffffffff
#define PX_INVALID_U16		0xffff

// Changing these parameters requires recompilation of the SDK

// Enable debug visualization
#define PX_ENABLE_DEBUG_VISUALIZATION	1
#define PX_CATCH_UNDEFINED_ENABLE_DEBUG_VISUALIZATION

// Enable simulation statistics generation
#define PX_ENABLE_SIM_STATS 1
#define PX_CATCH_UNDEFINED_ENABLE_SIM_STATS

#if !PX_DOXYGEN
namespace physx
{
#endif
	typedef PxU32 PxTriangleID;
	typedef PxU16 PxMaterialTableIndex;
	typedef PxU16 PxDeformableMaterialTableIndex;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
