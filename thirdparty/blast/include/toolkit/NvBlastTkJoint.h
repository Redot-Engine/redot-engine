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
//! @brief Defines the API for the NvBlastExtTkJoint class

#ifndef NVBLASTTKJOINT_H
#define NVBLASTTKJOINT_H

#include "NvBlastTkObject.h"

#include "NvVec3.h"


namespace Nv
{
namespace Blast
{

/**
The data contained in a TkJoint.
*/
struct TkJointData
{
    TkActor*        actors[2];          //!< The TkActor objects joined by the joint
    uint32_t        chunkIndices[2];    //!< The chunk indices within the corresponding TkActor objects joined by the joint.  The indexed chunks will be support chunks.
    nvidia::NvVec3  attachPositions[2]; //!< The position of the joint relative to each TkActor
};


/**
The TkJoint is may join two different TkActors, or be created internally within a single TkActor.

When a TkActor is created from a TkAsset with jointed bonds (the asset is created using a TkAssetDesc with joint flags on bonds, see TkActorDesc) then
internal TkJoint objects are created and associated with every TkActor created from that TkAsset.  The user only gets notification of the internal TkJoint
objects when the TkActor is split into separate TkActor objects that hold the support chunks joined by an internal TkJoint.

The user will be notified when the TkActor objects that are attached to TkJoint objects change, or are released.  In that case, a TkEvent with
a TkJointUpdateEvent payload is dispatched to TkEventListener objects registered with the TkFamily objects to which the actors belong.
*/
class TkJoint : public TkObject
{
public:
    /**
    Retrieve data in this joint.

    \return a TkJointData containing this joint's data.
    */
    virtual const TkJointData   getData() const = 0;
};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTTKJOINT_H
