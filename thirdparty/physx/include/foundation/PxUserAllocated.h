// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_USER_ALLOCATED_H
#define PX_USER_ALLOCATED_H

#include "PxAllocator.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	/**
	Provides new and delete using a UserAllocator.
	Guarantees that 'delete x;' uses the UserAllocator too.
	*/
	class PxUserAllocated
	{
	public:
		// PX_SERIALIZATION
		PX_INLINE void* operator new(size_t, void* address)
		{
			return address;
		}

		//~PX_SERIALIZATION
		// Matching operator delete to the above operator new.  Don't ask me
		// how this makes any sense - Nuernberger.
		PX_INLINE void operator delete(void*, void*)
		{
		}

		template <typename Alloc>
		PX_INLINE void* operator new(size_t size, Alloc alloc, const char* fileName, int line)
		{
			return alloc.allocate(size, fileName, line);
		}

		template <typename Alloc>
		PX_INLINE void* operator new(size_t size, size_t /*align*/, Alloc alloc, const char* fileName, int line)
		{
			// align is not respected, we have 16bit aligned allocator
			return alloc.allocate(size, fileName, line);
		}

		template <typename Alloc>
		PX_INLINE void* operator new [](size_t size, Alloc alloc, const char* fileName, int line)
		{
			return alloc.allocate(size, fileName, line);
		}

		template <typename Alloc>
		PX_INLINE void* operator new [](size_t size, size_t /*align*/, Alloc alloc, const char* fileName, int line)
		{
			// align is not respected, we have 16bit aligned allocator
			return alloc.allocate(size, fileName, line);
		}

		// placement delete
		template <typename Alloc>
		PX_INLINE void operator delete(void* ptr, Alloc alloc, const char* fileName, int line)
		{
			PX_UNUSED(fileName);
			PX_UNUSED(line);
			alloc.deallocate(ptr);
		}

		template <typename Alloc>
		PX_INLINE void operator delete [](void* ptr, Alloc alloc, const char* fileName, int line)
		{
			PX_UNUSED(fileName);
			PX_UNUSED(line);
			alloc.deallocate(ptr);
		}
			
		PX_INLINE void operator delete(void* ptr)
		{
			PxAllocator().deallocate(ptr);
		}

		PX_INLINE void operator delete [](void* ptr)
		{
			PxAllocator().deallocate(ptr);
		}
	};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

