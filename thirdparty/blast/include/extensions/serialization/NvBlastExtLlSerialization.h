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
//! @brief Defines low-level serialization support for the NvBlastExtSerialization blast extension

#pragma once

#include "NvBlastGlobals.h"


/**
Blast Low-level serialization support.  Contains serializers which can be used by the ExtSerialization manager.
*/


// Forward declarations
struct NvBlastAsset;
struct NvBlastFamily;


namespace Nv
{
namespace Blast
{

// Forward declarations
class ExtSerialization;


/** Standard Object Type IDs */
struct LlObjectTypeID
{
    enum Enum
    {
        Asset =     NVBLAST_FOURCC('L', 'L', 'A', 'S'),
        Family =    NVBLAST_FOURCC('L', 'L', 'F', 'A'),
    };
};

}   // namespace Blast
}   // namespace Nv


/**
Load all low-level serializers into the ExtSerialization manager.  *N.B.* This is done automatically when
the ExtSerialization manager is created via NvBlastExtSerializationCreate(), so currently this public function
is unnecessary.  Note also that other modules' serializers (e.g. ExtTkSerialization) are _not_ loaded
automatically, and need to be explicitly loaded by the user using their respective load functions.

It does no harm to call this function more than once; serializers already loaded will not be loaded again.

\param[in]  serialization   Serialization manager into which to load serializers.

\return the number of serializers loaded.
*/
NV_C_API size_t      NvBlastExtLlSerializerLoadSet(Nv::Blast::ExtSerialization& serialization);


/**
Utility wrapper function to serialize an NvBlastAsset.  Allocates the buffer internally using the
callack set in ExtSerialization::setBufferProvider.

Equivalent to:

    serialization.serializeIntoBuffer(buffer, asset, Nv::Blast::LlObjectTypeID::Asset);

\param[out] buffer          Pointer to the buffer created.
\param[in]  serialization   Serialization manager.
\param[in]  asset           Pointer to the NvBlastAsset to serialize.

\return the number of bytes serialized into the buffer (zero if unsuccessful).
*/
NV_C_API uint64_t    NvBlastExtSerializationSerializeAssetIntoBuffer(void*& buffer, Nv::Blast::ExtSerialization& serialization, const NvBlastAsset* asset);


/**
Utility wrapper function to serialize an NvBlastFamily.  Allocates the buffer internally using the
callack set in ExtSerialization::setBufferProvider.

Equivalent to:

    serialization.serializeIntoBuffer(buffer, family, Nv::Blast::LlObjectTypeID::Family);

\param[out] buffer          Pointer to the buffer created.
\param[in]  serialization   Serialization manager.
\param[in]  family          Pointer to the NvBlastFamily to serialize.

\return the number of bytes serialized into the buffer (zero if unsuccessful).
*/
NV_C_API uint64_t    NvBlastExtSerializationSerializeFamilyIntoBuffer(void*& buffer, Nv::Blast::ExtSerialization& serialization, const NvBlastFamily* family);
