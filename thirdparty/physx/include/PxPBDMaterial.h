// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PBD_MATERIAL_H
#define PX_PBD_MATERIAL_H

#include "PxBaseMaterial.h"
#include "PxParticleMaterial.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

	class PxScene;
	/**
	\brief Material class to represent a set of PBD particle material properties.

	\see #PxPhysics.createPBDMaterial
	*/
	class PxPBDMaterial : public PxBaseMaterial
	{
	public:
		
		/**
		\brief Sets friction

		\param[in] friction Friction. <b>Range:</b> [0, PX_MAX_F32)

		\see #getFriction()
		*/
		virtual		void	setFriction(PxReal friction) = 0;

		/**
		\brief Retrieves the friction value.

		\return The friction value.

		\see #setFriction()
		*/
		virtual		PxReal	getFriction() const = 0;

		/**
		\brief Sets velocity damping term

		\param[in] damping Velocity damping term. <b>Range:</b> [0, PX_MAX_F32)

		\see #getDamping
		*/
		virtual		void	setDamping(PxReal damping) = 0;

		/**
		\brief Retrieves the velocity damping term
		\return The velocity damping term.

		\see #setDamping()
		*/
		virtual		PxReal	getDamping() const = 0;

		/**
		\brief Sets adhesion term

		\param[in] adhesion adhesion coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getAdhesion
		*/
		virtual		void	setAdhesion(PxReal adhesion) = 0;

		/**
		\brief Retrieves the adhesion term
		\return The adhesion term.

		\see #setAdhesion()
		*/
		virtual		PxReal	getAdhesion() const = 0;

		/**
		\brief Sets gravity scale term

		\param[in] scale gravity scale coefficient. <b>Range:</b> (-PX_MAX_F32, PX_MAX_F32)

		\see #getAdhesion
		*/
		virtual		void	setGravityScale(PxReal scale) = 0;

		/**
		\brief Retrieves the gravity scale term
		\return The gravity scale term.

		\see #setAdhesion()
		*/
		virtual		PxReal	getGravityScale() const = 0;

		/**
		\brief Sets material adhesion radius scale. This is multiplied by the particle rest offset to compute the fall-off distance
		at which point adhesion ceases to operate.

		\param[in] scale Material adhesion radius scale. <b>Range:</b> [0, PX_MAX_F32)

		\see #getAdhesionRadiusScale
		*/
		virtual		void	setAdhesionRadiusScale(PxReal scale) = 0;

		/**
		\brief Retrieves the adhesion radius scale.
		\return The adhesion radius scale.

		\see #setAdhesionRadiusScale()
		*/
		virtual		PxReal	getAdhesionRadiusScale() const = 0;

		/**
		\brief Sets viscosity
	
		\param[in] viscosity Viscosity. <b>Range:</b> [0, PX_MAX_F32)

		\see #getViscosity()
		*/
		virtual		void	setViscosity(PxReal viscosity) = 0;

		/**
		\brief Retrieves the viscosity value.

		\return The viscosity value.

		\see #setViscosity()
		*/
		virtual		PxReal	getViscosity() const = 0;

		/**
		\brief Sets material vorticity confinement coefficient

		\param[in] vorticityConfinement Material vorticity confinement coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getVorticityConfinement()
		*/
		virtual		void	setVorticityConfinement(PxReal vorticityConfinement) = 0;

		/**
		\brief Retrieves the vorticity confinement coefficient.
		\return The vorticity confinement coefficient.

		\see #setVorticityConfinement()
		*/
		virtual		PxReal	getVorticityConfinement() const = 0;

		/**
		\brief Sets material surface tension coefficient

		\param[in] surfaceTension Material surface tension coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getSurfaceTension()
		*/
		virtual		void	setSurfaceTension(PxReal surfaceTension) = 0;

		/**
		\brief Retrieves the surface tension coefficient.
		\return The surface tension coefficient.

		\see #setSurfaceTension()
		*/
		virtual		PxReal	getSurfaceTension() const = 0;

		/**
		\brief Sets material cohesion coefficient

		\param[in] cohesion Material cohesion coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getCohesion()
		*/
		virtual		void	setCohesion(PxReal cohesion) = 0;

		/**
		\brief Retrieves the cohesion coefficient.
		\return The cohesion coefficient.

		\see #setCohesion()
		*/
		virtual		PxReal	getCohesion() const = 0;

		/**
		\brief Sets material lift coefficient

		\deprecated Particle-cloth, -rigids, -attachments and -volumes have been deprecated.

		\param[in] lift Material lift coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getLift()
		*/
		PX_DEPRECATED virtual void setLift(PxReal lift) = 0;

		/**
		\brief Retrieves the lift coefficient.

		\deprecated Particle-cloth, -rigids, -attachments and -volumes have been deprecated.

		\return The lift coefficient.

		\see #setLift()
		*/
		PX_DEPRECATED virtual PxReal getLift() const = 0;

		/**
		\brief Sets material drag coefficient

		\deprecated Particle-cloth, -rigids, -attachments and -volumes have been deprecated.

		\param[in] drag Material drag coefficient. <b>Range:</b> [0, PX_MAX_F32)

		\see #getDrag()
		*/
		PX_DEPRECATED virtual void setDrag(PxReal drag) = 0;

		/**
		\brief Retrieves the drag coefficient.

		\deprecated Particle-cloth, -rigids, -attachments and -volumes have been deprecated.

		\return The drag coefficient.

		\see #setDrag()
		*/
		PX_DEPRECATED virtual PxReal getDrag() const = 0;

		/**
		\brief Sets the CFL coefficient. Limits the relative motion between two approaching fluid particles.

		The distance to which the motion is clamped is defined by CFLcoefficient*particleContactOffset*2.
		A value of 0.5 will thus limit the appoaching motion to a distance of particleContactOffset.
		A value much larger than one will typically not limit the motion of the particles.
		
		\param[in] coefficient CFL coefficient. <b>Range:</b> [0, PX_MAX_F32), <b>Default:</b> 1.0

		\see #getCFLCoefficient()
		*/
		virtual		void	setCFLCoefficient(PxReal coefficient) = 0;

		/**
		\brief Retrieves the CFL coefficient.
		\return The CFL coefficient.

		\see #setCFLCoefficient()
		*/
		virtual		PxReal	getCFLCoefficient() const = 0;

		/**
		\brief Sets material particle friction scale. This allows the application to scale up/down the frictional effect between particles independent of the friction 
		coefficient, which also defines frictional behavior between the particle and rigid bodies/soft bodies/cloth etc.

		\param[in] scale particle friction scale. <b>Range:</b> [0, PX_MAX_F32)

		\see #getParticleFrictionScale()
		*/
		virtual		void	setParticleFrictionScale(PxReal scale) = 0;

		/**
		\brief Retrieves the particle friction scale.
		\return The particle friction scale.

		\see #setParticleFrictionScale()
		*/
		virtual		PxReal	getParticleFrictionScale() const = 0;

		/**
		\brief Sets material particle adhesion scale value. This is the adhesive value between particles defined as a scaled multiple of the adhesion parameter.

		\param[in] adhesion particle adhesion scale value. <b>Range:</b> [0, PX_MAX_F32)

		\see #getParticleAdhesionScale()
		*/
		virtual		void	setParticleAdhesionScale(PxReal adhesion) = 0;

		/**
		\brief Retrieves the particle adhesion scale value.
		\return The particle adhesion scale value.

		\see #setParticleAdhesionScale()
		*/
		virtual		PxReal	getParticleAdhesionScale() const = 0;

		virtual		const char*		getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxPBDMaterial"; }

	protected:
		PX_INLINE			PxPBDMaterial(PxType concreteType, PxBaseFlags baseFlags) : PxBaseMaterial(concreteType, baseFlags) {}
		PX_INLINE			PxPBDMaterial(PxBaseFlags baseFlags) : PxBaseMaterial(baseFlags) {}
		virtual				~PxPBDMaterial() {}
		virtual		bool	isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxPBDMaterial", PxBaseMaterial); }
	};
#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
