// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_MATERIAL_H
#define PX_MATERIAL_H

#include "PxBaseMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxScene;

/**
\brief Flags which control the behavior of a material.

\see PxMaterial 
*/
struct PxMaterialFlag
{
	enum Enum
	{
		/**
		\brief If this flag is set, friction computations are always skipped between shapes with this material and any other shape.
		*/
		eDISABLE_FRICTION = 1 << 0,

		/**
		\brief Whether to use strong friction.
		The difference between "normal" and "strong" friction is that the strong friction feature
		remembers the "friction error" between simulation steps. The friction is a force trying to
		hold objects in place (or slow them down) and this is handled in the solver. But since the
		solver is only an approximation, the result of the friction calculation can include a small
		"error" - e.g. a box resting on a slope should not move at all if the static friction is in
		action, but could slowly glide down the slope because of a small friction error in each 
		simulation step. The strong friction counter-acts this by remembering the small error and
		taking it to account during the next simulation step.

		However, in some cases the strong friction could cause problems, and this is why it is
		possible to disable the strong friction feature by setting this flag. One example is
		raycast vehicles that are sliding fast across the surface, but still need a precise
		steering behavior. It may be a good idea to reenable the strong friction when objects
		are coming to a rest, to prevent them from slowly creeping down inclines.

		Note: This flag only has an effect if the PxMaterialFlag::eDISABLE_FRICTION bit is 0.
		*/
		eDISABLE_STRONG_FRICTION = 1 << 1,

		/**
		\brief If this flag is raised in combination with negative restitution, the computed spring-damper output will be interpreted as
		acceleration instead of force targets, analog to acceleration spring constraints.
		The flag has no effect for non-compliant contacts (i.e., if restitution is nonnegative).
		In an interaction between a compliant-force and a compliant-acceleration body the latter will dominate.
		\see PxMaterial.setRestitution, PxMaterial.setDamping
		\see Px1DConstraintFlag.eACCELERATION_SPRING
		*/
		eCOMPLIANT_ACCELERATION_SPRING = 1 << 4
	};
};

/**
\brief collection of set bits defined in PxMaterialFlag.

\see PxMaterialFlag
*/
typedef PxFlags<PxMaterialFlag::Enum,PxU16> PxMaterialFlags;
PX_FLAGS_OPERATORS(PxMaterialFlag::Enum,PxU16)


/**
\brief Enumeration that determines the way in which two material properties will be combined to yield a friction or restitution coefficient for a collision.

When two actors come in contact with each other, they each have materials with various coefficients, but we only need a single set of coefficients for the pair.

Physics doesn't have any inherent combinations because the coefficients are determined empirically on a case by case
basis. However, simulating this with a pairwise lookup table is often impractical.

For this reason the following combine behaviors are available:

eAVERAGE
eMIN
eMULTIPLY
eMAX

The effective combine mode for the pair is maximum(material0.combineMode, material1.combineMode).

Notes that the restitution coefficient is overloaded if it is negative and represents a spring stiffness for compliant contacts. In the compliant contact case, the following rules apply:
	* If a compliant (restitution < 0) material interacts with a rigid (restitution >= 0) material, the compliant behavior will be chosen independent
	of combine mode. In all other cases (i.e., also for compliant-compliant interactions) the combine mode is used.
	* For a compliant-compliant interaction with eMULTIPLY combine mode, we multiply the values but keep the sign negative.
	* The material damping follows the same logic, i.e., for the compliant vs non-compliant case, we take the damping value of the compliant material. Otherwise the combine mode is respected.
	* In an interaction between a compliant-force and a compliant-acceleration body the latter will dominate and exclusively determine the collision behavior with its parameters.

\see PxMaterial.setFrictionCombineMode() PxMaterial.getFrictionCombineMode() PxMaterial.setRestitutionCombineMode() PxMaterial.getFrictionCombineMode()
*/
struct PxCombineMode
{
	enum Enum
	{
		eAVERAGE	= 0,		//!< Average: (a + b)/2
		eMIN		= 1,		//!< Minimum: minimum(a,b)
		eMULTIPLY	= 2,		//!< Multiply: a*b
		eMAX		= 3,		//!< Maximum: maximum(a,b)
		eN_VALUES	= 4,		//!< This is not a valid combine mode, it is a sentinel to denote the number of possible values. We assert that the variable's value is smaller than this.
		ePAD_32		= 0x7fffffff //!< This is not a valid combine mode, it is to assure that the size of the enum type is big enough.
	};
};

/**
\brief Material class to represent a set of surface properties. 

\see PxPhysics.createMaterial
*/
class PxMaterial : public PxBaseMaterial
{
public:

	/**
	\brief Sets the coefficient of dynamic friction.
	
	The coefficient of dynamic friction should be in [0, PX_MAX_F32). If set to greater than staticFriction, the effective value of staticFriction will be increased to match.

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] coef Coefficient of dynamic friction. <b>Range:</b> [0, PX_MAX_F32)

	\see getDynamicFriction()
	*/
	virtual		void	setDynamicFriction(PxReal coef) = 0;

	/**
	\brief Retrieves the DynamicFriction value.

	\return The coefficient of dynamic friction.

	\see setDynamicFriction
	*/
	virtual		PxReal	getDynamicFriction() const = 0;

	/**
	\brief Sets the coefficient of static friction
	
	The coefficient of static friction should be in the range [0, PX_MAX_F32)

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] coef Coefficient of static friction. <b>Range:</b> [0, PX_MAX_F32)

	\see getStaticFriction() 
	*/
	virtual		void	setStaticFriction(PxReal coef) = 0;

	/**
	\brief Retrieves the coefficient of static friction.
	\return The coefficient of static friction.

	\see setStaticFriction 
	*/
	virtual		PxReal	getStaticFriction() const = 0;

	/**
	\brief Sets the coefficient of restitution or the spring stiffness for compliant contact
	
	A coefficient of 0 makes the object bounce as little as possible, higher values up to 1.0 result in more bounce.
	If a negative value is provided it is interpreted as stiffness term for an implicit spring
	simulated at the contact site, with the spring positional error defined by
	the contact separation value. Higher stiffness terms produce stiffer springs that behave more like a rigid contact.

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] rest Coefficient of restitution / negative spring stiffness <b>Range:</b> (-INF,1]

	\see getRestitution()  setDamping()
	*/
	virtual		void	setRestitution(PxReal rest) = 0;

	/**
	\brief Retrieves the coefficient of restitution.     

	See #setRestitution.

	\return The coefficient of restitution.

	\see setRestitution() 
	*/
	virtual		PxReal	getRestitution() const = 0;

	/**
	\brief Sets the coefficient of damping

	This property only affects the simulation if compliant contact mode is enabled, i.e., a negative restitution value is set.
	Damping works together with spring stiffness. Spring stiffness corrects positional error while
	damping resists relative velocity. Setting a high damping coefficient can produce spongy contacts.

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] damping Coefficient of damping. <b>Range:</b> [0,INF)

	\see getDamping()
	*/
	virtual		void	setDamping(PxReal damping) = 0;

	/**
	\brief Retrieves the coefficient of damping.

	See #setDamping.

	\return The coefficient of damping.

	\see setDamping()
	*/
	virtual		PxReal	getDamping() const = 0;

	/**
	\brief Raises or clears a particular material flag.
	
	See the list of flags #PxMaterialFlag

	<b>Default:</b> No flag raised.

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in]	flag	The PxMaterial flag to raise(set) or clear.
	\param[in]	b		New state of the flag

	\see getFlags() setFlags() PxMaterialFlag
	*/
	virtual		void	setFlag(PxMaterialFlag::Enum flag, bool b) = 0;

	/**
	\brief sets all the material flags.
	
	See the list of flags #PxMaterialFlag

	<b>Default:</b> No flag raised.

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in]	flags	All PxMaterial flags

	\see getFlags() setFlag() PxMaterialFlag
	*/
	virtual		void 	setFlags(PxMaterialFlags flags) = 0;

	/**
	\brief Retrieves the flags. See #PxMaterialFlag.

	\return The material flags.

	\see PxMaterialFlag setFlags()
	*/
	virtual		PxMaterialFlags	getFlags() const = 0;

	/**
	\brief Sets the friction combine mode.
	
	See the enum ::PxCombineMode .

	<b>Default:</b> PxCombineMode::eAVERAGE

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] combMode Friction combine mode to set for this material. See #PxCombineMode.

	\see PxCombineMode getFrictionCombineMode setStaticFriction() setDynamicFriction()
	*/
	virtual		void	setFrictionCombineMode(PxCombineMode::Enum combMode) = 0;

	/**
	\brief Retrieves the friction combine mode.
	
	See #setFrictionCombineMode.

	\return The friction combine mode for this material.

	\see PxCombineMode setFrictionCombineMode() 
	*/
	virtual		PxCombineMode::Enum	getFrictionCombineMode() const = 0;

	/**
	\brief Sets the restitution combine mode.
	
	See the enum ::PxCombineMode .

	<b>Default:</b> PxCombineMode::eAVERAGE

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] combMode Restitution combine mode for this material. See #PxCombineMode.

	\see PxCombineMode getRestitutionCombineMode() setRestitution()
	*/
	virtual		void	setRestitutionCombineMode(PxCombineMode::Enum combMode) = 0;

	/**
	\brief Retrieves the restitution combine mode.
	
	See #setRestitutionCombineMode.

	\return The coefficient of restitution combine mode for this material.

	\see PxCombineMode setRestitutionCombineMode getRestitution()
	*/
	virtual		PxCombineMode::Enum	getRestitutionCombineMode() const = 0;

	/**
	\brief Sets the damping combine mode.
	
	See the enum ::PxCombineMode .

	<b>Default:</b> PxCombineMode::eAVERAGE

	<b>Sleeping:</b> Does <b>NOT</b> wake any actors which may be affected.

	\param[in] combMode Damping combine mode for this material. See #PxCombineMode.

	\see PxCombineMode getDampingCombineMode() setDamping()
	*/
	virtual		void	setDampingCombineMode(PxCombineMode::Enum combMode) = 0;

	/**
	\brief Retrieves the damping combine mode.
	
	\return The damping combine mode for this material.

	\see PxCombineMode setDampingCombineMode() getDamping()
	*/
	virtual		PxCombineMode::Enum	getDampingCombineMode() const = 0;

	// PxBase
	virtual		const char*	getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxMaterial"; }
	//~PxBase

protected:
	PX_INLINE			PxMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxBaseMaterial(concreteType, baseFlags)	{}
	PX_INLINE			PxMaterial(PxBaseFlags baseFlags) : PxBaseMaterial(baseFlags) {}
	virtual				~PxMaterial() {}
	virtual		bool	isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxMaterial", PxBaseMaterial); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
