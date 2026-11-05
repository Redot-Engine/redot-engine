// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_HASH_H
#define PX_HASH_H

#include "foundation/PxBasicTemplates.h"
#include "foundation/PxString.h"

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4302)
#endif

#if PX_LINUX
#include "foundation/PxSimpleTypes.h"
#endif

/*!
Central definition of hash functions
*/

#if !PX_DOXYGEN
namespace physx
{
#endif
// Hash functions

// Thomas Wang's 32 bit mix
// http://www.cris.com/~Ttwang/tech/inthash.htm
PX_FORCE_INLINE uint32_t PxComputeHash_Wang(const uint32_t key)
{
	uint32_t k = key;
	k += ~(k << 15);
	k ^= (k >> 10);
	k += (k << 3);
	k ^= (k >> 6);
	k += ~(k << 11);
	k ^= (k >> 16);
	return uint32_t(k);
}

// Thomas Wang's 64 bit mix
// http://www.cris.com/~Ttwang/tech/inthash.htm
PX_FORCE_INLINE uint32_t PxComputeHash_Wang(const uint64_t key)
{
	uint64_t k = key;
	k += ~(k << 32);
	k ^= (k >> 22);
	k += ~(k << 13);
	k ^= (k >> 8);
	k += (k << 3);
	k ^= (k >> 15);
	k += ~(k << 27);
	k ^= (k >> 31);
	return uint32_t(UINT32_MAX & k);
}

template <typename T, size_t byteSize>
struct HashSized
{
	PX_FORCE_INLINE uint32_t operator()(const T key) const
	{
		PX_COMPILE_TIME_ASSERT(byteSize <= 4);
		uint32_t x = uint32_t(key);
		x ^= x >> 16;
		x *= 0x21f0aaadU;
		x ^= x >> 15;
		x *= 0x735a2d97U;
		x ^= x >> 15;
		return x;
	}
};

template <typename T>
struct HashSized<T, 8>
{
	PX_FORCE_INLINE uint32_t operator()(const T key) const
	{
		uint64_t x = uint64_t(key);
		x ^= x >> 30;
		x *= 0xbf58476d1ce4e5b9;
		x ^= x >> 27;
		x *= 0x94d049bb133111eb;
		x ^= x >> 31;
		return uint32_t(x);
	}
};

template <typename T>
PX_FORCE_INLINE uint32_t PxComputeHash(const T key)
{
	return HashSized<T, sizeof(T)>()(key);
}

// Hash function for pairs
template <typename F, typename S>
PX_INLINE uint32_t PxComputeHash(const PxPair<F, S>& p)
{
	uint32_t seed = 0x876543;
	uint32_t m = 1000007;
	return PxComputeHash(p.second) ^ (m * (PxComputeHash(p.first) ^ (m * seed)));
}

// hash object for hash map template parameter
template <class Key>
struct PxHash
{
	uint32_t operator()(const Key& k) const
	{
		return PxComputeHash(k);
	}
	bool equal(const Key& k0, const Key& k1) const
	{
		return k0 == k1;
	}
};

// specialization for strings
template <>
struct PxHash<const char*>
{
  public:
	uint32_t operator()(const char* _string) const
	{
		// "DJB" string hash
		const uint8_t* string = reinterpret_cast<const uint8_t*>(_string);
		uint32_t h = 5381;
		for(const uint8_t* ptr = string; *ptr; ptr++)
			h = ((h << 5) + h) ^ uint32_t(*ptr);
		return h;
	}
	bool equal(const char* string0, const char* string1) const
	{
		return !Pxstrcmp(string0, string1);
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#if PX_VC
#pragma warning(pop)
#endif

#endif

