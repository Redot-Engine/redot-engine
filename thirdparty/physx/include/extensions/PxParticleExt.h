// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PARTICLE_EXT_H
#define PX_PARTICLE_EXT_H

#include "PxParticleBuffer.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

namespace ExtGpu
{

/**
\brief Structure to define user-defined particle state when constructing a new particle system.
*/
struct PxParticleBufferDesc
{
	PxVec4* positions;
	PxVec4* velocities;
	PxU32*  phases;
	PxU32	numActiveParticles;
	PxU32	maxParticles;

	PxParticleBufferDesc() : positions(NULL), velocities(NULL), phases(NULL), numActiveParticles(0), maxParticles(0) { }
};

/**
\brief Structure to define user-defined particle state when constructing a new particle system that includes diffuse particles.
*/
struct PxParticleAndDiffuseBufferDesc : public PxParticleBufferDesc
{
	PxDiffuseParticleParams diffuseParams;
	PxU32 maxDiffuseParticles;
	PxU32 maxActiveDiffuseParticles;

	PxParticleAndDiffuseBufferDesc() : PxParticleBufferDesc() { }
};

/**
\brief Creates and populates a particle buffer

\param[in] desc The particle buffer descriptor
\param[in] cudaContextManager A cuda context manager
\return A fully populated particle buffer ready to use
*/
PxParticleBuffer*						PxCreateAndPopulateParticleBuffer(const ExtGpu::PxParticleBufferDesc& desc, PxCudaContextManager* cudaContextManager);
		
/**
\brief Creates and populates a particle buffer that includes support for diffuse particles

\param[in] desc The particle buffer descriptor
\param[in] cudaContextManager A cuda context manager
\return A fully populated particle buffer ready to use
*/
PxParticleAndDiffuseBuffer*				PxCreateAndPopulateParticleAndDiffuseBuffer(const ExtGpu::PxParticleAndDiffuseBufferDesc& desc, PxCudaContextManager* cudaContextManager);

} // namespace ExtGpu

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif

