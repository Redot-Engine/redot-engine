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
//! @brief Vector math utility functions

#ifndef NVCMATH_H
#define NVCMATH_H

#include "NvCTypes.h"

/**
 * Some basic operators for NvcVec2 and NvcVec3
 */


/* NvcVec2 operators */

// Vector sum
inline NvcVec2 operator + (const NvcVec2& v, const NvcVec2& w)
{
    return { v.x + w.x, v.y + w.y };
}

// Vector difference
inline NvcVec2 operator - (const NvcVec2& v, const NvcVec2& w)
{
    return { v.x - w.x, v.y - w.y };
}

// Vector component product
inline NvcVec2 operator * (const NvcVec2& v, const NvcVec2& w)
{
    return { v.x * w.x, v.y * w.y };
}

// Vector component quotient
inline NvcVec2 operator / (const NvcVec2& v, const NvcVec2& w)
{
    return { v.x / w.x, v.y / w.y };
}

// Vector product with scalar (on right)
inline NvcVec2 operator * (const NvcVec2& v, float f)
{
    return { v.x * f, v.y * f };
}

// Vector product with scalar (on left)
inline NvcVec2 operator * (float f, const NvcVec2& v)
{
    return { f * v.x, f * v.y };
}

// Vector quotient with scalar (on right)
inline NvcVec2 operator / (const NvcVec2& v, float f)
{
    return { v.x / f, v.y / f };
}

// Vector quotient with scalar (on left)
inline NvcVec2 operator / (float f, const NvcVec2& v)
{
    return { f / v.x, f / v.y };
}

// Inner product
inline float operator | (const NvcVec2& v, const NvcVec2& w)
{
    return v.x * w.x + v.y * w.y;
}

// Vector negation
inline NvcVec2 operator - (const NvcVec2& v)
{
    return { -v.x, -v.y };
}

/* NvcVec2 assignment operators */

// Vector sum with assignment
inline NvcVec2& operator += (NvcVec2& v, const NvcVec2& w)
{
    return v = v + w;
}

// Vector difference with assignment
inline NvcVec2& operator -= (NvcVec2& v, const NvcVec2& w)
{
    return v = v - w;
}

// Vector component product with assignment
inline NvcVec2& operator *= (NvcVec2& v, const NvcVec2& w)
{
    return v = v * w;
}

// Vector component quotient with assignment
inline NvcVec2& operator /= (NvcVec2& v, const NvcVec2& w)
{
    return v = v / w;
}

// Vector product with scalar with assignment
inline NvcVec2& operator *= (NvcVec2& v, float f)
{
    return v = v * f;
}

// Vector quotient with scalar with assignment
inline NvcVec2& operator /= (NvcVec2& v, float f)
{
    return v = v / f;
}


/* NvcVec3 operators */

// Vector sum
inline NvcVec3 operator + (const NvcVec3& v, const NvcVec3& w)
{
    return { v.x + w.x, v.y + w.y, v.z + w.z };
}

// Vector difference
inline NvcVec3 operator - (const NvcVec3& v, const NvcVec3& w)
{
    return { v.x - w.x, v.y - w.y, v.z - w.z };
}

// Vector component product
inline NvcVec3 operator * (const NvcVec3& v, const NvcVec3& w)
{
    return { v.x * w.x, v.y * w.y, v.z * w.z };
}

// Vector component quotient
inline NvcVec3 operator / (const NvcVec3& v, const NvcVec3& w)
{
    return { v.x / w.x, v.y / w.y, v.z / w.z };
}

// Vector product with scalar (on right)
inline NvcVec3 operator * (const NvcVec3& v, float f)
{
    return { v.x * f, v.y * f, v.z * f };
}

// Vector product with scalar (on left)
inline NvcVec3 operator * (float f, const NvcVec3& v)
{
    return { f * v.x, f * v.y, f * v.z };
}

// Vector quotient with scalar (on right)
inline NvcVec3 operator / (const NvcVec3& v, float f)
{
    return { v.x / f, v.y / f, v.z / f };
}

// Vector quotient with scalar (on left)
inline NvcVec3 operator / (float f, const NvcVec3& v)
{
    return { f / v.x, f / v.y, f / v.z };
}

// Inner product
inline float operator | (const NvcVec3& v, const NvcVec3& w)
{
    return v.x * w.x + v.y * w.y + v.z * w.z;
}

// Cross product
inline NvcVec3 operator ^ (const NvcVec3& v, const NvcVec3& w)
{
    return { v.y * w.z - v.z * w.y, v.z * w.x - v.x * w.z, v.x * w.y - v.y * w.x };
}

// Vector negation
inline NvcVec3 operator - (const NvcVec3& v)
{
    return { -v.x, -v.y, -v.z };
}

/* NvcVec3 assignment operators */

// Vector sum with assignment
inline NvcVec3& operator += (NvcVec3& v, const NvcVec3& w)
{
    return v = v + w;
}

// Vector difference with assignment
inline NvcVec3& operator -= (NvcVec3& v, const NvcVec3& w)
{
    return v = v - w;
}

// Vector component product with assignment
inline NvcVec3& operator *= (NvcVec3& v, const NvcVec3& w)
{
    return v = v * w;
}

// Vector component quotient with assignment
inline NvcVec3& operator /= (NvcVec3& v, const NvcVec3& w)
{
    return v = v / w;
}

// Vector product with scalar with assignment
inline NvcVec3& operator *= (NvcVec3& v, float f)
{
    return v = v * f;
}

// Vector quotient with scalar with assignment
inline NvcVec3& operator /= (NvcVec3& v, float f)
{
    return v = v / f;
}

#endif  // #ifndef NVCMATH_H
