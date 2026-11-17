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
//! @brief Defines the API for the NvBlastExtTkAsset class

#ifndef NVBLASTTKASSET_H
#define NVBLASTTKASSET_H

#include "NvBlastTkIdentifiable.h"
#include "NvBlastTypes.h"
#include "NvVec3.h"

// Forward declarations
struct NvBlastAsset;


namespace Nv
{
namespace Blast
{

/**
A descriptor stored by a TkAsset for an internal joint.  Internal joints are created when a TkAsset is instanced into a TkActor.
*/
struct TkAssetJointDesc
{
    uint32_t        nodeIndices[2];     //!< The graph node indices corresponding to the support chunks joined by a joint
    nvidia::NvVec3  attachPositions[2]; //!< The joint's attachment positions in asset-local space
};


/**
The static data associated with a destructible actor.  TkAsset encapsulates an NvBlastAsset.  In addition to the NvBlastAsset,
the TkAsset stores joint descriptors (see TkAssetJointDesc).
*/
class TkAsset : public TkIdentifiable
{
public:
    /**
    Access to underlying low-level asset.

    \return a pointer to the (const) low-level NvBlastAsset object.
    */
    virtual const NvBlastAsset*         getAssetLL() const = 0;

    /**
    Get the number of chunks in this asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetChunkCount for details.

    \return the number of chunks in the asset.
    */
    virtual uint32_t                    getChunkCount() const = 0;

    /**
    Get the number of leaf chunks in the given asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetLeafChunkCount for details.

    \return the number of leaf chunks in the asset.
    */
    virtual uint32_t                    getLeafChunkCount() const = 0;

    /**
    Get the number of bonds in the given asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetBondCount for details.

    \return the number of bonds in the asset.
    */
    virtual uint32_t                    getBondCount() const = 0;

    /**
    Access an array of chunks of the given asset.
        
    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetChunks for details.

    \return a pointer to an array of chunks of the asset.
    */
    virtual const NvBlastChunk*         getChunks() const = 0;

    /**
    Access an array of bonds of the given asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetBonds for details.

    \return a pointer to an array of bonds of the asset.
    */
    virtual const NvBlastBond*          getBonds() const = 0;

    /**
    Access an support graph for the given asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetSupportGraph for details.

    \return a struct of support graph for the given asset.
    */
    virtual const NvBlastSupportGraph   getGraph() const = 0;

    /**
    Retrieve the size (in bytes) of the LL asset.

    NOTE: Wrapper function over low-level function call, see NvBlastAssetGetSize for details.

    \return the size of the data block (in bytes).
    */
    virtual uint32_t                    getDataSize() const = 0;

    /**
    The number of internal TkJoint objects that will be created when this asset is instanced into a TkActor
    (see TkFramework::createActor).  These joints will not trigger TkJointUpdateEvent events
    until this actor is split into actors such that a joint connects two actors.  At this time the actor's family
    will dispatch a TkJointUpdateEvent::External event during a call to TkGroup::endProcess() (see TkGroup).

    \return the number of descriptors for internal joints.
    */
    virtual uint32_t                    getJointDescCount() const = 0;

    /**
    The descriptors for the internal joints created when this asset is instanced.  (See getJointDescCount.)

    \return a pointer to the array of descriptors for internal joints.
    */
    virtual const TkAssetJointDesc*     getJointDescs() const = 0;
};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTTKASSET_H
