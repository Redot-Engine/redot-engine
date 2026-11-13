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
//! @brief NvBlastExtDamageShaders blast extension, provides damage shaders for applying damage to destructibles

#ifndef NVBLASTEXTDAMAGESHADERS_H
#define NVBLASTEXTDAMAGESHADERS_H

#include "NvBlastTypes.h"
#include "NvBlastDebugRender.h"


/**
A few example damage shader implementations.
*/


///////////////////////////////////////////////////////////////////////////////
//  Damage Accelerator 
///////////////////////////////////////////////////////////////////////////////

class NvBlastExtDamageAccelerator
{
public:
    virtual void release() = 0;

    virtual Nv::Blast::DebugBuffer fillDebugRender(int depth = -1, bool segments = false) = 0;
};

NV_C_API NvBlastExtDamageAccelerator* NvBlastExtDamageAcceleratorCreate(const NvBlastAsset* asset, int type);


///////////////////////////////////////////////////////////////////////////////
//  Damage Program
///////////////////////////////////////////////////////////////////////////////

/**
Damage program params.

Custom user params to be passed in shader functions. This structure hints recommended parameters layout, but it
doesn't required to be this way.

The idea of this 'hint' is that damage parameters are basically 2 entities: material + damage description.
1. Material is something that describes an actor properties (e.g. mass, stiffness, fragility) which are not expected to be changed often.
2. Damage description is something that describes particular damage event (e.g. position, radius and force of explosion).

Also this damage program hints that there could be more than one damage event happening and processed per one shader call (for efficiency reasons).
So different damage descriptions can be stacked and passed in one shader call (while material is kept the same obviously).
*/
struct NvBlastExtProgramParams
{
    NvBlastExtProgramParams(const void* desc, const void* material_ = nullptr, NvBlastExtDamageAccelerator* accelerator_ = nullptr)
        : damageDesc(desc), material(material_), accelerator(accelerator_) {}

    const void* damageDesc;         //!<    array of damage descriptions
    const void* material;           //!<    pointer to material
    NvBlastExtDamageAccelerator*    accelerator;
};


///////////////////////////////////////////////////////////////////////////////
//  Common Material 
///////////////////////////////////////////////////////////////////////////////

/**
Example of simple material. It is passed into damage shader, thus it is not used 
currently in any of them. The user can use it to filter and normalize before applying.

Material function implementers may choose their own set.
*/
struct NvBlastExtMaterial
{
    NvBlastExtMaterial() : health(100.f), minDamageThreshold(0.0f), maxDamageThreshold(1.0f) {}

    float   health;                 //!<    health
    float   minDamageThreshold;     //!<    min damage fraction threshold to be applied. Range [0, 1]. For example 0.1 filters all damage below 10% of health.
    float   maxDamageThreshold;     //!<    max damage fraction threshold to be applied. Range [0, 1]. For example 0.8 won't allow more then 80% of health damage to be applied.

    /**
    Helper to normalize damage.
    
    Pass damage defined in health, damage in range [0, 1] is returned, where 0 basically 
    indicates that the threshold wasn't reached and there is no point in applying it.

    \param[in]      damageInHealth          Damage defined in terms of health amount to be reduced.

    \return normalized damage
    */
    float getNormalizedDamage(float damageInHealth) const
    {
        const float damage = health > 0.f ? damageInHealth / health : 1.0f;
        return damage > minDamageThreshold ? (damage < maxDamageThreshold ? damage : maxDamageThreshold) : 0.f;
    }
};


///////////////////////////////////////////////////////////////////////////////
//  Point Radial Damage
///////////////////////////////////////////////////////////////////////////////

/**
Radial Damage Desc
*/
struct NvBlastExtRadialDamageDesc
{
    float   damage;         //!<    normalized damage amount, range: [0, 1] (maximum health value to be reduced)
    float   position[3];    //!<    origin of damage action
    float   minRadius;      //!<    inner radius of damage action
    float   maxRadius;      //!<    outer radius of damage action
};

/**
Radial Falloff and Radial Cutter damage for both graph and subgraph shaders.

NOTE: The signature of shader functions are equal to NvBlastGraphShaderFunction and NvBlastSubgraphShaderFunction respectively. 
They are not expected to be called directly.
@see NvBlastGraphShaderFunction, NvBlastSubgraphShaderFunction
*/
NV_C_API void NvBlastExtFalloffGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtFalloffSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtCutterGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtCutterSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);


///////////////////////////////////////////////////////////////////////////////
//  Capsule Radial Damage
///////////////////////////////////////////////////////////////////////////////

/**
Capsule Radial Damage Desc
*/
struct NvBlastExtCapsuleRadialDamageDesc
{
    float   damage;         //!<    normalized damage amount, range: [0, 1] (maximum health value to be reduced)
    float   position0[3];   //!<    damage segment point A position
    float   position1[3];   //!<    damage segment point B position
    float   minRadius;      //!<    inner radius of damage action
    float   maxRadius;      //!<    outer radius of damage action
};

/**
Capsule Radial Falloff damage for both graph and subgraph shaders.

For every bond/chunk damage is calculated from the distance to line segment AB described in NvBlastExtCapsuleRadialDamageDesc.
If distance is smaller then minRadius, full compressive amount of damage is applied. From minRadius to maxRaidus it linearly falls off to zero.

NOTE: The signature of shader functions are equal to NvBlastGraphShaderFunction and NvBlastSubgraphShaderFunction respectively.
They are not expected to be called directly.
@see NvBlastGraphShaderFunction, NvBlastSubgraphShaderFunction
*/
NV_C_API void NvBlastExtCapsuleFalloffGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtCapsuleFalloffSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);


///////////////////////////////////////////////////////////////////////////////
//  Shear Damage
///////////////////////////////////////////////////////////////////////////////

/**
Shear Damage Desc
*/
struct NvBlastExtShearDamageDesc
{
    float   damage;         //!<    normalized damage amount, range: [0, 1] (maximum health value to be reduced)

    float   normal[3];      //!<    directional damage component
    float   position[3];    //!<    origin of damage action

    float   minRadius;      //!<    inner radius of damage action
    float   maxRadius;      //!<    outer radius of damage action
};

/**
Shear Damage Shaders

NOTE: The signature of shader functions are equal to NvBlastGraphShaderFunction and NvBlastSubgraphShaderFunction respectively.
They are not expected to be called directly.
@see NvBlastGraphShaderFunction, NvBlastSubgraphShaderFunction
*/
NV_C_API void NvBlastExtShearGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtShearSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);


///////////////////////////////////////////////////////////////////////////////
//  Triangle Intersection Damage
///////////////////////////////////////////////////////////////////////////////

/**
Triangle Intersection Damage Desc
*/
struct NvBlastExtTriangleIntersectionDamageDesc
{
    float   damage;         //!<    normalized damage amount, range: [0, 1] (maximum health value to be reduced)
    NvcVec3 position0;      //!<    triangle point A position
    NvcVec3 position1;      //!<    triangle point B position
    NvcVec3 position2;      //!<    triangle point C position
};

/**
Triangle Intersection damage for both graph and subgraph shaders.

Every bond is considered to be a segment connecting two chunk centroids. For every bond (segment) intersection with passed triangle is checked. If intersects
full damage is applied on bond.
For subgraph shader segments are formed as connections between it's subchunks centroids. Intersection is check in the same fashion.

The idea is that if you want to cut an object say with the laser sword, you can form a triangle by taking the position of a sword on this timeframe and on previous one.
So that nothing will be missed in terms of space and time. By sweeping sword through whole object it will be cut in halves inevitably, since all bonds segments form connected graph.

NOTE: The signature of shader functions are equal to NvBlastGraphShaderFunction and NvBlastSubgraphShaderFunction respectively.
They are not expected to be called directly.
@see NvBlastGraphShaderFunction, NvBlastSubgraphShaderFunction
*/
NV_C_API void NvBlastExtTriangleIntersectionGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtTriangleIntersectionSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);


///////////////////////////////////////////////////////////////////////////////
//  Impact Spread
///////////////////////////////////////////////////////////////////////////////

/**
Impact Spread Damage Desc
*/
struct NvBlastExtImpactSpreadDamageDesc
{
    float   damage;         //!<    normalized damage amount, range: [0, 1] (maximum health value to be reduced)
    float   position[3];    //!<    origin of damage action

    float   minRadius;      //!<    inner radius of damage action
    float   maxRadius;      //!<    outer radius of damage action
};

/**
Impact Spread Damage Shaders.

It assumes that position is somewhere on the chunk and looks for nearest chunk to this position and damages it. 
Then it does breadth-first support graph traversal. For radial falloff metric distance is measured along the edges of the graph.
That allows to avoid damaging parts which are near in space but disjointed topologically. For example if you hit one column of an arc 
it would take much bigger radius for damage to travel to the other column than in the simple radial damage.

Shader is designed to be used with impact damage, where it is know in advance that actual hit happened.

This shader requires NvBlastExtDamageAccelerator passed in, it request scratch memory from it, therefore it is also designed to work
only in single threaded mode. It can easily be changed by passing scratch memory as a part of NvBlastExtProgramParams if required.

NOTE: The signature of shader functions are equal to NvBlastGraphShaderFunction and NvBlastSubgraphShaderFunction respectively.
They are not expected to be called directly.
@see NvBlastGraphShaderFunction, NvBlastSubgraphShaderFunction
*/
NV_C_API void NvBlastExtImpactSpreadGraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastGraphShaderActor* actor, const void* params);
NV_C_API void NvBlastExtImpactSpreadSubgraphShader(NvBlastFractureBuffers* commandBuffers, const NvBlastSubgraphShaderActor* actor, const void* params);


#endif // NVBLASTEXTDAMAGESHADERS_H
