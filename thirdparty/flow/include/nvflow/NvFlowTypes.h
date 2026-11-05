// SPDX-FileCopyrightText: Copyright (c) 2014-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: BSD-3-Clause
//
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

#ifndef NV_FLOW_TYPES_H
#define NV_FLOW_TYPES_H

#include "shaders/NvFlowShaderTypes.h"

//! \cond HIDDEN_SYMBOLS
#if defined(_WIN32)
#if defined(__cplusplus)
#define NV_FLOW_API extern "C" __declspec(dllexport)
#else
#define NV_FLOW_API __declspec(dllexport)
#endif
#define NV_FLOW_ABI __cdecl
#else
#if defined(__cplusplus)
#define NV_FLOW_API extern "C" __attribute__((visibility("default")))
#else
#define NV_FLOW_API __attribute__((visibility("default")))
#endif
#define NV_FLOW_ABI
#endif

#if defined(__cplusplus)
#define NV_FLOW_INLINE inline
#else
#define NV_FLOW_INLINE static
#endif

#if defined(_WIN32)
#define NV_FLOW_FORCE_INLINE inline __forceinline
#else
#define NV_FLOW_FORCE_INLINE inline __attribute__((always_inline))
#endif

// #define NV_FLOW_DEBUG_ALLOC

//! \endcond

typedef enum NvFlowLogLevel
{
    eNvFlowLogLevel_error = 0,
    eNvFlowLogLevel_warning = 1,
    eNvFlowLogLevel_info = 2,

    eNvFlowLogLevel_maxEnum = 0x7FFFFFFF
}NvFlowLogLevel;

typedef void(NV_FLOW_ABI* NvFlowLogPrint_t)(NvFlowLogLevel level, const char* format, ...);

#define NV_FLOW_FALSE 0
#define NV_FLOW_TRUE 1

#define NV_FLOW_INFINITY ((float)(1e+300 * 1e+300))

typedef enum NvFlowType
{
    eNvFlowType_unknown = 0,
    eNvFlowType_void = 1,
    eNvFlowType_function = 2,
    eNvFlowType_struct = 3,
    eNvFlowType_int = 4,
    eNvFlowType_int2 = 5,
    eNvFlowType_int3 = 6,
    eNvFlowType_int4 = 7,
    eNvFlowType_uint = 8,
    eNvFlowType_uint2 = 9,
    eNvFlowType_uint3 = 10,
    eNvFlowType_uint4 = 11,
    eNvFlowType_float = 12,
    eNvFlowType_float2 = 13,
    eNvFlowType_float3 = 14,
    eNvFlowType_float4 = 15,
    eNvFlowType_float4x4 = 16,
    eNvFlowType_bool32 = 17,
    eNvFlowType_uint8 = 18,
    eNvFlowType_uint16 = 19,
    eNvFlowType_uint64 = 20,
    eNvFlowType_char = 21,
    eNvFlowType_double = 22,

    eNvFlowType_count = 23,
    eNvFlowType_maxEnum = 0x7FFFFFFF
}NvFlowType;

typedef enum NvFlowContextApi
{
    eNvFlowContextApi_abstract = 0,
    eNvFlowContextApi_vulkan = 1,
    eNvFlowContextApi_d3d12 = 2,
    eNvFlowContextApi_cpu = 3,

    eNvFlowContextApi_count = 4,
    eNvFlowContextApi_maxEnum = 0x7FFFFFFF
}NvFlowContextApi;

typedef enum NvFlowFormat
{
    eNvFlowFormat_unknown = 0,

    eNvFlowFormat_r32g32b32a32_float = 1,
    eNvFlowFormat_r32g32b32a32_uint = 2,
    eNvFlowFormat_r32g32b32a32_sint = 3,

    eNvFlowFormat_r32g32b32_float = 4,
    eNvFlowFormat_r32g32b32_uint = 5,
    eNvFlowFormat_r32g32b32_sint = 6,

    eNvFlowFormat_r16g16b16a16_float = 7,
    eNvFlowFormat_r16g16b16a16_unorm = 8,
    eNvFlowFormat_r16g16b16a16_uint = 9,
    eNvFlowFormat_r16g16b16a16_snorm = 10,
    eNvFlowFormat_r16g16b16a16_sint = 11,

    eNvFlowFormat_r32g32_float = 12,
    eNvFlowFormat_r32g32_uint = 13,
    eNvFlowFormat_r32g32_sint = 14,

    eNvFlowFormat_r10g10b10a2_unorm = 15,
    eNvFlowFormat_r10g10b10a2_uint = 16,
    eNvFlowFormat_r11g11b10_float = 17,

    eNvFlowFormat_r8g8b8a8_unorm = 18,
    eNvFlowFormat_r8g8b8a8_unorm_srgb = 19,
    eNvFlowFormat_r8g8b8a8_uint = 20,
    eNvFlowFormat_r8g8b8a8_snorm = 21,
    eNvFlowFormat_r8g8b8a8_sint = 22,

    eNvFlowFormat_r16g16_float = 23,
    eNvFlowFormat_r16g16_unorm = 24,
    eNvFlowFormat_r16g16_uint = 25,
    eNvFlowFormat_r16g16_snorm = 26,
    eNvFlowFormat_r16g16_sint = 27,

    eNvFlowFormat_r32_float = 28,
    eNvFlowFormat_r32_uint = 29,
    eNvFlowFormat_r32_sint = 30,

    eNvFlowFormat_r8g8_unorm = 31,
    eNvFlowFormat_r8g8_uint = 32,
    eNvFlowFormat_r8g8_snorm = 33,
    eNvFlowFormat_r8g8_sint = 34,

    eNvFlowFormat_r16_float = 35,
    eNvFlowFormat_r16_unorm = 36,
    eNvFlowFormat_r16_uint = 37,
    eNvFlowFormat_r16_snorm = 38,
    eNvFlowFormat_r16_sint = 39,

    eNvFlowFormat_r8_unorm = 40,
    eNvFlowFormat_r8_uint = 41,
    eNvFlowFormat_r8_snorm = 42,
    eNvFlowFormat_r8_sint = 43,

    eNvFlowFormat_b8g8r8a8_unorm = 44,
    eNvFlowFormat_b8g8r8a8_unorm_srgb = 45,

    eNvFlowFormat_count = 256,
    eNvFlowFormat_maxEnum = 0x7FFFFFFF
}NvFlowFormat;

#endif
