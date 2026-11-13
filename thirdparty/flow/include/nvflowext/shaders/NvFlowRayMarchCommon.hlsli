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


#ifndef NV_FLOW_RAY_MARCH_COMMON_HLSLI
#define NV_FLOW_RAY_MARCH_COMMON_HLSLI

#include "NvFlowRayMarchParams.h"

#include "NvFlowShader.hlsli"

// ray origin is implied zero
bool NvFlowIntersectBox(float3 rayDir, float3 rayDirInv, float3 boxMin, float3 boxMax, out float tnear, out float tfar)
{
    // compute intersection of ray with all six bbox planes
    float3 tbot = boxMin * rayDirInv;
    float3 ttop = boxMax * rayDirInv;

    // re-order intersections to find smallest and largest on each axis
    float3 tmin = min(ttop, tbot);
    float3 tmax = max(ttop, tbot);

    // find the largest tmin and the smallest tmax
    tnear = max(max(tmin.x, tmin.y), max(tmin.x, tmin.z));
    tfar = min(min(tmax.x, tmax.y), min(tmax.x, tmax.z));

    return tfar > tnear;
}

int3 NvFlowRayMarchComputeFinalLocation(float3 rayDir, int4 location, int4 locationMin, int4 locationMax)
{
    return int3(
        rayDir.x > 0.f ? max(location.x, locationMax.x) : min(location.x, locationMin.x - 1),
        rayDir.y > 0.f ? max(location.y, locationMax.y) : min(location.y, locationMin.y - 1),
        rayDir.z > 0.f ? max(location.z, locationMax.z) : min(location.z, locationMin.z - 1)
        );
}

void NvFlowRayMarchAdvanceRay(
    float3 blockSizeWorld,
    float3 rayDir,
    float3 rayDirInv,
    float3 rayOrigin,
    inout int4 location,
    inout float hitT
)
{
    float hitTx = (float(location.x + (rayDir.x > 0.f ? +1 : 0)) * blockSizeWorld.x - rayOrigin.x) * rayDirInv.x;
    float hitTy = (float(location.y + (rayDir.y > 0.f ? +1 : 0)) * blockSizeWorld.y - rayOrigin.y) * rayDirInv.y;
    float hitTz = (float(location.z + (rayDir.z > 0.f ? +1 : 0)) * blockSizeWorld.z - rayOrigin.z) * rayDirInv.z;

    if (rayDir.x != 0.f && (hitTx <= hitTy || rayDir.y == 0.f) && (hitTx <= hitTz || rayDir.z == 0.f))
    {
        hitT = hitTx;
        location.x += rayDir.x > 0.f ? +1 : -1;
    }
    else if (rayDir.y != 0.f && (hitTy <= hitTx || rayDir.x == 0.f) && (hitTy <= hitTz || rayDir.z == 0.f))
    {
        hitT = hitTy;
        location.y += rayDir.y > 0.f ? +1 : -1;
    }
    else
    {
        hitT = hitTz;
        location.z += rayDir.z > 0.f ? +1 : -1;
    }
}

// source: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint NvFlowRayMarchHash(uint inputValue)
{
    uint state = inputValue * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float NvFlowRayMarchRandNorm(uint inputValue)
{
    return float(NvFlowRayMarchHash(inputValue) & 0xFFFF) * float(1.f / 65535.f);
}

float NvFlowRayMarchNoiseFromDir(float3 rayDir)
{
    float2 uv;
    if (abs(rayDir.x) > abs(rayDir.y) && abs(rayDir.x) > abs(rayDir.z))
    {
        uv = rayDir.yz;
    }
    else if (abs(rayDir.y) > abs(rayDir.x) && abs(rayDir.y) > abs(rayDir.z))
    {
        uv = rayDir.xz;
    }
    else //if (abs(rayDir.z) > abs(rayDir.x) && abs(rayDir.z) > abs(rayDir.y))
    {
        uv = rayDir.xy;
    }
    float maxAxis = max(abs(rayDir.x), max(abs(rayDir.y), abs(rayDir.z)));
    if (maxAxis > 0.f)
    {
        uv *= (1.f / maxAxis);
    }
    uv = 0.5f * uv + 0.5f;
    uint hashInput = uint(65535.f * uv.x) ^ (uint(65535.f * uv.y) << 16u);
    return NvFlowRayMarchRandNorm(hashInput);
}

#endif
