// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PXFOUNDATION_PXUNIXINTRINSICS_H
#define PXFOUNDATION_PXUNIXINTRINSICS_H

#include "foundation/PxAssert.h"

#if !(PX_LINUX || PX_APPLE_FAMILY)
#error "This file should only be included by Unix builds!!"
#endif

#if PX_LINUX && !PX_CUDA_COMPILER && !PX_EMSCRIPTEN
    // Linux and CUDA compilation does not work with std::isfnite, as it is not marked as CUDA callable
    #include <cmath>
    #ifndef isfinite
        using std::isfinite;
    #endif
#endif

#include <math.h>
#include <float.h>

#if !PX_DOXYGEN
namespace physx
{
#endif

namespace intrinsics
{
//! \brief platform-specific absolute value
PX_CUDA_CALLABLE PX_FORCE_INLINE float abs(float a)
{
	return ::fabsf(a);
}

//! \brief platform-specific select float
PX_CUDA_CALLABLE PX_FORCE_INLINE float fsel(float a, float b, float c)
{
	return (a >= 0.0f) ? b : c;
}

//! \brief platform-specific sign
PX_CUDA_CALLABLE PX_FORCE_INLINE float sign(float a)
{
	return (a >= 0.0f) ? 1.0f : -1.0f;
}

//! \brief platform-specific reciprocal
PX_CUDA_CALLABLE PX_FORCE_INLINE float recip(float a)
{
	return 1.0f / a;
}

//! \brief platform-specific reciprocal estimate
PX_CUDA_CALLABLE PX_FORCE_INLINE float recipFast(float a)
{
	return 1.0f / a;
}

//! \brief platform-specific square root
PX_CUDA_CALLABLE PX_FORCE_INLINE float sqrt(float a)
{
	return ::sqrtf(a);
}

//! \brief platform-specific reciprocal square root
PX_CUDA_CALLABLE PX_FORCE_INLINE float recipSqrt(float a)
{
	return 1.0f / ::sqrtf(a);
}

PX_CUDA_CALLABLE PX_FORCE_INLINE float recipSqrtFast(float a)
{
	return 1.0f / ::sqrtf(a);
}

//! \brief platform-specific sine
PX_CUDA_CALLABLE PX_FORCE_INLINE float sin(float a)
{
	return ::sinf(a);
}

//! \brief platform-specific cosine
PX_CUDA_CALLABLE PX_FORCE_INLINE float cos(float a)
{
	return ::cosf(a);
}

//! \brief platform-specific minimum
PX_CUDA_CALLABLE PX_FORCE_INLINE float selectMin(float a, float b)
{
	return a < b ? a : b;
}

//! \brief platform-specific maximum
PX_CUDA_CALLABLE PX_FORCE_INLINE float selectMax(float a, float b)
{
	return a > b ? a : b;
}

//! \brief platform-specific finiteness check (not INF or NAN)
PX_CUDA_CALLABLE PX_FORCE_INLINE bool isFinite(float a)
{
	//std::isfinite not recommended as of Feb 2017, since it doesn't work with g++/clang's floating point optimization.
    union localU { PxU32 i; float f; } floatUnion;
    floatUnion.f = a;
    return !((floatUnion.i & 0x7fffffff) >= 0x7f800000);
}

//! \brief platform-specific finiteness check (not INF or NAN)
PX_CUDA_CALLABLE PX_FORCE_INLINE bool isFinite(double a)
{
	return !!isfinite(a);
}

/*!
Sets \c count bytes starting at \c dst to zero.
*/
PX_FORCE_INLINE void* memZero(void* dest, size_t count)
{
	return memset(dest, 0, count);
}

/*!
Sets \c count bytes starting at \c dst to \c c.
*/
PX_FORCE_INLINE void* memSet(void* dest, int32_t c, size_t count)
{
	return memset(dest, c, count);
}

/*!
Copies \c count bytes from \c src to \c dst. User memMove if regions overlap.
*/
PX_FORCE_INLINE void* memCopy(void* dest, const void* src, size_t count)
{
	return memcpy(dest, src, count);
}

/*!
Copies \c count bytes from \c src to \c dst. Supports overlapping regions.
*/
PX_FORCE_INLINE void* memMove(void* dest, const void* src, size_t count)
{
	return memmove(dest, src, count);
}

} // namespace intrinsics
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
