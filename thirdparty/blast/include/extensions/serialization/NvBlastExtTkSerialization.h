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
// Copyright (c) 2022-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines blast toolkit (Tk) serialization support for the NvBlastExtSerialization blast extension

#pragma once

#include "NvBlastGlobals.h"


/**
Blast High-level serialization support.  Contains serializers which can be used by the ExtSerialization manager.
*/


namespace Nv
{
namespace Blast
{

// Forward declarations
class ExtSerialization;
class TkFramework;
class TkAsset;


/** Standard Object Type IDs */
struct TkObjectTypeID
{
    enum Enum
    {
        Asset = NVBLAST_FOURCC('T', 'K', 'A', 'S'),
    };
};

}   // namespace Blast
}   // namespace Nv


/**
Load all high-level serializers into the ExtSerialization manager.

It does no harm to call this function more than once; serializers already loaded will not be loaded again.

\param[in]  serialization   Serialization manager into which to load serializers.

\return the number of serializers loaded.
*/
NV_C_API size_t      NvBlastExtTkSerializerLoadSet(Nv::Blast::TkFramework& framework, Nv::Blast::ExtSerialization& serialization);


/**
Utility wrapper function to serialize a TkAsset.  Allocates the buffer internally using the
callack set in ExtSerialization::setBufferProvider.

Equivalent to:

    serialization.serializeIntoBuffer(buffer, asset, Nv::Blast::TkObjectTypeID::Asset);

\param[out] buffer          Pointer to the buffer created.
\param[in]  serialization   Serialization manager.
\param[in]  asset           Pointer to the TkAsset to serialize.

\return the number of bytes serialized into the buffer (zero if unsuccessful).
*/
NV_C_API uint64_t    NvBlastExtSerializationSerializeTkAssetIntoBuffer(void*& buffer, Nv::Blast::ExtSerialization& serialization, const Nv::Blast::TkAsset* asset);
