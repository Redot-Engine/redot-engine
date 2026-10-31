// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ALLOCA_H
#define PX_ALLOCA_H

#include "foundation/PxTempAllocator.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
template <typename T, typename Alloc = PxTempAllocator>
class PxScopedPointer : private Alloc
{
  public:
	~PxScopedPointer()
	{
		if(mOwned)
			Alloc::deallocate(mPointer);
	}

	operator T*() const
	{
		return mPointer;
	}

	T* mPointer;
	bool mOwned;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

  // Don't use inline for alloca !!!
#if PX_WINDOWS_FAMILY
	#include <malloc.h>
	#define PxAlloca(x) _alloca(x)
#elif PX_LINUX
	#include <malloc.h>
	#define PxAlloca(x) alloca(x)
#elif PX_APPLE_FAMILY
	#include <alloca.h>
	#define PxAlloca(x) alloca(x)
#elif PX_SWITCH
	#include <malloc.h>
	#define PxAlloca(x) alloca(x)
#endif

#define PxAllocaAligned(x, alignment) ((size_t(PxAlloca(x + alignment)) + (alignment - 1)) & ~size_t(alignment - 1))

/*! Stack allocation for \c count instances of \c type. Falling back to temp allocator if using more than 4kB. */
#define PX_ALLOCA(var, type, count)																	\
	physx::PxScopedPointer<type> var;																\
	{																								\
		const uint32_t size = sizeof(type) * (count);												\
		var.mOwned = size > 4096;																	\
		if(var.mOwned)																				\
			var.mPointer = reinterpret_cast<type*>(physx::PxTempAllocator().allocate(size, PX_FL));	\
		else																						\
			var.mPointer = reinterpret_cast<type*>(PxAlloca(size));									\
	}
#endif

