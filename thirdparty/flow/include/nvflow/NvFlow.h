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

#ifndef NV_FLOW_H
#define NV_FLOW_H

#include "NvFlowContext.h"
#include "shaders/NvFlowShaderTypes.h"

 /// ********************************* Op ***************************************

typedef enum NvFlowPinDir
{
    eNvFlowPinDir_in = 0,
    eNvFlowPinDir_out = 1,

    eNvFlowPinDir_count = 2,
    eNvFlowPinDir_maxEnum = 0x7FFFFFFF
}NvFlowPinDir;

struct NvFlowOp;
typedef struct NvFlowOp NvFlowOp;

struct NvFlowOpGraph;
typedef struct NvFlowOpGraph NvFlowOpGraph;

struct NvFlowOpGenericPinsIn;
typedef struct NvFlowOpGenericPinsIn NvFlowOpGenericPinsIn;

struct NvFlowOpGenericPinsOut;
typedef struct NvFlowOpGenericPinsOut NvFlowOpGenericPinsOut;

struct NvFlowOpExecuteGroup;
typedef struct NvFlowOpExecuteGroup NvFlowOpExecuteGroup;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowOpGraph)

typedef struct NvFlowOpExecuteGroupDesc
{
    NvFlowOpExecuteGroup* group;
    const char* name;
}NvFlowOpExecuteGroupDesc;

struct NvFlowOpInterface;
typedef struct NvFlowOpInterface NvFlowOpInterface;

typedef struct NvFlowOpInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    const char* opTypename;
    const NvFlowOpGraph* opGraph;
    const NvFlowReflectDataType* pinsIn;
    const NvFlowReflectDataType* pinsOut;
    const NvFlowOpExecuteGroupDesc* executeGroupDescs;
    NvFlowUint64 executeGroupCount;

    NvFlowOp*(NV_FLOW_ABI* create)(const NvFlowOpInterface* opInterface, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out);

    void(NV_FLOW_ABI* destroy)(NvFlowOp* op, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out);

    void(NV_FLOW_ABI* execute)(NvFlowOp* op, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out);

    void(NV_FLOW_ABI* executeGroup)(NvFlowOp* op, NvFlowOpExecuteGroup* group, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out);
}NvFlowOpInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowOpInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(char, opTypename, 0, 0)
NV_FLOW_REFLECT_POINTER(NvFlowOpGraph, opGraph, 0, 0)
NV_FLOW_REFLECT_POINTER(NvFlowReflectDataType, pinsIn, 0, 0)
NV_FLOW_REFLECT_POINTER(NvFlowReflectDataType, pinsOut, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(create, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroy, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(execute, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(executeGroup, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_OP_IMPL(name, nameImpl) \
    NvFlowOp* NvFlowOp_##nameImpl##_createGeneric(const NvFlowOpInterface* opInterface, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out) \
    { \
        return (NvFlowOp*)nameImpl##_create(opInterface, (const name##PinsIn*)in, (name##PinsOut*)out); \
    } \
    void NvFlowOp_##nameImpl##_destroyGeneric(NvFlowOp* op, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out) \
    { \
        nameImpl##_destroy((nameImpl*)op, (const name##PinsIn*)in, (name##PinsOut*)out); \
    } \
    void NvFlowOp_##nameImpl##_executeGeneric(NvFlowOp* op, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out) \
    { \
        nameImpl##_execute((nameImpl*)op, (const name##PinsIn*)in, (name##PinsOut*)out); \
    } \
    void NvFlowOp_##nameImpl##_executeGroupGeneric(NvFlowOp* op, NvFlowOpExecuteGroup* group, const NvFlowOpGenericPinsIn* in, NvFlowOpGenericPinsOut* out) \
    { \
        nameImpl##_execute((nameImpl*)op, (const name##PinsIn*)in, (name##PinsOut*)out); \
    } \
    NvFlowOpInterface* NvFlowOp_##nameImpl##_getOpInterface() \
    { \
        static const NvFlowOpExecuteGroupDesc executeGroupDesc = {0, 0}; \
        static NvFlowOpInterface iface = { \
            NV_FLOW_REFLECT_INTERFACE_INIT(NvFlowOpInterface), \
            #name, \
            0, \
            &name##PinsIn_NvFlowReflectDataType, \
            &name##PinsOut_NvFlowReflectDataType, \
            &executeGroupDesc, \
            1u, \
            NvFlowOp_##nameImpl##_createGeneric, \
            NvFlowOp_##nameImpl##_destroyGeneric, \
            NvFlowOp_##nameImpl##_executeGeneric, \
            NvFlowOp_##nameImpl##_executeGroupGeneric \
        }; \
        return &iface; \
    }

#define NV_FLOW_OP_TYPED(name) \
    typedef struct name \
    { \
        NvFlowOpInterface opInterface; \
        NvFlowOp* op; \
    }name; \
    NV_FLOW_INLINE NvFlowBool32 NV_FLOW_REFLECT_XCONCAT(name,_init)(name* ptr, NvFlowOpInterface* opInterface, const NV_FLOW_REFLECT_XCONCAT(name,PinsIn)* pinsIn, NV_FLOW_REFLECT_XCONCAT(name,PinsOut)* pinsOut) \
    { \
        NvFlowOpInterface_duplicate(&ptr->opInterface, opInterface); \
        if (NvFlowReflectStringCompare(ptr->opInterface.opTypename, #name) == 0) \
        { \
            ptr->op = ptr->opInterface.create(&ptr->opInterface, (const NvFlowOpGenericPinsIn*)pinsIn, (NvFlowOpGenericPinsOut*)pinsOut); \
            return NV_FLOW_TRUE; \
        } \
        ptr->opInterface.create = 0; \
        ptr->opInterface.destroy = 0; \
        ptr->opInterface.execute = 0; \
        ptr->opInterface.executeGroup = 0; \
        ptr->op = 0; \
        return NV_FLOW_FALSE; \
    } \
    NV_FLOW_INLINE void name##_destroy(name* ptr, const NV_FLOW_REFLECT_XCONCAT(name,PinsIn)* pinsIn, NV_FLOW_REFLECT_XCONCAT(name,PinsOut)* pinsOut) \
    { \
        ptr->opInterface.destroy(ptr->op, (const NvFlowOpGenericPinsIn*)pinsIn, (NvFlowOpGenericPinsOut*)pinsOut); \
    } \
    NV_FLOW_INLINE void name##_execute(name* ptr, const NV_FLOW_REFLECT_XCONCAT(name,PinsIn)* pinsIn, NV_FLOW_REFLECT_XCONCAT(name,PinsOut)* pinsOut) \
    { \
        ptr->opInterface.execute(ptr->op, (const NvFlowOpGenericPinsIn*)pinsIn, (NvFlowOpGenericPinsOut*)pinsOut); \
    }

/// ********************************* OpGraph ***************************************

// Reserved, not in use yet
struct NvFlowOpGraphInterface;
typedef struct NvFlowOpGraphInterface NvFlowOpGraphInterface;

/// ********************************* OpRuntime ***************************************

// Reserved, not in use yet
struct NvFlowOpRuntimeInterface;
typedef struct NvFlowOpRuntimeInterface NvFlowOpRuntimeInterface;

/// ********************************* Sparse ***************************************

struct NvFlowSparse;
typedef struct NvFlowSparse NvFlowSparse;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowSparse)

typedef struct NvFlowSparseParams
{
    NvFlowSparseLayerParams* layers;
    NvFlowUint layerCount;
    NvFlowSparseLevelParams* levels;
    NvFlowUint levelCount;
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
    NvFlowUint2* tableRanges;
    NvFlowUint64 tableRangeCount;
}NvFlowSparseParams;

// For new layer parameters less important to rendering, for improved binary compatibility
typedef struct NvFlowSparseSimLayerParams
{
    NvFlowBool32 clearOnRescale;
    float densityCellSizeNonAuto;
}NvFlowSparseSimLayerParams;

typedef struct NvFlowSparseSimParams
{
    NvFlowSparseParams sparseParams;
    NvFlowSparseSimLayerParams* layers;
    NvFlowUint layerCount;
}NvFlowSparseSimParams;

NV_FLOW_INLINE int NvFlow_packLayerAndLevel(int layer, int level)
{
    return (layer & 0x00FFFFFF) | (level << 24);
}

NV_FLOW_INLINE NvFlowInt2 NvFlow_unpackLayerAndLevel(int layerAndLevel)
{
    NvFlowInt2 ret;
    ret.x = layerAndLevel & 0x00FFFFFF;
    ret.y = layerAndLevel >> 24;
    return ret;
}

NV_FLOW_INLINE NvFlowUint NvFlowSparseParams_layerToLayerParamIdx(const NvFlowSparseParams* params, int layer, int level)
{
    NvFlowUint retLayerParamIdx = ~0u;
    for (NvFlowUint layerParamIdx = 0u; layerParamIdx < params->layerCount; layerParamIdx++)
    {
        if (params->layers[layerParamIdx].layerAndLevel == NvFlow_packLayerAndLevel(layer, level))
        {
            retLayerParamIdx = layerParamIdx;
            break;
        }
    }
    return retLayerParamIdx;
}

NV_FLOW_INLINE NvFlowUint NvFlowSparseParams_locationToBlockIdx(const NvFlowSparseParams* params, NvFlowInt4 location)
{
    const NvFlowSparseLevelParams* tableParams = &params->levels[0u];
    NvFlowUint3 bucketIdx = {
        (NvFlowUint)location.x & tableParams->tableDimLessOne.x,
        (NvFlowUint)location.y & tableParams->tableDimLessOne.y,
        (NvFlowUint)location.z & tableParams->tableDimLessOne.z
    };
    NvFlowUint bucketIdx1D = (bucketIdx.z << (tableParams->tableDimBits_xy)) |
        (bucketIdx.y << tableParams->tableDimBits_x) |
        (bucketIdx.x);
    NvFlowUint2 range = params->tableRanges[bucketIdx1D];
    NvFlowUint outBlockIdx = ~0u;
    for (NvFlowUint blockIdx = range.x; blockIdx < range.y; blockIdx++)
    {
        NvFlowInt4 compareLocation = params->locations[blockIdx];
        if (compareLocation.x == location.x &&
            compareLocation.y == location.y &&
            compareLocation.z == location.z &&
            compareLocation.w == location.w)
        {
            outBlockIdx = blockIdx;
            break;
        }
    }
    return outBlockIdx;
}

NV_FLOW_INLINE NvFlowBool32 NvFlowBlockIdxToLocation(const NvFlowSparseParams* params, NvFlowUint blockIdx, NvFlowInt4* out_location)
{
    NvFlowBool32 ret;
    if (blockIdx < params->locationCount)
    {
        *out_location = params->locations[blockIdx];
        ret = NV_FLOW_TRUE;
    }
    else
    {
        out_location->x = 0x40000000;
        out_location->y = 0x40000000;
        out_location->z = 0x40000000;
        out_location->w = 0x40000000;
        ret = NV_FLOW_FALSE;
    }
    return ret;
}

// TODO, maybe expand
#define NV_FLOW_REFLECT_TYPE NvFlowSparseParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, layerCount, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, levelCount, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

// TODO, maybe expand
#define NV_FLOW_REFLECT_TYPE NvFlowSparseSimParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, layerCount, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

// TODO, maybe expand
#define NV_FLOW_REFLECT_TYPE NvFlowSparseLevelParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, numLocations, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, numLayers, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSparseTexture
{
    NvFlowTextureTransient* textureTransient;
    NvFlowBufferTransient* sparseBuffer;
    NvFlowSparseParams sparseParams;
    NvFlowUint levelIdx;
    NvFlowFormat format;
}NvFlowSparseTexture;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowTextureTransient)
NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowBufferTransient)

#define NV_FLOW_REFLECT_TYPE NvFlowSparseTexture
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowTextureTransient, textureTransient, 0, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, sparseBuffer, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseParams, sparseParams, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, levelIdx, 0, 0)
NV_FLOW_REFLECT_ENUM(format, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_INLINE void NvFlowSparseTexture_passThrough(NvFlowSparseTexture* dst, const NvFlowSparseTexture* src)
{
    *dst = *src;
}

NV_FLOW_INLINE void NvFlowSparseTexture_duplicateWithFormat(NvFlowContextInterface* contextInterface, NvFlowContext* context, NvFlowSparseTexture* dst, const NvFlowSparseTexture* src, NvFlowFormat format)
{
    *dst = *src;

    NvFlowSparseLevelParams* levelParams = &dst->sparseParams.levels[dst->levelIdx];

    NvFlowTextureDesc texDesc = { eNvFlowTextureType_3d };
    texDesc.textureType = eNvFlowTextureType_3d;
    texDesc.usageFlags = eNvFlowTextureUsage_rwTexture | eNvFlowTextureUsage_texture;
    texDesc.format = format;
    texDesc.width = levelParams->dim.x;
    texDesc.height = levelParams->dim.y;
    texDesc.depth = levelParams->dim.z;
    texDesc.mipLevels = 1u;

    dst->textureTransient = contextInterface->getTextureTransient(context, &texDesc);
}

NV_FLOW_INLINE void NvFlowSparseTexture_duplicate(NvFlowContextInterface* contextInterface, NvFlowContext* context, NvFlowSparseTexture* dst, const NvFlowSparseTexture* src)
{
    NvFlowSparseTexture_duplicateWithFormat(contextInterface, context, dst, src, src->format);
}

typedef struct NvFlowSparseUpdateLayerParams
{
    NvFlowFloat3 blockSizeWorld;
    int layer;
    int level;
    NvFlowBool32 forceClear;
    NvFlowBool32 forceDisableEmitters;
    NvFlowBool32 forceDisableCoreSimulation;
    NvFlowBool32 clearOnRescale;
    float densityCellSizeNonAuto;
}NvFlowSparseUpdateLayerParams;

typedef struct NvFlowSparseInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowSparse*(NV_FLOW_ABI* create)(NvFlowContextInterface* contextInterface, NvFlowContext* context, NvFlowUint maxLocations);

    void(NV_FLOW_ABI* destroy)(NvFlowContext* context, NvFlowSparse* sparse);

    void(NV_FLOW_ABI* reset)(NvFlowContext* context, NvFlowSparse* sparse, NvFlowUint maxLocations);

    void(NV_FLOW_ABI* updateLayers)(NvFlowSparse* sparse, NvFlowUint64 updateId, NvFlowSparseUpdateLayerParams* layers, NvFlowUint numLayers);

    void(NV_FLOW_ABI* testUpdateLocations)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        NvFlowUint64 updateId,
        NvFlowInt4* locations,
        NvFlowUint numLocations,
        NvFlowUint3 baseBlockDimBits,
        NvFlowUint minLifetime,
        NvFlowUint* pOutNumLocations,
        NvFlowUint* pOutMaxLocations
    );

    void(NV_FLOW_ABI* updateLocations)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        NvFlowUint64 updateId,
        NvFlowInt4* locations,
        NvFlowUint numLocations,
        NvFlowUint3 baseBlockDimBits,
        NvFlowUint minLifetime
    );

    void(NV_FLOW_ABI* updateLayerDeltaTimes)(NvFlowSparse* sparse, float* layerDeltaTimes, NvFlowUint64 layerDeltaTimeCount);

    NvFlowBool32(NV_FLOW_ABI* getParams)(NvFlowSparse* sparse, NvFlowSparseParams* out);

    NvFlowBool32(NV_FLOW_ABI* getSimParams)(NvFlowSparse* sparse, NvFlowSparseSimParams* out);

    void(NV_FLOW_ABI* addPasses)(NvFlowContext* context, NvFlowSparse* sparse, NvFlowBufferTransient** pBufferTransient);

    void(NV_FLOW_ABI* addPassesNanoVdb)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        NvFlowUint gridType,
        NvFlowUint levelIdx,
        NvFlowSparseNanoVdbParams* pParams,
        NvFlowBufferTransient** pNanoVdbBufferTransient,
        NvFlowBufferTransient** pCacheBufferTransient
    );

    void(NV_FLOW_ABI* addPassesNanoVdbComputeStats)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        const NvFlowSparseNanoVdbParams* params,
        NvFlowBufferTransient* nanoVdbBufferTransient,
        NvFlowBufferTransient* cacheBufferTransient,
        NvFlowBufferTransient* targetNanoVdbBuffer
    );

    void(NV_FLOW_ABI* addPassesMigrate)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        NvFlowBool32 lowPrecisionRescale,
        NvFlowTextureTransient* oldTextureTransient,
        const NvFlowTextureDesc* oldTexDesc,
        NvFlowFormat targetFormat,
        NvFlowUint targetLevelIdx,
        NvFlowSparseTexture* valueOut,
        NvFlowTextureDesc* texDescOut
    );

    NvFlowBufferTransient*(NV_FLOW_ABI* getSparseBuffer)(NvFlowContext* context, NvFlowSparse* sparse);

    void(NV_FLOW_ABI* addPassesNanoVdbPruneLeaves)(
        NvFlowContext* context,
        NvFlowSparse* sparse,
        const NvFlowSparseNanoVdbParams* params,
        NvFlowBufferTransient* nanoVdbBufferTransient,
        NvFlowBufferTransient* cacheBufferTransient,
        NvFlowBufferTransient* srcNanoVdbBuffer,
        NvFlowBufferTransient* targetNanoVdbBuffer
    );
}NvFlowSparseInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(create, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroy, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(reset, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(updateLayers, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(testUpdateLocations, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(updateLocations, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(updateLayerDeltaTimes, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getSimParams, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPasses, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassesNanoVdb, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassesNanoVdbComputeStats, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassesMigrate, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getSparseBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassesNanoVdbPruneLeaves, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

/// ********************************* SparseNanoVdbExport ***************************************

typedef struct NvFlowSparseNanoVdbExportParams
{
    NvFlowBool32 enabled;
    NvFlowBool32 statisticsEnabled;
    NvFlowBool32 readbackEnabled;
    NvFlowBool32 interopEnabled;
    NvFlowBool32 temperatureEnabled;
    NvFlowBool32 fuelEnabled;
    NvFlowBool32 burnEnabled;
    NvFlowBool32 smokeEnabled;
    NvFlowBool32 velocityEnabled;
    NvFlowBool32 divergenceEnabled;
    NvFlowBool32 rgbaEnabled;
    NvFlowBool32 enableLowPrecisionRgba;
    NvFlowBool32 rgbEnabled;
}NvFlowSparseNanoVdbExportParams;

#define NvFlowSparseNanoVdbExportParams_default_init { \
    NV_FLOW_FALSE, /*enabled*/ \
    NV_FLOW_TRUE,  /*statisticsEnabled*/ \
    NV_FLOW_FALSE, /*readbackEnabled*/ \
    NV_FLOW_FALSE, /*interopEnabled*/ \
    NV_FLOW_FALSE, /*temperatureEnabled*/ \
    NV_FLOW_FALSE, /*fuelEnabled*/ \
    NV_FLOW_FALSE, /*burnEnabled*/ \
    NV_FLOW_TRUE,  /*smokeEnabled*/ \
    NV_FLOW_FALSE, /*velocityEnabled*/ \
    NV_FLOW_FALSE, /*divergenceEnabled*/ \
    NV_FLOW_FALSE, /*rgbaEnabled*/ \
    NV_FLOW_FALSE, /*enableLowPrecisionRgba*/ \
    NV_FLOW_FALSE, /*rgbEnabled*/ \
}
static const NvFlowSparseNanoVdbExportParams NvFlowSparseNanoVdbExportParams_default = NvFlowSparseNanoVdbExportParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseNanoVdbExportParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, statisticsEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, readbackEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, interopEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, temperatureEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, fuelEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, burnEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, smokeEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, velocityEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, divergenceEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rgbaEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableLowPrecisionRgba, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, rgbEnabled, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowSparseNanoVdbExportParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSparseNanoVdbExportPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseInterface* sparseInterface;
    NvFlowSparse* sparse;
    const NvFlowSparseNanoVdbExportParams** params;
    NvFlowUint64 paramCount;
    double absoluteSimTime;
    NvFlowUint readbackRingBufferCount;
    NvFlowUint interopRingBufferCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture density;
}NvFlowSparseNanoVdbExportPinsIn;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseNanoVdbExportPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowSparseInterface, sparseInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowSparse, sparse, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowSparseNanoVdbExportParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(double, absoluteSimTime, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, readbackRingBufferCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, interopRingBufferCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSparseNanoVdbExportReadback
{
    NvFlowUint64 globalFrameCompleted;
    double absoluteSimTimeCompleted;
    NvFlowUint* temperatureNanoVdbReadback;
    NvFlowUint64 temperatureNanoVdbReadbackCount;
    NvFlowUint* fuelNanoVdbReadback;
    NvFlowUint64 fuelNanoVdbReadbackCount;
    NvFlowUint* burnNanoVdbReadback;
    NvFlowUint64 burnNanoVdbReadbackCount;
    NvFlowUint* smokeNanoVdbReadback;
    NvFlowUint64 smokeNanoVdbReadbackCount;
    NvFlowUint* velocityNanoVdbReadback;
    NvFlowUint64 velocityNanoVdbReadbackCount;
    NvFlowUint* divergenceNanoVdbReadback;
    NvFlowUint64 divergenceNanoVdbReadbackCount;
    NvFlowUint* rgbaNanoVdbReadback;
    NvFlowUint64 rgbaNanoVdbReadbackCount;
    NvFlowUint* rgbNanoVdbReadback;
    NvFlowUint64 rgbNanoVdbReadbackCount;
}NvFlowSparseNanoVdbExportReadback;

#define NvFlowSparseNanoVdbExportReadback_default_init { \
    ~0llu, /*globalFrameCompleted*/ \
    0.0, /*absoluteSimTimeCompleted*/ \
    0, /*temperatureNanoVdbReadback*/ \
    0, /*temperatureNanoVdbReadbackCount*/ \
    0, /*fuelNanoVdbReadback*/ \
    0, /*fuelNanoVdbReadbackCount*/ \
    0, /*burnNanoVdbReadback*/ \
    0, /*burnNanoVdbReadbackCount*/ \
    0, /*smokeNanoVdbReadback*/ \
    0, /*smokeNanoVdbReadbackCount*/ \
    0, /*velocityNanoVdbReadback*/ \
    0, /*velocityNanoVdbReadbackCount*/ \
    0, /*divergenceNanoVdbReadback*/ \
    0, /*divergenceNanoVdbReadbackCount*/ \
    0, /*rgbaNanoVdbReadback*/ \
    0, /*rgbaNanoVdbReadbackCount*/ \
    0, /*rgbNanoVdbReadback*/ \
    0, /*rgbNanoVdbReadbackCount*/ \
}
static const NvFlowSparseNanoVdbExportReadback NvFlowSparseNanoVdbExportReadback_default = NvFlowSparseNanoVdbExportReadback_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseNanoVdbExportReadback
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, globalFrameCompleted, 0, 0)
NV_FLOW_REFLECT_VALUE(double, absoluteSimTimeCompleted, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, temperatureNanoVdbReadback, temperatureNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, fuelNanoVdbReadback, fuelNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, burnNanoVdbReadback, burnNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, smokeNanoVdbReadback, smokeNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, velocityNanoVdbReadback, velocityNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, divergenceNanoVdbReadback, divergenceNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, rgbaNanoVdbReadback, rgbaNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowUint, rgbNanoVdbReadback, rgbNanoVdbReadbackCount, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowSparseNanoVdbExportReadback_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSparseNanoVdbExportInterop
{
    NvFlowUint64 globalFrameCompleted;
    double absoluteSimTimeCompleted;
    NvFlowInteropHandle temperatureInteropHandle;
    NvFlowInteropHandle fuelInteropHandle;
    NvFlowInteropHandle burnInteropHandle;
    NvFlowInteropHandle smokeInteropHandle;
    NvFlowInteropHandle velocityInteropHandle;
    NvFlowInteropHandle divergenceInteropHandle;
    NvFlowInteropHandle rgbaInteropHandle;
    NvFlowInteropHandle rgbInteropHandle;
}NvFlowSparseNanoVdbExportInterop;

#define NvFlowSparseNanoVdbExportInterop_default_init { \
    ~0llu, /*globalFrameCompleted*/ \
    0.0, /*absoluteSimTimeCompleted*/ \
    NvFlowInteropHandle_default_init, /*temperatureInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*fuelInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*burnInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*smokeInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*velocityInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*divergenceInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*rgbaInteropHandle*/ \
    NvFlowInteropHandle_default_init, /*rgbInteropHandle*/ \
}
static const NvFlowSparseNanoVdbExportInterop NvFlowSparseNanoVdbExportInterop_default = NvFlowSparseNanoVdbExportInterop_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseNanoVdbExportInterop
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint64, globalFrameCompleted, 0, 0)
NV_FLOW_REFLECT_VALUE(double, absoluteSimTimeCompleted, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, temperatureInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, fuelInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, burnInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, smokeInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, velocityInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, divergenceInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, rgbaInteropHandle, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowInteropHandle, rgbInteropHandle, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowSparseNanoVdbExportInterop_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSparseNanoVdbExportPinsOut
{
    NvFlowBufferTransient* temperatureNanoVdb;
    NvFlowBufferTransient* fuelNanoVdb;
    NvFlowBufferTransient* burnNanoVdb;
    NvFlowBufferTransient* smokeNanoVdb;
    NvFlowBufferTransient* velocityNanoVdb;
    NvFlowBufferTransient* divergenceNanoVdb;
    NvFlowBufferTransient* rgbaNanoVdb;
    NvFlowBufferTransient* rgbNanoVdb;

    NvFlowSparseNanoVdbExportReadback* readbacks;
    NvFlowUint64 readbackCount;

    NvFlowSparseNanoVdbExportInterop* interops;
    NvFlowUint64 interopCount;

    double lastAbsoluteSimTimeCompleted;
}NvFlowSparseNanoVdbExportPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowSparseNanoVdbExportPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, temperatureNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, fuelNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, burnNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, smokeNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, velocityNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, divergenceNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, rgbaNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER(NvFlowBufferTransient, rgbNanoVdb, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowSparseNanoVdbExportReadback, readbacks, readbackCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(double, lastAbsoluteSimTimeCompleted, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowSparseNanoVdbExport)

/// ********************************* Advect ***************************************

typedef struct NvFlowAdvectionChannelParams
{
    float secondOrderBlendThreshold;
    float secondOrderBlendFactor;
    float damping;
    float fade;
}NvFlowAdvectionChannelParams;

#define NvFlowAdvectionChannelParams_default_init { \
    0.5f,     /*secondOrderBlendThreshold*/ \
    0.001f,     /*secondOrderBlendFactor*/ \
    0.01f,     /*damping*/ \
    0.f         /*fade*/ \
}
static const NvFlowAdvectionChannelParams NvFlowAdvectionChannelParams_default = NvFlowAdvectionChannelParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionChannelParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(float, secondOrderBlendThreshold, 0, 0)
NV_FLOW_REFLECT_VALUE(float, secondOrderBlendFactor, 0, 0)
NV_FLOW_REFLECT_VALUE(float, damping, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fade, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowAdvectionChannelParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowAdvectionCombustionParams
{
    NvFlowBool32 enabled;                //!< Allows advection to be disabled when not in use
    NvFlowBool32 downsampleEnabled;        //!< Allows density downsample in velocity advection to be disabled
    NvFlowBool32 combustionEnabled;        //!< Allows combustion to be disabled
    NvFlowBool32 forceFadeEnabled;        //!< Force fade to apply even when advection disabled

    NvFlowAdvectionChannelParams velocity;
    NvFlowAdvectionChannelParams divergence;
    NvFlowAdvectionChannelParams temperature;
    NvFlowAdvectionChannelParams fuel;
    NvFlowAdvectionChannelParams burn;
    NvFlowAdvectionChannelParams smoke;

    float ignitionTemp;            //!< Minimum temperature for combustion
    float burnPerTemp;            //!< Burn amount per unit temperature above ignitionTemp
    float fuelPerBurn;            //!< Fuel consumed per unit burn
    float tempPerBurn;            //!< Temperature increase per unit burn
    float smokePerBurn;            //!< Density increase per unit burn
    float divergencePerBurn;    //!< Expansion per unit burn
    float buoyancyPerTemp;        //!< Buoyant force per unit temperature
    float buoyancyPerSmoke;        //!< Buoyant force per unit smoke
    float buoyancyMaxSmoke;        //!< Smoke clamp value applied before computing smoke buoyancy
    float coolingRate;            //!< Cooling rate, exponential
    NvFlowFloat3 gravity;

    NvFlowBool32 globalFetch;    //!< Global fetch, removes velocity clamping
}NvFlowAdvectionCombustionParams;

#define NvFlowAdvectionCombustionParams_default_init { \
    NV_FLOW_TRUE, /*enabled*/ \
    NV_FLOW_TRUE, /*downsampleEnabled*/ \
    NV_FLOW_TRUE, /*combustionEnabled*/ \
    NV_FLOW_FALSE,    /*forceFadeEnabled*/ \
    {0.001f, 0.5f, 0.01f, 1.00f},    /*velocity : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    {0.001f, 0.5f, 0.01f, 1.00f},    /*divergence : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    {0.001f, 0.9f, 0.00f, 0.00f},    /*temperature : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    {0.001f, 0.9f, 0.00f, 0.00f},    /*fuel : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    {0.001f, 0.9f, 0.00f, 0.00f},    /*burn : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    {0.001f, 0.9f, 0.30f, 0.65f},    /*smoke : {secondOrderBlendThreshold, secondOrderBlendFactor, damping, fade}*/ \
    0.05f,    /*ignitionTemp*/ \
    4.f,    /*burnPerTemp*/ \
    0.25f,    /*fuelPerBurn*/ \
    5.f,    /*tempPerBurn*/ \
    3.f,    /*smokePerBurn*/ \
    0.f,    /*divergencePerBurn*/ \
    2.f,    /*buoyancyPerTemp*/ \
    0.f,    /*buoyancyPerSmoke*/ \
    1.f,    /*buoyancyMaxSmoke*/ \
    1.5f,    /*coolingRate*/ \
    0.f,    /*gravity.x*/ \
    0.f,    /*gravity.y*/ \
    -100.f,    /*gravity.z*/ \
    0u        /*globalFetch*/ \
}
static const NvFlowAdvectionCombustionParams NvFlowAdvectionCombustionParams_default = NvFlowAdvectionCombustionParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionCombustionParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, downsampleEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, combustionEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, forceFadeEnabled, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, velocity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, divergence, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, temperature, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, fuel, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, burn, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowAdvectionChannelParams, smoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, ignitionTemp, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnPerTemp, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelPerBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, tempPerBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokePerBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, divergencePerBurn, 0, 0)
NV_FLOW_REFLECT_VALUE(float, buoyancyPerTemp, 0, 0)
NV_FLOW_REFLECT_VALUE(float, buoyancyPerSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, buoyancyMaxSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(float, coolingRate, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowFloat3, gravity, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, globalFetch, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowAdvectionCombustionParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowAdvectionSimplePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    NvFlowSparseTexture velocity;
}NvFlowAdvectionSimplePinsIn;

typedef struct NvFlowAdvectionSimplePinsOut
{
    NvFlowSparseTexture velocity;
}NvFlowAdvectionSimplePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionSimplePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionSimplePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowAdvectionSimple)

typedef struct NvFlowAdvectionCombustionDensityPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowAdvectionCombustionParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture density;
    NvFlowSparseTexture voxelWeight;
    NvFlowSparseTexture densityTemp;
}NvFlowAdvectionCombustionDensityPinsIn;

typedef struct NvFlowAdvectionCombustionDensityPinsOut
{
    NvFlowSparseTexture density;
    NvFlowSparseTexture voxelWeight;
}NvFlowAdvectionCombustionDensityPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionCombustionDensityPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowAdvectionCombustionParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionCombustionDensityPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowAdvectionCombustionDensity)

typedef struct NvFlowAdvectionCombustionVelocityPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowAdvectionCombustionParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture voxelWeight;
    NvFlowSparseTexture velocityTemp;
    NvFlowSparseTexture density;
    NvFlowSparseTexture densityCoarse;
}NvFlowAdvectionCombustionVelocityPinsIn;

typedef struct NvFlowAdvectionCombustionVelocityPinsOut
{
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture voxelWeight;
    NvFlowSparseTexture densityCoarse;
}NvFlowAdvectionCombustionVelocityPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionCombustionVelocityPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowAdvectionCombustionParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocityTemp, eNvFlowReflectHint_pinEnabledMutable, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, density, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityCoarse, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowAdvectionCombustionVelocityPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, voxelWeight, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityCoarse, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowAdvectionCombustionVelocity)

/// ********************************* Pressure ***************************************

typedef struct NvFlowPressureParams
{
    NvFlowBool32 enabled;
}NvFlowPressureParams;

#define NvFlowPressureParams_default_init { \
    NV_FLOW_TRUE, /*enabled*/ \
}
static const NvFlowPressureParams NvFlowPressureParams_default = NvFlowPressureParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowPressureParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowPressureParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowPressurePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    const NvFlowPressureParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
}NvFlowPressurePinsIn;

typedef struct NvFlowPressurePinsOut
{
    NvFlowSparseTexture velocity;
}NvFlowPressurePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowPressurePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowPressureParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowPressurePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowPressure)

/// ********************************* Vorticity ***************************************

typedef struct NvFlowVorticityParams
{
    NvFlowBool32 enabled;
    float forceScale;
    float velocityMask;
    float constantMask;
    float densityMask;
    float velocityLogScale;
    float velocityLinearMask;
    float temperatureMask;
    float fuelMask;
    float burnMask;
    float smokeMask;
}NvFlowVorticityParams;

#define NvFlowVorticityParams_default_init { \
    NV_FLOW_TRUE, /*enabled*/ \
    0.6f,    /*forceScale*/ \
    1.f,    /*velocityMask*/ \
    0.f,    /*constantMask*/ \
    0.f,    /*densityMask*/ \
    1.f,    /*velocityLogScale*/ \
    0.f,    /*velocityLinearMask*/ \
    0.f,    /*temperatureMask*/ \
    0.f,    /*fuelMask*/ \
    0.f,    /*burnMask*/ \
    0.f        /*smokeMask*/ \
}
static const NvFlowVorticityParams NvFlowVorticityParams_default = NvFlowVorticityParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowVorticityParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(float, forceScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, constantMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, densityMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityLogScale, 0, 0)
NV_FLOW_REFLECT_VALUE(float, velocityLinearMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, temperatureMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, fuelMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, burnMask, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeMask, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowVorticityParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowVorticityPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    float deltaTime;
    const NvFlowVorticityParams** params;
    NvFlowUint64 paramCount;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture coarseDensity;
}NvFlowVorticityPinsIn;

typedef struct NvFlowVorticityPinsOut
{
    NvFlowSparseTexture velocity;
}NvFlowVorticityPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowVorticityPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(float, deltaTime, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowVorticityParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, coarseDensity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowVorticityPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowVorticity)

/// ********************************* Summary ***************************************

typedef struct NvFlowSummaryAllocateParams
{
    NvFlowBool32 enabled;
    float smokeThreshold;
    float speedThreshold;
    float speedThresholdMinSmoke;
    NvFlowBool32 enableNeighborAllocation;
}NvFlowSummaryAllocateParams;

#define NvFlowSummaryAllocateParams_default_init { \
    NV_FLOW_TRUE, /*enabled*/ \
    0.02f,    /*smokeThreshold*/ \
    1.f,    /*speedThreshold*/ \
    0.f,    /*speedThresholdMinSmoke*/ \
    NV_FLOW_TRUE,    /*enableNeighborAllocation*/ \
}
static const NvFlowSummaryAllocateParams NvFlowSummaryAllocateParams_default = NvFlowSummaryAllocateParams_default_init;

#define NV_FLOW_REFLECT_TYPE NvFlowSummaryAllocateParams
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enabled, 0, 0)
NV_FLOW_REFLECT_VALUE(float, smokeThreshold, 0, 0)
NV_FLOW_REFLECT_VALUE(float, speedThreshold, 0, 0)
NV_FLOW_REFLECT_VALUE(float, speedThresholdMinSmoke, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowBool32, enableNeighborAllocation, 0, 0)
NV_FLOW_REFLECT_END(&NvFlowSummaryAllocateParams_default)
#undef NV_FLOW_REFLECT_TYPE

typedef struct NvFlowSummaryFeedback
{
    void* data;
}NvFlowSummaryFeedback;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowSummaryFeedback)

typedef struct NvFlowSummaryPinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSummaryFeedback feedback;
    NvFlowSparseTexture velocity;
    NvFlowSparseTexture densityCoarse;
    const NvFlowSummaryAllocateParams** params;
    NvFlowUint64 paramCount;
}NvFlowSummaryPinsIn;

typedef struct NvFlowSummaryPinsOut
{
    NvFlowUint unused;
}NvFlowSummaryPinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowSummaryPinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSummaryFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, velocity, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseTexture, densityCoarse, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowSummaryAllocateParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowSummaryPinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, unused, eNvFlowReflectHint_none, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowSummary)

typedef struct NvFlowSummaryAllocatePinsIn
{
    NvFlowContextInterface* contextInterface;
    NvFlowContext* context;
    NvFlowSparseSimParams sparseSimParams;
    const NvFlowSummaryAllocateParams** params;
    NvFlowUint64 paramCount;
}NvFlowSummaryAllocatePinsIn;

typedef struct NvFlowSummaryAllocatePinsOut
{
    NvFlowSummaryFeedback feedback;
    NvFlowInt4* locations;
    NvFlowUint64 locationCount;
}NvFlowSummaryAllocatePinsOut;

#define NV_FLOW_REFLECT_TYPE NvFlowSummaryAllocatePinsIn
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_POINTER(NvFlowContextInterface, contextInterface, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_POINTER(NvFlowContext, context, eNvFlowReflectHint_pinEnabledGlobal, 0)
NV_FLOW_REFLECT_VALUE(NvFlowSparseSimParams, sparseSimParams, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_POINTER_ARRAY(NvFlowSummaryAllocateParams, params, paramCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowSummaryAllocatePinsOut
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowSummaryFeedback, feedback, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowInt4, locations, locationCount, eNvFlowReflectHint_pinEnabled, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_OP_TYPED(NvFlowSummaryAllocate)

/// ********************************* NvFlowOpList ***************************************

typedef struct NvFlowOpList
{
    NV_FLOW_REFLECT_INTERFACE();

    NvFlowOpGraphInterface* (NV_FLOW_ABI* getOpGraphInterface)();

    NvFlowOpRuntimeInterface* (NV_FLOW_ABI* getOpRuntimeInterface)();

    NvFlowSparseInterface* (NV_FLOW_ABI* getSparseInterface)();

    NvFlowOpInterface* (NV_FLOW_ABI* pSparseNanoVdbExport)();

    NvFlowOpInterface* (NV_FLOW_ABI* pAdvectionSimple)();

    NvFlowOpInterface* (NV_FLOW_ABI* pAdvectionCombustionDensity)();

    NvFlowOpInterface* (NV_FLOW_ABI* pAdvectionCombustionVelocity)();

    NvFlowOpInterface* (NV_FLOW_ABI* pPressure)();

    NvFlowOpInterface* (NV_FLOW_ABI* pVorticity)();

    NvFlowOpInterface* (NV_FLOW_ABI* pSummary)();

    NvFlowOpInterface* (NV_FLOW_ABI* pSummaryAllocate)();
}NvFlowOpList;

#define NV_FLOW_REFLECT_TYPE NvFlowOpList
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(getOpGraphInterface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getOpRuntimeInterface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getSparseInterface, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pSparseNanoVdbExport, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pAdvectionSimple, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pAdvectionCombustionDensity, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pAdvectionCombustionVelocity, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pPressure, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pVorticity, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pSummary, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(pSummaryAllocate, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

typedef NvFlowOpList* (NV_FLOW_ABI* PFN_NvFlowGetOpList)();

NV_FLOW_API NvFlowOpList* NvFlowGetOpList();

#endif
