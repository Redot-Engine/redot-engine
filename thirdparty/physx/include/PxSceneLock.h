// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SCENE_LOCK_H
#define PX_SCENE_LOCK_H

#include "PxPhysXConfig.h"
#include "PxScene.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief RAII wrapper for the PxScene read lock.

Use this class as follows to lock the scene for reading by the current thread 
for the duration of the enclosing scope:

	PxSceneReadLock lock(sceneRef);

\see PxScene::lockRead(), PxScene::unlockRead(), PxSceneFlag::eREQUIRE_RW_LOCK
*/
class PxSceneReadLock
{
	PxSceneReadLock(const PxSceneReadLock&);
	PxSceneReadLock& operator=(const PxSceneReadLock&);

public:
	
	/**
	\brief Constructor
	\param scene The scene to lock for reading
	\param file Optional string for debugging purposes
	\param line Optional line number for debugging purposes
	*/
	PxSceneReadLock(PxScene& scene, const char* file=NULL, PxU32 line=0)
		: mScene(scene)
	{
		mScene.lockRead(file, line);
	}

	~PxSceneReadLock()
	{
		mScene.unlockRead();
	}

private:

	PxScene& mScene;
};

/**
\brief RAII wrapper for the PxScene write lock.

Use this class as follows to lock the scene for writing by the current thread 
for the duration of the enclosing scope:

	PxSceneWriteLock lock(sceneRef);

\see PxScene::lockWrite(), PxScene::unlockWrite(), PxSceneFlag::eREQUIRE_RW_LOCK
*/
class PxSceneWriteLock
{
	PxSceneWriteLock(const PxSceneWriteLock&);
	PxSceneWriteLock& operator=(const PxSceneWriteLock&);

public:

	/**
	\brief Constructor
	\param scene The scene to lock for writing
	\param file Optional string for debugging purposes
	\param line Optional line number for debugging purposes
	*/
	PxSceneWriteLock(PxScene& scene, const char* file=NULL, PxU32 line=0)
		: mScene(scene)
	{
		mScene.lockWrite(file, line);
	}

	~PxSceneWriteLock()
	{
		mScene.unlockWrite();
	}

private:

	PxScene& mScene;
};


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
