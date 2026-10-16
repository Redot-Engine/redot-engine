// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_PHYSICS_H
#define PX_PHYSICS_H

#include "PxPhysXConfig.h"
#include "PxDeletionListener.h"
#include "foundation/PxTransform.h"
#include "PxShape.h"
#include "PxAggregate.h"
#include "PxParticleSystem.h"
#include "PxDeformableSurface.h"
#include "PxDeformableAttachment.h"
#include "PxDeformableElementFilter.h"
#include "foundation/PxPreprocessor.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxScene;
class PxSceneDesc;
class PxTolerancesScale;
class PxPvd;
class PxOmniPvd;
class PxInsertionCallback;

class PxRigidActor;
class PxConstraintConnector;
struct PxConstraintShaderTable;

class PxGeometry;
class PxFoundation;

class PxPruningStructure;
class PxBVH;

class PxDeformableVolumeMesh;

class PxInputStream;

/**
\brief Abstract singleton factory class used for instancing objects in the Physics SDK.

In addition you can use PxPhysics to set global parameters which will effect all scenes and create
objects that can be shared across multiple scenes.

You can get an instance of this class by calling PxCreatePhysics().

\see PxCreatePhysics() PxScene
*/
class PxPhysics
{
public:

	/** \name Basics
	*/
	//\{

	virtual ~PxPhysics() {}

	/**
	\brief Destroys the instance it is called on.

	Use this release method to destroy an instance of this class. Be sure
	to not keep a reference to this object after calling release.
	Avoid release calls while a scene is simulating (in between simulate() and fetchResults() calls).

	Note that this must be called once for each prior call to PxCreatePhysics, as
	there is a reference counter. Also note that you mustn't destroy the PxFoundation instance (holding the allocator, error callback etc.)
	until after the reference count reaches 0 and the SDK is actually removed.

	Releasing an SDK will also release any objects created through it (scenes, triangle meshes, convex meshes, heightfields, shapes etc.),
	provided the user hasn't already done so.

	\note Releasing the PxPhysics instance is a prerequisite to releasing the PxFoundation instance.

	\see PxCreatePhysics() PxFoundation
	*/
	virtual	void release() = 0;

	/**
	\brief Retrieves the Foundation instance.
	\return A reference to the Foundation object.
	*/
	virtual PxFoundation& getFoundation() = 0;

	/**
	\brief Gets PxPhysics object insertion interface.

	The insertion interface is needed for PxCreateTriangleMesh, PxCooking::createTriangleMesh etc., this allows runtime mesh creation.

	\see PxCreateTriangleMesh PxCreateHeightField PxCreateTetrahedronMesh PxCreateBVH
	     PxCooking::createTriangleMesh PxCooking::createHeightfield PxCooking::createTetrahedronMesh PxCooking::createBVH
	*/
	virtual PxInsertionCallback& getPhysicsInsertionCallback() = 0;

	/**
	\brief Retrieves the PxOmniPvd instance if there is one registered with PxPhysics.
	\return A pointer to a PxOmniPvd object.
	*/
	virtual PxOmniPvd* getOmniPvd() = 0;

	/**
	\brief Returns the simulation tolerance parameters.
	\return The current simulation tolerance parameters.
	*/
	virtual const PxTolerancesScale& getTolerancesScale() const = 0;

	//\}
	/** \name Aggregates
	*/
	//\{

	/**
	\brief Creates an aggregate with the specified maximum size and filtering hint.

	The previous API used "bool enableSelfCollision" which should now silently evaluates
	to a PxAggregateType::eGENERIC aggregate with its self-collision bit.

	Use PxAggregateType::eSTATIC or PxAggregateType::eKINEMATIC for aggregates that will
	only contain static or kinematic actors. This provides faster filtering when used in
	combination with PxPairFilteringMode.

	\param	[in] maxActor		The maximum number of actors that may be placed in the aggregate.
	\param	[in] maxShape		The maximum number of shapes that may be placed in the aggregate.
	\param	[in] filterHint		The aggregate's filtering hint.
	\return The new aggregate.

	\see PxAggregate PxAggregateFilterHint PxAggregateType PxPairFilteringMode
	*/
	virtual	PxAggregate* createAggregate(PxU32 maxActor, PxU32 maxShape, PxAggregateFilterHint filterHint) = 0;

	/**
	\brief Return the number of aggregates that currently exist.

	\return Number of aggregates.
	*/
	virtual PxU32 getNbAggregates() const = 0;

	//\}
	/** \name Triangle Meshes
	*/
	//\{

	/**
	\brief Creates a triangle mesh object.

	This can then be instanced into #PxShape objects.

	\param	[in] stream	The triangle mesh stream.
	\return The new triangle mesh.

	\see PxTriangleMesh PxMeshPreprocessingFlag PxTriangleMesh.release() PxInputStream PxTriangleMeshFlag
	*/
	virtual PxTriangleMesh* createTriangleMesh(PxInputStream& stream) = 0;

	/**
	\brief Return the number of triangle meshes that currently exist.

	\return Number of triangle meshes.

	\see getTriangleMeshes()
	*/
	virtual PxU32 getNbTriangleMeshes() const = 0;

	/**
	\brief Writes the array of triangle mesh pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the triangle meshes in the array is not specified.

	\param	[out] userBuffer	The buffer to receive triangle mesh pointers.
	\param	[in]  bufferSize	The number of triangle mesh pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first mesh pointer to be retrieved.
	\return The number of triangle mesh pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbTriangleMeshes() PxTriangleMesh
	*/
	virtual	PxU32 getTriangleMeshes(PxTriangleMesh** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Tetrahedron Meshes
	*/
	//\{

	/**
	\brief Creates a tetrahedron mesh object.

	This can then be instanced into #PxShape objects.

	\param[in] stream The tetrahedron mesh stream.
	\return The new tetrahedron mesh.

	\see PxTetrahedronMesh PxMeshPreprocessingFlag PxTetrahedronMesh.release() PxInputStream PxTriangleMeshFlag
	*/
	virtual PxTetrahedronMesh* createTetrahedronMesh(PxInputStream& stream) = 0;

	/**
	\brief Return the number of tetrahedron meshes that currently exist.

	\return Number of tetrahedron meshes.

	\see getTetrahedronMeshes()
	*/
	virtual PxU32 getNbTetrahedronMeshes() const = 0;

	/**
	\brief Writes the array of tetrahedron mesh pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the tetrahedron meshes in the array is not specified.

	\param[out] userBuffer The buffer to receive tetrahedron mesh pointers.
	\param[in] bufferSize The number of tetrahedron mesh pointers which can be stored in the buffer.
	\param[in] startIndex Index of first mesh pointer to be retrieved.
	\return The number of tetrahedron mesh pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbTetrahedronMeshes() PxTetrahedronMesh
	*/
	virtual	PxU32 getTetrahedronMeshes(PxTetrahedronMesh** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Heightfields
	*/
	//\{

	/**
	\brief Creates a heightfield object from previously cooked stream.

	This can then be instanced into #PxShape objects.

	\param	[in] stream	The heightfield mesh stream.
	\return The new heightfield.

	\see PxHeightField PxHeightField.release() PxInputStream
	*/
	virtual PxHeightField* createHeightField(PxInputStream& stream) = 0;

	/**
	\brief Return the number of heightfields that currently exist.

	\return Number of heightfields.

	\see getHeightFields()
	*/
	virtual PxU32 getNbHeightFields() const = 0;

	/**
	\brief Writes the array of heightfield pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the heightfields in the array is not specified.

	\param	[out] userBuffer	The buffer to receive heightfield pointers.
	\param	[in]  bufferSize	The number of heightfield pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first heightfield pointer to be retrieved.
	\return The number of heightfield pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbHeightFields() PxHeightField
	*/
	virtual	PxU32 getHeightFields(PxHeightField** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Convex meshes
	*/
	//\{

	/**
	\brief Creates a convex mesh object.

	This can then be instanced into #PxShape objects.

	\param	[in] stream	The stream to load the convex mesh from.
	\return The new convex mesh.

	\see PxConvexMesh PxConvexMesh.release() PxInputStream createTriangleMesh() PxConvexMeshGeometry PxShape
	*/
	virtual PxConvexMesh* createConvexMesh(PxInputStream& stream) = 0;

	/**
	\brief Return the number of convex meshes that currently exist.

	\return Number of convex meshes.

	\see getConvexMeshes()
	*/
	virtual PxU32 getNbConvexMeshes() const = 0;

	/**
	\brief Writes the array of convex mesh pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the convex meshes in the array is not specified.

	\param	[out] userBuffer	The buffer to receive convex mesh pointers.
	\param	[in]  bufferSize	The number of convex mesh pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first convex mesh pointer to be retrieved.
	\return The number of convex mesh pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbConvexMeshes() PxConvexMesh
	*/
	virtual	PxU32 getConvexMeshes(PxConvexMesh** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Deformable volume meshes
	*/
	//\{

	/**
	\brief Creates a deformable volume mesh object.

	\param[in] stream The deformable volume mesh stream.
	\return The new deformable volume mesh.

	\see createTetrahedronMesh
	*/
	virtual PxDeformableVolumeMesh* createDeformableVolumeMesh(PxInputStream& stream) = 0;

	//\}
	/** \name BVHs
	*/
	//\{

	/**
	\brief Creates a bounding volume hierarchy.

	\param	[in] stream	The stream to load the BVH from.
	\return The new BVH.

	\see PxBVH PxInputStream
	*/
	virtual PxBVH* createBVH(PxInputStream& stream) = 0;

	/**
	\brief Return the number of bounding volume hierarchies that currently exist.

	\return Number of bounding volume hierarchies.

	\see PxBVH getBVHs()
	*/
	virtual PxU32 getNbBVHs() const = 0;

	/**
	\brief Writes the array of bounding volume hierarchy pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the BVHs in the array is not specified.

	\param	[out] userBuffer	The buffer to receive BVH pointers.
	\param	[in]  bufferSize	The number of BVH pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first BVH pointer to be retrieved.
	\return The number of BVH pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbBVHs() PxBVH
	*/
	virtual	PxU32 getBVHs(PxBVH** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Scenes
	*/
	//\{

	/**
	\brief Creates a scene.

	\note Every scene uses a Thread Local Storage slot. This imposes a platform specific limit on the
	number of scenes that can be created.

	\param	[in] sceneDesc	Scene descriptor. See #PxSceneDesc
	\return The new scene object.

	\see PxScene PxScene.release() PxSceneDesc
	*/
	virtual PxScene* createScene(const PxSceneDesc& sceneDesc) = 0;

	/**
	\brief Gets number of created scenes.

	\return The number of scenes created.

	\see getScenes()
	*/
	virtual PxU32 getNbScenes() const = 0;

	/**
	\brief Writes the array of scene pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the scene pointers in the array is not specified.

	\param	[out] userBuffer	The buffer to receive scene pointers.
	\param	[in]  bufferSize	The number of scene pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first scene pointer to be retrieved.
	\return The number of scene pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbScenes() PxScene
	*/
	virtual	PxU32 getScenes(PxScene** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Actors
	*/
	//\{

	/**
	\brief Creates a static rigid actor with the specified pose and all other fields initialized
	to their default values.

	\param	[in] pose	The initial pose of the actor. Must be a valid transform.

	\see PxRigidStatic
	*/
	virtual PxRigidStatic* createRigidStatic(const PxTransform& pose) = 0;

	/**
	\brief Creates a dynamic rigid actor with the specified pose and all other fields initialized
	to their default values.

	\param	[in] pose	The initial pose of the actor. Must be a valid transform.

	\see PxRigidDynamic
	*/
	virtual PxRigidDynamic* createRigidDynamic(const PxTransform& pose) = 0;

	/**
	\brief Creates a pruning structure from actors.

	\note Every provided actor needs at least one shape with the eSCENE_QUERY_SHAPE flag set.
	\note Both static and dynamic actors can be provided.
	\note It is not allowed to pass in actors which are already part of a scene.
	\note Articulation links cannot be provided.

	\param	[in] actors		Array of actors to add to the pruning structure. Must be non NULL.
	\param	[in] nbActors	Number of actors in the array. Must be >0.
	\return Pruning structure created from given actors, or NULL if any of the actors did not comply with the above requirements.
	\see PxActor PxPruningStructure
	*/
	virtual PxPruningStructure* createPruningStructure(PxRigidActor*const* actors, PxU32 nbActors) = 0;

	//\}
	/** \name Shapes
	*/
	//\{

	/**
	\brief Creates a shape which may be attached to multiple actors

	The shape will be created with a reference count of 1.

	\param	[in] geometry		The geometry for the shape
	\param	[in] material		The material for the shape
	\param	[in] isExclusive	Whether this shape is exclusive to a single actor or maybe be shared
	\param	[in] shapeFlags		The PxShapeFlags to be set
	\return The shape

	\note Shared shapes are not mutable when they are attached to an actor

	\see PxShape
	*/
	PX_FORCE_INLINE	PxShape* createShape(	const PxGeometry& geometry,
											const PxMaterial& material,
											bool isExclusive = false,
											PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE)
	{
		PxMaterial* materialPtr = const_cast<PxMaterial*>(&material);
		return createShape(geometry, &materialPtr, 1, isExclusive, shapeFlags);
	}

	/**
	\brief Creates a shape which may be attached to exactly one deformable volume actor

	The shape will be created with a reference count of 1.

	\param	[in] geometry		The geometry for the shape
	\param	[in] material		The material for the shape
	\param	[in] isExclusive	Whether this shape is exclusive to a single actor or maybe be shared
	\param	[in] shapeFlags		The PxShapeFlags to be set
	\return The shape

	\note Shared shapes are not mutable when they are attached to an actor

	\see PxShape
	*/
	PX_FORCE_INLINE	PxShape* createShape(	const PxGeometry& geometry,
											const PxDeformableVolumeMaterial& material,
											bool isExclusive = false,
											PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE)
	{
		PxDeformableVolumeMaterial* materialPtr = const_cast<PxDeformableVolumeMaterial*>(&material);
		return createShape(geometry, &materialPtr, 1, isExclusive, shapeFlags);
	}

	/**
	\brief Creates a shape which may be attached to exactly one deformable surface actor

	The shape will be created with a reference count of 1.

	\param	[in] geometry		The geometry for the shape
	\param	[in] material		The material for the shape
	\param	[in] isExclusive	Whether this shape is exclusive to a single actor or maybe be shared
	\param	[in] shapeFlags		The PxShapeFlags to be set
	\return The shape

	\note Shared shapes are not mutable when they are attached to an actor

	\see PxShape
	*/
	PX_FORCE_INLINE	PxShape* createShape(	const PxGeometry& geometry,
											const PxDeformableSurfaceMaterial& material,
											bool isExclusive = false,
											PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE)
	{
		PxDeformableSurfaceMaterial* materialPtr = const_cast<PxDeformableSurfaceMaterial*>(&material);
		return createShape(geometry, &materialPtr, 1, isExclusive, shapeFlags);
	}

	/**
	\brief Creates a shape which may be attached to multiple actors

	The shape will be created with a reference count of 1.

	\param	[in] geometry		The geometry for the shape
	\param	[in] materials		The materials for the shape
	\param	[in] materialCount	The number of materials
	\param	[in] isExclusive	Whether this shape is exclusive to a single actor or may be shared
	\param	[in] shapeFlags		The PxShapeFlags to be set
	\return The shape

	\note Shared shapes are not mutable when they are attached to an actor
	\note Shapes created from *SDF* triangle-mesh geometries do not support more than one material.

	\see PxShape
	*/
	virtual PxShape* createShape(	const PxGeometry& geometry,
									PxMaterial*const * materials,
									PxU16 materialCount,
									bool isExclusive = false,
									PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE) = 0;

	virtual PxShape* createShape(	const PxGeometry& geometry,
									PxDeformableSurfaceMaterial*const * materials,
									PxU16 materialCount,
									bool isExclusive = false,
									PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE) = 0;

	virtual PxShape* createShape(	const PxGeometry& geometry,
									PxDeformableVolumeMaterial*const * materials,
									PxU16 materialCount,
									bool isExclusive = false,
									PxShapeFlags shapeFlags = PxShapeFlag::eVISUALIZATION | PxShapeFlag::eSCENE_QUERY_SHAPE | PxShapeFlag::eSIMULATION_SHAPE) = 0;

	/**
	\brief Return the number of shapes that currently exist.

	\return Number of shapes.

	\see getShapes()
	*/
	virtual PxU32 getNbShapes() const = 0;

	/**
	\brief Writes the array of shape pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the shapes in the array is not specified.

	\param	[out] userBuffer	The buffer to receive shape pointers.
	\param	[in]  bufferSize	The number of shape pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first shape pointer to be retrieved
	\return The number of shape pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbShapes() PxShape
	*/
	virtual	PxU32 getShapes(PxShape** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	//\}
	/** \name Constraints and Articulations
	*/
	//\{

	/**
	\brief Creates a constraint shader.

	\note A constraint shader will get added automatically to the scene the two linked actors belong to. Either, but not both, of actor0 and actor1 may
	be NULL to denote attachment to the world.

	\param	[in] actor0		The first actor
	\param	[in] actor1		The second actor
	\param	[in] connector	The connector object, which the SDK uses to communicate with the infrastructure for the constraint
	\param	[in] shaders	The shader functions for the constraint
	\param	[in] dataSize	The size of the data block for the shader

	\return The new constraint shader.

	\see PxConstraint
	*/
	virtual PxConstraint* createConstraint(PxRigidActor* actor0, PxRigidActor* actor1, PxConstraintConnector& connector, const PxConstraintShaderTable& shaders, PxU32 dataSize) = 0;

	/**
	\brief Return the number of constraints that currently exist.

	\return Number of constraints.
	*/
	virtual PxU32 getNbConstraints() const = 0;

	/**
	\brief Writes the constraints that currently exist into a user buffer.

	\param[out] userBuffer The buffer to receive constraint pointers.
	\param[in] bufferSize The number of constraint pointers which can be stored in the buffer.
	\param[in] startIndex Index of first constraint pointer to be retrieved.
	\return Number of constraint pointers written to the buffer.

	\see getNbConstraints() PxConstraint
	*/
	virtual PxU32 getConstraints(PxConstraint** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Creates a reduced-coordinate articulation with all fields initialized to their default values.

	\return the new articulation

	\see PxArticulationReducedCoordinate
	*/
	virtual PxArticulationReducedCoordinate* createArticulationReducedCoordinate() = 0;

	/**
	\brief Return the number of articulations that currently exist.

	\return Number of articulations.
	*/
	virtual PxU32 getNbArticulations() const = 0;

	//\}
	/** \name Misc
	*/
	//\{

	/**
	\brief Creates an attachment between two actors, based on the provided PxDeformableAttachmentData. At least one of the actors must be a deformable.

	An attachment is a collection of one or more positional constraints between a point on one actor and a point on another actor.
	Attachments between two rigid objects are not permitted, use joints instead.

	\note The attachment is only active when both actors are added to the same scene or one of the actors is NULL.

	\param[in] data Attachment data.
	\return The PxDeformableAttachment created if successful, NULL otherwise.
	\see PxDeformableAttachmentData, PxDeformableAttachment
	*/
	virtual PxDeformableAttachment* createDeformableAttachment(const PxDeformableAttachmentData& data) = 0;

	/**
	\brief Creates an element-level collision filter between two actors, based on the provided PxDeformableElementFilterData. At least one of the actors must be a deformable.

	Element filters define how parts of deformable actors are excluded from collisions.
	They are usually added to avoid conflicting attachment and contact constraints.

	\note The element filter is only active when both actors are added to the same scene or one of the actors is NULL.

	\param[in] data Element filter data.
	\return The PxDeformableElementFilter created if successful, NULL otherwise.
	\see PxDeformableElementFilterData, PxDeformableElementFilter
	*/
	virtual PxDeformableElementFilter* createDeformableElementFilter(const PxDeformableElementFilterData& data) = 0;

	/**
	\brief Creates a deformable surface with all fields initialized to their default values.

	\param[in] cudaContextManager The PxCudaContextManager this instance is tied to.
	\return the new deformable surface

	\see PxDeformableSurface
	*/
	virtual PxDeformableSurface* createDeformableSurface(PxCudaContextManager& cudaContextManager) = 0;

	/**
	\brief Creates a FEM-based deformable volume with all fields initialized to their default values.

	\param[in] cudaContextManager The PxCudaContextManager this instance is tied to.
	\return the new deformable volume

	\see PxDeformableVolume
	*/
	virtual PxDeformableVolume* createDeformableVolume(PxCudaContextManager& cudaContextManager) = 0;

	/**
	\brief Creates a particle system with a position-based dynamics (PBD) solver.

	A PBD particle system can be used to simulate particle systems with fluid and granular particles. It also allows simulating cloth using
	mass-spring constraints and rigid bodies by shape matching the bodies with particles.

	In order to accelerate neighborhood finding for particle-particle interactions (e.g.: for fluid density constraints) a regular grid is used.
	This grid is built every time step but may provide inaccurate neighborhood information during the solver iterations. The neighborhood scale
	parameter can be used to configure the grid such that it provides a more conservative neighborhood at the cost of run-time performance. 
	The grid cell width is defined as 2*particleContactOffset*neighborhoodScale. \see PxParticleSystem::setParticleContactOffset.
	The neighborhood scale parameter should typically not be much larger than 1.

	The maxNeighborhood defines how many particles fit into the neighborhood, at the cost of memory.
	
	Both maxNeighborhood and neighborhoodScale should be set as low as possible for performance, but high enough to not cause any behavioral degredation.

	\param[in] cudaContextManager The PxCudaContextManager this instance is tied to.
	\param[in] maxNeighborhood The maximum number of particles considered in neighborhood-based particle interaction calculations.
	\param[in] neighborhoodScale Scale applied to the particle contact offset used to define the particle neighborhood size spatially. <b>Range:</b> (1.0, PX_MAX_F32)
	\return the new particle system

	\see PxPBDParticleSystem
	*/
	virtual PxPBDParticleSystem* createPBDParticleSystem(PxCudaContextManager& cudaContextManager, PxU32 maxNeighborhood = 96, PxReal neighborhoodScale = 1.01f) = 0;

	/**
	\brief Create particle buffer to simulate fluid/granular material.

	\param[in] maxParticles The maximum number of particles in this buffer.
	\param[in] cudaContextManager The PxCudaContextManager this buffer is tied to.
	\return PxParticleBuffer instance

	\see PxParticleBuffer
	*/
	virtual PxParticleBuffer* createParticleBuffer(PxU32 maxParticles, PxCudaContextManager* cudaContextManager) = 0;

	/**
	\brief Create a particle buffer for fluid dynamics with diffuse particles. Diffuse particles are used to simulate fluid effects
	such as foam, spray and bubbles.

	\param[in] maxParticles The maximum number of particles in this buffer.
	\param[in] maxDiffuseParticles The max number of diffuse particles in this buffer.
	\param[in] cudaContextManager The PxCudaContextManager this buffer is tied to.
	\return PxParticleAndDiffuseBuffer instance

	\see PxParticleAndDiffuseBuffer, PxDiffuseParticleParams
	*/
	virtual PxParticleAndDiffuseBuffer* createParticleAndDiffuseBuffer(PxU32 maxParticles, PxU32 maxDiffuseParticles, PxCudaContextManager* cudaContextManager) = 0;

	//\}
	/** \name Materials
	*/
	//\{

	/**
	\brief Creates a new rigid body material with certain default properties.

	\return The new rigid body material.

	\param	[in] staticFriction		The coefficient of static friction
	\param	[in] dynamicFriction	The coefficient of dynamic friction
	\param	[in] restitution		The coefficient of restitution (if in range [0,1]) or the spring stiffness for compliant contact (if in range (-PX_MAX_REAL, 0))

	\see PxMaterial
	*/
	virtual PxMaterial* createMaterial(PxReal staticFriction, PxReal dynamicFriction, PxReal restitution) = 0;

	/**
	\brief Return the number of rigid body materials that currently exist.

	\return Number of rigid body materials.

	\see getMaterials()
	*/
	virtual PxU32 getNbMaterials() const = 0;

	/**
	\brief Writes the array of rigid body material pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the materials in the array is not specified.

	\param	[out] userBuffer	The buffer to receive material pointers.
	\param	[in]  bufferSize	The number of material pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first material pointer to be retrieved.
	\return The number of material pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbMaterials() PxMaterial
	*/
	virtual PxU32 getMaterials(PxMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Creates a new surface deformable material with certain default properties.

	\return The new surface deformable material.

	\param	[in] youngs					The young's modulus
	\param	[in] poissons				The poissons's ratio
	\param	[in] dynamicFriction		The dynamic friction coefficient
	\param	[in] thickness				The thickness of the surface
	\param	[in] bendingStiffness		The bending stiffness of the surface
	\param	[in] elasticityDamping		The damping of the surface with respect to stretch and shear
	\param	[in] bendingDamping			The damping of the surface with respect to bending

	\see PxDeformableSurfaceMaterial
	*/
	virtual PxDeformableSurfaceMaterial* createDeformableSurfaceMaterial(PxReal youngs, PxReal poissons, PxReal dynamicFriction, 
		PxReal thickness = 0.001f, PxReal bendingStiffness = 0.0f, PxReal elasticityDamping = 0.0f, PxReal bendingDamping = 0.0f) = 0;

	/**
	\brief Return the number of deformable surface materials that currently exist.

	\return Number of deformable surface materials.

	\see getDeformableSurfaceMaterials()
	*/
	virtual PxU32 getNbDeformableSurfaceMaterials() const = 0;

	/**
	\brief Writes the array of deformable surface material pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the materials in the array is not specified.

	\param	[out] userBuffer	The buffer to receive material pointers.
	\param	[in]  bufferSize	The number of material pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first material pointer to be retrieved.
	\return The number of material pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbDeformableSurfaceMaterials() PxDeformableSurfaceMaterial
	*/
	virtual PxU32 getDeformableSurfaceMaterials(PxDeformableSurfaceMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Creates a new deformable volume material with certain default properties.

	\return The new deformable volume material.

	\param	[in] youngs					The young's modulus
	\param	[in] poissons				The poissons's ratio
	\param	[in] dynamicFriction		The dynamic friction coefficient
	\param	[in] elasticityDamping		The elasticity damping

	\see PxDeformableVolumeMaterial
	*/
	virtual PxDeformableVolumeMaterial* createDeformableVolumeMaterial(PxReal youngs, PxReal poissons, PxReal dynamicFriction, PxReal elasticityDamping = 0.0f) = 0;

	/**
	\brief Return the number of deformable volume materials that currently exist.

	\return Number of materials.

	\see getDeformableVolumeMaterials()
	*/
	virtual PxU32 getNbDeformableVolumeMaterials() const = 0;

	/**
	\brief Writes the array of deformable volume material pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the materials in the array is not specified.

	\param	[out] userBuffer	The buffer to receive material pointers.
	\param	[in]  bufferSize	The number of material pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first material pointer to be retrieved.
	\return The number of material pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbDeformableVolumeMaterials() PxDeformableVolumeMaterial
	*/
	virtual PxU32 getDeformableVolumeMaterials(PxDeformableVolumeMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;

	/**
	\brief Creates a new PBD material with certain default properties.

	\param	[in]  friction				The friction parameter
	\param	[in]  damping				The velocity damping parameter
	\param	[in]  adhesion				The adhesion parameter
	\param	[in]  viscosity				The viscosity parameter
	\param	[in]  vorticityConfinement	The vorticity confinement coefficient
	\param	[in]  surfaceTension		The surface tension coefficient
	\param	[in]  cohesion				The cohesion parameter
	\param	[in]  lift					The lift parameter
	\param	[in]  drag					The drag parameter
	\param	[in]  cflCoefficient		The Courant-Friedrichs-Lewy(cfl) coefficient
	\param	[in]  gravityScale			The gravity scale
	\return The new PBD material.

	\see PxPBDMaterial
	*/
	virtual PxPBDMaterial* createPBDMaterial(PxReal friction, PxReal damping, PxReal adhesion, PxReal viscosity, PxReal vorticityConfinement, PxReal surfaceTension, PxReal cohesion, PxReal lift, PxReal drag, PxReal cflCoefficient = 1.f, PxReal gravityScale = 1.f) = 0;

	/**
	\brief Return the number of PBD materials that currently exist.

	\return Number of PBD materials.

	\see getPBDMaterials()
	*/
	virtual PxU32 getNbPBDMaterials() const = 0;

	/**
	\brief Writes the array of PBD material pointers to a user buffer.

	Returns the number of pointers written.

	The ordering of the materials in the array is not specified.

	\param	[out] userBuffer	The buffer to receive material pointers.
	\param	[in]  bufferSize	The number of material pointers which can be stored in the buffer.
	\param	[in]  startIndex	Index of first material pointer to be retrieved.
	\return The number of material pointers written to userBuffer, this should be less or equal to bufferSize.

	\see getNbPBDMaterials() PxPBDMaterial
	*/
	virtual PxU32 getPBDMaterials(PxPBDMaterial** userBuffer, PxU32 bufferSize, PxU32 startIndex = 0) const = 0;
	
	//\}
	/** \name Deletion Listeners
	*/
	//\{

	/**
	\brief Register a deletion listener. Listeners will be called whenever an object is deleted.

	It is illegal to register or unregister a deletion listener while deletions are being processed.

	\note By default a registered listener will receive events from all objects. Set the restrictedObjectSet parameter to true on registration and use #registerDeletionListenerObjects to restrict the received events to specific objects.

	\note The deletion events are only supported on core PhysX objects. In general, objects in extension modules do not provide this functionality, however, in the case of PxJoint objects, the underlying PxConstraint will send the events.

	\param	[in] observer				Observer object to send notifications to.
	\param	[in] deletionEvents			The deletion event types to get notified of.
	\param	[in] restrictedObjectSet	If false, the deletion listener will get events from all objects, else the objects to receive events from have to be specified explicitly through #registerDeletionListenerObjects.

	\see PxDeletionListener unregisterDeletionListener
	*/
	virtual void registerDeletionListener(PxDeletionListener& observer, const PxDeletionEventFlags& deletionEvents, bool restrictedObjectSet = false) = 0;

	/**
	\brief Unregister a deletion listener.

	It is illegal to register or unregister a deletion listener while deletions are being processed.

	\param	[in] observer	Observer object to stop sending notifications to.

	\see PxDeletionListener registerDeletionListener
	*/
	virtual void unregisterDeletionListener(PxDeletionListener& observer) = 0;

	/**
	\brief Register specific objects for deletion events.

	This method allows for a deletion listener to limit deletion events to specific objects only.

	\note It is illegal to register or unregister objects while deletions are being processed.

	\note The deletion listener has to be registered through #registerDeletionListener() and configured to support restricted object sets prior to this method being used.

	\param	[in] observer			Observer object to send notifications to.
	\param	[in] observables		List of objects for which to receive deletion events. Only PhysX core objects are supported. In the case of PxJoint objects, the underlying PxConstraint can be used to get the events.
	\param	[in] observableCount	Size of the observables list.

	\see PxDeletionListener unregisterDeletionListenerObjects
	*/
	virtual void registerDeletionListenerObjects(PxDeletionListener& observer, const PxBase* const* observables, PxU32 observableCount) = 0;

	/**
	\brief Unregister specific objects for deletion events.

	This method allows to clear previously registered objects for a deletion listener (see #registerDeletionListenerObjects()).

	\note It is illegal to register or unregister objects while deletions are being processed.

	\note The deletion listener has to be registered through #registerDeletionListener() and configured to support restricted object sets prior to this method being used.

	\param	[in] observer			Observer object to stop sending notifications to.
	\param	[in] observables		List of objects for which to not receive deletion events anymore.
	\param	[in] observableCount	Size of the observables list.

	\see PxDeletionListener registerDeletionListenerObjects
	*/
	virtual void unregisterDeletionListenerObjects(PxDeletionListener& observer, const PxBase* const* observables, PxU32 observableCount) = 0;

	//\}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

/**
\brief Creates an instance of the physics SDK.

Creates an instance of this class. May not be a class member to avoid name mangling.
Pass the constant #PX_PHYSICS_VERSION as the argument.
There may be only one instance of this class per process. Calling this method after an instance
has been created already will result in an error message and NULL will be returned.

\param version Version number we are expecting (should be #PX_PHYSICS_VERSION)
\param foundation Foundation instance (see PxFoundation)
\param scale values used to determine default tolerances for objects at creation time
\param trackOutstandingAllocations true if you want to track memory allocations
			so a debugger connection partway through your physics simulation will get
			an accurate map of everything that has been allocated so far. This could have a memory
			and performance impact on your simulation hence it defaults to off.
\param pvd When pvd points to a valid PxPvd instance (PhysX Visual Debugger), a connection to the specified PxPvd instance is created.
			If pvd is NULL no connection will be attempted.
\param omniPvd When omniPvd points to a valid PxOmniPvd instance PhysX will sample its internal structures to the defined OmniPvd output streams
			set in the PxOmniPvd object.
\return PxPhysics instance on success, NULL if operation failed

\see PxPhysics
*/
PX_C_EXPORT PX_PHYSX_CORE_API  physx::PxPhysics* PxCreatePhysics(physx::PxU32 version,
																physx::PxFoundation& foundation,
																const physx::PxTolerancesScale& scale,
																bool trackOutstandingAllocations = false,
																physx::PxPvd* pvd = NULL,
																physx::PxOmniPvd* omniPvd = NULL);

/**
\brief Retrieves the Physics SDK after it has been created.

Before using this function the user must call #PxCreatePhysics().

\note The behavior of this method is undefined if the Physics SDK instance has not been created already.
*/
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wreturn-type-c-linkage"
#endif

PX_C_EXPORT PX_PHYSX_CORE_API physx::PxPhysics& PX_CALL_CONV PxGetPhysics();

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif
