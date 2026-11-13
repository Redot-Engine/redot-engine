// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONSTRAINT_H
#define PX_CONSTRAINT_H

#include "PxPhysXConfig.h"
#include "PxConstraintDesc.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxRigidActor;
class PxScene;
class PxConstraintConnector;

/**
\brief constraint flags

\note eBROKEN is a read only flag
*/
struct PxConstraintFlag
{
	enum Enum
	{
		eBROKEN						= 1<<0,		//!< whether the constraint is broken
		eCOLLISION_ENABLED			= 1<<3,		//!< whether contacts should be generated between the objects this constraint constrains
		eVISUALIZATION				= 1<<4,		//!< whether this constraint should be visualized, if constraint visualization is turned on
		eDRIVE_LIMITS_ARE_FORCES	= 1<<5,		//!< \deprecated Will be removed in a future version and the limits will always be forces. limits for drive strength are forces rather than impulses
		eIMPROVED_SLERP				= 1<<7,		//!< perform preprocessing for improved accuracy on D6 Slerp Drive (this flag will be removed in a future release when preprocessing is no longer required)
		eDISABLE_PREPROCESSING		= 1<<8,		//!< suppress constraint preprocessing, intended for use with rowResponseThreshold. May result in worse solver accuracy for ill-conditioned constraints.
		eENABLE_EXTENDED_LIMITS		= 1<<9,		//!< enables extended limit ranges for angular limits (e.g., limit values > PxPi or < -PxPi)
		eGPU_COMPATIBLE				= 1<<10,	//!< please do not raise this flag as it is for internal use only
		eALWAYS_UPDATE				= 1<<11,	//!< updates the constraint each frame
		eDISABLE_CONSTRAINT			= 1<<12		//!< disables the constraint. SolverPrep functions won't be called for this constraint.
	};
};

/**
\brief constraint flags
\see PxConstraintFlag
*/
typedef PxFlags<PxConstraintFlag::Enum, PxU16> PxConstraintFlags;
PX_FLAGS_OPERATORS(PxConstraintFlag::Enum, PxU16)

/**
\brief a table of function pointers for a constraint

\see PxConstraint
*/
struct PxConstraintShaderTable
{
	PxConstraintSolverPrep	solverPrep;	//!< solver constraint generation function
	PxConstraintVisualize	visualize;	//!< constraint visualization function
	PxConstraintFlag::Enum	flag;		//!< constraint flags
};

/**
\brief A plugin class for implementing constraints

\see PxPhysics.createConstraint
*/
class PxConstraint : public PxBase
{
public:

	/**
	\brief Releases a PxConstraint instance.

	\note This call does not wake up the connected rigid bodies.

	\see PxPhysics.createConstraint, PxBase.release()
	*/
	virtual void				release()														PX_OVERRIDE = 0;

	/**
	\brief Retrieves the scene which this constraint belongs to.

	\return Owner Scene. NULL if not part of a scene.

	\see PxScene
	*/
	virtual PxScene*			getScene()												const	= 0;

	/**
	\brief Retrieves the actors for this constraint.

	\param[out] actor0 a reference to the pointer for the first actor
	\param[out] actor1 a reference to the pointer for the second actor

	\see PxActor
	*/
	virtual void				getActors(PxRigidActor*& actor0, PxRigidActor*& actor1)	const	= 0;

	/**
	\brief Sets the actors for this constraint.

	\param[in] actor0 a reference to the pointer for the first actor
	\param[in] actor1 a reference to the pointer for the second actor

	\see PxActor
	*/
	virtual void				setActors(PxRigidActor* actor0, PxRigidActor* actor1)			= 0;

	/**
	\brief Notify the scene that the constraint shader data has been updated by the application
	*/
	virtual void				markDirty()														= 0;

	/**
	\brief Retrieve the flags for this constraint

	\return the constraint flags
	\see PxConstraintFlags
	*/
	virtual PxConstraintFlags	getFlags()												const	= 0;

	/**
	\brief Set the flags for this constraint

	\param[in] flags the new constraint flags

	default: PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES

	\see PxConstraintFlags
	*/
	virtual void				setFlags(PxConstraintFlags flags)								= 0;

	/**
	\brief Set a flag for this constraint

	\param[in] flag the constraint flag
	\param[in] value the new value of the flag

	\see PxConstraintFlags
	*/
	virtual void				setFlag(PxConstraintFlag::Enum flag, bool value)				= 0;

	/**
	\brief Retrieve the constraint force most recently applied to maintain this constraint.

	\note It is not allowed to use this method while the simulation is running (except during PxScene::collide(),
	in PxContactModifyCallback or in contact report callbacks).
	
	\param[out] linear the constraint force
	\param[out] angular the constraint torque
	*/
	virtual void				getForce(PxVec3& linear, PxVec3& angular)				const	= 0;

	/**
	\brief whether the constraint is valid. 
	
	A constraint is valid if it has at least one dynamic rigid body or articulation link. A constraint that
	is not valid may not be inserted into a scene, and therefore a static actor to which an invalid constraint
	is attached may not be inserted into a scene.

	Invalid constraints arise only when an actor to which the constraint is attached has been deleted.
	*/
	virtual bool				isValid() const													= 0;

	/**
	\brief Set the break force and torque thresholds for this constraint. 
	
	If either the force or torque measured at the constraint exceed these thresholds the constraint will break.

	\param[in] linear the linear break threshold
	\param[in] angular the angular break threshold
	*/
	virtual	void				setBreakForce(PxReal linear, PxReal angular)				= 0;

	/**
	\brief Retrieve the constraint break force and torque thresholds
	
	\param[out] linear the linear break threshold
	\param[out] angular the angular break threshold
	*/
	virtual	void				getBreakForce(PxReal& linear, PxReal& angular)		const	= 0;

	/**
	\brief Set the minimum response threshold for a constraint row 
	
	When using mass modification for a joint or infinite inertia for a jointed body, very stiff solver constraints can be generated which 
	can destabilize simulation. Setting this value to a small positive value (e.g. 1e-8) will cause constraint rows to be ignored if very 
	large changes in impulses will generate only small changes in velocity. When setting this value, also set 
	PxConstraintFlag::eDISABLE_PREPROCESSING. The solver accuracy for this joint may be reduced.

	\param[in] threshold the minimum response threshold

	\see PxConstraintFlag::eDISABLE_PREPROCESSING
	*/
	virtual	void				setMinResponseThreshold(PxReal threshold)					= 0;

	/**
	\brief Retrieve the constraint break force and torque thresholds

	\return the minimum response threshold for a constraint row
	*/
	virtual	PxReal				getMinResponseThreshold()							const	= 0;

	/**
	\brief Fetch external owner of the constraint.
	
	Provides a reference to the external owner of a constraint and a unique owner type ID.

	\param[out] typeID Unique type identifier of the external object.
	\return Reference to the external object which owns the constraint.

	\see PxConstraintConnector.getExternalReference()
	*/
	virtual void*				getExternalReference(PxU32& typeID)							= 0;

	/**
	\brief Set the constraint functions for this constraint
	
	\param[in] connector the constraint connector object by which the SDK communicates with the constraint.
	\param[in] shaders the shader table for the constraint
 
	\see PxConstraintConnector PxConstraintSolverPrep PxConstraintVisualize
	*/
	virtual	void				setConstraintFunctions(PxConstraintConnector& connector, const PxConstraintShaderTable& shaders)	= 0;

	virtual	const char*			getConcreteTypeName() const PX_OVERRIDE	PX_FINAL { return "PxConstraint"; }

			void*				userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	PX_INLINE					PxConstraint(PxType concreteType, PxBaseFlags baseFlags) : PxBase(concreteType, baseFlags), userData(NULL) {}
	PX_INLINE					PxConstraint(PxBaseFlags baseFlags) : PxBase(baseFlags)	{}
	virtual						~PxConstraint() {}
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxConstraint", PxBase); }

public:
	/**
	\cond
	*/

	// for internal use only
	virtual PxConstraintGPUIndex getGPUIndex() const = 0;

	/**
	\endcond
	*/
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
