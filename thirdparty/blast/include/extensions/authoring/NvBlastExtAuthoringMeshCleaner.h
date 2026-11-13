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
//! @brief Defines the API for the NvBlastExtAuthoring blast sdk extension's MeshCleaner utility

#ifndef NVBLASTEXTAUTHORINGMESHCLEANER_H
#define NVBLASTEXTAUTHORINGMESHCLEANER_H

#include "NvBlastExtAuthoringTypes.h"

/**
    FractureTool has requirements to input meshes to fracture them successfully:
        1) Mesh should be closed (watertight)
        2) There should not be self-intersections and open-edges.
*/

/**
    Mesh cleaner input is closed mesh with self-intersections and open-edges (only in the interior). 
    It tries to track outer hull to make input mesh solid and meet requierements of FractureTool. If mesh contained some internal cavities they will be removed.
*/

namespace Nv
{
namespace Blast
{

class Mesh;

class MeshCleaner
{
public:
    virtual ~MeshCleaner() {}

    /**
        Tries to remove self intersections and open edges in interior of mesh.
        \param[in] mesh Mesh to be cleaned.
        \return Cleaned mesh or nullptr if failed.
    */
    virtual Mesh* cleanMesh(const Mesh* mesh) = 0;

    virtual void release() = 0;
};


} // namespace Blast
} // namespace Nv

#endif // ifndef NVBLASTEXTAUTHORINGMESHCLEANER_H