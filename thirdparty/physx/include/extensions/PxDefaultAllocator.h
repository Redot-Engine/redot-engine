// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFAULT_ALLOCATOR_H
#define PX_DEFAULT_ALLOCATOR_H

#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxAssert.h"
#include "foundation/PxMemory.h"
#include "common/PxPhysXCommonConfig.h"

#include <stdlib.h>

#if PX_WINDOWS_FAMILY || PX_LINUX_FAMILY || PX_SWITCH
#include <malloc.h>
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_WINDOWS_FAMILY
// on win32 we only have 8-byte alignment guaranteed, but the CRT provides special aligned allocation fns
PX_FORCE_INLINE void* platformAlignedAlloc(size_t size)
{
	return _aligned_malloc(size, 16);
}

PX_FORCE_INLINE void platformAlignedFree(void* ptr)
{
	_aligned_free(ptr);
}
#elif PX_LINUX_FAMILY || PX_SWITCH
PX_FORCE_INLINE void* platformAlignedAlloc(size_t size)
{
	return ::memalign(16, size);
}

PX_FORCE_INLINE void platformAlignedFree(void* ptr)
{
	::free(ptr);
}
#else
// on all other platforms we get 16-byte alignment by default
PX_FORCE_INLINE void* platformAlignedAlloc(size_t size)
{
	return ::malloc(size);
}

PX_FORCE_INLINE void platformAlignedFree(void* ptr)
{
	::free(ptr);
}
#endif

/**
\brief default implementation of the allocator interface required by the SDK
*/
class PxDefaultAllocator : public PxAllocatorCallback
{
public:
	virtual void* allocate(size_t size, const char*, const char*, int)
	{
		void* ptr = platformAlignedAlloc(size);
		PX_ASSERT((size_t(ptr) & 15)==0);
#if PX_STOMP_ALLOCATED_MEMORY
		if(ptr != NULL)
		{
			PxMemSet(ptr, PxI32(0xcd), PxU32(size));
		}
#endif
		return ptr;
	}

	virtual void deallocate(void* ptr)
	{
		platformAlignedFree(ptr);
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
