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


#ifndef NV_FLOW_RAY_MARCH_ISOSURFACE_HLSLI
#define NV_FLOW_RAY_MARCH_ISOSURFACE_HLSLI

#include "NvFlowRayMarchCommon.hlsli"

//masks
#define NV_FLOW_ISOSURFACE_V0_MASK 0x2
#define NV_FLOW_ISOSURFACE_V1_MASK 0x1
#define NV_FLOW_ISOSURFACE_V0V1_MASK 0x3

//flags
#define NV_FLOW_ISOSURFACE_V0_GTE_THRESH 0x2
#define NV_FLOW_ISOSURFACE_V1_GTE_THRESH 0x1
#define NV_FLOW_ISOSURFACE_RISING_EDGE 0x1  // V0 < thresh <= V1
#define NV_FLOW_ISOSURFACE_FALLING_EDGE 0x2 // V0 >= thresh < V1

void NvFlowRayMarchWireframeIsosurface(
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchIsosurfaceLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float hitT,
    int4 location,
    inout float debugHitT
)
{
    if (debugHitT == rayMinT && hitT > rayMinT)
    {
        float3 boxWorldPos = rayOrigin + hitT * rayDir;
        float3 blockFrac = params.blockSizeWorld * (boxWorldPos * params.blockSizeWorldInv - float3(location.xyz));
        float faceX = max(params.cellSize.x - blockFrac.x, blockFrac.x - float(paramsTable.blockDimLessOne.x) * params.cellSize.x);
        float faceY = max(params.cellSize.y - blockFrac.y, blockFrac.y - float(paramsTable.blockDimLessOne.y) * params.cellSize.y);
        float faceZ = max(params.cellSize.z - blockFrac.z, blockFrac.z - float(paramsTable.blockDimLessOne.z) * params.cellSize.z);
        if ((faceX > 0.f && faceY > 0.f) ||
            (faceY > 0.f && faceZ > 0.f) ||
            (faceZ > 0.f && faceX > 0.f))
        {
            debugHitT = hitT;
        }
    }
}

bool NvFlowRayMarchBlockIsosurface(
    Texture3D<float> densityIn,
    SamplerState densitySampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchIsosurfaceLayerShaderParams params,
    uint threshMask,
    uint threshFlags,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    uint blockIdx,
    int4 location,
    inout uint hitEdge,
    inout float hitT,
    inout float lastVal,
    inout float debugHitT
)
{
    float3 boxWorldMin = float3(location.xyz) * params.blockSizeWorld;
    float3 boxWorldMax = float3(location.xyz + int3(1, 1, 1)) * params.blockSizeWorld;

    const float ep = 0.0001f;

    boxWorldMin = (boxWorldMin - rayOrigin) - ep * params.cellSize;
    boxWorldMax = (boxWorldMax - rayOrigin) + ep * params.cellSize;

    float boxMinT;
    float boxMaxT;
    bool isHit = NvFlowIntersectBox(rayDir, rayDirInv, boxWorldMin, boxWorldMax, boxMinT, boxMaxT);

    boxMinT = max(rayMinT, boxMinT);
    if (boxMinT > boxMaxT)
    {
        isHit = false;
    }

    bool hitMax = false;
    if (boxMaxT > rayMaxT)
    {
        boxMaxT = rayMaxT;
        hitMax = true;
    }
    float3 hitRidxf = float3(0.f, 0.f, 0.f);

    if (isHit)
    {
        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframeIsosurface(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMinT,
                location,
                debugHitT
            );
        }

        float tmin = params.stepSizeInv * boxMinT;
        float tmax = params.stepSizeInv * boxMaxT;

        tmin = round(tmin);
        tmax = round(tmax);

        int numSteps = int(tmax - tmin);

        int3 virtualToRealOffset = NvFlowGlobalVirtualToRealOffset(tableIn, paramsTable, location, blockIdx);

        float currentT = params.stepSize * (tmin);

        float3 worldPos = rayOrigin + currentT * rayDir;
        float3 worldPosStep = params.stepSize * rayDir;

        float3 vidxf = worldPos * params.cellSizeInv;
        float3 vidxfStep = worldPosStep * params.cellSizeInv;

        float3 ridxf = vidxf + float3(virtualToRealOffset);

        int stepIdx = 0;
        for (; stepIdx < numSteps; stepIdx++)
        {
            if (currentT > rayMinT)
            {
                break;
            }
            ridxf += vidxfStep;
            currentT += params.stepSize;
        }
        for (; stepIdx < numSteps; stepIdx++)
        {
            float value = densityIn.SampleLevel(densitySampler, ridxf * paramsTable.dimInv, 0.0);
            float d = params.densityThreshold;
            uint threshTest = (uint(lastVal >= d)<<1 | uint(value >= d));
            if ((threshTest & threshMask) == threshFlags)
            {
                hitEdge = threshTest;
                hitRidxf = ridxf;
                hitT = currentT;
                break;
            }
            lastVal = value;
            ridxf += vidxfStep;
            currentT += params.stepSize;
        }

        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframeIsosurface(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMaxT,
                location,
                debugHitT
            );
        }
    }

    if (bool(hitEdge))
    {
        // march backwards
        float3 worldPosStep = params.stepSize * rayDir;
        float3 vidxfStep = (1.f / 256.f) * worldPosStep * params.cellSizeInv;

        // bisect to refine
        for (int stepIdx = 0; stepIdx < 8; stepIdx++)
        {
            float stepScale = float(1 << (7u - stepIdx));
            float3 vidxfStepLocal = stepScale * vidxfStep;
            float3 ridxfBisect = hitRidxf - vidxfStepLocal;
            float tStepLocal = (1.f / 256.f) * stepScale * params.stepSize;

            float value1 = densityIn.SampleLevel(densitySampler, ridxfBisect * paramsTable.dimInv, 0.0);

            float d = params.densityThreshold;
            uint threshTest = (uint(lastVal >= d)<<1 | uint(value1 >= d));
            if ((threshTest & threshMask) == threshFlags)
            {
                hitRidxf = ridxfBisect;
                hitT = hitT - tStepLocal;
            }
        }
    }
    return hitMax;
}

float3 NvFlowRayMarchBlocksIsosurfaceComputeNormal(
    Texture3D<float> densityIn,
    SamplerState densitySampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchIsosurfaceLayerShaderParams params,
    float3 hitVidxf
)
{
    // compute normal
    int3 vidx = NvFlowFloor_i(hitVidxf);
    int4 location = int4(vidx >> int3(paramsTable.blockDimBits), params.layerAndLevel);
    uint blockIdx = NvFlowLocationToBlockIdx(tableIn, paramsTable, location);
    float3 hitNormal = float3(0.f, 0.f, 0.f);
    if (blockIdx != ~0u)
    {
        float3 threadIdxf = float3(vidx & int3(paramsTable.blockDimLessOne)) + (hitVidxf - float3(vidx));

        float vxn = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(-1.f, 0.f, 0.f));
        float vxp = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(+1.f, 0.f, 0.f));
        float vyn = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(0.f, -1.f, 0.f));
        float vyp = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(0.f, +1.f, 0.f));
        float vzn = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(0.f, 0.f, -1.f));
        float vzp = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + float3(0.f, 0.f, +1.f));

        hitNormal = float3(vxp - vxn, vyp - vyn, vzp - vzn);
        if (hitNormal.x != 0.f || hitNormal.y != 0.f || hitNormal.z != 0.f)
        {
            hitNormal = normalize(hitNormal);
        }
    }
    return hitNormal;
}

float2 NvFlowRayMarchBlocksIsosurfaceComputeSlope(
    Texture3D<float> densityIn,
    SamplerState densitySampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchIsosurfaceLayerShaderParams params,
    float3 hitVidxf,
    float3 rayDir
)
{
    // compute slope
    int3 vidx = NvFlowFloor_i(hitVidxf);
    int4 location = int4(vidx >> int3(paramsTable.blockDimBits), params.layerAndLevel);
    uint blockIdx = NvFlowLocationToBlockIdx(tableIn, paramsTable, location);
    float2 slope = float2(0.f, 0.f);
    if (blockIdx != ~0u)
    {
        float3 threadIdxf = float3(vidx & int3(paramsTable.blockDimLessOne)) + (hitVidxf - float3(vidx));

        float vn = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf - rayDir);
        float vp = NvFlowLocalReadLinear1f(densityIn, densitySampler, tableIn, paramsTable, blockIdx, threadIdxf + rayDir);

        slope = 0.5f * float2(vp + vn, vp - vn);
    }
    return slope;
}

void NvFlowRayMarchBlocksIsosurface(
    Texture3D<float> densityIn,
    SamplerState densitySampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchIsosurfaceLayerShaderParams params,
    uint threshMask,
    uint threshFlags,
    float initVal,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    inout uint hitEdge,
    inout float hitT,
    inout float debugHitT
)
{
    float3 boxWorldMin = params.worldMin;
    float3 boxWorldMax = params.worldMax;

    float boxMinT;
    float boxMaxT;
    bool isHit = NvFlowIntersectBox(rayDir, rayDirInv, boxWorldMin - rayOrigin, boxWorldMax - rayOrigin, boxMinT, boxMaxT);

    boxMinT = max(rayMinT, boxMinT);
    if (boxMinT > boxMaxT)
    {
        isHit = false;
    }

    if (isHit)
    {
        float3 rayLocationWorld = rayDir * boxMinT + rayOrigin;
        int4 location = int4(NvFlowFloor_i(rayLocationWorld * params.blockSizeWorldInv), params.layerAndLevel);

        int3 finalLocation = NvFlowRayMarchComputeFinalLocation(rayDir, location, params.locationMin, params.locationMax);

        bool hitMax = false;
        float blockHitT = boxMinT;

        float lastVal = initVal;

        hitEdge = 0u;
        while (
            location.x != finalLocation.x &&
            location.y != finalLocation.y &&
            location.z != finalLocation.z &&
            !hitMax &&
            hitEdge == 0u)
        {
            uint blockIdx = NvFlowLocationToBlockIdx(tableIn, paramsTable, location);
            if (blockIdx != ~0u)
            {
                hitMax = NvFlowRayMarchBlockIsosurface(
                    densityIn,
                    densitySampler,
                    tableIn,
                    paramsTable,
                    params,
                    threshMask,
                    threshFlags,
                    rayOrigin,
                    rayMinT,
                    rayDir,
                    rayMaxT,
                    rayDirInv,
                    blockIdx,
                    location,
                    hitEdge,
                    hitT,
                    lastVal,
                    debugHitT
                );
            }
            NvFlowRayMarchAdvanceRay(
                params.blockSizeWorld,
                rayDir,
                rayDirInv,
                rayOrigin,
                location,
                blockHitT
            );
        }
    }
}

#endif
