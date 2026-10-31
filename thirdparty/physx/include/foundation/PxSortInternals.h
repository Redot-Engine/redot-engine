// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SORT_INTERNALS_H
#define PX_SORT_INTERNALS_H


#include "foundation/PxAssert.h"
#include "foundation/PxMathIntrinsics.h"
#include "foundation/PxBasicTemplates.h"
#include "foundation/PxUserAllocated.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
template <class T, class Predicate>
PX_INLINE void PxMedian3(T* elements, int32_t first, int32_t last, Predicate& compare)
{
	/*
	This creates sentinels because we know there is an element at the start minimum(or equal)
	than the pivot and an element at the end greater(or equal) than the pivot. Plus the
	median of 3 reduces the chance of degenerate behavour.
	*/

	int32_t mid = (first + last) / 2;

	if(compare(elements[mid], elements[first]))
		PxSwap(elements[first], elements[mid]);

	if(compare(elements[last], elements[first]))
		PxSwap(elements[first], elements[last]);

	if(compare(elements[last], elements[mid]))
		PxSwap(elements[mid], elements[last]);

	// keep the pivot at last-1
	PxSwap(elements[mid], elements[last - 1]);
}

template <class T, class Predicate>
PX_INLINE int32_t PxPartition(T* elements, int32_t first, int32_t last, Predicate& compare)
{
	PxMedian3(elements, first, last, compare);

	/*
	WARNING: using the line:

	T partValue = elements[last-1];

	and changing the scan loops to:

	while(comparator.greater(partValue, elements[++i]));
	while(comparator.greater(elements[--j], partValue);

	triggers a compiler optimizer bug on xenon where it stores a double to the stack for partValue
	then loads it as a single...:-(
	*/

	int32_t i = first;    // we know first is less than pivot(but i gets pre incremented)
	int32_t j = last - 1; // pivot is in last-1 (but j gets pre decremented)

	for(;;)
	{
		while(compare(elements[++i], elements[last - 1]))
			;
		while(compare(elements[last - 1], elements[--j]))
			;

		if(i >= j)
			break;

		PX_ASSERT(i <= last && j >= first);
		PxSwap(elements[i], elements[j]);
	}
	// put the pivot in place

	PX_ASSERT(i <= last && first <= (last - 1));
	PxSwap(elements[i], elements[last - 1]);

	return i;
}

template <class T, class Predicate>
PX_INLINE void PxSmallSort(T* elements, int32_t first, int32_t last, Predicate& compare)
{
	// selection sort - could reduce to fsel on 360 with floats.

	for(int32_t i = first; i < last; i++)
	{
		int32_t m = i;
		for(int32_t j = i + 1; j <= last; j++)
			if(compare(elements[j], elements[m]))
				m = j;

		if(m != i)
			PxSwap(elements[m], elements[i]);
	}
}

template <class PxAllocator>
class PxStack
{
	PxAllocator mAllocator;
	uint32_t mSize, mCapacity;
	int32_t* mMemory;
	bool mRealloc;

  public:
	PxStack(int32_t* memory, uint32_t capacity, const PxAllocator& inAllocator)
	: mAllocator(inAllocator), mSize(0), mCapacity(capacity), mMemory(memory), mRealloc(false)
	{
	}
	~PxStack()
	{
		if(mRealloc)
			mAllocator.deallocate(mMemory);
	}

	void grow()
	{
		mCapacity *= 2;
		int32_t* newMem =
		    reinterpret_cast<int32_t*>(mAllocator.allocate(sizeof(int32_t) * mCapacity, PX_FL));
		intrinsics::memCopy(newMem, mMemory, mSize * sizeof(int32_t));
		if(mRealloc)
			mAllocator.deallocate(mMemory);
		mRealloc = true;
		mMemory = newMem;
	}

	PX_INLINE void push(int32_t start, int32_t end)
	{
		if(mSize >= mCapacity - 1)
			grow();
		mMemory[mSize++] = start;
		mMemory[mSize++] = end;
	}

	PX_INLINE void pop(int32_t& start, int32_t& end)
	{
		PX_ASSERT(!empty());
		end = mMemory[--mSize];
		start = mMemory[--mSize];
	}

	PX_INLINE bool empty()
	{
		return mSize == 0;
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif
#endif

