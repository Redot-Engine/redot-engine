// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_WINDOWS_MATH_INTRINSICS_H
#define PX_WINDOWS_MATH_INTRINSICS_H

#include "foundation/PxAssert.h"

#if !PX_WINDOWS_FAMILY
#error "This file should only be included by Windows builds!!"
#endif

#include <math.h>
#include <float.h>

#if !PX_DOXYGEN
namespace physx
{
namespace intrinsics
{
#endif

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

//! \brief platform-specific reciprocal square root estimate
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
#if PX_CUDA_COMPILER
	return !!isfinite(a);
#else
	return (0 == ((_FPCLASS_SNAN | _FPCLASS_QNAN | _FPCLASS_NINF | _FPCLASS_PINF) & _fpclass(a)));
#endif
}

//! \brief platform-specific finiteness check (not INF or NAN)
PX_CUDA_CALLABLE PX_FORCE_INLINE bool isFinite(double a)
{
#if PX_CUDA_COMPILER
	return !!isfinite(a);
#else
	return (0 == ((_FPCLASS_SNAN | _FPCLASS_QNAN | _FPCLASS_NINF | _FPCLASS_PINF) & _fpclass(a)));
#endif
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

#if !PX_DOXYGEN
} // namespace intrinsics
} // namespace physx
#endif

#endif

