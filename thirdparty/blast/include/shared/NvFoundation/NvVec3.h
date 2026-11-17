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

#ifndef NV_NVFOUNDATION_NVVEC3_H
#define NV_NVFOUNDATION_NVVEC3_H

/** \addtogroup foundation
@{
*/

#include "NvMath.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/**
\brief 3 Element vector class.

This is a 3-dimensional vector class with public data members.
*/
class NvVec3
{
  public:
    /**
    \brief default constructor leaves data uninitialized.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3()
    {
    }

    /**
    \brief zero constructor.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3(NvZERO r) : x(0.0f), y(0.0f), z(0.0f)
    {
        NV_UNUSED(r);
    }

    /**
    \brief Assigns scalar parameter to all elements.

    Useful to initialize to zero or one.

    \param[in] a Value to assign to elements.
    */
    explicit NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3(float a) : x(a), y(a), z(a)
    {
    }

    /**
    \brief Initializes from 3 scalar parameters.

    \param[in] nx Value to initialize X component.
    \param[in] ny Value to initialize Y component.
    \param[in] nz Value to initialize Z component.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3(float nx, float ny, float nz) : x(nx), y(ny), z(nz)
    {
    }

    /**
    \brief Copy ctor.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3(const NvVec3& v) : x(v.x), y(v.y), z(v.z)
    {
    }

    // Operators

    /**
    \brief Assignment operator
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3& operator=(const NvVec3& p)
    {
        x = p.x;
        y = p.y;
        z = p.z;
        return *this;
    }

    /**
    \brief element access
    */
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE float& operator[](unsigned int index)
    {
        NV_ASSERT(index <= 2);

        return reinterpret_cast<float*>(this)[index];
    }

    /**
    \brief element access
    */
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE const float& operator[](unsigned int index) const
    {
        NV_ASSERT(index <= 2);

        return reinterpret_cast<const float*>(this)[index];
    }
    /**
    \brief returns true if the two vectors are exactly equal.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool operator==(const NvVec3& v) const
    {
        return x == v.x && y == v.y && z == v.z;
    }

    /**
    \brief returns true if the two vectors are not exactly equal.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool operator!=(const NvVec3& v) const
    {
        return x != v.x || y != v.y || z != v.z;
    }

    /**
    \brief tests for exact zero vector
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isZero() const
    {
        return x == 0.0f && y == 0.0f && z == 0.0f;
    }

    /**
    \brief returns true if all 3 elems of the vector are finite (not NAN or INF, etc.)
    */
    NV_CUDA_CALLABLE NV_INLINE bool isFinite() const
    {
        return NvIsFinite(x) && NvIsFinite(y) && NvIsFinite(z);
    }

    /**
    \brief is normalized - used by API parameter validation
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isNormalized() const
    {
        const float unitTolerance = 1e-4f;
        return isFinite() && NvAbs(magnitude() - 1) < unitTolerance;
    }

    /**
    \brief returns the squared magnitude

    Avoids calling NvSqrt()!
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float magnitudeSquared() const
    {
        return x * x + y * y + z * z;
    }

    /**
    \brief returns the magnitude
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float magnitude() const
    {
        return NvSqrt(magnitudeSquared());
    }

    /**
    \brief negation
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 operator-() const
    {
        return NvVec3(-x, -y, -z);
    }

    /**
    \brief vector addition
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 operator+(const NvVec3& v) const
    {
        return NvVec3(x + v.x, y + v.y, z + v.z);
    }

    /**
    \brief vector difference
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 operator-(const NvVec3& v) const
    {
        return NvVec3(x - v.x, y - v.y, z - v.z);
    }

    /**
    \brief scalar post-multiplication
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 operator*(float f) const
    {
        return NvVec3(x * f, y * f, z * f);
    }

    /**
    \brief scalar division
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 operator/(float f) const
    {
        f = 1.0f / f;
        return NvVec3(x * f, y * f, z * f);
    }

    /**
    \brief vector addition
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3& operator+=(const NvVec3& v)
    {
        x += v.x;
        y += v.y;
        z += v.z;
        return *this;
    }

    /**
    \brief vector difference
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3& operator-=(const NvVec3& v)
    {
        x -= v.x;
        y -= v.y;
        z -= v.z;
        return *this;
    }

    /**
    \brief scalar multiplication
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3& operator*=(float f)
    {
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }
    /**
    \brief scalar division
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3& operator/=(float f)
    {
        f = 1.0f / f;
        x *= f;
        y *= f;
        z *= f;
        return *this;
    }

    /**
    \brief returns the scalar product of this and other.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float dot(const NvVec3& v) const
    {
        return x * v.x + y * v.y + z * v.z;
    }

    /**
    \brief cross product
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 cross(const NvVec3& v) const
    {
        return NvVec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    /** return a unit vector */

    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 getNormalized() const
    {
        const float m = magnitudeSquared();
        return m > 0.0f ? *this * NvRecipSqrt(m) : NvVec3(0, 0, 0);
    }

    /**
    \brief normalizes the vector in place
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float normalize()
    {
        const float m = magnitude();
        if(m > 0.0f)
            *this /= m;
        return m;
    }

    /**
    \brief normalizes the vector in place. Does nothing if vector magnitude is under NV_NORMALIZATION_EPSILON.
    Returns vector magnitude if >= NV_NORMALIZATION_EPSILON and 0.0f otherwise.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float normalizeSafe()
    {
        const float mag = magnitude();
        if(mag < NV_NORMALIZATION_EPSILON)
            return 0.0f;
        *this *= 1.0f / mag;
        return mag;
    }

    /**
    \brief normalizes the vector in place. Asserts if vector magnitude is under NV_NORMALIZATION_EPSILON.
    returns vector magnitude.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float normalizeFast()
    {
        const float mag = magnitude();
        NV_ASSERT(mag >= NV_NORMALIZATION_EPSILON);
        *this *= 1.0f / mag;
        return mag;
    }

    /**
    \brief a[i] * b[i], for all i.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 multiply(const NvVec3& a) const
    {
        return NvVec3(x * a.x, y * a.y, z * a.z);
    }

    /**
    \brief element-wise minimum
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 minimum(const NvVec3& v) const
    {
        return NvVec3(NvMin(x, v.x), NvMin(y, v.y), NvMin(z, v.z));
    }

    /**
    \brief returns MIN(x, y, z);
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float minElement() const
    {
        return NvMin(x, NvMin(y, z));
    }

    /**
    \brief element-wise maximum
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 maximum(const NvVec3& v) const
    {
        return NvVec3(NvMax(x, v.x), NvMax(y, v.y), NvMax(z, v.z));
    }

    /**
    \brief returns MAX(x, y, z);
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float maxElement() const
    {
        return NvMax(x, NvMax(y, z));
    }

    /**
    \brief returns absolute values of components;
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec3 abs() const
    {
        return NvVec3(NvAbs(x), NvAbs(y), NvAbs(z));
    }

    float x, y, z;
};

NV_CUDA_CALLABLE static NV_FORCE_INLINE NvVec3 operator*(float f, const NvVec3& v)
{
    return NvVec3(f * v.x, f * v.y, f * v.z);
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVVEC3_H
