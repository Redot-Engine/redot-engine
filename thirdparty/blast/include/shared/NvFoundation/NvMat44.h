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

#ifndef NV_NVFOUNDATION_NVMAT44_H
#define NV_NVFOUNDATION_NVMAT44_H
/** \addtogroup foundation
@{
*/

#include "NvQuat.h"
#include "NvVec4.h"
#include "NvMat33.h"
#include "NvTransform.h"

#if !NV_DOXYGEN
namespace nvidia
{
#endif

/*!
\brief 4x4 matrix class

This class is layout-compatible with D3D and OpenGL matrices. More notes on layout are given in the NvMat33

@see NvMat33 NvTransform
*/

class NvMat44
{
  public:
    //! Default constructor
    NV_CUDA_CALLABLE NV_INLINE NvMat44()
    {
    }

    //! identity constructor
    NV_CUDA_CALLABLE NV_INLINE NvMat44(NvIDENTITY r)
    : column0(1.0f, 0.0f, 0.0f, 0.0f)
    , column1(0.0f, 1.0f, 0.0f, 0.0f)
    , column2(0.0f, 0.0f, 1.0f, 0.0f)
    , column3(0.0f, 0.0f, 0.0f, 1.0f)
    {
        NV_UNUSED(r);
    }

    //! zero constructor
    NV_CUDA_CALLABLE NV_INLINE NvMat44(NvZERO r) : column0(NvZero), column1(NvZero), column2(NvZero), column3(NvZero)
    {
        NV_UNUSED(r);
    }

    //! Construct from four 4-vectors
    NV_CUDA_CALLABLE NvMat44(const NvVec4& col0, const NvVec4& col1, const NvVec4& col2, const NvVec4& col3)
    : column0(col0), column1(col1), column2(col2), column3(col3)
    {
    }

    //! constructor that generates a multiple of the identity matrix
    explicit NV_CUDA_CALLABLE NV_INLINE NvMat44(float r)
    : column0(r, 0.0f, 0.0f, 0.0f)
    , column1(0.0f, r, 0.0f, 0.0f)
    , column2(0.0f, 0.0f, r, 0.0f)
    , column3(0.0f, 0.0f, 0.0f, r)
    {
    }

    //! Construct from three base vectors and a translation
    NV_CUDA_CALLABLE NvMat44(const NvVec3& col0, const NvVec3& col1, const NvVec3& col2, const NvVec3& col3)
    : column0(col0, 0), column1(col1, 0), column2(col2, 0), column3(col3, 1.0f)
    {
    }

    //! Construct from float[16]
    explicit NV_CUDA_CALLABLE NV_INLINE NvMat44(float values[])
    : column0(values[0], values[1], values[2], values[3])
    , column1(values[4], values[5], values[6], values[7])
    , column2(values[8], values[9], values[10], values[11])
    , column3(values[12], values[13], values[14], values[15])
    {
    }

    //! Construct from a quaternion
    explicit NV_CUDA_CALLABLE NV_INLINE NvMat44(const NvQuat& q)
    {
        const float x = q.x;
        const float y = q.y;
        const float z = q.z;
        const float w = q.w;

        const float x2 = x + x;
        const float y2 = y + y;
        const float z2 = z + z;

        const float xx = x2 * x;
        const float yy = y2 * y;
        const float zz = z2 * z;

        const float xy = x2 * y;
        const float xz = x2 * z;
        const float xw = x2 * w;

        const float yz = y2 * z;
        const float yw = y2 * w;
        const float zw = z2 * w;

        column0 = NvVec4(1.0f - yy - zz, xy + zw, xz - yw, 0.0f);
        column1 = NvVec4(xy - zw, 1.0f - xx - zz, yz + xw, 0.0f);
        column2 = NvVec4(xz + yw, yz - xw, 1.0f - xx - yy, 0.0f);
        column3 = NvVec4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    //! Construct from a diagonal vector
    explicit NV_CUDA_CALLABLE NV_INLINE NvMat44(const NvVec4& diagonal)
    : column0(diagonal.x, 0.0f, 0.0f, 0.0f)
    , column1(0.0f, diagonal.y, 0.0f, 0.0f)
    , column2(0.0f, 0.0f, diagonal.z, 0.0f)
    , column3(0.0f, 0.0f, 0.0f, diagonal.w)
    {
    }

    //! Construct from Mat33 and a translation
    NV_CUDA_CALLABLE NvMat44(const NvMat33& axes, const NvVec3& position)
    : column0(axes.column0, 0.0f), column1(axes.column1, 0.0f), column2(axes.column2, 0.0f), column3(position, 1.0f)
    {
    }

    NV_CUDA_CALLABLE NvMat44(const NvTransform& t)
    {
        *this = NvMat44(NvMat33(t.q), t.p);
    }

    /**
    \brief returns true if the two matrices are exactly equal
    */
    NV_CUDA_CALLABLE NV_INLINE bool operator==(const NvMat44& m) const
    {
        return column0 == m.column0 && column1 == m.column1 && column2 == m.column2 && column3 == m.column3;
    }

    //! Copy constructor
    NV_CUDA_CALLABLE NV_INLINE NvMat44(const NvMat44& other)
    : column0(other.column0), column1(other.column1), column2(other.column2), column3(other.column3)
    {
    }

    //! Assignment operator
    NV_CUDA_CALLABLE NV_INLINE const NvMat44& operator=(const NvMat44& other)
    {
        column0 = other.column0;
        column1 = other.column1;
        column2 = other.column2;
        column3 = other.column3;
        return *this;
    }

    //! Get transposed matrix
    NV_CUDA_CALLABLE NV_INLINE NvMat44 getTranspose() const
    {
        return NvMat44(
            NvVec4(column0.x, column1.x, column2.x, column3.x), NvVec4(column0.y, column1.y, column2.y, column3.y),
            NvVec4(column0.z, column1.z, column2.z, column3.z), NvVec4(column0.w, column1.w, column2.w, column3.w));
    }

    //! Unary minus
    NV_CUDA_CALLABLE NV_INLINE NvMat44 operator-() const
    {
        return NvMat44(-column0, -column1, -column2, -column3);
    }

    //! Add
    NV_CUDA_CALLABLE NV_INLINE NvMat44 operator+(const NvMat44& other) const
    {
        return NvMat44(column0 + other.column0, column1 + other.column1, column2 + other.column2,
                       column3 + other.column3);
    }

    //! Subtract
    NV_CUDA_CALLABLE NV_INLINE NvMat44 operator-(const NvMat44& other) const
    {
        return NvMat44(column0 - other.column0, column1 - other.column1, column2 - other.column2,
                       column3 - other.column3);
    }

    //! Scalar multiplication
    NV_CUDA_CALLABLE NV_INLINE NvMat44 operator*(float scalar) const
    {
        return NvMat44(column0 * scalar, column1 * scalar, column2 * scalar, column3 * scalar);
    }

    friend NvMat44 operator*(float, const NvMat44&);

    //! Matrix multiplication
    NV_CUDA_CALLABLE NV_INLINE NvMat44 operator*(const NvMat44& other) const
    {
        // Rows from this <dot> columns from other
        // column0 = transform(other.column0) etc
        return NvMat44(transform(other.column0), transform(other.column1), transform(other.column2),
                       transform(other.column3));
    }

    // a <op>= b operators

    //! Equals-add
    NV_CUDA_CALLABLE NV_INLINE NvMat44& operator+=(const NvMat44& other)
    {
        column0 += other.column0;
        column1 += other.column1;
        column2 += other.column2;
        column3 += other.column3;
        return *this;
    }

    //! Equals-sub
    NV_CUDA_CALLABLE NV_INLINE NvMat44& operator-=(const NvMat44& other)
    {
        column0 -= other.column0;
        column1 -= other.column1;
        column2 -= other.column2;
        column3 -= other.column3;
        return *this;
    }

    //! Equals scalar multiplication
    NV_CUDA_CALLABLE NV_INLINE NvMat44& operator*=(float scalar)
    {
        column0 *= scalar;
        column1 *= scalar;
        column2 *= scalar;
        column3 *= scalar;
        return *this;
    }

    //! Equals matrix multiplication
    NV_CUDA_CALLABLE NV_INLINE NvMat44& operator*=(const NvMat44& other)
    {
        *this = *this * other;
        return *this;
    }

    //! Element access, mathematical way!
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE float operator()(unsigned int row, unsigned int col) const
    {
        return (*this)[col][row];
    }

    //! Element access, mathematical way!
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE float& operator()(unsigned int row, unsigned int col)
    {
        return (*this)[col][row];
    }

    //! Transform vector by matrix, equal to v' = M*v
    NV_CUDA_CALLABLE NV_INLINE NvVec4 transform(const NvVec4& other) const
    {
        return column0 * other.x + column1 * other.y + column2 * other.z + column3 * other.w;
    }

    //! Transform vector by matrix, equal to v' = M*v
    NV_CUDA_CALLABLE NV_INLINE NvVec3 transform(const NvVec3& other) const
    {
        return transform(NvVec4(other, 1.0f)).getXYZ();
    }

    //! Rotate vector by matrix, equal to v' = M*v
    NV_CUDA_CALLABLE NV_INLINE const NvVec4 rotate(const NvVec4& other) const
    {
        return column0 * other.x + column1 * other.y + column2 * other.z; // + column3*0;
    }

    //! Rotate vector by matrix, equal to v' = M*v
    NV_CUDA_CALLABLE NV_INLINE const NvVec3 rotate(const NvVec3& other) const
    {
        return rotate(NvVec4(other, 1.0f)).getXYZ();
    }

    NV_CUDA_CALLABLE NV_INLINE NvVec3 getBasis(int num) const
    {
        NV_ASSERT(num >= 0 && num < 3);
        return (&column0)[num].getXYZ();
    }

    NV_CUDA_CALLABLE NV_INLINE NvVec3 getPosition() const
    {
        return column3.getXYZ();
    }

    NV_CUDA_CALLABLE NV_INLINE void setPosition(const NvVec3& position)
    {
        column3.x = position.x;
        column3.y = position.y;
        column3.z = position.z;
    }

    NV_CUDA_CALLABLE NV_FORCE_INLINE const float* front() const
    {
        return &column0.x;
    }

    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE NvVec4& operator[](unsigned int num)
    {
        return (&column0)[num];
    }
    NV_DEPRECATED NV_CUDA_CALLABLE NV_FORCE_INLINE const NvVec4& operator[](unsigned int num) const
    {
        return (&column0)[num];
    }

    NV_CUDA_CALLABLE NV_INLINE void scale(const NvVec4& p)
    {
        column0 *= p.x;
        column1 *= p.y;
        column2 *= p.z;
        column3 *= p.w;
    }

    NV_CUDA_CALLABLE NV_INLINE NvMat44 inverseRT(void) const
    {
        NvVec3 r0(column0.x, column1.x, column2.x), r1(column0.y, column1.y, column2.y),
            r2(column0.z, column1.z, column2.z);

        return NvMat44(r0, r1, r2, -(r0 * column3.x + r1 * column3.y + r2 * column3.z));
    }

    NV_CUDA_CALLABLE NV_INLINE bool isFinite() const
    {
        return column0.isFinite() && column1.isFinite() && column2.isFinite() && column3.isFinite();
    }

    // Data, see above for format!

    NvVec4 column0, column1, column2, column3; // the four base vectors
};

// implementation from NvTransform.h
NV_CUDA_CALLABLE NV_FORCE_INLINE NvTransform::NvTransform(const NvMat44& m)
{
    NvVec3 column0 = NvVec3(m.column0.x, m.column0.y, m.column0.z);
    NvVec3 column1 = NvVec3(m.column1.x, m.column1.y, m.column1.z);
    NvVec3 column2 = NvVec3(m.column2.x, m.column2.y, m.column2.z);

    q = NvQuat(NvMat33(column0, column1, column2));
    p = NvVec3(m.column3.x, m.column3.y, m.column3.z);
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVMAT44_H
