// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_ARTICULATION_RC_H
#define PX_ARTICULATION_RC_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"
#include "foundation/PxVec3.h"
#include "foundation/PxBounds3.h"
#include "foundation/PxTransform.h"
#include "solver/PxSolverDefs.h"
#include "PxArticulationFlag.h"
#include "PxArticulationTendon.h"
#include "PxArticulationMimicJoint.h"
#include "PxArticulationFlag.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	PX_ALIGN_PREFIX(16)
	/**
	\brief Data structure to represent spatial forces.
	*/
	struct PxSpatialForce
	{
		PxVec3 force;
		PxReal pad0;
		PxVec3 torque;
		PxReal pad1;
	}
	PX_ALIGN_SUFFIX(16);

	PX_ALIGN_PREFIX(16)
	/**
	\brief Data structure to represent spatial velocities.
	*/
	struct PxSpatialVelocity
	{
		PxVec3 linear;
		PxReal pad0;
		PxVec3 angular;
		PxReal pad1;
	}
	PX_ALIGN_SUFFIX(16);

	class PxConstraint;
	class PxScene;

	/**
	\brief Data structure used to access the root link state and acceleration.

	\see PxArticulationCache
	*/
	struct PxArticulationRootLinkData
	{
		PxTransform	transform;		//!< Actor transform
		// The velocities and accelerations below are with respect to the center of mass (COM) of the root link. The COM and actor frame origin may not coincide.
		PxVec3		worldLinVel;	//!< Link linear velocity
		PxVec3		worldAngVel;	//!< Link angular velocity
		PxVec3		worldLinAccel;	//!< Link classical linear acceleration
		PxVec3		worldAngAccel;	//!< Link angular acceleration
	};

	/**
	\brief Data structure used to read and write internal articulation data.

	\see PxArticulationCacheFlag, PxArticulationReducedCoordinate::createCache, PxArticulationReducedCoordinate::applyCache,
	PxArticulationReducedCoordinate::copyInternalStateToCache
	*/
	class PxArticulationCache
	{
	public:
		PxArticulationCache() :
			externalForces				(NULL),
			denseJacobian				(NULL),
			massMatrix					(NULL),
			coriolisForce				(NULL),
			gravityCompensationForce	(NULL),
			centroidalMomentumMatrix	(NULL),
			centroidalMomentumBias		(NULL),
			jointVelocity				(NULL),
			jointAcceleration			(NULL),
			jointPosition				(NULL),
			jointForce					(NULL),
			jointTargetPositions		(NULL),
			jointTargetVelocities		(NULL),
			linkVelocity				(NULL),
			linkAcceleration			(NULL),
			linkIncomingJointForce		(NULL),
			linkForce					(NULL),
			linkTorque					(NULL),
			rootLinkData				(NULL),
			coefficientMatrix			(NULL),
			lambda						(NULL),
			scratchMemory				(NULL),
			scratchAllocator			(NULL),
			version						(0)
		{}

		/**
		\brief Releases an articulation cache.

		\see PxArticulationReducedCoordinate::createCache, PxArticulationReducedCoordinate::applyCache,
			 PxArticulationReducedCoordinate::copyInternalStateToCache
		*/
		PX_PHYSX_CORE_API void		release();

		/**
		\brief External forces acting on the articulation links for inverse dynamics computation.

		- N = getNbLinks().
		- Indexing follows the low-level link indices, see PxArticulationLink::getLinkIndex.
		- The forces are with respect to the center of mass of the link.
		- This field cannot be used to apply forces to links during the next PxScene::simulate() call. Use PxRigidBody::addForce and related functions instead.

		\see PxArticulationReducedCoordinate::computeGeneralizedExternalForce
		*/
		PxSpatialForce*				externalForces;

		/**
		\brief Dense Jacobian data.

		- N = nbRows * nbCols = (6 * getNbLinks()) * (6 + getDofs()) -> size includes possible floating-base DOFs regardless of PxArticulationFlag::eFIX_BASE flag.
		- The links, i.e. rows are in order of the low-level link indices (minus one if PxArticulationFlag::eFIX_BASE is true), see PxArticulationLink::getLinkIndex.
		The corresponding spatial velocities are stacked [vx; vy; vz; wx; wy; wz], where vx and wx refer to the linear and rotational velocity in world X.
		- The DOFs, i.e. column indices correspond to the low-level DOF indices, see PxArticulationCache::jointVelocity.

		\see PxArticulationReducedCoordinate::computeDenseJacobian
		*/
		PxReal*						denseJacobian;

		/**
		\brief The generalized mass matrix used in inverse dynamics algorithms.

		- N = (getDofs() + 6) * (getDofs() + 6) -> size includes possible floating-base DOFs regardless of PxArticulationFlag::eFIX_BASE flag.
		- If PxArticulationFlag::eFIX_BASE is true, the terms corresponding to the root DoFs are included in the top left block of the matrix.
		  For these terms, column indices correspond to linear acceleration first then angular acceleration, row indices correspond to force first then torque.
		  The bottom right block of the matrix corresponds to the joint DoFs terms, column indices correspond to joint acceleration, row indices correspond to joint force.
		- If PxArticulationFlag::eFIX_BASE is false, only the terms corresponding to the joint DoFs are included, column indices correspond to joint acceleration,
		  row indices correspond to joint force.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
		- The mass matrix is indexed [nCols * row + column].

		\see PxArticulationReducedCoordinate::computeMassMatrix
		*/
		PxReal*						massMatrix;

		/**
		\brief The Coriolis and centrifugal compensation forces used in inverse dynamics algorithms.

		- N = getDofs() + 6 -> size includes possible floating-base DOFs regardless of PxArticulationFlag::eFIX_BASE flag.
		- If PxArticulationFlag::eFIX_BASE is true, the terms corresponding to the root DoFs are included at the start of
		  the array (force then torque), followed by the joint DoFs terms.
		- If PxArticulationFlag::eFIX_BASE is false, only the terms corresponding to the joint DoFs are included.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.

		\see PxArticulationReducedCoordinate::computeCoriolisCompensation
		*/
		PxReal*						coriolisForce;

		/**
		\brief The gravity compensation forces used in inverse dynamics algorithms.

		- N = getDofs() + 6 -> size includes possible floating-base DOFs regardless of PxArticulationFlag::eFIX_BASE flag.
		- If PxArticulationFlag::eFIX_BASE is true, the terms corresponding to the root DoFs are included at the start of
		  the array (force then torque), followed by the joint DoFs terms.
		- If PxArticulationFlag::eFIX_BASE is false, only the terms corresponding to the joint DoFs are included.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.

		\see PxArticulationReducedCoordinate::computeGravityCompensation
		*/
		PxReal*						gravityCompensationForce;

		/**
		\brief The centroidal momentum matrix that maps velocities to centroidal momentum.

		- N = 6 * (getDofs() + 6).
		- Each row includes first the term related to the DOF of the root (linear, then angular), then the joint DOF following the internal DOF index order,
		  see PxArticulationCache::jointVelocity.
		- The centroidal momentum includes first the linear and then the angular contribution.

		\see PxArticulationReducedCoordinate::computeCentroidalMomentumMatrix
		*/
		PxReal*						centroidalMomentumMatrix;

		/**
		\brief The centroidal momentum bias force. This is a second-order term to calculate the derivative of the centroidal momentum.

		- N = 6.
		- The centroidal momentum includes first the linear and then the angular contribution.

		\see PxArticulationReducedCoordinate::computeCentroidalMomentumMatrix
		*/
		PxReal*						centroidalMomentumBias;

		/**
		\brief The articulation joint DOF velocities.

		- N = getDofs().
		- Read/write using PxArticulationCacheFlag::eVELOCITY.
		- The indexing follows the internal DOF index order. Therefore, the application should calculate the DOF data indices by summing the joint DOFs in the order of
		the links' low-level indices (see the manual Section "Cache Indexing" for a snippet for this calculation):
		\verbatim Low-level link index:   | link 0 | link 1 | link 2 | link 3 | ... | <- PxArticulationLink::getLinkIndex()       \endverbatim
		\verbatim Link inbound joint DOF: | 0      | 1      | 2      | 1      | ... | <- PxArticulationLink::getInboundJointDof() \endverbatim
		\verbatim Low-level DOF index:    | -      | 0      | 1, 2   | 3      | ... |                                             \endverbatim
		The root link always has low-level index 0 and always has zero inbound joint DOFs. The link DOF indexing follows the order in PxArticulationAxis::Enum.
		For example, assume that low-level link 2 has an inbound spherical joint with two DOFs: eSWING1 and eSWING2. The corresponding low-level joint DOF indices
		are therefore 1 for eSWING1 and 2 for eSWING2.
		*/
		PxReal*						jointVelocity;

		/**
		\brief The articulation joint DOF accelerations.

		- N = getDofs().
		- Read using PxArticulationCacheFlag::eACCELERATION.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
		- Delta joint DOF velocities can be computed from acceleration * dt.
		*/
		PxReal*						jointAcceleration;

		/**
		\brief The articulation joint DOF positions.

		- N = getDofs().
		- Read/write using PxArticulationCacheFlag::ePOSITION.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
        - For spherical joints, the joint position for each axis on the joint must be in range [-Pi, Pi].
		*/
		PxReal*						jointPosition;

		/**
		\brief The articulation joint DOF forces.

		- N = getDofs().
		- Read/Write using PxArticulationCacheFlag::eFORCE.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
		- Applied joint forces persist and are applied each frame until changed.
		*/
		PxReal*						jointForce;

		/**
		\brief The articulation joint drive target positions.

		- N = getDofs().
		- Write using PxArticulationCacheFlag::eJOINT_TARGET_POSITIONS.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
		*/
		PxReal*						jointTargetPositions;

		/**
		\brief The articulation joint drive target velocities.

		- N = getDofs().
		- Write using PxArticulationCacheFlag::eJOINT_TARGET_VELOCITIES.
		- The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.
		 */
		PxReal*						jointTargetVelocities;

		/**
		\brief Link spatial velocity.

		- N = getNbLinks().
		- Read using PxArticulationCacheFlag::eLINK_VELOCITY.
		- The indexing follows the internal link indexing, see PxArticulationLink::getLinkIndex.
		- The velocity is with respect to the link's center of mass but represented in world space.

		\see PxRigidBody::getCMassLocalPose
		*/
		PxSpatialVelocity*			linkVelocity;

		/**
		\brief Link classical acceleration.

		- N = getNbLinks().
		- Read using PxArticulationCacheFlag::eLINK_ACCELERATION.
		- The indexing follows the internal link indexing, see PxArticulationLink::getLinkIndex.
		- The acceleration is with respect to the link's center of mass.

		\see PxArticulationReducedCoordinate::getLinkAcceleration, PxRigidBody::getCMassLocalPose
		*/
		PxSpatialVelocity*			linkAcceleration;

		/**
		\brief Link incoming joint force, i.e. the total force transmitted from the parent link to this link.

		- N = getNbLinks().
		- Read using PxArticulationCacheFlag::eLINK_INCOMING_JOINT_FORCE.
		- The indexing follows the internal link indexing, see PxArticulationLink::getLinkIndex.
		- The force is reported in the child joint frame of the link's incoming joint.

		\see PxArticulationJointReducedCoordinate::getChildPose
		\note The root link reports a zero spatial force.
		*/
		PxSpatialForce*			linkIncomingJointForce;

		/**
		\brief Link force, i.e. an external force applied to the link's center of mass.

		- N = getNbLinks().
		- Write using PxArticulationCacheFlag::eLINK_FORCE.
		- The indexing follows the internal link indexing, see PxArticulationLink::getLinkIndex.
		- The force is given in world space.
		*/
		PxVec3*					linkForce;

		/**
		\brief Link torque, i.e. an external torque applied to the link.

		- N = getNbLinks().
		- Write using PxArticulationCacheFlag::eLINK_TORQUE.
		- The indexing follows the internal link indexing, see PxArticulationLink::getLinkIndex.
		- The torque is given in world space.
		*/
		PxVec3*					linkTorque;

		/**
		\brief Root link transform, velocities, and accelerations.

		- N = 1.
		- Read/write using PxArticulationCacheFlag::eROOT_TRANSFORM and PxArticulationCacheFlag::eROOT_VELOCITIES (accelerations are read-only).

		\see PxArticulationRootLinkData
		*/
		PxArticulationRootLinkData*	rootLinkData;

		// Members and memory below here are not zeroed when zeroCache is called, and are not included in the size returned by PxArticulationReducedCoordinate::getCacheDataSize.

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Constraint coefficient matrix.

		- N = getCoefficentMatrixSize().
		- The user needs to allocate memory and set this member to the allocated memory.

		\see PxArticulationReducedCoordinate::computeCoefficientMatrix
		*/
		PX_DEPRECATED PxReal*			coefficientMatrix;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Constraint lambda values (impulses applied by the respective constraints).

		- N = getNbLoopJoints().
		- The user needs to allocate memory and set this member to the allocated memory.

		\see PxArticulationReducedCoordinate::computeLambda
		*/
		PX_DEPRECATED PxReal*			lambda;

		void*						scratchMemory;		//!< The scratch memory is used for internal calculations.
		void*						scratchAllocator;	//!< The scratch allocator is used for internal calculations.
		PxU32						version;			//!< The cache version used internally to check compatibility with the articulation, i.e. detect if the articulation configuration changed after the cache was created.
	};


	/**
	\brief Flag that configures articulation-state updates by PxArticulationReducedCoordinate::updateKinematic.
	*/
	struct PxArticulationKinematicFlag
	{
		enum Enum
		{
			ePOSITION = 1 << 0,		//!< Raise after any changes to the articulation root or joint positions using non-cache API calls. Updates links' positions and velocities.
			eVELOCITY = 1 << 1		//!< Raise after velocity-only changes to the articulation root or joints using non-cache API calls. Updates links' velocities.
		};
	};

	typedef physx::PxFlags<PxArticulationKinematicFlag::Enum, PxU8> PxArticulationKinematicFlags;
	PX_FLAGS_OPERATORS(PxArticulationKinematicFlag::Enum, PxU8)

#if PX_VC
#pragma warning(push)
#pragma warning(disable : 4435)
#endif

	/**
	\brief A tree structure of bodies connected by joints that is treated as a unit by the dynamics solver. Parametrized in reduced (joint) coordinates.

	\see PxArticulationJointReducedCoordinate, PxArticulationLink, PxPhysics::createArticulationReducedCoordinate
	*/
	class PxArticulationReducedCoordinate : public PxBase
	{
	public:

		/**
		\brief Returns the scene which this articulation belongs to.

		\return Owner Scene. NULL if not part of a scene.

		\see PxScene
		*/
		virtual		PxScene*		getScene()	const = 0;

		/**
		\brief Sets the solver iteration counts for the articulation.

		The solver iteration count determines how accurately contacts, drives, and limits are resolved.
		Setting a higher position iteration count may therefore help in scenarios where the articulation
		is subject to many constraints; for example, a manipulator articulation with drives and joint limits
		that is grasping objects, or several such articulations interacting through contacts. Other situations
		where higher position iterations may improve simulation fidelity are: large mass ratios within the
		articulation or between the articulation and an object in contact with it; or strong drives in the
		articulation being used to manipulate a light object.

		If intersecting bodies are being depenetrated too violently, increase the number of velocity
		iterations. More velocity iterations will drive the relative exit velocity of the intersecting
		objects closer to the correct value given the restitution.
		
		\param[in] minPositionIters Number of position iterations the solver should perform for this articulation. <b>Range:</b> [1,255]. <b>Default:</b> 4.
		\param[in] minVelocityIters Number of velocity iterations the solver should perform for this articulation. <b>Range:</b> [0,255]. <b>Default:</b> 1

		\note This call may not be made during simulation.

		\see getSolverIterationCounts()
		*/
		virtual		void				setSolverIterationCounts(PxU32 minPositionIters, PxU32 minVelocityIters = 1) = 0;

		/**
		\brief Returns the solver iteration counts.

		\see setSolverIterationCounts()
		*/
		virtual		void				getSolverIterationCounts(PxU32& minPositionIters, PxU32& minVelocityIters) const = 0;

		/**
		\brief Returns true if this articulation is sleeping.

		When an actor does not move for a period of time, it is no longer simulated in order to reduce computational cost. This state
		is called sleeping. However, because the object automatically wakes up when it is either touched by an awake object,
		or a sleep-affecting property is changed by the user, the entire sleep mechanism should be transparent to the user.

		An articulation can only go to sleep if all links are ready for sleeping. An articulation is guaranteed to be awake
		if at least one of the following holds:

		\li The wake counter of any link in the articulation is positive (see #setWakeCounter()).
		\li The mass-normalized energy of any link in the articulation is above a threshold (see #setSleepThreshold()).
		\li A non-zero force or torque has been applied to any joint or link.

		If an articulation is sleeping, the following state is guaranteed:

		\li The wake counter is zero.
		\li The linear and angular velocity of all links is zero.
		\li There is no force update pending.

		When an articulation gets inserted into a scene, it will be considered asleep if all the points above hold, else it will
		be treated as awake.

		If an articulation is asleep after the call to #PxScene::fetchResults() returns, it is guaranteed that the poses of the
		links were not changed. You can use this information to avoid updating the transforms of associated objects.

		\return True if the articulation is sleeping.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation,
		except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\see wakeUp() putToSleep()  getSleepThreshold() setSleepThreshold()
		*/
		virtual		bool				isSleeping() const = 0;

		/**
		\brief Sets the mass-normalized energy threshold below which the articulation may go to sleep.

		The articulation will sleep if the energy of each link is below this threshold.

		\param[in] threshold Energy below which the articulation may go to sleep. <b>Range:</b> [0, PX_MAX_F32)

		\note This call may not be made during simulation.

		<b>Default:</b> 5e-5f * PxTolerancesScale::speed * PxTolerancesScale::speed;

		\see isSleeping() getSleepThreshold() wakeUp() putToSleep()
		*/
		virtual		void				setSleepThreshold(PxReal threshold) = 0;

		/**
		\brief Returns the mass-normalized energy below which the articulation may go to sleep.

		\return The energy threshold for sleeping.

		\see isSleeping() wakeUp() putToSleep() setSleepThreshold()
		*/
		virtual		PxReal				getSleepThreshold() const = 0;

		/**
		\brief Sets the mass-normalized kinetic energy threshold below which the articulation may participate in stabilization.

		Articulations whose kinetic energy divided by their mass is above this threshold will not participate in stabilization.

		This value has no effect if PxSceneFlag::eENABLE_STABILIZATION was not enabled on the PxSceneDesc.

		<b>Default:</b> 5e-6f * PxTolerancesScale::speed * PxTolerancesScale::speed

		\param[in] threshold Energy below which the articulation may participate in stabilization. <b>Range:</b> [0,inf)

		\note This call may not be made during simulation.

		\see  getStabilizationThreshold() PxSceneFlag::eENABLE_STABILIZATION
		*/
		virtual		void				setStabilizationThreshold(PxReal threshold) = 0;

		/**
		\brief Returns the mass-normalized kinetic energy below which the articulation may participate in stabilization.

		Articulations whose kinetic energy divided by their mass is above this threshold will not participate in stabilization.

		\return The energy threshold for participating in stabilization.

		\see setStabilizationThreshold() PxSceneFlag::eENABLE_STABILIZATION
		*/
		virtual		PxReal				getStabilizationThreshold() const = 0;

		/**
		\brief Sets the wake counter for the articulation in seconds.

		- The wake counter value specifies a time threshold used to determine whether an articulation may be put to sleep.
		- The articulation will be put to sleep if all links have experienced a mass-normalised energy less than a threshold for at least
		a threshold time, as specified by the wake counter.
		- Passing in a positive value will wake up the articulation automatically.

		<b>Default:</b> 0.4s (which corresponds to 20 frames for a time step of 0.02s)

		\param[in] wakeCounterValue Wake counter value in seconds. <b>Range:</b> [0, PX_MAX_F32)

		\note This call may not be made during simulation, except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\see isSleeping() getWakeCounter()
		*/
		virtual		void				setWakeCounter(PxReal wakeCounterValue) = 0;

		/**
		\brief Returns the wake counter of the articulation in seconds.

		\return The wake counter of the articulation in seconds.

		\note This call may not be made during simulation, except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\see isSleeping() setWakeCounter()
		*/
		virtual		PxReal				getWakeCounter() const = 0;

		/**
		\brief Wakes up the articulation if it is sleeping.

		- The articulation will be woken up and might cause other touching objects to wake up as well during the next simulation step.
		- This will set the wake counter of the articulation to the value specified in #PxSceneDesc::wakeCounterResetValue.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation,
		except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\see isSleeping() putToSleep()
		*/
		virtual		void				wakeUp() = 0;

		/**
		\brief Forces the articulation to sleep.

		- The articulation will stay asleep during the next simulation step if not touched by another non-sleeping actor.
		- This will set any applied force, the velocity, and the wake counter of all bodies in the articulation to zero.

		\note This call may not be made during simulation, and may only be made on articulations that are in a scene.

		\see isSleeping() wakeUp()
		*/
		virtual		void				putToSleep() = 0;

		/**
		\brief Adds a link to the articulation with default attribute values.

		\param[in] parent The parent link in the articulation. Must be NULL if (and only if) this is the root link.
		\param[in] pose The initial pose of the new link. Must be a valid transform.

		\return The new link, or NULL if the link cannot be created.

		\note Creating a link is not allowed while the articulation is in a scene. In order to add a link,
		remove and then re-add the articulation to the scene.

		\note  When the articulation is added to a scene, the root link adopts the specified pose. The pose of the 
		root link is propagated through the ensemble of links from parent to child after accounting for each child's 
		inbound joint frames and the joint positions set by PxArticulationJointReducedCoordinate::setJointPosition().
		As a consequence, the pose of each non-root link is automatically overwritten when adding the articulation to the scene.

		\see PxArticulationLink
		*/
		virtual		PxArticulationLink*	createLink(PxArticulationLink* parent, const PxTransform& pose) = 0;

		/**
		\brief Releases the articulation, and all its links and corresponding joints.

		Attached mimic joints and tendons are released automatically when the articulation is released.

		\note This call may not be made during simulation.

		\note This call does not release any PxArticulationCache instance that has been instantiated using #createCache()
		*/
		virtual		void				release() PX_OVERRIDE = 0;

		/**
		\brief Returns the number of links in the articulation.

		\return The number of links.
		*/
		virtual		PxU32				getNbLinks() const = 0;

		/**
		\brief Returns the set of links in the articulation in the order that they were added to the articulation using createLink.

		The order of the links may be different from the order in which the data is stored in the cache, see PxArticulationLink::getLinkIndex.

		\param[in] userBuffer Buffer into which to write the array of articulation link pointers.
		\param[in] bufferSize The size of the buffer. If the buffer is not large enough to contain all the pointers to links,
		only as many as will fit are written.
		\param[in] startIndex Index of first link pointer to be retrieved.

		\return The number of links written into the buffer.

		\see PxArticulationLink
		*/
		virtual		PxU32				getLinks(PxArticulationLink** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0)	const = 0;

		/**
		\brief Returns the number of shapes in the articulation.

		\return The number of shapes.
		*/
		virtual		PxU32				getNbShapes() const = 0;

		/**
		\brief Sets a name string for the articulation that can be retrieved with getName().

		This is for debugging and is not used by the SDK. The string is not copied by the SDK,
		only the pointer is stored.

		\param[in] name A pointer to a char buffer used to specify the name of the articulation.

		\see getName()
		*/
		virtual		void				setName(const char* name) = 0;

		/**
		\brief Returns the name string set with setName().

		\return Name string associated with the articulation.

		\see setName()
		*/
		virtual		const char*			getName()			const = 0;

		/**
		\brief Returns the axis-aligned bounding box enclosing the articulation.

		\param[in] inflation Scale factor for computed world bounds. Box extents are multiplied by this value.

		\return The articulation's bounding box.

		\note It is not allowed to use this method while the simulation is running, except in a split simulation
		during #PxScene::collide() and up to #PxScene::advance(), and in PxContactModifyCallback or in contact report callbacks.

		\see PxBounds3
		*/
		virtual		PxBounds3			getWorldBounds(float inflation = 1.01f) const = 0;

		/**
		\brief Returns the aggregate associated with the articulation.

		\return The aggregate associated with the articulation or NULL if the articulation does not belong to an aggregate.

		\see PxAggregate
		*/
		virtual		PxAggregate*		getAggregate() const = 0;

		/**
		\brief Sets flags on the articulation.

		\param[in] flags The articulation flags.

		\note This call may not be made during simulation.

		\see PxArticulationFlag
		*/
		virtual		void					setArticulationFlags(PxArticulationFlags flags) = 0;

		/**
		\brief Raises or clears a flag on the articulation.

		\param[in] flag The articulation flag.
		\param[in] value The value to set the flag to.

		\note This call may not be made during simulation.

		\see PxArticulationFlag
		*/
		virtual		void					setArticulationFlag(PxArticulationFlag::Enum flag, bool value) = 0;

		/**
		\brief Returns the articulation's flags.

		\return The articulation's flags.

		\see PxArticulationFlag
		*/
		virtual		PxArticulationFlags		getArticulationFlags() const = 0;

		/**
		\brief Returns the total number of joint degrees-of-freedom (DOFs) of the articulation.

		- The six DOFs of the base of a floating-base articulation are not included in this count.
		- Example: Both a fixed-base and a floating-base double-pendulum with two revolute joints will have getDofs() == 2.
		- The return value is only valid for articulations that are in a scene.

		\return The number of joint DOFs, or 0xFFFFFFFF if the articulation is not in a scene.

		*/
		virtual			PxU32				getDofs() const = 0;

		/**
		\brief Creates an articulation cache that can be used to read and write internal articulation data.

		- When the structure of the articulation changes (e.g. adding a link) after the cache was created,
		the cache needs to be released and recreated.
		- Free the memory allocated for the cache by calling the release() method on the cache.
		- Caches can only be created by articulations that are in a scene.

		\return The cache, or NULL if the articulation is not in a scene.

		\see applyCache, copyInternalStateToCache
		*/
		virtual		PxArticulationCache*	createCache() const = 0;

		/**
		\brief Returns the size of the articulation cache in bytes.

		- The size does not include: the user-allocated memory for the coefficient matrix or lambda values;
		the scratch-related memory/members; and the cache version. See comment in #PxArticulationCache.
		- The return value is only valid for articulations that are in a scene.

		\return The byte size of the cache, or 0xFFFFFFFF if the articulation is not in a scene.

		\see PxArticulationCache
		*/
		virtual		PxU32					getCacheDataSize() const = 0;

		/**
		\brief Zeroes all data in the articulation cache, except user-provided and scratch memory, and cache version.

		\note This call may only be made on articulations that are in a scene.

		\see PxArticulationCache
		*/
		virtual		void					zeroCache(PxArticulationCache& cache) const = 0;

		/**
		\brief Applies the data in the cache to the articulation.

		This call wakes the articulation if it is sleeping, and the autowake parameter is true (default) or:
		- a nonzero joint velocity is applied or
		- a nonzero joint force is applied or
		- a nonzero root velocity is applied

		\param[in] cache The articulation data.
		\param[in] flags Indicate which data in the cache to apply to the articulation.
		\param[in] autowake If true, the call wakes up the articulation and increases the wake counter to #PxSceneDesc::wakeCounterResetValue
		if the counter value is below the reset value.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\note Calling applyCache(cache, PxArticulationCacheFlag::eROOT_TRANSFORM) has the same outcome as calling 
		PxArticulationReducedCoordinate::setRootGlobalPose() followed by PxArticulationReducedCoordinate::updateKinematic(PxArticulationKinematicFlag::ePOSITION).
		Similarly, calling applyCache(cache, PxArticulationCacheFlag::eROOT_VELOCITIES) is the cache equivalent of calling 
		PxArticulationReducedCoordinate::setRootLinearVelocity() followed by PxArticulationReducedCoordinate::updateKinematic(PxArticulationKinematicFlag::eVELOCITY).  
		Joint positions follow a similar pattern with applyCache(cache, PxArticulationCacheFlag::ePOSITION) having the same outcome as callling 
		PxArticulationJointReducedCoordinate::setJointPosition() followed by PxArticulationReducedCoordinate::updateKinematic(PxArticulationKinematicFlag::ePOSITION).  
		Finally, joint velocities updated with applyCache(PxArticulationCacheFlag::eVELOCITY) will produce the same outcome as calling 
		PxArticulationJointReducedCoordinate::setJointVelocity() followed by PxArticulationReducedCoordinate::updateKinematic(PxArticulationKinematicFlag::eVELOCITY).

		\note This method should not be used if the direct GPU API is enabled. See #PxDirectGPUAPI for the details.

		\see PxArticulationCache, PxArticulationCacheFlags, createCache, copyInternalStateToCache, PxScene::applyArticulationData
		*/
		virtual		void					applyCache(PxArticulationCache& cache, const PxArticulationCacheFlags flags, bool autowake = true) = 0;

		/**
		\brief Copies internal data of the articulation to the cache.

		\param[in] cache The articulation data.
		\param[in] flags Indicate which data to copy from the articulation to the cache.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\note This method should not be used if the direct GPU API is enabled. See #PxDirectGPUAPI for the details.

		\see PxArticulationCache, PxArticulationCacheFlags, createCache, applyCache
		*/
		virtual		void					copyInternalStateToCache(PxArticulationCache& cache, const PxArticulationCacheFlags flags) const = 0;


		/**
		\brief Converts maximal-coordinate joint DOF data to reduced coordinates.

		- Indexing into the maximal joint DOF data is via the link's low-level index minus 1 (the root link is not included).
		- The reduced-coordinate data follows the cache indexing convention, see PxArticulationCache::jointVelocity.

		\param[in]	maximum The maximal-coordinate joint DOF data with minimum array length N = (getNbLinks() - 1) * 6
		\param[out]	reduced	The reduced-coordinate joint DOF data with minimum array length N = getDofs()

		\note The articulation must be in a scene.

		\note This can be used as a helper function to prepare per joint cache data such as PxArticulationCache::jointVelocity.

		\see unpackJointData
		*/
		virtual		void					packJointData(const PxReal* maximum, PxReal* reduced) const = 0;

		/**
		\brief Converts reduced-coordinate joint DOF data to maximal coordinates.

		- Indexing into the maximal joint DOF data is via the link's low-level index minus 1 (the root link is not included).
		- The reduced-coordinate data follows the cache indexing convention, see PxArticulationCache::jointVelocity.

		\param[in]	reduced	The reduced-coordinate joint DOF data with minimum array length	N = getDofs().
		\param[out]	maximum The maximal-coordinate joint DOF data with minimum array length	N = (getNbLinks() - 1) * 6.

		\note The articulation must be in a scene.

		\see packJointData
		*/
		virtual		void					unpackJointData(const PxReal* reduced, PxReal* maximum) const = 0;

		/**
		\brief Prepares common articulation data based on articulation pose for inverse dynamics calculations.

		Usage:
		-# Set articulation pose (joint positions and base transform) via articulation cache and applyCache().
		-# Call commonInit.
		-# Call inverse dynamics computation method.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see computeGravityCompensation, computeCoriolisCompensation, computeGeneralizedExternalForce, computeJointAcceleration,
		computeJointForce, computeDenseJacobian, computeMassMatrix
		*/
		virtual		void					commonInit() const = 0;

		/**
		\brief Computes the forces required to counteract gravitational forces for the given articulation pose.

		In the case of a fixed-base articulation, the gravity compensation force accounts for the gravity on all the links and provides
		the force required to compensate the gravitational forces for all the joint DoFs.
		The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.

		In the case of a floating-base articulation, the gravity compensation force also accounts for the gravity on the root link and also provides
		the force on the root required to compensate its gravitational force. The indexing is:
			| Root force X | Root force Y | Root force Z | Root torque X | Root torque Y | Root torque Z | Force/Torque DOF 0 | ... | Force/Torque DOF N |

		- Inputs:	Articulation pose (joint positions + base transform).
		- Outputs:	Forces to counteract gravity (in cache).

		- The joint forces returned are determined purely by gravity for the articulation in the current joint and base pose, and joints at rest;
		  i.e. external forces, joint velocities, and joint accelerations are set to zero. Joint drives are also not considered in the computation.
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[out] cache Out: PxArticulationCache::gravityCompensationForce.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit
		*/
		virtual		void					computeGravityCompensation(PxArticulationCache& cache) const = 0;

		/**
		\brief Computes the joint DOF forces (and root force) required to counteract Coriolis and centrifugal forces for the given articulation state.

		In the case of a fixed-base articulation, the Coriolis and centrifugal compensation force accounts for forces resulting to the current
		joint velocities. The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.

		In the case of a floating-base articulation, the Coriolis and centrifugal compensation force also accounts for forces resulting to the current
		root velocity. The indexing is:
			| Root force X | Root force Y | Root force Z | Root torque X | Root torque Y | Root torque Z | Force/Torque DOF 0 | ... | Force/Torque DOF N |

		- Inputs:	Articulation state (joint positions and velocities (in cache), and base transform and spatial velocity).
		- Outputs:	Joint forces (and root force) to counteract Coriolis and centrifugal forces (in cache).

		- The forces returned are determined purely by the articulation's state; i.e. external forces, gravity, and joint accelerations are set to zero.
		  Joint drives and potential damping terms, such as link angular or linear damping, or joint friction, are also not considered in the computation.
		- Prior to the computation, update/set the base spatial velocity with PxArticulationCache::rootLinkData and applyCache().
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[in,out] cache In: PxArticulationCache::jointVelocity and PxArticulationCache::linkVelocity; Out: PxArticulationCache::coriolisForce.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit
		*/
		virtual		void					computeCoriolisCompensation(PxArticulationCache& cache) const = 0;

		/**
		\brief Computes the joint DOF forces required to counteract external spatial forces applied to articulation links.

		- Inputs:	External forces on links (in cache), articulation pose (joint positions + base transform).
		- Outputs:	Joint forces to counteract the external forces (in cache).

		- Only the external spatial forces provided in the cache and the articulation pose are considered in the computation.
		- The external spatial forces are with respect to the links' centers of mass, and not the actor's origin.
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[in,out] cache In: PxArticulationCache::externalForces; Out: PxArticulationCache::jointForce.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit
		*/
		virtual		void					computeGeneralizedExternalForce(PxArticulationCache& cache) const = 0;

		/**
		\brief Computes the joint accelerations for the given articulation state and joint forces.

		- Inputs:	Joint forces (in cache) and articulation state (joint positions and velocities (in cache), and base transform and spatial velocity).
		- Outputs:	Joint accelerations (in cache).

		- The computation includes Coriolis terms and gravity. However, joint drives, external forces, and potential damping (link damping, friction) terms
		are not considered in the computation.
		- Prior to the computation, update/set the base spatial velocity with PxArticulationCache::rootLinkData and applyCache().
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[in,out] cache In: PxArticulationCache::jointForce and PxArticulationCache::jointVelocity; Out: PxArticulationCache::jointAcceleration.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit
		*/
		virtual		void					computeJointAcceleration(PxArticulationCache& cache) const = 0;

		/**
		\brief Computes the joint forces for the given articulation pose and joint accelerations, not considering gravity and velocity.

		- Inputs:	Joint accelerations (in cache).
		- Outputs:	Joint forces (in cache).

		- Gravity, Coriolis effects, joint drives and potential damping terms are not considered in the computation
		(for example, linear link damping or joint friction).
		- To compute the joint force for a different pose, the joint positions and root transform first need to be applied with applyCache() as this function ignores any values set to joint positions and root transform in the cache
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[in,out] cache In: PxArticulationCache::jointAcceleration; Out: PxArticulationCache::jointForce.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit
		*/
		virtual		void					computeJointForce(PxArticulationCache& cache) const = 0;

		/**
		\brief Compute the dense Jacobian for the articulation in world space, including the DOFs of a potentially floating base.

		This computes the dense representation of an inherently sparse matrix. Multiplication with this matrix maps
		joint space velocities to world-space linear and angular (i.e. spatial) velocities of the centers of mass of the links.

		\param[out] cache Sets cache.denseJacobian matrix. The matrix is indexed [nCols * row + column].
		\param[out] nRows Set to number of rows in matrix, which corresponds to nbLinks() * 6, minus 6 if PxArticulationFlag::eFIX_BASE is true.
		\param[out] nCols Set to number of columns in matrix, which corresponds to the number of joint DOFs, plus 6 in the case PxArticulationFlag::eFIX_BASE is false.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.
		*/
		virtual		void					computeDenseJacobian(PxArticulationCache& cache, PxU32& nRows, PxU32& nCols) const = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Computes the coefficient matrix for contact forces.

		- The matrix dimension is getCoefficientMatrixSize() = getDofs() * getNbLoopJoints(), and the DOF (column) indexing follows the internal DOF order, see PxArticulationCache::jointVelocity.
		- Each column in the matrix is the joint forces effected by a contact based on impulse strength 1.
		- The user must allocate memory for PxArticulationCache::coefficientMatrix where the required size of the PxReal array is equal to getCoefficientMatrixSize().
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[out] cache Out: PxArticulationCache::coefficientMatrix.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit, getCoefficientMatrixSize
		*/
		virtual	PX_DEPRECATED	void		computeCoefficientMatrix(PxArticulationCache& cache) const = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Computes the lambda values when the test impulse is 1.

		- The user must allocate memory for PxArticulationCache::lambda where the required size of the PxReal array is equal to getNbLoopJoints().
		- commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[out] cache Out: PxArticulationCache::lambda.
		\param[in] initialState The initial state of the articulation system.
		\param[in] jointTorque M(q)*qddot + C(q,qdot) + g(q) <- calculate by summing joint forces obtained with computeJointForce and computeGravityCompensation.
		\param[in] maxIter Maximum number of solver iterations to run. If the system converges, fewer iterations may be used.

		\return True if convergence was achieved within maxIter; False if convergence was not achieved or the operation failed otherwise.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see commonInit, getNbLoopJoints
		*/
		virtual	PX_DEPRECATED	bool		computeLambda(PxArticulationCache& cache, PxArticulationCache& initialState, const PxReal* const jointTorque, const PxU32 maxIter) const = 0;

		/**
		\brief Compute the mass matrix M that maps accelerations to forces: forces = M * accelerations.

		In the case of a fixed-base articulation, the mass matrix maps joint accelerations to joint forces.
		The indexing follows the internal DOF index order, see PxArticulationCache::jointVelocity.

		In the case of a floating-base articulation, the mass matrix also includes terms required to map root accelerations
		to root forces. The mass matrix should be used with accelerations and forces that follows the indexing below:
			| Root force X       |     | Root linear acceleration X  |
			| Root force Y	     |     | Root linear acceleration Y  |
			| Root force Z       |     | Root linear acceleration Z  |
			| Root torque X      |     | Root angular acceleration X |
			| Root torque Y      |     | Root angular acceleration Y |
			| Root torque Z      | = M | Root angular acceleration Z |
			| Force/Torque DOF 0 |     | Joint acceleration 0        |
			| Force/Torque DOF 1 |     | Joint acceleration 1        |
			| ...                |     | ...                         |
			| Force/Torque DOF N |     | Joint acceleration N        |

		- Inputs:	Articulation pose (joint positions and base transform).
		- Outputs:	Mass matrix (in cache).

		commonInit() must be called before the computation, and after setting the articulation pose via applyCache().

		\param[out] cache Out: PxArticulationCache::massMatrix.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.
		\note The mass matrix is indexed [nCols * row + column].

		\see commonInit, PxArticulationCache::massMatrix
		*/
		virtual		void					computeMassMatrix(PxArticulationCache& cache) const = 0;

		/**
		\brief Compute the articulation's center of mass.

		\return The articulation's center of mass given either in the world frame (rootFrame = false) or in the root frame
		(rootFrame = true). PxVec3(0.0f) is returned if the articulation is not in a scene or the call is made during simulation.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.
		*/
		virtual		PxVec3				computeArticulationCOM(const bool rootFrame = false) const = 0;

		/**
		\brief Compute the centroidal momentum matrix and corresponding bias force of an articulation.

		- Inputs:	Articulation state (joint positions and velocities, and base transform and spatial velocity),
					articulation mass matrix, Coriolis and Centrifugal compensation forces.
		- Outputs:	Centroidal momentum matrix and bias force (in cache).

		commonInit(), computeMassMatrix() and computeCoriolisCompensation() must be called before the computation,
		and after setting the articulation pose and velocities via applyCache().

		\param[out] cache Out: PxArticulationCache::centroidalMomentumMatrix and PxArticulationCache::centroidalMomentumBias.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.
		This call may also only be made for floating-base articulations.

		\see commonInit, computeMassMatrix, computeCoriolisCompensation
		*/
		virtual		void					computeCentroidalMomentumMatrix(PxArticulationCache& cache) const = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Adds a loop joint to the articulation system for inverse dynamics.

		\param[in] joint The joint to add.

		\note This call may not be made during simulation.

		\see PxFixedJoint, PxSphericalJoint, PxRevoluteJoint, PxPrismaticJoint, PxDistanceJoint, PxD6Joint
		*/
		virtual	PX_DEPRECATED	void		addLoopJoint(PxConstraint* joint) = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Removes a loop joint from the articulation for inverse dynamics.

		\note This call may not be made during simulation.

		\param[in] joint The joint to remove.
		*/
		virtual	PX_DEPRECATED	void		removeLoopJoint(PxConstraint* joint) = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Returns the number of loop joints in the articulation for inverse dynamics.

		\return The number of loop joints.
		*/
		virtual	PX_DEPRECATED	PxU32		getNbLoopJoints() const = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.
		\brief Returns the set of loop constraints (i.e. joints) in the articulation.

		\param[in] userBuffer Target buffer for the constraint pointers.
		\param[in] bufferSize The size of the buffer. If this is not large enough to contain all the pointers to the constraints,
		only as many as will fit are written. Use getNbLoopJoints() to size the buffer for retrieving all constraints.
		\param[in] startIndex Index of first constraint pointer to be retrieved.

		\return The number of constraints written into the buffer.
		*/
		virtual	PX_DEPRECATED	PxU32		getLoopJoints(PxConstraint** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

		/**
		\deprecated The API related to loop joints will be removed in a future version once a replacement is made available.

		\brief Returns the required size of the coefficient matrix in the articulation.

		\return Size of the coefficient matrix (equal to getDofs() * getNbLoopJoints()).

		\note This call may only be made on articulations that are in a scene.

		\see computeCoefficientMatrix
		*/
		virtual	PX_DEPRECATED	PxU32		getCoefficientMatrixSize() const = 0;

		/**
		\brief Sets the root link transform in the world frame.

		- Use updateKinematic() after all state updates to the articulation via non-cache API such as this method,
		in order to update link states for the next simulation frame or querying.

		\param[in] pose The new root link transform.
		\param[in] autowake If true and the articulation is in a scene, the articulation will be woken up and the wake counter of 
		each link will be reset to #PxSceneDesc::wakeCounterResetValue.

		\note This call may not be made during simulation.

		\note PxArticulationCache::rootLinkData similarly allows the root link pose to be updated and potentially offers better performance 
		if the root link pose is to be updated along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see getRootGlobalPose, updateKinematic, PxArticulationCache, applyCache
		*/
		virtual		void					setRootGlobalPose(const PxTransform& pose, bool autowake = true) = 0;

		/**
		\brief Returns the root link transform (world to actor frame).

		\return The root link transform.

		\note This call is not allowed while the simulation is running except in a split simulation during #PxScene::collide() and up to #PxScene::advance(),
		and in PxContactModifyCallback or in contact report callbacks.

		\note PxArticulationCache::rootLinkData similarly allows the root link pose to be queried and potentially offers better performance if the root
		link pose is to be queried along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see setRootGlobalPose, PxArticulationCache, copyInternalStateToCache
		*/
		virtual		PxTransform				getRootGlobalPose() const = 0;

		/**
		\brief Sets the root link linear center-of-mass velocity.

		- The linear velocity is with respect to the link's center of mass and not the actor frame origin.
		- The articulation is woken up if the input velocity is nonzero (ignoring autowake) and the articulation is in a scene.
		- Use updateKinematic() after all state updates to the articulation via non-cache API such as this method,
		in order to update link states for the next simulation frame or querying.

		\param[in] linearVelocity The new root link center-of-mass linear velocity.
		\param[in] autowake If true and the articulation is in a scene, the call wakes up the articulation and increases the wake counter
		to #PxSceneDesc::wakeCounterResetValue if the counter value is below the reset value.

		\note This call may not be made during simulation, except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\note PxArticulationCache::rootLinkData similarly allows the root link linear velocity to be updated and potentially offers better performance 
		if the root link linear velocity is to be updated along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see updateKinematic, getRootLinearVelocity, setRootAngularVelocity, getRootAngularVelocity, PxRigidBody::getCMassLocalPose, PxArticulationCache, applyCache
		*/
		virtual		void					setRootLinearVelocity(const PxVec3& linearVelocity, bool autowake = true) = 0;

		/**
		\brief Gets the root link center-of-mass linear velocity.

		- The linear velocity is with respect to the link's center of mass and not the actor frame origin.

		\return The root link center-of-mass linear velocity.

		\note This call is not allowed while the simulation is running except in a split simulation during #PxScene::collide() and up to #PxScene::advance(),
		and in PxContactModifyCallback or in contact report callbacks.

		\note PxArticulationCache::rootLinkData similarly allows the root link linear velocity to be queried and potentially offers better performance 
		if the root link linear velocity is to be queried along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see setRootLinearVelocity, setRootAngularVelocity, getRootAngularVelocity, PxRigidBody::getCMassLocalPose, PxArticulationCache, applyCache
		*/
		virtual		PxVec3					getRootLinearVelocity() const = 0;

		/**
		\brief Sets the root link angular velocity.

		- The articulation is woken up if the input velocity is nonzero (ignoring autowake) and the articulation is in a scene.
		- Use updateKinematic() after all state updates to the articulation via non-cache API such as this method,
		in order to update link states for the next simulation frame or querying.

		\param[in] angularVelocity The new root link angular velocity.
		\param[in] autowake If true and the articulation is in a scene, the call wakes up the articulation and increases the wake counter
		to #PxSceneDesc::wakeCounterResetValue if the counter value is below the reset value.

		\note This call may not be made during simulation, except in a split simulation in-between #PxScene::fetchCollision and #PxScene::advance.

		\note PxArticulationCache::rootLinkData similarly allows the root link angular velocity to be updated and potentially offers better performance 
		if the root link angular velocity is to be updated along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see updateKinematic, getRootAngularVelocity, setRootLinearVelocity, getRootLinearVelocity, PxArticulationCache, applyCache
		*/
		virtual		void					setRootAngularVelocity(const PxVec3& angularVelocity, bool autowake = true) = 0;

		/**
		\brief Gets the root link angular velocity.

		\return The root link angular velocity.

		\note This call is not allowed while the simulation is running except in a split simulation during #PxScene::collide() and up to #PxScene::advance(),
		and in PxContactModifyCallback or in contact report callbacks.

		\note PxArticulationCache::rootLinkData similarly allows the root link angular velocity to be queried and potentially offers better performance 
		if the root link angular velocity is to be queried along with other state variables. 

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see setRootAngularVelocity, setRootLinearVelocity, getRootLinearVelocity, PxArticulationCache, applyCache
		*/
		virtual		PxVec3					getRootAngularVelocity() const = 0;

		/**
		\brief Returns the (classical) link acceleration in world space for the given low-level link index.

		- The returned acceleration is not a spatial, but a classical, i.e. body-fixed acceleration (https://en.wikipedia.org/wiki/Spatial_acceleration).
		- The (linear) acceleration is with respect to the link's center of mass and not the actor frame origin.

		\param[in] linkId The low-level link index, see PxArticulationLink::getLinkIndex.

		\return The link's center-of-mass classical acceleration, or 0 if the call is made before the articulation participated in a first simulation step.

		\note This call may only be made on articulations that are in a scene. It is not allowed to use this method while the simulation
		is running.  The exceptions to this rule are a split simulation during #PxScene::collide() and up to #PxScene::advance(); 
		in PxContactModifyCallback; and in contact report callbacks.

		\note This method should not be used after the direct GPU API has been enabled and initialized. See #PxDirectGPUAPI for the details.

		\see PxArticulationLink::getLinkIndex, PxRigidBody::getCMassLocalPose
		*/
		virtual		PxSpatialVelocity		getLinkAcceleration(const PxU32 linkId) = 0;

		/**
		\brief Returns the GPU articulation index.

		\return The GPU index, or 0xFFFFFFFF if the articulation is not in a scene.
		*/
		virtual 	PxArticulationGPUIndex				getGPUIndex() const = 0;

		/**
		\brief Creates a spatial tendon to attach to the articulation with default attribute values.

		\return The new spatial tendon.

		\note Creating a spatial tendon is not allowed while the articulation is in a scene. In order to
		add the tendon, remove and then re-add the articulation to the scene.

		\note The spatial tendon is released with PxArticulationReducedCoordinate::release()

		\see PxArticulationSpatialTendon
		*/
		virtual		PxArticulationSpatialTendon*	createSpatialTendon() = 0;

		/**
		\brief Creates a fixed tendon to attach to the articulation with default attribute values.

		\return The new fixed tendon.

		\note Creating a fixed tendon is not allowed while the articulation is in a scene. In order to
		add the tendon, remove and then re-add the articulation to the scene.

		\note The fixed tendon is released with PxArticulationReducedCoordinate::release()

		\see PxArticulationFixedTendon
		*/
		virtual		PxArticulationFixedTendon*		createFixedTendon() = 0;

		/**
		\brief Returns the spatial tendons attached to the articulation.

		The order of the tendons in the buffer is not necessarily identical to the order in which the tendons were added to the articulation.

		\param[in] userBuffer The buffer into which to write the array of pointers to the tendons.
		\param[in] bufferSize The size of the buffer. If this is not large enough to contain all the pointers to tendons,
		only as many as will fit are written. Use getNbSpatialTendons to size for all attached tendons.
		\param[in] startIndex Index of first tendon pointer to be retrieved.

		\return The number of tendons written into the buffer.

		\see PxArticulationSpatialTendon, getNbSpatialTendons
		*/
		virtual		PxU32					getSpatialTendons(PxArticulationSpatialTendon** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0)	const = 0;

		/**
		\brief Returns the number of spatial tendons in the articulation.

		\return The number of tendons.
		*/
		virtual		PxU32					getNbSpatialTendons() const = 0;

		/**
		\brief Returns the fixed tendons attached to the articulation.

		The order of the tendons in the buffer is not necessarily identical to the order in which the tendons were added to the articulation.

		\param[in] userBuffer The buffer into which to write the array of pointers to the tendons.
		\param[in] bufferSize The size of the buffer. If this is not large enough to contain all the pointers to tendons,
		only as many as will fit are written. Use getNbFixedTendons to size for all attached tendons.
		\param[in] startIndex Index of first tendon pointer to be retrieved.

		\return The number of tendons written into the buffer.

		\see PxArticulationFixedTendon, getNbFixedTendons
		*/
		virtual		PxU32					getFixedTendons(PxArticulationFixedTendon** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0)	const = 0;

		/**
		\brief Returns the number of fixed tendons in the articulation.

		\return The number of tendons.
		*/
		virtual		PxU32					getNbFixedTendons() const = 0;

		/**
		\brief Create a mimic joint that will enforce a relationship between two joints.
		\param[in] jointA is the first joint of the joint pair controlled by the mimic joint.
		\param[in] axisA specifies the degree of freedom of jointA that will be controlled by the mimic joint.
		\param[in] jointB is the second joint of the joint pair controlled by the mimic joint.
		\param[in] axisB specifies the degree of freedom of jointB that will be controlled by the mimic joint.
		\param[in] gearRatio is the gearing ratio enforced by the mimic joint.
		\param[in] naturalFrequency specifies the oscillation frequency of the mimic joint's compliance, specified in s^-1.
		\param[in] dampingRatio specifies the damping ratio of the mimic joint's compliance.
		\param[in] offset is the offset enforced by the mimic joint.
		\note If naturalFrequency is less than or equal to zero it is assumed that the mimic joint has no compliance and is a hard constraint.
		\note If dampingRatio is less than or equal to zero it is assumed that the mimic joint has no compliance and is a hard constraint.
		\note In the absence of compliance, the mimic joint enforces the rule: qA + gearRatio*qB + offset = 0 with qA denoting the 
		joint position of the specified degree of freedom of jointA and qB denoting the joint position of the specified degree of freedom of jointB.
		\note Larger values of naturalFrequency and dampingRatio will make the mimic joint stiffer and more akin to a hard constraint.
		\note A damping ratio less than 1.0 is not recommended.
		\note If dampingRatio is less than or equal to zero and naturalFrequency greater than zero, the mimic joint will behave as a hard constraint.  
		If dampingRatio is greater than zero and naturalFrequency less than or equal to zero, the mimic joint will also behave as a hard constraint.  
		*/
		virtual		PxArticulationMimicJoint*		createMimicJoint(
				const PxArticulationJointReducedCoordinate& jointA, PxArticulationAxis::Enum axisA, 
				const PxArticulationJointReducedCoordinate& jointB, PxArticulationAxis::Enum axisB, 
				PxReal gearRatio, PxReal offset, 
				PxReal naturalFrequency = 0.0f, PxReal dampingRatio = 0.0f) = 0;

		/**
		\brief Returns the mimic joints added to the articulation.

		The order of the mimic joints in the buffer is not necessarily identical to the order in which the mimic joints were added to the articulation.

		\param[in] userBuffer The buffer into which to write the array of pointers to the mimic joints.
		\param[in] bufferSize The size of the buffer. If this is not large enough to contain all the pointers to mimic joints,
		only as many as will fit are written. Use getNbMimicJoints() to size for all attached mimic joints.
		\param[in] startIndex Index of first mimic joints pointer to be retrieved.

		\return The number of mimic joints written into the buffer.

		\see PxArticulationMimicJoint, getNbMimicJoints
		*/
		virtual		PxU32							getMimicJoints(PxArticulationMimicJoint** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0)	const = 0;

		/**
		\brief Returns the number of mimic joints in the articulation.
		\return The number of mimic joints.
		*/
		virtual		 PxU32							getNbMimicJoints() const = 0;


		/**
		\brief Update link velocities and/or positions in the articulation.

	    An alternative that potentially offers better performance is to use the PxArticulationCache API.

		If the application updates the root state (position and velocity) or joint state via any combination of
		the non-cache API calls

		- setRootGlobalPose(), setRootLinearVelocity(), setRootAngularVelocity()
		- PxArticulationJointReducedCoordinate::setJointPosition(), PxArticulationJointReducedCoordinate::setJointVelocity()

		the application needs to call this method after the state setting in order to update the link states for
		the next simulation frame or querying.

		Use
		- PxArticulationKinematicFlag::ePOSITION after any changes to the articulation root or joint positions using non-cache API calls. Updates links' positions and velocities.
		- PxArticulationKinematicFlag::eVELOCITY after velocity-only changes to the articulation root or joints using non-cache API calls. Updates links' velocities only.

		\note This call may only be made on articulations that are in a scene, and may not be made during simulation.

		\see PxArticulationKinematicFlags, PxArticulationCache, applyCache
		*/
		virtual		void					updateKinematic(PxArticulationKinematicFlags flags) = 0;


		/**
		\brief Returns the string name of the dynamic type.

		\return The string name.
		*/
		virtual		const char*				getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL { return "PxArticulationReducedCoordinate"; }

		virtual								~PxArticulationReducedCoordinate() {}

		void*								userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

	protected:
		PX_INLINE							PxArticulationReducedCoordinate(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL)   {}
		PX_INLINE							PxArticulationReducedCoordinate(PxBaseFlags baseFlags) : PxBase(baseFlags) {}

		virtual		bool			isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxArticulationReducedCoordinate", PxBase); }
	};

#if PX_VC
#pragma warning(pop)
#endif

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
