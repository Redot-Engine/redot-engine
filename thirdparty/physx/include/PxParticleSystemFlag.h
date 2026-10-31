// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PARTICLE_SYSTEM_FLAG_H
#define PX_PARTICLE_SYSTEM_FLAG_H

#include "foundation/PxFlags.h"
#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Identifies dirty particle buffers that need to be updated in the particle system.

This flag can be used mark the device user buffers that are dirty and need to be written to the particle system.
*/
struct PxParticleBufferFlag
{
	enum Enum
	{
		eNONE = 0,								//!< No data specified

		eUPDATE_POSITION = 1 << 0,				//!< Specifies the position (first 3 floats) and inverse mass (last float) data (array of PxVec4 * number of particles)
		eUPDATE_VELOCITY = 1 << 1,				//!< Specifies the velocity (first 3 floats) data (array of PxVec4 * number of particles)
		eUPDATE_PHASE = 1 << 2,					//!< Specifies the per-particle phase flag data (array of PxU32 * number of particles)
		eUPDATE_RESTPOSITION = 1 << 3,			//!< Specifies the rest position (first 3 floats) data
		eUPDATE_DIFFUSE_PARAM = 1 << 4,			//!< Specifies the diffuse particle parameter buffer (see PxDiffuseParticleParams)

		eALL =
		eUPDATE_POSITION | eUPDATE_VELOCITY | eUPDATE_PHASE | eUPDATE_RESTPOSITION | eUPDATE_DIFFUSE_PARAM
	};
};

typedef PxFlags<PxParticleBufferFlag::Enum, PxU32> PxParticleBufferFlags;

/**
\brief Identifies per-particle behavior for a PxParticleSystem.

See #PxPBDParticleSystem::createPhase().
*/
struct PxParticlePhaseFlag
{
	enum Enum
	{
		eParticlePhaseGroupMask = 0x000fffff,			//!< Bits [ 0, 19] represent the particle group for controlling collisions
		eParticlePhaseFlagsMask = 0xfff00000,			//!< Bits [20, 23] hold flags about how the particle behave 

		eParticlePhaseSelfCollide = 1 << 20,			//!< If set this particle will interact with particles of the same group
		eParticlePhaseSelfCollideFilter = 1 << 21,		//!< If set this particle will ignore collisions with particles closer than the radius in the rest pose, this flag should not be specified unless valid rest positions have been specified using setRestParticles()
		eParticlePhaseFluid = 1 << 22					//!< If set this particle will generate fluid density constraints for its overlapping neighbors
	};
};

typedef PxFlags<PxParticlePhaseFlag::Enum, PxU32> PxParticlePhaseFlags;
	
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
