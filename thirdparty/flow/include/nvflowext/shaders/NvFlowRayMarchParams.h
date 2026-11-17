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


#ifndef NV_FLOW_RAY_MARCH_PARAMS_H
#define NV_FLOW_RAY_MARCH_PARAMS_H

#include "NvFlowShaderTypes.h"

struct NvFlowRayMarchCloudLayerShaderParams
{
    float densityMultiplier;
    NvFlowUint enableCloudMode;
    float pad1;
    float pad2;
    NvFlowFloat3 ambientColor;
    float ambientMultiplier;
    NvFlowFloat3 volumeBaseColor;
    float volumeColorMultiplier;
    NvFlowFloat3 sunDirection;
    float shadowStepMultiplier;
    NvFlowFloat3 attenuationMultiplier;
    int numShadowSteps;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowRayMarchCloudLayerShaderParams NvFlowRayMarchCloudLayerShaderParams;
#endif

struct NvFlowRayMarchLayerShaderParams
{
    NvFlowFloat3 blockSizeWorld;
    float minBlockSizeWorld;
    NvFlowFloat3 blockSizeWorldInv;
    float maxBlockSizeWorld;
    NvFlowFloat3 cellSize;
    float stepSize;
    NvFlowFloat3 cellSizeInv;
    float stepSizeInv;
    NvFlowInt4 locationMin;
    NvFlowInt4 locationMax;
    NvFlowFloat3 worldMin;
    NvFlowUint enableBlockWireframe;
    NvFlowFloat3 worldMax;
    NvFlowUint enableRawMode;

    NvFlowFloat3 velocityCellSizeInv;
    float deltaTime;

    int layerAndLevel;
    float layerAndLevelColormapV;
    float alphaScale;
    float colorScale;

    float shadowFactor;
    NvFlowUint rawModeIsosurface;
    NvFlowUint rawModeNormalize;
    float pad3;

    float colormapXOffset;
    float colormapXScale;
    float pad4;
    float pad5;

    NvFlowRayMarchCloudLayerShaderParams cloud;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowRayMarchLayerShaderParams NvFlowRayMarchLayerShaderParams;
#endif

struct NvFlowRayMarchIsosurfaceLayerShaderParams
{
    NvFlowFloat3 blockSizeWorld;
    float minBlockSizeWorld;
    NvFlowFloat3 blockSizeWorldInv;
    float maxBlockSizeWorld;
    NvFlowFloat3 cellSize;
    float stepSize;
    NvFlowFloat3 cellSizeInv;
    float stepSizeInv;
    NvFlowInt4 locationMin;
    NvFlowInt4 locationMax;
    NvFlowFloat3 worldMin;
    NvFlowUint enableBlockWireframe;
    NvFlowFloat3 worldMax;
    NvFlowUint visualizeNormals;

    int layerAndLevel;
    float densityThreshold;
    NvFlowUint refractionMode;
    NvFlowUint pad2;

    NvFlowFloat3 fluidColor;
    float fluidIoR;
    NvFlowFloat3 fluidSpecularReflectance;
    float fluidAbsorptionCoefficient;
    NvFlowFloat3 fluidDiffuseReflectance;
    float pad3;
    NvFlowFloat3 fluidRadiance;
    float pad4;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowRayMarchIsosurfaceLayerShaderParams NvFlowRayMarchIsosurfaceLayerShaderParams;
#endif

struct NvFlowRayMarchShaderParams
{
    NvFlowSparseLevelParams levelParamsVelocity;
    NvFlowSparseLevelParams levelParamsDensity;

    NvFlowFloat4x4 projection;
    NvFlowFloat4x4 view;
    NvFlowFloat4x4 projectionJitteredInv;
    NvFlowFloat4x4 viewInv;

    NvFlowFloat4 rayDir00;
    NvFlowFloat4 rayDir10;
    NvFlowFloat4 rayDir01;
    NvFlowFloat4 rayDir11;

    NvFlowFloat4 rayOrigin00;
    NvFlowFloat4 rayOrigin10;
    NvFlowFloat4 rayOrigin01;
    NvFlowFloat4 rayOrigin11;

    float width;
    float height;
    float widthInv;
    float heightInv;

    float depthWidth;
    float depthHeight;
    float depthWidthInv;
    float depthHeightInv;

    NvFlowUint numLayers;
    float maxWorldDistance;
    NvFlowUint isReverseZ;
    float compositeColorScale;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowRayMarchShaderParams NvFlowRayMarchShaderParams;
#endif

struct NvFlowSelfShadowLayerShaderParams
{
    NvFlowRayMarchLayerShaderParams base;
    float minIntensity;
    NvFlowUint numSteps;
    NvFlowUint isPointLight;
    float stepOffset;
    NvFlowFloat3 lightDirection;
    NvFlowUint enabled;
    NvFlowFloat3 lightPosition;
    float pad3;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowSelfShadowLayerShaderParams NvFlowSelfShadowLayerShaderParams;
#endif

struct NvFlowSelfShadowShaderParams
{
    NvFlowUint blockIdxOffset;
    NvFlowUint pad1;
    NvFlowUint pad2;
    NvFlowUint pad3;
    NvFlowSparseLevelParams coarseDensityTable;
    NvFlowSparseLevelParams densityTable;
};
#ifdef NV_FLOW_CPU
typedef struct NvFlowSelfShadowShaderParams NvFlowSelfShadowShaderParams;
#endif

#endif
