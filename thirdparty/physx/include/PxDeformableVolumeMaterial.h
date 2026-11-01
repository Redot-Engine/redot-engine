// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_DEFORMABLE_VOLUME_MATERIAL_H
#define PX_DEFORMABLE_VOLUME_MATERIAL_H

#include "PxDeformableMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

struct PxDeformableVolumeMaterialModel
{
	enum Enum
	{
		eCO_ROTATIONAL,   //!< Default model. Well suited for high stiffness. Does need tetrahedra with good shapes (no extreme slivers) in the rest pose.
		eNEO_HOOKEAN      //!< Well suited for lower stiffness. Robust to any tetrahedron shape.
	};
};

class PxScene;
/**
\brief Material class to represent a set of deformable volume material properties.

\see PxPhysics.createDeformableVolumeMaterial
*/
class PxDeformableVolumeMaterial : public PxDeformableMaterial
{
public:

	/**
	\brief Sets the material model.

	\param[in] model The material model

	\see getMaterialModel
	*/
	virtual		void	setMaterialModel(PxDeformableVolumeMaterialModel::Enum model) = 0;
	
	/**
	\brief Retrieves the material model.
	\return The material model.

	\see setMaterialModel()
	*/
	virtual		PxDeformableVolumeMaterialModel::Enum getMaterialModel() const = 0;

	/**
	\brief Gets the concrete type name.
	\return The name of the concrete type.
	*/
	virtual		const char*		getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxDeformableVolumeMaterial"; }

protected:
	PX_INLINE			PxDeformableVolumeMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxDeformableMaterial(concreteType, baseFlags) {}
	PX_INLINE			PxDeformableVolumeMaterial(PxBaseFlags baseFlags) : PxDeformableMaterial(baseFlags) {}
	virtual				~PxDeformableVolumeMaterial() {}
	virtual		bool	isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxDeformableVolumeMaterial", PxDeformableMaterial); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif // PX_DEFORMABLE_VOLUME_MATERIAL_H
