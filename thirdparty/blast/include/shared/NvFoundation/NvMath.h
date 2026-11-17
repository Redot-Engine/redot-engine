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

#ifndef NV_NVFOUNDATION_NVMATH_H
#define NV_NVFOUNDATION_NVMATH_H

/** \addtogroup foundation
@{
*/

#include "NvPreprocessor.h"

#if NV_VC
#if defined(_MSC_VER) // Godot patch: MSVC-only pragma
#pragma warning(push)
#pragma warning(disable : 4985) // 'symbol name': attributes not present on previous declaration
#endif
#endif
#include <math.h>
#if NV_VC
#if defined(_MSC_VER) // Godot patch: MSVC-only pragma
#pragma warning(pop)
#endif
#endif

#include "NvAssert.h"
#include "NvIntrinsics.h"
#include <float.h>

#if !NV_DOXYGEN
namespace nvidia {
#endif

// constants
static const float NvPi = float(3.141592653589793);
static const float NvHalfPi = float(1.57079632679489661923);
static const float NvTwoPi = float(6.28318530717958647692);
static const float NvInvPi = float(0.31830988618379067154);
static const float NvInvTwoPi = float(0.15915494309189533577);
static const float NvPiDivTwo = float(1.57079632679489661923);
static const float NvPiDivFour = float(0.78539816339744830962);

/**
\brief The return value is the greater of the two specified values.
*/
template <class T>
NV_CUDA_CALLABLE NV_FORCE_INLINE T NvMax(T a, T b) {
	return a < b ? b : a;
}

//! overload for float to use fsel on xbox
template <>
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvMax(float a, float b) {
	return intrinsics::selectMax(a, b);
}

/**
\brief The return value is the lesser of the two specified values.
*/
template <class T>
NV_CUDA_CALLABLE NV_FORCE_INLINE T NvMin(T a, T b) {
	return a < b ? a : b;
}

template <>
//! overload for float to use fsel on xbox
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvMin(float a, float b) {
	return intrinsics::selectMin(a, b);
}

/*
Many of these are just implemented as NV_CUDA_CALLABLE NV_FORCE_INLINE calls to the C lib right now,
but later we could replace some of them with some approximations or more
clever stuff.
*/

/**
\brief abs returns the absolute value of its argument.
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvAbs(float a) {
	return intrinsics::abs(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvEquals(float a, float b, float eps) {
	return (NvAbs(a - b) < eps);
}

/**
\brief abs returns the absolute value of its argument.
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvAbs(double a) {
	return ::fabs(a);
}

/**
\brief abs returns the absolute value of its argument.
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE int32_t NvAbs(int32_t a) {
	return ::abs(a);
}

/**
\brief Clamps v to the range [hi,lo]
*/
template <class T>
NV_CUDA_CALLABLE NV_FORCE_INLINE T NvClamp(T v, T lo, T hi) {
	NV_ASSERT(lo <= hi);
	return NvMin(hi, NvMax(lo, v));
}

//! \brief Square root.
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvSqrt(float a) {
	return intrinsics::sqrt(a);
}

//! \brief Square root.
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvSqrt(double a) {
	return ::sqrt(a);
}

//! \brief reciprocal square root.
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvRecipSqrt(float a) {
	return intrinsics::recipSqrt(a);
}

//! \brief reciprocal square root.
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvRecipSqrt(double a) {
	return 1 / ::sqrt(a);
}

//! trigonometry -- all angles are in radians.

//! \brief Sine of an angle ( <b>Unit:</b> Radians )
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvSin(float a) {
	return intrinsics::sin(a);
}

//! \brief Sine of an angle ( <b>Unit:</b> Radians )
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvSin(double a) {
	return ::sin(a);
}

//! \brief Cosine of an angle (<b>Unit:</b> Radians)
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvCos(float a) {
	return intrinsics::cos(a);
}

//! \brief Cosine of an angle (<b>Unit:</b> Radians)
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvCos(double a) {
	return ::cos(a);
}

/**
\brief Tangent of an angle.
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvTan(float a) {
	return ::tanf(a);
}

/**
\brief Tangent of an angle.
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvTan(double a) {
	return ::tan(a);
}

/**
\brief Arcsine.
Returns angle between -PI/2 and PI/2 in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvAsin(float f) {
	return ::asinf(NvClamp(f, -1.0f, 1.0f));
}

/**
\brief Arcsine.
Returns angle between -PI/2 and PI/2 in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvAsin(double f) {
	return ::asin(NvClamp(f, -1.0, 1.0));
}

/**
\brief Arccosine.
Returns angle between 0 and PI in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvAcos(float f) {
	return ::acosf(NvClamp(f, -1.0f, 1.0f));
}

/**
\brief Arccosine.
Returns angle between 0 and PI in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvAcos(double f) {
	return ::acos(NvClamp(f, -1.0, 1.0));
}

/**
\brief ArcTangent.
Returns angle between -PI/2 and PI/2 in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvAtan(float a) {
	return ::atanf(a);
}

/**
\brief ArcTangent.
Returns angle between -PI/2 and PI/2 in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvAtan(double a) {
	return ::atan(a);
}

/**
\brief Arctangent of (x/y) with correct sign.
Returns angle between -PI and PI in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE float NvAtan2(float x, float y) {
	return ::atan2f(x, y);
}

/**
\brief Arctangent of (x/y) with correct sign.
Returns angle between -PI and PI in radians
<b>Unit:</b> Radians
*/
NV_CUDA_CALLABLE NV_FORCE_INLINE double NvAtan2(double x, double y) {
	return ::atan2(x, y);
}

//! \brief returns true if the passed number is a finite floating point number as opposed to INF, NAN, etc.
NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvIsFinite(float f) {
	return intrinsics::isFinite(f);
}

//! \brief returns true if the passed number is a finite floating point number as opposed to INF, NAN, etc.
NV_CUDA_CALLABLE NV_FORCE_INLINE bool NvIsFinite(double f) {
	return intrinsics::isFinite(f);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvFloor(float a) {
	return ::floorf(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvExp(float a) {
	return ::expf(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvCeil(float a) {
	return ::ceilf(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvSign(float a) {
	return nvidia::intrinsics::sign(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvPow(float x, float y) {
	return ::powf(x, y);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float NvLog(float x) {
	return ::logf(x);
}

#if !NV_DOXYGEN
} // namespace nvidia
#endif

/** @} */
#endif // #ifndef NV_NVFOUNDATION_NVMATH_H
