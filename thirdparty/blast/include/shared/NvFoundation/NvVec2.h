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

#ifndef NV_NVFOUNDATION_NVVEC2_H
#define NV_NVFOUNDATION_NVVEC2_H

/** \addtogroup foundation
@{
*/

#include "NvMath.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/**
\brief 2 Element vector class.

This is a 2-dimensional vector class with public data members.
*/
class NvVec2
{
  public:
    /**
    \brief default constructor leaves data uninitialized.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2()
    {
    }

    /**
    \brief zero constructor.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2(NvZERO r) : x(0.0f), y(0.0f)
    {
        NV_UNUSED(r);
    }

    /**
    \brief Assigns scalar parameter to all elements.

    Useful to initialize to zero or one.

    \param[in] a Value to assign to elements.
    */
    explicit NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2(float a) : x(a), y(a)
    {
    }

    /**
    \brief Initializes from 2 scalar parameters.

    \param[in] nx Value to initialize X component.
    \param[in] ny Value to initialize Y component.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2(float nx, float ny) : x(nx), y(ny)
    {
    }

    /**
    \brief Copy ctor.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2(const NvVec2& v) : x(v.x), y(v.y)
    {
    }

    // Operators

    /**
    \brief Assignment operator
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2& operator=(const NvVec2& p)
    {
        x = p.x;
        y = p.y;
        return *this;
    }

    /**
    \brief element access
    */
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE float& operator[](int index)
    {
        NV_ASSERT(index >= 0 && index <= 1);

        return reinterpret_cast<float*>(this)[index];
    }

    /**
    \brief element access
    */
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE const float& operator[](int index) const
    {
        NV_ASSERT(index >= 0 && index <= 1);

        return reinterpret_cast<const float*>(this)[index];
    }

    /**
    \brief returns true if the two vectors are exactly equal.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool operator==(const NvVec2& v) const
    {
        return x == v.x && y == v.y;
    }

    /**
    \brief returns true if the two vectors are not exactly equal.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool operator!=(const NvVec2& v) const
    {
        return x != v.x || y != v.y;
    }

    /**
    \brief tests for exact zero vector
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE bool isZero() const
    {
        return x == 0.0f && y == 0.0f;
    }

    /**
    \brief returns true if all 2 elems of the vector are finite (not NAN or INF, etc.)
    */
    NV_CUDA_CALLABLE NV_INLINE bool isFinite() const
    {
        return NvIsFinite(x) && NvIsFinite(y);
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
        return x * x + y * y;
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
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 operator-() const
    {
        return NvVec2(-x, -y);
    }

    /**
    \brief vector addition
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 operator+(const NvVec2& v) const
    {
        return NvVec2(x + v.x, y + v.y);
    }

    /**
    \brief vector difference
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 operator-(const NvVec2& v) const
    {
        return NvVec2(x - v.x, y - v.y);
    }

    /**
    \brief scalar post-multiplication
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 operator*(float f) const
    {
        return NvVec2(x * f, y * f);
    }

    /**
    \brief scalar division
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 operator/(float f) const
    {
        f = 1.0f / f; // PT: inconsistent notation with operator /=
        return NvVec2(x * f, y * f);
    }

    /**
    \brief vector addition
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2& operator+=(const NvVec2& v)
    {
        x += v.x;
        y += v.y;
        return *this;
    }

    /**
    \brief vector difference
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2& operator-=(const NvVec2& v)
    {
        x -= v.x;
        y -= v.y;
        return *this;
    }

    /**
    \brief scalar multiplication
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2& operator*=(float f)
    {
        x *= f;
        y *= f;
        return *this;
    }
    /**
    \brief scalar division
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2& operator/=(float f)
    {
        f = 1.0f / f; // PT: inconsistent notation with operator /
        x *= f;
        y *= f;
        return *this;
    }

    /**
    \brief returns the scalar product of this and other.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float dot(const NvVec2& v) const
    {
        return x * v.x + y * v.y;
    }

    /** return a unit vector */

    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 getNormalized() const
    {
        const float m = magnitudeSquared();
        return m > 0.0f ? *this * NvRecipSqrt(m) : NvVec2(0, 0);
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
    \brief a[i] * b[i], for all i.
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 multiply(const NvVec2& a) const
    {
        return NvVec2(x * a.x, y * a.y);
    }

    /**
    \brief element-wise minimum
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 minimum(const NvVec2& v) const
    {
        return NvVec2(NvMin(x, v.x), NvMin(y, v.y));
    }

    /**
    \brief returns MIN(x, y);
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float minElement() const
    {
        return NvMin(x, y);
    }

    /**
    \brief element-wise maximum
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec2 maximum(const NvVec2& v) const
    {
        return NvVec2(NvMax(x, v.x), NvMax(y, v.y));
    }

    /**
    \brief returns MAX(x, y);
    */
    NV_CUDA_CALLABLE NV_FORCE_INLINE float maxElement() const
    {
        return NvMax(x, y);
    }

    float x, y;
};

NV_CUDA_CALLABLE static NV_FORCE_INLINE NvVec2 operator*(float f, const NvVec2& v)
{
    return NvVec2(f * v.x, f * v.y);
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVVEC2_H
