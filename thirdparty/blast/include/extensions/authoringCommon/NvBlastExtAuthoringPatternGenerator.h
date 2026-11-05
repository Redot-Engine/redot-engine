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
//! @brief Defines the PatternGenerator API used by the authoring tools, allowing the user to create fracture patterns

#ifndef NVBLASTEXTAUTHORINGPATTERNGENERATOR_H
#define NVBLASTEXTAUTHORINGPATTERNGENERATOR_H

#include "NvBlastGlobals.h"

namespace Nv
{
    namespace Blast
    {
        typedef float (*RNG_CALLBACK)(void);

        struct PatternDescriptor
        {
            RNG_CALLBACK RNG = nullptr;

            uint32_t interiorMaterialId = 1000;
        };

        struct UniformPatternDesc : public PatternDescriptor
        {
            uint32_t cellsCount = 2;

            float radiusMin     = 0.0f;
            float radiusMax     = 1.0f;
            float radiusDistr   = 1.0f;

            float debrisRadiusMult = 1.0f;
        };

        struct BeamPatternDesc : public PatternDescriptor
        {
            uint32_t cellsCount;

            float radiusMin;
            float radiusMax;
        };

        struct RegularRadialPatternDesc : public PatternDescriptor
        {
            float radiusMin = 0.0f;
            float radiusMax = 1.0f;
            
            uint32_t radialSteps = 3;
            uint32_t angularSteps = 8;

            float aperture = .0f;

            float angularNoiseAmplitude = 0.0f;
            
            float radialNoiseAmplitude = 0.0f;
            float radialNoiseFrequency = 0.0f;

            float debrisRadiusMult = 1.0f;
        };


        struct DamagePattern
        {
            /**
            Used to compute activated chunks.
            */
            float activationRadius;
            float angle; // For cone shape activation
            enum ActivationDistanceType
            {
                Point = 0,
                Line,
                Cone
            };
            ActivationDistanceType activationType = Point;
            // ----------------------------------------------


            uint32_t cellsCount;
            class Mesh** cellsMeshes = nullptr;

            virtual void release() = 0;
        };

        class PatternGenerator
        {
        public:
            virtual DamagePattern* generateUniformPattern(const UniformPatternDesc* desc) = 0;
            virtual DamagePattern* generateBeamPattern(const BeamPatternDesc* desc) = 0;
            virtual DamagePattern* generateRegularRadialPattern(const RegularRadialPatternDesc* desc) = 0;


            virtual DamagePattern* generateVoronoiPattern(uint32_t pointCount, const NvcVec3* points, int32_t interiorMaterialId) = 0;
            virtual void release() = 0;
        };

        NV_C_API void savePatternToObj(DamagePattern* pattern);
        
    } // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTEXTAUTHORINGMESHCLEANER_H