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
//! @brief Defines the API for the NvBlastExtAuthoring blast sdk extension's BooleanTool

#ifndef NVBLASTAUTHORINGBOOLEANTOOL_H
#define NVBLASTAUTHORINGBOOLEANTOOL_H

#include "NvBlastExtAuthoringTypes.h"

namespace Nv
{
namespace Blast
{

// Forward declaration
class Mesh;
class SpatialAccelerator;

/**
    Tool for performing boolean operations on polygonal meshes.
    Tool supports only closed meshes. Performing boolean on meshes with holes can lead to unexpected behavior, e.g. holes in result geometry.
*/
class BooleanTool
{
public:
    virtual ~BooleanTool() {}

    /**
     *  Release BooleanTool memory
     */
    virtual void release() = 0;

    /**
     *  Operation to perform
     */
    enum Op
    {
        Intersection,
        Union,
        Difference
    };

    /**
     *  Perform boolean operation on two polygonal meshes (A and B).
     *  \param[in] meshA    Mesh A
     *  \param[in] accelA   Spatial accelerator for meshA.  Can be nullptr.
     *  \param[in] meshB    Mesh B
     *  \param[in] accelB   Spatial accelerator for meshB.  Can be nullptr.
     *  \param[in] op       Boolean operation type (see BooleanTool::Op)
     *  \return new mesh result of the boolean operation.  If nullptr, result is the empty set.
     */
    virtual Mesh*   performBoolean(const Mesh* meshA, SpatialAccelerator* accelA, const Mesh* meshB, SpatialAccelerator* accelB, Op op) = 0;

    /**
     *  Test whether point contained in mesh.
     *  \param[in] mesh     Mesh geometry
     *  \param[in] accel    Spatial accelerator for mesh.  Can be nullptr.
     *  \param[in] point    Point which should be tested
     *  \return true iff point is inside of mesh
     */
    virtual bool    pointInMesh(const Mesh* mesh, SpatialAccelerator* accel, const NvcVec3& point) = 0;
};

}  // namespace Blast
}  // namespace Nv

#endif  // ifndef NVBLASTAUTHORINGBOOLEANTOOL_H
