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
//! @brief Defines the API for the low-level blast library.

#ifndef NVBLAST_H
#define NVBLAST_H


#include "NvBlastTypes.h"


///////////////////////////////////////////////////////////////////////////////
//  NvBlastAsset functions
///////////////////////////////////////////////////////////////////////////////
///@{

/**
Calculates the memory requirements for an asset based upon its descriptor.
Use this function when building an asset with NvBlastCreateAsset.

\param[in] desc             Asset descriptor (see NvBlastAssetDesc).  Used to calculate node count.
\param[in] logFn            User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the memory size (in bytes) required for the asset, or zero if desc is invalid.
*/
NV_C_API size_t NvBlastGetAssetMemorySize(const NvBlastAssetDesc* desc, NvBlastLog logFn);

/**
Calculates the memory requirements for an asset based upon supplied sized data.
Used primarily with serialization.

\param[in] sizeData         Alternate form where all size data is already known.
\param[in] logFn            User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the memory size (in bytes) required for the asset, or zero if data is invalid.
*/
NV_C_API size_t NvBlastGetAssetMemorySizeFromSizeData(const NvBlastAssetMemSizeData& sizeData, NvBlastLog logFn);

/**
Returns the number of bytes of scratch memory that the user must supply to NvBlastCreateAsset,
based upon the descriptor that will be passed into that function.

\param[in] desc     The asset descriptor that will be passed into NvBlastCreateAsset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of bytes of scratch memory required for a call to NvBlastCreateAsset with that descriptor.
*/
NV_C_API size_t NvBlastGetRequiredScratchForCreateAsset(const NvBlastAssetDesc* desc, NvBlastLog logFn);


/**
Asset-building function.

Constructs an NvBlastAsset in-place at the address given by the user.  The address must point to a block
of memory of at least the size given by NvBlastGetAssetMemorySize(desc, logFn), and must be 16-byte aligned.

Support chunks (marked in the NvBlastChunkDesc struct) must provide full coverage over the asset.
This means that from any leaf chunk to the root node, exactly one chunk must be support.  If this condition
is not met the function fails to create an asset.

Any bonds described by NvBlastBondDesc descriptors that reference non-support chunks will be removed.  
Duplicate bonds will be removed as well (bonds that are between the same chunk pairs).

Chunks in the asset should be arranged such that sibling chunks (chunks with the same parent) are contiguous.
Chunks are also should be arranged such that leaf chunks (chunks with no children) are at the end of the chunk list.
If chunks aren't arranged properly the function fails to create an asset.

\param[in] mem      Pointer to block of memory of at least the size given by NvBlastGetAssetMemorySize(desc, logFn).  Must be 16-byte aligned.
\param[in] desc     Asset descriptor (see NvBlastAssetDesc).
\param[in] scratch  User-supplied scratch memory of size NvBlastGetRequiredScratchForCreateAsset(desc) bytes.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to new NvBlastAsset (will be the same address as mem), or NULL if unsuccessful.
*/
NV_C_API NvBlastAsset* NvBlastCreateAsset(void* mem, const NvBlastAssetDesc* desc, void* scratch, NvBlastLog logFn);


/**
Calculates the memory requirements for a family based upon an asset.
Use this function when building a family with NvBlastAssetCreateFamily.

\param[in] asset        Asset used to build the family (see NvBlastAsset).
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the memory size (in bytes) required for the family, or zero if asset is invalid.
*/
NV_C_API size_t NvBlastAssetGetFamilyMemorySize(const NvBlastAsset* asset, NvBlastLog logFn);

/**
Calculates the memory requirements for a family based upon supplied sized data.
Used primarily with serialization.

\param[in] sizeData     Alternate form where all size data is already known.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the memory size (in bytes) required for the family, or zero if data is invalid.
*/
NV_C_API size_t NvBlastAssetGetFamilyMemorySizeFromSizeData(const NvBlastAssetMemSizeData& sizeData, NvBlastLog logFn);


/**
Fill out the size data from the provided asset

\param[in] asset        Asset to pull the size data from (see NvBlastAsset).

\return Filled out size data struct.
*/
NV_C_API NvBlastAssetMemSizeData NvBlastAssetMemSizeDataFromAsset(const NvBlastAsset* asset);

/**
Family-building function.

Constructs an NvBlastFamily in-place at the address given by the user.  The address must point to a block
of memory of at least the size given by NvBlastAssetGetFamilyMemorySize(asset, logFn), and must be 16-byte aligned.

\param[in] mem          Pointer to block of memory of at least the size given by NvBlastAssetGetFamilyMemorySize(asset, logFn).  Must be 16-byte aligned.
\param[in] asset        Asset to instance.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the family.
*/
NV_C_API NvBlastFamily* NvBlastAssetCreateFamily(void* mem, const NvBlastAsset* asset, NvBlastLog logFn);

/**
Family-building function.

Constructs an NvBlastFamily in-place at the address given by the user.  The address must point to a block
of memory of at least the size given by NvBlastAssetGetFamilyMemorySize(sizeData, logFn), and must be 16-byte aligned.

\param[in] mem          Pointer to block of memory of at least the size given by NvBlastAssetGetFamilyMemorySize(asset, logFn).  Must be 16-byte aligned.
\param[in] sizeData     Data used to init buffer sizes.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the family.
*/
NV_C_API NvBlastFamily* NvBlastAssetCreateFamilyFromSizeData(void* mem, const NvBlastAssetMemSizeData& sizeData, NvBlastLog logFn);


/**
Retrieve the asset ID.

\param[in] asset    The given asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the ID of the asset.
*/
NV_C_API NvBlastID NvBlastAssetGetID(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Set an asset's ID

\param[in] asset    The given asset.
\param[in] id       A pointer to the id to copy into the asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true iff the id is successfully set.
*/
NV_C_API bool NvBlastAssetSetID(NvBlastAsset* asset, const NvBlastID* id, NvBlastLog logFn);


/**
Retrieve the data format version for the given asset

\param[in] asset    The asset.  Cannot be NULL.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the data format version (NvBlastAssetDataFormat).
*/
NV_C_API uint32_t NvBlastAssetGetFormatVersion(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Retrieve the memory size (in bytes) of the given data asset

\param[in] asset    The asset.  Cannot be NULL.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the memory size of the asset (in bytes).
*/
NV_C_API uint32_t NvBlastAssetGetSize(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Get the number of chunks in the given asset.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of chunks in the asset.
*/
NV_C_API uint32_t NvBlastAssetGetChunkCount(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Get the number of support chunks in the given asset.  This will equal the number of graph nodes in
NvBlastSupportGraph::nodeCount returned by NvBlastAssetGetSupportGraph only if no extra "external" node was created.
If such bonds were created, then an extra "external" graph node is added,
and this function will return NvBlastSupportGraph::nodeCount - 1.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of chunks in the asset.
*/
NV_C_API uint32_t NvBlastAssetGetSupportChunkCount(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Get the number of leaf chunks in the given asset.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of leaf chunks in the asset.
*/
NV_C_API uint32_t NvBlastAssetGetLeafChunkCount(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Get the first subsupport chunk index in the given asset.  Chunks are sorted such that subsupport chunks
come last.  This is the first subsupport chunk index.  Equals to total chunk count if there are no subsupport
chunks.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the first subsupport chunk index in the asset.
*/
NV_C_API uint32_t NvBlastAssetGetFirstSubsupportChunkIndex(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Get the number of bonds in the given asset.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of bonds in the asset.
*/
NV_C_API uint32_t NvBlastAssetGetBondCount(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Access the support graph for the given asset.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return a struct of support graph for the given asset.
*/
NV_C_API const NvBlastSupportGraph NvBlastAssetGetSupportGraph(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Access a map from chunk index to graph node index.
Returned map is valid in the domain [0, NvBlastAssetGetChunkCount(asset, logFn)).
Non-support chunks are mapped to the invalid index 0xFFFFFFFF.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return an array of uint32_t values defining the map, of size NvBlastAssetGetChunkCount(asset, logFn).
*/
NV_C_API const uint32_t* NvBlastAssetGetChunkToGraphNodeMap(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Access an array of chunks of the given asset.

\param[in] asset    The asset.
\param[in] logFn    User - supplied message function(see NvBlastLog definition).May be NULL.

\return a pointer to an array of chunks of the asset.
*/
NV_C_API const NvBlastChunk* NvBlastAssetGetChunks(const NvBlastAsset* asset, NvBlastLog logFn);


/**
Access an array of bonds of the given asset.

\param[in] asset    The asset.
\param[in] logFn    User - supplied message function(see NvBlastLog definition).May be NULL.

\return a pointer to an array of bonds of the asset.
*/
NV_C_API const NvBlastBond* NvBlastAssetGetBonds(const NvBlastAsset* asset, NvBlastLog logFn);


/**
A buffer size sufficient to serialize an actor instanced from a given asset.
This function is faster than NvBlastActorGetSerializationSize, and can be used to create a reusable buffer
for actor serialization.

\param[in] asset    The asset.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the required buffer size in bytes.
*/
NV_C_API uint32_t NvBlastAssetGetActorSerializationSizeUpperBound(const NvBlastAsset* asset, NvBlastLog logFn);

///@} End NvBlastAsset functions


///////////////////////////////////////////////////////////////////////////////
//  NvBlastAsset helper functions
///////////////////////////////////////////////////////////////////////////////
///@{

/**
Function to ensure (check and update) support coverage of chunks.

Support chunks (marked in the NvBlastChunkDesc struct) must provide full coverage over the asset.
This means that from any leaf chunk to the root node, exactly one chunk must be support.  If this condition
is not met, the actual support chunks will be adjusted accordingly.

Chunk order depends on support coverage, so this function should be called before chunk reordering.

\param[in]  chunkDescs          Array of chunk descriptors of size chunkCount. It will be updated accordingly.
\param[in]  chunkCount          The number of chunk descriptors.
\param[in]  scratch             User-supplied scratch storage, must point to chunkCount valid bytes of memory.
\param[in]  logFn               User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true iff coverage was already exact.
*/
NV_C_API bool NvBlastEnsureAssetExactSupportCoverage(NvBlastChunkDesc* chunkDescs, uint32_t chunkCount, void* scratch, NvBlastLog logFn);


/**
Build chunk reorder map. 

NvBlastCreateAsset function requires NvBlastChunkDesc array to be in correct oder:

1. Root chunks (chunks with invalid parent index) must be first in the asset's chunk list.
2. Chunks in the asset must be arranged such that sibling chunks (chunks with the same parent) are contiguous.
3. Chunks must be arranged such that upper-support chunks (support chunks and their parent chunks) go first in 
chunk list.

This function builds chunk reorder map which can be used to order chunk descs. Reordering chunk's descriptors 
according to generated map places them in correct order for NvBlastCreateAsset to succeed.

Iff chunks are already ordered correctly, function returns 'true' and identity chunk reorder map. Otherwise 'false' is returned.

\param[out] chunkReorderMap User-supplied map of size chunkCount to fill. For every chunk index this array will contain new chunk position (index).
\param[in]  chunkDescs      Array of chunk descriptors of size chunkCount.
\param[in]  chunkCount      The number of chunk descriptors.
\param[in]  scratch         User-supplied scratch storage, must point to 3 * chunkCount * sizeof(uint32_t) valid bytes of memory.
\param[in]  logFn           User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true iff the chunks did not require reordering (chunkReorderMap is the identity map).
*/
NV_C_API bool NvBlastBuildAssetDescChunkReorderMap(uint32_t* chunkReorderMap, const NvBlastChunkDesc* chunkDescs, uint32_t chunkCount, void* scratch, NvBlastLog logFn);


/**
Apply chunk reorder map.

Function applies reorder map on NvBlastChunkDesc and NvBlastBondDesc arrays. It reorders chunks, replaces their 'parentChunkIndex' field
with new indices. Bonds are kept in the same order, but their 'chunkIndices' field is updated with proper indices.

@see NvBlastBuildAssetDescChunkReorderMap

\param[out] reorderedChunkDescs         User-supplied array of size chunkCount to fill with new reordered NvBlastChunkDesc's.
\param[in]  chunkDescs                  Array of chunk descriptors of size chunkCount.
\param[in]  chunkCount                  The number of chunk descriptors.
\param[in]  bondDescs                   Array of bond descriptors of size chunkCount. It will be updated accordingly.
\param[in]  bondCount                   The number of bond descriptors.
\param[in]  chunkReorderMap             Chunk reorder map to use, must be of size chunkCount.
\param[in]  keepBondNormalChunkOrder    If true, bond normals will be flipped if their chunk index order was reveresed by the reorder map.
\param[in]  logFn                       User-supplied message function (see NvBlastLog definition).  May be NULL.
*/
NV_C_API void NvBlastApplyAssetDescChunkReorderMap
(
    NvBlastChunkDesc* reorderedChunkDescs,
    const NvBlastChunkDesc* chunkDescs,
    uint32_t chunkCount,
    NvBlastBondDesc* bondDescs,
    uint32_t bondCount,
    const uint32_t* chunkReorderMap,
    bool keepBondNormalChunkOrder,
    NvBlastLog logFn
);


/**
Apply chunk reorder map.

Function applies reorder map on NvBlastChunkDesc and NvBlastBondDesc arrays. It reorders chunks, replaces their 'parentChunkIndex' field
with new indices. Bonds are kept in the same order, but their 'chunkIndices' field is updated with proper indices.

This overload of function reorders chunks in place.

@see NvBlastBuildAssetDescChunkReorderMap

\param[in]  chunkDescs                  Array of chunk descriptors of size chunkCount. It will be updated accordingly.
\param[in]  chunkCount                  The number of chunk descriptors.
\param[in]  bondDescs                   Array of bond descriptors of size chunkCount. It will be updated accordingly.
\param[in]  bondCount                   The number of bond descriptors.
\param[in]  chunkReorderMap             Chunk reorder map to use, must be of size chunkCount.
\param[in]  keepBondNormalChunkOrder    If true, bond normals will be flipped if their chunk index order was reveresed by the reorder map.
\param[in]  scratch                     User-supplied scratch storage, must point to chunkCount * sizeof(NvBlastChunkDesc) valid bytes of memory.
\param[in]  logFn                       User-supplied message function (see NvBlastLog definition).  May be NULL.
*/
NV_C_API void NvBlastApplyAssetDescChunkReorderMapInPlace
(
    NvBlastChunkDesc* chunkDescs,
    uint32_t chunkCount,
    NvBlastBondDesc* bondDescs,
    uint32_t bondCount,
    const uint32_t* chunkReorderMap,
    bool keepBondNormalChunkOrder,
    void* scratch,
    NvBlastLog logFn
);


/**
Build and apply chunk reorder map. 

Function basically calls NvBlastBuildAssetDescChunkReorderMap and NvBlastApplyAssetDescChunkReorderMap. Used for Convenience.

\param[in]  chunkDescs                  Array of chunk descriptors of size chunkCount. It will be updated accordingly.
\param[in]  chunkCount                  The number of chunk descriptors.
\param[in]  bondDescs                   Array of bond descriptors of size chunkCount. It will be updated accordingly.
\param[in]  bondCount                   The number of bond descriptors.
\param[in]  chunkReorderMap             Chunk reorder map to fill, must be of size chunkCount.
\param[in]  keepBondNormalChunkOrder    If true, bond normals will be flipped if their chunk index order was reveresed by the reorder map.
\param[in]  scratch                     User-supplied scratch storage, must point to chunkCount * sizeof(NvBlastChunkDesc) valid bytes of memory.
\param[in]  logFn                       User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true iff the chunks did not require reordering (chunkReorderMap is the identity map).
*/
NV_C_API bool NvBlastReorderAssetDescChunks
(
    NvBlastChunkDesc* chunkDescs,
    uint32_t chunkCount,
    NvBlastBondDesc* bondDescs,
    uint32_t bondCount,
    uint32_t* chunkReorderMap,
    bool keepBondNormalChunkOrder,
    void* scratch,
    NvBlastLog logFn
);

///@} End NvBlastAsset helper functions


///////////////////////////////////////////////////////////////////////////////
//  NvBlastFamily functions
///////////////////////////////////////////////////////////////////////////////
///@{

/**
Retrieve the data format version for the given family.

\param[in] family   The family.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the family format version.
*/
NV_C_API uint32_t NvBlastFamilyGetFormatVersion(const NvBlastFamily* family, NvBlastLog logFn);


/**
Retrieve the asset of the given family.

\param[in] family   The family.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to the asset associated with the family.
*/
NV_C_API const NvBlastAsset* NvBlastFamilyGetAsset(const NvBlastFamily* family, NvBlastLog logFn);


/**
Set asset to the family. It should be the same asset as the one family was created from (same ID).

\param[in] family   The family.
\param[in] asset    Asset to instance.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.
*/
NV_C_API void NvBlastFamilySetAsset(NvBlastFamily* family, const NvBlastAsset* asset, NvBlastLog logFn);


/**
Retrieve the size (in bytes) of the given family.

\param[in] family   The family.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the size of the family (in bytes).
*/
NV_C_API uint32_t NvBlastFamilyGetSize(const NvBlastFamily* family, NvBlastLog logFn);


/**
Retrieve the asset ID of the given family.

\param[in] family   The family.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the ID of the asset associated with the family.
*/
NV_C_API NvBlastID NvBlastFamilyGetAssetID(const NvBlastFamily* family, NvBlastLog logFn);


/**
Returns the number of bytes of scratch memory that the user must supply to NvBlastFamilyCreateFirstActor.

\param[in] family   The family from which the first actor will be instanced.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of bytes of scratch memory required for a call to NvBlastFamilyCreateFirstActor.
*/
NV_C_API size_t NvBlastFamilyGetRequiredScratchForCreateFirstActor(const NvBlastFamily* family, NvBlastLog logFn);


/**
Instance the family's asset into a new, unfractured actor.

\param[in] family   Family in which to create a new actor.  The family must have no other actors in it.  (See NvBlastAssetCreateFamily.)
\param[in] desc     Actor descriptor (see NvBlastActorDesc).
\param[in] scratch  User-supplied scratch memory of size NvBlastFamilyGetRequiredScratchForCreateFirstActor(asset) bytes, where 'asset' is the NvBlastAsset from which the family was created.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to new NvBlastActor if successful (the actor was successfully inserted into the family), or NULL if unsuccessful.
*/
NV_C_API NvBlastActor* NvBlastFamilyCreateFirstActor(NvBlastFamily* family, const NvBlastActorDesc* desc, void* scratch, NvBlastLog logFn);


/**
Retrieve the number of active actors associated with the given family.

\param[in] family   The family.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of active actors in the family.
*/
NV_C_API uint32_t NvBlastFamilyGetActorCount(const NvBlastFamily* family, NvBlastLog logFn);


/**
Deserialize a single Actor from a buffer into the given family.  The actor will be inserted if it
is compatible with the current family state.  That is, it must not share any chunks or internal
IDs with the actors already present in the family.

\param[in] family   Family in which to deserialize the actor.
\param[in] buffer   User-supplied buffer containing the actor to deserialize.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the deserialized actor if successful, NULL otherwise.
*/
NV_C_API NvBlastActor* NvBlastFamilyDeserializeActor(NvBlastFamily* family, const void* buffer, NvBlastLog logFn);


/**
Retrieve the active actors associated with the given family.

\param[out] actors      User-supplied array to be filled with the returned actor pointers.
\param[out] actorsSize  The size of the actors array.  To receive all actor pointers, the size must be at least that given by NvBlastFamilyGetActorCount(family).
\param[in] family       The family.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of actor pointers written to actors.  This will not exceed actorsSize.
*/
NV_C_API uint32_t NvBlastFamilyGetActors(NvBlastActor** actors, uint32_t actorsSize, const NvBlastFamily* family, NvBlastLog logFn);


/**
Retrieve the actor associated with the given actor index.

\param[in] family       The family.
\param[in] actorIndex   The index of actor.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to actor associated with given actor index.  NULL if there is no such actor or it is inactive.
*/
NV_C_API NvBlastActor* NvBlastFamilyGetActorByIndex(const NvBlastFamily* family, uint32_t actorIndex, NvBlastLog logFn);

/**
Retrieve the actor associated with the given chunk.

\param[in] family       The family.
\param[in] chunkIndex   The index of chunk.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to actor associated with given chunk.  NULL if there is no such actor.
*/
NV_C_API NvBlastActor* NvBlastFamilyGetChunkActor(const NvBlastFamily* family, uint32_t chunkIndex, NvBlastLog logFn);


/**
Retrieve the actor indices associated with chunks.
NOTE: the returned array size equals the number of support chunks in the asset.

\param[in] family       The family.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return pointer to actor associated with given chunk.  NULL if there is no such actor.
*/
NV_C_API uint32_t* NvBlastFamilyGetChunkActorIndices(const NvBlastFamily* family, NvBlastLog logFn);


/**
Retrieve the max active actor count family could have.

\param[in] family       The family.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the max number of active actors family could have.
*/
NV_C_API uint32_t NvBlastFamilyGetMaxActorCount(const NvBlastFamily* family, NvBlastLog logFn);

///@} End NvBlastFamily functions


///////////////////////////////////////////////////////////////////////////////////////
//  NvBlastActor accessor, serialization, and deactivation functions
///////////////////////////////////////////////////////////////////////////////////////
///@{

/**
Get the number of visible chunks for this actor.  May be used in conjunction with NvBlastActorGetVisibleChunkIndices.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of visible chunk indices for the actor.
*/
NV_C_API uint32_t NvBlastActorGetVisibleChunkCount(const NvBlastActor* actor, NvBlastLog logFn);


/**
Retrieve a list of visible chunk indices for the actor into the given array.

\param[in] visibleChunkIndices      User-supplied array to be filled in with indices of visible chunks for this actor.
\param[in] visibleChunkIndicesSize  The size of the visibleChunkIndices array.  To receive all visible chunk indices, the size must be at least that given by NvBlastActorGetVisibleChunkCount(actor).
\param[in] actor                    The actor.
\param[in] logFn                    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of indices written to visibleChunkIndices.  This will not exceed visibleChunkIndicesSize.
*/
NV_C_API uint32_t NvBlastActorGetVisibleChunkIndices(uint32_t* visibleChunkIndices, uint32_t visibleChunkIndicesSize, const NvBlastActor* actor, NvBlastLog logFn);


/**
Get the number of graph nodes for this actor.  May be used in conjunction with NvBlastActorGetGraphNodeIndices.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of graph node indices for the actor.
*/
NV_C_API uint32_t NvBlastActorGetGraphNodeCount(const NvBlastActor* actor, NvBlastLog logFn);


/**
Retrieve a list of graph node indices for the actor into the given array.

\param[in] graphNodeIndices     User-supplied array to be filled in with indices of graph nodes for this actor.
\param[in] graphNodeIndicesSize The size of the graphNodeIndices array.  To receive all graph node indices, the size must be at least that given by NvBlastActorGetGraphNodeCount(actor).
\param[in] actor                The actor.
\param[in] logFn                User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of indices written to graphNodeIndices.  This will not exceed graphNodeIndicesSize.
*/
NV_C_API uint32_t NvBlastActorGetGraphNodeIndices(uint32_t* graphNodeIndices, uint32_t graphNodeIndicesSize, const NvBlastActor* actor, NvBlastLog logFn);


/**
Access the bond health data for an actor.

This function returns a pointer to the head of an array of bond healths (floats).  This array is the same for any actor that
has been created from repeated fracturing of the same original instance of an asset (in the same instance family).

The indices obtained from NvBlastSupportGraph::adjacentBondIndices in the asset may be used to access this array.

The size of the array returned is NvBlastAssetGetBondCount(asset, logFn), where 'asset' is the NvBlastAsset
that was used to create the actor.

This array is valid as long as any actor in the instance family for the input actor exists.

If the input actor is invalid, NULL will be returned.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the array of bond healths for the actor's instance family, or NULL if the actor is invalid.
*/
NV_C_API const float* NvBlastActorGetBondHealths(const NvBlastActor* actor, NvBlastLog logFn);


/**
Access the cached bond health data for an actor.  It is intended to be populated with pre-damage health values.

This function returns a pointer to the head of an array of bond healths (floats).  This array is the same for any actor that
has been created from repeated fracturing of the same original instance of an asset (in the same instance family).

The indices obtained from NvBlastSupportGraph::adjacentBondIndices in the asset may be used to access this array.

The size of the array returned is NvBlastAssetGetBondCount(asset, logFn), where 'asset' is the NvBlastAsset
that was used to create the actor.

This array is valid as long as any actor in the instance family for the input actor exists.

If the input actor is invalid, NULL will be returned.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the array of bond healths for the actor's instance family, or NULL if the actor is invalid.
*/
NV_C_API const float* NvBlastActorGetCachedBondHeaths(const NvBlastActor* actor, NvBlastLog logFn);


/**
Tell the system to cache the bond health for the given bond index.

\param[in] actor        The actor.
\param[in] bondIndex    The bond to cache the health value.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true if value was cached, false otherwise
 */
NV_C_API bool NvBlastActorCacheBondHeath(const NvBlastActor* actor, uint32_t bondIndex, NvBlastLog logFn);


/**
The buffer size needed to serialize a single actor.  This will give the exact size needed.  For an upper bound
on the buffer size needed for any actor instanced from an NvBlastAsset, use NvBlastAssetGetActorSerializationSizeUpperBound.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the required buffer size in bytes.
*/
NV_C_API uint32_t NvBlastActorGetSerializationSize(const NvBlastActor* actor, NvBlastLog logFn);


/**
Serialize a single actor to a buffer.

\param[out] buffer      User-supplied buffer, must be at least of size given by NvBlastActorGetSerializationSize(actor).
\param[in] bufferSize   The size of the user-supplied buffer.
\param[in] actor        The actor.
\param[in] logFn        User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of bytes written to the buffer, or 0 if there is an error (such as an under-sized buffer).
*/
NV_C_API uint32_t NvBlastActorSerialize(void* buffer, uint32_t bufferSize, const NvBlastActor* actor, NvBlastLog logFn);


/**
Access to an actor's family.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the family with which the actor is associated.
*/
NV_C_API NvBlastFamily* NvBlastActorGetFamily(const NvBlastActor* actor, NvBlastLog logFn);


/**
Access to an actor's internal index.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return actor's internal index in family.
*/
NV_C_API uint32_t NvBlastActorGetIndex(const NvBlastActor* actor, NvBlastLog logFn);


/**
Deactivate an actor within its family.  Conceptually this is "destroying" the actor, however memory will not be released until the family is released.

\param[in] actor    Points to a user-supplied actor struct.  May be NULL, in which case this function no-ops.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true iff successful (actor was active).
*/
NV_C_API bool NvBlastActorDeactivate(NvBlastActor* actor, NvBlastLog logFn);

///@} End NvBlastActor accessor, serialization, and deactivation functions


///////////////////////////////////////////////////////////////////////////////
//  NvBlastActor damage and fracturing functions
///////////////////////////////////////////////////////////////////////////////
///@{

/**
Creates fracture commands for the actor using a damage program and program parameters (material and damage descriptions).

\param[in,out]  commandBuffers      Target buffers to hold generated commands.
                                    To avoid data loss, provide an entry for every support chunk and every bond in the original actor.
\param[in]      actor               The NvBlastActor to create fracture commands for.
\param[in]      program             A NvBlastDamageProgram containing damage shaders.
\param[in]      programParams       Parameters for the NvBlastDamageProgram.
\param[in]      logFn               User-supplied message function (see NvBlastLog definition).  May be NULL.
\param[in,out]  timers              If non-NULL this struct will be filled out with profiling information for the step, in profile build configurations.

Interpretation of NvBlastFractureBuffers:
As input:
    Counters denote available entries for FractureData.
    Chunk and Bond userdata are not used.
    Health values are not used.

As output:
    Counters denote valid entires in FractureData arrays.
    Chunks and Bond userdata reflect the respective userdata set during asset initialization, where implemented by the material function.
    Health values denote how much damage is to be applied.
*/
NV_C_API void NvBlastActorGenerateFracture
(
    NvBlastFractureBuffers* commandBuffers,
    const NvBlastActor* actor,
    const NvBlastDamageProgram program, 
    const void* programParams,
    NvBlastLog logFn, 
    NvBlastTimers* timers
);


/**
Applies the direct fracture and breaks graph bonds/edges as necessary.
Chunks damaged beyond their respective health fracture their children recursively, creating a NvBlastChunkFractureData for each.

\param[in,out]  eventBuffers        Target buffers to hold applied fracture events. May be NULL, in which case events are not reported.
                                    To avoid data loss, provide an entry for every lower-support chunk and every bond in the original actor.
\param[in,out]  actor               The NvBlastActor to apply fracture to.
\param[in]      commands            The fracture commands to process.
\param[in]      logFn               User-supplied message function (see NvBlastLog definition).  May be NULL.
\param[in,out]  timers              If non-NULL this struct will be filled out with profiling information for the step, in profile build configurations.

Interpretation of NvBlastFractureBuffers:
commands:
    Counters denote the number of command entries to process.
    Chunk and Bond userdata are not used.
    Health values denote the amount of damage to apply, as a positive value.

eventBuffers as input:
    Counters denote available entries for FractureData.
    Chunk and Bond userdata are not used.
    Health values are not used.

eventBuffers as output:
    Counters denote valid entires in FractureData arrays.
    Chunks and Bond userdata reflect the respective userdata set during asset initialization.
    Health values denote how much health is remaining for the damaged element. 
    Broken elements report a negative value corresponding to the superfluous health damage.

commands and eventBuffers may point to the same memory.
*/
NV_C_API void NvBlastActorApplyFracture
(
    NvBlastFractureBuffers* eventBuffers,
    NvBlastActor* actor,
    const NvBlastFractureBuffers* commands,
    NvBlastLog logFn,
    NvBlastTimers* timers
);


/**
Releases the oldActor and creates its children newActors if necessary.

\param[out]     result              The list of deleted and created NvBlastActor objects.
\param[in]      actor               The actor to split.
\param[in]      newActorsMaxCount   Number of available NvBlastActor slots. In the worst case, one NvBlastActor may be created for every chunk in the asset.
\param[in]      scratch             Scratch Memory used during processing. NvBlastActorGetRequiredScratchForSplit provides the necessary size.
\param[in]      logFn               User-supplied message function (see NvBlastLog definition).  May be NULL.
\param[in,out]  timers              If non-NULL this struct will be filled out with profiling information for the step, in profile build configurations

\return 1..n:   new actors were created
\return 0:      oldActor is unchanged
*/
NV_C_API uint32_t NvBlastActorSplit
(
    NvBlastActorSplitEvent* result, 
    NvBlastActor* actor,
    uint32_t newActorsMaxCount,
    void* scratch,
    NvBlastLog logFn,
    NvBlastTimers* timers
);


/**
Returns the number of bytes of scratch memory that the user must supply to NvBlastActorSplit,
based upon the actor that will be passed into that function.

\param[in] actor    The actor that will be passed into NvBlastActorSplit.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the number of bytes of scratch memory required for a call to NvBlastActorSplit with that actor.
*/
NV_C_API size_t NvBlastActorGetRequiredScratchForSplit(const NvBlastActor* actor, NvBlastLog logFn);


/**
Returns the upper-bound number of actors which can be created by calling NvBlastActorSplit with that actor, this
value can't exceed chunk count.

\param[in] actor    The actor.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return the upper-bound number of actors which can be created by calling NvBlastActorSplit with that actor.
*/
NV_C_API uint32_t NvBlastActorGetMaxActorCountForSplit(const NvBlastActor* actor, NvBlastLog logFn);


/**
Determines if the actor can fracture further.

\param[in] actor    The actor potentially being fractured.
\param[in] logFn    User-supplied message function (see NvBlastLog definition).  May be NULL.

\return true if any result can be expected from fracturing the actor. false if no further change to the actor is possible.
*/
NV_C_API bool NvBlastActorCanFracture(const NvBlastActor* actor, NvBlastLog logFn);


/**
Determines if the actor is damaged (was fractured) and split call is required.

The actor could be damaged by calling NvBlastActorApplyFracture and NvBlastActorSplit is expected after.
This function gives a hint that NvBlastActorSplit will have some work to be done and actor could potentially be split.
If actor is not damaged calling NvBlastActorSplit will make no effect.

\return true iff split call is required for this actor.
*/
NV_C_API bool NvBlastActorIsSplitRequired(const NvBlastActor* actor, NvBlastLog logFn);


/**
\return true iff this actor contains the "external" support graph node, created when a bond contains the UINT32_MAX value for one of their chunkIndices.
*/
NV_C_API bool NvBlastActorHasExternalBonds(const NvBlastActor* actor, NvBlastLog logFn);

// DEPRICATED: remove on next major version bump
#define NvBlastActorIsBoundToWorld NvBlastActorHasExternalBonds

///@} End NvBlastActor damage and fracturing functions


///////////////////////////////////////////////////////////////////////////////
//  NvBlastTimers functions and helpers
///////////////////////////////////////////////////////////////////////////////
///@{

/**
Resets all values in the given NvBlastTimers struct to zero.

\param[in]  timers  The NvBlastTimers to set to zero.
*/
NV_C_API void NvBlastTimersReset(NvBlastTimers* timers);


/**
Convert a tick value from NvBlastTimers to seconds.

\param[in]  ticks   The tick value.

\return the seconds correposnding to the input tick value.
*/
NV_C_API double NvBlastTicksToSeconds(int64_t ticks);

///@} End NvBlastTimers functions and helpers


#endif // ifndef NVBLAST_H
