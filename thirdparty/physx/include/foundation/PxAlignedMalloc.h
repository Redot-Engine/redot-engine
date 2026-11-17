// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ALIGNED_MALLOC_H
#define PX_ALIGNED_MALLOC_H

#include "PxUserAllocated.h"

/*!
Allocate aligned memory.
Alignment must be a power of 2!
-- should be templated by a base allocator
*/

#if !PX_DOXYGEN
namespace physx
{
#endif
	/**
	Allocator, which is used to access the global PxAllocatorCallback instance
	(used for dynamic data types template instantiation), which can align memory
	*/

	// SCS: AlignedMalloc with 3 params not found, seems not used on PC either
	// disabled for now to avoid GCC error

	template <uint32_t N, typename BaseAllocator = PxAllocator>
	class PxAlignedAllocator : public BaseAllocator
	{
	public:
		PxAlignedAllocator(const BaseAllocator& base = BaseAllocator()) : BaseAllocator(base)
		{
		}

		void* allocate(size_t size, const char* file, int line, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);

			size_t pad = N - 1 + sizeof(size_t); // store offset for delete.
			uint8_t* base = reinterpret_cast<uint8_t*>(BaseAllocator::allocate(size + pad, file, line));
			if (!base)
				return NULL;

			uint8_t* ptr = reinterpret_cast<uint8_t*>(size_t(base + pad) & ~(size_t(N) - 1)); // aligned pointer, ensuring N
																							  // is a size_t
																							  // wide mask
			reinterpret_cast<size_t*>(ptr)[-1] = size_t(ptr - base); // store offset

			return ptr;
		}

		void deallocate(void* ptr, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);

			if (ptr == NULL)
				return;

			uint8_t* base = reinterpret_cast<uint8_t*>(ptr) - reinterpret_cast<size_t*>(ptr)[-1];
			BaseAllocator::deallocate(base);
		}
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

