// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SCENE_QUERY_SYSTEM_EXT_H
#define PX_SCENE_QUERY_SYSTEM_EXT_H

#include "PxSceneQuerySystem.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Creates an external scene query system.

	An external SQ system is the part of a PxScene that deals with scene queries (SQ). This is usually taken care of
	by an internal implementation inside PxScene, but it is also possible to re-route all SQ calls to an external
	implementation, potentially opening the door to some customizations in behavior and features for advanced users.

	The following external SQ system is an example of how an implementation would look like. It re-uses much of the
	same code as the internal version, but it could be re-implemented in a completely different way to match users'
	specific needs.

	\param[in] desc			Scene query descriptor
	\param[in] contextID	Context ID parameter, sent to the profiler

	\return	An external SQ system instance

	\see PxSceneQuerySystem PxSceneQueryDesc
	*/
	PxSceneQuerySystem* PxCreateExternalSceneQuerySystem(const PxSceneQueryDesc& desc, PxU64 contextID);

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
