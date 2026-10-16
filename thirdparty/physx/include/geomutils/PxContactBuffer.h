// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONTACTBUFFER_H
#define PX_CONTACTBUFFER_H

#include "PxPhysXConfig.h"
#include "geomutils/PxContactPoint.h"
#include "PxContact.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	class PxContactBuffer
	{
	public:

		static const PxU32 MAX_CONTACTS = 255;

		PxContactPoint	contacts[MAX_CONTACTS];
		PxU32			count;
		PxU32			pad;

		PX_FORCE_INLINE void reset()
		{
			count = 0;
		}

		PX_FORCE_INLINE bool contact(const PxVec3& worldPoint, const PxVec3& worldNormalIn, PxReal separation, PxU32 faceIndex1 = PXC_CONTACT_NO_FACE_INDEX)
		{
			PX_ASSERT(PxAbs(worldNormalIn.magnitude()-1)<1e-3f);

			if(count>=MAX_CONTACTS)
				return false;

			PxContactPoint& p	= contacts[count++];
			p.normal			= worldNormalIn;
			p.point				= worldPoint;
			p.separation		= separation;
			p.internalFaceIndex1= faceIndex1;
			return true;
		}

		PX_FORCE_INLINE bool contact(const PxContactPoint& pt)
		{
			if(count>=MAX_CONTACTS)
				return false;
			contacts[count++] = pt;
			return true;
		}

		PX_FORCE_INLINE PxContactPoint* contact()
		{
			if(count>=MAX_CONTACTS)
				return NULL;
			return &contacts[count++];
		}
	};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
