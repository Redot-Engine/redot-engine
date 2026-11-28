// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_SURFACE_MATERIAL_H
#define PX_DEFORMABLE_SURFACE_MATERIAL_H

#include "PxDeformableMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

/**
\brief Material class to represent surface deformable material properties.

\see PxPhysics.createDeformableSurfaceMaterial
*/
class PxDeformableSurfaceMaterial : public PxDeformableMaterial
{
public:

	/**
	\brief Sets material thickness

	<b>Default:</b> 0.001
	\param[in] thickness Material thickness.

	\see getThickness()
	*/
	virtual		void			setThickness(PxReal thickness) = 0;

	/**
	\brief Retrieves the material thickness.
	
	<b>Default:</b> 0.001
	\return thickness.
	\see setThickness()
	*/
	virtual		PxReal			getThickness() const = 0;

	/**
	\brief Sets material bending stiffness

	<b>Default:</b> 0.0
	\param[in] bendingStiffness Material bending stiffness.
	\see getBendingStiffness()
	*/
	virtual		void			setBendingStiffness(PxReal bendingStiffness) = 0;

	/**
	\brief Retrieves the material bending stiffness.

	\return bendingStiffness.
	\see setBendingStiffness()
	*/
	virtual		PxReal			getBendingStiffness() const = 0;

	/**
	\brief Sets material bending damping

	\param[in] bendingDamping Material bending damping.

	\see getBendingDamping()
	*/
	virtual		void			setBendingDamping(PxReal bendingDamping) = 0;

	/**
	\brief Retrieves the material bending damping.
	\return bending damping.

	\see setBendingDamping()
	*/
	virtual		PxReal			getBendingDamping() const = 0;


	/**
	\brief Gets the concrete type name.
	\return The name of the concrete type.
	*/
	virtual		const char*		getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxDeformableSurfaceMaterial"; }

protected:
	PX_INLINE					PxDeformableSurfaceMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxDeformableMaterial(concreteType, baseFlags) {}
	PX_INLINE					PxDeformableSurfaceMaterial(PxBaseFlags baseFlags) : PxDeformableMaterial(baseFlags) {}
	virtual						~PxDeformableSurfaceMaterial() {}
	virtual		bool			isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableSurfaceMaterial", PxDeformableMaterial); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_SURFACE_MATERIAL_H
