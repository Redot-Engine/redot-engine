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

#ifndef NV_FLOW_REFLECT_H
#define NV_FLOW_REFLECT_H

#include "NvFlowTypes.h"

/// ************************* NvFlowReflect **********************************

NV_FLOW_INLINE int NvFlowReflectStringCompare(const char* a, const char* b)
{
    a = a ? a : "\0";
    b = b ? b : "\0";
    int idx = 0;
    while (a[idx] || b[idx])
    {
        if (a[idx] != b[idx])
        {
            return a[idx] < b[idx] ? -1 : +1;
        }
        idx++;
    }
    return 0;
}

NV_FLOW_INLINE const char* NvFlowTypeToString(NvFlowType type)
{
    switch (type)
    {
    case eNvFlowType_unknown: return "unknown";
    case eNvFlowType_void: return "void";
    case eNvFlowType_function: return "function";
    case eNvFlowType_struct: return "struct";
    case eNvFlowType_int: return "int";
    case eNvFlowType_int2: return "int2";
    case eNvFlowType_int3: return "int3";
    case eNvFlowType_int4: return "int4";
    case eNvFlowType_uint: return "uint";
    case eNvFlowType_uint2: return "uint2";
    case eNvFlowType_uint3: return "uint3";
    case eNvFlowType_uint4: return "uint4";
    case eNvFlowType_float: return "float";
    case eNvFlowType_float2: return "float2";
    case eNvFlowType_float3: return "float3";
    case eNvFlowType_float4: return "float4";
    case eNvFlowType_float4x4: return "float4x4";
    case eNvFlowType_bool32: return "bool32";
    case eNvFlowType_uint8: return "uint8";
    case eNvFlowType_uint16: return "uint16";
    case eNvFlowType_uint64: return "uint64";
    case eNvFlowType_char: return "char";
    case eNvFlowType_double: return "double";
    default: return "unknown";
    }
}

NV_FLOW_INLINE NvFlowType NvFlowTypeFromString(const char* name)
{
    if (NvFlowReflectStringCompare(name, "unknown") == 0) { return eNvFlowType_unknown; }
    else if (NvFlowReflectStringCompare(name, "struct") == 0) { return eNvFlowType_struct; }
    else if (NvFlowReflectStringCompare(name, "void") == 0) { return eNvFlowType_void; }
    else if (NvFlowReflectStringCompare(name, "function") == 0) { return eNvFlowType_function; }
    else if (NvFlowReflectStringCompare(name, "int") == 0) { return eNvFlowType_int; }
    else if (NvFlowReflectStringCompare(name, "int2") == 0) { return eNvFlowType_int2; }
    else if (NvFlowReflectStringCompare(name, "int3") == 0) { return eNvFlowType_int3; }
    else if (NvFlowReflectStringCompare(name, "int4") == 0) { return eNvFlowType_int4; }
    else if (NvFlowReflectStringCompare(name, "uint") == 0) { return eNvFlowType_uint; }
    else if (NvFlowReflectStringCompare(name, "uint2") == 0) { return eNvFlowType_uint2; }
    else if (NvFlowReflectStringCompare(name, "uint3") == 0) { return eNvFlowType_uint3; }
    else if (NvFlowReflectStringCompare(name, "uint4") == 0) { return eNvFlowType_uint4; }
    else if (NvFlowReflectStringCompare(name, "float") == 0) { return eNvFlowType_float; }
    else if (NvFlowReflectStringCompare(name, "float2") == 0) { return eNvFlowType_float2; }
    else if (NvFlowReflectStringCompare(name, "float3") == 0) { return eNvFlowType_float3; }
    else if (NvFlowReflectStringCompare(name, "float4") == 0) { return eNvFlowType_float4; }
    else if (NvFlowReflectStringCompare(name, "float4x4") == 0) { return eNvFlowType_float4x4; }
    else if (NvFlowReflectStringCompare(name, "bool32") == 0) { return eNvFlowType_bool32; }
    else if (NvFlowReflectStringCompare(name, "uint8") == 0) { return eNvFlowType_uint8; }
    else if (NvFlowReflectStringCompare(name, "uint16") == 0) { return eNvFlowType_uint16; }
    else if (NvFlowReflectStringCompare(name, "uint64") == 0) { return eNvFlowType_uint64; }
    else if (NvFlowReflectStringCompare(name, "char") == 0) { return eNvFlowType_char; }
    else if (NvFlowReflectStringCompare(name, "double") == 0) { return eNvFlowType_double; }
    else return eNvFlowType_unknown;
}

NV_FLOW_INLINE void NvFlowReflectMemcpy(void* dst, const void* src, NvFlowUint64 numBytes)
{
    for (NvFlowUint64 byteIdx = 0u; byteIdx < numBytes; byteIdx++)
    {
        ((NvFlowUint8*)dst)[byteIdx] = ((const NvFlowUint8*)src)[byteIdx];
    }
}

NV_FLOW_INLINE void NvFlowReflectClear(void* dst, NvFlowUint64 numBytes)
{
    for (NvFlowUint64 byteIdx = 0u; byteIdx < numBytes; byteIdx++)
    {
        ((NvFlowUint8*)dst)[byteIdx] = 0;
    }
}

typedef NvFlowUint NvFlowReflectHintFlags;

typedef enum NvFlowReflectHint
{
    eNvFlowReflectHint_none = 0x00,

    eNvFlowReflectHint_transient = 0x00000001,            // Hint to not serialize
    eNvFlowReflectHint_noEdit = 0x00000002,                // Hint to not expose to editor
    eNvFlowReflectHint_transientNoEdit = 0x00000003,

    eNvFlowReflectHint_resource = 0x0000000C,            // Mask for resource hints
    eNvFlowReflectHint_asset = 0x0000000C,                // Hint to serialize as external asset, instead of inlined
    eNvFlowReflectHint_bufferId = 0x00000004,            // Hint to treat NvFlowUint64 as bufferId, allowing conversion from paths to ids
    eNvFlowReflectHint_textureId = 0x00000008,            // Hint to treat NvFlowUint64 as textureId, allowing conversion from paths to ids

    eNvFlowReflectHint_pinEnabled = 0x00010000,
    eNvFlowReflectHint_pinGlobal = 0x00020000,
    eNvFlowReflectHint_pinEnabledGlobal = 0x00030000,
    eNvFlowReflectHint_pinMutable = 0x00040000,
    eNvFlowReflectHint_pinEnabledMutable = 0x00050000,
    eNvFlowReflectHint_pinGroup = 0x00080000,

    eNvFlowReflectHint_maxEnum = 0x7FFFFFFF
}NvFlowReflectHint;

typedef NvFlowUint NvFlowReflectModeFlags;

typedef enum NvFlowReflectMode
{
    eNvFlowReflectMode_value = 0x00,
    eNvFlowReflectMode_pointer = 0x01,
    eNvFlowReflectMode_array = 0x02,
    eNvFlowReflectMode_pointerArray = 0x03,
    eNvFlowReflectMode_valueVersioned = 0x04,
    eNvFlowReflectMode_pointerVersioned = 0x05,
    eNvFlowReflectMode_arrayVersioned = 0x06,
    eNvFlowReflectMode_pointerArrayVersioned = 0x07,

    eNvFlowReflectMode_maxEnum = 0x7FFFFFFF
}NvFlowReflectMode;

struct NvFlowReflectDataType;
typedef struct NvFlowReflectDataType NvFlowReflectDataType;

typedef struct NvFlowReflectData
{
    NvFlowReflectHintFlags reflectHints;
    NvFlowReflectModeFlags reflectMode;
    const NvFlowReflectDataType* dataType;
    const char* name;
    NvFlowUint64 dataOffset;
    NvFlowUint64 arraySizeOffset;
    NvFlowUint64 versionOffset;
    const char* metadata;
}NvFlowReflectData;

typedef struct NvFlowReflectDataType
{
    NvFlowType dataType;
    NvFlowUint64 elementSize;
    const char* structTypename;
    const NvFlowReflectData* childReflectDatas;
    NvFlowUint64 childReflectDataCount;
    const void* defaultValue;
}NvFlowReflectDataType;

typedef void(NV_FLOW_ABI* NvFlowReflectProcess_t)(NvFlowUint8* data, const NvFlowReflectDataType* dataType, void* userdata);

NV_FLOW_INLINE NvFlowBool32 NvFlowReflectLayoutCompare(
    const NvFlowReflectDataType* dstType,
    const NvFlowReflectDataType* srcType
)
{
    if (dstType == srcType)
    {
        return NV_FLOW_TRUE;
    }
    if (dstType->dataType != srcType->dataType ||
        dstType->elementSize != srcType->elementSize ||
        dstType->childReflectDataCount != srcType->childReflectDataCount ||
        NvFlowReflectStringCompare(dstType->structTypename, srcType->structTypename) != 0)
    {
        return NV_FLOW_FALSE;
    }
    for (NvFlowUint64 childIdx = 0u; childIdx < dstType->childReflectDataCount; childIdx++)
    {
        const NvFlowReflectData* childDst = dstType->childReflectDatas + childIdx;
        const NvFlowReflectData* childSrc = srcType->childReflectDatas + childIdx;
        if (childDst->reflectMode != childSrc->reflectMode ||
            childDst->dataOffset != childSrc->dataOffset ||
            childDst->arraySizeOffset != childSrc->arraySizeOffset ||
            childDst->versionOffset != childSrc->versionOffset ||
            NvFlowReflectStringCompare(childDst->name, childSrc->name) != 0)
        {
            return NV_FLOW_FALSE;
        }
        if (!NvFlowReflectLayoutCompare(childDst->dataType, childSrc->dataType))
        {
            return NV_FLOW_FALSE;
        }
    }
    return NV_FLOW_TRUE;
}

NV_FLOW_INLINE void NvFlowReflectCopyByName(
    void* dstData, const NvFlowReflectDataType* dstType,
    const void* srcData, const NvFlowReflectDataType* srcType
)
{
    NvFlowUint8* dstData8 = (NvFlowUint8*)dstData;
    const NvFlowUint8* srcData8 = (const NvFlowUint8*)srcData;

    // For safety, take min of elementSize
    NvFlowUint64 safeCopySize = srcType->elementSize < dstType->elementSize ? srcType->elementSize : dstType->elementSize;

    // Start with raw copy, to potential cover non-reflect data
    NvFlowReflectMemcpy(dstData, srcData, safeCopySize);

    // Copy by name
    if (dstType != srcType)
    {
        NvFlowUint64 srcIdx = 0u;
        for (NvFlowUint64 dstIdx = 0u; dstIdx < dstType->childReflectDataCount; dstIdx++)
        {
            const NvFlowReflectData* childDst = dstType->childReflectDatas + dstIdx;
            NvFlowUint64 matchSrcIdx = ~0llu;
            for (NvFlowUint64 srcCount = 0u; matchSrcIdx == ~0llu && srcCount < srcType->childReflectDataCount; srcCount++)
            {
                const NvFlowReflectData* childSrc = srcType->childReflectDatas + srcIdx;
                if (childDst->name == childSrc->name ||
                    NvFlowReflectStringCompare(childDst->name, childSrc->name) == 0)
                {
                    matchSrcIdx = srcIdx;
                }
                srcIdx++;
                if (srcIdx >= srcType->childReflectDataCount)
                {
                    srcIdx = 0u;
                }
            }
            if (matchSrcIdx < srcType->childReflectDataCount)
            {
                const NvFlowReflectData* childSrc = srcType->childReflectDatas + matchSrcIdx;
                if (childSrc->dataType->dataType == eNvFlowType_struct &&
                    (childSrc->reflectMode == eNvFlowReflectMode_value || childSrc->reflectMode == eNvFlowReflectMode_valueVersioned))
                {
                    NvFlowReflectCopyByName(
                        dstData8 + childDst->dataOffset, childDst->dataType,
                        srcData8 + childSrc->dataOffset, childSrc->dataType
                    );
                }
                else
                {
                    // only copy if not covered by bulk memcpy
                    if (childDst->dataOffset != childSrc->dataOffset)
                    {
                        NvFlowReflectMemcpy(
                            dstData8 + childDst->dataOffset,
                            srcData8 + childSrc->dataOffset,
                            (childDst->reflectMode & eNvFlowReflectMode_pointerArray) ? sizeof(void*) : childDst->dataType->elementSize
                        );
                    }
                    if (childDst->reflectMode & eNvFlowReflectMode_array)
                    {
                        if (childDst->arraySizeOffset != childSrc->arraySizeOffset)
                        {
                            NvFlowReflectMemcpy(
                                dstData8 + childDst->arraySizeOffset,
                                srcData8 + childSrc->arraySizeOffset,
                                sizeof(NvFlowUint64)
                            );
                        }
                    }
                    if (childDst->reflectMode & eNvFlowReflectMode_valueVersioned)
                    {
                        if (childDst->versionOffset != childSrc->versionOffset)
                        {
                            NvFlowReflectMemcpy(
                                dstData8 + childDst->versionOffset,
                                srcData8 + childSrc->versionOffset,
                                sizeof(NvFlowUint64)
                            );
                        }
                    }
                }
            }
            else
            {
                NvFlowReflectClear(
                    dstData8 + childDst->dataOffset,
                    (childDst->reflectMode & eNvFlowReflectMode_pointerArray) ? sizeof(void*) : childDst->dataType->elementSize
                );
                if (childDst->reflectMode & eNvFlowReflectMode_array)
                {
                    NvFlowReflectClear(
                        dstData8 + childDst->arraySizeOffset,
                        sizeof(NvFlowUint64)
                    );
                }
                if (childDst->reflectMode & eNvFlowReflectMode_valueVersioned)
                {
                    NvFlowReflectClear(
                        dstData8 + childDst->versionOffset,
                        sizeof(NvFlowUint64)
                    );
                }
            }
        }
    }
}

// Reflect blocks must start with #define NV_FLOW_REFLECT_TYPE typename
// And end with #undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_XSTR(X) NV_FLOW_REFLECT_STR(X)
#define NV_FLOW_REFLECT_STR(X) #X

#define NV_FLOW_REFLECT_XCONCAT(A, B) NV_FLOW_REFLECT_CONCAT(A, B)
#define NV_FLOW_REFLECT_CONCAT(A, B) A##B

#define NV_FLOW_REFLECT_VALIDATE(type) \
    NV_FLOW_INLINE type* NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtr)(const type* v) { return (type*)v; } \
    NV_FLOW_INLINE type** NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtr)(const type* const * v) { return (type**)v; } \
    NV_FLOW_INLINE type*** NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtrPtr)(const type*const *const * v) { return (type***)v; }

#if defined(__cplusplus)
#define NV_FLOW_REFLECT_VALIDATE_value(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointer(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_array(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerArray(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_valueVersioned(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerVersioned(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_arrayVersioned(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerArrayVersioned(type, name) NV_FLOW_REFLECT_XCONCAT(type,_NvFlowValidatePtrPtrPtr)(&((NV_FLOW_REFLECT_TYPE*)0)->name)
#else
#define NV_FLOW_REFLECT_VALIDATE_value(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointer(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_array(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerArray(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_valueVersioned(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerVersioned(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_arrayVersioned(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#define NV_FLOW_REFLECT_VALIDATE_pointerArrayVersioned(type, name) (&((NV_FLOW_REFLECT_TYPE*)0)->name)
#endif

#define NV_FLOW_REFLECT_BUILTIN_IMPL(enumName, typeName) \
    static const NvFlowReflectDataType NV_FLOW_REFLECT_XCONCAT(typeName,_NvFlowReflectDataType) = { enumName, sizeof(typeName), 0, 0, 0, 0 }; \
    NV_FLOW_REFLECT_VALIDATE(typeName)

#define NV_FLOW_REFLECT_STRUCT_OPAQUE_IMPL(name) \
    static const NvFlowReflectDataType NV_FLOW_REFLECT_XCONCAT(name,_NvFlowReflectDataType) = { eNvFlowType_struct, 0llu, #name, 0, 0, 0 }; \
    NV_FLOW_REFLECT_VALIDATE(name)

#define NV_FLOW_REFLECT_BEGIN() \
        static const NvFlowReflectData NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_reflectDatas)[] = {

#define NV_FLOW_REFLECT_END(defaultValue) \
        }; \
        static const NvFlowReflectDataType NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_NvFlowReflectDataType) = { \
            eNvFlowType_struct, \
            sizeof(NV_FLOW_REFLECT_TYPE), \
            NV_FLOW_REFLECT_XSTR(NV_FLOW_REFLECT_TYPE), \
            NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_reflectDatas), \
            sizeof(NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_reflectDatas)) / sizeof(NvFlowReflectData), \
            defaultValue \
        }; \
        NV_FLOW_REFLECT_VALIDATE(NV_FLOW_REFLECT_TYPE)

#define NV_FLOW_REFLECT_TYPE_ALIAS(SRC, DST) \
    typedef SRC DST; \
    static const NvFlowReflectDataType NV_FLOW_REFLECT_XCONCAT(DST,_NvFlowReflectDataType) = { \
        eNvFlowType_struct, \
        sizeof(SRC), \
        #DST, \
        NV_FLOW_REFLECT_XCONCAT(SRC,_reflectDatas), \
        sizeof(NV_FLOW_REFLECT_XCONCAT(SRC,_reflectDatas)) / sizeof(NvFlowReflectData), \
        &NV_FLOW_REFLECT_XCONCAT(SRC,_default) \
        }; \
    NV_FLOW_REFLECT_VALIDATE(DST)

NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_int, int)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_int2, NvFlowInt2)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_int3, NvFlowInt3)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_int4, NvFlowInt4)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint, NvFlowUint)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint2, NvFlowUint2)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint3, NvFlowUint3)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint4, NvFlowUint4)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_float, float)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_float2, NvFlowFloat2)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_float3, NvFlowFloat3)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_float4, NvFlowFloat4)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_float4x4, NvFlowFloat4x4)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_bool32, NvFlowBool32)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint8, NvFlowUint8)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint16, NvFlowUint16)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_uint64, NvFlowUint64)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_char, char)
NV_FLOW_REFLECT_BUILTIN_IMPL(eNvFlowType_double, double)

#if defined(__cplusplus)
#define NV_FLOW_REFLECT_SIZE_OFFSET(name_size) (NvFlowUint64)NV_FLOW_REFLECT_VALIDATE_value(NvFlowUint64, name_size)
#define NV_FLOW_REFLECT_VERSION_OFFSET(version) (NvFlowUint64)NV_FLOW_REFLECT_VALIDATE_value(NvFlowUint64, version)
#else
#define NV_FLOW_REFLECT_SIZE_OFFSET(name_size) (NvFlowUint64)(&((NV_FLOW_REFLECT_TYPE*)0)->name_size)
#define NV_FLOW_REFLECT_VERSION_OFFSET(version) (NvFlowUint64)(&((NV_FLOW_REFLECT_TYPE*)0)->version)
#endif

/// Builtin
#define NV_FLOW_REFLECT_GENERIC(reflectMode, type, name, ARRAY, VERSION, reflectHints, metadata) { \
    reflectHints, \
    NV_FLOW_REFLECT_XCONCAT(eNvFlowReflectMode_,reflectMode), \
    &NV_FLOW_REFLECT_XCONCAT(type,_NvFlowReflectDataType), \
    #name, \
    (NvFlowUint64)NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_VALIDATE_,reflectMode)(type, name), \
    ARRAY, \
    VERSION, \
    metadata \
    },
#define NV_FLOW_REFLECT_VALUE(type, name, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(value, type, name, 0, 0, reflectHints, metadata)
#define NV_FLOW_REFLECT_POINTER(type, name, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(pointer, type, name, 0, 0, reflectHints, metadata)
#define NV_FLOW_REFLECT_ARRAY(type, name, name_size, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(array, type, name, NV_FLOW_REFLECT_SIZE_OFFSET(name_size), 0, reflectHints, metadata)
#define NV_FLOW_REFLECT_POINTER_ARRAY(type, name, name_size, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(pointerArray, type, name, NV_FLOW_REFLECT_SIZE_OFFSET(name_size), 0, reflectHints, metadata)
#define NV_FLOW_REFLECT_VALUE_VERSIONED(type, name, version, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(valueVersioned, type, name, 0, NV_FLOW_REFLECT_VERSION_OFFSET(version), reflectHints, metadata)
#define NV_FLOW_REFLECT_POINTER_VERSIONED(type, name, version, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(pointerVersioned, type, name, 0, NV_FLOW_REFLECT_VERSION_OFFSET(version), reflectHints, metadata)
#define NV_FLOW_REFLECT_ARRAY_VERSIONED(type, name, name_size, version, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(arrayVersioned, type, name, NV_FLOW_REFLECT_SIZE_OFFSET(name_size), NV_FLOW_REFLECT_VERSION_OFFSET(version), reflectHints, metadata)
#define NV_FLOW_REFLECT_POINTER_ARRAY_VERSIONED(type, name, name_size, version, reflectHints, metadata) NV_FLOW_REFLECT_GENERIC(pointerArrayVersioned, type, name, NV_FLOW_REFLECT_SIZE_OFFSET(name_size), NV_FLOW_REFLECT_VERSION_OFFSET(version), reflectHints, metadata)

/// Function Pointer
static const NvFlowReflectDataType function_NvFlowReflectDataType = { eNvFlowType_function, 0llu, 0, 0, 0, 0 };
#define NV_FLOW_REFLECT_FUNCTION_POINTER(name, reflectHints, metadata) { \
    reflectHints, \
    eNvFlowReflectMode_pointer, \
    &function_NvFlowReflectDataType, \
    #name, \
    (NvFlowUint64)(&((NV_FLOW_REFLECT_TYPE*)0)->name), \
    0, \
    0, \
    metadata \
    },

/// Void
static const NvFlowReflectDataType void_NvFlowReflectDataType = { eNvFlowType_void, 0llu, 0, 0, 0, 0 };
#define NV_FLOW_REFLECT_VOID_POINTER(name, reflectHints, metadata) { \
    reflectHints, \
    eNvFlowReflectMode_pointer, \
    &void_NvFlowReflectDataType, \
    #name, \
    (NvFlowUint64)(&((NV_FLOW_REFLECT_TYPE*)0)->name), \
    0, \
    0, \
    metadata \
    },

/// Enum
#define NV_FLOW_REFLECT_ENUM(name, reflectHints, metadata) { \
    reflectHints, \
    eNvFlowReflectMode_value, \
    &NvFlowUint_NvFlowReflectDataType, \
    #name, \
    (NvFlowUint64)(&((NV_FLOW_REFLECT_TYPE*)0)->name), \
    0, \
    0, \
    metadata \
    },

#define NV_FLOW_REFLECT_INTERFACE() const NvFlowReflectDataType* interface_NvFlowReflectDataType

#define NV_FLOW_REFLECT_INTERFACE_INIT(type) &NV_FLOW_REFLECT_XCONCAT(type,_NvFlowReflectDataType)

#define NV_FLOW_REFLECT_INTERFACE_IMPL() \
    NV_FLOW_INLINE void NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_duplicate)(NV_FLOW_REFLECT_TYPE* dst, const NV_FLOW_REFLECT_TYPE* src) \
    { \
        const NvFlowReflectDataType* dstType = &NV_FLOW_REFLECT_XCONCAT(NV_FLOW_REFLECT_TYPE,_NvFlowReflectDataType); \
        NvFlowReflectCopyByName( \
            dst, dstType, \
            src, src->interface_NvFlowReflectDataType  \
        ); \
        dst->interface_NvFlowReflectDataType = dstType; \
    }

#define NV_FLOW_REFLECT_TYPE NvFlowReflectData
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_VALUE(NvFlowUint, reflectHints, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint, reflectMode, 0, 0)
NV_FLOW_REFLECT_VOID_POINTER(/*NvFlowReflectDataType,*/ dataType, 0, 0)    // void to break circular reference
NV_FLOW_REFLECT_POINTER(char, name, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, dataOffset, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, arraySizeOffset, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

#define NV_FLOW_REFLECT_TYPE NvFlowReflectDataType
NV_FLOW_REFLECT_BEGIN()
NV_FLOW_REFLECT_ENUM(dataType, 0, 0)
NV_FLOW_REFLECT_VALUE(NvFlowUint64, elementSize, 0, 0)
NV_FLOW_REFLECT_POINTER(char, structTypename, 0, 0)
NV_FLOW_REFLECT_ARRAY(NvFlowReflectData, childReflectDatas, childReflectDataCount, 0, 0)
NV_FLOW_REFLECT_VOID_POINTER(defaultValue, 0, 0)
NV_FLOW_REFLECT_END(0)
#undef NV_FLOW_REFLECT_TYPE

NV_FLOW_INLINE const char* NvFlowReflectTrimPrefix(const char* name)
{
    if (name && name[0] == 'N')
    {
        name++;
    }
    if (name && name[0] == 'v')
    {
        name++;
    }
    return name;
}

#define NV_FLOW_CAST_PAIR(X, Y) \
    NV_FLOW_INLINE X* cast(Y* ptr) { return (X*)ptr; } \
    NV_FLOW_INLINE Y* cast(X* ptr) { return (Y*)ptr; } \
    NV_FLOW_INLINE const X* cast(const Y* ptr) { return (X*)ptr; } \
    NV_FLOW_INLINE const Y* cast(const X* ptr) { return (Y*)ptr; }

#define NV_FLOW_CAST_PAIR_NAMED(name, X, Y) \
    NV_FLOW_INLINE X* name##_cast(Y* ptr) { return (X*)ptr; } \
    NV_FLOW_INLINE Y* name##_cast(X* ptr) { return (Y*)ptr; } \
    NV_FLOW_INLINE const X* name##_cast(const Y* ptr) { return (X*)ptr; } \
    NV_FLOW_INLINE const Y* name##_cast(const X* ptr) { return (Y*)ptr; }


typedef struct NvFlowDatabaseTypeSnapshot
{
    NvFlowUint64 version;
    const NvFlowReflectDataType* dataType;
    NvFlowUint8** instanceDatas;
    NvFlowUint64 instanceCount;
}NvFlowDatabaseTypeSnapshot;

typedef struct NvFlowDatabaseSnapshot
{
    NvFlowUint64 version;
    NvFlowDatabaseTypeSnapshot* typeSnapshots;
    NvFlowUint64 typeSnapshotCount;
}NvFlowDatabaseSnapshot;

NV_FLOW_INLINE void NvFlowDatabaseSnapshot_findType(const NvFlowDatabaseSnapshot* snapshot, const NvFlowReflectDataType* findDataType, NvFlowDatabaseTypeSnapshot** pSnapshot)
{
    // try to find matching pointer first
    for (NvFlowUint64 idx = 0u; idx < snapshot->typeSnapshotCount; idx++)
    {
        if (snapshot->typeSnapshots[idx].dataType == findDataType)
        {
            *pSnapshot = &snapshot->typeSnapshots[idx];
            return;
        }
    }
    // try to find by matching size and name
    for (NvFlowUint64 idx = 0u; idx < snapshot->typeSnapshotCount; idx++)
    {
        if (snapshot->typeSnapshots[idx].dataType->elementSize == findDataType->elementSize &&
            NvFlowReflectStringCompare(snapshot->typeSnapshots[idx].dataType->structTypename, findDataType->structTypename) == 0)
        {
            *pSnapshot = &snapshot->typeSnapshots[idx];
            return;
        }
    }
    *pSnapshot = 0;
}

NV_FLOW_INLINE void NvFlowDatabaseSnapshot_findTypeArray(const NvFlowDatabaseSnapshot* snapshot, const NvFlowReflectDataType* findDataType, void*** pData, NvFlowUint64* pCount)
{
    NvFlowDatabaseTypeSnapshot* typeSnapshot = 0;
    NvFlowDatabaseSnapshot_findType(snapshot, findDataType, &typeSnapshot);
    if (typeSnapshot)
    {
        *pData = (void**)typeSnapshot->instanceDatas;
        *pCount = typeSnapshot->instanceCount;
    }
    else
    {
        *pData = 0;
        *pCount = 0llu;
    }
}

#define NV_FLOW_DATABASE_SNAPSHOT_FIND_TYPE_ARRAY(snapshot, type) \
    type** type##_elements = 0; \
    NvFlowUint64 type##_elementCount = 0llu; \
    NvFlowDatabaseSnapshot_findTypeArray(snapshot, &type##_NvFlowReflectDataType, (void***)&type##_elements, &type##_elementCount);

#endif
