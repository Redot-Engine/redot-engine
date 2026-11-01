// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2016-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines the ConvexMeshBuilder API used by the authoring tools, allowing the user to specify a convex hull description for a chunk

#ifndef NVBLASTEXTAUTHORINGCONVEXMESHBUILDER_H
#define NVBLASTEXTAUTHORINGCONVEXMESHBUILDER_H

#include "NvCTypes.h"

namespace Nv
{
namespace Blast
{

struct CollisionHull;

/**
    ConvexMeshBuilder provides routine to build collision hulls from array of vertices.
    Collision hull is built as convex hull of provided point set.
    If due to some reason building of convex hull is failed, collision hull is built as bounding box of vertex set.
*/
class ConvexMeshBuilder
{
public:
    
    /**
    Release ConvexMeshBuilder memory
    */
    virtual void                    release() = 0;

    /**
        Method creates CollisionHull from provided array of vertices.
        \param[in]  verticesCount   Number of vertices
        \param[in]  vertexData      Vertex array of some object, for which collision geometry should be built
        \param[out] output          Reference on CollisionHull object in which generated geometry should be saved
    */
    virtual CollisionHull* buildCollisionGeometry(uint32_t verticesCount, const NvcVec3* vertexData) = 0;

    /**
        Release CollisionHull memory.
    */
    virtual void releaseCollisionHull(CollisionHull* hull) const = 0;
};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTEXTAUTHORINGCONVEXMESHBUILDER_H
