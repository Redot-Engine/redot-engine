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

#ifndef NV_C_TYPES_H
#define NV_C_TYPES_H

#include "NvPreprocessor.h"
#ifdef _MSC_VER
#ifndef _INTPTR
#define _INTPTR 0
#endif
#endif
#include <stdint.h>

/**  C type for 2-float vectors */
typedef struct
{
    float x, y;
} NvcVec2;

/**  C type for 3-float vectors */
typedef struct
{
    float x, y, z;
} NvcVec3;

/**  C type for 4-float vectors */
typedef struct
{
    float x, y, z, w;
} NvcVec4;

/**  C type for quaternions */
typedef struct
{
    float x, y, z, w;
} NvcQuat;

/**  C type for transforms */
typedef struct
{
    NvcQuat q;
    NvcVec3 p;
} NvcTransform;

/**  C type for 3x3 matrices */
typedef struct
{
    NvcVec3 column0, column1, column2, column3;
} NvcMat34;

/**  C type for 3x3 matrices */
typedef struct
{
    NvcVec3 column0, column1, column2;
} NvcMat33;

/**  C type for 4x4 matrices */
typedef struct
{
    NvcVec4 column0, column1, column2, column3;
} NvcMat44;

/** C type for 3d bounding box */
typedef struct
{
    NvcVec3 minimum;
    NvcVec3 maximum;
} NvcBounds3;

/** C type for a plane */
typedef struct
{
    NvcVec3 n;
    float d;
} NvcPlane;

/**  C type for 2-integer vectors */
typedef struct
{
    int32_t x, y;
} NvcVec2i;

/**  C type for 3-integer vectors */
typedef struct
{
    int32_t x, y, z;
} NvcVec3i;

/**  C type for 4-integer vectors */
typedef struct
{
    int32_t x, y, z, w;
} NvcVec4i;

/** @} */

#endif // NV_C_TYPES_H
