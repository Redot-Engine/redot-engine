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

#ifndef NV_NVFOUNDATION_NVBOUNDS3_H
#define NV_NVFOUNDATION_NVBOUNDS3_H

/** \addtogroup foundation
@{
*/

#include "NvTransform.h"
#include "NvMat33.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

// maximum extents defined such that floating point exceptions are avoided for standard use cases
#define NV_MAX_BOUNDS_EXTENTS (NV_MAX_REAL * 0.25f)

/**
\brief Class representing 3D range or axis aligned bounding box.

Stored as minimum and maximum extent corners. Alternate representation
would be center and dimensions.
May be empty or nonempty. For nonempty bounds, minimum <= maximum has to hold for all axes.
Empty bounds have to be represented as minimum = NV_MAX_BOUNDS_EXTENTS and maximum = -NV_MAX_BOUNDS_EXTENTS for all
axes.
All other representations are invalid and the behavior is undefined.
*/
class NvBounds3
{
  public:
    /**
    \brief Default constructor, not performing any initialization for performance reason.
    \remark Use empty() function below to construct empty bounds.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3()
    {
    }

    /**
    \brief Construct from two bounding points
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3(const NvVec3& minimum, const NvVec3& maximum);

    /**
    \brief Return empty bounds.
    */
    static NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 empty();

    /**
    \brief returns the AABB containing v0 and v1.
    \param v0 first point included in the AABB.
    \param v1 second point included in the AABB.
    */
    static NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 boundsOfPoints(const NvVec3& v0, const NvVec3& v1);

    /**
    \brief returns the AABB from center and extents vectors.
    \param center Center vector
    \param extent Extents vector
    */
    static NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 centerExtents(const NvVec3& center, const NvVec3& extent);

    /**
    \brief Construct from center, extent, and (not necessarily orthogonal) basis
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3
    basisExtent(const NvVec3& center, const NvMat33& basis, const NvVec3& extent);

    /**
    \brief Construct from pose and extent
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3 poseExtent(const NvTransform& pose, const NvVec3& extent);

    /**
    \brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

    This version is safe to call for empty bounds.

    \param[in] matrix Transform to apply, can contain scaling as well
    \param[in] bounds The bounds to transform.
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3 transformSafe(const NvMat33& matrix, const NvBounds3& bounds);

    /**
    \brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

    Calling this method for empty bounds leads to undefined behavior. Use #transformSafe() instead.

    \param[in] matrix Transform to apply, can contain scaling as well
    \param[in] bounds The bounds to transform.
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3 transformFast(const NvMat33& matrix, const NvBounds3& bounds);

    /**
    \brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

    This version is safe to call for empty bounds.

    \param[in] transform Transform to apply, can contain scaling as well
    \param[in] bounds The bounds to transform.
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3 transformSafe(const NvTransform& transform, const NvBounds3& bounds);

    /**
    \brief gets the transformed bounds of the passed AABB (resulting in a bigger AABB).

    Calling this method for empty bounds leads to undefined behavior. Use #transformSafe() instead.

    \param[in] transform Transform to apply, can contain scaling as well
    \param[in] bounds The bounds to transform.
    */
    static NV_CUDA_CALLABLE NV_INLINE NvBounds3 transformFast(const NvTransform& transform, const NvBounds3& bounds);

    /**
    \brief Sets empty to true
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void setEmpty();

    /**
    \brief Sets the bounds to maximum size [-NV_MAX_BOUNDS_EXTENTS, NV_MAX_BOUNDS_EXTENTS].
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void setMaximal();

    /**
    \brief expands the volume to include v
    \param v Point to expand to.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void include(const NvVec3& v);

    /**
    \brief expands the volume to include b.
    \param b Bounds to perform union with.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void include(const NvBounds3& b);

    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isEmpty() const;

    /**
    \brief indicates whether the intersection of this and b is empty or not.
    \param b Bounds to test for intersection.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool intersects(const NvBounds3& b) const;

    /**
     \brief computes the 1D-intersection between two AABBs, on a given axis.
     \param a       the other AABB
     \param axis    the axis (0, 1, 2)
     */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool intersects1D(const NvBounds3& a, uint32_t axis) const;

    /**
    \brief indicates if these bounds contain v.
    \param v Point to test against bounds.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool contains(const NvVec3& v) const;

    /**
     \brief checks a box is inside another box.
     \param box     the other AABB
     */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isInside(const NvBounds3& box) const;

    /**
    \brief returns the center of this axis aligned box.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 getCenter() const;

    /**
    \brief get component of the box's center along a given axis
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float getCenter(uint32_t axis) const;

    /**
    \brief get component of the box's extents along a given axis
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float getExtents(uint32_t axis) const;

    /**
    \brief returns the dimensions (width/height/depth) of this axis aligned box.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 getDimensions() const;

    /**
    \brief returns the extents, which are half of the width/height/depth.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 getExtents() const;

    /**
    \brief scales the AABB.

    This version is safe to call for empty bounds.

    \param scale Factor to scale AABB by.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void scaleSafe(float scale);

    /**
    \brief scales the AABB.

    Calling this method for empty bounds leads to undefined behavior. Use #scaleSafe() instead.

    \param scale Factor to scale AABB by.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void scaleFast(float scale);

    /**
    fattens the AABB in all 3 dimensions by the given distance.

    This version is safe to call for empty bounds.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void fattenSafe(float distance);

    /**
    fattens the AABB in all 3 dimensions by the given distance.

    Calling this method for empty bounds leads to undefined behavior. Use #fattenSafe() instead.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE void fattenFast(float distance);

    /**
    checks that the AABB values are not NaN
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isFinite() const;

    /**
    checks that the AABB values describe a valid configuration.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isValid() const;

    NvVec3 minimum, maximum;
};

NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3::NvBounds3(const NvVec3& minimum_, const NvVec3& maximum_)
: minimum(minimum_), maximum(maximum_)
{
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 NvBounds3::empty()
{
    return NvBounds3(NvVec3(NV_MAX_BOUNDS_EXTENTS), NvVec3(-NV_MAX_BOUNDS_EXTENTS));
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::isFinite() const
{
    return minimum.isFinite() && maximum.isFinite();
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 NvBounds3::boundsOfPoints(const NvVec3& v0, const NvVec3& v1)
{
    return NvBounds3(v0.minimum(v1), v0.maximum(v1));
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvBounds3 NvBounds3::centerExtents(const NvVec3& center, const NvVec3& extent)
{
    return NvBounds3(center - extent, center + extent);
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3
NvBounds3::basisExtent(const NvVec3& center, const NvMat33& basis, const NvVec3& extent)
{
    // extended basis vectors
    NvVec3 c0 = basis.column0 * extent.x;
    NvVec3 c1 = basis.column1 * extent.y;
    NvVec3 c2 = basis.column2 * extent.z;

    NvVec3 w;
    // find combination of base vectors that produces max. distance for each component = sum of abs()
    w.x = NvAbs(c0.x) + NvAbs(c1.x) + NvAbs(c2.x);
    w.y = NvAbs(c0.y) + NvAbs(c1.y) + NvAbs(c2.y);
    w.z = NvAbs(c0.z) + NvAbs(c1.z) + NvAbs(c2.z);

    return NvBounds3(center - w, center + w);
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3 NvBounds3::poseExtent(const NvTransform& pose, const NvVec3& extent)
{
    return basisExtent(pose.p, NvMat33(pose.q), extent);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::setEmpty()
{
    minimum = NvVec3(NV_MAX_BOUNDS_EXTENTS);
    maximum = NvVec3(-NV_MAX_BOUNDS_EXTENTS);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::setMaximal()
{
    minimum = NvVec3(-NV_MAX_BOUNDS_EXTENTS);
    maximum = NvVec3(NV_MAX_BOUNDS_EXTENTS);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::include(const NvVec3& v)
{
    NV_ASSERT(isValid());
    minimum = minimum.minimum(v);
    maximum = maximum.maximum(v);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::include(const NvBounds3& b)
{
    NV_ASSERT(isValid());
    minimum = minimum.minimum(b.minimum);
    maximum = maximum.maximum(b.maximum);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::isEmpty() const
{
    NV_ASSERT(isValid());
    return minimum.x > maximum.x;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::intersects(const NvBounds3& b) const
{
    NV_ASSERT(isValid() && b.isValid());
    return !(b.minimum.x > maximum.x || minimum.x > b.maximum.x || b.minimum.y > maximum.y || minimum.y > b.maximum.y ||
             b.minimum.z > maximum.z || minimum.z > b.maximum.z);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::intersects1D(const NvBounds3& a, uint32_t axis) const
{
    NV_ASSERT(isValid() && a.isValid());
    return maximum[axis] >= a.minimum[axis] && a.maximum[axis] >= minimum[axis];
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::contains(const NvVec3& v) const
{
    NV_ASSERT(isValid());

    return !(v.x < minimum.x || v.x > maximum.x || v.y < minimum.y || v.y > maximum.y || v.z < minimum.z ||
             v.z > maximum.z);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::isInside(const NvBounds3& box) const
{
    NV_ASSERT(isValid() && box.isValid());
    if(box.minimum.x > minimum.x)
        return false;
    if(box.minimum.y > minimum.y)
        return false;
    if(box.minimum.z > minimum.z)
        return false;
    if(box.maximum.x < maximum.x)
        return false;
    if(box.maximum.y < maximum.y)
        return false;
    if(box.maximum.z < maximum.z)
        return false;
    return true;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 NvBounds3::getCenter() const
{
    NV_ASSERT(isValid());
    return (minimum + maximum) * 0.5f;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvBounds3::getCenter(uint32_t axis) const
{
    NV_ASSERT(isValid());
    return (minimum[axis] + maximum[axis]) * 0.5f;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvBounds3::getExtents(uint32_t axis) const
{
    NV_ASSERT(isValid());
    return (maximum[axis] - minimum[axis]) * 0.5f;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 NvBounds3::getDimensions() const
{
    NV_ASSERT(isValid());
    return maximum - minimum;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 NvBounds3::getExtents() const
{
    NV_ASSERT(isValid());
    return getDimensions() * 0.5f;
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::scaleSafe(float scale)
{
    NV_ASSERT(isValid());
    if(!isEmpty())
        scaleFast(scale);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::scaleFast(float scale)
{
    NV_ASSERT(isValid());
    *this = centerExtents(getCenter(), getExtents() * scale);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::fattenSafe(float distance)
{
    NV_ASSERT(isValid());
    if(!isEmpty())
        fattenFast(distance);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE void NvBounds3::fattenFast(float distance)
{
    NV_ASSERT(isValid());
    minimum.x -= distance;
    minimum.y -= distance;
    minimum.z -= distance;

    maximum.x += distance;
    maximum.y += distance;
    maximum.z += distance;
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3 NvBounds3::transformSafe(const NvMat33& matrix, const NvBounds3& bounds)
{
    NV_ASSERT(bounds.isValid());
    return !bounds.isEmpty() ? transformFast(matrix, bounds) : bounds;
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3 NvBounds3::transformFast(const NvMat33& matrix, const NvBounds3& bounds)
{
    NV_ASSERT(bounds.isValid());
    return NvBounds3::basisExtent(matrix * bounds.getCenter(), matrix, bounds.getExtents());
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3 NvBounds3::transformSafe(const NvTransform& transform, const NvBounds3& bounds)
{
    NV_ASSERT(bounds.isValid());
    return !bounds.isEmpty() ? transformFast(transform, bounds) : bounds;
}

NV_CUDA_CALLABLE NV_INLINE NvBounds3 NvBounds3::transformFast(const NvTransform& transform, const NvBounds3& bounds)
{
    NV_ASSERT(bounds.isValid());
    return NvBounds3::basisExtent(transform.transform(bounds.getCenter()), NvMat33(transform.q), bounds.getExtents());
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvBounds3::isValid() const
{
    return (isFinite() && (((minimum.x <= maximum.x) && (minimum.y <= maximum.y) && (minimum.z <= maximum.z)) ||
                           ((minimum.x == NV_MAX_BOUNDS_EXTENTS) && (minimum.y == NV_MAX_BOUNDS_EXTENTS) &&
                            (minimum.z == NV_MAX_BOUNDS_EXTENTS) && (maximum.x == -NV_MAX_BOUNDS_EXTENTS) &&
                            (maximum.y == -NV_MAX_BOUNDS_EXTENTS) && (maximum.z == -NV_MAX_BOUNDS_EXTENTS))));
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVBOUNDS3_H
