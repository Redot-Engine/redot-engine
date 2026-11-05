// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_MATERIAL_H
#define PX_DEFORMABLE_MATERIAL_H

#include "PxPhysXConfig.h"
#include "PxBaseMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxScene;

/**
\brief Material class to represent a set of deformable material properties.

\see PxPhysics.createDeformableVolumeMaterial
*/
class PxDeformableMaterial : public PxBaseMaterial
{
public:

	/**
	\brief Sets young's modulus which defines the body's stiffness

	<b>Default:</b> 1.e6
	\param[in] young Young's modulus. <b>Range:</b> [0, PX_MAX_F32)
	\see getYoungsModulus()
	*/
	virtual		void	setYoungsModulus(PxReal young) = 0;

	/**
	\brief Retrieves the young's modulus value.

	\return The young's modulus value.

	\see setYoungsModulus()
	*/
	virtual		PxReal	getYoungsModulus() const = 0;

	/**
	\brief Sets the Poisson's ratio which defines the body's volume preservation.

	<b>Default:</b> 0.45
	\param[in] poisson Poisson's ratio. <b>Range:</b> [0, 0.5]

	\see getPoissons()
	*/
	virtual		void	setPoissons(PxReal poisson) = 0;

	/**
	\brief Retrieves the Poisson's ratio.
	\return The Poisson's ratio.

	\see setPoissons()
	*/
	virtual		PxReal	getPoissons() const = 0;

	/**
	\brief Sets the dynamic friction value which defines the strength of resistance when two objects slide relative to each other while in contact.

	<b>Default:</b> 0.0
	\param[in] dynamicFriction The dynamic friction value. <b>Range:</b> [0, PX_MAX_F32)

	\see getDynamicFriction()
	*/
	virtual		void	setDynamicFriction(PxReal dynamicFriction) = 0;

	/**
	\brief Retrieves the dynamic friction value
	\return The dynamic friction value

	\see setDynamicFriction()
	*/
	virtual		PxReal	getDynamicFriction() const = 0;

	/**
	\brief Sets material damping

	\param[in] elasticityDamping Material damping.

	\see getDamping()
	*/
	virtual		void			setElasticityDamping(PxReal elasticityDamping) = 0;

	/**
	\brief Retrieves the material damping.
	\return damping.

	\see setDamping()
	*/
	virtual		PxReal			getElasticityDamping() const = 0;

protected:
	PX_INLINE			PxDeformableMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxBaseMaterial(concreteType, baseFlags)	{}
	PX_INLINE			PxDeformableMaterial(PxBaseFlags baseFlags) : PxBaseMaterial(baseFlags) {}
	virtual				~PxDeformableMaterial() {}
	virtual		bool	isKindOf(const char* name) const { PX_IS_KIND_OF(name, "PxDeformableMaterial", PxBaseMaterial); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_MATERIAL_H
