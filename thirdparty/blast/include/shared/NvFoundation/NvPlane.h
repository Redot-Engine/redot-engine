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
// Copyright (c) 2008-2024 NVIDIA Corporation. All rights reserved.

#ifndef NV_NVFOUNDATION_NVPLANE_H
#define NV_NVFOUNDATION_NVPLANE_H

/** \addtogroup foundation
@{
*/

#include "NvMath.h"
#include "NvVec3.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/**
\brief Representation of a plane.

 Plane equation used: n.dot(v) + d = 0
*/
class NvPlane
{
  public:
    /**
    \brief Constructor
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvPlane()
    {
    }

    /**
    \brief Constructor from a normal and a distance
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvPlane(float nx, float ny, float nz, float distance) : n(nx, ny, nz), d(distance)
    {
    }

    /**
    \brief Constructor from a normal and a distance
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvPlane(const NvVec3& normal, float distance) : n(normal), d(distance)
    {
    }

    /**
    \brief Constructor from a point on the plane and a normal
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvPlane(const NvVec3& point, const NvVec3& normal)
    : n(normal), d(-point.dot(n)) // p satisfies normal.dot(p) + d = 0
    {
    }

    /**
    \brief Constructor from three points
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvPlane(const NvVec3& p0, const NvVec3& p1, const NvVec3& p2)
    {
        n = (p1 - p0).cross(p2 - p0).getNormalized();
        d = -p0.dot(n);
    }

    /**
    \brief returns true if the two planes are exactly equal
    */
    NV_CUDA_CALLABLE NV_INLINE bool operator==(const NvPlane& p) const
    {
        return n == p.n && d == p.d;
    }

    NV_CUDA_CALLABLE NV_FORCE_INLINE float distance(const NvVec3& p) const
    {
        return p.dot(n) + d;
    }

    NV_CUDA_CALLABLE NV_FORCE_INLINE bool contains(const NvVec3& p) const
    {
        return NvAbs(distance(p)) < (1.0e-7f);
    }

    /**
    \brief projects p into the plane
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 project(const NvVec3& p) const
    {
        return p - n * distance(p);
    }

    /**
    \brief find an arbitrary point in the plane
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 pointInPlane() const
    {
        return -n * d;
    }

    /**
    \brief equivalent plane with unit normal
    */

    NV_CUDA_CALLABLE NV_FORCE_INLINE void normalize()
    {
        float denom = 1.0f / n.magnitude();
        n *= denom;
        d *= denom;
    }

    NvVec3 n; //!< The normal to the plane
    float d;  //!< The distance from the origin
};

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVPLANE_H
