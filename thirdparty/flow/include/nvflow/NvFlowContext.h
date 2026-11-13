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

#ifndef NV_FLOW_CONTEXT_H
#define NV_FLOW_CONTEXT_H

#include "NvFlowReflect.h"

/// ************************* NvFlowContext **********************************

struct NvFlowContext;
typedef struct NvFlowContext NvFlowContext;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowContext)

typedef enum NvFlowTextureBindingType
{
    eNvFlowTextureBindingType_separateSampler = 0,

    eNvFlowTextureBindingType_count = 1,
    eNvFlowTextureBindingType_maxEnum = 0x7FFFFFFF
}NvFlowTextureBindingType;

typedef struct NvFlowContextConfig
{
    NvFlowContextApi api;
    NvFlowTextureBindingType textureBinding;
}NvFlowContextConfig;

typedef struct NvFlowBytecode
{
    const void* data;
    NvFlowUint64 sizeInBytes;
}NvFlowBytecode;

struct NvFlowBuffer;
typedef struct NvFlowBuffer NvFlowBuffer;

struct NvFlowBufferTransient;
typedef struct NvFlowBufferTransient NvFlowBufferTransient;

struct NvFlowBufferAcquire;
typedef struct NvFlowBufferAcquire NvFlowBufferAcquire;

typedef enum NvFlowMemoryType
{
    eNvFlowMemoryType_device = 0,
    eNvFlowMemoryType_upload = 1,
    eNvFlowMemoryType_readback = 2,

    eNvFlowMemoryType_maxEnum = 0x7FFFFFFF
}NvFlowMemoryType;

typedef NvFlowUint NvFlowBufferUsageFlags;

typedef enum NvFlowBufferUsage
{
    eNvFlowBufferUsage_constantBuffer = 0x01,
    eNvFlowBufferUsage_structuredBuffer = 0x02,
    eNvFlowBufferUsage_buffer = 0x04,
    eNvFlowBufferUsage_rwStructuredBuffer = 0x08,
    eNvFlowBufferUsage_rwBuffer = 0x10,

    eNvFlowBufferUsage_indirectBuffer = 0x20,
    eNvFlowBufferUsage_bufferCopySrc = 0x40,
    eNvFlowBufferUsage_bufferCopyDst = 0x80,

    eNvFlowBufferUsage_maxEnum = 0x7FFFFFFF
}NvFlowBufferUsage;

typedef struct NvFlowBufferDesc
{
    NvFlowBufferUsageFlags usageFlags;
    NvFlowFormat format;
    NvFlowUint structureStride;
    NvFlowUint64 sizeInBytes;
}NvFlowBufferDesc;

struct NvFlowTexture;
typedef struct NvFlowTexture NvFlowTexture;

struct NvFlowTextureTransient;
typedef struct NvFlowTextureTransient NvFlowTextureTransient;

struct NvFlowTextureAcquire;
typedef struct NvFlowTextureAcquire NvFlowTextureAcquire;

struct NvFlowSampler;
typedef struct NvFlowSampler NvFlowSampler;

typedef enum NvFlowTextureType
{
    eNvFlowTextureType_1d = 0,
    eNvFlowTextureType_2d = 1,
    eNvFlowTextureType_3d = 2,

    eNvFlowTextureType_maxEnum = 0x7FFFFFFF
}NvFlowTextureType;

typedef NvFlowUint NvFlowTextureUsageFlags;

typedef enum NvFlowTextureUsage
{
    eNvFlowTextureUsage_texture = 0x01,
    eNvFlowTextureUsage_rwTexture = 0x02,
    eNvFlowTextureUsage_textureCopySrc = 0x04,
    eNvFlowTextureUsage_textureCopyDst = 0x08,

    eNvFlowTextureUsage_maxEnum = 0x7FFFFFFF
}NvFlowTextureUsage;

typedef struct NvFlowTextureDesc
{
    NvFlowTextureType textureType;
    NvFlowTextureUsageFlags usageFlags;
    NvFlowFormat format;
    NvFlowUint width;
    NvFlowUint height;
    NvFlowUint depth;
    NvFlowUint mipLevels;
    NvFlowFloat4 optimizedClearValue;
}NvFlowTextureDesc;

typedef enum NvFlowSamplerAddressMode
{
    eNvFlowSamplerAddressMode_wrap = 0,
    eNvFlowSamplerAddressMode_clamp = 1,
    eNvFlowSamplerAddressMode_mirror = 2,
    eNvFlowSamplerAddressMode_border = 3,

    eNvFlowSamplerAddressMode_count = 4,
    eNvFlowSamplerAddressMode_maxEnum = 0x7FFFFFFF
}NvFlowSamplerAddressMode;

typedef enum NvFlowSamplerFilterMode
{
    eNvFlowSamplerFilterMode_point = 0,
    eNvFlowSamplerFilterMode_linear = 1,

    eNvFlowSamplerFilterMode_count = 2,
    eNvFlowSamplerFilterMode_maxEnum = 0x7FFFFFFF
}NvFlowSamplerFilterMode;

typedef struct NvFlowSamplerDesc
{
    NvFlowSamplerAddressMode addressModeU;
    NvFlowSamplerAddressMode addressModeV;
    NvFlowSamplerAddressMode addressModeW;
    NvFlowSamplerFilterMode filterMode;
}NvFlowSamplerDesc;

typedef enum NvFlowDescriptorType
{
    eNvFlowDescriptorType_unknown = 0,

    /// Explicit in NFSL shader code
    eNvFlowDescriptorType_constantBuffer = 1,            // HLSL register b
    eNvFlowDescriptorType_structuredBuffer = 2,            // HLSL register t
    eNvFlowDescriptorType_buffer = 3,                    // HLSL register t
    eNvFlowDescriptorType_texture = 4,                    // HLSL register t
    eNvFlowDescriptorType_sampler = 5,                    // HLSL register s
    eNvFlowDescriptorType_rwStructuredBuffer = 6,        // HLSL register u
    eNvFlowDescriptorType_rwBuffer = 7,                    // HLSL register u
    eNvFlowDescriptorType_rwTexture = 8,                // HLSL register u

    /// If requiresCombinedTextureSampler, uses TextureSampler instead of separate texture and sampler
    eNvFlowDescriptorType_textureSampler = 9,            // Vulkan only

    /// Descriptors not explicitly mentioned in shaders
    eNvFlowDescriptorType_indirectBuffer = 10,            // No register
    eNvFlowDescriptorType_bufferCopySrc = 11,            // No register
    eNvFlowDescriptorType_bufferCopyDst = 12,            // No register
    eNvFlowDescriptorType_textureCopySrc = 13,            // No register
    eNvFlowDescriptorType_textureCopyDst = 14,            // No register

    eNvFlowDescriptorType_count = 15,
    eNvFlowDescriptorType_maxEnum = 0x7FFFFFFF
}NvFlowDescriptorType;

typedef struct NvFlowResource
{
    NvFlowBufferTransient* bufferTransient;
    NvFlowTextureTransient* textureTransient;
    NvFlowSampler* sampler;
}NvFlowResource;

typedef enum NvFlowRegisterHlsl
{
    eNvFlowRegisterHlsl_unknown = 0,
    eNvFlowRegisterHlsl_b = 1,
    eNvFlowRegisterHlsl_t = 2,
    eNvFlowRegisterHlsl_s = 3,
    eNvFlowRegisterHlsl_u = 4,

    eNvFlowRegisterHlsl_count = 5,
    eNvFlowRegisterHlsl_maxEnum = 0x7FFFFFFF
}NvFlowRegisterHlsl;

typedef struct NvFlowDescriptorWriteD3D12
{
    NvFlowRegisterHlsl registerHlsl;
    NvFlowUint registerIndex;
    NvFlowUint space;
}NvFlowDescriptorWriteD3D12;

typedef struct NvFlowDescriptorWriteVulkan
{
    NvFlowUint binding;
    NvFlowUint arrayIndex;
    NvFlowUint set;
}NvFlowDescriptorWriteVulkan;

typedef struct NvFlowDescriptorWriteUnion
{
    NvFlowDescriptorWriteD3D12 d3d12;
    NvFlowDescriptorWriteVulkan vulkan;
}NvFlowDescriptorWriteUnion;

typedef struct NvFlowDescriptorWrite
{
    NvFlowDescriptorType type;
    NvFlowDescriptorWriteUnion write;
}NvFlowDescriptorWrite;

typedef struct NvFlowBindingDescD3D12
{
    NvFlowRegisterHlsl registerHlsl;
    NvFlowUint registerBegin;
    NvFlowUint numRegisters;
    NvFlowUint space;
}NvFlowBindingDescD3D12;

typedef struct NvFlowBindingDescVulkan
{
    NvFlowUint binding;
    NvFlowUint descriptorCount;
    NvFlowUint set;
}NvFlowBindingDescVulkan;

typedef struct NvFlowBindingDescUnion
{
    NvFlowBindingDescD3D12 d3d12;
    NvFlowBindingDescVulkan vulkan;
}NvFlowBindingDescUnion;

typedef struct NvFlowBindingDesc
{
    NvFlowDescriptorType type;
    NvFlowBindingDescUnion bindingDesc;
}NvFlowBindingDesc;

struct NvFlowComputePipeline;
typedef struct NvFlowComputePipeline NvFlowComputePipeline;

typedef struct NvFlowComputePipelineDesc
{
    NvFlowUint numBindingDescs;
    NvFlowBindingDesc* bindingDescs;

    NvFlowBytecode bytecode;
}NvFlowComputePipelineDesc;

typedef struct NvFlowPassComputeParams
{
    NvFlowComputePipeline* pipeline;
    NvFlowUint3 gridDim;

    NvFlowUint numDescriptorWrites;
    const NvFlowDescriptorWrite* descriptorWrites;
    const NvFlowResource* resources;

    const char* debugLabel;
}NvFlowPassComputeParams;

typedef struct NvFlowPassCopyBufferParams
{
    NvFlowUint64 srcOffset;
    NvFlowUint64 dstOffset;
    NvFlowUint64 numBytes;

    NvFlowBufferTransient* src;
    NvFlowBufferTransient* dst;

    const char* debugLabel;
}NvFlowPassCopyBufferParams;

typedef struct NvFlowPassCopyBufferToTextureParams
{
    NvFlowUint64 bufferOffset;
    NvFlowUint bufferRowPitch;
    NvFlowUint bufferDepthPitch;

    NvFlowUint textureMipLevel;
    NvFlowUint3 textureOffset;
    NvFlowUint3 textureExtent;

    NvFlowBufferTransient* src;
    NvFlowTextureTransient* dst;

    const char* debugLabel;
}NvFlowPassCopyBufferToTextureParams;

typedef struct NvFlowPassCopyTextureToBufferParams
{
    NvFlowUint64 bufferOffset;
    NvFlowUint bufferRowPitch;
    NvFlowUint bufferDepthPitch;

    NvFlowUint textureMipLevel;
    NvFlowUint3 textureOffset;
    NvFlowUint3 textureExtent;

    NvFlowTextureTransient* src;
    NvFlowBufferTransient* dst;

    const char* debugLabel;
}NvFlowPassCopyTextureToBufferParams;

typedef struct NvFlowPassCopyTextureParams
{
    NvFlowUint srcMipLevel;
    NvFlowUint3 srcOffset;

    NvFlowUint dstMipLevel;
    NvFlowUint3 dstOffset;

    NvFlowUint3 extent;

    NvFlowTextureTransient* src;
    NvFlowTextureTransient* dst;

    const char* debugLabel;
}NvFlowPassCopyTextureParams;

typedef void(*NvFlowContextThreadPoolTask_t)(NvFlowUint taskIdx, NvFlowUint threadIdx, void* sharedMem, void* userdata);

typedef enum NvFlowInteropHandleType
{
    eNvFlowInteropHandleType_unknown = 0,
    eNvFlowInteropHandleType_opaqueFd = 1,
    eNvFlowInteropHandleType_opaqueWin32 = 2,

    eNvFlowInteropHandleType_count = 3,
    eNvFlowInteropHandleType_maxEnum = 0x7FFFFFFF
}NvFlowInteropHandleType;

typedef struct NvFlowInteropHandle
{
    NvFlowInteropHandleType type;
    NvFlowUint64 value;
    NvFlowUint64 resourceSizeInBytes;
}NvFlowInteropHandle;

#define NvFlowInteropHandle_default_init { \
    eNvFlowInteropHandleType_unknown, /*type*/ \
    0u, /*value*/  \
    0u, /*resourceSizeInBytes*/  \
}
static const NvFlowInteropHandle NvFlowInteropHandle_default = NvFlowInteropHandle_default_init;

NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(NvFlowInteropHandle)

typedef enum NvFlowContextFeature
{
    eNvFlowContextFeature_unknown = 0,
    eNvFlowContextFeature_aliasResourceFormats = 1,
    eNvFlowContextFeature_bufferExternalHandle = 2,

    eNvFlowContextFeature_count = 3,
    eNvFlowContextFeature_maxEnum = 0x7FFFFFFF
}NvFlowContextFeature;

typedef struct NvFlowContextInterface
{
    NV_FLOW_REFLECT_INTERFACE();

    void(NV_FLOW_ABI* getContextConfig)(NvFlowContext* context, NvFlowContextConfig* config);

    NvFlowBool32(NV_FLOW_ABI* isFeatureSupported)(NvFlowContext* context, NvFlowContextFeature feature);

    NvFlowUint64(NV_FLOW_ABI* getCurrentFrame)(NvFlowContext* context);

    NvFlowUint64(NV_FLOW_ABI* getLastFrameCompleted)(NvFlowContext* context);

    NvFlowUint64(NV_FLOW_ABI* getCurrentGlobalFrame)(NvFlowContext* context);

    NvFlowUint64(NV_FLOW_ABI* getLastGlobalFrameCompleted)(NvFlowContext* context);

    NvFlowLogPrint_t(NV_FLOW_ABI* getLogPrint)(NvFlowContext* context);


    void(NV_FLOW_ABI* executeTasks)(NvFlowContext* context, NvFlowUint taskCount, NvFlowUint taskGranularity, NvFlowContextThreadPoolTask_t task, void* userdata);


    NvFlowBuffer*(NV_FLOW_ABI* createBuffer)(NvFlowContext* context, NvFlowMemoryType memoryType, const NvFlowBufferDesc* desc);

    void(NV_FLOW_ABI* destroyBuffer)(NvFlowContext* context, NvFlowBuffer* buffer);

    NvFlowBufferTransient*(NV_FLOW_ABI* getBufferTransient)(NvFlowContext* context, const NvFlowBufferDesc* desc);

    NvFlowBufferTransient*(NV_FLOW_ABI* registerBufferAsTransient)(NvFlowContext* context, NvFlowBuffer* buffer);

    NvFlowBufferTransient* (NV_FLOW_ABI* aliasBufferTransient)(NvFlowContext* context, NvFlowBufferTransient* buffer, NvFlowFormat format, NvFlowUint structureStride);

    NvFlowBufferAcquire*(NV_FLOW_ABI* enqueueAcquireBuffer)(NvFlowContext* context, NvFlowBufferTransient* buffer);

    NvFlowBool32(NV_FLOW_ABI* getAcquiredBuffer)(NvFlowContext* context, NvFlowBufferAcquire* acquire, NvFlowBuffer** outBuffer);

    void*(NV_FLOW_ABI* mapBuffer)(NvFlowContext* context, NvFlowBuffer* buffer);

    void(NV_FLOW_ABI* unmapBuffer)(NvFlowContext* context, NvFlowBuffer* buffer);

    NvFlowBufferTransient*(NV_FLOW_ABI* getBufferTransientById)(NvFlowContext* context, NvFlowUint64 bufferId);

    void(NV_FLOW_ABI* getBufferExternalHandle)(NvFlowContext* context, NvFlowBuffer* buffer, NvFlowInteropHandle* dstHandle);

    void(NV_FLOW_ABI* closeBufferExternalHandle)(NvFlowContext* context, NvFlowBuffer* buffer, const NvFlowInteropHandle* srcHandle);

    NvFlowBuffer*(NV_FLOW_ABI* createBufferFromExternalHandle)(NvFlowContext* context, const NvFlowBufferDesc* desc, const NvFlowInteropHandle* interopHandle);


    NvFlowTexture*(NV_FLOW_ABI* createTexture)(NvFlowContext* context, const NvFlowTextureDesc* desc);

    void(NV_FLOW_ABI* destroyTexture)(NvFlowContext* context, NvFlowTexture* texture);

    NvFlowTextureTransient*(NV_FLOW_ABI* getTextureTransient)(NvFlowContext* context, const NvFlowTextureDesc* desc);

    NvFlowTextureTransient*(NV_FLOW_ABI* registerTextureAsTransient)(NvFlowContext* context, NvFlowTexture* texture);

    NvFlowTextureTransient* (NV_FLOW_ABI* aliasTextureTransient)(NvFlowContext* context, NvFlowTextureTransient* texture, NvFlowFormat format);

    NvFlowTextureAcquire*(NV_FLOW_ABI* enqueueAcquireTexture)(NvFlowContext* context, NvFlowTextureTransient* texture);

    NvFlowBool32(NV_FLOW_ABI* getAcquiredTexture)(NvFlowContext* context, NvFlowTextureAcquire* acquire, NvFlowTexture** outTexture);

    NvFlowTextureTransient*(NV_FLOW_ABI* getTextureTransientById)(NvFlowContext* context, NvFlowUint64 textureId);


    NvFlowSampler*(NV_FLOW_ABI* createSampler)(NvFlowContext* context, const NvFlowSamplerDesc* desc);

    NvFlowSampler*(NV_FLOW_ABI* getDefaultSampler)(NvFlowContext* context);

    void(NV_FLOW_ABI* destroySampler)(NvFlowContext* context, NvFlowSampler* sampler);


    NvFlowComputePipeline*(NV_FLOW_ABI* createComputePipeline)(NvFlowContext* context, const NvFlowComputePipelineDesc* desc);

    void(NV_FLOW_ABI* destroyComputePipeline)(NvFlowContext* context, NvFlowComputePipeline* pipeline);


    void(NV_FLOW_ABI* addPassCompute)(NvFlowContext* context, const NvFlowPassComputeParams* params);

    void(NV_FLOW_ABI* addPassCopyBuffer)(NvFlowContext* context, const NvFlowPassCopyBufferParams* params);

    void(NV_FLOW_ABI* addPassCopyBufferToTexture)(NvFlowContext* context, const NvFlowPassCopyBufferToTextureParams* params);

    void(NV_FLOW_ABI* addPassCopyTextureToBuffer)(NvFlowContext* context, const NvFlowPassCopyTextureToBufferParams* params);

    void(NV_FLOW_ABI* addPassCopyTexture)(NvFlowContext* context, const NvFlowPassCopyTextureParams* params);

}NvFlowContextInterface;

#define NV_FLOW_REFLECT_TYPE NvFlowContextInterface
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_FUNCTION_POINTER(getContextConfig, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(isFeatureSupported, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getCurrentFrame, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getLastFrameCompleted, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getCurrentGlobalFrame, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getLastGlobalFrameCompleted, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getLogPrint, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(executeTasks, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getBufferTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(registerBufferAsTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(aliasBufferTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(enqueueAcquireBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getAcquiredBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(mapBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(unmapBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getBufferTransientById, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getBufferExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(closeBufferExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createBufferFromExternalHandle, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getTextureTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(registerTextureAsTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(aliasTextureTransient, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(enqueueAcquireTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getAcquiredTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getTextureTransientById, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createSampler, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(getDefaultSampler, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroySampler, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(createComputePipeline, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(destroyComputePipeline, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassCompute, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassCopyBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassCopyBufferToTexture, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassCopyTextureToBuffer, 0, 0)
NV_FLOW_REFLECT_FUNCTION_POINTER(addPassCopyTexture, 0, 0)
NV_FLOW_REFLECT_END(0)
NV_FLOW_REFLECT_INTERFACE_IMPL()
#undef NV_FLOW_REFLECT_TYPE

#endif
