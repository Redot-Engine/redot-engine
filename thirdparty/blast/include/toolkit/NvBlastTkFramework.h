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
//! @brief Defines the API for the NvBlastExtTkFramework class

#ifndef NVBLASTTKFRAMEWORK_H
#define NVBLASTTKFRAMEWORK_H


#include "NvBlastTkType.h"
#include "NvBlastTkEvent.h"

#include "NvPreprocessor.h"
#include "NvBlastTypes.h"

#include "NvVec3.h"


namespace Nv
{
namespace Blast
{

// Forward declarations
class TkObject;
class TkEventDispatcher;
class TkAsset;
struct TkGroupDesc;
class TkGroup;
class TkActor;
class TkJoint;
class TkIdentifiable;
struct TkAssetJointDesc;



/**
Descriptor for asset creation

Used to create a TkAsset.  This may be used by an authoring tool to describe the asset to be created.

The TkAssetDesc is a simple extension of the low-level NvBlastAsset descriptor, NvBlastAssetDesc.
*/
struct TkAssetDesc : public NvBlastAssetDesc
{
    /**
    Flags which may be associated with each bond described in the base NvBlastAssetDesc.
    */
    enum BondFlags
    {
        NoFlags = 0,

        /**
        If this flag is set then a TkJoint will be created joining the support chunks jointed by the bond.

        These joints will remain "quiet" until the actor is split in such a way that the joint joins two
        different actors.  In that case, a TkJointUpdateEvent will be dispatched with subtype External.
        (See TkJointUpdateEvent.)
        */
        BondJointed = (1 << 0)
    };

    /**
    An array of size bondCount, see BondFlags.
    If NULL, all flags are assumed to be NoFlags.
    */
    const uint8_t*  bondFlags;

    /** Constructor sets sane default values.  The zero chunkCount will cause TkFramework::createAsset(...) to fail, though gracefully. */
    TkAssetDesc() : bondFlags(nullptr)
    {
        chunkCount = bondCount = 0;
        chunkDescs = nullptr;
        bondDescs = nullptr;
    }
};


/**
Descriptor for actor creation.

The TkActorDesc is a simple extension of the low-level NvBlastActor descriptor, NvBlastActorDesc.
*/
struct TkActorDesc : public NvBlastActorDesc
{
    const TkAsset* asset;   //!< The TkAsset to instance

    /** Constructor sets sane default values */
    TkActorDesc(const TkAsset* inAsset = nullptr) : asset(inAsset)
    {
        uniformInitialBondHealth = uniformInitialLowerSupportChunkHealth = 1.0f;
        initialBondHealths = initialSupportChunkHealths = nullptr;
    }
};


/**
Descriptor for joint creation.
*/
struct TkJointDesc
{
    TkFamily*       families[2];        //!< The TkFamily objects containing the chunks joined by the joint
    uint32_t        chunkIndices[2];    //!< The chunk indices within the corresponding TkFamily objects joined by the joint.  The indexed chunks will be support chunks.
    nvidia::NvVec3  attachPositions[2]; //!< The position of the joint relative to each TkActor which owns the chunks jointed by this joint
};


/**
Struct-enum to index object types handled by the framework
*/
struct TkTypeIndex
{
    enum Enum
    {
        Asset = 0,  //!< TkAsset object type
        Family,     //!< TkFamily object type
        Group,      //!< TkGroup object type

        TypeCount
    };
};


/**
BlastTk Framework.

The framework exists as a singleton and is used to create objects, deserialize object streams, and hold references
to identified objects (TkAsset, TkFamily, and TkGroup) which may be recalled by their GUIDs.
*/
class TkFramework
{
public:
    /**
    Release this framework and all contained objects.
    Global singleton is set to NULL.
    */
    virtual void            release() = 0;

    /**
    To find the type information for a given TkIdentifiable-derived class, use this funtion with the TkTypeIndex::Enum
    corresponding to the desired class name.

    \param[in]  typeIndex   Enumerated object type (see TkTypeIndex).

    \return type object associated with the object's class.
    */
    virtual const TkType*   getType(TkTypeIndex::Enum typeIndex) const = 0;

    /**
    Look up an object derived from TkIdentifiable by its ID.

    \param[in]  id  The ID of the object to look up (see NvBlastID).

    \return pointer the object if it exists, NULL otherwise.
    */
    virtual TkIdentifiable* findObjectByID(const NvBlastID& id) const = 0;

    /**
    The number of TkIdentifiable-derived objects in the framework of the given type.

    \param[in]  type    The type object for the given type.

    \return the number of objects that currently exist of the given type.
    */
    virtual uint32_t        getObjectCount(const TkType& type) const = 0;

    /**
    Retrieve an array of pointers (into the user-supplied buffer) to TkIdentifiable-derived objects of the given type.

    \param[out] buffer      A user-supplied array of TkIdentifiable pointers.
    \param[in]  bufferSize  The number of elements available to write into buffer.
    \param[in]  type        The type object for the given type.
    \param[in]  indexStart  The starting index of the object.

    \return the number of TkIdentifiable pointers written to the buffer.
    */
    virtual uint32_t        getObjects(TkIdentifiable** buffer, uint32_t bufferSize, const TkType& type, uint32_t indexStart = 0) const = 0;

    //////// Asset creation ////////
    /**
    Helper function to build and apply chunk reorder map, so that chunk descriptors are properly ordered for the createAsset function.

    This is a convenience wrapper for the low-level NvBlastReorderAssetDescChunks function.

    This function may modify both the chunkDescs and bondDescs array, since rearranging chunk descriptors requires re-indexing within the bond descriptors.

    \param[in]  chunkDescs                  Array of chunk descriptors of size chunkCount. It will be updated accordingly.
    \param[in]  chunkCount                  The number of chunk descriptors.
    \param[in]  bondDescs                   Array of bond descriptors of size chunkCount. It will be updated accordingly.
    \param[in]  bondCount                   The number of bond descriptors.
    \param[in]  chunkReorderMap             If not NULL, must be a pointer to a uint32_t array of size desc.chunkCount.  Maps old chunk indices to the reordered chunk indices.
    \param[in]  keepBondNormalChunkOrder    If true, bond normals will be flipped if their chunk index order was reveresed by the reorder map.

    \return true iff the chunks did not require reordering (chunkReorderMap is the identity map).
    */
    virtual bool            reorderAssetDescChunks(NvBlastChunkDesc* chunkDescs, uint32_t chunkCount, NvBlastBondDesc* bondDescs, uint32_t bondCount, uint32_t* chunkReorderMap = nullptr, bool keepBondNormalChunkOrder = false) const = 0;

    /**
    Helper function to ensure (check and update) support coverage of chunks, required for asset creation via the createAsset function.

    This is a convenience wrapper for the low-level NvBlastEnsureAssetExactSupportCoverage function.

    The chunk descriptors may have their support flags be modified to ensure exact coverage.

    \param[in]  chunkDescs  An array of chunk descriptors.
    \param[in]  chunkCount  The size of the chunkDescs array.

    \return true iff coverage was already exact.
    */
    virtual bool            ensureAssetExactSupportCoverage(NvBlastChunkDesc* chunkDescs, uint32_t chunkCount) const = 0;

    /**
    Create an asset from the given descriptor.

    \param[in]  desc    The asset descriptor (see TkAssetDesc).

    \return the created asset, if the descriptor was valid and memory was available for the operation.  Otherwise, returns NULL.
    */
    virtual TkAsset*        createAsset(const TkAssetDesc& desc) = 0;

    /**
    Create an asset from a low-level NvBlastAsset.

    \param[in]  assetLL         The low-level NvBlastAsset to encapsulate.
    \param[in]  jointDescs      Optional joint descriptors to add to the new asset.
    \param[in]  jointDescCount  The number of joint descriptors in the jointDescs array.  If non-zero, jointDescs cannot be NULL.
    \param[in]  ownsAsset       Does this TkAsset own the NvBlastAsset and thus is responsible for freeing it.

    \return the created asset, if memory was available for the operation.  Otherwise, returns NULL.
    */
    virtual TkAsset*        createAsset(const NvBlastAsset* assetLL, Nv::Blast::TkAssetJointDesc* jointDescs = nullptr, uint32_t jointDescCount = 0, bool ownsAsset = false) = 0;

    //////// Group creation ////////
    /**
    Create a group from the given descriptor.  A group is a processing unit, to which the user may add TkActors.  New actors generated
    from splitting a TkActor are automatically put into the same group.  However, any actor may be removed from its group and optionally
    placed into another group, or left groupless.

    \param[in]  desc    The group descriptor (see TkGroupDesc).

    \return the created group, if the descriptor was valid and memory was available for the operation.  Otherwise, returns NULL.
    */
    virtual TkGroup*        createGroup(const TkGroupDesc& desc) = 0;

    //////// Actor creation ////////
    /**
    Create an actor from the given descriptor.  The actor will be the first member of a new TkFamily.

    \param[in]  desc    The actor descriptor (see TkActorDesc).

    \return the created actor, if the descriptor was valid and memory was available for the operation.  Otherwise, returns NULL.
    */
    virtual TkActor*        createActor(const TkActorDesc& desc) = 0;

    //////// Joint creation ////////
    /**
    Create a joint from the given descriptor.  The following restrictions apply:
    
    * Only one joint may be created between any two support chunks.

    * A joint cannot be created between chunks within the same actor using this method.  See TkAssetDesc for a description of
    bond joint flags, which will create internal joints within an actor.

    \param[in]  desc    The joint descriptor (see TkJointDesc).

    \return the created joint, if the descriptor was valid and memory was available for the operation.  Otherwise, returns NULL.
    */
    virtual TkJoint*        createJoint(const TkJointDesc& desc)  = 0;

protected:
    /**
    Destructor is virtual and not public - use the release() method instead of explicitly deleting the TkFramework
    */
    virtual                 ~TkFramework() {}
};

} // namespace Blast
} // namespace Nv


//////// Global API to Create and Access Framework ////////

/**
Create a new TkFramework.  This creates a global singleton, and will fail if a TkFramework object already exists.

\return the new TkFramework if successful, NULL otherwise.
*/
NV_C_API Nv::Blast::TkFramework* NvBlastTkFrameworkCreate();


/**
Retrieve a pointer to the global TkFramework singleton (if it exists).

\return the pointer to the global TkFramework (NULL if none exists).
*/
NV_C_API Nv::Blast::TkFramework* NvBlastTkFrameworkGet();


#endif // ifndef NVBLASTTKFRAMEWORK_H
