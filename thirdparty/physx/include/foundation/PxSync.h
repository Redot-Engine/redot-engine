// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SYNC_H
#define PX_SYNC_H

#include "foundation/PxAllocator.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
/*!
Implementation notes:
* - Calling set() on an already signaled Sync does not change its state.
* - Calling reset() on an already reset Sync does not change its state.
* - Calling set() on a reset Sync wakes all waiting threads (potential for thread contention).
* - Calling wait() on an already signaled Sync will return true immediately.
* - NOTE: be careful when pulsing an event with set() followed by reset(), because a
*   thread that is not waiting on the event will miss the signal.
*/
class PX_FOUNDATION_API PxSyncImpl
{
  public:
	static const uint32_t waitForever = 0xffffffff;

	PxSyncImpl();

	~PxSyncImpl();

	/** Wait on the object for at most the given number of ms. Returns
	*  true if the object is signaled. Sync::waitForever will block forever
	*  or until the object is signaled.
	*/

	bool wait(uint32_t milliseconds = waitForever);

	/** Signal the synchronization object, waking all threads waiting on it */

	void set();

	/** Reset the synchronization object */

	void reset();

	/**
	Size of this class.
	*/
	static uint32_t getSize();
};

/*!
Implementation notes:
* - Calling set() on an already signaled Sync does not change its state.
* - Calling reset() on an already reset Sync does not change its state.
* - Calling set() on a reset Sync wakes all waiting threads (potential for thread contention).
* - Calling wait() on an already signaled Sync will return true immediately.
* - NOTE: be careful when pulsing an event with set() followed by reset(), because a
*   thread that is not waiting on the event will miss the signal.
*/
template <typename Alloc = PxReflectionAllocator<PxSyncImpl> >
class PxSyncT : protected Alloc
{
  public:
	static const uint32_t waitForever = PxSyncImpl::waitForever;

	PxSyncT(const Alloc& alloc = Alloc()) : Alloc(alloc)
	{
		mImpl = reinterpret_cast<PxSyncImpl*>(Alloc::allocate(PxSyncImpl::getSize(), PX_FL));
		PX_PLACEMENT_NEW(mImpl, PxSyncImpl)();
	}

	~PxSyncT()
	{
		mImpl->~PxSyncImpl();
		Alloc::deallocate(mImpl);
	}

	/** Wait on the object for at most the given number of ms. Returns
	*  true if the object is signaled. Sync::waitForever will block forever
	*  or until the object is signaled.
	*/

	bool wait(uint32_t milliseconds = PxSyncImpl::waitForever)
	{
		return mImpl->wait(milliseconds);
	}

	/** Signal the synchronization object, waking all threads waiting on it */

	void set()
	{
		mImpl->set();
	}

	/** Reset the synchronization object */

	void reset()
	{
		mImpl->reset();
	}

  private:
	class PxSyncImpl* mImpl;
};

typedef PxSyncT<> PxSync;

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

