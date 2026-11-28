// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TEMP_ALLOCATOR_H
#define PX_TEMP_ALLOCATOR_H

#include "foundation/PxAllocator.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxTempAllocator
{
  public:
	PX_FORCE_INLINE PxTempAllocator(const char* = 0)
	{
	}
	PX_FOUNDATION_API void* allocate(size_t size, const char* file, PxI32 line);
	PX_FOUNDATION_API void deallocate(void* ptr);
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

