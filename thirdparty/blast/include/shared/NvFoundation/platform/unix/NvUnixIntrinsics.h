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

#ifndef NV_UNIX_NVUNIXINTRINSICS_H
#define NV_UNIX_NVUNIXINTRINSICS_H

#include "Nv.h"
#include "NvAssert.h"

#if !(NV_LINUX || NV_ANDROID || NV_PS4 || NV_APPLE_FAMILY)
#error "This file should only be included by Unix builds!!"
#endif

#include <math.h>
#include <float.h>

namespace nvidia
{
namespace intrinsics
{
//! \brief platform-specific absolute value
NV_CUDA_CALLABLE NV_FORCE_INLINE float abs(float a)
{
    return ::fabsf(a);
}

//! \brief platform-specific select float
NV_CUDA_CALLABLE NV_FORCE_INLINE float fsel(float a, float b, float c)
{
    return (a >= 0.0f) ? b : c;
}

//! \brief platform-specific sign
NV_CUDA_CALLABLE NV_FORCE_INLINE float sign(float a)
{
    return (a >= 0.0f) ? 1.0f : -1.0f;
}

//! \brief platform-specific reciprocal
NV_CUDA_CALLABLE NV_FORCE_INLINE float recip(float a)
{
    return 1.0f / a;
}

//! \brief platform-specific reciprocal estimate
NV_CUDA_CALLABLE NV_FORCE_INLINE float recipFast(float a)
{
    return 1.0f / a;
}

//! \brief platform-specific square root
NV_CUDA_CALLABLE NV_FORCE_INLINE float sqrt(float a)
{
    return ::sqrtf(a);
}

//! \brief platform-specific reciprocal square root
NV_CUDA_CALLABLE NV_FORCE_INLINE float recipSqrt(float a)
{
    return 1.0f / ::sqrtf(a);
}

NV_CUDA_CALLABLE NV_FORCE_INLINE float recipSqrtFast(float a)
{
    return 1.0f / ::sqrtf(a);
}

//! \brief platform-specific sine
NV_CUDA_CALLABLE NV_FORCE_INLINE float sin(float a)
{
    return ::sinf(a);
}

//! \brief platform-specific cosine
NV_CUDA_CALLABLE NV_FORCE_INLINE float cos(float a)
{
    return ::cosf(a);
}

//! \brief platform-specific minimum
NV_CUDA_CALLABLE NV_FORCE_INLINE float selectMin(float a, float b)
{
    return a < b ? a : b;
}

//! \brief platform-specific maximum
NV_CUDA_CALLABLE NV_FORCE_INLINE float selectMax(float a, float b)
{
    return a > b ? a : b;
}

//! \brief platform-specific finiteness check (not INF or NAN)
NV_CUDA_CALLABLE NV_FORCE_INLINE bool isFinite(float a)
{
    return !!isfinite(a);
}

//! \brief platform-specific finiteness check (not INF or NAN)
NV_CUDA_CALLABLE NV_FORCE_INLINE bool isFinite(double a)
{
    return !!isfinite(a);
}

/*!
Sets \c count bytes starting at \c dst to zero.
*/
NV_FORCE_INLINE void* memZero(void* NV_RESTRICT dest, uint32_t count)
{
    return memset(dest, 0, count);
}

/*!
Sets \c count bytes starting at \c dst to \c c.
*/
NV_FORCE_INLINE void* memSet(void* NV_RESTRICT dest, int32_t c, uint32_t count)
{
    return memset(dest, c, count);
}

/*!
Copies \c count bytes from \c src to \c dst. User memMove if regions overlap.
*/
NV_FORCE_INLINE void* memCopy(void* NV_RESTRICT dest, const void* NV_RESTRICT src, uint32_t count)
{
    return memcpy(dest, src, count);
}

/*!
Copies \c count bytes from \c src to \c dst. Supports overlapping regions.
*/
NV_FORCE_INLINE void* memMove(void* NV_RESTRICT dest, const void* NV_RESTRICT src, uint32_t count)
{
    return memmove(dest, src, count);
}

/*!
Set 128B to zero starting at \c dst+offset. Must be aligned.
*/
NV_FORCE_INLINE void memZero128(void* NV_RESTRICT dest, uint32_t offset = 0)
{
    NV_ASSERT(((size_t(dest) + offset) & 0x7f) == 0);
    memSet(reinterpret_cast<char * NV_RESTRICT>(dest) + offset, 0, 128);
}

} // namespace intrinsics
} // namespace nvidia

#endif // #ifndef NV_UNIX_NVUNIXINTRINSICS_H
