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
//! @brief Defines the SpatialAccelerator API used by the BooleanTool

#ifndef NVBLASTEXTAUTHORINGACCELERATOR_H
#define NVBLASTEXTAUTHORINGACCELERATOR_H

#include "NvBlastExtAuthoringTypes.h"


namespace Nv
{
    namespace Blast
    {

        class Mesh;

        /**
            Acceleration structure interface.
        */
        class SpatialAccelerator
        {
        public:

            /**
                Set state of accelerator to return all facets which possibly can intersect given facet bound.
                \param[in] pos Vertex buffer
                \param[in] ed Edge buffer
                \param[in] fc Facet which should be tested.
            */
            virtual void    setState(const NvcBounds3* bounds) = 0;

            /**
                Set state of accelerator to return all facets which possibly can intersect given facet.
                \param[in] pos Vertex buffer
                \param[in] ed Edge buffer
                \param[in] fc Facet which should be tested.
            */
            virtual void    setState(const Vertex* pos, const Edge* ed, const Facet& fc) = 0;
            /**
                Set state of accelerator to return all facets which possibly can cover given point. Needed for testing whether point is inside mesh.
                \param[in] point Point which should be tested.
            */
            virtual void    setState(const NvcVec3& point) = 0;
            /**
                Recieve next facet for setted state.
                \return Next facet index, or -1 if no facets left.
            */
            virtual int32_t getNextFacet() = 0;


            virtual void setPointCmpDirection(int32_t dir) = 0;
            
            virtual void release() = 0;

            virtual ~SpatialAccelerator() {}
        };

        /**
            Used for some implementations of spatial accelerators.
        */
        class SpatialGrid
        {
        public:
            virtual void setMesh(const Nv::Blast::Mesh* m) = 0;

            virtual void release() = 0;
        };
    } // namespace Blast
} // namsepace Nv


#endif // ifndef NVBLASTEXTAUTHORINGACCELERATOR_H
