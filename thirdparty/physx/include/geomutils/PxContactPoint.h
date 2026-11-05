// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONTACT_POINT_H
#define PX_CONTACT_POINT_H

#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	struct PxContactPoint
	{
		/**
		\brief The normal of the contacting surfaces at the contact point.

		For two shapes s0 and s1, the normal points in the direction that s0 needs to move in to resolve the contact with s1.
		*/
		PX_ALIGN(16, PxVec3	normal);

		/**
		\brief The separation of the shapes at the contact point. A negative separation denotes a penetration.
		*/
		PxReal	separation;

		/**
		\brief The point of contact between the shapes, in world space. 
		*/
		PX_ALIGN(16, PxVec3	point);	

		/**
		\brief The max impulse permitted at this point
		*/
		PxReal maxImpulse;

		PX_ALIGN(16, PxVec3 targetVel);

		/**
		\brief The static friction coefficient
		*/
		PxReal staticFriction;

		/**
		\brief Material flags for this contact (eDISABLE_FRICTION, eDISABLE_STRONG_FRICTION). \see PxMaterialFlag
		*/
		PxU8 materialFlags;

		/**
		\brief The surface index of shape 1 at the contact point. This is used to identify the surface material.

		\note This field is only supported by triangle meshes and heightfields, else it will be set to PXC_CONTACT_NO_FACE_INDEX.
		*/
		PxU32   internalFaceIndex1;

		/**
		\brief The dynamic friction coefficient
		*/
		PxReal dynamicFriction;

		/**
		\brief The restitution coefficient
		*/
		PxReal restitution;

		/**
		\brief Damping coefficient (for compliant contacts)
		*/
		PxReal damping;
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
