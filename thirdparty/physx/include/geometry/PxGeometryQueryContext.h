// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_GEOMETRY_QUERY_CONTEXT_H
#define PX_GEOMETRY_QUERY_CONTEXT_H

#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief A per-thread context passed to low-level query functions.

	This is a user-defined optional parameter that gets passed down to low-level query functions (raycast / overlap / sweep).

	This is not used directly in PhysX, although the context in this case is the PxHitCallback used in the query. This allows
	user-defined query functions, such as the ones from PxCustomGeometry, to get some additional data about the query. In this
	case this is a 'per-query' context rather than 'per-thread', but the initial goal of this parameter is to give custom
	query callbacks access to per-thread data structures (e.g. caches) that could be needed to implement the callbacks.

	In any case this is mostly for user-controlled query systems.
	*/
	struct PxQueryThreadContext
	{
	};

	/**
	\brief A per-thread context passed to low-level raycast functions.
	*/
	typedef PxQueryThreadContext PxRaycastThreadContext;

	/**
	\brief A per-thread context passed to low-level overlap functions.
	*/
	typedef PxQueryThreadContext PxOverlapThreadContext;

	/**
	\brief A per-thread context passed to low-level sweep functions.
	*/
	typedef PxQueryThreadContext PxSweepThreadContext;

#if !PX_DOXYGEN
}
#endif

#endif
