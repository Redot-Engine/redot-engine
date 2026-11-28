// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_CONSTRAINT_DESC_H
#define PX_CONSTRAINT_DESC_H


#include "PxPhysXConfig.h"
#include "foundation/PxFlags.h"
#include "foundation/PxVec3.h"
#include "foundation/PxTransform.h"
#include "common/PxBase.h"

#if !PX_DOXYGEN
namespace physx { namespace pvdsdk {
#endif
	class PvdDataStream;
#if !PX_DOXYGEN
}}
#endif

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
 \brief Constraint row flags

 These flags configure the post-processing of constraint rows and the behavior of the solver while solving constraints
*/
struct Px1DConstraintFlag
{
	PX_CUDA_CALLABLE Px1DConstraintFlag(){}

	enum Type
	{
		eSPRING					= 1<<0,	//!< whether the constraint is a spring. Mutually exclusive with eRESTITUTION. If set, eKEEPBIAS is ignored.
		eACCELERATION_SPRING	= 1<<1,	//!< whether the constraint is a force or acceleration spring. Only valid if eSPRING is set.
		eRESTITUTION			= 1<<2,	//!< whether the restitution model should be applied to generate the target velocity. Mutually exclusive with eSPRING. If restitution causes a bounces, eKEEPBIAS is ignored
		eKEEPBIAS				= 1<<3,	//!< whether to keep the error term when solving for velocity. Ignored if restitution generates bounce, or eSPRING is set.
		eOUTPUT_FORCE			= 1<<4,	//!< whether to accumulate the force value from this constraint in the force total that is reported for the constraint and tested for breakage
		eHAS_DRIVE_LIMIT		= 1<<5,	//!< whether the constraint has a drive force limit (which will be scaled by dt unless #PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES is set)
		eANGULAR_CONSTRAINT		= 1<<6,	//!< whether this is an angular or linear constraint
	};
};

typedef PxFlags<Px1DConstraintFlag::Type, PxU16> Px1DConstraintFlags;
PX_FLAGS_OPERATORS(Px1DConstraintFlag::Type, PxU16)

/**
\brief Constraint type hints which the solver uses to optimize constraint handling
*/
struct PxConstraintSolveHint
{
	enum Enum
	{
		eNONE					= 0,		//!< no special properties
		eACCELERATION1			= 256,		//!< a group of acceleration drive constraints with the same stiffness and drive parameters
		eSLERP_SPRING			= 258,		//!< temporary special value to identify SLERP drive rows
		eACCELERATION2			= 512,		//!< a group of acceleration drive constraints with the same stiffness and drive parameters
		eACCELERATION3			= 768,		//!< a group of acceleration drive constraints with the same stiffness and drive parameters
		eROTATIONAL_EQUALITY	= 1024,		//!< for internal purpose only, please do not use.
		eROTATIONAL_INEQUALITY	= 1025,		//!< for internal purpose only, please do not use.

		/**
		\brief Mark as equality constraint.

		If a 1D constraint is an equality constraint with [-PX_MAX_FLT, PX_MAX_FLT] force limits and a velocity target equal zero, then this
		flag can be raised to allow the solver to internally change the jacobian of this constraint and have it being orthogonalized relative
		to other equality constraints in the same PxConstraint (unless PxConstraintFlag::eDISABLE_PREPROCESSING is set). This can improve
		the convergence when solving the constraints.
		*/
		eEQUALITY				= 2048,

		/**
		\brief Mark as inequality constraint.

		If a 1D constraint is an inequality constraint with [0, PX_MAX_FLT] force limits, then this flag can be raised to allow the solver
		to internally change the jacobian of this constraint and have it being orthogonalized relative to the equality constraints in the
		same PxConstraint (unless PxConstraintFlag::eDISABLE_PREPROCESSING is set). This can improve the convergence when solving the
		constraints.
		*/
		eINEQUALITY				= 2049
	};
};

/**
\brief A one-dimensional constraint that constrains the relative motion of two rigid bodies.

A constraint is expressed as a set of 1-dimensional constraint rows which define the required constraint
on the objects' velocities. 

The constraint Jacobian J is specified by the parameters linear0, angular0, linear1, angular1 as follows

	J = {linear0, angular0, -linear1, -angular1}

The velocity target of the constraint is specified by Px1DConstraint::velocityTarget and the geometric error of the constraint 
is specified by Px1DConstraint::geometricError.

The output of the constraint is a velocity state (sdot = ds/dt with s denoting the constraint state) expressed in the world frame:

	sdot = {linearVelocity0, angularVelocity0, linearVelocity1, angularVelocity1}

with linearVelocity0 and angularVelocity0 denoting the linear and angular velocity of body0 of the constraint; 
and linearVelocity1 and angularVelocity1 denoting the linear and angular velocity of body1 of the constraint.

The constraint seeks an updated sdot that obeys a simple constraint rule:

	J*sdot + BaumgarteTerm*geometricError/dt - velocityTarget = 0

where BaumgarteTerm is a multiplier in range (0, 1). The Baumgarte term is not exposed but is instead internally 
set according to a simple metric chosen to enhance numerical stability. If the PGS solver is employed then dt is 
taken from the scene timestep.

Another way of expressing the constraint rule is as follows:

	linear0.dot(linearVelocity0) + angular0.dot(angularVelocity0) 
	- linear1.dot(linearVelocity1) - angular1.dot(angularVelocity1)
	+ BaumgarteTerm*geometricError/dt - velocityTarget = 0

The PhysX solver runs two phases: position iterations followed by velocity iterations. Position iterations derive
the velocity that is used to integrate the transform of a rigid body. Velocity iterations on the other hand derive
the final velocity of a rigid body. The constraint rule presented above only gets applied during position iterations,
during velocity iterations the geometricError term is usually ignored and the applied constraint rule is:

	J*sdot - velocityTarget = 0

The flag Px1DConstraintFlag::eKEEPBIAS can be used to have velocity iterations apply the same constraint rule as
position iterations.

A 1d constraint may be either a restitution constraint or a hard constraint or a spring constraint.

Restitution constraints have two modes of operation, depending on the speed of the constraint. These two modes are: 
a) a bounce mode that employs a restitution value specified by RestitutionModifiers::restitution
b) a non-bounce mode that employs zero restitution and ignores RestitutionModifiers::restitution.
The constraint speed immediately before the solver begins is computed as follows:
    constraintPreSolverSpeed = J * sdotPreSolver 
with sdotPreSolver denoting the rigid body velocities recorded after applying external forces and torques to the rigid bodies 
but before the solver begins.
If the bounce mode is active, the pre solver velocity is expected to flip direction and have restitution applied:
     bounceSpeed = -restitution * constraintPreSolverSpeed
Restitution will kick in if the following conditions are met:
\li -constraintPreSolverSpeed exceeds the bounce threshold (RestitutionModifiers::velocityThreshold)
\li (bounceSpeed * Px1DConstraint::geometricError) <= 0 (bounceSpeed points in the
opposite direction of the geometric error)
If these hold, then the provided Px1DConstraint::geometricError and Px1DConstraint::velocityTarget parameter will get overriden
internally. The former will get set to zero, the latter will get set to bounceSpeed. If restitution does not activate because
the listed conditions are not met, then the target velocity will be taken from the value stored in velocityTarget and the
geometric error will be taken from the value stored in geometricError.
RestitutionModifiers::restitution may be greater than 1 and may be less than 0 ie it is not limited to 0 <= restitution <= 1.

Hard constraints attempt to find sdot that satisfies the constraint equation:

	J*sdot + BaumgarteTerm*geometricError/dt - velocityTarget = 0

Spring constraints are quite different from restitution and hard constraints in that they attempt to compute and apply a spring force as follows:

    F = stiffness * -geometricError + damping * (velocityTarget - J*sdot)

where F is the constraint force or acceleration and J*sdot is the instantaneous constraint speed. Springs are
implemented with a fully implicit time-stepping scheme: that is, the force or acceleration is a function of the position
and velocity after the solve. Note that F gets applied to the first rigid body and -F to the second rigid body.

All constraints support limits on the minimum or maximum impulse applied.
*/

PX_ALIGN_PREFIX(16)
struct Px1DConstraint
{
	PxVec3			linear0;			//!< linear component of velocity jacobian in world space
	PxReal			geometricError;		//!< geometric error of the constraint along this axis
	PxVec3			angular0;			//!< angular component of velocity jacobian in world space
	PxReal			velocityTarget;		//!< velocity target for the constraint along this axis

	PxVec3			linear1;			//!< linear component of velocity jacobian in world space
	PxReal			minImpulse;			//!< minimum impulse the solver may apply to enforce this constraint
	PxVec3			angular1;			//!< angular component of velocity jacobian in world space
	PxReal			maxImpulse;			//!< maximum impulse the solver may apply to enforce this constraint

	union
	{
		struct SpringModifiers
		{
			PxReal	stiffness;			//!< spring parameter, for spring constraints
			PxReal	damping;			//!< damping parameter, for spring constraints
		} spring;
		struct RestitutionModifiers
		{
			PxReal	restitution;		//!< restitution parameter for determining additional "bounce"
			PxReal	velocityThreshold;	//!< minimum impact velocity for bounce
		} bounce;
	} mods;

	PxU16			flags;				//!< a set of Px1DConstraintFlags
	PxU16			solveHint;			//!< constraint optimization hint, should be an element of PxConstraintSolveHint

	PxU32			pad;  // for padding only
}
PX_ALIGN_SUFFIX(16);

/** 
\brief Flags for determining which components of the constraint should be visualized.

\see PxConstraintVisualize
*/
struct PxConstraintVisualizationFlag
{
	enum Enum
	{
		eLOCAL_FRAMES	= 1,	//!< visualize constraint frames
		eLIMITS			= 2		//!< visualize constraint limits
	};
};

/**
\brief Struct for specifying mass scaling for a pair of rigids
*/
PX_ALIGN_PREFIX(16)
struct PxConstraintInvMassScale
{
	PxReal linear0;		//!< multiplier for inverse mass of body0
	PxReal angular0;	//!< multiplier for inverse MoI of body0
	PxReal linear1;		//!< multiplier for inverse mass of body1
	PxReal angular1;	//!< multiplier for inverse MoI of body1

	PX_CUDA_CALLABLE PX_FORCE_INLINE PxConstraintInvMassScale(){}
	PX_CUDA_CALLABLE PX_FORCE_INLINE PxConstraintInvMassScale(PxReal lin0, PxReal ang0, PxReal lin1, PxReal ang1) : linear0(lin0), angular0(ang0), linear1(lin1), angular1(ang1){}
}
PX_ALIGN_SUFFIX(16);

/**
\brief Solver constraint generation shader

This function is called by the constraint solver framework. The function must be reentrant, since it may be called simultaneously
from multiple threads, and should access only the arguments passed into it.

Developers writing custom constraints are encouraged to read the documentation in the user guide and the implementation code in PhysXExtensions.

\param[out] constraints			An array of solver constraint rows to be filled in
\param[out] bodyAWorldOffset	The origin point (offset from the position vector of bodyA's center of mass) at which the constraint is resolved. This value does not affect how constraints are solved, only the constraint force reported. 
\param[in] maxConstraints		The size of the constraint buffer. At most this many constraints rows may be written
\param[out] invMassScale		The inverse mass and inertia scales for the constraint
\param[in] constantBlock		The constant data block
\param[in] bodyAToWorld			The center of mass frame of the first constrained body (the identity transform if the first actor is static, or if a NULL actor pointer was provided for it)
\param[in] bodyBToWorld			The center of mass frame of the second constrained body (the identity transform if the second actor is static, or if a NULL actor pointer was provided for it)
\param[in] useExtendedLimits	Enables limit ranges outside of (-PI, PI)
\param[out] cAtW				The world space location of body A's joint frame (position only)
\param[out] cBtW				The world space location of body B's joint frame (position only)

\return the number of constraint rows written.
*/
typedef PxU32 (*PxConstraintSolverPrep)(Px1DConstraint* constraints,
										PxVec3p& bodyAWorldOffset,
										PxU32 maxConstraints,
										PxConstraintInvMassScale& invMassScale,
										const void* constantBlock,
										const PxTransform& bodyAToWorld,
										const PxTransform& bodyBToWorld,
										bool useExtendedLimits,
										PxVec3p& cAtW,
										PxVec3p& cBtW);

/**
\brief API used to visualize details about a constraint.
*/
class PxConstraintVisualizer
{
protected:
	virtual ~PxConstraintVisualizer(){}
public:
	/** \brief Visualize joint frames

	\param[in] parent	Parent transformation
	\param[in] child	Child transformation
	*/
	virtual void visualizeJointFrames(const PxTransform& parent, const PxTransform& child) = 0;

	/** \brief Visualize joint linear limit

	\param[in] t0		Base transformation
	\param[in] t1		End transformation
	\param[in] value	Distance
	*/
	virtual void visualizeLinearLimit(const PxTransform& t0, const PxTransform& t1, PxReal value) = 0;

	/** \brief Visualize joint angular limit

	\param[in] t0		Transformation for the visualization
	\param[in] lower	Lower limit angle
	\param[in] upper	Upper limit angle
	*/
	virtual void visualizeAngularLimit(const PxTransform& t0, PxReal lower, PxReal upper) = 0;

	/** \brief Visualize limit cone

	\param[in] t			Transformation for the visualization
	\param[in] tanQSwingY	Tangent of the quarter Y angle 
	\param[in] tanQSwingZ	Tangent of the quarter Z angle 
	*/
	virtual void visualizeLimitCone(const PxTransform& t, PxReal tanQSwingY, PxReal tanQSwingZ) = 0;

	/** \brief Visualize joint double cone

	\param[in] t		Transformation for the visualization
	\param[in] angle	Limit angle
	*/
	virtual void visualizeDoubleCone(const PxTransform& t, PxReal angle) = 0;

	/** \brief Visualize line

	\param[in] p0	Start position
	\param[in] p1	End postion
	\param[in] color Color
	*/
	virtual void visualizeLine(const PxVec3& p0, const PxVec3& p1, PxU32 color) = 0;
};

/** \brief Solver constraint visualization function

This function is called by the constraint post-solver framework to visualize the constraint

\param[out] visualizer		The render buffer to render to
\param[in] constantBlock	The constant data block
\param[in] body0Transform	The center of mass frame of the first constrained body (the identity if the actor is static, or a NULL pointer was provided for it)
\param[in] body1Transform	The center of mass frame of the second constrained body (the identity if the actor is static, or a NULL pointer was provided for it)
\param[in] flags			The visualization flags (PxConstraintVisualizationFlag)

\see PxRenderBuffer 
*/
typedef void (*PxConstraintVisualize)(PxConstraintVisualizer& visualizer,
									  const void* constantBlock,
									  const PxTransform& body0Transform,
									  const PxTransform& body1Transform,
									  PxU32 flags);

/**
\brief Flags for determining how PVD should serialize a constraint update

\see PxConstraintConnector::updatePvdProperties, PvdSceneClient::updateConstraint
*/
struct PxPvdUpdateType
{
	enum Enum
	{
		CREATE_INSTANCE, //!< triggers createPvdInstance call, creates an instance of a constraint
		RELEASE_INSTANCE, //!< triggers releasePvdInstance call, releases an instance of a constraint
		UPDATE_ALL_PROPERTIES, //!< triggers updatePvdProperties call, updates all properties of a constraint
		UPDATE_SIM_PROPERTIES //!< triggers simUpdate call, updates all simulation properties of a constraint
	};
};

/**
\brief This class connects a custom constraint to the SDK

This class connects a custom constraint to the SDK, and functions are called by the SDK
to query the custom implementation for specific information to pass on to the application
or inform the constraint when the application makes calls into the SDK which will update
the custom constraint's internal implementation
*/
class PxConstraintConnector
{
public:
	/** \brief Pre-simulation data preparation
	when the constraint is marked dirty, this function is called at the start of the simulation
	step for the SDK to copy the constraint data block.
	*/
	virtual void*	prepareData()	= 0;

	/** 
	\brief this function is called by the SDK to update PVD's view of it
	*/
	virtual bool	updatePvdProperties(physx::pvdsdk::PvdDataStream& pvdConnection,
										const PxConstraint* c,
										PxPvdUpdateType::Enum updateType) const		= 0;

	/**
	\brief this function is called by the SDK to update OmniPVD's view of it
	*/
	virtual void	updateOmniPvdProperties() const		= 0;

	/**
	\brief Constraint release callback

	When the SDK deletes a PxConstraint object this function is called by the SDK. In general
	custom constraints should not be deleted directly by applications: rather, the constraint
	should respond to a release() request by calling PxConstraint::release(), then wait for
	this call to release its own resources.
	
	This function is also called when a PxConstraint object is deleted on cleanup due to 
	destruction of the PxPhysics object.
	*/
	virtual void	onConstraintRelease()	= 0;

	/**
	\brief Center-of-mass shift callback

	This function is called by the SDK when the CoM of one of the actors is moved. Since the
	API specifies constraint positions relative to actors, and the constraint shader functions
	are supplied with coordinates relative to bodies, some synchronization is usually required
	when the application moves an object's center of mass.
	*/
	virtual void	onComShift(PxU32 actor)	= 0;

	/** 
	\brief Origin shift callback

	This function is called by the SDK when the scene origin gets shifted and allows to adjust
	custom data which contains world space transforms.

	\note If the adjustments affect constraint shader data, it is necessary to call PxConstraint::markDirty()
	to make sure that the data gets synced at the beginning of the next simulation step.

	\param[in] shift Translation vector the origin is shifted by.

	\see PxScene.shiftOrigin()
	*/
	virtual void	onOriginShift(const PxVec3& shift)	= 0;

	/**
	\brief Fetches external data for a constraint.
	
	This function is used by the SDK to acquire a reference to the owner of a constraint and a unique
	owner type ID. This information will be passed on when a breakable constraint breaks or when
	#PxConstraint::getExternalReference() is called.

	\param[out] typeID Unique type identifier of the external object. The value 0xffffffff is reserved and should not be used. Furthermore, if the PhysX extensions library is used, some other IDs are reserved already (see PxConstraintExtIDs)
	\return Reference to the external object which owns the constraint.

	\see PxConstraintInfo PxSimulationEventCallback.onConstraintBreak()
	*/
	virtual void*	getExternalReference(PxU32& typeID)	= 0;

	/**
	\brief Obtain a reference to a PxBase interface if the constraint has one.

	If the constraint does not implement the PxBase interface, it should return NULL. 
	*/
	virtual PxBase*	getSerializable()	= 0;

	/**
	\brief Obtain the shader function pointer used to prep rows for this constraint
	*/
	virtual PxConstraintSolverPrep	getPrep()	const	= 0;

	/**
	\brief Obtain the pointer to the constraint's constant data
	*/
	virtual const void*	getConstantBlock()	const	= 0;

	/**
	\brief Let the connector know it has been connected to a constraint.
	*/
	virtual	void	connectToConstraint(PxConstraint*)	{}

	/**
	\brief virtual destructor
	*/
	virtual	~PxConstraintConnector() {}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
