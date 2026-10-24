// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_MEMORY_H
#define PX_MEMORY_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxMathIntrinsics.h"
#include "foundation/PxSimpleTypes.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Sets the bytes of the provided buffer to zero.

	\param	dest	[out]	Pointer to block of memory to set zero.
	\param	count	[in]	Number of bytes to set to zero.

	\return Pointer to memory block (same as input)
	*/
	PX_FORCE_INLINE void* PxMemZero(void* dest, size_t count)
	{
		// This is to avoid undefined behavior
		return (count != 0) ? physx::intrinsics::memZero(dest, count) : NULL;
	}

	/**
	\brief Sets the bytes of the provided buffer to the specified value.

	\param	dest	[out]	Pointer to block of memory to set to the specified value.
	\param	c		[in]	Value to set the bytes of the block of memory to.
	\param	count	[in]	Number of bytes to set to the specified value.

	\return Pointer to memory block (same as input)
	*/
	PX_FORCE_INLINE void* PxMemSet(void* dest, PxI32 c, size_t count)
	{
		// This is to avoid undefined behavior
		return (count != 0) ? physx::intrinsics::memSet(dest, c, count) : NULL;
	}

	/**
	\brief Copies the bytes of one memory block to another. The memory blocks must not overlap.

	\note Use #PxMemMove if memory blocks overlap.

	\param dest		[out]	Pointer to block of memory to copy to.
	\param src		[in]	Pointer to block of memory to copy from.
	\param count	[in]	Number of bytes to copy.

	\return Pointer to destination memory block
	*/
	PX_FORCE_INLINE void* PxMemCopy(void* dest, const void* src, size_t count)
	{
		// This is to avoid undefined behavior
		return (count != 0) ? physx::intrinsics::memCopy(dest, src, count) : NULL;
	}

	/**
	\brief Copies the bytes of one memory block to another. The memory blocks can overlap.

	\note Use #PxMemCopy if memory blocks do not overlap.

	\param dest		[out]	Pointer to block of memory to copy to.
	\param src		[in]	Pointer to block of memory to copy from.
	\param count	[in]	Number of bytes to copy.

	\return Pointer to destination memory block
	*/
	PX_FORCE_INLINE void* PxMemMove(void* dest, const void* src, size_t count)
	{
		return physx::intrinsics::memMove(dest, src, count);
	}

	/**
	Mark a specified amount of memory with 0xcd pattern. This is used to check that the meta data 
	definition for serialized classes is complete in checked builds.

	\param ptr		[out]	Pointer to block of memory to initialize.
	\param byteSize	[in]	Number of bytes to initialize.
	*/
	PX_INLINE void PxMarkSerializedMemory(void* ptr, size_t byteSize)
	{
#if PX_CHECKED
		PxMemSet(ptr, 0xcd, byteSize);
#else
		PX_UNUSED(ptr);
		PX_UNUSED(byteSize);
#endif
	}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

