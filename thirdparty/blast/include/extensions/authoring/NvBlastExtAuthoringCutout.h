// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright (c) 2016-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines the API for the NvBlastExtAuthoring blast sdk extension's CutoutSet, used for cutout fracturing

#ifndef NVBLASTAUTHORINGCUTOUT_H
#define NVBLASTAUTHORINGCUTOUT_H

#include "NvBlastExtAuthoringTypes.h"


namespace Nv
{
namespace Blast
{

/**
Interface to a "cutout set," used with chippable fracturing.  A cutout set is created from a bitmap.  The
result is turned into cutouts which are applied to the mesh.  For example, a bitmap which looks like a brick
pattern will generate a cutout for each "brick," forming the cutout set.

Each cutout is a 2D entity, meant to be projected onto various faces of a mesh.  They are represented
by a set of 2D vertices, which form closed loops.  More than one loop may represent a single cutout, if
the loops are forced to be convex.  Otherwise, a cutout is represented by a single loop.
*/
class CutoutSet
{
public:
    /** Returns the number of cutouts in the set. */
    virtual uint32_t                getCutoutCount() const = 0;

    /**
    Applies to the cutout indexed by cutoutIndex:
    Returns the number of vertices in the cutout.
    */
    virtual uint32_t                getCutoutVertexCount(uint32_t cutoutIndex, uint32_t loopIndex) const = 0;

    /**
    Applies to the cutout indexed by cutoutIndex:
    Returns the number of loops in this cutout.
    */
    virtual uint32_t                getCutoutLoopCount(uint32_t cutoutIndex) const = 0;

    /**
    Applies to the cutout indexed by cutoutIndex:
    Returns the vertex indexed by vertexIndex.  (Only the X and Y coordinates are used.)
    */
    virtual const NvcVec3&          getCutoutVertex(uint32_t cutoutIndex, uint32_t loopIndex, uint32_t vertexIndex) const = 0;

    /**
    If smoothing group should be changed for adjacent to this vertex faces return true
    */
    virtual bool                    isCutoutVertexToggleSmoothingGroup(uint32_t cutoutIndex, uint32_t loopIndex, uint32_t vertexIndex) const = 0;

    /**
    Whether or not this cutout set is to be tiled.
    */
    virtual bool                    isPeriodic() const = 0;

    /**
    The dimensions of the fracture map used to create the cutout set.
    */
    virtual const NvcVec2&          getDimensions() const = 0;

    /** Releases all memory and deletes itself. */
    virtual void                    release() = 0;

protected:
    /** Protected destructor.  Use the release() method. */
    virtual                         ~CutoutSet() {}
};

} // namespace Blast
} // namespace Nv


#endif // idndef NVBLASTAUTHORINGCUTOUT_H
