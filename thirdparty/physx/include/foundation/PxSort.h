// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SORT_H
#define PX_SORT_H


#include "foundation/PxSortInternals.h"
#include "foundation/PxAlloca.h"

#define PX_SORT_PARANOIA PX_DEBUG

/**
\brief Sorts an array of objects in ascending order, assuming
that the predicate implements the < operator:

\see PxLess, PxGreater
*/

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4706) // disable the warning that we did an assignment within a conditional expression, as
// this was intentional.
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif
template <class T, class Predicate, class PxAllocator>
void PxSort(T* elements, uint32_t count, const Predicate& compare, const PxAllocator& inAllocator,
          const uint32_t initialStackSize = 32)
{
	static const uint32_t SMALL_SORT_CUTOFF = 5; // must be >= 3 since we need 3 for median

	PX_ALLOCA(stackMem, int32_t, initialStackSize);
	PxStack<PxAllocator> stack(stackMem, initialStackSize, inAllocator);

	int32_t first = 0, last = int32_t(count - 1);
	if(last > first)
	{
		for(;;)
		{
			while(last > first)
			{
				PX_ASSERT(first >= 0 && last < int32_t(count));
				if(uint32_t(last - first) < SMALL_SORT_CUTOFF)
				{
					PxSmallSort(elements, first, last, compare);
					break;
				}
				else
				{
					const int32_t partIndex = PxPartition(elements, first, last, compare);

					// push smaller sublist to minimize stack usage
					if((partIndex - first) < (last - partIndex))
					{
						stack.push(first, partIndex - 1);
						first = partIndex + 1;
					}
					else
					{
						stack.push(partIndex + 1, last);
						last = partIndex - 1;
					}
				}
			}

			if(stack.empty())
				break;

			stack.pop(first, last);
		}
	}
#if PX_SORT_PARANOIA
	for(uint32_t i = 1; i < count; i++)
		PX_ASSERT(!compare(elements[i], elements[i - 1]));
#endif
}

template <class T, class Predicate>
void PxSort(T* elements, uint32_t count, const Predicate& compare)
{
	PxSort(elements, count, compare, typename PxAllocatorTraits<T>::Type());
}

template <class T>
void PxSort(T* elements, uint32_t count)
{
	PxSort(elements, count, PxLess<T>(), typename PxAllocatorTraits<T>::Type());
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#if PX_VC
#pragma warning(pop)
#endif
#endif

