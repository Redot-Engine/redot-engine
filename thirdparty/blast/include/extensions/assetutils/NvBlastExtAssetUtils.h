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
//
// Copyright (c) 2016-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines the API for the NvBlastExtAssetUtils blast sdk extension

#ifndef NVBLASTEXTASSETUTILS_H
#define NVBLASTEXTASSETUTILS_H


#include "NvBlastTypes.h"
#include "NvCTypes.h"
#include <stdint.h>


/**
Reauthor the provided asset to create external bonds in the specified support chunks.

\param[in] asset                    Pointer to the original asset. Won't be modified.
\param[in] externalBoundChunks      Array of support chunk indices which are to be bound to the external body.
\param[in] externalBoundChunksCount Size of externalBoundChunks array.
\param[in] bondDirections           Array of normals for each bond (size externalBoundChunksCount)
\param[in] bondUserData             Array of user data values for the new bonds, of size externalBoundChunksCount.  May be NULL.  If NULL, bond user data will be set to zero.

\return a new asset with added bonds if successful, NULL otherwise.
*/
NV_C_API NvBlastAsset* NvBlastExtAssetUtilsAddExternalBonds
(
    const NvBlastAsset* asset,
    const uint32_t* externalBoundChunks,
    uint32_t externalBoundChunkCount,
    const NvcVec3* bondDirections,
    const uint32_t* bondUserData
);

// DEPRICATED: remove on next major version bump
#define NvBlastExtAssetUtilsAddWorldBonds NvBlastExtAssetUtilsAddExternalBonds


/**
Bond descriptor used to merge assets.

In addition to the NvBlastBondDesc fields, adds "component" indices to indicate
to which component asset the chunk indices in NvBlastBondDesc refer.  Used in the
function NvBlastExtAssetUtilsMergeAssets.
*/
struct NvBlastExtAssetUtilsBondDesc : public NvBlastBondDesc
{
    uint32_t    componentIndices[2];    //!< The asset component for the corresponding chunkIndices[2] value.
};


/**
Creates an asset descriptor from an asset.

NOTE: This function allocates memory using the allocator in NvBlastGlobals, to create the new chunk and bond
descriptor arrays referenced in the returned NvBlastAssetDesc.  The user must free this memory after use with
NVBLAST_FREE appied to the pointers in the returned NvBlastAssetDesc.

\param[in]  asset   The asset from which to create a descriptor.

\return an asset descriptor that will build an exact duplicate of the input asset.
*/
NV_C_API NvBlastAssetDesc NvBlastExtAssetUtilsCreateDesc(const NvBlastAsset* asset);


/**
Creates an asset descriptor which will build an asset that merges several assets.  Each asset (or component)
is given a transform, applied to the geometric information in the chunk and bond descriptors.

New bond descriptors may be given to bond support chunks from different components.

An NvBlastAsset may appear more than once in the components array.

This function will call NvBlastEnsureAssetExactSupportCoverage on the returned chunk descriptors.  It will also
call NvBlastReorderAssetDescChunks if the user passes in valid arrays for chunkReorderMap and chunkReorderMapSize.
Otherwise, the user must ensure that the returned chunk descriptors are in a valid order is valid before using them.

NOTE: This function allocates memory using the allocator in NvBlastGlobals, to create the new chunk and bond
descriptor arrays referenced in the returned NvBlastAssetDesc.  The user must free this memory after use with
NVBLAST_FREE appied to the pointers in the returned NvBlastAssetDesc.

\param[in]  components          An array of assets to merge, of size componentCount.
\param[in]  scales              An array of scales to apply to the geometric data in the chunks and bonds.
                                If NULL, no scales are applied.  If not NULL, the array must be of size componentCount.
\param[in]  rotations           An array of rotations to apply to the geometric data in the chunks and bonds,
                                stored quaternion format. The quaternions MUST be normalized.  If NULL, no rotations are applied.
                                If not NULL, the array must be of size componentCount.
\param[in]  translations        An array of translations to apply to the geometric data in the chunks and bonds.
                                If NULL, no translations are applied.  If not NULL, the array must be of size componentCount.
\param[in]  componentCount      The size of the components and relativeTransforms arrays.
\param[in]  newBondDescs        Descriptors of type NvBlastExtAssetUtilsBondDesc for new bonds between components, of size newBondCount.  If NULL, newBondCount must be 0.
\param[in]  newBondCount        The size of the newBondDescs array.
\param[in]  chunkIndexOffsets   If not NULL, must point to a uin32_t array of size componentCount.  It will be filled with the starting elements in chunkReorderMap corresponding to
                                each component.
\param[in]  chunkReorderMap     If not NULL, the returned descriptor is run through NvBlastReorderAssetDescChunks, to ensure that it is a valid asset descriptor.  In the process, chunks
                                may be reordered (in addition to their natural re-indexing due to them all being placed in one array).  To map from the old chunk indexing for the various
                                component assets to the chunk indexing used in the returned descriptor, set chunkReorderMap to point to a uin32_t array of size equal to the total number
                                of chunks in all components, and pass in a non-NULL value to chunkIndexOffsets as described above.  Then, for component index c and chunk index k within
                                that component, the new chunk index is given by: index = chunkReorderMap[ k + chunkIndexOffsets[c] ].
\param[in]  chunkReorderMapSize The size of the array passed into chunkReorderMap, if chunkReorderMap is not NULL.  This is for safety, so that this function does not overwrite chunkReorderMap.

\return an asset descriptor that will build an asset which merges the components, using NvBlastCreateAsset.
*/
NV_C_API NvBlastAssetDesc NvBlastExtAssetUtilsMergeAssets
(
    const NvBlastAsset** components,
    const NvcVec3* scales,
    const NvcQuat* rotations,
    const NvcVec3* translations,
    uint32_t componentCount,
    const NvBlastExtAssetUtilsBondDesc* newBondDescs,
    uint32_t newBondCount,
    uint32_t* chunkIndexOffsets,
    uint32_t* chunkReorderMap,
    uint32_t chunkReorderMapSize
);


/**
Transforms asset in place using scale, rotation, transform. 
Chunk centroids, chunk bond centroids and bond normals are being transformed.
Chunk volume and bond area are changed accordingly.

\param[in, out] asset       Pointer to the asset to be transformed (modified).
\param[in]      scale       Pointer to scale to be applied. Can be nullptr.
\param[in]      rotation    Pointer to rotation to be applied. Can be nullptr.
\param[in]      translation Pointer to translation to be applied. Can be nullptr.
*/
NV_C_API void NvBlastExtAssetTransformInPlace
(
    NvBlastAsset* asset,
    const NvcVec3* scale,
    const NvcQuat* rotation,
    const NvcVec3* translation
);

#endif // ifndef NVBLASTEXTASSETUTILS_H
