// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_MIDPHASE_DESC_H
#define PX_MIDPHASE_DESC_H

#include "geometry/PxTriangleMesh.h"
#include "cooking/PxBVH33MidphaseDesc.h"
#include "cooking/PxBVH34MidphaseDesc.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**

\brief Structure describing parameters affecting midphase mesh structure.

\see PxCookingParams, PxBVH33MidphaseDesc, PxBVH34MidphaseDesc
*/
class PxMidphaseDesc
{
public:
	PX_FORCE_INLINE PxMidphaseDesc()	{ setToDefault(PxMeshMidPhase::eBVH34);	}

	/**
	\brief	Returns type of midphase mesh structure.
	\return	PxMeshMidPhase::Enum 

	\see PxMeshMidPhase::Enum
	*/
	PX_FORCE_INLINE PxMeshMidPhase::Enum getType() const { return mType; }

	/**
	\brief	Midphase descriptors union

	\see PxBV33MidphaseDesc, PxBV34MidphaseDesc
	*/
	union {
		PxBVH33MidphaseDesc  mBVH33Desc;
		PxBVH34MidphaseDesc  mBVH34Desc;
    };

	/**
	\brief	Initialize the midphase mesh structure descriptor
	\param[in] type Midphase mesh structure descriptor

	\see PxBV33MidphaseDesc, PxBV34MidphaseDesc
	*/
	void setToDefault(PxMeshMidPhase::Enum type)
	{
		mType = type;
		if(type==PxMeshMidPhase::eBVH33)
			mBVH33Desc.setToDefault();
		else if(type==PxMeshMidPhase::eBVH34)
			mBVH34Desc.setToDefault();
	}

	/**
	\brief Returns true if the descriptor is valid.
	\return true if the current settings are valid.
	*/
	bool isValid() const
	{
		if(mType==PxMeshMidPhase::eBVH33)
			return mBVH33Desc.isValid();
		else if(mType==PxMeshMidPhase::eBVH34)
			return mBVH34Desc.isValid();
		return false;
	}

	/**
	\brief Assignment operator
	\return this
	*/
	PX_FORCE_INLINE PxMidphaseDesc&	operator=(PxMeshMidPhase::Enum descType) 
	{ 
		setToDefault(descType);
		return *this; 
	}

protected:	
	PxMeshMidPhase::Enum	mType;
};

#if !PX_DOXYGEN
} // namespace physx
#endif


#endif

