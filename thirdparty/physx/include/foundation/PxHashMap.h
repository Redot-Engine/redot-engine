// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_HASHMAP_H
#define PX_HASHMAP_H

#include "foundation/PxHashInternals.h"

// TODO: make this doxy-format
//
// This header defines two hash maps. Hash maps
// * support custom initial table sizes (rounded up internally to power-of-2)
// * support custom static allocator objects
// * auto-resize, based on a load factor (i.e. a 64-entry .75 load factor hash will resize
//                                        when the 49th element is inserted)
// * are based on open hashing
// * have O(1) contains, erase
//
// Maps have STL-like copying semantics, and properly initialize and destruct copies of objects
//
// There are two forms of map: coalesced and uncoalesced. Coalesced maps keep the entries in the
// initial segment of an array, so are fast to iterate over; however deletion is approximately
// twice as expensive.
//
// HashMap<T>:
//		bool			insert(const Key& k, const Value& v)	O(1) amortized (exponential resize policy)
//		Value &			operator[](const Key& k)				O(1) for existing objects, else O(1) amortized
//		const Entry *	find(const Key& k);						O(1)
//		bool			erase(const T& k);						O(1)
//		uint32_t			size();									constant
//		bool			reserve(uint32_t size);					O(MAX(currentOccupancy,size))
//		void			clear();								O(currentOccupancy) (with zero constant for objects
// without
// destructors)
//      Iterator		getIterator();
//
// operator[] creates an entry if one does not exist, initializing with the default constructor.
// CoalescedHashMap<T> does not support getIterator, but instead supports
// 		const Key *getEntries();
//
// Use of iterators:
//
// for(HashMap::Iterator iter = test.getIterator(); !iter.done(); ++iter)
//			myFunction(iter->first, iter->second);

#if !PX_DOXYGEN
namespace physx
{
#endif

template <class Key, class Value, class HashFn = PxHash<Key>, class Allocator = PxAllocator>
class PxHashMap : public physx::PxHashMapBase<Key, Value, HashFn, Allocator>
{
  public:
	typedef physx::PxHashMapBase<Key, Value, HashFn, Allocator> HashMapBase;
	typedef typename HashMapBase::Iterator Iterator;

	PxHashMap(uint32_t initialTableSize = 64, float loadFactor = 0.75f) : HashMapBase(initialTableSize, loadFactor)
	{
	}
	PxHashMap(uint32_t initialTableSize, float loadFactor, const Allocator& alloc)
	: HashMapBase(initialTableSize, loadFactor, alloc)
	{
	}
	PxHashMap(const Allocator& alloc) : HashMapBase(64, 0.75f, alloc)
	{
	}
	Iterator getIterator()
	{
		return Iterator(HashMapBase::mBase);
	}
};

template <class Key, class Value, class HashFn = PxHash<Key>, class Allocator = PxAllocator>
class PxCoalescedHashMap : public physx::PxHashMapBase<Key, Value, HashFn, Allocator>
{
  public:
	typedef physx::PxHashMapBase<Key, Value, HashFn, Allocator> HashMapBase;

	PxCoalescedHashMap(uint32_t initialTableSize = 64, float loadFactor = 0.75f)
	: HashMapBase(initialTableSize, loadFactor)
	{
	}
	const PxPair<const Key, Value>* getEntries() const
	{
		return HashMapBase::mBase.getEntries();
	}
};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

