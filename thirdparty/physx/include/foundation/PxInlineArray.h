// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_INLINE_ARRAY_H
#define PX_INLINE_ARRAY_H

#include "foundation/PxArray.h"
#include "foundation/PxInlineAllocator.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

// array that pre-allocates for N elements
template <typename T, uint32_t N, typename Alloc = typename PxAllocatorTraits<T>::Type>
class PxInlineArray : public PxArray<T, PxInlineAllocator<N * sizeof(T), Alloc> >
{
	typedef PxInlineAllocator<N * sizeof(T), Alloc> Allocator;

  public:
	PxInlineArray(const PxEMPTY v) : PxArray<T, Allocator>(v)
	{
		if(isInlined())
			this->mData = reinterpret_cast<T*>(PxArray<T, Allocator>::getInlineBuffer());
	}

	PX_INLINE bool isInlined() const
	{
		return Allocator::isBufferUsed();
	}

	PX_INLINE void setExtraSize(PxU8 extraSize)
	{
		Allocator::setExtraSize(extraSize);
	}

	PX_INLINE explicit PxInlineArray(const Alloc& alloc = Alloc()) : PxArray<T, Allocator>(alloc)
	{
		this->mData = this->allocate(N);
		this->mCapacity = N;
	}
};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

