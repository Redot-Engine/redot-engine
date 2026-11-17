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
//! @brief Defines the Mesh API used by the authoring tools

#ifndef NVBLASTAUTHORINGMESH_H
#define NVBLASTAUTHORINGMESH_H

#include "NvBlastExtAuthoringTypes.h"

namespace Nv
{
namespace Blast
{

/**
    Class for internal mesh representation
*/
class Mesh
{
public:
    
    virtual ~Mesh() {}

    /**
        Release Mesh memory
    */
    virtual void                release() = 0;

    /**
        Return true if mesh is valid
    */
    virtual bool                isValid() const = 0;

    /**
        Return writable pointer on vertices array
    */
    virtual Vertex*             getVerticesWritable() = 0;

    /**
    Return pointer on vertices array
    */
    virtual const Vertex*       getVertices() const = 0;


    /**
        Return writable pointer on edges array
    */
    virtual Edge*               getEdgesWritable() = 0;

    /**
    Return pointer on edges array
    */
    virtual const Edge*         getEdges() const = 0;

    /**
        Return writable pointer on facets array
    */
    virtual Facet*              getFacetsBufferWritable() = 0;

    /**
    Return pointer on facets array
    */
    virtual const Facet*        getFacetsBuffer() const = 0;

    /**
        Return writable pointer on specified facet
    */
    virtual Facet*              getFacetWritable(int32_t facet) = 0;
    /**
        Return pointer on specified facet
    */
    virtual const Facet*        getFacet(int32_t facet) const = 0;

    /**
        Return edges count
    */
    virtual uint32_t            getEdgesCount() const = 0;

    /**
        Return vertices count
    */
    virtual uint32_t            getVerticesCount() const = 0;

    /**
        Return facet count
    */
    virtual uint32_t            getFacetCount() const = 0;

    /**
        Return reference on mesh bounding box.
    */
    virtual const NvcBounds3&   getBoundingBox() const = 0;

    /**
        Return writable reference on mesh bounding box.
    */
    virtual NvcBounds3& getBoundingBoxWritable() = 0;


    /**
        Set per-facet material id.
    */
    virtual void    setMaterialId(const int32_t* materialIds) = 0;

    /**
    Replaces an material id on faces with a new one
    */
    virtual void    replaceMaterialId(int32_t oldMaterialId, int32_t newMaterialId) = 0;

    /**
        Set per-facet smoothing group.
    */
    virtual void    setSmoothingGroup(const int32_t* smoothingGroups) = 0;

    /**
        Recalculate bounding box
    */
    virtual void                recalculateBoundingBox() = 0;

    /**
        Compute mesh volume and centroid.  Assumes mesh has outward normals and no holes.
    */
    virtual float               getMeshVolumeAndCentroid(NvcVec3& centroid) const = 0;

    /**
        Calculate per-facet bounding boxes.
    */
    virtual void                calcPerFacetBounds() = 0;

    /**
        Get pointer on facet bounding box, if not calculated return nullptr.
    */
    virtual const NvcBounds3*   getFacetBound(uint32_t index) const = 0;

};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTAUTHORINGMESH_H
