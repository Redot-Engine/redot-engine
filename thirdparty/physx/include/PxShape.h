// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_SHAPE_H
#define PX_SHAPE_H

#include "PxPhysXConfig.h"
#include "common/PxBase.h"
#include "foundation/PxSimpleTypes.h"
#include "geometry/PxGeometry.h"
#include "geometry/PxGeometryHelpers.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxBoxGeometry;
class PxSphereGeometry;
class PxCapsuleGeometry;
class PxPlaneGeometry;
class PxConvexMeshGeometry;
class PxTriangleMeshGeometry;
class PxTetrahedronMeshGeometry;
class PxHeightFieldGeometry;
class PxParticleSystemGeometry;
class PxRigidActor;
struct PxFilterData;
class PxBaseMaterial;
class PxMaterial;
class PxDeformableSurfaceMaterial;
class PxDeformableVolumeMaterial;

/**
\brief Flags which affect the behavior of PxShapes.

\see PxShape PxShape.setFlag()
*/
struct PxShapeFlag
{
	enum Enum
	{
		/**
		\brief The shape will partake in collision in the physical simulation.

		\note It is illegal to raise the eSIMULATION_SHAPE and eTRIGGER_SHAPE flags.
		In the event that one of these flags is already raised the sdk will reject any 
		attempt to raise the other.  To raise the eSIMULATION_SHAPE first ensure that 
		eTRIGGER_SHAPE is already lowered.

		\note This flag has no effect if simulation is disabled for the corresponding actor (see #PxActorFlag::eDISABLE_SIMULATION).

		\see PxSimulationEventCallback.onContact() PxScene.setSimulationEventCallback() PxShape.setFlag(), PxShape.setFlags()
		*/
		eSIMULATION_SHAPE				= (1<<0),

		/**
		\brief The shape will partake in scene queries (ray casts, overlap tests, sweeps, ...).
		*/
		eSCENE_QUERY_SHAPE				= (1<<1),

		/**
		\brief The shape is a trigger which can send reports whenever other shapes enter/leave its volume.

		\note Triangle meshes and heightfields can not be triggers. Shape creation will fail in these cases.

		\note Shapes marked as triggers do not collide with other objects. If an object should act both
		as a trigger shape and a collision shape then create a rigid body with two shapes, one being a 
		trigger shape and the other a collision shape. 	It is illegal to raise the eTRIGGER_SHAPE and 
		eSIMULATION_SHAPE flags on a single PxShape instance.  In the event that one of these flags is already 
		raised the sdk will reject any attempt to raise the other.  To raise the eTRIGGER_SHAPE flag first 
		ensure that eSIMULATION_SHAPE flag is already lowered.

		\note Trigger shapes will no longer send notification events for interactions with other trigger shapes.

		\note Shapes marked as triggers are allowed to participate in scene queries, provided the eSCENE_QUERY_SHAPE flag is set. 

		\note This flag has no effect if simulation is disabled for the corresponding actor (see #PxActorFlag::eDISABLE_SIMULATION).

		\see PxSimulationEventCallback.onTrigger() PxScene.setSimulationEventCallback() PxShape.setFlag(), PxShape.setFlags()
		*/
		eTRIGGER_SHAPE					= (1<<2),

		/**
		\brief Enable debug renderer for this shape

		\see PxScene.getRenderBuffer() PxRenderBuffer PxVisualizationParameter
		*/
		eVISUALIZATION					= (1<<3)
	};
};

/**
\brief collection of set bits defined in PxShapeFlag.

\see PxShapeFlag
*/
typedef PxFlags<PxShapeFlag::Enum,PxU8> PxShapeFlags;
PX_FLAGS_OPERATORS(PxShapeFlag::Enum,PxU8)

/**
\brief Abstract class for collision shapes.

Shapes are shared, reference counted objects.

An instance can be created by calling the createShape() method of the PxRigidActor class, or
the createShape() method of the PxPhysics class.

<h3>Visualizations</h3>
\li PxVisualizationParameter::eCOLLISION_AABBS
\li PxVisualizationParameter::eCOLLISION_SHAPES
\li PxVisualizationParameter::eCOLLISION_AXES

\see PxPhysics.createShape() PxRigidActor.createShape() PxBoxGeometry PxSphereGeometry PxCapsuleGeometry PxPlaneGeometry PxConvexMeshGeometry
PxTriangleMeshGeometry PxHeightFieldGeometry
*/
class PxShape : public PxRefCounted
{
public:
	/**
	\brief Adjust the geometry of the shape.

	\note The type of the passed in geometry must match the geometry type of the shape.
	\note It is not allowed to change the geometry type of a shape.
	\note This function does not guarantee correct/continuous behavior when objects are resting on top of old or new geometry.

	\param[in] geometry New geometry of the shape.

	\see PxGeometry PxGeometryType getGeometryType()
	*/
	virtual		void	setGeometry(const PxGeometry& geometry) = 0;

	/**
	\brief Retrieve a reference to the shape's geometry.

	\warning The returned reference has the same lifetime as the PxShape it comes from.

	\return	Reference to internal PxGeometry object.

	\see PxGeometry PxGeometryType getGeometryType() setGeometry()
	*/
	virtual		const PxGeometry&	getGeometry() const = 0;

	/**
	\brief Retrieves the actor which this shape is associated with.

	\return The actor this shape is associated with, if it is an exclusive shape, else NULL

	\see PxRigidStatic, PxRigidDynamic, PxArticulationLink
	*/
	virtual		PxRigidActor*	getActor() const = 0;

/************************************************************************************************/

/** \name Pose Manipulation
*/
//\{

	/**
	\brief Sets the pose of the shape in actor space, i.e. relative to the actors to which they are attached.
	
	This transformation is identity by default.

	The local pose is an attribute of the shape, and so will apply to all actors to which the shape is attached.

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	<i>Note:</i> Does not automatically update the inertia properties of the owning actor (if applicable); use the
	PhysX extensions method #PxRigidBodyExt::updateMassAndInertia() to do this.

	<b>Default:</b> the identity transform

	\param[in] pose	The new transform from the actor frame to the shape frame. <b>Range:</b> rigid body transform

	\see getLocalPose() 
	*/
	virtual		void	setLocalPose(const PxTransform& pose)		= 0;

	/**
	\brief Retrieves the pose of the shape in actor space, i.e. relative to the actor they are owned by.

	This transformation is identity by default.

	\return Pose of shape relative to the actor's frame.

	\see setLocalPose() 
	*/
	virtual		PxTransform	getLocalPose()	const	= 0;

//\}
/************************************************************************************************/

/** \name Collision Filtering
*/
//\{

	/**
	\brief Sets the user definable collision filter data.
	
	<b>Sleeping:</b> Does wake up the actor if the filter data change causes a formerly suppressed
	collision pair to be enabled.

	<b>Default:</b> (0,0,0,0)

	\see getSimulationFilterData() 
	*/
	virtual		void	setSimulationFilterData(const PxFilterData& data)	= 0;

	/**
	\brief Retrieves the shape's collision filter data.

	\see setSimulationFilterData() 
	*/
	virtual		PxFilterData	getSimulationFilterData()	const	= 0;

	/**
	\brief Sets the user definable query filter data.

	<b>Default:</b> (0,0,0,0)

	\see getQueryFilterData() 
	*/
	virtual		void	setQueryFilterData(const PxFilterData& data)	= 0;

	/**
	\brief Retrieves the shape's Query filter data.

	\see setQueryFilterData() 
	*/
	virtual		PxFilterData	getQueryFilterData()	const	= 0;

//\}
/************************************************************************************************/

	/**
	\brief Assigns material(s) to the shape. Will remove existing materials from the shape.
	
	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] materials List of material pointers to assign to the shape. See #PxMaterial
	\param[in] materialCount The number of materials provided.

	\see PxPhysics.createMaterial() getMaterials() 
	*/
	virtual		void	setMaterials(PxMaterial*const* materials, PxU16 materialCount) = 0;

	/**
	\brief Assigns surface deformable material(s) to the shape. Will remove existing materials from the shape.

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] materials List of material pointers to assign to the shape. See #PxDeformableSurfaceMaterial
	\param[in] materialCount The number of materials provided.

	\see PxPhysics.createDeformableSurfaceMaterial() getDeformableSurfaceMaterials() 
	*/
	virtual		void	setDeformableSurfaceMaterials(PxDeformableSurfaceMaterial*const* materials, PxU16 materialCount) = 0;

	/**
	\brief Assigns deformable volume material(s) to the shape. Will remove existing materials from the shape.

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] materials List of material pointers to assign to the shape. See #PxDeformableVolumeMaterial
	\param[in] materialCount The number of materials provided.

	\see PxPhysics.createDeformableVolumeMaterial() getDeformableVolumeMaterials()
	*/
	virtual		void	setDeformableVolumeMaterials(PxDeformableVolumeMaterial* const* materials, PxU16 materialCount) = 0;

	/**
	\brief Returns the number of materials assigned to the shape.

	You can use #getMaterials() to retrieve the material pointers.

	\return Number of materials associated with this shape.

	\see PxMaterial getMaterials()
	*/
	virtual		PxU16	getNbMaterials()		const	= 0;

	/**
	\brief Retrieve all the material pointers associated with the shape.

	You can retrieve the number of material pointers by calling #getNbMaterials()

	Note: The returned data may contain invalid pointers if you release materials using #PxMaterial::release().

	\param[out] userBuffer The buffer to store the material pointers.
	\param[in] bufferSize Size of provided user buffer.
	\param[in] startIndex Index of first material pointer to be retrieved
	\return Number of material pointers written to the buffer.

	\see PxMaterial getNbMaterials() PxMaterial::release()
	*/
	virtual		PxU32	getMaterials(PxMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex=0) const = 0;

	/**
	\brief Retrieve all the surface deformable material pointers associated with the shape.

	You can retrieve the number of material pointers by calling #getNbMaterials()

	Note: The returned data may contain invalid pointers if you release materials using #PxMaterial::release().

	\param[out] userBuffer The buffer to store the material pointers.
	\param[in] bufferSize Size of provided user buffer.
	\param[in] startIndex Index of first material pointer to be retrieved
	\return Number of material pointers written to the buffer.

	\see PxDeformableSurfaceMaterial getNbMaterials() PxMaterial::release()
	*/
	virtual		PxU32	getDeformableSurfaceMaterials(PxDeformableSurfaceMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Retrieve all the deformable volume material pointers associated with the shape.

	You can retrieve the number of material pointers by calling #getNbMaterials()

	Note: The returned data may contain invalid pointers if you release materials using #PxMaterial::release().

	\param[out] userBuffer The buffer to store the material pointers.
	\param[in] bufferSize Size of provided user buffer.
	\param[in] startIndex Index of first material pointer to be retrieved
	\return Number of material pointers written to the buffer.

	\see PxDeformableVolumeMaterial getNbMaterials() PxMaterial::release()
	*/
	virtual		PxU32	getDeformableVolumeMaterials(PxDeformableVolumeMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Retrieve material from given triangle index.

	The input index is the internal triangle index as used inside the SDK. This is the index
	returned to users by various SDK functions such as raycasts.
	
	This function is only useful for triangle meshes or heightfields, which have per-triangle
	materials. For other shapes or SDF triangle meshes, the function returns the single material
	associated with the	shape, regardless of the index.

	\param[in] faceIndex The internal triangle index whose material you want to retrieve.
	\return Material from input triangle

	\note If faceIndex value of 0xFFFFffff is passed as an input for mesh and heightfield shapes, this function will issue a warning and return NULL.
	\note Scene queries set the value of PxQueryHit::faceIndex to 0xFFFFffff whenever it is undefined or does not apply.

	\see PxMaterial getNbMaterials() PxMaterial::release()
	*/
	virtual		PxBaseMaterial*		getMaterialFromInternalFaceIndex(PxU32 faceIndex) const = 0;

	/**
	\brief Sets the contact offset.

	Shapes whose distance is less than the sum of their contactOffset values will generate contacts. The contact offset must be positive and
	greater than the rest offset. Having a contactOffset greater than than the restOffset allows the collision detection system to
	predictively enforce the contact constraint even when the objects are slightly separated. This prevents jitter that would occur
	if the constraint were enforced only when shapes were within the rest distance.

	<b>Default:</b> 0.02f * PxTolerancesScale::length

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] contactOffset <b>Range:</b> [maximum(0,restOffset), PX_MAX_F32)

	\see getContactOffset PxTolerancesScale setRestOffset
	*/
	virtual		void	setContactOffset(PxReal contactOffset)	= 0;

	/**
	\brief Retrieves the contact offset. 

	\return The contact offset of the shape.

	\see setContactOffset()
	*/
	virtual		PxReal	getContactOffset() const	= 0;

	/**
	\brief Sets the rest offset. 

	Two shapes will come to rest at a distance equal to the sum of their restOffset values. If the restOffset is 0, they should converge to touching 
	exactly.  Having a restOffset greater than zero is useful to have objects slide smoothly, so that they do not get hung up on irregularities of 
	each others' surfaces.

	<b>Default:</b> 0.0f

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] restOffset	<b>Range:</b> (-PX_MAX_F32, contactOffset)

	\see getRestOffset setContactOffset
	*/
	virtual		void	setRestOffset(PxReal restOffset)	= 0;

	/**
	\brief Retrieves the rest offset. 

	\return The rest offset of the shape.

	\see setRestOffset()
	*/
	virtual		PxReal	getRestOffset() const	= 0;

	/**
	\brief Sets the density used to interact with fluids.

	To be physically accurate, the density of a rigid body should be computed as its mass divided by its volume. To
	simplify tuning the interaction of fluid and rigid bodies, the density for fluid can differ from the real density. This
	allows to create floating bodies, even if they are supposed to sink with their mass and volume.

	<b>Default:</b> 800.0f

	\param[in] densityForFluid	<b>Range:</b> (0, PX_MAX_F32)

	\see getDensityForFluid
	*/
	virtual		void	setDensityForFluid(PxReal densityForFluid)	= 0;

	/**
	\brief Retrieves the density used to interact with fluids.

	\return The density of the body when interacting with fluid.

	\see setDensityForFluid()
	*/
	virtual		PxReal	getDensityForFluid() const	= 0;

	/**
	\brief Sets torsional patch radius.
	
	This defines the radius of the contact patch used to apply torsional friction. If the radius is 0 (and minTorsionalPatchRadius
	is 0 too, see #setMinTorsionalPatchRadius), no torsional friction will be applied. If the radius is > 0, some torsional friction
	will be applied. This is proportional to the penetration depth so, if the shapes are separated or penetration is zero, no
	torsional friction will be applied. It is used to approximate rotational friction introduced by the compression of contacting surfaces.

	\note Will only be active, if the friction patch has a single anchor point only. This is for example the case, if a contact patch
	      has a single contact point.

	\note Only supported in combination with solver type PxSolverType::eTGS.

	<b>Default:</b> 0.0

	\param[in] radius	<b>Range:</b> [0, PX_MAX_F32)
	*/
	virtual		void	setTorsionalPatchRadius(PxReal radius) = 0;

	/**
	\brief Gets torsional patch radius.

	See #setTorsionalPatchRadius for more info.

	\return The torsional patch radius of the shape.
	*/
	virtual		PxReal	getTorsionalPatchRadius() const = 0;

	/**
	\brief Sets minimum torsional patch radius.

	This defines the minimum radius of the contact patch used to apply torsional friction. If the radius is 0, the amount of torsional friction
	that will be applied will be entirely dependent on the value of torsionalPatchRadius. 
	
	If the radius is > 0, some torsional friction will be applied regardless of the value of torsionalPatchRadius or the amount of penetration.

	\note Will only be active in certain cases, see #setTorsionalPatchRadius for details.

	<b>Default:</b> 0.0

	\param[in] radius	<b>Range:</b> [0, PX_MAX_F32)
	*/
	virtual		void	setMinTorsionalPatchRadius(PxReal radius) = 0;

	/**
	\brief Gets minimum torsional patch radius.

	See #setMinTorsionalPatchRadius for more info.

	\return The minimum torsional patch radius of the shape.
	*/
	virtual		PxReal	getMinTorsionalPatchRadius() const = 0;

	/**
	\brief Returns the GPU shape index.

	\note This function only returns valid results if GPU dynamics is enabled.

	\return The GPU index, or 0xFFFFFFFF if the shape is not attached to a PxActor that is inserted into a PxScene.

	\see PxDirectGPUAPI::evaluateSDFDistances().
	*/
	virtual PxShapeGPUIndex  getGPUIndex() const = 0;

/************************************************************************************************/

	/**
	\brief Sets shape flags

	<b>Sleeping:</b> Does <b>NOT</b> wake the associated actor up automatically.

	\param[in] flag The shape flag to enable/disable. See #PxShapeFlag.
	\param[in] value True to set the flag. False to clear the flag specified in flag.

	<b>Default:</b> PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSIMULATION_SHAPE | PxShapeFlag::eSCENE_QUERY_SHAPE

	\see PxShapeFlag getFlags()
	*/
	virtual		void	setFlag(PxShapeFlag::Enum flag, bool value) = 0;

	/**
	\brief Sets shape flags

	\see PxShapeFlag getFlags()
	*/
	virtual		void	setFlags(PxShapeFlags inFlags) = 0;

	/**
	\brief Retrieves shape flags.

	\return The values of the shape flags.

	\see PxShapeFlag setFlag()
	*/
	virtual		PxShapeFlags	getFlags() const = 0;

	/**
	\brief Returns true if the shape is exclusive to an actor.
	
	\see PxPhysics::createShape()
	*/
	virtual		bool	isExclusive() const	= 0;

	/**
	\brief Sets a name string for the object that can be retrieved with #getName().
	
	This is for debugging and is not used by the SDK.
	The string is not copied by the SDK, only the pointer is stored.

	<b>Default:</b> NULL
	
	\param[in] name The name string to set the objects name to.

	\see getName()
	*/
	virtual		void	setName(const char* name)		= 0;

	/**
	\brief retrieves the name string set with setName().
	\return The name associated with the shape.

	\see setName()
	*/
	virtual		const char*	getName()			const	= 0;


	virtual		const char*	getConcreteTypeName() const	PX_OVERRIDE	PX_FINAL	{ return "PxShape"; }

/************************************************************************************************/

				void*		userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.

protected:
	PX_INLINE				PxShape(PxBaseFlags baseFlags) : PxRefCounted(baseFlags) {}
	PX_INLINE				PxShape(PxType concreteType, PxBaseFlags baseFlags) : PxRefCounted(concreteType, baseFlags), userData(NULL) {}
	virtual					~PxShape() {}
	virtual		bool		isKindOf(const char* name) const PX_OVERRIDE { PX_IS_KIND_OF(name, "PxShape", PxRefCounted); }
};

#if !PX_DOXYGEN
} // namespace physx
#endif

#endif
