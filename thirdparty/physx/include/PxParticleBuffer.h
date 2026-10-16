// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PARTICLE_BUFFER_H
#define PX_PARTICLE_BUFFER_H

#include "common/PxBase.h"
#include "common/PxPhysXCommonConfig.h"
#include "common/PxTypeInfo.h"

#include "PxParticleSystemFlag.h"

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec4.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4435)
#endif

class PxCudaContextManager;

/**
\brief The shared base class for all particle buffers, can be instantiated directly to simulate granular and fluid particles.

See #PxPhysics::createParticleBuffer.

A particle buffer is a container that specifies per-particle attributes of a set of particles that will be used during the simulation 
of a particle system. It exposes direct access to the underlying GPU buffers and is independent of the scene and particle system. Particle
buffers can be added/removed from a particle system at any time between simulation steps, and transferred from one particle system to another.
*/
class PxParticleBuffer : public PxBase
{
public:

	/**
	\brief Get positions and inverse masses for this particle buffer.
	\return A pointer to a device buffer containing the positions and inverse mass packed as PxVec4(pos.x, pos.y, pos.z, inverseMass).
	*/
	virtual PxVec4*				getPositionInvMasses() const = 0;

	/**
	\brief Get velocities for this particle buffer.
	\return A pointer to a device buffer containing the velocities packed as PxVec4(vel.x, vel.y, vel.z, 0.0f).
	*/
	virtual PxVec4*				getVelocities() const = 0;

	/**
	\brief Get phases for this particle buffer.

	See #PxParticlePhaseFlag

	\return A pointer to a device buffer containing the per-particle phases for this particle buffer.
	*/
	virtual PxU32*				getPhases() const = 0;

	/**
	\brief Set the number of active particles for this particle buffer.
	\param[in] nbActiveParticles The number of active particles.

	The number of active particles can be <= PxParticleBuffer::getMaxParticles(). The particle system will simulate the first
	x particles in the #PxParticleBuffer, where x is the number of active particles.
	*/
	virtual void				setNbActiveParticles(PxU32 nbActiveParticles) = 0;

	/**
	\brief Get the number of active particles for this particle buffer.
	\return The number of active particles.
	*/
	virtual PxU32				getNbActiveParticles() const = 0;

	/**
	\brief Get the maximum number particles this particle buffer can hold.

	The maximum number of particles is specified when creating a #PxParticleBuffer. See #PxPhysics::createParticleBuffer.

	\return The maximum number of particles.
	*/
	virtual PxU32				getMaxParticles() const = 0;

	/**
	\brief Get the start index for the first particle of this particle buffer in the complete list of
	particles of the particle system this buffer is used in.

	The return value is only correct if the particle buffer is assigned to a particle system and at least
	one call to simulate() has been performed.

	\return The index of the first particle in the complete particle list.
	*/
	virtual PxU32				getFlatListStartIndex() const = 0;

	/**
	\brief Raise dirty flags on this particle buffer to communicate that the corresponding data has been updated
	by the user.
	\param[in] flags The flag corresponding to the data that is dirty.

	See #PxParticleBufferFlag.
	*/
	virtual void				raiseFlags(PxParticleBufferFlag::Enum flags) = 0;

	/**
	\brief Release this buffer and deallocate all the memory.
	*/
	virtual void				release() = 0;

	/**
	\brief Retrieve unique index that does not change over the lifetime of a PxParticleBuffer.
	*/
	virtual PxU32				getUniqueId() const = 0;

	/**
	\brief Sets a name string for the object that can be retrieved with getName().

	This is for debugging and is not used by the SDK. The string is not copied by the SDK,
	only the pointer is stored.

	\param[in] name String to set the objects name to.

	<b>Default:</b> NULL

	\see getName()
	*/
	virtual	void				setName(const char* name) = 0;

	/**
	\brief Retrieves the name string set with setName().

	\return Name string associated with object.

	\see setName()
	*/
	virtual	const char*			getName() const = 0;

	//public variables:
	void*						userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:

	virtual						~PxParticleBuffer() { }
	PX_INLINE 					PxParticleBuffer(PxType type) : PxBase(type, PxBaseFlag::eOWNS_MEMORY | PxBaseFlag::eIS_RELEASABLE), userData(NULL) {}

private:
	PX_NOCOPY(PxParticleBuffer)
};

/**
\brief Parameters to configure the behavior of diffuse particles
*/
class PxDiffuseParticleParams
{
public:
	/**
	\brief Construct parameters with default values.
	*/
	PX_INLINE PxDiffuseParticleParams()
	{
		threshold = 100.0f;
		lifetime = 5.0f;
		airDrag = 0.0f;
		bubbleDrag = 0.5f;
		buoyancy = 0.8f;
		kineticEnergyWeight = 0.01f;
		pressureWeight = 1.0f;
		divergenceWeight = 5.0f;
		collisionDecay = 0.5f;
		useAccurateVelocity = false;
	}

	/**
	\brief (re)sets the structure to the default.	
	*/
	PX_INLINE void setToDefault()
	{
		*this = PxDiffuseParticleParams();
	}
	
	PxReal	threshold;				//!< Particles with potential value greater than the threshold will spawn diffuse particles
	PxReal	lifetime;				//!< Diffuse particle will be removed after the specified lifetime
	PxReal	airDrag;				//!< Air drag force factor for spray particles
	PxReal	bubbleDrag;				//!< Fluid drag force factor for bubble particles
	PxReal	buoyancy;				//!< Buoyancy force factor for bubble particles
	PxReal	kineticEnergyWeight;	//!< Contribution from kinetic energy when deciding diffuse particle creation.
	PxReal	pressureWeight;			//!< Contribution from pressure when deciding diffuse particle creation.
	PxReal	divergenceWeight;		//!< Contribution from divergence when deciding diffuse particle creation.
	PxReal	collisionDecay;			//!< Decay factor of diffuse particles' lifetime after they collide with shapes.
	bool	useAccurateVelocity;	//!< If true, enables accurate velocity estimation when using PBD solver.
};

/**
\brief A particle buffer used to simulate diffuse particles.

See #PxPhysics::createParticleAndDiffuseBuffer.
*/
class PxParticleAndDiffuseBuffer : public PxParticleBuffer
{
public:

	/**
	\brief Get a device buffer of positions and remaining lifetimes for the diffuse particles.
	\return A device buffer containing positions and lifetimes of diffuse particles packed as PxVec4(pos.x, pos.y, pos.z, lifetime).
	*/
	virtual PxVec4*					getDiffusePositionLifeTime() const = 0;

	/**
	\brief Get a device buffer of velocities for the diffuse particles.
	\return A device buffer containing velocities of diffuse particles.
	*/
	virtual PxVec4*					getDiffuseVelocities() const = 0;

	/**
	\brief Get number of currently active diffuse particles.
	\return The number of currently active diffuse particles.
	*/
	virtual PxU32					getNbActiveDiffuseParticles() const = 0;

	/**
	\brief Set the maximum possible number of diffuse particles for this buffer.
	\param[in] maxActiveDiffuseParticles the maximum number of active diffuse particles.

	\note Must be in the range [0, PxParticleAndDiffuseBuffer::getMaxDiffuseParticles()]
	*/
	virtual void					setMaxActiveDiffuseParticles(PxU32 maxActiveDiffuseParticles) = 0;

	/**
	\brief Get maximum possible number of diffuse particles.
	\return The maximum possible number diffuse particles.
	*/
	virtual PxU32					getMaxDiffuseParticles() const = 0;

	/**
	\brief Set the parameters for diffuse particle simulation.
	\param[in] params The diffuse particle parameters.

	See #PxDiffuseParticleParams
	*/
	virtual void					setDiffuseParticleParams(const PxDiffuseParticleParams& params) = 0;

	/**
	\brief Get the parameters currently used for diffuse particle simulation.
	\return A PxDiffuseParticleParams structure.
	*/
	virtual PxDiffuseParticleParams	getDiffuseParticleParams() const = 0;

protected:

	virtual 						~PxParticleAndDiffuseBuffer() {}
	PX_INLINE 						PxParticleAndDiffuseBuffer(PxType type) : PxParticleBuffer(type){}

private:
	PX_NOCOPY(PxParticleAndDiffuseBuffer)
};

#if PX_VC
#pragma warning(pop)
#endif


#if !PX_DOXYGEN
} // namespace physx
#endif


#endif
