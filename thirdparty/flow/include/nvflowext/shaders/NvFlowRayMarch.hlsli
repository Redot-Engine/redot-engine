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


#ifndef NV_FLOW_RAY_MARCH_HLSLI
#define NV_FLOW_RAY_MARCH_HLSLI

#define NV_FLOW_RAY_MARCH_CLOUD

#include "NvFlowRayMarchCommon.hlsli"

void NvFlowRayMarchWireframe(
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float hitT,
    int4 location,
    float brightness,
    inout float4 sum,
    inout float nominalT
    )
{
    if (hitT > rayMinT)
    {
        float3 boxWorldPos = rayOrigin + hitT * rayDir;
        float3 blockFrac = params.blockSizeWorld * (boxWorldPos * params.blockSizeWorldInv - float3(location.xyz));
        float faceX = max(2.f * params.cellSize.x - blockFrac.x, blockFrac.x - float(paramsTable.blockDimLessOne.x - 1) * params.cellSize.x);
        float faceY = max(2.f * params.cellSize.y - blockFrac.y, blockFrac.y - float(paramsTable.blockDimLessOne.y - 1) * params.cellSize.y);
        float faceZ = max(2.f * params.cellSize.z - blockFrac.z, blockFrac.z - float(paramsTable.blockDimLessOne.z - 1) * params.cellSize.z);
        if ((faceX > 0.f && faceY > 0.f) ||
            (faceY > 0.f && faceZ > 0.f) ||
            (faceZ > 0.f && faceX > 0.f))
        {
            faceX -= params.cellSize.x;
            faceY -= params.cellSize.y;
            faceZ -= params.cellSize.z;
            float4 color = float4(0.f, 0.f, 0.f, 0.f);
            if (faceX > faceY && faceX > faceZ)
            {
                color.g = brightness;
                color.a = abs(max(faceY, faceZ)) * params.cellSizeInv.x;
            }
            if (faceY > faceX && faceY > faceZ)
            {
                color.g = brightness;
                color.a = abs(max(faceX, faceZ)) * params.cellSizeInv.y;
            }
            if (faceZ > faceX && faceZ > faceY)
            {
                color.g = brightness;
                color.a = abs(max(faceX, faceY)) * params.cellSizeInv.z;
            }
            color.a *= color.a;
            color.a = 1.f - color.a;

            nominalT = sum.a * (color.a * hitT) + nominalT;

            sum.rgb = sum.a * (color.a * color.rgb) + sum.rgb;
            sum.a = (1.f - color.a) * sum.a;
        }
    }
}

bool NvFlowRayMarchBlock(
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    Texture2D<float4> colormapIn,
    SamplerState colormapSampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    float rayNoise,
    uint blockIdx,
    int4 location,
    inout float4 sum,
    inout float nominalT
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

    if (isHit)
    {
        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframe(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMinT,
                location,
                1.f,
                sum,
                nominalT
            );
        }

        float cellMinT = params.stepSizeInv * boxMinT;
        float cellMaxT = params.stepSizeInv * boxMaxT;

        cellMinT = -floor(-(cellMinT + rayNoise)) - rayNoise;
        cellMaxT = -floor(-(cellMaxT + rayNoise)) - rayNoise;

        int numSteps = int(cellMaxT - cellMinT);

        int3 virtualToRealOffset = NvFlowGlobalVirtualToRealOffset(tableIn, paramsTable, location, blockIdx);

        float currentT = params.stepSize * (cellMinT);

        float3 worldPos = rayOrigin + currentT * rayDir;
        float3 worldPosStep = params.stepSize * rayDir;

        float3 vidxf = worldPos * params.cellSizeInv;
        float3 vidxfStep = worldPosStep * params.cellSizeInv;

        float3 ridxf = vidxf + float3(virtualToRealOffset);

        for (int stepIdx = 0; stepIdx < numSteps; stepIdx++)
        {
            float4 value = densityIn.SampleLevel(densitySampler, ridxf * paramsTable.dimInv, 0.0);

            float4 color = value;
            if (params.enableRawMode == 0u)
            {
                // colormap by temperature
                float colormapU = params.colormapXScale * value.r + params.colormapXOffset;
                color = colormapIn.SampleLevel(colormapSampler, float2(colormapU, params.layerAndLevelColormapV), 0.0);

                // scale based on light value
                color.rgb *= (1.f + params.shadowFactor * (value.z - 1.f));

                // modulate alpha by smoke
                color.a *= value.a;
            }
            else
            {
                // isosurface
                if (params.rawModeIsosurface != 0u)
                {
                    color.a = value.a > 0.5f ? 1.f : 0.f;
                }
                // normalize color to avoid edge darkening
                if (params.rawModeNormalize != 0u)
                {
                    if (color.a > 0.f)
                    {
                        color.rgb *= (1.f / value.a);
                    }
                }
            }

            color.rgb *= params.colorScale;

            color = max(float4(0.f, 0.f, 0.f, 0.f), color);
            color.a = min(1.f, color.a);

            color.a *= params.alphaScale;

            nominalT = sum.a * (color.a * currentT) + nominalT;

            sum.rgb = sum.a * (color.a * color.rgb) + sum.rgb;
            sum.a = (1.f - color.a) * sum.a;

            ridxf += vidxfStep;
            currentT += params.stepSize;
        }

        if (sum.a < 0.00005f)
        {
            hitMax = true;
        }

        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframe(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMaxT,
                location,
                0.3f,
                sum,
                nominalT
            );
        }
    }
    return hitMax;
}

#ifdef NV_FLOW_RAY_MARCH_CLOUD
bool NvFlowRayMarchBlockCloud(
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    Texture2D<float4> colormapIn,
    SamplerState colormapSampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    uint blockIdx,
    int4 location,
    inout float4 sum,
    inout float nominalT
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

    if (isHit)
    {
        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframe(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMinT,
                location,
                1.f,
                sum,
                nominalT
            );
        }

        float cellMinT = params.stepSizeInv * boxMinT;
        float cellMaxT = params.stepSizeInv * boxMaxT;

        cellMinT = round(cellMinT);
        cellMaxT = round(cellMaxT);

        int numSteps = int(cellMaxT - cellMinT);

        int3 virtualToRealOffset = NvFlowGlobalVirtualToRealOffset(tableIn, paramsTable, location, blockIdx);

        float currentT = params.stepSize * (cellMinT);

        float3 worldPos = rayOrigin + currentT * rayDir;
        float3 worldPosStep = params.stepSize * rayDir;

        float3 vidxf = worldPos * params.cellSizeInv;
        float3 vidxfStep = worldPosStep * params.cellSizeInv;

        float3 ridxf = vidxf + float3(virtualToRealOffset);

        // shadow ray marching
        int numShadowSteps = int(params.cloud.numShadowSteps);
        float3 ambientSampleOffset = float3(0.0f, 0.2f, 0.0f);
        float3 shadowStep = params.stepSize * normalize(params.cloud.sunDirection) * params.cellSizeInv * params.cloud.shadowStepMultiplier;

        for (int stepIdx = 0; stepIdx < numSteps; stepIdx++)
        {
            // stop marching when transmittance is below threshold (almost opaque)
            if (sum.a < 0.001f)
                break;

            float4 value = densityIn.SampleLevel(densitySampler, ridxf * paramsTable.dimInv, 0.0);

            // shadow ray marching
            float density = 0.0f;
            float densitySample = value.a;

            if (densitySample > 0.001f)  // optimization: skip shadow calculation if density is almost zero
            {
                float shadowDensity = 0.0f;
                float4 shadowSum = float4(0.f, 0.f, 0.f, 1.f);
                float3 shadowSamplePos = vidxf + shadowStep; // needs jitter to reduce banding artifacts

                // for each view ray sample, march along its shadow ray, sample + accumulate density
                for (int shadowStepIdx = 0; shadowStepIdx < numShadowSteps; shadowStepIdx++)
                {
                    float4 shadowSample = NvFlowGlobalReadLinear4f(densityIn, densitySampler, tableIn,
                        paramsTable, shadowSamplePos, params.layerAndLevel);
                    shadowDensity += shadowSample.a;
                    shadowSamplePos += shadowStep;
                }

                density = clamp(densitySample * params.cloud.densityMultiplier, 0.0f, 1.0f);
                float3 shadowAttenuation = exp((-shadowDensity / (float(numShadowSteps))) * params.cloud.attenuationMultiplier);
                sum.rgb += shadowAttenuation * density * sum.a * params.cloud.volumeBaseColor * params.cloud.volumeColorMultiplier;
                sum.a *= 1.0f - density;

                // sample nearby density to estimate ambient occlusion
                float3 aoSamplePos = vidxf + ambientSampleOffset;
                float4 aoSample = NvFlowGlobalReadLinear4f(densityIn, densitySampler, tableIn,
                    paramsTable, aoSamplePos, params.layerAndLevel);
                float aoDensity = exp(-aoSample.a);
                sum.rgb += aoDensity * density * sum.a * params.cloud.ambientColor * params.cloud.ambientMultiplier;
            }

            ridxf += vidxfStep;
            vidxf += vidxfStep;
            currentT += params.stepSize;
        }
        if (hitMax)
        {
            // stop marching when transmittance is below threshold (almost opaque)
            if (sum.a > 0.001f)
            {
                float4 value = densityIn.SampleLevel(densitySampler, ridxf * paramsTable.dimInv, 0.0);

                // shadow ray marching
                float density = 0.0f;
                float densitySample = value.a;

                if (densitySample > 0.001f)  // optimization: skip shadow calculation if density is almost zero
                {
                    float shadowDensity = 0.0f;
                    float4 shadowSum = float4(0.f, 0.f, 0.f, 1.f);
                    float3 shadowSamplePos = vidxf + shadowStep; // needs jitter to reduce banding artifacts

                    // for each view ray sample, march along its shadow ray, sample + accumulate density
                    for (int shadowStepIdx = 0; shadowStepIdx < numShadowSteps; shadowStepIdx++)
                    {
                        float4 shadowSample = NvFlowGlobalReadLinear4f(densityIn, densitySampler, tableIn,
                            paramsTable, shadowSamplePos, params.layerAndLevel);
                        shadowDensity += shadowSample.a;
                        shadowSamplePos += shadowStep;
                    }

                    density = clamp(densitySample * params.cloud.densityMultiplier, 0.0f, 1.0f);
                    float3 shadowAttenuation = exp((-shadowDensity / (float(numShadowSteps))) * params.cloud.attenuationMultiplier);
                    sum.rgb += shadowAttenuation * density * sum.a * params.cloud.volumeBaseColor * params.cloud.volumeColorMultiplier;
                    sum.a *= 1.0f - density;

                    // sample nearby density to estimate ambient occlusion
                    float3 aoSamplePos = vidxf + ambientSampleOffset;
                    float4 aoSample = NvFlowGlobalReadLinear4f(densityIn, densitySampler, tableIn,
                        paramsTable, aoSamplePos, params.layerAndLevel);
                    float aoDensity = exp(-aoSample.a);
                    sum.rgb += aoDensity * density * sum.a * params.cloud.ambientColor * params.cloud.ambientMultiplier;
                }

                ridxf += vidxfStep;
                vidxf += vidxfStep;
                currentT += params.stepSize;
            }
        }

        if (params.enableBlockWireframe != 0u)
        {
            NvFlowRayMarchWireframe(
                paramsTable,
                params,
                rayOrigin,
                rayMinT,
                rayDir,
                boxMaxT,
                location,
                0.3f,
                sum,
                nominalT
            );
        }
    }
    return hitMax;
}
#endif

void NvFlowRayMarchBlocks(
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    Texture2D<float4> colormapIn,
    SamplerState colormapSampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    inout float4 sum,
    inout float nominalT
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

        float rayNoise = NvFlowRayMarchNoiseFromDir(rayDir);

        bool hitMax = false;
        float blockHitT = boxMinT;

        while (
            location.x != finalLocation.x &&
            location.y != finalLocation.y &&
            location.z != finalLocation.z &&
            !hitMax)
        {
            uint blockIdx = NvFlowLocationToBlockIdx(tableIn, paramsTable, location);
            if (blockIdx != ~0u)
            {
#ifdef NV_FLOW_RAY_MARCH_CLOUD
                if (params.cloud.enableCloudMode != 0u)
                {
                    hitMax = NvFlowRayMarchBlockCloud(
                        densityIn,
                        densitySampler,
                        colormapIn,
                        colormapSampler,
                        tableIn,
                        paramsTable,
                        params,
                        rayOrigin,
                        rayMinT,
                        rayDir,
                        rayMaxT,
                        rayDirInv,
                        blockIdx,
                        location,
                        sum,
                        nominalT
                    );
                }
                else
#endif
                {
                    hitMax = NvFlowRayMarchBlock(
                        densityIn,
                        densitySampler,
                        colormapIn,
                        colormapSampler,
                        tableIn,
                        paramsTable,
                        params,
                        rayOrigin,
                        rayMinT,
                        rayDir,
                        rayMaxT,
                        rayDirInv,
                        rayNoise,
                        blockIdx,
                        location,
                        sum,
                        nominalT
                    );
                }
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

void NvFlowRayMarchBlocksMotion(
    Texture3D<float4> velocityIn,
    SamplerState velocitySampler,
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    Texture2D<float4> colormapIn,
    SamplerState colormapSampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTableVelocity,
    NvFlowSparseLevelParams paramsTableDensity,
    NvFlowRayMarchLayerShaderParams params,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    inout float4 sum,
    inout float nominalT,
    inout float3 nominalMotion
)
{
    float4 localSum = float4(0.f, 0.f, 0.f, 1.f);
    float localNominalT = 0.f;
    NvFlowRayMarchBlocks(
        densityIn,
        densitySampler,
        colormapIn,
        colormapSampler,
        tableIn,
        paramsTableDensity,
        params,
        rayOrigin,
        rayMinT,
        rayDir,
        rayMaxT,
        rayDirInv,
        localSum,
        localNominalT
    );
    if (localSum.a < 1.f)
    {
        float motionT = localNominalT / (1.f - localSum.a);
        float3 vidxf = (rayDir * motionT + rayOrigin) * params.velocityCellSizeInv;
        float4 nominalVelocityLocal = NvFlowGlobalReadLinear4f(velocityIn, velocitySampler, tableIn, paramsTableVelocity, vidxf, params.layerAndLevel);
        float3 nominalMotionLocal = params.deltaTime * nominalVelocityLocal.xyz;
        nominalMotion += sum.a * (1.f - localSum.a) * nominalMotionLocal;
    }
    nominalT += sum.a * localNominalT;
    sum.rgb += sum.a * localSum.rgb;
    sum.a *= localSum.a;
}

void NvFlowRayMarchBlocksLayered(
    Texture3D<float4> velocityIn,
    SamplerState velocitySampler,
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    Texture2D<float4> colormapIn,
    SamplerState colormapSampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTableVelocity,
    NvFlowSparseLevelParams paramsTableDensity,
    StructuredBuffer<NvFlowRayMarchLayerShaderParams> layerParamsIn,
    uint numLayers,
    float3 rayOrigin,
    float rayMinT,
    float3 rayDir,
    float rayMaxT,
    float3 rayDirInv,
    inout float4 sum,
    inout float nominalT,
    inout float3 nominalMotion
)
{
    for (uint layerParamIdx = 0u; layerParamIdx < numLayers; layerParamIdx++)
    {
        float4 sum_local = float4(0.f, 0.f, 0.f, 1.f);
        float nominalT_local = 0.f;
        float3 nominalMotion_local = float3(0.f, 0.f, 0.f);
        NvFlowRayMarchBlocksMotion(
            velocityIn,
            velocitySampler,
            densityIn,
            densitySampler,
            colormapIn,
            colormapSampler,
            tableIn,
            paramsTableVelocity,
            paramsTableDensity,
            layerParamsIn[layerParamIdx],
            rayOrigin,
            rayMinT,
            rayDir,
            rayMaxT,
            rayDirInv,
            sum_local,
            nominalT_local,
            nominalMotion_local
        );
        if (sum.w == 1.f)
        {
            sum = sum_local;
            nominalT= nominalT_local;
            nominalMotion = nominalMotion_local;
        }
        else if (sum_local.w < 1.f)
        {
            float t_local = nominalT_local / (1.f - sum_local.w);
            float t = nominalT / (1.f - sum.a);

            // to soften sorting transition
            float thicknessInv = (1.f / 8.f) * layerParamsIn[layerParamIdx].cellSizeInv.x;

            float w = max(0.f, min(1.f, (t - t_local) * thicknessInv + 0.5f));
            float w_accum = (1.f - w) + w * sum_local.a;
            float w_local = w + (1.f - w) * sum.a;

            nominalT = w_accum * nominalT + w_local * nominalT_local;
            sum.rgb = w_accum * sum.rgb  + w_local * sum_local.rgb;
            nominalMotion = w_accum * nominalMotion + w_local * nominalMotion_local;
            sum.a *= sum_local.a;
        }
    }
}

float4 NvFlowRayMarchSampleDensity(
    Texture3D<float4> densityIn,
    SamplerState densitySampler,
    StructuredBuffer<uint> tableIn,
    NvFlowSparseLevelParams paramsTable,
    NvFlowRayMarchLayerShaderParams params,
    float3 worldLocation,
    inout NvFlowGlobalAccessor accessor
)
{
    float3 boxWorldMin = params.worldMin;
    float3 boxWorldMax = params.worldMax;
    float4 density4 = float4(0.f, 0.f, 0.f, 0.f);
    if (worldLocation.x >= boxWorldMin.x && worldLocation.x <= boxWorldMax.x &&
        worldLocation.y >= boxWorldMin.y && worldLocation.y <= boxWorldMax.y &&
        worldLocation.z >= boxWorldMin.z && worldLocation.z <= boxWorldMax.z )
    {
        float3 vidxf = worldLocation * params.cellSizeInv;
        density4 = NvFlowGlobalAccessorReadLinear4f(densityIn, densitySampler, tableIn, paramsTable, vidxf, params.layerAndLevel, accessor);
    }
    else
    {
        accessor.valid = 0;
    }
    return density4;
}

#endif
