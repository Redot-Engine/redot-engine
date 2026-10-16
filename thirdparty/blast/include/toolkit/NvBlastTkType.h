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


#ifndef NVBLASTTKTYPE_H
#define NVBLASTTKTYPE_H

#include "NvBlastTypes.h"

//! @file
//!
//! @brief Defines the API for the NvBlastExtTkType class

namespace Nv
{
namespace Blast
{

/**
Interface for static (class) type data.  This data is used for identification in streams,
class-specific object queries in TkFramework, etc.  Only classes derived from TkIdentifiable
use TkType data.
*/
class TkType
{
public:
    /**
    The class name.

    \return the class name.
    */
    virtual const char* getName() const = 0;

    /**
    The data format version for this class.  When deserializing, this version must match the
    current version.  If not, the user may convert the data format using the format conversion
    extension.

    \return the version number.
    */
    virtual uint32_t    getVersion() const = 0;

    /**
    Test for equality.  This type is used in static (per-class) data, so types are equal exactly
    when their addresses are equal.

    \param[in]  type    The TkType to compare with this TkType.

    \return true if this type equals the input type, false otherwise.
    */
    bool                operator == (const TkType& type) const
    {
        return &type == this;
    }
};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTTKTYPE_H
