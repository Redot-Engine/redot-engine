// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONTROLLER_BEHAVIOR_H
#define PX_CONTROLLER_BEHAVIOR_H

#include "PxFiltering.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	class PxShape;
	class PxObstacle;
	class PxController;

	/**
	\brief specifies controller behavior
	*/
	struct PxControllerBehaviorFlag
	{
		enum Enum
		{
			eCCT_CAN_RIDE_ON_OBJECT		= (1<<0),	//!< Controller can ride on touched object (i.e. when this touched object is moving horizontally). \note The CCT vs. CCT case is not supported.
			eCCT_SLIDE					= (1<<1),	//!< Controller should slide on touched object
			eCCT_USER_DEFINED_RIDE		= (1<<2)	//!< Disable all code dealing with controllers riding on objects, let users define it outside of the SDK.
		};
	};

	/**
	\brief Bitfield that contains a set of raised flags defined in PxControllerBehaviorFlag.

	\see PxControllerBehaviorFlag
	*/
	typedef PxFlags<PxControllerBehaviorFlag::Enum, PxU8> PxControllerBehaviorFlags;
	PX_FLAGS_OPERATORS(PxControllerBehaviorFlag::Enum, PxU8)

	/**
	\brief User behavior callback.

	This behavior callback is called to customize the controller's behavior w.r.t. touched shapes.
	*/
	class PxControllerBehaviorCallback
	{
	public:
		/**
		\brief Retrieve behavior flags for a shape.

		When the CCT touches a shape, the CCT's behavior w.r.t. this shape can be customized by users.
		This function retrieves the desired PxControllerBehaviorFlag flags capturing the desired behavior.

		\param[in] shape	The shape the CCT is currently touching
		\param[in] actor	The actor owning the shape

		\return Desired behavior flags for the given shape

		\see PxControllerBehaviorFlag
		*/
		virtual PxControllerBehaviorFlags getBehaviorFlags(const PxShape& shape, const PxActor& actor) = 0;

		/**
		\brief Retrieve behavior flags for a controller.

		When the CCT touches a controller, the CCT's behavior w.r.t. this controller can be customized by users.
		This function retrieves the desired PxControllerBehaviorFlag flags capturing the desired behavior.

		\note The flag PxControllerBehaviorFlag::eCCT_CAN_RIDE_ON_OBJECT is not supported.

		\param[in] controller	The controller the CCT is currently touching

		\return Desired behavior flags for the given controller

		\see PxControllerBehaviorFlag
		*/
		virtual PxControllerBehaviorFlags getBehaviorFlags(const PxController& controller) = 0;

		/**
		\brief Retrieve behavior flags for an obstacle.

		When the CCT touches an obstacle, the CCT's behavior w.r.t. this obstacle can be customized by users.
		This function retrieves the desired PxControllerBehaviorFlag flags capturing the desired behavior.

		\param[in] obstacle		The obstacle the CCT is currently touching

		\return Desired behavior flags for the given obstacle

		\see PxControllerBehaviorFlag
		*/
		virtual PxControllerBehaviorFlags getBehaviorFlags(const PxObstacle& obstacle) = 0;

	protected:
		virtual ~PxControllerBehaviorCallback(){}
	};

#if !PX_DOXYGEN
}
#endif

#endif
