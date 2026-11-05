// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ASSERT_H
#define PX_ASSERT_H

#include <stdint.h>
#include "foundation/PxFoundationConfig.h"

#if PX_CUDA_COMPILER
#include <assert.h>
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
 * \brief  Built-in assert function
 */
PX_FOUNDATION_API void PxAssert(const char* exp, const char* file, int line, bool& ignore);

#if !PX_ENABLE_ASSERTS
	#define PX_ASSERT(exp) ((void)0)
	#define PX_ALWAYS_ASSERT_MESSAGE(exp) ((void)0)
	#define PX_ASSERT_WITH_MESSAGE(condition, message) ((void)0)
#else
#if PX_VC
	#define PX_CODE_ANALYSIS_ASSUME(exp)	\
		__analysis_assume(!!(exp)) // This macro will be used to get rid of analysis warning messages if a PX_ASSERT is used
	// to "guard" illegal mem access, for example.
#else
	#define PX_CODE_ANALYSIS_ASSUME(exp)
#endif
#if PX_CUDA_COMPILER
	#define PX_ASSERT(exp)																			\
		{																							\
			assert(exp);																			\
		}
	#define PX_ALWAYS_ASSERT_MESSAGE PX_ASSERT
	#define PX_ASSERT_WITH_MESSAGE(exp, message)													\
		{																							\
			assert(exp);																			\
		}
#else
	#define PX_ASSERT(exp)																			\
		{																							\
			static bool _ignore = false;															\
			((void)((!!(exp)) || (!_ignore && (physx::PxAssert(#exp, PX_FL, _ignore), false))));	\
			PX_CODE_ANALYSIS_ASSUME(exp);															\
		}
	#define PX_ALWAYS_ASSERT_MESSAGE(exp)															\
		{																							\
			static bool _ignore = false;															\
			if(!_ignore)																			\
				physx::PxAssert(exp, PX_FL, _ignore);												\
		}
	#define PX_ASSERT_WITH_MESSAGE(exp, message)													\
		{																							\
			static bool _ignore = false;															\
			((void)((!!(exp)) || (!_ignore && (physx::PxAssert(message, PX_FL, _ignore), false))));	\
			PX_CODE_ANALYSIS_ASSUME(exp);															\
		}
#endif
#endif // !PX_ENABLE_ASSERTS

#define PX_ALWAYS_ASSERT() PX_ASSERT(0)

#if !PX_DOXYGEN
} // namespace physx
#endif


#endif

