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

#ifndef NV_FLOW_EXT_H
#define NV_FLOW_EXT_H

#include "NvFlowContext.h"
#include "NvFlow.h"

/// ********************************* Points ***************************************

typedef struct NvFlowPointsParams
{
    NvFlowUint64 luid;

    NvFlowBool32 isVisible;
    int layerOffset;
    int levelOffset;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;

    NvFlowFloat3* points;
    NvFlowUint64 pointCount;
    NvFlowUint64 pointVersion;

    NvFlowFloat3* colors;
    NvFlowUint64 colorCount;
    NvFlowUint64 colorVersion;

    float* widths;
    NvFlowUint64 widthCount;
    NvFlowUint64 widthVersion;

    NvFlowFloat3* extent;
    NvFlowUint64 extentCount;
    NvFlowUint64 extentVersion;
}NvFlowPointsParams;

#define NvFlowPointsParams_default_init { \
    0llu,                /*luid*/ \
    NV_FLOW_FALSE,      /*isVisible*/ \
    0,                  /*layerOffset*/ \
    0,                  /*levelOffset*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    0,                  /*points*/ \
    0,                  /*pointCount*/ \
    0,                  /*pointVersion*/ \
    0,                  /*colors*/ \
    0,                  /*colorCount*/ \
    0,                  /*colorVersion*/ \
    0,                  /*widths*/ \
    0,                  /*widthCount*/ \
    0,                  /*widthVersion*/ \
    0,                  /*extent*/ \
    0,                  /*extentCount*/ \
    0,                  /*extentVersion*/ \
}
static const NvFlowPointsParams NvFlowPointsParams_default = NvFlowPointsParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowPointsParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isVisible, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layerOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, points, pointCount, pointVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, colors, colorCount, colorVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, widths, widthCount, widthVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, extent, extentCount, extentVersion, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowPointsParams_default)
#undef NV_FLOW_REFLECT_TYPE

/// ********************************* Volumes ***************************************

typedef struct NvFlowNanoVdbAssetParams
{
    NvFlowUint64 luid;

    NvFlowUint* nanoVdbs;
    NvFlowUint64 nanoVdbCount;
    NvFlowUint64 nanoVdbVersion;
    NvFlowUint64 nanoVdbFirstElement;
    NvFlowUint64 nanoVdbPendingVersion;
}NvFlowNanoVdbAssetParams;

#define NvFlowNanoVdbAssetParams_default_init { \
    0llu,                /*luid*/ \
    0,                  /*nanoVdbs*/ \
    0,                  /*nanoVdbCount*/ \
    0,                  /*nanoVdbVersion*/ \
    0,                  /*nanoVdbFirstElement*/ \
    0,                  /*nanoVdbPendingVersion*/ \
}
static const NvFlowNanoVdbAssetParams NvFlowNanoVdbAssetParams_default = NvFlowNanoVdbAssetParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowNanoVdbAssetParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbs, nanoVdbCount, nanoVdbVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbFirstElement, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbPendingVersion, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowNanoVdbAssetParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowVolumeParams
{
    NvFlowUint64 luid;

    NvFlowBool32 isVisible;
    int layerOffset;
    int levelOffset;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;

    NvFlowUint64 nanoVdbRedLuid;
    NvFlowUint64 nanoVdbGreenLuid;
    NvFlowUint64 nanoVdbBlueLuid;
    NvFlowUint64 nanoVdbAlphaLuid;
    NvFlowUint64 nanoVdbRgbaLuid;
    NvFlowUint64 nanoVdbRgba8Luid;
}NvFlowVolumeParams;

#define NvFlowVolumeParams_default_init { \
    0llu,                /*luid*/ \
    NV_FLOW_FALSE,      /*isVisible*/ \
    0,                  /*layerOffset*/ \
    0,                  /*levelOffset*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    0,                  /*nanoVdbRedLuid*/ \
    0,                  /*nanoVdbGreenLuid*/ \
    0,                  /*nanoVdbBlueLuid*/ \
    0,                  /*nanoVdbAlphaLuid*/ \
    0,                  /*nanoVdbRgbaLuid*/ \
    0,                  /*nanoVdbRgba8Luid*/ \
}
static const NvFlowVolumeParams NvFlowVolumeParams_default = NvFlowVolumeParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowVolumeParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isVisible, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layerOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbRedLuid, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbGreenLuid, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbBlueLuid, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbAlphaLuid, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbRgbaLuid, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbRgba8Luid, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowVolumeParams_default)
#undef NV_FLOW_REFLECT_TYPE

/// ********************************* PointCloud ***************************************

typedef struct NvFlowPointCloudParams
{
    NvFlowUint64 luid;

    NvFlowUint64* childLuids;
    NvFlowUint64 childLuidCount;
    NvFlowUint64 childLuidVersion;

    NvFlowBool32 attributeCopyEnabled;

    NvFlowBool32 enabled;
    NvFlowBool32 autoCellSize;
    int levelCount;
    float cellSize;
    NvFlowBool32 enableLowPrecision;
    NvFlowBool32 updateWhilePaused;
    NvFlowBool32 fadeEnabled;
    float fadeRate;
    NvFlowBool32 enableSmallBlocks;

    float rayMarchAttenuation;
    float rayMarchColorScale;

    NvFlowBool32 enableStreaming;
    NvFlowBool32 streamOnce;
    NvFlowBool32 streamClearAtStart;
    NvFlowUint streamingBatchSize;
    NvFlowBool32 colorIsSrgb;
    NvFlowBool32 followVisibility;
    float widthScale;
    float coupleRate;
}NvFlowPointCloudParams;

#define NvFlowPointCloudParams_default_init { \
    0llu,                /*luid*/ \
    0,                  /*childLuids*/ \
    0,                  /*childLuidCount*/ \
    0,                  /*childLuidVersion*/ \
    NV_FLOW_TRUE,       /*attributeCopyEnabled*/ \
    NV_FLOW_TRUE,       /*enabled*/ \
    NV_FLOW_TRUE,       /*autoCellSize*/ \
    1,                  /*levelCount*/ \
    0.015625f,          /*cellSize*/ \
    NV_FLOW_TRUE,       /*enableLowPrecision*/ \
    NV_FLOW_TRUE,       /*updateWhilePaused*/ \
    NV_FLOW_FALSE,      /*fadeEnabled*/ \
    1.f,                /*fadeRate*/ \
    NV_FLOW_FALSE,      /*enableSmallBlocks*/ \
    10.f,               /*rayMarchAttenuation*/ \
    10.f,               /*rayMarchColorScale*/ \
    NV_FLOW_TRUE,       /*enableStreaming*/ \
    NV_FLOW_TRUE,       /*streamOnce*/ \
    NV_FLOW_TRUE,       /*streamClearAtStart*/ \
    4194304u,           /*streamingBatchSize*/ \
    NV_FLOW_TRUE,       /*colorIsSrgb*/ \
    NV_FLOW_TRUE,       /*followVisibility*/ \
    1.f,                /*widthScale*/ \
    100000000.f,         /*coupleRate*/ \
}
static const NvFlowPointCloudParams NvFlowPointCloudParams_default = NvFlowPointCloudParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowPointCloudParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint64, childLuids, childLuidCount, childLuidVersion, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, attributeCopyEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, autoCellSize, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelCount, 0, 0)
NV_FLOW_REFLECT_VALUE(float, cellSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableLowPrecision, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, updateWhilePaused, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, fadeEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fadeRate, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableSmallBlocks, 0, 0)
NV_FLOW_REFLECT_VALUE(float, rayMarchAttenuation, 0, 0)
NV_FLOW_REFLECT_VALUE(float, rayMarchColorScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableStreaming, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamOnce, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamClearAtStart, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, streamingBatchSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, colorIsSrgb, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, followVisibility, 0, 0)
NV_FLOW_REFLECT_VALUE(float, widthScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRate, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowPointCloudParams_default)
#undef NV_FLOW_REFLECT_TYPE

/// ********************************* EmitterSphere ***************************************

typedef struct NvFlowEmitterSphereParams
{
    NvFlowUint64 luid;

    NvFlowBool32 enabled;
    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;
    NvFlowFloat3 position;
    int layer;
    int level;
    float radius;
    NvFlowBool32 radiusIsWorldSpace;
    float allocationScale;

    NvFlowFloat3 velocity;
    float divergence;

    float temperature;
    float fuel;
    float burn;
    float smoke;

    float coupleRateVelocity;
    float coupleRateDivergence;

    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    NvFlowUint numSubSteps;

    float physicsVelocityScale;
    NvFlowBool32 applyPostPressure;

    NvFlowBool32 multisample;
    NvFlowUint numTraceSamples;
    float traceDeltaTime;
}NvFlowEmitterSphereParams;

#define NvFlowEmitterSphereParams_default_init { \
    0llu,                /*luid*/ \
    NV_FLOW_TRUE,        /*enabled*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,        /*velocityIsWorldSpace*/ \
    {0.f, 0.f, 0.f},    /*position*/ \
    0,                    /*layer*/ \
    0,                    /*level*/ \
    10.f,                /*radius*/ \
    NV_FLOW_TRUE,        /*radiusIsWorldSpace*/ \
    1.f,                /*allocationScale*/ \
    {0.f, 0.f, 400.f},    /*velocity*/ \
    0.f,                /*divergence*/ \
    0.5f,    /*temperature*/ \
    0.8f,    /*fuel*/ \
    0.f,    /*burn*/ \
    0.f,    /*smoke*/ \
    2.f,    /*coupleRateVelocity*/ \
    0.f,    /*coupleRateDivergence*/ \
    2.f,    /*coupleRateTemperature*/ \
    2.f,    /*coupleRateFuel*/ \
    0.f,    /*coupleRateBurn*/ \
    0.f,    /*coupleRateSmoke*/ \
    1u,                /*numSubSteps*/ \
    0.f,            /*physicsVelocityScale*/ \
    NV_FLOW_FALSE,    /*applyPostPressure*/ \
    NV_FLOW_FALSE,    /*multisample*/ \
    0u, /*numTraceSamples*/ \
    0.016f, /*traceDeltaTime*/ \
}
static const NvFlowEmitterSphereParams NvFlowEmitterSphereParams_default = NvFlowEmitterSphereParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterSphereParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, position, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(float, radius, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, radiusIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numSubSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(float, physicsVelocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, multisample, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numTraceSamples, 0, 0)
NV_FLOW_REFLECT_VALUE(float, traceDeltaTime, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterSphereParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterSpherePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterSphereParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterSphereParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture valueTemp;
    NvFlowSparseTexture velocity;
    NvFlowBool32 isPostPressure;
}NvFlowEmitterSpherePinsIn;

typedef struct NvFlowEmitterSpherePinsOut
{
    NvFlowSparseTexture value;
}NvFlowEmitterSpherePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterSpherePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterSphereParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterSphereParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, valueTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterSpherePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterSphere)

/// ********************************* EmitterSphereAllocate ***************************************

typedef struct NvFlowEmitterSphereAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseParams sparseParams;
    float deltaTime;
    const NvFlowEmitterSphereParams*const* params;
    NvFlowUint64 paramCount;
}NvFlowEmitterSphereAllocatePinsIn;

typedef struct NvFlowEmitterSphereAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
}NvFlowEmitterSphereAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterSphereAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterSphereParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterSphereAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterSphereAllocate)

/// ********************************* EmitterBox ***************************************

typedef struct NvFlowEmitterBoxParams
{
    NvFlowUint64 luid;

    NvFlowBool32 enabled;
    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;
    NvFlowFloat3 position;
    int layer;
    int level;
    NvFlowFloat3 halfSize;
    float allocationScale;

    NvFlowFloat3 velocity;
    float divergence;

    float temperature;
    float fuel;
    float burn;
    float smoke;

    float coupleRateVelocity;
    float coupleRateDivergence;

    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    float physicsVelocityScale;
    NvFlowBool32 applyPostPressure;

    NvFlowBool32 multisample;

    NvFlowFloat4* clippingPlanes;
    NvFlowUint64 clippingPlaneCount;
    NvFlowUint* clippingPlaneCounts;
    NvFlowUint64 clippingPlaneCountCount;

    NvFlowBool32 isPhysicsCollision;
    NvFlowBool32 allLayers;

}NvFlowEmitterBoxParams;

#define NvFlowEmitterBoxParams_default_init { \
    0llu,            /*luid*/ \
    NV_FLOW_TRUE,    /*enabled*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                        /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                        /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,            /*velocityIsWorldSpace*/ \
    {0.f, 0.f, 0.f},        /*position*/ \
    0,                        /*layer*/ \
    0,                        /*level*/ \
    {10.f, 10.f, 10.f},        /*halfSize*/ \
    1.f,                    /*allocationScale*/ \
    {0.f, 0.f, 400.f},        /*velocity*/ \
    0.f,                    /*divergence*/ \
    0.5f,    /*temperature*/ \
    0.8f,    /*fuel*/ \
    0.f,    /*burn*/ \
    0.f,    /*smoke*/ \
    2.f,    /*coupleRateVelocity*/ \
    0.f,    /*coupleRateDivergence*/ \
    2.f,    /*coupleRateTemperature*/ \
    2.f,    /*coupleRateFuel*/ \
    0.f,    /*coupleRateBurn*/ \
    0.f,    /*coupleRateSmoke*/ \
    0.f,            /*physicsVelocityScale*/ \
    NV_FLOW_FALSE,    /*applyPostPressure*/ \
    NV_FLOW_FALSE,    /*multisample*/ \
    0,    /*clippingPlanes*/ \
    0,    /*clippingPlaneCount*/ \
    0,    /*clippingPlaneCounts*/ \
    0,    /*clippingPlaneCountCount*/ \
    NV_FLOW_FALSE, /*isPhysicsCollision*/ \
    NV_FLOW_FALSE /*allLayers*/ \
}
static const NvFlowEmitterBoxParams NvFlowEmitterBoxParams_default = NvFlowEmitterBoxParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterBoxParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, position, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, halfSize, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, physicsVelocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, multisample, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, clippingPlanes, clippingPlaneCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, clippingPlaneCounts, clippingPlaneCountCount, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPhysicsCollision, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, allLayers, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterBoxParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterBoxPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterBoxParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterBoxParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    const int* physicsCollisionLayers;
    NvFlowUint64 physicsCollisionLayerCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture valueTemp;
    NvFlowBool32 isPostPressure;
}NvFlowEmitterBoxPinsIn;

typedef struct NvFlowEmitterBoxPinsOut
{
    NvFlowSparseTexture value;
}NvFlowEmitterBoxPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterBoxPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterBoxParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterBoxParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(int, physicsCollisionLayers, physicsCollisionLayerCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, valueTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterBoxPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterBox)

/// ********************************* EmitterBoxAllocate ***************************************

typedef struct NvFlowEmitterBoxAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseParams sparseParams;
    float deltaTime;
    const NvFlowEmitterBoxParams*const* params;
    NvFlowUint64 paramCount;
    const int* physicsCollisionLayers;
    NvFlowUint64 physicsCollisionLayerCount;
}NvFlowEmitterBoxAllocatePinsIn;

typedef struct NvFlowEmitterBoxAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
}NvFlowEmitterBoxAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterBoxAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterBoxParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(int, physicsCollisionLayers, physicsCollisionLayerCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterBoxAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterBoxAllocate)

/// ********************************* EmitterPoint ***************************************

typedef struct NvFlowEmitterPointParams
{
    NvFlowUint64 luid;

    NvFlowUint64* pointsLuids;
    NvFlowUint64 pointsLuidCount;
    NvFlowUint64 pointsLuidVersion;

    NvFlowBool32 enabled;
    NvFlowBool32 followVisibility;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;
    NvFlowUint numSubSteps;

    int layer;
    int level;
    int levelCount;

    NvFlowBool32 allocateMask;
    NvFlowFloat3 velocity;
    float divergence;
    float temperature;
    float fuel;
    float burn;
    float smoke;

    NvFlowBool32 colorIsSrgb;

    float widthScale;
    float velocityScale;
    float divergenceScale;
    float temperatureScale;
    float fuelScale;
    float burnScale;
    float smokeScale;

    float coupleRateVelocity;
    float coupleRateDivergence;
    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    NvFlowFloat3* pointPositions;
    NvFlowUint64 pointPositionCount;
    NvFlowUint64 pointPositionVersion;
    float* pointWidths;
    NvFlowUint64 pointWidthCount;
    NvFlowUint64 pointWidthVersion;
    NvFlowBool32* pointAllocateMasks;
    NvFlowUint64 pointAllocateMaskCount;
    NvFlowUint64 pointAllocateMaskVersion;

    NvFlowFloat3* pointVelocities;
    NvFlowUint64 pointVelocityCount;
    NvFlowUint64 pointVelocityVersion;
    float* pointDivergences;
    NvFlowUint64 pointDivergenceCount;
    NvFlowUint64 pointDivergenceVersion;
    NvFlowFloat3* pointColors;
    NvFlowUint64 pointColorCount;
    NvFlowUint64 pointColorVersion;
    float* pointTemperatures;
    NvFlowUint64 pointTemperatureCount;
    NvFlowUint64 pointTemperatureVersion;
    float* pointFuels;
    NvFlowUint64 pointFuelCount;
    NvFlowUint64 pointFuelVersion;
    float* pointBurns;
    NvFlowUint64 pointBurnCount;
    NvFlowUint64 pointBurnVersion;
    float* pointSmokes;
    NvFlowUint64 pointSmokeCount;
    NvFlowUint64 pointSmokeVersion;

    float* pointCoupleRateVelocities;
    NvFlowUint64 pointCoupleRateVelocityCount;
    NvFlowUint64 pointCoupleRateVelocityVersion;
    float* pointCoupleRateDivergences;
    NvFlowUint64 pointCoupleRateDivergenceCount;
    NvFlowUint64 pointCoupleRateDivergenceVersion;
    float* pointCoupleRateTemperatures;
    NvFlowUint64 pointCoupleRateTemperatureCount;
    NvFlowUint64 pointCoupleRateTemperatureVersion;
    float* pointCoupleRateFuels;
    NvFlowUint64 pointCoupleRateFuelCount;
    NvFlowUint64 pointCoupleRateFuelVersion;
    float* pointCoupleRateBurns;
    NvFlowUint64 pointCoupleRateBurnCount;
    NvFlowUint64 pointCoupleRateBurnVersion;
    float* pointCoupleRateSmokes;
    NvFlowUint64 pointCoupleRateSmokeCount;
    NvFlowUint64 pointCoupleRateSmokeVersion;

    NvFlowBool32 applyPostPressure;
    NvFlowBool32 enableStreaming;
    NvFlowBool32 streamOnce;
    NvFlowBool32 streamClearAtStart;
    NvFlowUint streamingBatchSize;
    NvFlowBool32 updateCoarseDensity;
    NvFlowBool32 enableInterpolation;
    NvFlowUint voxelsPerPoint;
}NvFlowEmitterPointParams;

#define NvFlowEmitterPointParams_default_init { \
    0llu,            /*luid*/ \
    0,                /*pointsLuids*/ \
    0,                /*pointsLuidCount*/ \
    0,                /*pointsLuidVersion*/ \
    NV_FLOW_TRUE,    /*enabled*/ \
    NV_FLOW_FALSE,    /*followVisibility*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,        /*velocityIsWorldSpace*/ \
    1u,                    /*numSubSteps*/ \
    0,                    /*layer*/ \
    0,                    /*level*/ \
    1,                  /*levelCount*/ \
    NV_FLOW_TRUE,        /*allocateMask*/ \
    {0.f, 0.f, 100.f},    /*velocity*/ \
    0.f,                /*divergence*/ \
    2.f,                /*temperature*/ \
    2.f,                /*fuel*/ \
    0.f,                /*burn*/ \
    2.f,                /*smoke*/ \
    NV_FLOW_FALSE,        /*colorIsSrgb*/ \
    1.f,        /*widthScale*/ \
    1.f,        /*velocityScale*/ \
    1.f,        /*divergenceScale*/ \
    1.f,        /*temperatureScale*/ \
    1.f,        /*fuelScale*/ \
    1.f,        /*burnScale*/ \
    1.f,        /*smokeScale*/ \
    200.f,        /*coupleRateVelocity*/ \
    0.f,        /*coupleRateDivergence*/ \
    200.f,        /*coupleRateTemperature*/ \
    200.f,        /*coupleRateFuel*/ \
    0.f,        /*coupleRateBurn*/ \
    200.f,        /*coupleRateSmoke*/ \
    0,        /*pointPositions*/ \
    0,        /*pointPositionCount*/ \
    0,        /*pointPositionVersion*/ \
    0,        /*pointWidths*/ \
    0,        /*pointWidthCount*/ \
    0,        /*pointWidthVersion*/ \
    0,        /*pointAllocateMasks*/ \
    0,        /*pointAllocateMaskCount*/ \
    0,        /*pointAllocateMaskVersion*/ \
    0,        /*pointVelocities*/ \
    0,        /*pointVelocityCount*/ \
    0,        /*pointVelocityVersion*/ \
    0,        /*pointDivergences*/ \
    0,        /*pointDivergenceCount*/ \
    0,        /*pointDivergenceVersion*/ \
    0,        /*pointColors*/ \
    0,        /*pointColorCount*/ \
    0,        /*pointColorVersion*/ \
    0,        /*pointTemperatures*/ \
    0,        /*pointTemperatureCount*/ \
    0,        /*pointTemperatureVersion*/ \
    0,        /*pointFuels*/ \
    0,        /*pointFuelCount*/ \
    0,        /*pointFuelVersion*/ \
    0,        /*pointBurns*/ \
    0,        /*pointBurnCount*/ \
    0,        /*pointBurnVersion*/ \
    0,        /*pointSmokes*/ \
    0,        /*pointSmokeCount*/ \
    0,        /*pointSmokeVersion*/ \
    0,        /*pointCoupleRateVelocities*/ \
    0,        /*pointCoupleRateVelocityCount*/ \
    0,        /*pointCoupleRateVelocityVersion*/ \
    0,        /*pointCoupleRateDivergences*/ \
    0,        /*pointCoupleRateDivergenceCount*/ \
    0,        /*pointCoupleRateDivergenceVersion*/ \
    0,        /*pointCoupleRateTemperatures*/ \
    0,        /*pointCoupleRateTemperatureCount*/ \
    0,        /*pointCoupleRateTemperatureVersion*/ \
    0,        /*pointCoupleRateFuels*/ \
    0,        /*pointCoupleRateFuelCount*/ \
    0,        /*pointCoupleRateFuelVersion*/ \
    0,        /*pointCoupleRateBurns*/ \
    0,        /*pointCoupleRateBurnCount*/ \
    0,        /*pointCoupleRateBurnVersion*/ \
    0,        /*pointCoupleRateSmokes*/ \
    0,        /*pointCoupleRateSmokeCount*/ \
    0,        /*pointCoupleRateSmokeVersion*/ \
    NV_FLOW_FALSE,    /*applyPostPressure*/ \
    NV_FLOW_FALSE,    /*enableStreaming*/ \
    NV_FLOW_FALSE,    /*streamOnce*/ \
    NV_FLOW_FALSE,    /*streamClearAtStart*/ \
    1048576,        /*streamingBatchSize*/ \
    NV_FLOW_FALSE,    /*updateCoarseDensity*/ \
    NV_FLOW_FALSE,    /*enableInterpolation*/ \
    8u,    /*voxelsPerPoint*/ \
}
static const NvFlowEmitterPointParams NvFlowEmitterPointParams_default = NvFlowEmitterPointParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterPointParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint64, pointsLuids, pointsLuidCount, pointsLuidVersion, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, followVisibility, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numSubSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelCount, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, allocateMask, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, colorIsSrgb, 0, 0)
NV_FLOW_REFLECT_VALUE(float, widthScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergenceScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperatureScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, pointPositions, pointPositionCount, pointPositionVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointWidths, pointWidthCount, pointWidthVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowBool32, pointAllocateMasks, pointAllocateMaskCount, pointAllocateMaskVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, pointVelocities, pointVelocityCount, pointVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointDivergences, pointDivergenceCount, pointDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, pointColors, pointColorCount, pointColorVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointTemperatures, pointTemperatureCount, pointTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointFuels, pointFuelCount, pointFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointBurns, pointBurnCount, pointBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointSmokes, pointSmokeCount, pointSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateVelocities, pointCoupleRateVelocityCount, pointCoupleRateVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateDivergences, pointCoupleRateDivergenceCount, pointCoupleRateDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateTemperatures, pointCoupleRateTemperatureCount, pointCoupleRateTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateFuels, pointCoupleRateFuelCount, pointCoupleRateFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateBurns, pointCoupleRateBurnCount, pointCoupleRateBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, pointCoupleRateSmokes, pointCoupleRateSmokeCount, pointCoupleRateSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableStreaming, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamOnce, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamClearAtStart, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, streamingBatchSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, updateCoarseDensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableInterpolation, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, voxelsPerPoint, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterPointParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterPointFeedback
{
    void* data;
}NvFlowEmitterPointFeedback;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowEmitterPointFeedback)

typedef struct NvFlowEmitterPointPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterPointParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterPointParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    const NvFlowPointsParams* const* pointsParams;
    NvFlowUint64 pointsParamCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture voxelWeight;
    NvFlowSparseTexture coarseDensity;
    NvFlowBool32 isPostPressure;
    NvFlowEmitterPointFeedback feedback;
    NvFlowUint64 streamingVersion;
    NvFlowUint64 streamingFinishedVersion;
    NvFlowUint64 streamingPointsVersion;
    NvFlowUint64 streamingNanoVdbVersion;
}NvFlowEmitterPointPinsIn;

typedef struct NvFlowEmitterPointPinsOut
{
    NvFlowSparseTexture value;
    NvFlowSparseTexture voxelWeight;
    NvFlowUint64 streamingVersion;
    NvFlowUint64 streamingFinishedVersion;
    NvFlowUint64 streamingPointsVersion;
}NvFlowEmitterPointPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterPointPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterPointParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterPointParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowPointsParams, pointsParams, pointsParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensity, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterPointFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingFinishedVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingPointsVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingNanoVdbVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterPointPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingFinishedVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingPointsVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterPoint)

/// ********************************* EmitterPointAllocate ***************************************

typedef struct NvFlowEmitterPointAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseSimParams sparseSimParams;
    float deltaTime;
    const NvFlowEmitterPointParams*const* params;
    NvFlowUint64 paramCount;
    const NvFlowPointsParams* const* pointsParams;
    NvFlowUint64 pointsParamCount;
    NvFlowUint3 baseBlockDimBits;
}NvFlowEmitterPointAllocatePinsIn;

typedef struct NvFlowEmitterPointAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
    NvFlowEmitterPointFeedback feedback;
}NvFlowEmitterPointAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterPointAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseSimParams, sparseSimParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterPointParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowPointsParams, pointsParams, pointsParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint3, baseBlockDimBits, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterPointAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterPointFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterPointAllocate)

/// ********************************* EmitterMesh ***************************************

typedef struct NvFlowEmitterMeshParams
{
    NvFlowUint64 luid;

    NvFlowBool32 enabled;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;
    NvFlowUint numSubSteps;

    int layer;
    int level;

    float minDistance;
    float maxDistance;

    NvFlowBool32 allocateMask;
    NvFlowFloat3 velocity;
    float divergence;
    float temperature;
    float fuel;
    float burn;
    float smoke;

    NvFlowBool32 orientationLeftHanded;
    NvFlowBool32 colorIsSrgb;

    float velocityScale;
    float divergenceScale;
    float temperatureScale;
    float fuelScale;
    float burnScale;
    float smokeScale;

    float coupleRateVelocity;
    float coupleRateDivergence;
    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    int* meshSubsetFaceCounts;
    NvFlowUint64 meshSubsetFaceCountCount;
    NvFlowUint64 meshSubsetFaceCountVersion;
    int* meshSubsetLayers;
    NvFlowUint64 meshSubsetLayerCount;
    NvFlowUint64 meshSubsetLayerVersion;
    NvFlowBool32* meshSubsetEnableds;
    NvFlowUint64 meshSubsetEnabledCount;
    NvFlowUint64 meshSubsetEnabledVersion;

    NvFlowFloat3* meshPositions;
    NvFlowUint64 meshPositionCount;
    NvFlowUint64 meshPositionVersion;
    int* meshFaceVertexIndices;
    NvFlowUint64 meshFaceVertexIndexCount;
    NvFlowUint64 meshFaceVertexIndexVersion;
    int* meshFaceVertexCounts;
    NvFlowUint64 meshFaceVertexCountCount;
    NvFlowUint64 meshFaceVertexCountVersion;

    NvFlowFloat3* meshVelocities;
    NvFlowUint64 meshVelocityCount;
    NvFlowUint64 meshVelocityVersion;
    float* meshDivergences;
    NvFlowUint64 meshDivergenceCount;
    NvFlowUint64 meshDivergenceVersion;
    NvFlowFloat3* meshColors;
    NvFlowUint64 meshColorCount;
    NvFlowUint64 meshColorVersion;
    float* meshTemperatures;
    NvFlowUint64 meshTemperatureCount;
    NvFlowUint64 meshTemperatureVersion;
    float* meshFuels;
    NvFlowUint64 meshFuelCount;
    NvFlowUint64 meshFuelVersion;
    float* meshBurns;
    NvFlowUint64 meshBurnCount;
    NvFlowUint64 meshBurnVersion;
    float* meshSmokes;
    NvFlowUint64 meshSmokeCount;
    NvFlowUint64 meshSmokeVersion;

    float* meshCoupleRateVelocities;
    NvFlowUint64 meshCoupleRateVelocityCount;
    NvFlowUint64 meshCoupleRateVelocityVersion;
    float* meshCoupleRateDivergences;
    NvFlowUint64 meshCoupleRateDivergenceCount;
    NvFlowUint64 meshCoupleRateDivergenceVersion;
    float* meshCoupleRateTemperatures;
    NvFlowUint64 meshCoupleRateTemperatureCount;
    NvFlowUint64 meshCoupleRateTemperatureVersion;
    float* meshCoupleRateFuels;
    NvFlowUint64 meshCoupleRateFuelCount;
    NvFlowUint64 meshCoupleRateFuelVersion;
    float* meshCoupleRateBurns;
    NvFlowUint64 meshCoupleRateBurnCount;
    NvFlowUint64 meshCoupleRateBurnVersion;
    float* meshCoupleRateSmokes;
    NvFlowUint64 meshCoupleRateSmokeCount;
    NvFlowUint64 meshCoupleRateSmokeVersion;

    float physicsVelocityScale;
    NvFlowBool32 applyPostPressure;
    NvFlowBool32 isPhysicsCollision;
}NvFlowEmitterMeshParams;

#define NvFlowEmitterMeshParams_default_init { \
    0llu,            /*luid*/ \
    NV_FLOW_TRUE,    /*enabled*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,        /*velocityIsWorldSpace*/ \
    1u,                    /*numSubSteps*/ \
    0,                    /*layer*/ \
    0,                    /*level*/ \
    -0.8f,                /*minDistance*/ \
    0.3f,                /*minDistance*/ \
    NV_FLOW_TRUE,        /*allocateMask*/ \
    {0.f, 0.f, 100.f},    /*velocity*/ \
    0.f,                /*divergence*/ \
    2.f,                /*temperature*/ \
    0.8f,                /*fuel*/ \
    0.f,                /*burn*/ \
    2.f,                /*smoke*/ \
    NV_FLOW_FALSE,        /*orientationLeftHanded*/ \
    NV_FLOW_FALSE,        /*colorIsSrgb*/ \
    1.f,        /*velocityScale*/ \
    1.f,        /*divergenceScale*/ \
    1.f,        /*temperatureScale*/ \
    1.f,        /*fuelScale*/ \
    1.f,        /*burnScale*/ \
    1.f,        /*smokeScale*/ \
    2.f,        /*coupleRateVelocity*/ \
    0.f,        /*coupleRateDivergence*/ \
    10.f,        /*coupleRateTemperature*/ \
    2.f,        /*coupleRateFuel*/ \
    0.f,        /*coupleRateBurn*/ \
    2.f,        /*coupleRateSmoke*/ \
    0,        /*meshSubsetFaceCounts*/ \
    0,        /*meshSubsetFaceCountCount*/ \
    0,        /*meshSubsetFaceCountVersion*/ \
    0,        /*meshSubsetLayers*/ \
    0,        /*meshSubsetLayerCount*/ \
    0,        /*meshSubsetLayerVersion*/ \
    0,        /*meshSubsetEnableds*/ \
    0,        /*meshSubsetEnabledCount*/ \
    0,        /*meshSubsetEnabledVersion*/ \
    0,        /*meshPositions*/ \
    0,        /*meshPositionCount*/ \
    0,        /*meshPositionVersion*/ \
    0,        /*meshFaceVertexIndices*/ \
    0,        /*meshFaceVertexIndexCount;*/ \
    0,        /*meshFaceVertexIndexVersion*/ \
    0,        /*meshFaceVertexCounts*/ \
    0,        /*meshFaceVertexCountCount*/ \
    0,        /*meshFaceVertexCountVersion*/ \
    0,        /*meshVelocities*/ \
    0,        /*meshVelocityCount*/ \
    0,        /*meshVelocityVersion*/ \
    0,        /*meshDivergences*/ \
    0,        /*meshDivergenceCount*/ \
    0,        /*meshDivergenceVersion*/ \
    0,        /*meshColors*/ \
    0,        /*meshColorCount*/ \
    0,        /*meshColorVersion*/ \
    0,        /*meshTemperatures*/ \
    0,        /*meshTemperatureCount*/ \
    0,        /*meshTemperatureVersion*/ \
    0,        /*meshFuels*/ \
    0,        /*meshFuelCount*/ \
    0,        /*meshFuelVersion*/ \
    0,        /*meshBurns*/ \
    0,        /*meshBurnCount*/ \
    0,        /*meshBurnVersion*/ \
    0,        /*meshSmokes*/ \
    0,        /*meshSmokeCount*/ \
    0,        /*meshSmokeVersion*/ \
    0,        /*meshCoupleRateVelocities*/ \
    0,        /*meshCoupleRateVelocityCount*/ \
    0,        /*meshCoupleRateVelocityVersion*/ \
    0,        /*meshCoupleRateDivergences*/ \
    0,        /*meshCoupleRateDivergenceCount*/ \
    0,        /*meshCoupleRateDivergenceVersion*/ \
    0,        /*meshCoupleRateTemperatures*/ \
    0,        /*meshCoupleRateTemperatureCount*/ \
    0,        /*meshCoupleRateTemperatureVersion*/ \
    0,        /*meshCoupleRateFuels*/ \
    0,        /*meshCoupleRateFuelCount*/ \
    0,        /*meshCoupleRateFuelVersion*/ \
    0,        /*meshCoupleRateBurns*/ \
    0,        /*meshCoupleRateBurnCount*/ \
    0,        /*meshCoupleRateBurnVersion*/ \
    0,        /*meshCoupleRateSmokes*/ \
    0,        /*meshCoupleRateSmokeCount*/ \
    0,        /*meshCoupleRateSmokeVersion*/ \
    0.f,    /*physicsVelocityScale*/ \
    NV_FLOW_FALSE,    /*applyPostPressure*/ \
    NV_FLOW_FALSE,  /*isPhysicsCollision*/ \
}
static const NvFlowEmitterMeshParams NvFlowEmitterMeshParams_default = NvFlowEmitterMeshParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterMeshParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numSubSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(float, minDistance, 0, 0)
NV_FLOW_REFLECT_VALUE(float, maxDistance, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, allocateMask, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, orientationLeftHanded, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, colorIsSrgb, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergenceScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperatureScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(int, meshSubsetFaceCounts, meshSubsetFaceCountCount, meshSubsetFaceCountVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(int, meshSubsetLayers, meshSubsetLayerCount, meshSubsetLayerVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowBool32, meshSubsetEnableds, meshSubsetEnabledCount, meshSubsetEnabledVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, meshPositions, meshPositionCount, meshPositionVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(int, meshFaceVertexIndices, meshFaceVertexIndexCount, meshFaceVertexIndexVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(int, meshFaceVertexCounts, meshFaceVertexCountCount, meshFaceVertexCountVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, meshVelocities, meshVelocityCount, meshVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshDivergences, meshDivergenceCount, meshDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, meshColors, meshColorCount, meshColorVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshTemperatures, meshTemperatureCount, meshTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshFuels, meshFuelCount, meshFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshBurns, meshBurnCount, meshBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshSmokes, meshSmokeCount, meshSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateVelocities, meshCoupleRateVelocityCount, meshCoupleRateVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateDivergences, meshCoupleRateDivergenceCount, meshCoupleRateDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateTemperatures, meshCoupleRateTemperatureCount, meshCoupleRateTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateFuels, meshCoupleRateFuelCount, meshCoupleRateFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateBurns, meshCoupleRateBurnCount, meshCoupleRateBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, meshCoupleRateSmokes, meshCoupleRateSmokeCount, meshCoupleRateSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_VALUE(float, physicsVelocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPhysicsCollision, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterMeshParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterMeshFeedback
{
    void* data;
}NvFlowEmitterMeshFeedback;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowEmitterMeshFeedback)

typedef struct NvFlowEmitterMeshPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterMeshParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterMeshParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    const int* physicsCollisionLayers;
    NvFlowUint64 physicsCollisionLayerCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture valueTemp;
    NvFlowBool32 isPostPressure;
    NvFlowEmitterMeshFeedback feedback;
}NvFlowEmitterMeshPinsIn;

typedef struct NvFlowEmitterMeshPinsOut
{
    NvFlowSparseTexture value;
}NvFlowEmitterMeshPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterMeshPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterMeshParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterMeshParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(int, physicsCollisionLayers, physicsCollisionLayerCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, valueTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterMeshFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterMeshPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterMesh)

/// ********************************* EmitterMeshAllocate ***************************************

typedef struct NvFlowEmitterMeshAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseParams sparseParams;
    float deltaTime;
    const NvFlowEmitterMeshParams*const* params;
    NvFlowUint64 paramCount;
    const int* physicsCollisionLayers;
    NvFlowUint64 physicsCollisionLayerCount;
    NvFlowUint3 baseBlockDimBits;
}NvFlowEmitterMeshAllocatePinsIn;

typedef struct NvFlowEmitterMeshAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
    NvFlowEmitterMeshFeedback feedback;
}NvFlowEmitterMeshAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterMeshAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterMeshParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(int, physicsCollisionLayers, physicsCollisionLayerCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint3, baseBlockDimBits, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterMeshAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterMeshFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterMeshAllocate)

/// ********************************* EmitterTexture ***************************************

typedef struct NvFlowEmitterTextureParams
{
    NvFlowUint64 luid;

    NvFlowBool32 enabled;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;
    NvFlowFloat3 position;
    int layer;
    int level;
    NvFlowFloat3 halfSize;
    float allocationScale;

    NvFlowFloat3 velocity;
    float divergence;

    float temperature;
    float fuel;
    float burn;
    float smoke;

    NvFlowBool32 colorIsSrgb;

    float velocityScale;
    float divergenceScale;
    float temperatureScale;
    float fuelScale;
    float burnScale;
    float smokeScale;

    float coupleRateVelocity;
    float coupleRateDivergence;

    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    NvFlowUint textureWidth;
    NvFlowUint textureHeight;
    NvFlowUint textureDepth;
    NvFlowUint textureFirstElement;

    NvFlowBool32 textureIsColumnMajor;

    float minDistance;
    float maxDistance;

    float* textureDistances;
    NvFlowUint64 textureDistanceCount;
    NvFlowUint64 textureDistanceVersion;

    NvFlowFloat3* textureVelocities;
    NvFlowUint64 textureVelocityCount;
    NvFlowUint64 textureVelocityVersion;
    float* textureDivergences;
    NvFlowUint64 textureDivergenceCount;
    NvFlowUint64 textureDivergenceVersion;
    float* textureTemperatures;
    NvFlowUint64 textureTemperatureCount;
    NvFlowUint64 textureTemperatureVersion;
    float* textureFuels;
    NvFlowUint64 textureFuelCount;
    NvFlowUint64 textureFuelVersion;
    float* textureBurns;
    NvFlowUint64 textureBurnCount;
    NvFlowUint64 textureBurnVersion;
    float* textureSmokes;
    NvFlowUint64 textureSmokeCount;
    NvFlowUint64 textureSmokeVersion;

    float* textureCoupleRateVelocities;
    NvFlowUint64 textureCoupleRateVelocityCount;
    NvFlowUint64 textureCoupleRateVelocityVersion;
    float* textureCoupleRateDivergences;
    NvFlowUint64 textureCoupleRateDivergenceCount;
    NvFlowUint64 textureCoupleRateDivergenceVersion;
    float* textureCoupleRateTemperatures;
    NvFlowUint64 textureCoupleRateTemperatureCount;
    NvFlowUint64 textureCoupleRateTemperatureVersion;
    float* textureCoupleRateFuels;
    NvFlowUint64 textureCoupleRateFuelCount;
    NvFlowUint64 textureCoupleRateFuelVersion;
    float* textureCoupleRateBurns;
    NvFlowUint64 textureCoupleRateBurnCount;
    NvFlowUint64 textureCoupleRateBurnVersion;
    float* textureCoupleRateSmokes;
    NvFlowUint64 textureCoupleRateSmokeCount;
    NvFlowUint64 textureCoupleRateSmokeVersion;

    NvFlowBool32 applyPostPressure;
}NvFlowEmitterTextureParams;

#define NvFlowEmitterTextureParams_default_init { \
    0llu,                /*luid*/ \
    NV_FLOW_TRUE,        /*enabled*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,        /*velocityIsWorldSpace*/ \
    {0.f, 0.f, 0.f},    /*position*/ \
    0,                    /*layer*/ \
    0,                    /*level*/ \
    {10.f, 10.f, 10.f},    /*halfSize*/ \
    1.f,                /*fallocationScale*/ \
    {0.f, 0.f, 400.f},    /*velocity*/ \
    0.f,                /*divergence*/ \
    0.5f,    /*temperature*/ \
    0.8f,    /*fuel*/ \
    0.f,    /*burn*/ \
    0.f,    /*smoke*/ \
    NV_FLOW_FALSE,        /*colorIsSrgb*/ \
    1.f,        /*velocityScale*/ \
    1.f,        /*divergenceScale*/ \
    1.f,        /*temperatureScale*/ \
    1.f,        /*fuelScale*/ \
    1.f,        /*burnScale*/ \
    1.f,        /*smokeScale*/ \
    2.f,    /*coupleRateVelocity*/ \
    0.f,    /*coupleRateDivergence*/ \
    2.f,    /*coupleRateTemperature*/ \
    2.f,    /*coupleRateFuel*/ \
    0.f,    /*coupleRateBurn*/ \
    0.f,    /*coupleRateSmoke*/ \
    0u,        /*textureWidth*/ \
    0u,        /*textureHeight*/ \
    0u,        /*textureDepth*/ \
    0u,        /*textureFirstElement*/ \
    NV_FLOW_FALSE, /*textureIsColumnMajor*/ \
    -0.25f,    /*minDistance*/ \
    0.25f,    /*maxDistance*/ \
    0,    /*textureDistances*/ \
    0,    /*textureDistanceCount*/ \
    0,    /*textureDistanceVersion*/ \
    0,    /*textureVelocities*/ \
    0,    /*textureVelocityCount*/ \
    0,    /*textureVelocityVersion*/ \
    0,    /*textureDivergences*/ \
    0,    /*textureDivergenceCount*/ \
    0,    /*textureDivergenceVersion*/ \
    0,    /*textureTemperatures*/ \
    0,    /*textureTemperatureCount*/ \
    0,    /*textureTemperatureVersion*/ \
    0,    /*textureFuels*/ \
    0,    /*textureFuelCount*/ \
    0,    /*textureFuelVersion*/ \
    0,    /*textureBurns*/ \
    0,    /*textureBurnCount*/ \
    0,    /*textureBurnVersion*/ \
    0,    /*textureSmokes*/ \
    0,    /*textureSmokeCount*/ \
    0,    /*textureSmokeVersion*/ \
    0,    /*textureCoupleRateVelocities*/ \
    0,    /*textureCoupleRateVelocityCount*/ \
    0,    /*textureCoupleRateVelocityVersion*/ \
    0,    /*textureCoupleRateDivergences*/ \
    0,    /*textureCoupleRateDivergenceCount*/ \
    0,    /*textureCoupleRateDivergenceVersion*/ \
    0,    /*textureCoupleRateTemperatures*/ \
    0,    /*textureCoupleRateTemperatureCount*/ \
    0,    /*textureCoupleRateTemperatureVersion*/ \
    0,    /*textureCoupleRateFuels*/ \
    0,    /*textureCoupleRateFuelCount*/ \
    0,    /*textureCoupleRateFuelVersion*/ \
    0,    /*textureCoupleRateBurns*/ \
    0,    /*textureCoupleRateBurnCount*/ \
    0,    /*textureCoupleRateBurnVersion*/ \
    0,    /*textureCoupleRateSmokes*/ \
    0,    /*textureCoupleRateSmokeCount*/ \
    0,    /*textureCoupleRateSmokeVersion*/ \
    NV_FLOW_FALSE    /*applyPostPressure*/ \
}
static const NvFlowEmitterTextureParams NvFlowEmitterTextureParams_default = NvFlowEmitterTextureParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterTextureParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, position, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, halfSize, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, colorIsSrgb, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergenceScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperatureScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, textureWidth, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, textureHeight, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, textureDepth, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, textureFirstElement, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, textureIsColumnMajor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, minDistance, 0, 0)
NV_FLOW_REFLECT_VALUE(float, maxDistance, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureDistances, textureDistanceCount, textureDistanceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowFloat3, textureVelocities, textureVelocityCount, textureVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureDivergences, textureDivergenceCount, textureDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureTemperatures, textureTemperatureCount, textureTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureFuels, textureFuelCount, textureFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureBurns, textureBurnCount, textureBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureSmokes, textureSmokeCount, textureSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateVelocities, textureCoupleRateVelocityCount, textureCoupleRateVelocityVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateDivergences, textureCoupleRateDivergenceCount, textureCoupleRateDivergenceVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateTemperatures, textureCoupleRateTemperatureCount, textureCoupleRateTemperatureVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateFuels, textureCoupleRateFuelCount, textureCoupleRateFuelVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateBurns, textureCoupleRateBurnCount, textureCoupleRateBurnVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY_VERSIONED(float, textureCoupleRateSmokes, textureCoupleRateSmokeCount, textureCoupleRateSmokeVersion, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterTextureParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterTexturePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterTextureParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterTextureParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture valueTemp;
    NvFlowBool32 isPostPressure;
}NvFlowEmitterTexturePinsIn;

typedef struct NvFlowEmitterTexturePinsOut
{
    NvFlowSparseTexture value;
}NvFlowEmitterTexturePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterTexturePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterTextureParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterTextureParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, valueTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterTexturePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterTexture)

/// ********************************* EmitterTextureAllocate ***************************************

typedef struct NvFlowEmitterTextureAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseParams sparseParams;
    float deltaTime;
    const NvFlowEmitterTextureParams*const* params;
    NvFlowUint64 paramCount;
}NvFlowEmitterTextureAllocatePinsIn;

typedef struct NvFlowEmitterTextureAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
}NvFlowEmitterTextureAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterTextureAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterTextureParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterTextureAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterTextureAllocate)

/// ********************************* EmitterNanoVdb ***************************************

typedef struct NvFlowEmitterNanoVdbParams
{
    NvFlowUint64 luid;

    NvFlowUint64* volumeLuids;
    NvFlowUint64 volumeLuidCount;
    NvFlowUint64 volumeLuidVersion;

    NvFlowBool32 enabled;
    NvFlowBool32 followVisibility;

    NvFlowFloat4x4 localToWorld;
    NvFlowFloat4x4 localToWorldVelocity;
    NvFlowBool32 velocityIsWorldSpace;

    int layer;
    int level;
    int levelCount;
    float allocationScale;

    NvFlowFloat3 velocity;
    float divergence;

    float temperature;
    float fuel;
    float burn;
    float smoke;

    NvFlowBool32 colorIsSrgb;
    NvFlowBool32 swapChannels;
    NvFlowBool32 absoluteValue;
    NvFlowBool32 computeSpeed;

    float velocityScale;
    float divergenceScale;
    float temperatureScale;
    float fuelScale;
    float burnScale;
    float smokeScale;

    float coupleRateVelocity;
    float coupleRateDivergence;

    float coupleRateTemperature;
    float coupleRateFuel;
    float coupleRateBurn;
    float coupleRateSmoke;

    float minDistance;
    float maxDistance;
    float minSmoke;
    float maxSmoke;

    NvFlowUint* nanoVdbDistances;
    NvFlowUint64 nanoVdbDistanceCount;
    NvFlowUint64 nanoVdbDistanceVersion;
    NvFlowUint64 nanoVdbDistanceFirstElement;

    NvFlowUint* nanoVdbVelocities;
    NvFlowUint64 nanoVdbVelocityCount;
    NvFlowUint64 nanoVdbVelocityVersion;
    NvFlowUint64 nanoVdbVelocityFirstElement;

    NvFlowUint* nanoVdbDivergences;
    NvFlowUint64 nanoVdbDivergenceCount;
    NvFlowUint64 nanoVdbDivergenceVersion;
    NvFlowUint64 nanoVdbDivergenceFirstElement;

    NvFlowUint* nanoVdbTemperatures;
    NvFlowUint64 nanoVdbTemperatureCount;
    NvFlowUint64 nanoVdbTemperatureVersion;
    NvFlowUint64 nanoVdbTemperatureFirstElement;

    NvFlowUint* nanoVdbFuels;
    NvFlowUint64 nanoVdbFuelCount;
    NvFlowUint64 nanoVdbFuelVersion;
    NvFlowUint64 nanoVdbFuelFirstElement;

    NvFlowUint* nanoVdbBurns;
    NvFlowUint64 nanoVdbBurnCount;
    NvFlowUint64 nanoVdbBurnVersion;
    NvFlowUint64 nanoVdbBurnFirstElement;

    NvFlowUint* nanoVdbSmokes;
    NvFlowUint64 nanoVdbSmokeCount;
    NvFlowUint64 nanoVdbSmokeVersion;
    NvFlowUint64 nanoVdbSmokeFirstElement;

    NvFlowUint* nanoVdbCoupleRateVelocities;
    NvFlowUint64 nanoVdbCoupleRateVelocityCount;
    NvFlowUint64 nanoVdbCoupleRateVelocityVersion;
    NvFlowUint64 nanoVdbCoupleRateVelocityFirstElement;

    NvFlowUint* nanoVdbCoupleRateDivergences;
    NvFlowUint64 nanoVdbCoupleRateDivergenceCount;
    NvFlowUint64 nanoVdbCoupleRateDivergenceVersion;
    NvFlowUint64 nanoVdbCoupleRateDivergenceFirstElement;

    NvFlowUint* nanoVdbCoupleRateTemperatures;
    NvFlowUint64 nanoVdbCoupleRateTemperatureCount;
    NvFlowUint64 nanoVdbCoupleRateTemperatureVersion;
    NvFlowUint64 nanoVdbCoupleRateTemperatureFirstElement;

    NvFlowUint* nanoVdbCoupleRateFuels;
    NvFlowUint64 nanoVdbCoupleRateFuelCount;
    NvFlowUint64 nanoVdbCoupleRateFuelVersion;
    NvFlowUint64 nanoVdbCoupleRateFuelFirstElement;

    NvFlowUint* nanoVdbCoupleRateBurns;
    NvFlowUint64 nanoVdbCoupleRateBurnCount;
    NvFlowUint64 nanoVdbCoupleRateBurnVersion;
    NvFlowUint64 nanoVdbCoupleRateBurnFirstElement;

    NvFlowUint* nanoVdbCoupleRateSmokes;
    NvFlowUint64 nanoVdbCoupleRateSmokeCount;
    NvFlowUint64 nanoVdbCoupleRateSmokeVersion;
    NvFlowUint64 nanoVdbCoupleRateSmokeFirstElement;

    NvFlowUint* nanoVdbRgba8s;
    NvFlowUint64 nanoVdbRgba8Count;
    NvFlowUint64 nanoVdbRgba8Version;
    NvFlowUint64 nanoVdbRgba8FirstElement;

    NvFlowBool32 applyPostPressure;
    NvFlowBool32 enableStreaming;
    NvFlowBool32 streamOnce;
    NvFlowBool32 streamClearAtStart;
    NvFlowUint streamingBatchSize;
    NvFlowBool32 updateCoarseDensity;
    NvFlowBool32 allocateActiveLeaves;

    NvFlowUint64 nanoVdbDistancesInterop;
    NvFlowUint64 nanoVdbDistanceCountInterop;
    NvFlowUint64 nanoVdbVelocitiesInterop;
    NvFlowUint64 nanoVdbVelocityCountInterop;
    NvFlowUint64 nanoVdbDivergencesInterop;
    NvFlowUint64 nanoVdbDivergenceCountInterop;
    NvFlowUint64 nanoVdbTemperaturesInterop;
    NvFlowUint64 nanoVdbTemperatureCountInterop;
    NvFlowUint64 nanoVdbFuelsInterop;
    NvFlowUint64 nanoVdbFuelCountInterop;
    NvFlowUint64 nanoVdbBurnsInterop;
    NvFlowUint64 nanoVdbBurnCountInterop;
    NvFlowUint64 nanoVdbSmokesInterop;
    NvFlowUint64 nanoVdbSmokeCountInterop;
}NvFlowEmitterNanoVdbParams;

#define NvFlowEmitterNanoVdbParams_default_init { \
    0llu,                /*luid*/ \
    0,                    /*volumeLuids*/ \
    0,                    /*volumeLuidCount*/ \
    0,                    /*volumeLuidVersion*/ \
    NV_FLOW_TRUE,        /*enabled*/ \
    NV_FLOW_FALSE,      /*followVisibility*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorld*/ \
    { \
        1.f, 0.f, 0.f, 0.f, \
        0.f, 1.f, 0.f, 0.f, \
        0.f, 0.f, 1.f, 0.f, \
        0.f, 0.f, 0.f, 1.f \
    },                    /*localToWorldVelocity*/ \
    NV_FLOW_FALSE,        /*velocityIsWorldSpace*/ \
    0,                    /*layer*/ \
    0,                    /*level*/ \
    1,                  /*levelCount*/ \
    1.f,                /*fallocationScale*/ \
    {0.f, 0.f, 0.f},    /*velocity*/ \
    0.f,                /*divergence*/ \
    2.f,    /*temperature*/ \
    0.0f,    /*fuel*/ \
    0.f,    /*burn*/ \
    10.f,    /*smoke*/ \
    NV_FLOW_FALSE,        /*colorIsSrgb*/ \
    NV_FLOW_FALSE,        /*swapChannels*/ \
    NV_FLOW_FALSE,        /*absoluteValue*/ \
    NV_FLOW_FALSE,        /*computeSpeed*/ \
    1.f,        /*velocityScale*/ \
    1.f,        /*divergenceScale*/ \
    1.f,        /*temperatureScale*/ \
    1.f,        /*fuelScale*/ \
    1.f,        /*burnScale*/ \
    1.f,        /*smokeScale*/ \
    2.f,    /*coupleRateVelocity*/ \
    0.f,    /*coupleRateDivergence*/ \
    2.f,    /*coupleRateTemperature*/ \
    0.f,    /*coupleRateFuel*/ \
    0.f,    /*coupleRateBurn*/ \
    2.f,    /*coupleRateSmoke*/ \
    -0.25f,    /*minDistance*/ \
    0.25f,    /*maxDistance*/ \
    -NV_FLOW_INFINITY,    /*minSmoke*/ \
    NV_FLOW_INFINITY,    /*maxSmoke*/ \
    0, /*nanoVdbDistances*/ \
    0, /*nanoVdbDistanceCount*/ \
    0, /*nanoVdbDistanceVersion*/ \
    0, /*nanoVdbDistanceFirstElement*/ \
    0, /*nanoVdbVelocities*/ \
    0, /*nanoVdbVelocityCount*/ \
    0, /*nanoVdbVelocityVersion*/ \
    0, /*nanoVdbVelocityFirstElement*/ \
    0, /*nanoVdbDivergences*/ \
    0, /*nanoVdbDivergenceCount*/ \
    0, /*nanoVdbDivergenceVersion*/ \
    0, /*nanoVdbDivergenceFirstElement*/ \
    0, /*nanoVdbTemperatures*/ \
    0, /*nanoVdbTemperatureCount*/ \
    0, /*nanoVdbTemperatureVersion*/ \
    0, /*nanoVdbTemperatureFirstElement*/ \
    0, /*nanoVdbFuels*/ \
    0, /*nanoVdbFuelCount*/ \
    0, /*nanoVdbFuelVersion*/ \
    0, /*nanoVdbFuelFirstElement*/ \
    0, /*nanoVdbBurns*/ \
    0, /*nanoVdbBurnCount*/ \
    0, /*nanoVdbBurnVersion*/ \
    0, /*nanoVdbBurnFirstElement*/ \
    0, /*nanoVdbSmokes*/ \
    0, /*nanoVdbSmokeCount*/ \
    0, /*nanoVdbSmokeVersion*/ \
    0, /*nanoVdbSmokeFirstElement*/ \
    0, /*nanoVdbCoupleRateVelocities*/ \
    0, /*nanoVdbCoupleRateVelocityCount*/ \
    0, /*nanoVdbCoupleRateVelocityVersion*/ \
    0, /*nanoVdbCoupleRateVelocityFirstElement*/ \
    0, /*nanoVdbCoupleRateDivergences*/ \
    0, /*nanoVdbCoupleRateDivergenceCount*/ \
    0, /*nanoVdbCoupleRateDivergenceVersion*/ \
    0, /*nanoVdbCoupleRateDivergenceFirstElement*/ \
    0, /*nanoVdbCoupleRateTemperatures*/ \
    0, /*nanoVdbCoupleRateTemperatureCount*/ \
    0, /*nanoVdbCoupleRateTemperatureVersion*/ \
    0, /*nanoVdbCoupleRateTemperatureFirstElement*/ \
    0, /*nanoVdbCoupleRateFuels*/ \
    0, /*nanoVdbCoupleRateFuelCount*/ \
    0, /*nanoVdbCoupleRateFuelVersion*/ \
    0, /*nanoVdbCoupleRateFuelFirstElement*/ \
    0, /*nanoVdbCoupleRateBurns*/ \
    0, /*nanoVdbCoupleRateBurnCount*/ \
    0, /*nanoVdbCoupleRateBurnVersion*/ \
    0, /*nanoVdbCoupleRateBurnFirstElement*/ \
    0, /*nanoVdbCoupleRateSmokes*/ \
    0, /*nanoVdbCoupleRateSmokeCount*/ \
    0, /*nanoVdbCoupleRateSmokeVersion*/ \
    0, /*nanoVdbCoupleRateSmokeFirstElement*/ \
    0, /*nanoVdbRgba8s*/ \
    0, /*nanoVdbRgba8Count*/ \
    0, /*nanoVdbRgba8Version*/ \
    0, /*nanoVdbRgba8FirstElement*/ \
    NV_FLOW_FALSE,    /*applyPostPressure*/ \
    NV_FLOW_FALSE, /*enableStreaming*/ \
    NV_FLOW_FALSE, /*streamOnce*/ \
    NV_FLOW_FALSE, /*streamClearAtStart*/ \
    1048576, /*streamingBatchSize*/ \
    NV_FLOW_FALSE, /*updateCoarseDensity*/ \
    NV_FLOW_TRUE,    /*allocateActiveLeaves*/ \
    0, /*nanoVdbDistancesInterop*/ \
    0, /*nanoVdbDistanceCountInterop*/ \
    0, /*nanoVdbVelocitiesInterop*/ \
    0, /*nanoVdbVelocityCountInterop*/ \
    0, /*nanoVdbDivergencesInterop*/ \
    0, /*nanoVdbDivergenceCountInterop*/ \
    0, /*nanoVdbTemperaturesInterop*/ \
    0, /*nanoVdbTemperatureCountInterop*/ \
    0, /*nanoVdbFuelsInterop*/ \
    0, /*nanoVdbFuelCountInterop*/ \
    0, /*nanoVdbBurnsInterop*/ \
    0, /*nanoVdbBurnCountInterop*/ \
    0, /*nanoVdbSmokesInterop*/ \
    0, /*nanoVdbSmokeCountInterop*/ \
}
static const NvFlowEmitterNanoVdbParams NvFlowEmitterNanoVdbParams_default = NvFlowEmitterNanoVdbParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterNanoVdbParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint64, volumeLuids, volumeLuidCount, volumeLuidVersion, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, followVisibility, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorld, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4x4, localToWorldVelocity, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityIsWorldSpace, 0, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelCount, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, colorIsSrgb, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, swapChannels, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, absoluteValue, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, computeSpeed, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergenceScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperatureScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateDivergence, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateFuel, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coupleRateSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, minDistance, 0, 0)
NV_FLOW_REFLECT_VALUE(float, maxDistance, 0, 0)
NV_FLOW_REFLECT_VALUE(float, minSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, maxSmoke, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbDistances, nanoVdbDistanceCount, nanoVdbDistanceVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDistanceFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbVelocities, nanoVdbVelocityCount, nanoVdbVelocityVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbVelocityFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbDivergences, nanoVdbDivergenceCount, nanoVdbDivergenceVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDivergenceFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbTemperatures, nanoVdbTemperatureCount, nanoVdbTemperatureVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbTemperatureFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbFuels, nanoVdbFuelCount, nanoVdbFuelVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbFuelFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbBurns, nanoVdbBurnCount, nanoVdbBurnVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbBurnFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbSmokes, nanoVdbSmokeCount, nanoVdbSmokeVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbSmokeFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateVelocities, nanoVdbCoupleRateVelocityCount, nanoVdbCoupleRateVelocityVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateVelocityFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateDivergences, nanoVdbCoupleRateDivergenceCount, nanoVdbCoupleRateDivergenceVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateDivergenceFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateTemperatures, nanoVdbCoupleRateTemperatureCount, nanoVdbCoupleRateTemperatureVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateTemperatureFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateFuels, nanoVdbCoupleRateFuelCount, nanoVdbCoupleRateFuelVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateFuelFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateBurns, nanoVdbCoupleRateBurnCount, nanoVdbCoupleRateBurnVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateBurnFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbCoupleRateSmokes, nanoVdbCoupleRateSmokeCount, nanoVdbCoupleRateSmokeVersion, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbCoupleRateSmokeFirstElement, 0, 0)
NV_FLOW_REFLECT_ARRAY_VERSIONED(NvFlowUint, nanoVdbRgba8s, nanoVdbRgba8Count, nanoVdbRgba8Version, eNvFlowReflectHint_asset, "asset(nanovdb)")
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbRgba8FirstElement, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPostPressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableStreaming, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamOnce, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, streamClearAtStart, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, streamingBatchSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, updateCoarseDensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, allocateActiveLeaves, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDistancesInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDistanceCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbVelocitiesInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbVelocityCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDivergencesInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbDivergenceCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbTemperaturesInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbTemperatureCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbFuelsInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbFuelCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbBurnsInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbBurnCountInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbSmokesInterop, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, nanoVdbSmokeCountInterop, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEmitterNanoVdbParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEmitterNanoVdbFeedback
{
    void* data;
}NvFlowEmitterNanoVdbFeedback;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowEmitterNanoVdbFeedback)

typedef struct NvFlowEmitterNanoVdbPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowEmitterNanoVdbParams*const* velocityParams;
    NvFlowUint64 velocityParamCount;
    const NvFlowEmitterNanoVdbParams*const* densityParams;
    NvFlowUint64 densityParamCount;
    NvFlowSparseTexture value;
    NvFlowSparseTexture voxelWeight;
    NvFlowSparseTexture coarseDensity;
    NvFlowBool32 isPostPressure;
    NvFlowEmitterNanoVdbFeedback feedback;
    NvFlowUint64 streamingVersion;
    NvFlowUint64 streamingFinishedVersion;
    NvFlowUint64 streamingPointsVersion;
    NvFlowUint64 streamingNanoVdbVersion;
}NvFlowEmitterNanoVdbPinsIn;

typedef struct NvFlowEmitterNanoVdbPinsOut
{
    NvFlowSparseTexture value;
    NvFlowSparseTexture voxelWeight;
    NvFlowUint64 streamingVersion;
    NvFlowUint64 streamingFinishedVersion;
    NvFlowUint64 streamingNanoVdbVersion;
}NvFlowEmitterNanoVdbPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterNanoVdbPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterNanoVdbParams, velocityParams, velocityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterNanoVdbParams, densityParams, densityParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensity, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPostPressure, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterNanoVdbFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingFinishedVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingPointsVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingNanoVdbVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterNanoVdbPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingFinishedVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, streamingNanoVdbVersion, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterNanoVdb)

/// ********************************* EmitterNanoVdbAllocate ***************************************

typedef struct NvFlowEmitterNanoVdbAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseSimParams sparseSimParams;
    float deltaTime;
    const NvFlowEmitterNanoVdbParams*const* params;
    NvFlowUint64 paramCount;
    const NvFlowVolumeParams* const* volumeParams;
    NvFlowUint64 volumeParamCount;
    const NvFlowNanoVdbAssetParams* const* nanoVdbAssetParams;
    NvFlowUint64 nanoVdbAssetParamCount;
}NvFlowEmitterNanoVdbAllocatePinsIn;

typedef struct NvFlowEmitterNanoVdbAllocatePinsOut
{
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
    NvFlowEmitterNanoVdbFeedback feedback;
}NvFlowEmitterNanoVdbAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterNanoVdbAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseSimParams, sparseSimParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEmitterNanoVdbParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowVolumeParams, volumeParams, volumeParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowNanoVdbAssetParams, nanoVdbAssetParams, nanoVdbAssetParamCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEmitterNanoVdbAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEmitterNanoVdbFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEmitterNanoVdbAllocate)

/// ********************************* Ellipsoid Raster ***************************************

typedef struct NvFlowEllipsoidRasterFeedback
{
    void* data;
}NvFlowEllipsoidRasterFeedback;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowEllipsoidRasterFeedback)

typedef struct NvFlowEllipsoidRasterParams
{
    float scale;
    float density;
    NvFlowBool32 sdfMode;
    const NvFlowFloat4* positions;
    NvFlowUint64 positionCount;
    const NvFlowFloat3* positionFloat3s;
    NvFlowUint64 positionFloat3Count;
    const NvFlowFloat4* anisotropyE1s;
    NvFlowUint64 anisotropyE1Count;
    const NvFlowFloat4* anisotropyE2s;
    NvFlowUint64 anisotropyE2Count;
    const NvFlowFloat4* anisotropyE3s;
    NvFlowUint64 anisotropyE3Count;
    NvFlowUint smoothIterations;
    float allocationScale;
    float allocationOffset;
}NvFlowEllipsoidRasterParams;

#define NvFlowEllipsoidRasterParams_default_init { \
    1.f,    /*scale*/ \
    1.f,    /*density*/ \
    NV_FLOW_TRUE, /*sdfMode*/ \
    0,        /*positions*/ \
    0u,        /*positionsCount*/ \
    0,        /*positionFloat3s*/ \
    0u,        /*positionsFloat3Count*/ \
    0,        /*anisotropyE1s*/ \
    0u,        /*anisotropyE1Count*/ \
    0,        /*anisotropyE2s*/ \
    0u,        /*anisotropyE2Count*/ \
    0,        /*anisotropyE3s*/ \
    0u,        /*anisotropyE3Count*/ \
    3u,        /*smoothIterations*/ \
    1.f,    /*allocationScale*/ \
    0.f        /*allocationOffset*/ \
}
static const NvFlowEllipsoidRasterParams NvFlowEllipsoidRasterParams_default = NvFlowEllipsoidRasterParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowEllipsoidRasterParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(float, scale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, density, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, sdfMode, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, positions, positionCount, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY(NvFlowFloat3, positionFloat3s, positionFloat3Count, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, anisotropyE1s, anisotropyE1Count, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, anisotropyE2s, anisotropyE2Count, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, anisotropyE3s, anisotropyE3Count, eNvFlowReflectHint_asset, "asset(array)")
NV_FLOW_REFLECT_VALUE(NvFlowUint, smoothIterations, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, allocationOffset, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowEllipsoidRasterParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowEllipsoidRasterPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowEllipsoidRasterFeedback feedback;
    const NvFlowEllipsoidRasterParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture layout;
}NvFlowEllipsoidRasterPinsIn;

typedef struct NvFlowEllipsoidRasterPinsOut
{
    NvFlowSparseTexture value;
}NvFlowEllipsoidRasterPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEllipsoidRasterPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEllipsoidRasterFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEllipsoidRasterParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, layout, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEllipsoidRasterPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, value, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEllipsoidRaster)

typedef struct NvFlowEllipsoidRasterAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseParams sparseParams;
    const NvFlowEllipsoidRasterParams** params;
    NvFlowUint64 paramCount;
}NvFlowEllipsoidRasterAllocatePinsIn;

typedef struct NvFlowEllipsoidRasterAllocatePinsOut
{
    NvFlowEllipsoidRasterFeedback feedback;
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
}NvFlowEllipsoidRasterAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowEllipsoidRasterAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowEllipsoidRasterParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowEllipsoidRasterAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowEllipsoidRasterFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowEllipsoidRasterAllocate)

/// ********************************* Shadow ***************************************

typedef struct NvFlowShadowParams
{
    NvFlowBool32 enabled;
    NvFlowFloat3 lightDirection;
    NvFlowFloat3 lightPosition;
    NvFlowBool32 isPointLight;
    float attenuation;
    float stepSizeScale;
    float stepOffsetScale;
    float minIntensity;
    NvFlowUint numSteps;
    NvFlowBool32 coarsePropagate;
    NvFlowBool32 enableRawMode;
    NvFlowBool32 rawModeIsosurface;
    NvFlowBool32 rawModeNormalize;
    float colormapXMin;
    float colormapXMax;
}NvFlowShadowParams;

#define NvFlowShadowParams_default_init { \
    NV_FLOW_TRUE,        /*enabled*/ \
    {1.f, 1.f, 1.f},    /*lightDirection*/ \
    {0.f, 0.f, 0.f},    /*lightPosition*/ \
    NV_FLOW_FALSE,        /*isPointLight*/ \
    0.045f,                /*attenuation*/ \
    0.75f,                /*stepSizeScale*/ \
    1.f,                /*stepOffsetScale*/ \
    0.125f,                /*minIntensity*/ \
    16u,                /*numSteps*/ \
    NV_FLOW_TRUE,        /*coarsePropagate*/ \
    NV_FLOW_FALSE,        /*enableRawMode*/ \
    NV_FLOW_TRUE,       /*rawModeIsosurface*/ \
    NV_FLOW_TRUE,       /*rawModeNormalize*/ \
    0.f,                /*colormapXMin*/ \
    1.f,                /*colormapXMax*/ \
}
static const NvFlowShadowParams NvFlowShadowParams_default = NvFlowShadowParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowShadowParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, lightDirection, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, lightPosition, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPointLight, 0, 0)
NV_FLOW_REFLECT_VALUE(float, attenuation, 0, 0)
NV_FLOW_REFLECT_VALUE(float, stepSizeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, stepOffsetScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, minIntensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, coarsePropagate, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableRawMode, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rawModeIsosurface, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rawModeNormalize, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colormapXMin, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colormapXMax, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowShadowParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowShadowPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowShadowParams** params;
    NvFlowUint64 paramCount;
    NvFlowTextureTransient* colormap;
    NvFlowSparseTexture density;
    NvFlowSparseTexture coarseDensity;
}NvFlowShadowPinsIn;

typedef struct NvFlowShadowPinsOut
{
    NvFlowSparseTexture densityShadow;
}NvFlowShadowPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowShadowPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowShadowParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, colormap, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowShadowPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowShadow)

/// ********************************* DebugVolume ***************************************

typedef struct NvFlowDebugVolumeParams
{
    NvFlowBool32 enabled;
    NvFlowBool32 enableSpeedAsTemperature;
    NvFlowBool32 enableVelocityAsDensity;
    NvFlowBool32 enableDivergenceAsSmoke;
    NvFlowBool32 applyPreShadow;
    NvFlowBool32 outputScaleBySmoke;
    NvFlowFloat3 velocityScale;
    float outputTemperatureScale;
    float outputFuelScale;
    float outputBurnScale;
    float outputSmokeScale;
    float outputTemperatureOffset;
    float outputFuelOffset;
    float outputBurnOffset;
    float outputSmokeOffset;
    NvFlowFloat4 slicePlane;
    float slicePlaneThickness;
}NvFlowDebugVolumeParams;

#define NvFlowDebugVolumeParams_default_init { \
    NV_FLOW_TRUE,           /*enabled*/ \
    NV_FLOW_FALSE,            /*enableSpeedAsTemperature*/ \
    NV_FLOW_FALSE,            /*enableVelocityAsDensity*/ \
    NV_FLOW_FALSE,            /*enableDivergenceAsSmoke*/ \
    NV_FLOW_FALSE,            /*applyPreShadow*/ \
    NV_FLOW_FALSE,            /*outputScaleBySmoke*/ \
    {0.01f, 0.01f, 0.01f},    /*velocityScale*/ \
    1.f,                    /*outputTemperatureScale*/ \
    1.f,                    /*outputFuelScale*/ \
    1.f,                    /*outputBurnScale*/ \
    1.f,                    /*outputSmokeScale*/ \
    0.f,                    /*outputTemperatureOffset*/ \
    0.f,                    /*outputFuelOffset*/ \
    0.f,                    /*outputBurnOffset*/ \
    0.f,                    /*outputSmokeOffset*/ \
    {1.f, 0.f, 0.f, 0.f},   /*slicePlane*/ \
    0.f,                    /*slicePlaneThickness*/ \
}
static const NvFlowDebugVolumeParams NvFlowDebugVolumeParams_default = NvFlowDebugVolumeParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowDebugVolumeParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableSpeedAsTemperature, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableVelocityAsDensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableDivergenceAsSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, applyPreShadow, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, outputScaleBySmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, velocityScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputTemperatureScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputFuelScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputBurnScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputSmokeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputTemperatureOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputFuelOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputBurnOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(float, outputSmokeOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat4, slicePlane, 0, 0)
NV_FLOW_REFLECT_VALUE(float, slicePlaneThickness, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowDebugVolumeParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowDebugVolumePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowDebugVolumeParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture densityShadow;
    NvFlowSparseTexture coarseDensityShadow;
    NvFlowBool32 isPreShadow;
}NvFlowDebugVolumePinsIn;

typedef struct NvFlowDebugVolumePinsOut
{
    NvFlowSparseTexture densityShadow;
    NvFlowSparseTexture coarseDensityShadow;
}NvFlowDebugVolumePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowDebugVolumePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowDebugVolumeParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensityShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, isPreShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowDebugVolumePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensityShadow, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowDebugVolume)

/// ********************************* RayMarch ***************************************

typedef struct NvFlowRayMarchCloudParams
{
    NvFlowBool32 enableCloudMode;
    NvFlowFloat3 sunDirection;
    NvFlowFloat3 ambientColor;
    float ambientMultiplier;
    float densityMultiplier;
    NvFlowFloat3 volumeBaseColor;
    float volumeColorMultiplier;
    float shadowStepMultiplier;
    int numShadowSteps;
    NvFlowFloat3 attenuationMultiplier;
}NvFlowRayMarchCloudParams;

#define NvFlowRayMarchCloudParams_default_init { \
    NV_FLOW_FALSE,    /*enableCloudMode*/ \
    {1.f, 1.f, 1.f}, /*sunDirection*/ \
    {0.4f, 0.55f, 0.9f}, /*ambientColor*/ \
    1.0f, /*ambientMultiplier*/ \
    0.5f, /*densityMultiplier*/ \
    {1.1f, 1.f, 0.95f}, /*volumeBaseColor*/ \
    1.0f, /*volumeColorMultiplier*/ \
    1.0f, /*shadowStepMultiplier*/ \
    10u, /*numShadowSteps*/ \
    {1.f, 1.f, 1.f} /*attenuationMultiplier*/ \
}
static const NvFlowRayMarchCloudParams NvFlowRayMarchCloudParams_default = NvFlowRayMarchCloudParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchCloudParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableCloudMode, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, sunDirection, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, ambientColor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, ambientMultiplier, 0, 0)
NV_FLOW_REFLECT_VALUE(float, densityMultiplier, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, volumeBaseColor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, volumeColorMultiplier, 0, 0)
NV_FLOW_REFLECT_VALUE(float, shadowStepMultiplier, 0, 0)
NV_FLOW_REFLECT_VALUE(int, numShadowSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, attenuationMultiplier, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowRayMarchCloudParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowRayMarchParams
{
    NvFlowBool32 enableBlockWireframe;
    NvFlowBool32 enableRawMode;
    NvFlowBool32 rawModeIsosurface;
    NvFlowBool32 rawModeNormalize;
    float colorScale;
    float attenuation;
    float stepSizeScale;
    float shadowFactor;
    float colormapXMin;
    float colormapXMax;
    NvFlowRayMarchCloudParams cloud;
}NvFlowRayMarchParams;

#define NvFlowRayMarchParams_default_init { \
    NV_FLOW_FALSE,    /*enableBlockWireframe*/ \
    NV_FLOW_FALSE,    /*enableRawMode*/ \
    NV_FLOW_TRUE,   /*rawModeIsosurface*/ \
    NV_FLOW_TRUE,   /*rawModeNormalize*/ \
    1.f,            /*colorScale*/ \
    0.05f,            /*attenuation*/ \
    0.75f,            /*stepSizeScale*/ \
    1.f,            /*shadowFactor*/ \
    0.f,            /*colormapXMin*/ \
    1.f,            /*colormapXMax*/ \
    NvFlowRayMarchCloudParams_default_init /*cloud*/ \
}
static const NvFlowRayMarchParams NvFlowRayMarchParams_default = NvFlowRayMarchParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableBlockWireframe, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableRawMode, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rawModeIsosurface, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rawModeNormalize, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colorScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, attenuation, 0, 0)
NV_FLOW_REFLECT_VALUE(float, stepSizeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, shadowFactor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colormapXMin, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colormapXMax, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowRayMarchCloudParams, cloud, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowRayMarchParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowRayMarchIsosurfaceParams
{
    NvFlowBool32 enableBlockWireframe;
    float stepSizeScale;
    float densityThreshold;
    NvFlowBool32 refractionMode;
    NvFlowBool32 visualizeNormals;
    float fluidIoR;
    NvFlowFloat3 fluidColor;
    float fluidAbsorptionCoefficient;
    NvFlowFloat3 fluidSpecularReflectance;
    NvFlowFloat3 fluidDiffuseReflectance;
    NvFlowFloat3 fluidRadiance;
}NvFlowRayMarchIsosurfaceParams;

#define NvFlowRayMarchIsosurfaceParams_default_init { \
    NV_FLOW_FALSE,    /*enableBlockWireframe*/ \
    0.75f,            /*stepSizeScale*/ \
    0.5f,            /*densityThreshold*/ \
    NV_FLOW_FALSE,    /*refractionMode*/ \
    NV_FLOW_FALSE,    /*visualizeNormals*/ \
    1.333f,                /*fluidIoR*/ \
    {0.9f, 0.9f, 1.f},    /*fluidColor*/ \
    0.0035f,            /*fluidAbsorptionCoefficient*/ \
    {0.1f, 0.1f, 0.1f}, /*fluidSpecularReflectance*/ \
    {0.1f, 0.1f, 0.1f}, /*fluidDiffuseReflectance*/ \
    {0.f, 0.f, 0.f}        /*fluidRadiance*/ \
}
static const NvFlowRayMarchIsosurfaceParams NvFlowRayMarchIsosurfaceParams_default = NvFlowRayMarchIsosurfaceParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchIsosurfaceParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableBlockWireframe, 0, 0)
NV_FLOW_REFLECT_VALUE(float, stepSizeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, densityThreshold, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, refractionMode, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, visualizeNormals, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fluidIoR, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, fluidColor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fluidAbsorptionCoefficient, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, fluidSpecularReflectance, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, fluidDiffuseReflectance, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, fluidRadiance, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowRayMarchIsosurfaceParams_default)
#undef NV_FLOW_REFLECT_TYPE

#define NvFlowRayMarchColormapParams_default_pointCount_init 6u
static const NvFlowUint64 NvFlowRayMarchColormapParams_default_pointCount = NvFlowRayMarchColormapParams_default_pointCount_init;
#define NvFlowRayMarchColormapParams_default_xPoints_init { \
    0.000000f, \
    0.050000f, \
    0.15000f, \
    0.600000f, \
    0.850000f, \
    1.000000f \
}
static const float NvFlowRayMarchColormapParams_default_xPoints[NvFlowRayMarchColormapParams_default_pointCount_init] = NvFlowRayMarchColormapParams_default_xPoints_init;

#define NvFlowRayMarchColormapParams_default_colorScalePoints_init { \
    1.000000f, \
    1.000000f, \
    1.000000f, \
    1.000000f, \
    1.000000f, \
    1.000000f \
}
static const float NvFlowRayMarchColormapParams_default_colorScalePoints[NvFlowRayMarchColormapParams_default_pointCount_init] = NvFlowRayMarchColormapParams_default_colorScalePoints_init;

#define NvFlowRayMarchColormapParams_default_rgbaPoints_smoke_init { \
    { 0.9f,    0.9f, 0.9f,    0.004902f }, \
    { 0.9f,    0.9f, 0.9f,    0.904902f }, \
    { 0.9f,    0.9f, 0.9f,    0.904902f }, \
    { 0.9f,    0.9f, 0.9f,    0.904902f }, \
    { 0.9f,    0.9f, 0.9f,    0.904902f }, \
    { 0.9f,    0.9f, 0.9f,    0.904902f }, \
}
static const NvFlowFloat4 NvFlowRayMarchColormapParams_default_rgbaPoints_smoke[NvFlowRayMarchColormapParams_default_pointCount_init] = NvFlowRayMarchColormapParams_default_rgbaPoints_smoke_init;

#define NvFlowRayMarchColormapParams_default_rgbaPoints_fire_init { \
    { 0.015400f,    0.017700f,    0.015400f,    0.004902f }, \
    { 0.035750f,    0.035750f,    0.035750f,    0.504902f }, \
    { 0.035750f,    0.035750f,    0.035750f,    0.504902f }, \
    { 1.f,            0.1594125f, 0.0135315f, 0.800000f }, \
    { 13.534992f,    2.986956f,  0.125991f,  0.800000f }, \
    { 78.08f,       39.04f,     6.1f,       0.700000f }, \
}
static const NvFlowFloat4 NvFlowRayMarchColormapParams_default_rgbaPoints_fire[NvFlowRayMarchColormapParams_default_pointCount_init] = NvFlowRayMarchColormapParams_default_rgbaPoints_fire_init;

typedef struct NvFlowRayMarchColormapParams
{
    NvFlowUint resolution;
    const float* xPoints;
    NvFlowUint64 xPointCount;
    const NvFlowFloat4* rgbaPoints;
    NvFlowUint64 rgbaPointCount;
    const float* colorScalePoints;
    NvFlowUint64 colorScalePointCount;
    float colorScale;
}NvFlowRayMarchColormapParams;

#define NvFlowRayMarchColormapParams_default_init { \
    32u,    /*resolution*/ \
    NvFlowRayMarchColormapParams_default_xPoints,        /*xPoints*/ \
    NvFlowRayMarchColormapParams_default_pointCount_init,        /*xPointCount*/ \
    NvFlowRayMarchColormapParams_default_rgbaPoints_smoke,        /*rgbaPoints*/ \
    NvFlowRayMarchColormapParams_default_pointCount_init,        /*rgbaPointCount*/ \
    NvFlowRayMarchColormapParams_default_colorScalePoints,        /*colorScalePoints*/ \
    NvFlowRayMarchColormapParams_default_pointCount_init,        /*colorScaleCount*/ \
    2.5f /*colorScale*/ \
}
static const NvFlowRayMarchColormapParams NvFlowRayMarchColormapParams_default = NvFlowRayMarchColormapParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchColormapParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, resolution, 0, 0)
NV_FLOW_REFLECT_ARRAY(float, xPoints, xPointCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowFloat4, rgbaPoints, rgbaPointCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(float, colorScalePoints, colorScalePointCount, 0, 0)
NV_FLOW_REFLECT_VALUE(float, colorScale, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowRayMarchColormapParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowRayMarchTargetTexture
{
    const NvFlowFloat4x4* view;
    const NvFlowFloat4x4* projection;
    const NvFlowFloat4x4* projectionJittered;
    NvFlowUint textureWidth;
    NvFlowUint textureHeight;
    NvFlowUint sceneDepthWidth;
    NvFlowUint sceneDepthHeight;
    NvFlowTextureTransient* sceneDepthIn;
    NvFlowFormat sceneColorFormat;
    NvFlowTextureTransient* sceneColorIn;
    NvFlowTextureTransient** pSceneColorOut;
}NvFlowRayMarchTargetTexture;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowRayMarchTargetTexture)

typedef struct NvFlowRayMarchPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowRayMarchParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture density;
    NvFlowTextureTransient* colormap;
    const NvFlowRayMarchTargetTexture* target;
    float compositeColorScale;
}NvFlowRayMarchPinsIn;

typedef struct NvFlowRayMarchPinsOut
{
    NvFlowUint unused;
}NvFlowRayMarchPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowRayMarchParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, colormap, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowRayMarchTargetTexture, target, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, compositeColorScale, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, unused, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowRayMarch)

typedef struct NvFlowRayMarchUpdateColormapPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowRayMarchColormapParams** params;
    NvFlowUint64 paramCount;
}NvFlowRayMarchUpdateColormapPinsIn;

typedef struct NvFlowRayMarchUpdateColormapPinsOut
{
    NvFlowTextureTransient* colormap;
}NvFlowRayMarchUpdateColormapPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchUpdateColormapPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowRayMarchColormapParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchUpdateColormapPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, colormap, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowRayMarchUpdateColormap)

typedef struct NvFlowRayMarchIsosurfacePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowRayMarchIsosurfaceParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture density;
    const NvFlowRayMarchTargetTexture* target;
    float compositeColorScale;
}NvFlowRayMarchIsosurfacePinsIn;

typedef struct NvFlowRayMarchIsosurfacePinsOut
{
    NvFlowUint unused;
}NvFlowRayMarchIsosurfacePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchIsosurfacePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowRayMarchIsosurfaceParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowRayMarchTargetTexture, target, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(float, compositeColorScale, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchIsosurfacePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, unused, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowRayMarchIsosurface)

typedef struct NvFlowRayMarchCopyTexturePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowTextureTransient* texture;
    NvFlowUint width;
    NvFlowUint height;
    NvFlowFormat format;
}NvFlowRayMarchCopyTexturePinsIn;

typedef struct NvFlowRayMarchCopyTexturePinsOut
{
    NvFlowTextureTransient* texture;
}NvFlowRayMarchCopyTexturePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchCopyTexturePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, texture, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, width, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, height, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ENUM(format, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowRayMarchCopyTexturePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, texture, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowRayMarchCopyTexture)

/// ********************************* NvFlowExtOpList ***************************************

typedef struct NvFlowExtOpList
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterSphere)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterSphereAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterBox)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterBoxAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterPoint)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterPointAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterMesh)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterMeshAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterTexture)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterTextureAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterNanoVdb)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEmitterNanoVdbAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEllipsoidRaster)();

    NvFlowOpInterface* (NV_FLOW_ABI* pEllipsoidRasterAllocate)();

    NvFlowOpInterface* (NV_FLOW_ABI* pShadow)();

    NvFlowOpInterface* (NV_FLOW_ABI* pDebugVolume)();

    NvFlowOpInterface* (NV_FLOW_ABI* pRayMarch)();

    NvFlowOpInterface* (NV_FLOW_ABI* pRayMarchUpdateColormap)();

    NvFlowOpInterface* (NV_FLOW_ABI* pRayMarchIsosurface)();

    NvFlowOpInterface* (NV_FLOW_ABI* pRayMarchCopyTexture)();
}NvFlowExtOpList;

#define NV_FLOW_REFLECT_TYPE NvFlowExtOpList
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterSphere, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterSphereAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterBox, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterBoxAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterPoint, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterPointAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterTextureAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterNanoVdb, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEmitterNanoVdbAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEllipsoidRaster, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pEllipsoidRasterAllocate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pShadow, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pDebugVolume, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pRayMarch, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pRayMarchUpdateColormap, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pRayMarchIsosurface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pRayMarchCopyTexture, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowExtOpList* (NV_FLOW_ABI* PFN_NvFlowGetExtOpList)();

NV_FLOW_API NvFlowExtOpList* NvFlowGetExtOpList();

/// ********************************* Grid ***************************************

struct NvFlowGrid;
typedef struct NvFlowGrid NvFlowGrid;

typedef struct NvFlowGridDesc
{
    NvFlowUint maxLocations;
    NvFlowUint maxLocationsIsosurface;
}NvFlowGridDesc;

#define NvFlowGridDesc_default_init { \
    4096u,    /*maxLocations*/ \
    4096u    /*maxLocationsIsosurface*/ \
}
static const NvFlowGridDesc NvFlowGridDesc_default = NvFlowGridDesc_default_init;

NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterSphereParams, NvFlowGridEmitterSphereParams)
NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterBoxParams, NvFlowGridEmitterBoxParams)
NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterPointParams, NvFlowGridEmitterPointParams)
NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterMeshParams, NvFlowGridEmitterMeshParams)
NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterTextureParams, NvFlowGridEmitterTextureParams)
NV_FLOW_REFLECT_TYPE_ALIAS(NvFlowEmitterNanoVdbParams, NvFlowGridEmitterNanoVdbParams)

typedef struct NvFlowGridSimulateLayerParams
{
    NvFlowUint64 luid;
    int layer;
    int level;
    int levelCount;
    float densityCellSize;
    float levelCellSizeMultiplier;
    NvFlowBool32 autoCellSize;
    NvFlowBool32 enableSmallBlocks;
    NvFlowBool32 enableLowPrecisionVelocity;
    NvFlowBool32 enableLowPrecisionDensity;
    NvFlowBool32 enableLowPrecisionRescale;
    NvFlowBool32 enableHighPrecisionVelocity;
    NvFlowBool32 enableHighPrecisionDensity;
    NvFlowBool32 forceClear;
    NvFlowBool32 forceDisableEmitters;
    NvFlowBool32 forceDisableCoreSimulation;
    NvFlowBool32 clearOnRescale;
    NvFlowBool32 simulateWhenPaused;
    NvFlowUint blockMinLifetime;
    float stepsPerSecond;
    float timeScale;
    NvFlowUint maxStepsPerSimulate;
    NvFlowBool32 physicsCollisionEnabled;
    NvFlowBool32 physicsConvexCollision;
    NvFlowBool32 enableVariableTimeStep;
    NvFlowBool32 interpolateTimeSteps;
    NvFlowUint velocitySubSteps;
    NvFlowAdvectionCombustionParams advection;
    NvFlowVorticityParams vorticity;
    NvFlowPressureParams pressure;
    NvFlowSummaryAllocateParams summaryAllocate;
    NvFlowSparseNanoVdbExportParams nanoVdbExport;
}NvFlowGridSimulateLayerParams;

#define NvFlowGridSimulateLayerParams_default_init { \
    0llu,            /*luid*/ \
    0,                /*layer*/ \
    0,                /*level*/ \
    1,                /*levelCount*/ \
    0.5f,            /*densityCellSize*/ \
    0.5f,            /*levelCellSizeMultiplier*/ \
    NV_FLOW_FALSE,    /*autoCellSize*/ \
    NV_FLOW_FALSE,    /*enableSmallBlocks*/ \
    NV_FLOW_FALSE,    /*enableLowPrecisionVelocity*/ \
    NV_FLOW_FALSE,    /*enableLowPrecisionDensity*/ \
    NV_FLOW_FALSE,    /*enableLowPrecisionRescale*/ \
    NV_FLOW_FALSE,    /*enableHighPrecisionVelocity*/ \
    NV_FLOW_FALSE,    /*enableHighPrecisionDensity*/ \
    NV_FLOW_FALSE,    /*forceClear*/ \
    NV_FLOW_FALSE,    /*forceDisableEmitters*/ \
    NV_FLOW_FALSE,    /*forceDisableCoreSimulation*/ \
    NV_FLOW_FALSE,  /*clearOnRescale*/ \
    NV_FLOW_FALSE,    /*simulateWhenPaused*/ \
    4u,                /*blockMinLifetime*/ \
    60.f,            /*stepsPerSecond*/ \
    1.f,            /*timeScale*/ \
    1u,                /*maxStepsPerSimulate*/ \
    NV_FLOW_FALSE,  /*physicsCollisionEnabled*/ \
    NV_FLOW_TRUE,   /*physicsConvexCollision*/ \
    NV_FLOW_FALSE,    /*enableVariableTimeStep*/ \
    NV_FLOW_FALSE,    /*interpolateTimeSteps*/ \
    1u,                /*velocitySubSteps*/ \
    NvFlowAdvectionCombustionParams_default_init, /*advection*/ \
    NvFlowVorticityParams_default_init, /*vorticity*/ \
    NvFlowPressureParams_default_init,    /*pressure*/ \
    NvFlowSummaryAllocateParams_default_init,    /*summaryAllocate*/ \
    NvFlowSparseNanoVdbExportParams_default_init /*nanoVdbExport*/ \
}
static const NvFlowGridSimulateLayerParams NvFlowGridSimulateLayerParams_default = NvFlowGridSimulateLayerParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowGridSimulateLayerParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(int, levelCount, 0, 0)
NV_FLOW_REFLECT_VALUE(float, densityCellSize, 0, 0)
NV_FLOW_REFLECT_VALUE(float, levelCellSizeMultiplier, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, autoCellSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableSmallBlocks, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableLowPrecisionVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableLowPrecisionDensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableLowPrecisionRescale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableHighPrecisionVelocity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableHighPrecisionDensity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, forceClear, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, forceDisableEmitters, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, forceDisableCoreSimulation, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, clearOnRescale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, simulateWhenPaused, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, blockMinLifetime, 0, 0)
NV_FLOW_REFLECT_VALUE(float, stepsPerSecond, 0, 0)
NV_FLOW_REFLECT_VALUE(float, timeScale, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, maxStepsPerSimulate, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, physicsCollisionEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, physicsConvexCollision, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableVariableTimeStep, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, interpolateTimeSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, velocitySubSteps, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionCombustionParams, advection, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowVorticityParams, vorticity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowPressureParams, pressure, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSummaryAllocateParams, summaryAllocate, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseNanoVdbExportParams, nanoVdbExport, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowGridSimulateLayerParams_default)
#undef NV_FLOW_REFLECT_TYPE


typedef struct NvFlowGridOffscreenLayerParams
{
    NvFlowUint64 luid;
    int layer;
    int level;
    NvFlowShadowParams shadow;
    NvFlowRayMarchColormapParams colormap;
    NvFlowDebugVolumeParams debugVolume;
}NvFlowGridOffscreenLayerParams;

#define NvFlowGridOffscreenLayerParams_default_init { \
    0llu,    /*luid*/ \
    0,    /*layer*/ \
    0,    /*level*/ \
    NvFlowShadowParams_default_init,    /*shadow*/ \
    NvFlowRayMarchColormapParams_default_init, /*colormap*/ \
    NvFlowDebugVolumeParams_default_init /*debugVolume*/ \
}
static const NvFlowGridOffscreenLayerParams NvFlowGridOffscreenLayerParams_default = NvFlowGridOffscreenLayerParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowGridOffscreenLayerParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowShadowParams, shadow, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowRayMarchColormapParams, colormap, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowDebugVolumeParams, debugVolume, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowGridOffscreenLayerParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowRenderSettingsParams
{
    NvFlowBool32 enableAutoApply;
    NvFlowBool32 flowEnabled;
    int maxBlocks;
    NvFlowBool32 rayTracedShadowsEnabled;
    NvFlowBool32 rayTracedReflectionsEnabled;
    NvFlowBool32 rayTracedTranslucencyEnabled;
    NvFlowBool32 pathTracingEnabled;
    NvFlowBool32 pathTracingShadowsEnabled;
    NvFlowBool32 compositeEnabled;
}NvFlowRenderSettingsParams;

#define NvFlowRenderSettingsParams_default_init { \
    NV_FLOW_TRUE,  /*enableAutoApply*/ \
    NV_FLOW_TRUE,  /*flowEnabled*/ \
    0,             /*maxBlocks*/ \
    NV_FLOW_FALSE, /*rayTracedShadowsEnabled*/ \
    NV_FLOW_TRUE,  /*rayTracedReflectionsEnabled*/ \
    NV_FLOW_TRUE,  /*rayTracedTranslucencyEnabled*/ \
    NV_FLOW_TRUE,  /*pathTracingEnabled*/ \
    NV_FLOW_FALSE, /*pathTracingShadowsEnabled*/ \
    NV_FLOW_TRUE,  /*compositeEnabled*/ \
}
static const NvFlowRenderSettingsParams NvFlowRenderSettingsParams_default = NvFlowRenderSettingsParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowRenderSettingsParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableAutoApply, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, flowEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(int, maxBlocks, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rayTracedShadowsEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rayTracedReflectionsEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rayTracedTranslucencyEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, pathTracingEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, pathTracingShadowsEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, compositeEnabled, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowRenderSettingsParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowGridRenderLayerParams
{
    NvFlowUint64 luid;
    int layer;
    NvFlowRayMarchParams rayMarch;
    int level;
    NvFlowRenderSettingsParams renderSettings;
}NvFlowGridRenderLayerParams;

#define NvFlowGridRenderLayerParams_default_init { \
    0llu, /*luid*/ \
    0, /*layer*/ \
    NvFlowRayMarchParams_default_init, /*rayMarch*/ \
    0, /*level*/ \
    NvFlowRenderSettingsParams_default_init, /*renderSettings*/ \
}
static const NvFlowGridRenderLayerParams NvFlowGridRenderLayerParams_default = NvFlowGridRenderLayerParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowGridRenderLayerParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowRayMarchParams, rayMarch, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowRenderSettingsParams, renderSettings, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowGridRenderLayerParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowGridIsosurfaceLayerParams
{
    NvFlowUint64 luid;
    int layer;
    int level;
    float densityCellSize;
    NvFlowEllipsoidRasterParams ellipsoidRaster;
    NvFlowRayMarchIsosurfaceParams rayMarchIsosurface;
}NvFlowGridIsosurfaceLayerParams;

#define NvFlowGridIsosurfaceLayerParams_default_init { \
    0llu,    /*luid*/ \
    0,        /*layer*/ \
    0,        /*level*/ \
    2.f,    /*densityCellSize*/ \
    NvFlowEllipsoidRasterParams_default_init,    /*ellipsoidRaster*/ \
    NvFlowRayMarchIsosurfaceParams_default_init    /*rayMarchIsosurface*/ \
}
static const NvFlowGridIsosurfaceLayerParams NvFlowGridIsosurfaceLayerParams_default = NvFlowGridIsosurfaceLayerParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowGridIsosurfaceLayerParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, luid, eNvFlowReflectHint_transientNoEdit, 0)
NV_FLOW_REFLECT_VALUE(int, layer, 0, 0)
NV_FLOW_REFLECT_VALUE(int, level, 0, 0)
NV_FLOW_REFLECT_VALUE(float, densityCellSize, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowEllipsoidRasterParams, ellipsoidRaster, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowRayMarchIsosurfaceParams, rayMarchIsosurface, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowGridIsosurfaceLayerParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowGridReadbackParams
{
    const char* gridParamsName;
    NvFlowBool32 enableGridRenderDataNanoVdb;
    NvFlowBool32 onlyLatest;
    NvFlowUint readbackRingBufferCount;
    NvFlowUint interopRingBufferCount;
}NvFlowGridReadbackParams;

#define NvFlowGridReadbackParams_default_init { \
    "flowUsdReadback",        /*gridParamsName*/ \
    NV_FLOW_FALSE, /*gridRenderDataNanoVdbNull*/ \
    NV_FLOW_FALSE, /*onlyLatest*/ \
    0u, /*readbackRingBufferCount*/ \
    0u, /*interopRingBufferCount*/ \
}
static const NvFlowGridReadbackParams NvFlowGridReadbackParams_default = NvFlowGridReadbackParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowGridReadbackParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(char, gridParamsName, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableGridRenderDataNanoVdb, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, onlyLatest, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, readbackRingBufferCount, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, interopRingBufferCount, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowGridReadbackParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowGridReadbackData
{
    NvFlowUint numLocations;
    NvFlowUint maxLocations;
    NvFlowUint64 lastGlobalFrameCompleted;
    double lastAbsoluteSimTimeCompleted;
}NvFlowGridReadbackData;

#define NV_FLOW_REFLECT_TYPE NvFlowGridReadbackData
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, numLocations, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, maxLocations, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, lastGlobalFrameCompleted, 0, 0)
NV_FLOW_REFLECT_VALUE(double, lastAbsoluteSimTimeCompleted, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowGridParamsDescSnapshot
{
    NvFlowDatabaseSnapshot snapshot;
    double absoluteSimTime;
    float deltaTime;
    NvFlowBool32 globalForceClear;
    const NvFlowUint8* userdata;
    NvFlowUint64 userdataSizeInBytes;
}NvFlowGridParamsDescSnapshot;

typedef struct NvFlowGridParamsDesc
{
    NvFlowGridParamsDescSnapshot* snapshots;
    NvFlowUint64 snapshotCount;
}NvFlowGridParamsDesc;

// This is based on a legacy version of NvFlowSparseNanoVdbExportReadback
typedef struct NvFlowGridRenderDataNanoVdbReadback
{
    NvFlowUint64 globalFrameCompleted;
    NvFlowUint8* temperatureNanoVdbReadback;
    NvFlowUint64 temperatureNanoVdbReadbackSize;
    NvFlowUint8* fuelNanoVdbReadback;
    NvFlowUint64 fuelNanoVdbReadbackSize;
    NvFlowUint8* burnNanoVdbReadback;
    NvFlowUint64 burnNanoVdbReadbackSize;
    NvFlowUint8* smokeNanoVdbReadback;
    NvFlowUint64 smokeNanoVdbReadbackSize;
    NvFlowUint8* velocityNanoVdbReadback;
    NvFlowUint64 velocityNanoVdbReadbackSize;
    NvFlowUint8* divergenceNanoVdbReadback;
    NvFlowUint64 divergenceNanoVdbReadbackSize;
}NvFlowGridRenderDataNanoVdbReadback;

// This is based on a legacy version of NvFlowGridRenderDataNanoVdb
typedef struct NvFlowGridRenderDataNanoVdb
{
    NvFlowBufferTransient* temperatureNanoVdb;
    NvFlowBufferTransient* fuelNanoVdb;
    NvFlowBufferTransient* burnNanoVdb;
    NvFlowBufferTransient* smokeNanoVdb;
    NvFlowBufferTransient* velocityNanoVdb;
    NvFlowBufferTransient* divergenceNanoVdb;

    NvFlowGridRenderDataNanoVdbReadback* readbacks;
    NvFlowUint64 readbackCount;
}NvFlowGridRenderDataNanoVdb;

typedef struct NvFlowGridRenderData
{
    NvFlowBufferTransient* sparseBuffer;
    NvFlowTextureTransient* densityTexture;
    NvFlowTextureTransient* velocityTexture;
    NvFlowTextureTransient* colormap;
    NvFlowSparseParams sparseParams;
    NvFlowGridRenderDataNanoVdb nanoVdb;
}NvFlowGridRenderData;

typedef struct NvFlowGridIsosurfaceData
{
    NvFlowBufferTransient* sparseBuffer;
    NvFlowTextureTransient* densityTexture;
    NvFlowSparseParams sparseParams;
}NvFlowGridIsosurfaceData;

typedef struct NvFlowGridInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowGrid*(NV_FLOW_ABI* createGrid)(
        NvFlowContextInterface* contextInterface,
        NvFlowContext* context,
        NvFlowOpList* opList,
        NvFlowExtOpList* extOpList,
        const NvFlowGridDesc* desc
        );

    void(NV_FLOW_ABI* destroyGrid)(
        NvFlowContext* context,
        NvFlowGrid* grid
        );

    void(NV_FLOW_ABI* resetGrid)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridDesc* desc
        );

    void(NV_FLOW_ABI* simulate)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridParamsDesc* params,
        NvFlowBool32 globalForceClear
        );

    void(NV_FLOW_ABI* offscreen)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridParamsDesc* params
        );

    void(NV_FLOW_ABI* getRenderData)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        NvFlowGridRenderData* renderData
        );

    void(NV_FLOW_ABI* render)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridParamsDesc* params,
        const NvFlowFloat4x4* view,
        const NvFlowFloat4x4* projection,
        const NvFlowFloat4x4* projectionJittered,
        NvFlowUint width,
        NvFlowUint height,
        NvFlowUint sceneDepthWidth,
        NvFlowUint sceneDepthHeight,
        float compositeColorScale,
        NvFlowTextureTransient* sceneDepthIn,
        NvFlowFormat sceneColorFormat,
        NvFlowTextureTransient* sceneColorIn,
        NvFlowTextureTransient** pSceneColorOut
        );

    void(NV_FLOW_ABI* updateIsosurface)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridParamsDesc* params
        );

    void(NV_FLOW_ABI* getIsosurfaceData)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        NvFlowGridIsosurfaceData* isosurfaceData
        );

    void(NV_FLOW_ABI* renderIsosurface)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        const NvFlowGridParamsDesc* params,
        const NvFlowFloat4x4* view,
        const NvFlowFloat4x4* projection,
        const NvFlowFloat4x4* projectionJittered,
        NvFlowUint width,
        NvFlowUint height,
        NvFlowUint sceneDepthWidth,
        NvFlowUint sceneDepthHeight,
        float compositeColorScale,
        NvFlowTextureTransient* sceneDepthIn,
        NvFlowFormat sceneColorFormat,
        NvFlowTextureTransient* sceneColorIn,
        NvFlowTextureTransient** pSceneColorOut
        );

    void(NV_FLOW_ABI* copyTexture)(
        NvFlowContext* context,
        NvFlowGrid* grid,
        NvFlowUint width,
        NvFlowUint height,
        NvFlowFormat sceneColorFormat,
        NvFlowTextureTransient* sceneColorIn,
        NvFlowTextureTransient** pSceneColorOut
        );

    NvFlowUint(NV_FLOW_ABI* getActiveBlockCount)(NvFlowGrid* grid);

    NvFlowUint(NV_FLOW_ABI* getActiveBlockCountIsosurface)(NvFlowGrid* grid);

    void(NV_FLOW_ABI* setResourceMinLifetime)(NvFlowContext* context, NvFlowGrid* grid, NvFlowUint64 minLifetime);
}NvFlowGridInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowGridInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(createGrid, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyGrid, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(resetGrid, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(simulate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(offscreen, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getRenderData, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(render, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(updateIsosurface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getIsosurfaceData, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(renderIsosurface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(copyTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getActiveBlockCount, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getActiveBlockCountIsosurface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(setResourceMinLifetime, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowGridInterface* (NV_FLOW_ABI* PFN_NvFlowGetGridInterface)();

NV_FLOW_API NvFlowGridInterface* NvFlowGetGridInterface();

NV_FLOW_API NvFlowGridInterface* NvFlowGetGridInterfaceNoOpt();

/// ********************************* Grid Params ***************************************

struct NvFlowGridParams;
typedef struct NvFlowGridParams NvFlowGridParams;

struct NvFlowGridParamsSnapshot;
typedef struct NvFlowGridParamsSnapshot NvFlowGridParamsSnapshot;

struct NvFlowGridParamsNamed;
typedef struct NvFlowGridParamsNamed NvFlowGridParamsNamed;

struct NvFlowGridParamsLayer;
typedef struct NvFlowGridParamsLayer NvFlowGridParamsLayer;

typedef struct NvFlowGridParamsInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowGridParams*(NV_FLOW_ABI* createGridParams)();

    void(NV_FLOW_ABI* destroyGridParams)(NvFlowGridParams* gridParams);

    void(NV_FLOW_ABI* enumerateParamTypes)(
        NvFlowGridParams* gridParams,
        const char** pTypenames,
        const char** pDisplayTypenames,
        const NvFlowReflectDataType** pDataTypes,
        NvFlowUint64* pCount
        );

    void(NV_FLOW_ABI* getVersion)(
        NvFlowGridParams* gridParams,
        NvFlowUint64* pStagingVersion,
        NvFlowUint64* pMinActiveVersion
        );

    void(NV_FLOW_ABI* commitParams)(
        NvFlowGridParams* gridParams,
        const NvFlowGridParamsDescSnapshot* snapshot
        );

    NvFlowBool32(NV_FLOW_ABI* resetParams)(NvFlowGridParams* gridParams);

    NvFlowGridParamsSnapshot*(NV_FLOW_ABI* getParamsSnapshot)(
        NvFlowGridParams* gridParams,
        double absoluteSimTime,
        NvFlowUint64 pullId
        );

    NvFlowBool32(NV_FLOW_ABI* mapParamsDesc)(
        NvFlowGridParams* gridParams,
        NvFlowGridParamsSnapshot* snapshot,
        NvFlowGridParamsDesc* pParamsDesc
        );

    void(NV_FLOW_ABI* unmapParamsDesc)(
        NvFlowGridParams* gridParams,
        NvFlowGridParamsSnapshot* snapshot
        );

    NvFlowGridParamsNamed*(NV_FLOW_ABI* createGridParamsNamed)(const char* name);

    int(NV_FLOW_ABI* destroyGridParamsNamed)(NvFlowGridParamsNamed* gridParamsNamed);

    NvFlowGridParams*(NV_FLOW_ABI* mapGridParamsNamed)(NvFlowGridParamsNamed* gridParamsNamed);

    NvFlowGridParamsLayer* (NV_FLOW_ABI* createGridParamsLayerStandard)();

    void(NV_FLOW_ABI* destroyGridParamsLayer)(NvFlowGridParamsLayer* layer);

    NvFlowGridParamsDescSnapshot(NV_FLOW_ABI* applyGridParamsLayer)(
        NvFlowGridParamsLayer* layer,
        const NvFlowGridParamsDescSnapshot* snapshot,
        NvFlowUint64 stagingVersion,
        NvFlowUint64 minActiveVersion
        );

}NvFlowGridParamsInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowGridParamsInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(createGridParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyGridParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(enumerateParamTypes, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getVersion, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(commitParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(resetParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getParamsSnapshot, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(mapParamsDesc, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(unmapParamsDesc, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createGridParamsNamed, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyGridParamsNamed, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(mapGridParamsNamed, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createGridParamsLayerStandard, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyGridParamsLayer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(applyGridParamsLayer, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowGridParamsInterface* (NV_FLOW_ABI* PFN_NvFlowGetGridParamsInterface)();

NV_FLOW_API NvFlowGridParamsInterface* NvFlowGetGridParamsInterface();

/// ********************************* Thread Pool ***************************************

struct NvFlowThreadPool;
typedef struct NvFlowThreadPool NvFlowThreadPool;

typedef void(*NvFlowThreadPoolTask_t)(NvFlowUint taskIdx, NvFlowUint threadIdx, void* sharedMem, void* userdata);

typedef struct NvFlowThreadPoolInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowUint(NV_FLOW_ABI* getDefaultThreadCount)();

    NvFlowThreadPool*(NV_FLOW_ABI* create)(NvFlowUint threadCount, NvFlowUint64 sharedMemorySizeInBytes);

    void(NV_FLOW_ABI* destroy)(NvFlowThreadPool* pool);

    NvFlowUint(NV_FLOW_ABI* getThreadCount)(NvFlowThreadPool* pool);

    void(NV_FLOW_ABI* execute)(NvFlowThreadPool* pool, NvFlowUint taskCount, NvFlowUint taskGranularity, NvFlowThreadPoolTask_t task, void* userdata);
}NvFlowThreadPoolInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowThreadPoolInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(getDefaultThreadCount, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(create, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroy, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getThreadCount, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(execute, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowThreadPoolInterface* (NV_FLOW_ABI* PFN_NvFlowThreadPoolInterface)();

NV_FLOW_API NvFlowThreadPoolInterface* NvFlowGetThreadPoolInterface();

/// ********************************* Optimization Layer ***************************************

struct NvFlowContextOpt;
typedef struct NvFlowContextOpt NvFlowContextOpt;

typedef struct NvFlowContextOptInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowContextOpt*(NV_FLOW_ABI* create)(NvFlowContextInterface* backendContextInterface, NvFlowContext* backendContext);

    void(NV_FLOW_ABI* destroy)(NvFlowContextOpt* contextOpt);

    void(NV_FLOW_ABI* getContext)(NvFlowContextOpt* contextOpt, NvFlowContextInterface** pContextInterface, NvFlowContext** pContext);

    void(NV_FLOW_ABI* flush)(NvFlowContextOpt* contextOpt);

    NvFlowBufferTransient*(NV_FLOW_ABI* importBackendBufferTransient)(NvFlowContextOpt* contextOpt, NvFlowBufferTransient* backendBufferTransient);

    NvFlowTextureTransient*(NV_FLOW_ABI* importBackendTextureTransient)(NvFlowContextOpt* contextOpt, NvFlowTextureTransient* backendTextureTransient);

    void(NV_FLOW_ABI* exportBufferTransient)(NvFlowContextOpt* contextOpt, NvFlowBufferTransient* bufferTransient, NvFlowBufferTransient** pBackendBufferTransient);

    void(NV_FLOW_ABI* exportTextureTransient)(NvFlowContextOpt* contextOpt, NvFlowTextureTransient* textureTransient, NvFlowTextureTransient** pBackendTextureTransient);

    void(NV_FLOW_ABI* setResourceMinLifetime)(NvFlowContextOpt* contextOpt, NvFlowUint64 minLifetime);
}NvFlowContextOptInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowContextOptInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(create, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroy, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getContext, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(flush, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(importBackendBufferTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(importBackendTextureTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(exportBufferTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(exportTextureTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(setResourceMinLifetime, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowContextOptInterface*(NV_FLOW_ABI* PFN_NvFlowGetContextOptInterface)();

NV_FLOW_API NvFlowContextOptInterface* NvFlowGetContextOptInterface();

/// ********************************* Reference Device ***************************************

struct NvFlowDeviceManager;
typedef struct NvFlowDeviceManager NvFlowDeviceManager;

typedef struct NvFlowPhysicalDeviceDesc
{
    NvFlowUint8 deviceUUID[16u];
    NvFlowUint8 deviceLUID[8u];
    NvFlowUint deviceNodeMask;
    NvFlowBool32 deviceLUIDValid;
}NvFlowPhysicalDeviceDesc;

typedef struct NvFlowDeviceDesc
{
    NvFlowUint deviceIndex;
    NvFlowBool32 enableExternalUsage;
    NvFlowLogPrint_t logPrint;
}NvFlowDeviceDesc;

struct NvFlowSwapchainDesc;
typedef struct NvFlowSwapchainDesc NvFlowSwapchainDesc;

#if defined(NV_FLOW_SWAPCHAIN_DESC)
struct NvFlowSwapchainDesc
{
#if defined(_WIN32)
    HINSTANCE hinstance;
    HWND hwnd;
#else
    Display* dpy;
    Window window;
#endif
    NvFlowFormat format;
};
#endif

struct NvFlowDevice;
typedef struct NvFlowDevice NvFlowDevice;

struct NvFlowDeviceQueue;
typedef struct NvFlowDeviceQueue NvFlowDeviceQueue;

struct NvFlowDeviceSemaphore;
typedef struct NvFlowDeviceSemaphore NvFlowDeviceSemaphore;

struct NvFlowSwapchain;
typedef struct NvFlowSwapchain NvFlowSwapchain;

typedef struct NvFlowProfilerEntry
{
    const char* label;
    float cpuDeltaTime;
    float gpuDeltaTime;
}NvFlowProfilerEntry;

typedef struct NvFlowDeviceMemoryStats
{
    NvFlowUint64 deviceMemoryBytes;
    NvFlowUint64 uploadMemoryBytes;
    NvFlowUint64 readbackMemoryBytes;
    NvFlowUint64 otherMemoryBytes;
}NvFlowDeviceMemoryStats;

typedef struct NvFlowDeviceInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowDeviceManager*(NV_FLOW_ABI* createDeviceManager)(NvFlowBool32 enableValidationOnDebugBuild, NvFlowThreadPoolInterface* threadPoolInterface, NvFlowUint threadCount);

    void(NV_FLOW_ABI* destroyDeviceManager)(NvFlowDeviceManager* manager);

    NvFlowBool32(NV_FLOW_ABI* enumerateDevices)(NvFlowDeviceManager* manager, NvFlowUint deviceIndex, NvFlowPhysicalDeviceDesc* pDesc);


    NvFlowDevice*(NV_FLOW_ABI* createDevice)(NvFlowDeviceManager* manager, const NvFlowDeviceDesc* desc);

    void(NV_FLOW_ABI* destroyDevice)(NvFlowDeviceManager* manager, NvFlowDevice* device);

    void(NV_FLOW_ABI* getMemoryStats)(NvFlowDevice* device, NvFlowDeviceMemoryStats* dstStats);


    NvFlowDeviceSemaphore*(NV_FLOW_ABI* createSemaphore)(NvFlowDevice* device);

    void(NV_FLOW_ABI* destroySemaphore)(NvFlowDeviceSemaphore* semaphore);

    void(NV_FLOW_ABI* getSemaphoreExternalHandle)(NvFlowDeviceSemaphore* semaphore, void* dstHandle, NvFlowUint64 dstHandleSize);

    void(NV_FLOW_ABI* closeSemaphoreExternalHandle)(NvFlowDeviceSemaphore* semaphore, const void* srcHandle, NvFlowUint64 srcHandleSize);


    NvFlowDeviceQueue*(NV_FLOW_ABI* getDeviceQueue)(NvFlowDevice* device);

    int(NV_FLOW_ABI* flush)(NvFlowDeviceQueue* queue, NvFlowUint64* flushedFrameID, NvFlowDeviceSemaphore* waitSemaphore, NvFlowDeviceSemaphore* signalSemaphore);

    NvFlowUint64(NV_FLOW_ABI* getLastFrameCompleted)(NvFlowDeviceQueue* queue);

    void(NV_FLOW_ABI* waitForFrame)(NvFlowDeviceQueue* queue, NvFlowUint64 frameFrameID);

    void(NV_FLOW_ABI* waitIdle)(NvFlowDeviceQueue* queue);

    NvFlowContextInterface*(NV_FLOW_ABI* getContextInterface)(NvFlowDeviceQueue* queue);

    NvFlowContext*(NV_FLOW_ABI* getContext)(NvFlowDeviceQueue* queue);


    NvFlowSwapchain*(NV_FLOW_ABI* createSwapchain)(NvFlowDeviceQueue* queue, const NvFlowSwapchainDesc* desc);

    void(NV_FLOW_ABI* destroySwapchain)(NvFlowSwapchain* swapchain);

    void(NV_FLOW_ABI* resizeSwapchain)(NvFlowSwapchain* swapchain, NvFlowUint width, NvFlowUint height);

    int(NV_FLOW_ABI* presentSwapchain)(NvFlowSwapchain* swapchain, NvFlowBool32 vsync, NvFlowUint64* flushedFrameID);

    NvFlowTexture*(NV_FLOW_ABI* getSwapchainFrontTexture)(NvFlowSwapchain* swapchain);


    void(NV_FLOW_ABI* enableProfiler)(NvFlowContext* context, void* userdata, void(NV_FLOW_ABI* reportEntries)(void* userdata, NvFlowUint64 captureID, NvFlowUint numEntries, NvFlowProfilerEntry* entries));

    void(NV_FLOW_ABI* disableProfiler)(NvFlowContext* context);


    NvFlowUint64(NV_FLOW_ABI* registerBufferId)(NvFlowContext* context, NvFlowBuffer* buffer);

    NvFlowUint64(NV_FLOW_ABI* registerTextureId)(NvFlowContext* context, NvFlowTexture* texture);

    void(NV_FLOW_ABI* unregisterBufferId)(NvFlowContext* context, NvFlowUint64 bufferId);

    void(NV_FLOW_ABI* unregisterTextureId)(NvFlowContext* context, NvFlowUint64 textureId);

    void(NV_FLOW_ABI* setResourceMinLifetime)(NvFlowContext* context, NvFlowUint64 minLifetime);


    void(NV_FLOW_ABI* getBufferExternalHandle)(NvFlowContext* context, NvFlowBuffer* buffer, void* dstHandle, NvFlowUint64 dstHandleSize, NvFlowUint64* pBufferSizeInBytes);

    void(NV_FLOW_ABI* closeBufferExternalHandle)(NvFlowContext* context, NvFlowBuffer* buffer, const void* srcHandle, NvFlowUint64 srcHandleSize);

}NvFlowDeviceInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowDeviceInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(createDeviceManager, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyDeviceManager, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(enumerateDevices, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createDevice, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyDevice, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getMemoryStats, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createSemaphore, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroySemaphore, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getSemaphoreExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(closeSemaphoreExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getDeviceQueue, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(flush, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getLastFrameCompleted, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(waitForFrame, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(waitIdle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getContextInterface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getContext, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createSwapchain, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroySwapchain, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(resizeSwapchain, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(presentSwapchain, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getSwapchainFrontTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(enableProfiler, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(disableProfiler, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(registerBufferId, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(registerTextureId, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(unregisterBufferId, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(unregisterTextureId, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(setResourceMinLifetime, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getBufferExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(closeBufferExternalHandle, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowDeviceInterface* (NV_FLOW_ABI* PFN_NvFlowGetDeviceInterface)(NvFlowContextApi api);

NV_FLOW_API NvFlowDeviceInterface* NvFlowGetDeviceInterface(NvFlowContextApi api);

/// ********************************* RadixSort ***************************************

struct NvFlowRadixSort;
typedef struct NvFlowRadixSort NvFlowRadixSort;

typedef struct NvFlowRadixSortInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowRadixSort*(NV_FLOW_ABI* create)(NvFlowContextInterface* contextInterface, NvFlowContext* context);

    void(NV_FLOW_ABI* destroy)(NvFlowContext* context, NvFlowRadixSort* radixSort);

    void(NV_FLOW_ABI* reserve)(NvFlowContext* context, NvFlowRadixSort* radixSort, NvFlowUint numKeys);

    void(NV_FLOW_ABI* getInputBuffers)(NvFlowContext* context, NvFlowRadixSort* radixSort, NvFlowBufferTransient** pKeyBuffer, NvFlowBufferTransient** pValBuffer);

    void(NV_FLOW_ABI* sort)(NvFlowContext* context, NvFlowRadixSort* radixSort, NvFlowUint numKeys, NvFlowUint numKeyBits);

    void(NV_FLOW_ABI* getOutputBuffers)(NvFlowContext* context, NvFlowRadixSort* radixSort, NvFlowBufferTransient** pKeyBuffer, NvFlowBufferTransient** pValBuffer);
}NvFlowRadixSortInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowRadixSortInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(create, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroy, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(reserve, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getInputBuffers, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(sort, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getOutputBuffers, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowRadixSortInterface* (NV_FLOW_ABI* PFN_NvFlowGetRadixSortInterface)();

NV_FLOW_API NvFlowRadixSortInterface* NvFlowGetRadixSortInterface();

#endif
