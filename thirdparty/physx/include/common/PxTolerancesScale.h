// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_TOLERANCES_SCALE_H
#define PX_TOLERANCES_SCALE_H


#include "common/PxPhysXCommonConfig.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Class to define the scale at which simulation runs. Most simulation tolerances are
calculated in terms of the values here. 

\note if you change the simulation scale, you will probably also wish to change the scene's
default value of gravity, and stable simulation will probably require changes to the scene's 
bounceThreshold also.
*/

class PxTolerancesScale
{
public: 

	/** 
	\brief The approximate size of objects in the simulation. 
	
	For simulating roughly human-sized in metric units, 1 is a good choice.
	If simulation is done in centimetres, use 100 instead. This is used to
	estimate certain length-related tolerances.
	*/
	PxReal	length;

	/** 
	\brief The typical magnitude of velocities of objects in simulation. This is used to estimate 
	whether a contact should be treated as bouncing or resting based on its impact velocity,
	and a kinetic energy threshold below which the simulation may put objects to sleep.

	For normal physical environments, a good choice is the approximate speed of an object falling
	under gravity for one second.
	*/
	PxReal	speed;

	/**
	\brief constructor sets to default 

	\param[in]	defaultLength	Default length
	\param[in]	defaultSpeed	Default speed
	*/
	PX_INLINE explicit PxTolerancesScale(float defaultLength=1.0f, float defaultSpeed=10.0f);

	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid (returns always true).
	*/
	PX_INLINE bool isValid() const;

};

PX_INLINE PxTolerancesScale::PxTolerancesScale(float defaultLength, float defaultSpeed) :
	length	(defaultLength),
	speed	(defaultSpeed)
	{
	}

PX_INLINE bool PxTolerancesScale::isValid() const
{
	return length>0.0f;
}

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
