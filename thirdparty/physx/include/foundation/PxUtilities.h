// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_UTILITIES_H
#define PX_UTILITIES_H

#include "foundation/PxVec3.h"
#include "foundation/PxAssert.h"
#include "foundation/PxIntrinsics.h"
#include "foundation/PxBasicTemplates.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
PX_INLINE char PxLittleEndian()
{
	int i = 1;
	return *(reinterpret_cast<char*>(&i));
}

// PT: checked casts
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 PxTo32(PxU64 value)
{
	PX_ASSERT(value <= 0xffffffff);
	return PxU32(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU32 PxToU32(PxI32 value)
{
	PX_ASSERT(value >= 0);
	return PxU32(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU16 PxTo16(PxU32 value)
{
	PX_ASSERT(value <= 0xffff);
	return PxU16(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU8 PxTo8(PxU16 value)
{
	PX_ASSERT(value <= 0xff);
	return PxU8(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU8 PxTo8(PxU32 value)
{
	PX_ASSERT(value <= 0xff);
	return PxU8(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxU8 PxTo8(PxI32 value)
{
	PX_ASSERT(value <= 0xff);
	PX_ASSERT(value >= 0);
	return PxU8(value);
}
PX_CUDA_CALLABLE PX_FORCE_INLINE PxI8 PxToI8(PxU32 value)
{
	PX_ASSERT(value <= 0x7f);
	return PxI8(value);
}

//! \cond
/*!
Get number of elements in array
*/
template <typename T, size_t N>
char (&PxArraySizeHelper(T (&array)[N]))[N];
#define PX_ARRAY_SIZE(_array) (sizeof(physx::PxArraySizeHelper(_array)))
//! \endcond

/*!
Sort two elements using operator<

On return x will be the smaller of the two
*/
template <class T>
PX_CUDA_CALLABLE PX_FORCE_INLINE void PxOrder(T& x, T& y)
{
	if(y < x)
		PxSwap(x, y);
}

// most architectures can do predication on real comparisons, and on VMX, it matters

PX_CUDA_CALLABLE PX_FORCE_INLINE void PxOrder(PxReal& x, PxReal& y)
{
	PxReal newX = PxMin(x, y);
	PxReal newY = PxMax(x, y);
	x = newX;
	y = newY;
}

/*!
Sort two elements using operator< and also keep order
of any extra data
*/
template <class T, class E1>
PX_CUDA_CALLABLE PX_FORCE_INLINE void PxOrder(T& x, T& y, E1& xe1, E1& ye1)
{
	if(y < x)
	{
		swap(x, y);
		swap(xe1, ye1);
	}
}

#if PX_GCC_FAMILY && !PX_EMSCRIPTEN
__attribute__((noreturn))
#endif
    PX_INLINE void PxDebugBreak()
{
#if PX_WINDOWS
	__debugbreak();
#elif PX_LINUX
	__builtin_trap();
#elif PX_GCC_FAMILY
	__builtin_trap();
#else
	PX_ASSERT(false);
#endif
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

