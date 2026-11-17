// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_D6_JOINT_H
#define PX_D6_JOINT_H

#include "extensions/PxJoint.h"
#include "extensions/PxJointLimit.h"
#include "foundation/PxFlags.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxD6Joint;

/**
\brief Create a D6 joint.

 \param[in] physics		The physics SDK
 \param[in] actor0		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame0	The position and orientation of the joint relative to actor0
 \param[in] actor1		An actor to which the joint is attached. NULL may be used to attach the joint to a specific point in the world frame
 \param[in] localFrame1	The position and orientation of the joint relative to actor1 

\see PxD6Joint
*/
PxD6Joint*	PxD6JointCreate(PxPhysics& physics, PxRigidActor* actor0, const PxTransform& localFrame0, PxRigidActor* actor1, const PxTransform& localFrame1);

/**
\brief Used to specify one of the degrees of freedom of  a D6 joint.

\see PxD6Joint
*/
struct PxD6Axis
{
	enum Enum
	{
		eX      = 0,	//!< motion along the X axis
		eY      = 1,	//!< motion along the Y axis
		eZ      = 2,	//!< motion along the Z axis
		eTWIST  = 3,	//!< motion around the X axis
		eSWING1 = 4,	//!< motion around the Y axis
		eSWING2 = 5,	//!< motion around the Z axis
		eCOUNT	= 6
	};
};


/**
\brief Used to specify the range of motions allowed for a degree of freedom in a D6 joint.

\see PxD6Joint
*/
struct PxD6Motion
{
	enum Enum
	{
		eLOCKED,	//!< The DOF is locked, it does not allow relative motion.
		eLIMITED,	//!< The DOF is limited, it only allows motion within a specific range.
		eFREE		//!< The DOF is free and has its full range of motion.
	};
};


/**
\brief The configuration to use for driving to the angular component of a target pose or velocity.

\see PxD6Joint::setAngularDriveConfig() PxD6Drive PxD6Joint::setDrive()
*/
struct PxD6AngularDriveConfig
{
	enum Enum
	{
		/**
		\brief The joint tries to reach the angular drive target by separately driving along each angular degree of freedom.

		Each angular degree of freedom can have its own set of drive parameters. The degrees of freedom are covered by a twist and two swing axes.
		As a consequence, only the following options are available when setting the drive parameters: PxD6Drive::eSWING1, PxD6Drive::eSWING2, 
		PxD6Drive::eTWIST (see #PxD6Joint::setDrive()).
		*/
		eSWING_TWIST,

		/**
		\brief The joint tries to reach the angular drive target by following a spherical linear interpolation (SLERP) based path.

		A single set of drive parameters will be used for all angular degrees of freedom and PxD6Drive::eSLERP is the only valid option to set
		those parameters (see #PxD6Joint::setDrive()).
		*/
		eSLERP
	};
};


/**
\brief Used to specify which axes of a D6 joint are driven. 

Each drive is an implicit force-limited damped spring:

force = spring * (target position - position) + damping * (targetVelocity - velocity)

Alternatively, the spring may be configured to generate a specified acceleration instead of a force.

A linear axis is affected by drive only if the corresponding drive flag is set. There are two possible models
for angular drive: swing/twist, which may be used to drive one or more angular degrees of freedom, or slerp,
which may only be used to drive all three angular degrees simultaneously. Please use #PxD6AngularDriveConfig
to configure the angular drive model.

\see PxD6Joint PxD6AngularDriveConfig
*/
struct PxD6Drive
{
	enum Enum
	{
		eX						= 0,	//!< drive along the X-axis
		eY						= 1,	//!< drive along the Y-axis
		eZ						= 2,	//!< drive along the Z-axis

		/**
		\brief rotational drive around the X-axis

		\note Only allowed if the angular drive configuration is set to PxD6AngularDriveConfig::eSWING_TWIST.
		*/
		eTWIST					= 3,

		/**
		\brief rotational drive around the Y-axis

		\note Only allowed if the angular drive configuration is set to PxD6AngularDriveConfig::eSWING_TWIST.
		*/
		eSWING1					= 4,

		/**
		\brief rotational drive around the Z-axis

		\note Only allowed if the angular drive configuration is set to PxD6AngularDriveConfig::eSWING_TWIST.
		*/
		eSWING2					= 5,

		/**
		\brief drive of all three angular degrees along a SLERP-path

		\note Only allowed if the angular drive configuration is set to PxD6AngularDriveConfig::eSLERP.
		*/
		eSLERP					= 6,

		eCOUNT					= 7
	};
};

/** 
\brief flags for configuring the drive model of a PxD6Joint

\see PxD6JointDrive PxD6Joint
*/
struct PxD6JointDriveFlag
{
	enum Enum
	{
		eACCELERATION	= (1 << 0),	//!< drive spring is for the acceleration at the joint (rather than the force) 

		/**
		\brief Add drive force/torque to the joint force/torque total.

		If this flag is raised, the force/torque value from this drive constraint will be accumulated
		in the force/torque total that is reported for the underlying PxConstraint object. Note that
		because the force/torque total changes, the joint break behavior will change too.

		<b>Default:</b> False

		\see PxConstraint::getForce()
		*/
		eOUTPUT_FORCE	= (1 << 1)
	};
};
typedef PxFlags<PxD6JointDriveFlag::Enum, PxU32> PxD6JointDriveFlags;
PX_FLAGS_OPERATORS(PxD6JointDriveFlag::Enum, PxU32)

/** 
\brief parameters for configuring the drive model of a PxD6Joint

\see PxD6Joint
*/
class PxD6JointDrive : public PxSpring
{
public:
	PxReal					forceLimit;			//!< the force limit of the drive - may be an impulse or a force depending on PxConstraintFlag::eDRIVE_LIMITS_ARE_FORCES
	PxD6JointDriveFlags		flags;				//!< the joint drive flags 

	/**
	\brief default constructor for PxD6JointDrive.
	*/
	PxD6JointDrive(): PxSpring(0,0), forceLimit(PX_MAX_F32), flags(0) {}

	/**
	\brief constructor a PxD6JointDrive.

	\param[in] driveStiffness	The stiffness of the drive spring.
	\param[in] driveDamping		The damping of the drive spring
	\param[in] driveForceLimit	The maximum impulse or force that can be exerted by the drive
	\param[in] isAcceleration	Whether the drive is an acceleration drive or a force drive
	*/
	PxD6JointDrive(PxReal driveStiffness, PxReal driveDamping, PxReal driveForceLimit, bool isAcceleration = false)
	: PxSpring(driveStiffness, driveDamping)
	, forceLimit(driveForceLimit)
	, flags(isAcceleration?PxU32(PxD6JointDriveFlag::eACCELERATION) : 0) 
	{}

	/** 
	\brief returns true if the drive is valid
	*/
	bool isValid() const
	{
		return PxIsFinite(stiffness) && stiffness>=0 &&
			   PxIsFinite(damping) && damping >=0 &&
			   PxIsFinite(forceLimit) && forceLimit >=0;
	}
};


/**
 \brief A D6 joint is a general constraint between two actors.
 
 It allows the application to individually define the linear and rotational degrees of freedom, 
 and also to configure a variety of limits and driven degrees of freedom.

 By default all degrees of freedom are locked. So to create a prismatic joint with free motion 
 along the x-axis:

 \code	
    ...
    joint->setMotion(PxD6Axis::eX, PxD6JointMotion::eFREE);
     ...
 \endcode

 Or a Revolute joint with motion free allowed around the x-axis:

 \code
    ... 
	joint->setMotion(PxD6Axis::eTWIST, PxD6JointMotion::eFREE);
    ...
 \endcode

 Degrees of freedom may also be set to limited instead of locked.
 
 There are two different kinds of linear limits available. The first kind is a single limit value
 for all linear degrees of freedom, which may act as a linear, circular, or spherical limit depending
 on which degrees of freedom are limited. This is similar to a distance limit. Then, the second kind
 supports a pair of limit values for each linear axis, which can be used to implement a traditional
 prismatic joint for example.

 If the twist degree of freedom is limited, is supports upper and lower limits. The two swing degrees
 of freedom are limited with a cone limit.
\see PxD6JointCreate() PxJoint 
*/
class PxD6Joint : public PxJoint
{
public:

	/**
	\brief Set the motion type around the specified axis.

	Each axis may independently specify that the degree of freedom is locked (blocking relative movement
	along or around this axis), limited by the corresponding limit, or free.

	\param[in] axis the axis around which motion is specified
	\param[in] type the motion type around the specified axis

	<b>Default:</b> all degrees of freedom are locked

	\see getMotion() PxD6Axis PxD6Motion
	*/
	virtual void				setMotion(PxD6Axis::Enum axis, PxD6Motion::Enum type)	= 0;

	/**
	\brief Get the motion type around the specified axis.

	\see setMotion() PxD6Axis PxD6Motion

	\param[in] axis the degree of freedom around which the motion type is specified
	\return the motion type around the specified axis
	*/
	virtual PxD6Motion::Enum	getMotion(PxD6Axis::Enum axis)	const	= 0;

	/**
	\brief get the twist angle of the joint, in the range (-2*Pi, 2*Pi]
	*/
	virtual PxReal				getTwistAngle()	const	= 0;

	/**
	\brief get the swing angle of the joint from the Y axis
	*/
	virtual PxReal				getSwingYAngle()	const	= 0;

	/**
	\brief get the swing angle of the joint from the Z axis
	*/
	virtual PxReal				getSwingZAngle()	const	= 0;

	/**
	\brief Set the distance limit for the joint. 

	A single limit constraints all linear limited degrees of freedom, forming a linear, circular 
	or spherical constraint on motion depending on the number of limited degrees. This is similar
	to a distance limit.

	\param[in] limit the distance limit structure

	\see getDistanceLimit() PxJointLinearLimit
	*/
	virtual	void				setDistanceLimit(const PxJointLinearLimit& limit)	= 0;

	/**
	\brief Get the distance limit for the joint. 

	\return the distance limit structure

	\see setDistanceLimit() PxJointLinearLimit
	*/
	virtual	PxJointLinearLimit	getDistanceLimit()	const	= 0;

	/**
	\brief Set the linear limit for a given linear axis. 

	This function extends the previous setDistanceLimit call with the following features:
	- there can be a different limit for each linear axis
	- each limit is defined by two values, i.e. it can now be asymmetric

	This can be used to create prismatic joints similar to PxPrismaticJoint, or point-in-quad joints,
	or point-in-box joints.

	\param[in] axis		The limited linear axis (must be PxD6Axis::eX, PxD6Axis::eY or PxD6Axis::eZ)
	\param[in] limit	The linear limit pair structure

	\see getLinearLimit() 
	*/
	virtual void					setLinearLimit(PxD6Axis::Enum axis, const PxJointLinearLimitPair& limit)	= 0;

	/**
	\brief Get the linear limit for a given linear axis. 

	\param[in] axis		The limited linear axis (must be PxD6Axis::eX, PxD6Axis::eY or PxD6Axis::eZ)

	\return the linear limit pair structure from desired axis

	\see setLinearLimit() PxJointLinearLimit
	*/
	virtual PxJointLinearLimitPair	getLinearLimit(PxD6Axis::Enum axis)	const	= 0;

	/**
	\brief Set the twist limit for the joint. 

	The twist limit controls the range of motion around the twist axis. 

	The limit angle range is (-2*Pi, 2*Pi).

	\param[in] limit the twist limit structure

	\see getTwistLimit() PxJointAngularLimitPair
	*/
	virtual	void				setTwistLimit(const PxJointAngularLimitPair& limit)	= 0;

	/**
	\brief Get the twist limit for the joint. 

	\return the twist limit structure

	\see setTwistLimit() PxJointAngularLimitPair
	*/
	virtual	PxJointAngularLimitPair	getTwistLimit()	const	= 0;

	/**
	\brief Set the swing cone limit for the joint. 

	The cone limit is used if either or both swing axes are limited. The extents are 
	symmetrical and measured in the frame of the parent. If only one swing degree of freedom 
	is limited, the corresponding value from the cone limit defines the limit range.

	\param[in] limit the cone limit structure

	\see getLimitCone() PxJointLimitCone 
	*/
	virtual	void				setSwingLimit(const PxJointLimitCone& limit)	= 0;

	/**
	\brief Get the cone limit for the joint. 

	\return the swing limit structure

	\see setLimitCone() PxJointLimitCone
	*/
	virtual	PxJointLimitCone	getSwingLimit()	const	= 0;

	/**
	\brief Set a pyramidal swing limit for the joint. 

	The pyramid limits will only be used in the following cases:
	- both swing Y and Z are limited. The limit shape is then a pyramid.
	- Y is limited and Z is locked, or vice versa. The limit shape is an asymmetric angular section, similar to
	what is supported for the twist axis.
	The remaining cases (Y limited and Z is free, or vice versa) are not supported.

	\param[in] limit the cone limit structure

	\see getLimitCone() PxJointLimitPyramid 
	*/
	virtual	void				setPyramidSwingLimit(const PxJointLimitPyramid& limit)	= 0;

	/**
	\brief Get the pyramidal swing limit for the joint. 

	\return the swing limit structure

	\see setLimitCone() PxJointLimitPyramid
	*/
	virtual	PxJointLimitPyramid	getPyramidSwingLimit()	const	= 0;

	/**
	\brief Set the angular drive model to apply.

	\note The configuration will limit the allowed set of angular drive types (see #PxD6Drive) to use
	      when calling #PxD6Joint::setDrive().

	\note Changing the angular drive model, will reset all the parameters for the angular drives to
	      their default values (see #PxD6Joint::setDrive() for information on the default values).

	\param[in] config The angular drive model to apply.

	\see PxD6AngularDriveConfig getAngularDriveConfig()

	<b>Default</b> PxD6AngularDriveConfig::eSWING_TWIST
	*/
	virtual void setAngularDriveConfig(PxD6AngularDriveConfig::Enum config) = 0;

	/**
	\brief Get the angular drive model to apply.

	\return The angular drive model to apply.

	\see PxD6AngularDriveConfig setAngularDriveConfig()
	*/
	virtual PxD6AngularDriveConfig::Enum getAngularDriveConfig() const = 0;

	/**
	\brief Set the drive parameters for the specified drive type.

	\note The angular drive configuration (see #PxD6AngularDriveConfig) defines what type of
	      angular drives will be accepted.

	\param[in] index the type of drive being specified
	\param[in] drive the drive parameters

	\see getDrive() PxD6JointDrive PxD6AngularDriveConfig

	<b>Default</b> The default drive spring and damping values are zero, the force limit is PX_MAX_F32, and no flags are set.
	*/
	virtual void				setDrive(PxD6Drive::Enum index, const PxD6JointDrive& drive)	= 0;

	/**
	\brief Get the drive parameters for the specified drive type. 

	\note The angular drive configuration (see #PxD6AngularDriveConfig) defines what type of
	      angular drives will be accepted.

	\param[in] index the specified drive type

	\see setDrive() PxD6JointDrive PxD6AngularDriveConfig
	*/
	virtual PxD6JointDrive		getDrive(PxD6Drive::Enum index)	const	= 0;

	/**
	\brief Set the drive goal pose 

	The goal is relative to the constraint frame of actor[0]

	<b>Default</b> the identity transform

	\param[in] pose The goal drive pose if positional drive is in use. 
	\param[in] autowake If true and the attached actors are in a scene, this call wakes them up and increases their
	wake counters to #PxSceneDesc::wakeCounterResetValue if the counter value is below the reset value.

	\see setDrivePosition()
	*/
	virtual void				setDrivePosition(const PxTransform& pose, bool autowake = true)	= 0;

	/**
	\brief Get the drive goal pose.

	\see getDrivePosition()
	*/
	virtual PxTransform			getDrivePosition()	const	= 0;

	/**
	\brief Set the target goal velocity for drive.

	The velocity is measured in the constraint frame of actor[0]

	\param[in] linear The goal velocity for linear drive
	\param[in] angular The goal velocity for angular drive
	\param[in] autowake If true and the attached actors are in a scene, this call wakes them up and increases their
	wake counters to #PxSceneDesc::wakeCounterResetValue if the counter value is below the reset value.

	\see getDriveVelocity()
	*/
	virtual	void				setDriveVelocity(const PxVec3& linear, const PxVec3& angular, bool autowake = true)	= 0;

	/**
	\brief Get the target goal velocity for joint drive.

	\param[in] linear The goal velocity for linear drive
	\param[in] angular The goal velocity for angular drive

	\see setDriveVelocity()
	*/
	virtual void				getDriveVelocity(PxVec3& linear, PxVec3& angular)	const	= 0;

	/**
	\brief Returns the GPU D6 joint index.

	\note Only use in combination with enabled GPU dynamics and enabled direct GPU API
	      (see #PxSceneFlag::eENABLE_GPU_DYNAMICS, #PxSceneFlag::eENABLE_DIRECT_GPU_API,
		  #PxBroadPhaseType::eGPU)

	\return The GPU index, or PX_INVALID_D6_JOINT_GPU_INDEX if the joint is not part of a PxScene.

	\see PxDirectGPUAPI::getD6JointData()
	*/
	virtual PxD6JointGPUIndex getGPUIndex() const = 0;

	/**
	\brief Returns string name of PxD6Joint, used for serialization
	*/
	virtual	const char*			getConcreteTypeName() const	PX_OVERRIDE	{ return "PxD6Joint"; }

protected:

	//serialization

	/**
	\brief Constructor
	*/
	PX_INLINE					PxD6Joint(PxType concreteType, PxBaseFlags baseFlags) : PxJoint(concreteType, baseFlags) {}

	/**
	\brief Deserialization constructor
	*/
	PX_INLINE					PxD6Joint(PxBaseFlags baseFlags) : PxJoint(baseFlags) {}

	/**
	\brief Returns whether a given type name matches with the type of this instance
	*/
	virtual	bool				isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxD6Joint", PxJoint); }

	//~serialization
	
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
