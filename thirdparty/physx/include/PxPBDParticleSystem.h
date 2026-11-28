// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PBD_PARTICLE_SYSTEM_H
#define PX_PBD_PARTICLE_SYSTEM_H

#include "foundation/PxSimpleTypes.h"
#include "foundation/PxVec3.h"
#include "foundation/PxArray.h"
#include "cudamanager/PxCudaTypes.h"
#include "PxParticleSystemFlag.h"
#include "PxFiltering.h"
#include "PxActor.h"
#include "PxParticleSystem.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4435)
#endif

class PxCudaContextManager;
class PxGpuParticleSystem;

class PxParticleAndDiffuseBuffer;
class PxParticleBuffer;

/**
\brief Container to hold a pair of corresponding device and host pointers. These pointers should point to GPU / CPU mirrors of the same data, but
this is not enforced.
*/
template <typename Type>
struct PxGpuMirroredPointer
{
	Type* mDevicePtr;
	Type* mHostPtr;

	PxGpuMirroredPointer(Type* devicePtr, Type* hostPtr) : mDevicePtr(devicePtr), mHostPtr(hostPtr) { }
};

/**
\brief Particle system callback base class to schedule work that should be done before, while or after the particle system updates.
A call to fetchResultsParticleSystem() on the PxScene will synchronize the work such that the caller knows that all tasks of this callback completed.
*/
class PxParticleSystemCallback
{
public:
	/**
	\brief Method gets called when dirty data from the particle system is uploated to the gpu

	\param[in] gpuParticleSystem Pointers to the particle systems gpu data available as host accessible pointer and as gpu accessible pointer
	\param[in] stream The stream on which all cuda kernel calls get scheduled for execution. A call to fetchResultsParticleSystem() on the
	PxScene will synchronize the work such that the caller knows that the task completed.
	*/
	virtual void onBegin(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) = 0;

	/**
	\brief Method gets called when the simulation step of the particle system is performed

	\param[in] gpuParticleSystem Pointers to the particle systems gpu data available as host accessible pointer and as gpu accessible pointer
	\param[in] stream The stream on which all cuda kernel calls get scheduled for execution. A call to fetchResultsParticleSystem() on the
	PxScene will synchronize the work such that the caller knows that the task completed.
	*/
	virtual void onAdvance(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) = 0;

	/**
	\brief Method gets called after the particle system simulation step completed

	\param[in] gpuParticleSystem Pointers to the particle systems gpu data available as host accessible pointer and as gpu accessible pointer
	\param[in] stream The stream on which all cuda kernel calls get scheduled for execution. A call to fetchResultsParticleSystem() on the
	PxScene will synchronize the work such that the caller knows that the task completed.
	*/
	virtual void onPostSolve(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) = 0;

	/**
	\brief Destructor
	*/
	virtual ~PxParticleSystemCallback() {}
};

/**
\brief Special callback that forwards calls to arbitrarily many sub-callbacks
*/
class PxMultiCallback : public PxParticleSystemCallback
{
private:
	PxArray<PxParticleSystemCallback*> mCallbacks;

public:
	PxMultiCallback() : mCallbacks(0) {}

	virtual void onPostSolve(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) PX_OVERRIDE
	{
		for (PxU32 i = 0; i < mCallbacks.size(); ++i)
			mCallbacks[i]->onPostSolve(gpuParticleSystem, stream);
	}

	virtual void onBegin(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) PX_OVERRIDE
	{
		for (PxU32 i = 0; i < mCallbacks.size(); ++i)
			mCallbacks[i]->onBegin(gpuParticleSystem, stream);
	}

	virtual void onAdvance(const PxGpuMirroredPointer<PxGpuParticleSystem>& gpuParticleSystem, CUstream stream) PX_OVERRIDE
	{
		for (PxU32 i = 0; i < mCallbacks.size(); ++i)
			mCallbacks[i]->onAdvance(gpuParticleSystem, stream);
	}

	/**
	\brief Adds a callback

	\param[in] callback The callback to add
	\return True if the callback was added
	*/
	bool addCallback(PxParticleSystemCallback* callback)
	{
		if (mCallbacks.find(callback) != mCallbacks.end())
			return false;
		mCallbacks.pushBack(callback);
		return true;
	}

	/**
	\brief Removes a callback

	\param[in] callback The callback to remove
	\return True if the callback was removed
	*/
	bool removeCallback(const PxParticleSystemCallback* callback)
	{
		for (PxU32 i = 0; i < mCallbacks.size(); ++i)
		{
			if (mCallbacks[i] == callback)
			{
				mCallbacks.remove(i);
				return true;
			}
		}
		return false;
	}
};

/**
\brief Flags which control the behaviour of a particle system.

See #PxPBDParticleSystem::setParticleFlag(), #PxPBDParticleSystem::setParticleFlags(), #PxPBDParticleSystem::getParticleFlags()
*/
struct PxParticleFlag
{
	enum Enum
	{
		eDISABLE_SELF_COLLISION = 1 << 0,	//!< Disables particle self-collision
		eDISABLE_RIGID_COLLISION = 1 << 1,	//!< Disables particle-rigid body collision
		eFULL_DIFFUSE_ADVECTION = 1 << 2,	//!< Enables full advection of diffuse particles. By default, diffuse particles are advected only by particles in the cell they are contained. This flag enables full neighbourhood generation (more expensive).
		eENABLE_SPECULATIVE_CCD = 1 << 3	//!< Enables speculative CCD for particle-rigid body collision. \see PxRigidBodyFlag::eENABLE_SPECULATIVE_CCD.
	};
};

typedef PxFlags<PxParticleFlag::Enum, PxU32> PxParticleFlags;

/**
\brief Collection of flags providing a mechanism to lock motion along a specific axis.

\see PxParticleSystem.setParticleLockFlag(), PxParticleSystem.getParticleLockFlags()
*/
struct PxParticleLockFlag
{
	enum Enum
	{
		eLOCK_X = (1 << 0),
		eLOCK_Y = (1 << 1),
		eLOCK_Z = (1 << 2)
	};
};

typedef PxFlags<PxParticleLockFlag::Enum, PxU8> PxParticleLockFlags;
PX_FLAGS_OPERATORS(PxParticleLockFlag::Enum, PxU8)

/**
\brief A particle system that uses the position based dynamics(PBD) solver.

The position based dynamics solver for particle systems supports behaviors like
fluid, cloth, inflatables etc. 

*/
class PxPBDParticleSystem : public PxActor
{
public:

	virtual                             ~PxPBDParticleSystem() {}

	/**
	\brief Sets the solver iteration counts for the body.

	The position iteration count determines how accurately contacts and internal particle-particle
	interactions (fluid density, self-collision) are resolved. With too few, particles can
	become unstable or a fluid can appear too compressible; a higher position iteration count may help.

	Velocity iterations run after the position solve and correct velocities only. Increasing the
	velocity iteration count may reduce the velocity lag inherent in position-based dynamics at
	contacts, as well as violent depenetration. Internal particle-particle interactions are not
	solved for, biasing the velocity result toward contacts.

	<b>Default:</b> 4 position iterations, 1 velocity iteration

	\param[in] minPositionIters Number of position iterations the solver should perform for this particle system. <b>Range:</b> [1,255]
	\param[in] minVelocityIters Number of velocity iterations the solver should perform for this particle system. <b>Range:</b> [0,255]

	See #getSolverIterationCounts()
	*/
	virtual     void					setSolverIterationCounts(PxU32 minPositionIters, PxU32 minVelocityIters = 1) = 0;

	/**
	\brief Retrieves the solver iteration counts.

	See #setSolverIterationCounts()
	*/
	virtual     void					getSolverIterationCounts(PxU32& minPositionIters, PxU32& minVelocityIters) const = 0;

	/**
	\brief Retrieves the collision filter settings.

	\return The filter data
	*/
	virtual     PxFilterData			getSimulationFilterData() const = 0;

	/**
	\brief Set collision filter settings

	Allows to control with which objects the particle system collides

	\param[in] data The filter data
	*/
	virtual     void					setSimulationFilterData(const PxFilterData& data) = 0;

	/**
	\brief Set particle flag

	Allows to control self collision etc.

	\param[in] flag The flag to set
	\param[in] val The new value of the flag
	*/
	virtual     void					setParticleFlag(PxParticleFlag::Enum flag, bool val) = 0;

	/**
	\brief Set particle flags

	Allows to control self collision etc.

	\param[in] flags The flags to set
	*/
	virtual     void					setParticleFlags(PxParticleFlags flags) = 0;

	/**
	\brief Retrieves the particle flags.

	\return The particle flags
	*/
	virtual     PxParticleFlags			getParticleFlags() const = 0;

	/**
	\brief Set the maximal depenetration velocity particles can reach

	Allows to limit the particles' maximal depenetration velocity to avoid that collision responses lead to very high particle velocities

	<b>Default:</b> 1.0e32

	\param[in] maxDepenetrationVelocity The maximal depenetration velocity
	*/
	virtual     void					setMaxDepenetrationVelocity(PxReal maxDepenetrationVelocity) = 0;

	/**
	\brief Retrieves maximal depenetration velocity a particle can have.

	\return The maximal depenetration velocity
	*/
	virtual     PxReal					getMaxDepenetrationVelocity() const = 0;

	/**
	\brief Set the maximal linear velocity particles can reach.

	Allows to limit the particles' maximal velocity to control the maximal distance a particle can move per frame.

	<b>Default:</b> PX_MAX_F32

	\param[in] maxLinearVelocity The maximal linear velocity
	*/
	virtual     void					setMaxLinearVelocity(PxReal maxLinearVelocity) = 0;

	/**
	\brief Retrieves maximal linear velocity a particle can have.

	\return The maximal linear velocity
	*/
	virtual     PxReal					getMaxLinearVelocity() const = 0;

	/**
	\deprecated Use setMaxLinearVelocity() instead.
	*/
	PX_DEPRECATED PX_FORCE_INLINE void	setMaxVelocity(PxReal maxVelocity) { setMaxLinearVelocity(maxVelocity); }

	/**
	\deprecated Use getMaxLinearVelocity() instead.
	*/
	PX_DEPRECATED PX_FORCE_INLINE PxReal getMaxVelocity() const { return getMaxLinearVelocity(); }


	/**
	\brief Return the cuda context manager

	\return The cuda context manager
	*/
	virtual     PxCudaContextManager*	getCudaContextManager() const = 0;

	/**
	\brief Set the rest offset for the collision between particles and rigids or deformable bodies.

	A particle and a rigid or deformable body will come to rest at a distance equal to the sum of their restOffset values.

	\param[in] restOffset <b>Range:</b> (0, contactOffset)
	*/
	virtual     void					setRestOffset(PxReal restOffset) = 0;

	/**
	\brief Return the rest offset
	\return the rest offset

	See #setRestOffset()
	*/
	virtual     PxReal					getRestOffset() const = 0;

	/**
	\brief Set the contact offset for the collision between particles and rigids or soft bodies

	The contact offset needs to be larger than the rest offset.
	Contact constraints are generated for a particle and a rigid or deformable below the distance equal to the sum of their contacOffset values.

	\param[in] contactOffset <b>Range:</b> (restOffset, PX_MAX_F32)
	*/
	virtual     void					setContactOffset(PxReal contactOffset) = 0;

	/**
	\brief Return the contact offset
	\return the contact offset

	See #setContactOffset()
	*/
	virtual     PxReal					getContactOffset() const = 0;

	/**
	\brief Set the contact offset for the interactions between particles

	The particle contact offset needs to be larger than the fluid rest offset and larger than the solid rest offset.
	Interactions for two particles are computed if their distance is below twice the particleContactOffset value.

	\param[in] particleContactOffset <b>Range:</b> (Max(solidRestOffset, fluidRestOffset), PX_MAX_F32)
	*/
	virtual     void					setParticleContactOffset(PxReal particleContactOffset) = 0;

	/**
	\brief Return the particle contact offset
	\return the particle contact offset

	See #setParticleContactOffset()
	*/
	virtual     PxReal					getParticleContactOffset() const = 0;

	/**
	\brief Set the solid rest offset

	Two solid particles (or a solid and a fluid particle) will come to rest at a distance equal to twice the solidRestOffset value.

	\param[in] solidRestOffset  <b>Range:</b> (0, particleContactOffset)
	*/
	virtual     void					setSolidRestOffset(PxReal solidRestOffset) = 0;

	/**
	\brief Return the solid rest offset
	\return the solid rest offset

	See #setSolidRestOffset()
	*/
	virtual     PxReal					getSolidRestOffset() const = 0;


	/**
	\brief Reads the particle lock flags.

	See the list of flags #PxParticleLockFlag

	\return The values of the particle lock flags.

	\see PxParticleLockFlag setParticleLockFlag()
	*/
	virtual		PxParticleLockFlags		getParticleLockFlags() const = 0;

	/**
	\brief Raises or clears a particular particle lock flag.

	See the list of flags #PxParticleLockFlag

	<b>Default:</b> no flags are set


	\param[in] flag		The PxParticleLockFlag to raise(set) or clear.
	\param[in] value	The new boolean value for the flag.

	\see PxParticleLockFlag getParticleLockFlags()
	*/
	virtual		void					setParticleLockFlag(PxParticleLockFlag::Enum flag, bool value) = 0;

	/**
	\brief Set all particle lock flags.
	\see setParticleLockFlag()
	*/
	virtual		void					setParticleLockFlags(PxParticleLockFlags flags) = 0;


	/**
	\brief Creates combined particle flag with particle material and particle phase flags.

	\param[in] material A material instance to associate with the new particle group.
	\param[in] flags The particle phase flags.
	\return The combined particle group index and phase flags.

	See #PxParticlePhaseFlag
	*/
	virtual     PxU32					createPhase(PxPBDMaterial* material, PxParticlePhaseFlags flags) = 0;


	/**
	\brief Returns number of particle materials referenced by particle phases
	\return The number of particle materials
	*/
	virtual     PxU32					getNbParticleMaterials() const = 0;

	/**
	\brief Returns particle materials referenced by particle phases
	\return The particle materials
	*/
	virtual     PxU32					getParticleMaterials(PxPBDMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Sets a user notify object which receives special simulation events when they occur.

	\note Do not set the callback while the simulation is running. Calls to this method while the simulation is running will be ignored.
	\note A call to fetchResultsParticleSystem() on the PxScene will synchronize the work such that the caller knows that all worke done in the callback completed.

	\param[in] callback User notification callback. See PxSimulationEventCallback.

	See #PxParticleSystemCallback, #getParticleSystemCallback()
	*/
	virtual     void					setParticleSystemCallback(PxParticleSystemCallback* callback) = 0;

	/**
	\brief Retrieves the simulationEventCallback pointer set with setSimulationEventCallback().
	\return The current user notify pointer. See PxSimulationEventCallback.

	See #PxParticleSystemCallback, #setParticleSystemCallback()
	*/
	virtual     PxParticleSystemCallback* getParticleSystemCallback() const = 0;

	/**
	\brief Add an existing particle buffer to the particle system.
	\param[in] particleBuffer a PxParticleBuffer*.

	See #PxParticleBuffer.
	*/
	virtual     void					addParticleBuffer(PxParticleBuffer* particleBuffer) = 0;

	/**
	\brief Remove particle buffer from the particle system.
	\param[in] particleBuffer a PxParticleBuffer*.

	See #PxParticleBuffer.
	*/
	virtual     void					removeParticleBuffer(PxParticleBuffer* particleBuffer) = 0;

	/**
	\brief Returns the GPU particle system index.
	\return The GPU index, if the particle system is in a scene and PxSceneFlag::eENABLE_DIRECT_GPU_API is set, or 0xFFFFFFFF otherwise.
	*/
	virtual     PxU32					getGpuParticleSystemIndex() = 0;

	/**
	\brief Set wind direction and intensity

	\param[in] wind The wind direction and intensity
	*/
	virtual     void                    setWind(const PxVec3& wind) = 0;

	/**
	\brief Retrieves the wind direction and intensity.

	\return The wind direction and intensity
	*/
	virtual     PxVec3                  getWind() const = 0;

	/**
	\brief Set the fluid boundary density scale

	Defines how strong of a contribution the boundary (typically a rigid surface) should have on a fluid particle's density.

	\param[in] fluidBoundaryDensityScale  <b>Range:</b> (0.0, 1.0)
	*/
	virtual     void                    setFluidBoundaryDensityScale(PxReal fluidBoundaryDensityScale) = 0;

	/**
	\brief Return the fluid boundary density scale
	\return the fluid boundary density scale

	See #setFluidBoundaryDensityScale()
	*/
	virtual     PxReal                  getFluidBoundaryDensityScale() const = 0;

	/**
	\brief Set the fluid rest offset

	Two fluid particles will come to rest at a distance equal to twice the fluidRestOffset value.

	\param[in] fluidRestOffset  <b>Range:</b> (0, particleContactOffset)
	*/
	virtual     void                    setFluidRestOffset(PxReal fluidRestOffset) = 0;

	/**
	\brief Return the fluid rest offset
	\return the fluid rest offset

	See #setFluidRestOffset()
	*/
	virtual     PxReal                  getFluidRestOffset() const = 0;

	/**
	\brief Set the particle system grid size x dimension

	\param[in] gridSizeX x dimension in the particle grid
	*/
	virtual     void                    setGridSizeX(PxU32 gridSizeX) = 0;

	/**
	\brief Get the particle system grid size x dimension
	\return[in] the x dimension in the particle grid

	See #setGridSizeX()
	*/
	virtual     PxU32                    getGridSizeX() const = 0;

	/**
	\brief Set the particle system grid size y dimension

	\param[in] gridSizeY y dimension in the particle grid
	*/
	virtual     void                    setGridSizeY(PxU32 gridSizeY) = 0;

	/**
	\brief Get the particle system grid size y dimension
	\return[in] the y dimension in the particle grid

	See #setGridSizeY()
	*/
	virtual     PxU32                    getGridSizeY() const = 0;

	/**
	\brief Set the particle system grid size z dimension

	\param[in] gridSizeZ z dimension in the particle grid
	*/
	virtual     void                    setGridSizeZ(PxU32 gridSizeZ) = 0;

	/**
	\brief Get the particle system grid size z dimension
	\return[in] the z dimension in the particle grid

	See #setGridSizeZ()
	*/
	virtual     PxU32                    getGridSizeZ() const = 0;


	virtual     const char*             getConcreteTypeName() const PX_OVERRIDE	PX_FINAL { return "PxPBDParticleSystem"; }

protected:
	PX_INLINE                           PxPBDParticleSystem(PxType concreteType, PxBaseFlags baseFlags) : PxActor(concreteType, baseFlags) {}
	PX_INLINE                           PxPBDParticleSystem(PxBaseFlags baseFlags) : PxActor(baseFlags) {}
	virtual     bool                    isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxPBDParticleSystem", PxActor); }
};

#if PX_VC
#pragma warning(pop)
#endif


#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
