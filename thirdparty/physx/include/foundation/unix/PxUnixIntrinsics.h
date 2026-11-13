// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PSFOUNDATION_PSUNIXINTRINSICS_H
#define PSFOUNDATION_PSUNIXINTRINSICS_H

#include "foundation/PxAssert.h"
#include <math.h>

// this file is for internal intrinsics - that is, intrinsics that are used in
// cross platform code but do not appear in the API

#if !(PX_LINUX || PX_APPLE_FAMILY)
#error "This file should only be included by unix builds!!"
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

PX_FORCE_INLINE void PxMemoryBarrier()
{
	__sync_synchronize();
}

/*!
Return the index of the highest set bit. Undefined for zero arg.
*/
PX_INLINE uint32_t PxHighestSetBitUnsafe(uint64_t v)
{
	return uint32_t(63 - __builtin_clzl(v));
}

/*!
Return the index of the highest set bit. Undefined for zero arg.
*/
PX_INLINE uint32_t PxHighestSetBitUnsafe(uint32_t v)
{
	return uint32_t(31 - __builtin_clz(v));
}

/*!
Return the index of the lowest set bit. Undefined for zero arg.
*/
PX_INLINE uint32_t PxLowestSetBitUnsafe(uint64_t v)
{
	return uint32_t(__builtin_ctzl(v));
}

/*!
Return the index of the lowest set bit. Undefined for zero arg.
*/
PX_INLINE uint32_t PxLowestSetBitUnsafe(uint32_t v)
{
	return uint32_t(__builtin_ctz(v));
}

/*!
Returns the index of the highest set bit. Returns 32 for v=0.
*/
PX_INLINE uint32_t PxCountLeadingZeros(uint32_t v)
{
	if(v)
		return uint32_t(__builtin_clz(v));
	else
		return 32u;
}

/*!
Prefetch aligned 64B x86, 32b ARM around \c ptr+offset.
*/
PX_FORCE_INLINE void PxPrefetchLine(const void* ptr, uint32_t offset = 0)
{
#if PX_CUDA_COMPILER
	__builtin_prefetch(reinterpret_cast<const char*>(ptr) + offset, 0, 3);
#else
	__builtin_prefetch(reinterpret_cast<const char* PX_RESTRICT>(ptr) + offset, 0, 3);
#endif
}

/*!
Prefetch \c count bytes starting at \c ptr.
*/
PX_FORCE_INLINE void PxPrefetch(const void* ptr, uint32_t count = 1)
{
	const char* cp = reinterpret_cast<const char*>(ptr);
	uint64_t p = size_t(ptr);
	uint64_t startLine = p >> 6, endLine = (p + count - 1) >> 6;
	uint64_t lines = endLine - startLine + 1;
	do
	{
		PxPrefetchLine(cp);
		cp += 64;
	} while(--lines);
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
