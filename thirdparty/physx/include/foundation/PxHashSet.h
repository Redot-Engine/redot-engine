// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_HASHSET_H
#define PX_HASHSET_H

#include "foundation/PxHashInternals.h"

// TODO: make this doxy-format

// This header defines two hash sets. Hash sets
// * support custom initial table sizes (rounded up internally to power-of-2)
// * support custom static allocator objects
// * auto-resize, based on a load factor (i.e. a 64-entry .75 load factor hash will resize
//                                        when the 49th element is inserted)
// * are based on open hashing
//
// Sets have STL-like copying semantics, and properly initialize and destruct copies of objects
//
// There are two forms of set: coalesced and uncoalesced. Coalesced sets keep the entries in the
// initial segment of an array, so are fast to iterate over; however deletion is approximately
// twice as expensive.
//
// HashSet<T>:
//		bool		insert(const T& k)						amortized O(1) (exponential resize policy)
// 		bool		contains(const T& k)	const;			O(1)
//		bool		erase(const T& k);						O(1)
//		uint32_t		size()					const;			constant
//		bool		reserve(uint32_t size);					O(MAX(size, currentOccupancy))
//		void		clear();								O(currentOccupancy) (with zero constant for objects without
// destructors)
//      Iterator    getIterator();
//
// Use of iterators:
//
// for(HashSet::Iterator iter = test.getIterator(); !iter.done(); ++iter)
//			myFunction(*iter);
//
// CoalescedHashSet<T> does not support getIterator, but instead supports
// 		const Key *getEntries();
//
// insertion into a set already containing the element fails returning false, as does
// erasure of an element not in the set
//

#if !PX_DOXYGEN
namespace physx
{
#endif
template <class Key, class HashFn = PxHash<Key>, class Allocator = PxAllocator>
class PxHashSet : public physx::PxHashSetBase<Key, HashFn, Allocator, false>
{
  public:
	typedef physx::PxHashSetBase<Key, HashFn, Allocator, false> HashSetBase;
	typedef typename HashSetBase::Iterator Iterator;

	PxHashSet(uint32_t initialTableSize = 64, float loadFactor = 0.75f) : HashSetBase(initialTableSize, loadFactor)
	{
	}
	PxHashSet(uint32_t initialTableSize, float loadFactor, const Allocator& alloc)
	: HashSetBase(initialTableSize, loadFactor, alloc)
	{
	}
	PxHashSet(const Allocator& alloc) : HashSetBase(64, 0.75f, alloc)
	{
	}
	Iterator getIterator()
	{
		return Iterator(HashSetBase::mBase);
	}
};

template <class Key, class HashFn = PxHash<Key>, class Allocator = PxAllocator>
class PxCoalescedHashSet : public physx::PxHashSetBase<Key, HashFn, Allocator, true>
{
  public:
	typedef typename physx::PxHashSetBase<Key, HashFn, Allocator, true> HashSetBase;

	PxCoalescedHashSet(uint32_t initialTableSize = 64, float loadFactor = 0.75f)
	: HashSetBase(initialTableSize, loadFactor)
	{
	}

	PxCoalescedHashSet(uint32_t initialTableSize, float loadFactor, const Allocator& alloc)
	: HashSetBase(initialTableSize, loadFactor, alloc)
	{
	}
	PxCoalescedHashSet(const Allocator& alloc) : HashSetBase(64, 0.75f, alloc)
	{
	}

	const Key* getEntries() const
	{
		return HashSetBase::mBase.getEntries();
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

