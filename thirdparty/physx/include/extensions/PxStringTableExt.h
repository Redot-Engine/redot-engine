// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_STRING_TABLE_EXT_H
#define PX_STRING_TABLE_EXT_H

#include "foundation/PxAllocatorCallback.h"
#include "common/PxStringTable.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief a factory class for creating PxStringTable with a specific allocator.

\see PxStringTable 
*/

class PxStringTableExt
{
public:
	static PxStringTable& createStringTable( physx::PxAllocatorCallback& inAllocator );
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
