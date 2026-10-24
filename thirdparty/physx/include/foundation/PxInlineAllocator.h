// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_INLINE_ALLOCATOR_H
#define PX_INLINE_ALLOCATOR_H

#include "foundation/PxUserAllocated.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
// This is used by the array class to allocate some space for a small number of objects along with the metadata.
// You can allocate an additional N bytes (set with PxInlineAllocator::setExtraSize()) for e.g. safe SIMD loads.
// PT: TODO: merge with PxInlineArray.h
template <PxU32 N, typename BaseAllocator>
class PxInlineAllocator : private BaseAllocator
{
  public:
	PxInlineAllocator(const PxEMPTY v) : BaseAllocator(v)
	{
	}

	PxInlineAllocator(const BaseAllocator& alloc = BaseAllocator()) : BaseAllocator(alloc), mExtraSize(0), mBufferUsed(false)
	{
	}

	PxInlineAllocator(const PxInlineAllocator& alloc) : BaseAllocator(alloc), mExtraSize(alloc.mExtraSize), mBufferUsed(false)
	{
	}

	void* allocate(size_t size, const char* filename, PxI32 line, uint32_t* cookie=NULL)
	{
		PX_UNUSED(cookie);
		if(!mBufferUsed && size <= N)
		{
			mBufferUsed = true;
			return mBuffer;
		}
		return BaseAllocator::allocate(size + size_t(mExtraSize), filename, line);
	}

	void deallocate(void* ptr, uint32_t* cookie=NULL)
	{
		PX_UNUSED(cookie);
		if(ptr == mBuffer)
			mBufferUsed = false;
		else
			BaseAllocator::deallocate(ptr);
	}

	PX_FORCE_INLINE PxU8*	getInlineBuffer()				{ return mBuffer;			}
	PX_FORCE_INLINE bool	isBufferUsed()		const		{ return mBufferUsed;		}
	PX_FORCE_INLINE void	setExtraSize(PxU8 extraSize)	{ mExtraSize = extraSize;	}

  protected:
	PxU8	mBuffer[N];
	PxU8	mExtraSize;	// PT: user-controlled extra size for safe SIMD loads
	bool	mBufferUsed;
};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

