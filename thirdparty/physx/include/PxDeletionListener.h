// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DELETION_LISTENER_H
#define PX_DELETION_LISTENER_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	
/**
\brief Flags specifying deletion event types.

\see PxDeletionListener::onRelease PxPhysics.registerDeletionListener()
*/
struct PxDeletionEventFlag
{
	enum Enum
	{
		eUSER_RELEASE					= (1<<0),	//!< The user has called release on an object.
		eMEMORY_RELEASE					= (1<<1)	//!< The destructor of an object has been called and the memory has been released.
	};
};

/**
\brief Collection of set bits defined in PxDeletionEventFlag.

\see PxDeletionEventFlag
*/
typedef PxFlags<PxDeletionEventFlag::Enum,PxU8> PxDeletionEventFlags;
PX_FLAGS_OPERATORS(PxDeletionEventFlag::Enum,PxU8)


/**
\brief interface to get notification on object deletion

*/
class PxDeletionListener
{
public:
	/**
	\brief Notification if an object or its memory gets released

	If release() gets called on a PxBase object, an eUSER_RELEASE event will get fired immediately. The object state can be queried in the callback but
	it is not allowed to change the state. Furthermore, when reading from the object it is the user's responsibility to make sure that no other thread 
	is writing at the same time to the object (this includes the simulation itself, i.e., #PxScene::fetchResults() must not get called at the same time).

	Calling release() on a PxBase object does not necessarily trigger its destructor immediately. For example, the object can be shared and might still
	be referenced by other objects or the simulation might still be running and accessing the object state. In such cases the destructor will be called
	as soon as it is safe to do so. After the destruction of the object and its memory, an eMEMORY_RELEASE event will get fired. In this case it is not
	allowed to dereference the object pointer in the callback.

	\param[in] observed The object for which the deletion event gets fired.
	\param[in] userData The user data pointer of the object for which the deletion event gets fired. Not available for all object types in which case it will be set to 0.
	\param[in] deletionEvent The type of deletion event. Do not dereference the object pointer argument if the event is eMEMORY_RELEASE.

	*/
	virtual void onRelease(const PxBase* observed, void* userData, PxDeletionEventFlag::Enum deletionEvent) = 0;

protected:
	PxDeletionListener() {}
	virtual ~PxDeletionListener() {}
};


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
