// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PVD_SCENE_CLIENT_H
#define PX_PVD_SCENE_CLIENT_H


#include "foundation/PxFlags.h"
#include "foundation/PxVec3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif
	namespace pvdsdk
	{
		class PvdClient;
	}
	struct PxDebugPoint;
	struct PxDebugLine;
	struct PxDebugTriangle;
	struct PxDebugText;
#if !PX_DOXYGEN
} // namespace physx
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief PVD scene Flags. They are disabled by default, and only works if PxPvdInstrumentationFlag::eDEBUG is set.
*/
struct PxPvdSceneFlag
{
	enum Enum
	{
		eTRANSMIT_CONTACTS     = (1 << 0), //! Transmits contact stream to PVD.
		eTRANSMIT_SCENEQUERIES = (1 << 1), //! Transmits scene query stream to PVD.
		eTRANSMIT_CONSTRAINTS  = (1 << 2)  //! Transmits constraints visualize stream to PVD.
	};
};

/**
\brief Bitfield that contains a set of raised flags defined in PxPvdSceneFlag.

\see PxPvdSceneFlag
*/
typedef PxFlags<PxPvdSceneFlag::Enum, PxU8> PxPvdSceneFlags;
PX_FLAGS_OPERATORS(PxPvdSceneFlag::Enum, PxU8)

/**
\brief Special client for PxScene.
It provides access to the PxPvdSceneFlag.
It also provides simple user debug services that associated scene position such as immediate rendering and camera updates.
*/
class PxPvdSceneClient
{
  public:
	/**
	Sets the PVD flag. See PxPvdSceneFlag.
	\param flag Flag to set.
	\param value value the flag gets set to.
	*/
	virtual void setScenePvdFlag(PxPvdSceneFlag::Enum flag, bool value) = 0;

	/**
	Sets the PVD flags. See PxPvdSceneFlags.
	\param flags Flags to set.
	*/
	virtual void setScenePvdFlags(PxPvdSceneFlags flags) = 0;

	/**
	Retrieves the PVD flags. See PxPvdSceneFlags.
	*/
	virtual PxPvdSceneFlags getScenePvdFlags() const = 0;

	/**
	update camera on PVD application's render window
	*/
	virtual void updateCamera(const char* name, const PxVec3& origin, const PxVec3& up, const PxVec3& target) = 0;

	/**
	draw points on PVD application's render window
	*/
	virtual void drawPoints(const physx::PxDebugPoint* points, PxU32 count) = 0;

	/**
	draw lines on PVD application's render window
	*/
	virtual void drawLines(const physx::PxDebugLine* lines, PxU32 count) = 0;

	/**
	draw triangles on PVD application's render window
	*/
	virtual void drawTriangles(const physx::PxDebugTriangle* triangles, PxU32 count) = 0;

	/**
	draw text on PVD application's render window
	*/
	virtual void drawText(const physx::PxDebugText& text) = 0;

	/**
	get the underlying client, for advanced users
	*/
	virtual physx::pvdsdk::PvdClient* getClientInternal() = 0;

protected:
	virtual ~PxPvdSceneClient(){}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
