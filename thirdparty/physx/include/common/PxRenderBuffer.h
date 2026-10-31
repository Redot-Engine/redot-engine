// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_RENDER_BUFFER_H
#define PX_RENDER_BUFFER_H


#include "common/PxPhysXCommonConfig.h"
#include "foundation/PxVec3.h"
#include "foundation/PxMat33.h"
#include "foundation/PxBounds3.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Default color values used for debug rendering.
*/
struct PxDebugColor
{
	enum Enum
	{
		eARGB_BLACK		= 0xff000000,
		eARGB_RED		= 0xffff0000,
		eARGB_GREEN		= 0xff00ff00,
		eARGB_BLUE		= 0xff0000ff,
		eARGB_YELLOW	= 0xffffff00,
		eARGB_MAGENTA	= 0xffff00ff,
		eARGB_CYAN		= 0xff00ffff,
		eARGB_WHITE		= 0xffffffff,
		eARGB_GREY		= 0xff808080,
		eARGB_DARKRED	= 0xff880000,
		eARGB_DARKGREEN	= 0xff008800,
		eARGB_DARKBLUE	= 0xff000088
	};
};


/**
\brief Used to store a single point and colour for debug rendering.
*/
struct PxDebugPoint
{
	PxDebugPoint(const PxVec3& p, const PxU32& c)
		: pos(p), color(c) {}

	PxVec3	pos;
	PxU32	color;
};

/**
\brief Used to store a single line and colour for debug rendering.
*/
struct PxDebugLine
{
	PxDebugLine(const PxVec3& p0, const PxVec3& p1, const PxU32& c)
		: pos0(p0), color0(c), pos1(p1), color1(c) {}

	PxVec3	pos0;
	PxU32	color0;
	PxVec3	pos1;
	PxU32	color1;
};

/**
\brief Used to store a single triangle and colour for debug rendering.
*/
struct PxDebugTriangle
{
	PxDebugTriangle(const PxVec3& p0, const PxVec3& p1, const PxVec3& p2, const PxU32& c)
		: pos0(p0), color0(c), pos1(p1), color1(c), pos2(p2), color2(c) {}

	PxVec3	pos0;
	PxU32	color0;
	PxVec3	pos1;
	PxU32	color1;
	PxVec3	pos2;
	PxU32	color2;
};

/**
\brief Used to store a text for debug rendering. Doesn't own 'string' array.
*/
struct PxDebugText
{
	PxDebugText() : string(0)
	{
	}

	PxDebugText(const PxVec3& pos, const PxReal& sz, const PxU32& clr, const char* str)
		: position(pos), size(sz), color(clr), string(str)
	{
	}

	PxVec3 position;
	PxReal size;
	PxU32 color;
	const char* string;
};


/**
\brief Interface for points, lines, triangles, and text buffer.
*/
class PxRenderBuffer
{
public:
	virtual ~PxRenderBuffer() {}

	virtual PxU32 getNbPoints() const = 0;
	virtual const PxDebugPoint* getPoints() const = 0;
	virtual void addPoint(const PxDebugPoint& point) = 0;

	virtual PxU32 getNbLines() const = 0;
	virtual const PxDebugLine* getLines() const = 0;
	virtual void addLine(const PxDebugLine& line) = 0;
	virtual PxDebugLine* reserveLines(const PxU32 nbLines) = 0;
	virtual PxDebugPoint* reservePoints(const PxU32 nbLines) = 0;

	virtual PxU32 getNbTriangles() const = 0;
	virtual const PxDebugTriangle* getTriangles() const = 0;
	virtual void addTriangle(const PxDebugTriangle& triangle) = 0;

	virtual void append(const PxRenderBuffer& other) = 0;
	virtual void clear() = 0;

	virtual void shift(const PxVec3& delta) = 0;

	virtual bool empty() const = 0;
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
