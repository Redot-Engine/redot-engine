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


#ifndef NV_FLOW_SHADER_HLSLI
#define NV_FLOW_SHADER_HLSLI

#include "NvFlowShaderTypes.h"

int3 NvFlowFloor_i(float3 v)
{
    return int3(floor(v));
}

int4 NvFlowTableValueToIndexWrite(int3 threadIdx, uint v, int x, int y, int z)
{
    int4 ridx = int4(
        (threadIdx.x + int((v <<  1u) | 1u)) & 0xFFF,
        (threadIdx.y + int((v >> 10u) | 1u)) & 0x7FF,
        (threadIdx.z + int((v >> 20u) | 1u)) & 0x7FF,
        int(v >> 31u)
    );
    int mask = int(~v) >> 31;
    ridx.x += (mask & x);
    ridx.y += (mask & y);
    ridx.z += (mask & z);
    return ridx;
}

int4 NvFlowTableValueToIndexRead(int3 threadIdx, uint v)
{
    int4 ridx = int4(
        (threadIdx.x + int((v <<  1u) | 1u)) & 0xFFF,
        (threadIdx.y + int((v >> 10u) | 1u)) & 0x7FF,
        (threadIdx.z + int((v >> 20u) | 1u)) & 0x7FF,
        int(v >> 31u)
    );
    ridx.x = ridx.x | int(~v & 0x80000000);
    return ridx;
}

uint NvFlowNeighborToIdx(uint blockIdx, int x, int y, int z)
{
    return uint((blockIdx << 5u) + (uint(((z << 3) + z) + ((y << 1) + y) + x + 13 + 5) & 31u));
}

uint NvFlowSingleToIdx(uint blockIdx)
{
    return uint((blockIdx << 5u) + 13u + 5u);
}

int3 NvFlowComputeThreadIdx(NvFlowSparseLevelParams tableParams, uint threadIdx1D)
{
    return int3(
        int(int(threadIdx1D) & int(tableParams.blockDimLessOne.x)),
        int((int(threadIdx1D) >> int(tableParams.blockDimBits.x)) & int(tableParams.blockDimLessOne.y)),
        int((int(threadIdx1D) >> int(tableParams.blockDimBits.x + tableParams.blockDimBits.y)) & int(tableParams.blockDimLessOne.z))
    );
}

uint NvFlowGetLayerParamIdx(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx)
{
    uint layerParamIdx;
    if (blockIdx < tableParams.numLocations)
    {
        uint layerParamIdxIdx = blockIdx + tableParams.layerParamIdxOffset;
        layerParamIdx = table[layerParamIdxIdx];
    }
    else
    {
        layerParamIdx = 0u;
    }
    return layerParamIdx;
}

uint NvFlowLocationToBlockIdx(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 location)
{
    uint3 bucketIdx = uint3(uint3(location.xyz) & tableParams.tableDimLessOne);
    uint bucketIdx1D = (bucketIdx.z << (tableParams.tableDimBits_xy)) |
        (bucketIdx.y << tableParams.tableDimBits_x) |
        (bucketIdx.x);
    uint range_begin = table[(bucketIdx1D << 1u) + 0u];
    uint range_end = table[(bucketIdx1D << 1u) + 1u];
    uint outBlockIdx = ~0u;
    for (uint blockIdx = range_begin; blockIdx < range_end; blockIdx++)
    {
        uint compareLocationIdx = (blockIdx << 2) + tableParams.locationOffset;
        int compareLocation_x = int(table[compareLocationIdx + 0]);
        int compareLocation_y = int(table[compareLocationIdx + 1]);
        int compareLocation_z = int(table[compareLocationIdx + 2]);
        int compareLocation_w = int(table[compareLocationIdx + 3]);
        if (compareLocation_x == location.x &&
            compareLocation_y == location.y &&
            compareLocation_z == location.z &&
            compareLocation_w == location.w)
        {
            outBlockIdx = blockIdx;
            break;
        }
    }
    return outBlockIdx;
}

bool NvFlowBlockIdxToLocation(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, out int4 out_location)
{
    bool ret;
    if (blockIdx < tableParams.numLocations)
    {
        uint locationIdx = (blockIdx << 2) + tableParams.locationOffset;
        out_location.x = int(table[locationIdx + 0]);
        out_location.y = int(table[locationIdx + 1]);
        out_location.z = int(table[locationIdx + 2]);
        out_location.w = int(table[locationIdx + 3]);
        ret = true;
    }
    else
    {
        out_location.x = 0x40000000;
        out_location.y = 0x40000000;
        out_location.z = 0x40000000;
        out_location.w = 0x40000000;
        ret = false;
    }
    return ret;
}

uint NvFlowNewListIdxToBlockIdx(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint newListIdx)
{
    return table[newListIdx + tableParams.newListOffset];
}

bool NvFlowRealToVirtual(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, out int4 out_vidx)
{
    int4 location;
    bool ret = NvFlowBlockIdxToLocation(table, tableParams, blockIdx, location);
    out_vidx = int4((location.xyz << int3(tableParams.blockDimBits)) + threadIdx, location.w);
    return ret;
}

int4 NvFlowGlobalVirtualToReal(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx)
{
    int4 location = int4(vidx.xyz >> int3(tableParams.blockDimBits), vidx.w);
    uint blockIdx = NvFlowLocationToBlockIdx(table, tableParams, location);
    uint tableValue = (blockIdx != ~0u) ? table[blockIdx + tableParams.blockLevelOffsetGlobal] : 0u;
    int3 threadIdx = vidx.xyz & int3(tableParams.blockDimLessOne);
    return NvFlowTableValueToIndexRead(threadIdx, tableValue);
}

int3 NvFlowGlobalVirtualToRealOffset(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 location, uint blockIdx)
{
    uint tableValue = (blockIdx != ~0u) ? table[blockIdx + tableParams.blockLevelOffsetGlobal] : 0u;
    int4 ridx_base = NvFlowTableValueToIndexRead(int3(0, 0, 0), tableValue);
    return ridx_base.xyz - int3(location.xyz << tableParams.blockDimBits);
}

struct NvFlowGlobalAccessor
{
    int4 location;
    int3 virtualToReal;
    int valid;
};

void NvFlowGlobalAccessorInit(inout NvFlowGlobalAccessor accessor)
{
    accessor.location = int4(0, 0, 0, 0);
    accessor.virtualToReal = int3(0, 0, 0);
    accessor.valid = 0;
}

int4 NvFlowGlobalAccessorVirtualToReal(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx, inout NvFlowGlobalAccessor accessor)
{
    int4 location = int4(vidx.xyz >> int3(tableParams.blockDimBits), vidx.w);
    if (location.x != accessor.location.x ||
        location.y != accessor.location.y ||
        location.z != accessor.location.z ||
        location.w != accessor.location.w)
    {
        accessor.location = location;
        uint blockIdx = NvFlowLocationToBlockIdx(table, tableParams, location);
        uint tableValue = (blockIdx != ~0u) ? table[blockIdx + tableParams.blockLevelOffsetGlobal] : 0u;
        int4 ridx_base = NvFlowTableValueToIndexRead(int3(0, 0, 0), tableValue);
        accessor.virtualToReal = ridx_base.xyz - int3(location.xyz << tableParams.blockDimBits);
        accessor.valid = ridx_base.w;
    }
    int4 ridx;
    ridx.xyz = vidx.xyz + accessor.virtualToReal;
    ridx.w = accessor.valid;
    return ridx;
}

float NvFlowGlobalRead1f(Texture3D<float> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx)
{
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float(0.f);
}

float NvFlowGlobalAccessorRead1f(Texture3D<float> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx, inout NvFlowGlobalAccessor accessor)
{
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float(0.f);
}

float NvFlowGlobalReadLinear1f(Texture3D<float> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float(0.f);
}

float NvFlowGlobalAccessorReadLinear1f(Texture3D<float> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel, inout NvFlowGlobalAccessor accessor)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float(0.f);
}

float2 NvFlowGlobalRead2f(Texture3D<float2> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx)
{
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float2(0.f, 0.f);
}

float2 NvFlowGlobalAccessorRead2f(Texture3D<float2> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx, inout NvFlowGlobalAccessor accessor)
{
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float2(0.f, 0.f);
}

float2 NvFlowGlobalReadLinear2f(Texture3D<float2> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float2(0.f, 0.f);
}

float2 NvFlowGlobalAccessorReadLinear2f(Texture3D<float2> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel, inout NvFlowGlobalAccessor accessor)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float2(0.f, 0.f);
}

float4 NvFlowGlobalRead4f(Texture3D<float4> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx)
{
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float4(0.f, 0.f, 0.f, 0.f);
}

float4 NvFlowGlobalAccessorRead4f(Texture3D<float4> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int4 vidx, inout NvFlowGlobalAccessor accessor)
{
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float4(0.f, 0.f, 0.f, 0.f);
}

float4 NvFlowGlobalReadLinear4f(Texture3D<float4> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, vidx);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float4(0.f, 0.f, 0.f, 0.f);
}

float4 NvFlowGlobalAccessorReadLinear4f(Texture3D<float4> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, float3 vidxf, int layerAndLevel, inout NvFlowGlobalAccessor accessor)
{
    int4 vidx = int4(NvFlowFloor_i(vidxf), layerAndLevel);
    int4 readIdx = NvFlowGlobalAccessorVirtualToReal(table, tableParams, vidx, accessor);
    float3 readIdxf = float3(readIdx.xyz) + vidxf - float3(vidx.xyz);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float4(0.f, 0.f, 0.f, 0.f);
}

int4 NvFlowLocalVirtualToReal(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    int3 neighbor = threadIdx >> int3(tableParams.blockDimBits);
    return NvFlowTableValueToIndexRead(threadIdx, table[NvFlowNeighborToIdx(blockIdx, neighbor.x, neighbor.y, neighbor.z) + tableParams.blockLevelOffsetLocal]);
}

int4 NvFlowSingleVirtualToReal(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    return NvFlowTableValueToIndexRead(threadIdx, table[NvFlowSingleToIdx(blockIdx) + tableParams.blockLevelOffsetLocal]);
}

float3 NvFlowLocalVirtualToRealf(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return readIdxf;
}

float3 NvFlowLocalVirtualToRealSafef(StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 location;
    NvFlowBlockIdxToLocation(table, tableParams, blockIdx, location);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, int4((location.xyz << int3(tableParams.blockDimBits)) + threadIdx, location.w));
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return readIdxf;
}

float4 NvFlowLocalRead4f(Texture3D<float4> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float4(0.f, 0.f, 0.f, 0.f);
}

float4 NvFlowLocalReadSafe4f(Texture3D<float4> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    int4 location;
    NvFlowBlockIdxToLocation(table, tableParams, blockIdx, location);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, int4((location.xyz << int3(tableParams.blockDimBits)) + threadIdx, location.w));
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float4(0.f, 0.f, 0.f, 0.f);
}

float2 NvFlowLocalRead2f(Texture3D<float2> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : float2(0.f, 0.f);
}

float NvFlowLocalRead1f(Texture3D<float> textureRead, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx)
{
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    return bool(readIdx.w) ? textureRead[readIdx.xyz] : 0.f;
}

float4 NvFlowLocalReadLinear4f(Texture3D<float4> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float4(0.f, 0.f, 0.f, 0.f);
}

float4 NvFlowLocalReadLinearSafe4f(Texture3D<float4> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 location;
    NvFlowBlockIdxToLocation(table, tableParams, blockIdx, location);
    int4 readIdx = NvFlowGlobalVirtualToReal(table, tableParams, int4((location.xyz << int3(tableParams.blockDimBits)) + threadIdx, location.w));
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float4(0.f, 0.f, 0.f, 0.f);
}

float2 NvFlowLocalReadLinear2f(Texture3D<float2> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : float2(0.f, 0.f);
}

float NvFlowLocalReadLinear1f(Texture3D<float> textureRead, SamplerState textureSampler, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, float3 threadIdxf)
{
    int3 threadIdx = NvFlowFloor_i(threadIdxf);
    int4 readIdx = NvFlowLocalVirtualToReal(table, tableParams, blockIdx, threadIdx);
    float3 readIdxf = float3(readIdx.xyz) + threadIdxf - float3(threadIdx);
    return bool(readIdx.w) ? textureRead.SampleLevel(textureSampler, readIdxf * tableParams.dimInv, 0.0) : 0.f;
}

void NvFlowLocalWriteNeighbor4f(RWTexture3D<float4> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int x, int y, int z, uint blockIdx, int3 threadIdx, float4 value)
{
    int4 index = NvFlowTableValueToIndexWrite(threadIdx, table[NvFlowNeighborToIdx(blockIdx, x, y, z) + tableParams.blockLevelOffsetLocal], x, y, z);
    texRW[index.xyz] = bool(index.w) ? value : float4(0.f, 0.f, 0.f, 0.f);
}

void NvFlowLocalWriteNeighbor2f(RWTexture3D<float2> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int x, int y, int z, uint blockIdx, int3 threadIdx, float2 value)
{
    int4 index = NvFlowTableValueToIndexWrite(threadIdx, table[NvFlowNeighborToIdx(blockIdx, x, y, z) + tableParams.blockLevelOffsetLocal], x, y, z);
    texRW[index.xyz] = bool(index.w) ? value : float2(0.f, 0.f);
}

void NvFlowLocalWriteNeighbor1f(RWTexture3D<float> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int x, int y, int z, uint blockIdx, int3 threadIdx, float value)
{
    int4 index = NvFlowTableValueToIndexWrite(threadIdx, table[NvFlowNeighborToIdx(blockIdx, x, y, z) + tableParams.blockLevelOffsetLocal], x, y, z);
    texRW[index.xyz] = bool(index.w) ? value : 0.f;
}

void NvFlowLocalWriteNeighbor1ui(RWTexture3D<uint> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, int x, int y, int z, uint blockIdx, int3 threadIdx, uint value)
{
    int4 index = NvFlowTableValueToIndexWrite(threadIdx, table[NvFlowNeighborToIdx(blockIdx, x, y, z) + tableParams.blockLevelOffsetLocal], x, y, z);
    texRW[index.xyz] = bool(index.w) ? value : 0u;
}

int3 NvFlowLocalComputeNeighbor(NvFlowSparseLevelParams tableParams, int3 threadIdx)
{
    int3 neighbor = int3(0, 0, 0);
    if (threadIdx.x == 0)
    {
        neighbor.x = -1;
    }
    if (threadIdx.x == int(tableParams.blockDimLessOne.x))
    {
        neighbor.x = +1;
    }
    if (threadIdx.y == 0)
    {
        neighbor.y = -1;
    }
    if (threadIdx.y == int(tableParams.blockDimLessOne.y))
    {
        neighbor.y = +1;
    }
    if (threadIdx.z == 0)
    {
        neighbor.z = -1;
    }
    if (threadIdx.z == int(tableParams.blockDimLessOne.z))
    {
        neighbor.z = +1;
    }
    return neighbor;
}

void NvFlowLocalWrite4f(RWTexture3D<float4> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float4 value)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, 0, 0, 0, blockIdx, threadIdx, value);
    if (neighbor.x != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, neighbor.x, 0, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, 0, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, 0, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, neighbor.x, neighbor.y, 0, blockIdx, threadIdx,  value);
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, 0, neighbor.y, neighbor.z, blockIdx, threadIdx,  value);
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, neighbor.x, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor4f(texRW, table, tableParams, neighbor.x, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
}

void NvFlowLocalWrite2f(RWTexture3D<float2> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float2 value)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, 0, 0, 0, blockIdx, threadIdx, value);
    if (neighbor.x != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, neighbor.x, 0, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, 0, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, 0, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, neighbor.x, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, 0, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, neighbor.x, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor2f(texRW, table, tableParams, neighbor.x, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
}

void NvFlowLocalWrite1f(RWTexture3D<float> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float value)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, 0, 0, 0, blockIdx, threadIdx, value);
    if (neighbor.x != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, neighbor.x, 0, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, 0, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, 0, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, neighbor.x, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, 0, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, neighbor.x, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1f(texRW, table, tableParams, neighbor.x, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
}

void NvFlowLocalWrite1ui(RWTexture3D<uint> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, uint value)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, 0, 0, 0, blockIdx, threadIdx, value);
    if (neighbor.x != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, neighbor.x, 0, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, 0, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, 0, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, neighbor.x, neighbor.y, 0, blockIdx, threadIdx, value);
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, 0, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, neighbor.x, 0, neighbor.z, blockIdx, threadIdx, value);
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        NvFlowLocalWriteNeighbor1ui(texRW, table, tableParams, neighbor.x, neighbor.y, neighbor.z, blockIdx, threadIdx, value);
    }
}

void NvFlowBlockClear4f(RWTexture3D<float4> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float4 clearValue)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    // assume block is valid, ignore .w
    int3 readIdx = NvFlowSingleVirtualToReal(table, tableParams, blockIdx, threadIdx).xyz;

    texRW[readIdx] = clearValue;
    if (neighbor.x != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, 0)] = clearValue;
    }
    if (neighbor.y != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.z != 0)
    {
        texRW[readIdx + int3(0, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, neighbor.z)] = clearValue;
    }
}

void NvFlowBlockClear2f(RWTexture3D<float2> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float2 clearValue)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    // assume block is valid, ignore .w
    int3 readIdx = NvFlowSingleVirtualToReal(table, tableParams, blockIdx, threadIdx).xyz;

    texRW[readIdx] = clearValue;
    if (neighbor.x != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, 0)] = clearValue;
    }
    if (neighbor.y != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.z != 0)
    {
        texRW[readIdx + int3(0, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, neighbor.z)] = clearValue;
    }
}

void NvFlowBlockClear1f(RWTexture3D<float> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, float clearValue)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    // assume block is valid, ignore .w
    int3 readIdx = NvFlowSingleVirtualToReal(table, tableParams, blockIdx, threadIdx).xyz;

    texRW[readIdx] = clearValue;
    if (neighbor.x != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, 0)] = clearValue;
    }
    if (neighbor.y != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.z != 0)
    {
        texRW[readIdx + int3(0, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, neighbor.z)] = clearValue;
    }
}

void NvFlowBlockClear1ui(RWTexture3D<uint> texRW, StructuredBuffer<uint> table, NvFlowSparseLevelParams tableParams, uint blockIdx, int3 threadIdx, uint clearValue)
{
    int3 neighbor = NvFlowLocalComputeNeighbor(tableParams, threadIdx);

    // assume block is valid, ignore .w
    int3 readIdx = NvFlowSingleVirtualToReal(table, tableParams, blockIdx, threadIdx).xyz;

    texRW[readIdx] = clearValue;
    if (neighbor.x != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, 0)] = clearValue;
    }
    if (neighbor.y != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.z != 0)
    {
        texRW[readIdx + int3(0, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, 0)] = clearValue;
    }
    if (neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(0, neighbor.y, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, 0, neighbor.z)] = clearValue;
    }
    if (neighbor.x != 0 && neighbor.y != 0 && neighbor.z != 0)
    {
        texRW[readIdx + int3(neighbor.x, neighbor.y, neighbor.z)] = clearValue;
    }
}

#endif
