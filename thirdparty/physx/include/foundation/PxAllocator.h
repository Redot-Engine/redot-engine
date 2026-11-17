// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ALLOCATOR_H
#define PX_ALLOCATOR_H

#include "foundation/PxAllocatorCallback.h"
#include "foundation/PxAssert.h"
#include "foundation/PxFoundation.h"
#include "foundation/PxIO.h"
#include <stdlib.h>

#if PX_VC
	#pragma warning(push)
	#pragma warning(disable : 4577)
#endif

#if PX_WINDOWS_FAMILY
#if(_MSC_VER >= 1923)
	#include <typeinfo>
#else
	#include <typeinfo.h>
#endif
#endif

#if(PX_APPLE_FAMILY)
	#include <typeinfo>
#endif

#include <new>

#if PX_VC
	#pragma warning(pop)
#endif

// PT: the rules are simple:
// - PX_ALLOC/PX_ALLOCATE/PX_FREE is similar to malloc/free. Use that for POD/anything that doesn't need ctor/dtor.
// - PX_NEW/PX_DELETE is similar to new/delete. Use that for anything that needs a ctor/dtor.
// - Everything goes through the user allocator.
// - Inherit from PxUserAllocated to PX_NEW something. Do it even on small classes, it's free.
// - You cannot PX_NEW a POD. Use PX_ALLOC.

#define PX_ALLOC(n, name) physx::PxAllocator().allocate(n, PX_FL)

// PT: use this one to reduce the amount of visible reinterpret_cast
#define PX_ALLOCATE(type, count, name)	reinterpret_cast<type*>(PX_ALLOC(count*sizeof(type), name))

#define PX_FREE(x)							\
	if(x)									\
	{										\
		physx::PxAllocator().deallocate(x);	\
		x = NULL;							\
	}

#define PX_FREE_THIS	physx::PxAllocator().deallocate(this)

// PT: placement new is only needed when you control where the object is created (i.e. you already have an address for it before creating the object).
// So there are basically 2 legitimate placement new usages in PhysX:
// - binary deserialization
// - arrays/pools
// If you end up writing "PX_PLACEMENT_NEW(PX_ALLOC(sizeof(X), "X")", consider deriving X from PxUserAllocated and using PX_NEW instead.
#define PX_PLACEMENT_NEW(p, T)	new (p) T
#define PX_NEW(T)				new (physx::PxReflectionAllocator<T>(), PX_FL) T
#define PX_DELETE_THIS			delete this
#define PX_DELETE(x)			if(x)	{ delete x;		x = NULL;	}
#define PX_DELETE_ARRAY(x)		if(x)	{ delete []x;	x = NULL;	}
#define PX_RELEASE(x)			if(x)	{ x->release(); x = NULL;	}

#if !PX_DOXYGEN
namespace physx
{
#endif
	/**
	\brief Allocator used to access the global PxAllocatorCallback instance without providing additional information.
	*/
	class PxAllocator
	{
	public:
		PX_FORCE_INLINE	PxAllocator(const char* = NULL){}

		static PX_FORCE_INLINE	void*	allocate(size_t size, const char* file, int line, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			return size ? PxGetBroadcastAllocator()->allocate(size, "", file, line) : NULL;
		}

		static PX_FORCE_INLINE	void	deallocate(void* ptr, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			if(ptr)
				PxGetBroadcastAllocator()->deallocate(ptr);
		}
	};

	/**
	* \brief Bootstrap allocator using malloc/free.
	* Don't use unless your objects get allocated before foundation is initialized.
	*/
	class PxRawAllocator
	{
	public:
		PxRawAllocator(const char* = 0)	{}

		static PX_FORCE_INLINE	void*	allocate(size_t size, const char*, int, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			// malloc returns valid pointer for size==0, no need to check
			return ::malloc(size);
		}

		static PX_FORCE_INLINE	void	deallocate(void* ptr, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			// free(0) is guaranteed to have no side effect, no need to check
			::free(ptr);
		}
	};

	/**
	\brief Allocator used to access the global PxAllocatorCallback instance using a static name derived from T.
	*/
	template <typename T>
	class PxReflectionAllocator
	{
		static const char* getName(bool reportAllocationNames)
		{
			if(!reportAllocationNames)
				return "<allocation names disabled>";
#if PX_GCC_FAMILY
			return __PRETTY_FUNCTION__;
#else
			// name() calls malloc(), raw_name() wouldn't
			return typeid(T).name();
#endif
		}

	public:
		PxReflectionAllocator(const PxEMPTY)	{}
		PxReflectionAllocator(const char* = 0)	{}

		inline PxReflectionAllocator(const PxReflectionAllocator&)	{}

		static PX_FORCE_INLINE	void*	allocate(size_t size, const char* filename, int line, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			if(!size)
				return NULL;

			bool reportAllocationNames;
			PxAllocatorCallback* cb = PxGetBroadcastAllocator(&reportAllocationNames);

			return cb->allocate(size, getName(reportAllocationNames), filename, line);
		}

		static PX_FORCE_INLINE	void	deallocate(void* ptr, uint32_t* cookie=NULL)
		{
			PX_UNUSED(cookie);
			if(ptr)
				PxGetBroadcastAllocator()->deallocate(ptr);
		}
	};

	template <typename T>
	struct PxAllocatorTraits
	{
		typedef PxReflectionAllocator<T> Type;
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

