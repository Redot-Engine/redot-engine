// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_POOL_H
#define PX_POOL_H

#include "foundation/PxArray.h"
#include "foundation/PxSort.h"
#include "foundation/PxBasicTemplates.h"
#include "foundation/PxInlineArray.h"
#include "foundation/PxMemory.h"

namespace physx
{

/*!
Simple allocation pool
*/
template <class T, class Alloc = typename PxAllocatorTraits<T>::Type>
class PxPoolBase : public PxUserAllocated, public Alloc
{
	PX_NOCOPY(PxPoolBase)
  protected:
	PxPoolBase(const Alloc& alloc, uint32_t elementsPerSlab, uint32_t slabSize)
	: Alloc(alloc), mSlabs(alloc), mElementsPerSlab(elementsPerSlab), mUsed(0), mSlabSize(slabSize), mFreeElement(0)
	{
		mSlabs.reserve(64);
		PX_COMPILE_TIME_ASSERT(sizeof(T) >= sizeof(size_t));
	}

  public:
	~PxPoolBase()
	{
		if(mUsed)
			disposeElements();

		for(void** slabIt = mSlabs.begin(), *slabEnd = mSlabs.end(); slabIt != slabEnd; ++slabIt)
			Alloc::deallocate(*slabIt);
	}

	// Allocate space for single object
	PX_INLINE T* allocate()
	{
		if(mFreeElement == 0)
			allocateSlab();
		T* p = reinterpret_cast<T*>(mFreeElement);
		mFreeElement = mFreeElement->mNext;
		mUsed++;

		PxMarkSerializedMemory(p, sizeof(T));
		return p;
	}

	// Put space for a single element back in the lists
	PX_INLINE void deallocate(T* p)
	{
		if(p)
		{
			PX_ASSERT(mUsed);
			mUsed--;
			push(reinterpret_cast<FreeList*>(p));
		}
	}

	PX_INLINE T* construct()
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T()) : NULL;
	}

	template <class A1>
	PX_INLINE T* construct(A1& a)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a)) : NULL;
	}

	template <class A1, class A2>
	PX_INLINE T* construct(A1& a, A2& b)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b)) : NULL;
	}

	template <class A1, class A2, class A3>
	PX_INLINE T* construct(A1& a, A2& b, A3& c)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c)) : NULL;
	}

	template <class A1, class A2, class A3>
	PX_INLINE T* construct(A1* a, A2& b, A3& c)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c)) : NULL;
	}

	template <class A1, class A2, class A3, class A4>
	PX_INLINE T* construct(A1& a, A2& b, A3& c, A4& d)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d)) : NULL;
	}

	template <class A1, class A2, class A3, class A4, class A5>
	PX_INLINE T* construct(A1& a, A2& b, A3& c, A4& d, A5& e)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d, e)) : NULL;
	}

	template <class A1, class A2, class A3, class A4, class A5, class A6>
	PX_INLINE T* construct(A1& a, A2& b, A3& c, A4& d, A5& e, A6& f)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d, e, f)) : NULL;
	}

	template <class A1, class A2, class A3, class A4, class A5, class A6>
	PX_INLINE T* construct(const A1& a, A2& b, const A3& c, A4& d, A5& e, A6& f)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d, e, f)) : NULL;
	}
	
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7>
	PX_INLINE T* construct(A1& a, A2& b, A3& c, A4& d, A5& e, A6& f, A7& g)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d, e, f, g)) : NULL;
	}
	
	template <class A1, class A2, class A3, class A4, class A5, class A6, class A7, class A8>
	PX_INLINE T* construct(A1& a, A2& b, A3& c, A4& d, A5& e, A6& f, A7& g, A8& h)
	{
		T* t = allocate();
		return t ? PX_PLACEMENT_NEW(t, T(a, b, c, d, e, f, g, h)) : NULL;
	}

	PX_INLINE void destroy(T* const p)
	{
		if(p)
		{
			p->~T();
			deallocate(p);
		}
	}

  protected:
	struct FreeList
	{
		FreeList* mNext;
	};

	// All the allocated slabs, sorted by pointer
	PxArray<void*, Alloc> mSlabs;

	const uint32_t mElementsPerSlab;
	uint32_t mUsed;
	const uint32_t mSlabSize;

	FreeList* mFreeElement; // Head of free-list

	// Helper function to get bitmap of allocated elements

	void push(FreeList* p)
	{
		p->mNext = mFreeElement;
		mFreeElement = p;
	}

	// Allocate a slab and segregate it into the freelist
	void allocateSlab()
	{
		T* slab = reinterpret_cast<T*>(Alloc::allocate(mSlabSize, PX_FL));

		mSlabs.pushBack(slab);

		// Build a chain of nodes for the freelist
		T* it = slab + mElementsPerSlab;
		while(--it >= slab)
			push(reinterpret_cast<FreeList*>(it));
	}

	/*
	Cleanup method. Go through all active slabs and call destructor for live objects,
	then free their memory
	*/
	void disposeElements()
	{
		PxArray<void*, Alloc> freeNodes(*this);
		while(mFreeElement)
		{
			freeNodes.pushBack(mFreeElement);
			mFreeElement = mFreeElement->mNext;
		}
		Alloc& alloc(*this);
		PxSort(freeNodes.begin(), freeNodes.size(), PxLess<void*>(), alloc);
		PxSort(mSlabs.begin(), mSlabs.size(), PxLess<void*>(), alloc);

		typename PxArray<void*, Alloc>::Iterator slabIt = mSlabs.begin(), slabEnd = mSlabs.end();
		for(typename PxArray<void*, Alloc>::Iterator freeIt = freeNodes.begin(); slabIt != slabEnd; ++slabIt)
		{
			for(T* tIt = reinterpret_cast<T*>(*slabIt), *tEnd = tIt + mElementsPerSlab; tIt != tEnd; ++tIt)
			{
				if(freeIt != freeNodes.end() && *freeIt == tIt)
					++freeIt;
				else
					tIt->~T();
			}
		}
	}
};

// original pool implementation
template <class T, class Alloc = typename PxAllocatorTraits<T>::Type>
class PxPool : public PxPoolBase<T, Alloc>
{
  public:
	PxPool(const Alloc& alloc = Alloc(), uint32_t elementsPerSlab = 32)
	: PxPoolBase<T, Alloc>(alloc, elementsPerSlab, elementsPerSlab * sizeof(T))
	{
	}
};

// allows specification of the slab size instead of the occupancy
template <class T, uint32_t slabSize, class Alloc = typename PxAllocatorTraits<T>::Type>
class PxPool2 : public PxPoolBase<T, Alloc>
{
  public:
	PxPool2(const Alloc& alloc = Alloc()) : PxPoolBase<T, Alloc>(alloc, slabSize / sizeof(T), slabSize)
	{
		PX_COMPILE_TIME_ASSERT(slabSize > sizeof(T));
	}
};

} // namespace physx

#endif
