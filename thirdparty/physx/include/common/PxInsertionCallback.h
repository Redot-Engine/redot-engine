// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_INSERTION_CALLBACK_H
#define PX_INSERTION_CALLBACK_H

#include "common/PxBase.h"


#if !PX_DOXYGEN
namespace physx
{
#endif

	/**
	\brief Callback interface that permits TriangleMesh, Heightfield, ConvexMesh or BVH to be used
	directly without the need to store the cooking results into a stream.

	Using this is advised only if real-time cooking is required; using "offline" cooking and
	streams is otherwise preferred.

	The default PxInsertionCallback implementations must be used. The PxPhysics
	default callback can be obtained using the PxPhysics::getPhysicsInsertionCallback().
	The PxCooking default callback can be obtained using the PxCooking::getStandaloneInsertionCallback().

	\see PxCooking PxPhysics
	*/
	class PxInsertionCallback
	{
	public:
		PxInsertionCallback()				{}

		/**
		\brief Builds object (TriangleMesh, Heightfield, ConvexMesh or BVH) from given data in PxPhysics.		

		\param type Object type to build.
		\param data Object data
		\return PxBase Created object in PxPhysics.
		*/
		virtual PxBase* buildObjectFromData(PxConcreteType::Enum type, void* data) = 0;

	protected:
		virtual ~PxInsertionCallback()		{}
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
