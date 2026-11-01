// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_UNION_CAST_H
#define PX_UNION_CAST_H

#include "foundation/PxPreprocessor.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

// Needed for clang 7
#if PX_CLANG && PX_CLANG_MAJOR >= 7
 #define USE_VOLATILE_UNION volatile 
#else
 #define USE_VOLATILE_UNION
#endif

template <class A, class B>
PX_FORCE_INLINE A PxUnionCast(B b)
{
	union AB
	{
		AB(B bb) : _b(bb)
		{
		}
		 B _b;
		 A _a;
	} USE_VOLATILE_UNION u(b);
	return u._a;
}

#undef USE_VOLATILE_UNION

#if !PX_DOXYGEN
} // namespace physx
#endif


#endif

