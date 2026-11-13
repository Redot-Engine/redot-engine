// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_BIT_UTILS_H
#define PX_BIT_UTILS_H

#include "foundation/PxMathIntrinsics.h"
#include "foundation/PxAssert.h"
#include "foundation/PxIntrinsics.h"
#include "foundation/PxMathIntrinsics.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
PX_INLINE uint32_t PxBitCount(uint32_t v)
{
	// from http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
	uint32_t const w = v - ((v >> 1) & 0x55555555);
	uint32_t const x = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	return (((x + (x >> 4)) & 0xF0F0F0F) * 0x1010101) >> 24;
}

PX_INLINE bool PxIsPowerOfTwo(uint32_t x)
{
	return x != 0 && (x & (x - 1)) == 0;
}

// "Next Largest Power of 2
// Given a binary integer value x, the next largest power of 2 can be computed by a SWAR algorithm
// that recursively "folds" the upper bits into the lower bits. This process yields a bit vector with
// the same most significant 1 as x, but all 1's below it. Adding 1 to that value yields the next
// largest power of 2. For a 32-bit value:"
PX_INLINE uint32_t PxNextPowerOfTwo(uint32_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return x + 1;
}

// "Next Largest Power of 2
// Given a binary integer value x, the next largest power of 2 can be computed by a SWAR algorithm
// that recursively "folds" the upper bits into the lower bits. This process yields a bit vector with
// the same most significant 1 as x, but all 1's below it. Adding 1 to that value yields the next
// largest power of 2. For a 64-bit value:"
PX_INLINE uint64_t PxNextPowerOfTwo(uint64_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	x |= (x >> 32);
	return x + 1;
}

/*!
Return the index of the lowest set bit. Not valid for zero arg.
*/

PX_INLINE uint32_t PxLowestSetBit(uint32_t x)
{
	PX_ASSERT(x);
	return PxLowestSetBitUnsafe(x);
}

/*!
Return the index of the lowest set bit. Not valid for zero arg.
*/

PX_INLINE uint32_t PxLowestSetBit(uint64_t x)
{
	PX_ASSERT(x);
	return PxLowestSetBitUnsafe(x);
}

/*!
Return the index of the highest set bit. Not valid for zero arg.
*/

PX_INLINE uint32_t PxHighestSetBit(uint32_t x)
{
	PX_ASSERT(x);
	return PxHighestSetBitUnsafe(x);
}

/*!
Return the index of the highest set bit. Not valid for zero arg.
*/
PX_INLINE uint32_t PxHighestSetBit(uint64_t x)
{
	PX_ASSERT(x);
	return PxHighestSetBitUnsafe(x);
}

// Helper function to approximate log2 of an integer value
// assumes that the input is actually power of two.
PX_INLINE uint32_t PxILog2(uint32_t num)
{
	for(uint32_t i = 0; i < 32; i++)
	{
		num >>= 1;
		if(num == 0)
			return i;
	}

	PX_ASSERT(0);
	return uint32_t(-1);
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

