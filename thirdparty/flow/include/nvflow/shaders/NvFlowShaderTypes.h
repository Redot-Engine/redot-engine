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


#ifndef NV_FLOW_SHADER_TYPES_H
#define NV_FLOW_SHADER_TYPES_H

#ifndef NV_FLOW_CPU
#if defined(__cplusplus)
#define NV_FLOW_CPU 1
#endif
#endif

#ifdef NV_FLOW_CPU_SHADER

// For CPU Shader, basic types defined at global scope before shader

#elif defined(NV_FLOW_CPU)

typedef unsigned int NvFlowBool32;
typedef unsigned int NvFlowUint;
typedef unsigned char NvFlowUint8;
typedef unsigned short NvFlowUint16;
typedef unsigned long long NvFlowUint64;

typedef struct NvFlowUint2
{
    NvFlowUint x, y;
}NvFlowUint2;

typedef struct NvFlowUint3
{
    NvFlowUint x, y, z;
}NvFlowUint3;

typedef struct NvFlowUint4
{
    NvFlowUint x, y, z, w;
}NvFlowUint4;

typedef struct NvFlowInt2
{
    int x, y;
}NvFlowInt2;

typedef struct NvFlowInt3
{
    int x, y, z;
}NvFlowInt3;

typedef struct NvFlowInt4
{
    int x, y, z, w;
}NvFlowInt4;

typedef struct NvFlowFloat2
{
    float x, y;
}NvFlowFloat2;

typedef struct NvFlowFloat3
{
    float x, y, z;
}NvFlowFloat3;

typedef struct NvFlowFloat4
{
    float x, y, z, w;
}NvFlowFloat4;

typedef struct NvFlowFloat4x4
{
    NvFlowFloat4 x, y, z, w;
}NvFlowFloat4x4;

#else

#define NvFlowUint uint
#define NvFlowUint2 uint2
#define NvFlowUint3 uint3
#define NvFlowUint4 uint4
#define NvFlowInt2 int2
#define NvFlowInt3 int3
#define NvFlowInt4 int4
#define NvFlowFloat2 float2
#define NvFlowFloat3 float3
#define NvFlowFloat4 float4
#define NvFlowFloat4x4 float4x4
#define NvFlowBool32 uint

#endif

struct NvFlowSparseLevelParams
{
    NvFlowUint3 blockDimLessOne;
    NvFlowUint threadsPerBlock;

    NvFlowUint3 blockDimBits;
    NvFlowUint numLocations;

    NvFlowUint3 tableDimLessOne;
    NvFlowUint tableDim3;

    NvFlowUint tableDimBits_x;
    NvFlowUint tableDimBits_xy;
    NvFlowUint tableDimBits_z;
    NvFlowUint locationOffset;

    NvFlowUint allocationOffset;
    NvFlowUint newListOffset;
    NvFlowUint blockLevelOffsetGlobal;
    NvFlowUint blockLevelOffsetLocal;

    NvFlowUint layerParamIdxOffset;
    NvFlowUint numLayers;
    NvFlowUint pad0;
    NvFlowUint pad1;

    NvFlowUint3 dim;
    NvFlowUint maxLocations;
    NvFlowFloat3 dimInv;
    NvFlowUint numNewLocations;

    NvFlowInt4 globalLocationMin;
    NvFlowInt4 globalLocationMax;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowSparseLevelParams NvFlowSparseLevelParams;
#endif

struct NvFlowSparseLayerParams
{
    NvFlowFloat3 blockSizeWorld;
    float blockSizeWorld3;
    NvFlowFloat3 blockSizeWorldInv;
    int layerAndLevel;
    NvFlowInt4 locationMin;
    NvFlowInt4 locationMax;
    NvFlowFloat3 worldMin;
    NvFlowUint forceClear;
    NvFlowFloat3 worldMax;
    NvFlowUint forceDisableEmitters;
    NvFlowUint numLocations;
    float deltaTime;
    NvFlowUint forceDisableCoreSimulation;
    NvFlowUint gridReset;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowSparseLayerParams NvFlowSparseLayerParams;
#endif

struct NvFlowSparseNanoVdbParams
{
    NvFlowUint2 nanovdb_size_without_leaves;
    NvFlowUint2 nanovdb_size_with_leaves;

    NvFlowUint2 list_tile_offset;
    NvFlowUint2 list_upper_offset;
    NvFlowUint2 list_lower_offset;
    NvFlowUint2 list_leaf_offset;

    NvFlowUint2 cache_tile_offset;
    NvFlowUint2 cache_upper_offset;
    NvFlowUint2 cache_lower_offset;
    NvFlowUint2 cache_leaf_offset;

    NvFlowUint list_tile_count;
    NvFlowUint list_upper_count;
    NvFlowUint list_lower_count;
    NvFlowUint list_leaf_count;

    NvFlowUint cache_tile_count;
    NvFlowUint cache_upper_count;
    NvFlowUint cache_lower_count;
    NvFlowUint cache_leaf_count;

    NvFlowUint2 cache_size;
    NvFlowUint grid_count;
    NvFlowUint grid_type;

    NvFlowUint3 subGridDimLessOne;
    NvFlowUint pad3;
    NvFlowUint3 subGridDimBits;
    NvFlowUint pad4;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowSparseNanoVdbParams NvFlowSparseNanoVdbParams;
#endif

#endif
