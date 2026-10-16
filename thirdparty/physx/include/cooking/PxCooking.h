// Copyright (c) 2001-2004 NovodeX AG. All rights reserved.
// Copyright (c) 2004-2008 AGEIA Technologies, Inc. All rights reserved.
// SPDX-FileCopyrightText: Copyright (c) 2008-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
// SPDX-License-Identifier: Apache-2.0

#ifndef PX_COOKING_H
#define PX_COOKING_H
#include "common/PxPhysXCommonConfig.h"
#include "common/PxTolerancesScale.h"
#include "cooking/Pxc.h"

#include "cooking/PxConvexMeshDesc.h"
#include "cooking/PxTriangleMeshDesc.h"
#include "cooking/PxTetrahedronMeshDesc.h"
#include "cooking/PxMidphaseDesc.h"
#include "cooking/PxBVHDesc.h"
#include "geometry/PxTriangleMesh.h"
#include "geometry/PxTetrahedronMesh.h"
#include "geometry/PxBVH.h"

#if !PX_DOXYGEN
namespace physx
{
#endif

class PxInsertionCallback;
class PxFoundation;
class PxAllocatorCallback;
class PxHeightFieldDesc;

/**
\brief Result from convex cooking.
*/
struct PxConvexMeshCookingResult
{
	enum Enum
	{
		/**
		\brief Convex mesh cooking succeeded.
		*/
		eSUCCESS,

		/**
		\brief Convex mesh cooking failed, algorithm couldn't find 4 initial vertices without a small triangle.

		\see PxCookingParams::areaTestEpsilon PxConvexFlag::eCHECK_ZERO_AREA_TRIANGLES
		*/
		eZERO_AREA_TEST_FAILED,

		/**
		\brief Convex mesh cooking succeeded, but the algorithm has reached the 255 polygons limit.
			The produced hull does not contain all input vertices. Try to simplify the input vertices
			or try to use the eINFLATE_CONVEX or the eQUANTIZE_INPUT flags.

		\see PxConvexFlag::eINFLATE_CONVEX PxConvexFlag::eQUANTIZE_INPUT
		*/
		ePOLYGONS_LIMIT_REACHED,

		/**
		\brief Something unrecoverable happened. Check the error stream to find out what.
		*/
		eFAILURE,

		/**
		\brief Convex mesh cooking succeeded, but the algorithm could not make the mesh GPU compatible because the
			in-sphere radius is more than 100x smaller than the largest extent. Collision detection for any pair involving
			this convex mesh will fall back to CPU.
		*/
		eNON_GPU_COMPATIBLE
	};
};

/** \brief Enumeration for convex mesh cooking algorithms. */
struct PxConvexMeshCookingType
{
	enum Enum
	{
		/**
		\brief The Quickhull algorithm constructs the hull from the given input points. The resulting hull
		will only contain a subset of the input points. 
		
		*/
		eQUICKHULL
	};
};

/**
\brief Result from triangle mesh cooking
*/
struct PxTriangleMeshCookingResult
{
	enum Enum
	{
		/**
		\brief Everything is A-OK.
		*/
		eSUCCESS	= 0,

		/**
		\brief A triangle is too large for well-conditioned results. Tessellate the mesh for better behavior, see the user guide section on cooking for more details.
		*/
		eLARGE_TRIANGLE,

		/**
		\brief The mesh cleaning operation removed all triangles, resulting in an empty mesh.
		*/
		eEMPTY_MESH,

		/**
		\brief Something unrecoverable happened. Check the error stream to find out what.
		*/
		eFAILURE
	};
};

/**
\brief Enum for the set of mesh pre-processing parameters.
*/
struct PxMeshPreprocessingFlag
{
	enum Enum
	{
		/**
		\brief When set, mesh welding is performed. See PxCookingParams::meshWeldTolerance. Mesh cleaning must be enabled.
		*/
		eWELD_VERTICES	=	1 << 0, 

		/**
		\brief When set, mesh cleaning is disabled. This makes cooking faster.

		When mesh cleaning is disabled, mesh welding is also disabled.

		It is recommended to use only meshes that passed during validateTriangleMesh.
		*/
		eDISABLE_CLEAN_MESH	=	1 << 1, 

		/**
		\brief When set, active edges are not computed and just enabled for all edges. This makes cooking faster but contact generation slower.
		*/
		eDISABLE_ACTIVE_EDGES_PRECOMPUTE	=	1 << 2,

		/**
		\brief When set, 32-bit indices will always be created regardless of triangle count.

		\note By default mesh will be created with 16-bit indices for triangle count <= 0xFFFF and 32-bit otherwise.
		*/
		eFORCE_32BIT_INDICES	=	1 << 3,

		/**
		\brief When set, a list of triangles will be created for each associated vertex in the mesh.
		*/
		eENABLE_VERT_MAPPING	=   1 << 4,

		/**
		\brief When set, inertia data is calculated for the mesh, assuming unit density.
		*/
		eENABLE_INERTIA	= 1 << 5
	};
};

typedef PxFlags<PxMeshPreprocessingFlag::Enum,PxU32> PxMeshPreprocessingFlags;

/**
\brief Structure describing parameters affecting mesh cooking.

\see PxSetCookingParams() PxGetCookingParams()
*/
struct PxCookingParams
{
	/**
	\brief Zero-size area epsilon used in convex hull computation.

	If the area of a triangle of the hull is below this value, the triangle will be rejected. This test
	is done only if PxConvexFlag::eCHECK_ZERO_AREA_TRIANGLES is used.

	\see PxConvexFlag::eCHECK_ZERO_AREA_TRIANGLES

	<b>Default value:</b> 0.06f*PxTolerancesScale.length*PxTolerancesScale.length

	<b>Range:</b> (0.0f, PX_MAX_F32)
	*/
	float		areaTestEpsilon;

	/**
	\brief Plane tolerance used in convex hull computation.

	The value is used during hull construction. When a new point is about to be added to the hull it
	gets dropped when the point is closer to the hull than the planeTolerance. The planeTolerance
	is increased according to the hull size. 

	If 0.0f is set all points are accepted when the convex hull is created. This may lead to edge cases
	where the new points may be merged into an existing polygon and the polygons plane equation might 
	slightly change therefore. This might lead to failures during polygon merging phase in the hull computation.

	It is recommended to use the default value, however if it is required that all points needs to be
	accepted or huge thin convexes are created, it might be required to lower the default value.

	\note The plane tolerance is used only within PxConvexMeshCookingType::eQUICKHULL algorithm.

	<b>Default value:</b> 0.0007f

	<b>Range:</b> <0.0f, PX_MAX_F32)
	*/
	float		planeTolerance;

	/**
	\brief Convex hull creation algorithm.

	<b>Default value:</b> PxConvexMeshCookingType::eQUICKHULL

	\see PxConvexMeshCookingType
	*/
	PxConvexMeshCookingType::Enum convexMeshCookingType;

	/**
	\brief When true, the face remap table is not created.  This saves a significant amount of memory, but the SDK will
		not be able to provide the remap information for internal mesh triangles returned by collisions, 
		sweeps or raycasts hits.

	<b>Default value:</b> false
	*/
	bool		suppressTriangleMeshRemapTable;

	/**
	\brief When true, the triangle adjacency information is created. You can get the adjacency triangles
	for a given triangle from getTriangle.

	<b>Default value:</b> false
	*/
	bool		buildTriangleAdjacencies;

	/**
	\brief When true, additional information required for GPU-accelerated rigid body simulation is created. This can increase memory usage and cooking times for convex meshes and triangle meshes.

	<b>Default value:</b> false
	*/
	bool		buildGPUData;

	/**
	\brief Tolerance scale is used to check if cooked triangles are not too huge. This check will help with simulation stability.

	\note The PxTolerancesScale values have to match the values used when creating a PxPhysics or PxScene instance.

	\see PxTolerancesScale
	*/
	PxTolerancesScale scale;

	/**
	\brief Mesh pre-processing parameters. Used to control options like whether the mesh cooking performs vertex welding before cooking.

	<b>Default value:</b> 0
	*/
	PxMeshPreprocessingFlags	meshPreprocessParams;

	/**
	\brief Mesh weld tolerance. If mesh welding is enabled, this controls the distance at which vertices are welded.
	If mesh welding is not enabled, this value defines the acceptance distance for mesh validation. Provided no two vertices are within this distance, the mesh is considered to be
	clean. If not, a warning will be emitted. Having a clean, welded mesh is required to achieve the best possible performance.

	The default vertex welding uses a snap-to-grid approach. This approach effectively truncates each vertex to integer values using meshWeldTolerance.
	Once these snapped vertices are produced, all vertices that snap to a given vertex on the grid are remapped to reference a single vertex. Following this,
	all triangles' indices are remapped to reference this subset of clean vertices. It should be noted that	the vertices that we do not alter the
	position of the vertices; the snap-to-grid is only performed to identify nearby vertices.

	The mesh validation approach also uses the same snap-to-grid approach to identify nearby vertices. If more than one vertex snaps to a given grid coordinate,
	we ensure that the distance between the vertices is at least meshWeldTolerance. If this is not the case, a warning is emitted.

	<b>Default value:</b> 0.0
	*/
	PxReal		meshWeldTolerance;

	/**
	\brief "Zero-area" epsilon used in mesh cleaning.

	This is similar to areaTestEpsilon, but for the mesh cleaning operation. 

	If the area of a triangle is below this value, the triangle will be removed. This is only done when mesh cleaning is enabled,
	i.e. when PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH is not set.

	Default value is 0.0f to be consistent with previous PhysX versions, which only removed triangles whose area
	was exactly zero.

	\see PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH

	<b>Default value:</b> 0.0f

	<b>Range:</b> (0.0f, PX_MAX_F32)
	*/
	PxReal		meshAreaMinLimit;

	/**
	\brief Maximum edge length.

	If an edge of a triangle is above this value, a warning is sent to the error stream. This is only a check,
	corresponding triangles are not removed.
	
	This is only done when mesh cleaning is enabled, i.e. when PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH is not set.

	Default value is 500.0f to be consistent with previous PhysX versions. This value is internally multiplied by
	PxTolerancesScale::length before being used. Use 0.0f to disable the checks.

	\see PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH

	<b>Default value:</b> 500.0f

	<b>Range:</b> (0.0f, PX_MAX_F32)
	*/
	PxReal		meshEdgeLengthMaxLimit;

	/**
	\brief Controls the desired midphase desc structure for triangle meshes.

	\see PxBVH33MidphaseDesc, PxBVH34MidphaseDesc, PxMidphaseDesc

	<b>Default value:</b> PxMeshMidPhase::eBVH34
	*/
	PxMidphaseDesc midphaseDesc;

	/**
	\brief Vertex limit beyond which additional acceleration structures are computed for each convex mesh. Increase that limit to reduce memory usage.
	Computing the extra structures all the time does not guarantee optimal performance. There is a per-platform break-even point below which the
	extra structures actually hurt performance.

	<b>Default value:</b> 32
	*/
	PxU32	gaussMapLimit;

	/**
	\brief Maximum mass ratio allowed on vertices touched by a single tet. If a any tetrahedron exceeds the mass ratio, the masses will get smoothed locally
	until the maximum mass ratio is matched. Value should not be below 1. Smoothing might not fully converge for values <1.5. The smaller the maximum
	allowed ratio, the better the stability during simulation.

	<b>Default value:</b> FLT_MAX
	*/
	PxReal maxWeightRatioInTet;

	PxCookingParams(const PxTolerancesScale& sc):
		areaTestEpsilon					(0.06f*sc.length*sc.length),
		planeTolerance					(0.0007f),
		convexMeshCookingType			(PxConvexMeshCookingType::eQUICKHULL),
		suppressTriangleMeshRemapTable	(false),
		buildTriangleAdjacencies		(false),
		buildGPUData					(false),
		scale							(sc),
		meshPreprocessParams			(0),
		meshWeldTolerance				(0.0f),
		meshAreaMinLimit				(0.0f),
		meshEdgeLengthMaxLimit			(500.0f),
		gaussMapLimit					(32),
		maxWeightRatioInTet             (FLT_MAX)
	{
	}
};

#if !PX_DOXYGEN
} // namespace physx
#endif

// Immediate cooking

/**
\brief Gets standalone object insertion interface. 

This interface allows the creation of standalone objects that can exist without a PxPhysics or PxScene object.

\see PxCreateTriangleMesh PxCreateHeightfield PxCreateTetrahedronMesh PxCreateBVH
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxInsertionCallback* PxGetStandaloneInsertionCallback();

// ==== BVH ====

/**
\brief Cooks a bounding volume hierarchy. The results are written to the stream.

PxCookBVH() allows a BVH description to be cooked into a binary stream
suitable for loading and performing BVH detection at runtime.

\param[in] desc		The BVH descriptor.
\param[in] stream	User stream to output the cooked data.
\return true on success.

\see PxBVH PxRigidActorExt::getRigidActorShapeLocalBoundsList
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookBVH(const physx::PxBVHDesc& desc, physx::PxOutputStream& stream);

/**
\brief Cooks and creates a bounding volume hierarchy without going through a stream.

\note This method does the same as PxCookBVH, but the produced BVH is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\note PxInsertionCallback can be obtained through PxPhysics::getPhysicsInsertionCallback()
or PxGetStandaloneInsertionCallback().

\param[in] desc					The BVH descriptor.
\param[in] insertionCallback	The insertion interface.
\return PxBVH pointer on success

\see PxCookBVH() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxBVH* PxCreateBVH(const physx::PxBVHDesc& desc, physx::PxInsertionCallback& insertionCallback);

/**
\brief Cooks and creates a bounding volume hierarchy without going through a stream. Convenience function for standalone objects.

\note This method does the same as PxCookBVH, but the produced BVH is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\param[in] desc					The BVH descriptor.
\return PxBVH pointer on success

\see PxCookBVH() PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxBVH* PxCreateBVH(const physx::PxBVHDesc& desc)
{
	return PxCreateBVH(desc, *PxGetStandaloneInsertionCallback());
}

// ==== Heightfield ====

/**
\brief Cooks a heightfield. The results are written to the stream.

To create a heightfield object there is an option to precompute some of calculations done while loading the heightfield data.

PxCookHeightField() allows a heightfield description to be cooked into a binary stream
suitable for loading and performing collision detection at runtime.

\param[in] desc		The heightfield descriptor to read the HF from.
\param[in] stream	User stream to output the cooked data.
\return true on success

\see PxPhysics.createHeightField()
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookHeightField(const physx::PxHeightFieldDesc& desc, physx::PxOutputStream& stream);

/**
\brief Cooks and creates a heightfield mesh and inserts it into PxPhysics.

\param[in] desc					The heightfield descriptor to read the HF from.
\param[in] insertionCallback	The insertion interface from PxPhysics.
\return PxHeightField pointer on success

\see PxCookHeightField() PxPhysics.createHeightField() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxHeightField* PxCreateHeightField(const physx::PxHeightFieldDesc& desc, physx::PxInsertionCallback& insertionCallback);

/**
\brief Cooks and creates a heightfield mesh and inserts it into PxPhysics. Convenience function for standalone objects.

\param[in] desc					The heightfield descriptor to read the HF from.	
\return PxHeightField pointer on success

\see PxCookHeightField() PxPhysics.createHeightField() PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxHeightField* PxCreateHeightField(const physx::PxHeightFieldDesc& desc)
{
	return PxCreateHeightField(desc, *PxGetStandaloneInsertionCallback());
}

// ==== Convex meshes ====

/**
\brief Cooks a convex mesh. The results are written to the stream.

To create a triangle mesh object it is necessary to first 'cook' the mesh data into
a form which allows the SDK to perform efficient collision detection.

PxCookConvexMesh() allows a mesh description to be cooked into a binary stream
suitable for loading and performing collision detection at runtime.

\note The number of vertices and the number of convex polygons in a cooked convex mesh is limited to 255.
\note If those limits are exceeded in either the user-provided data or the final cooked mesh, an error is reported.

\param[in] params		The cooking parameters
\param[in] desc			The convex mesh descriptor to read the mesh from.
\param[in] stream		User stream to output the cooked data.
\param[out] condition	Result from convex mesh cooking.
\return true on success.

\see PxCookTriangleMesh() PxConvexMeshCookingResult::Enum
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookConvexMesh(const physx::PxCookingParams& params, const physx::PxConvexMeshDesc& desc, physx::PxOutputStream& stream, physx::PxConvexMeshCookingResult::Enum* condition=NULL);

/**
\brief Cooks and creates a convex mesh without going through a stream.

\note This method does the same as PxCookConvexMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\note PxInsertionCallback can be obtained through PxPhysics::getPhysicsInsertionCallback()
or PxGetStandaloneInsertionCallback().

\param[in] params				The cooking parameters
\param[in] desc					The convex mesh descriptor to read the mesh from.
\param[in] insertionCallback	The insertion interface from PxPhysics.
\param[out] condition			Result from convex mesh cooking.
\return PxConvexMesh pointer on success	

\see PxCookConvexMesh() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxConvexMesh* PxCreateConvexMesh(const physx::PxCookingParams& params, const physx::PxConvexMeshDesc& desc, physx::PxInsertionCallback& insertionCallback, physx::PxConvexMeshCookingResult::Enum* condition=NULL);

/**
\brief Cooks and creates a convex mesh without going through a stream. Convenience function for standalone objects.
	
\note This method does the same as cookConvexMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.
	
\param[in] params		The cooking parameters
\param[in] desc			The convex mesh descriptor to read the mesh from.
\return PxConvexMesh pointer on success

\see PxCookConvexMesh()  PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxConvexMesh* PxCreateConvexMesh(const physx::PxCookingParams& params, const physx::PxConvexMeshDesc& desc)
{
	return PxCreateConvexMesh(params, desc, *PxGetStandaloneInsertionCallback());
}

/**
\brief Verifies if the convex mesh is valid. Prints an error message for each inconsistency found.

The convex mesh descriptor must contain an already created convex mesh - the vertices, indices and polygons must be provided.	

\note This function should be used if PxConvexFlag::eDISABLE_MESH_VALIDATION is planned to be used in release builds.

\param[in] params	The cooking parameters
\param[in] desc		The convex mesh descriptor to read the mesh from.

\return true if all the validity conditions hold, false otherwise.

\see PxCookConvexMesh()
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxValidateConvexMesh(const physx::PxCookingParams& params, const physx::PxConvexMeshDesc& desc);

/**
\brief Compute hull polygons from given vertices and triangles. Polygons are needed for PxConvexMeshDesc rather than triangles.

Please note that the resulting polygons may have different number of vertices. Some vertices may be removed. 
The output vertices, indices and polygons must be used to construct a hull.

The provided PxAllocatorCallback does allocate the out arrays. It is the user responsibility to deallocated those arrays.

\param[in] params			The cooking parameters
\param[in] mesh				Simple triangle mesh containing vertices and triangles used to compute polygons.
\param[in] inCallback		Memory allocator for out array allocations.
\param[out] nbVerts			Number of vertices used by polygons.
\param[out] vertices		Vertices array used by polygons.
\param[out] nbIndices		Number of indices used by polygons.
\param[out] indices			Indices array used by polygons.
\param[out] nbPolygons		Number of created polygons.
\param[out] hullPolygons	Polygons array.
\return true on success

\see PxCookConvexMesh() PxConvexFlags PxConvexMeshDesc PxSimpleTriangleMesh
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxComputeHullPolygons(const physx::PxCookingParams& params, const physx::PxSimpleTriangleMesh& mesh, physx::PxAllocatorCallback& inCallback, physx::PxU32& nbVerts, physx::PxVec3*& vertices,
														physx::PxU32& nbIndices, physx::PxU32*& indices, physx::PxU32& nbPolygons, physx::PxHullPolygon*& hullPolygons);

// ==== Triangle meshes ====

/**
\brief Verifies if the triangle mesh is valid. Prints an error message for each inconsistency found.

The following conditions are true for a valid triangle mesh:
1. There are no duplicate vertices (within specified vertexWeldTolerance. See PxCookingParams::meshWeldTolerance)
2. There are no large triangles (within specified PxTolerancesScale.)

\param[in] params	The cooking parameters
\param[in] desc		The triangle mesh descriptor to read the mesh from.

\return true if all the validity conditions hold, false otherwise.

\see PxCookTriangleMesh()
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxValidateTriangleMesh(const physx::PxCookingParams& params, const physx::PxTriangleMeshDesc& desc);

/**
\brief Cooks a triangle mesh. The results are written to the stream.

To create a triangle mesh object it is necessary to first 'cook' the mesh data into
a form which allows the SDK to perform efficient collision detection.

PxCookTriangleMesh() allows a mesh description to be cooked into a binary stream
suitable for loading and performing collision detection at runtime.

\param[in] params		The cooking parameters
\param[in]	desc		The triangle mesh descriptor to read the mesh from.
\param[in]	stream		User stream to output the cooked data.
\param[out]	condition	Result from triangle mesh cooking.
\return true on success

\see PxCookConvexMesh() PxPhysics.createTriangleMesh() PxTriangleMeshCookingResult::Enum
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookTriangleMesh(const physx::PxCookingParams& params, const physx::PxTriangleMeshDesc& desc, physx::PxOutputStream& stream, physx::PxTriangleMeshCookingResult::Enum* condition=NULL);

/**
\brief Cooks and creates a triangle mesh without going through a stream.

\note This method does the same as PxCookTriangleMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\note PxInsertionCallback can be obtained through PxPhysics::getPhysicsInsertionCallback()
or PxGetStandaloneInsertionCallback().

\param[in] params				The cooking parameters
\param[in] desc					The triangle mesh descriptor to read the mesh from.
\param[in] insertionCallback	The insertion interface from PxPhysics.
\param[out] condition			Result from triangle mesh cooking.
\return PxTriangleMesh pointer on success.	

\see PxCookTriangleMesh() PxPhysics.createTriangleMesh() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxTriangleMesh* PxCreateTriangleMesh(const physx::PxCookingParams& params, const physx::PxTriangleMeshDesc& desc, physx::PxInsertionCallback& insertionCallback, physx::PxTriangleMeshCookingResult::Enum* condition=NULL);

/**
\brief Cooks and creates a triangle mesh without going through a stream. Convenience function for standalone objects.

\note This method does the same as cookTriangleMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.
\return PxTriangleMesh pointer on success.

\param[in] params	The cooking parameters
\param[in] desc		The triangle mesh descriptor to read the mesh from.

\see PxCookTriangleMesh() PxPhysics.createTriangleMesh() PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxTriangleMesh*	PxCreateTriangleMesh(const physx::PxCookingParams& params, const physx::PxTriangleMeshDesc& desc)
{
	return PxCreateTriangleMesh(params, desc, *PxGetStandaloneInsertionCallback());
}

// ==== Tetrahedron & deformable volume meshes ====

/**
\brief Cooks a tetrahedron mesh. The results are written to the stream.

To create a tetrahedron mesh object it is necessary to first 'cook' the mesh data into
a form which allows the SDK to perform efficient collision detection.

PxCookTetrahedronMesh() allows a mesh description to be cooked into a binary stream
suitable for loading and performing collision detection at runtime.

\param[in] params		The cooking parameters
\param[in] meshDesc		The tetrahedron mesh descriptor to read the mesh from.
\param[in] stream		User stream to output the cooked data.
\return true on success

\see PxCookConvexMesh() PxPhysics.createTetrahedronMesh() 
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookTetrahedronMesh(const physx::PxCookingParams& params, const physx::PxTetrahedronMeshDesc& meshDesc, physx::PxOutputStream& stream);

/**
\brief Cooks and creates a tetrahedron mesh without going through a stream.

\note This method does the same as PxCookTetrahedronMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\note PxInsertionCallback can be obtained through PxPhysics::getPhysicsInsertionCallback()
or PxGetStandaloneInsertionCallback().

\param[in] params				The cooking parameters
\param[in] meshDesc				The tetrahedron mesh descriptor to read the mesh from.
\param[in] insertionCallback	The insertion interface from PxPhysics.
\return PxTetrahedronMesh pointer on success.

\see PxCookTetrahedronMesh() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxTetrahedronMesh* PxCreateTetrahedronMesh(const physx::PxCookingParams& params, const physx::PxTetrahedronMeshDesc& meshDesc, physx::PxInsertionCallback& insertionCallback);

/**
\brief Cooks and creates a tetrahedron mesh without going through a stream. Convenience function for standalone objects.

\note This method does the same as PxCookTetrahedronMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\param[in] params		The cooking parameters
\param[in] meshDesc		The tetrahedron mesh descriptor to read the mesh from.
\return PxTetrahedronMesh pointer on success.

\see PxCookTetrahedronMesh() PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxTetrahedronMesh*	PxCreateTetrahedronMesh(const physx::PxCookingParams& params, const physx::PxTetrahedronMeshDesc& meshDesc)
{
	return PxCreateTetrahedronMesh(params, meshDesc, *PxGetStandaloneInsertionCallback());
}

/**
\brief Cooks a deformable volume mesh. The results are written to the stream.

To create a deformable volume mesh object it is necessary to first 'cook' the mesh data into
a form which allows the SDK to perform efficient collision detection and to store data
used during the FEM calculations.

PxCookDeformableVolumeMesh() allows a mesh description to be cooked into a binary stream
suitable for loading and performing collision detection at runtime.

\param[in] params				The cooking parameters
\param[in] simulationMeshDesc	The tetrahedron mesh descriptor to read the simulation mesh from.
\param[in] collisionMeshDesc	The tetrahedron mesh descriptor to read the collision mesh from.
\param[in] simulationDataDesc	The deformable volume data descriptor to read mapping information from.
\param[in] stream				User stream to output the cooked data.
\return true on success

\see PxCookConvexMesh() PxPhysics.createTriangleMesh() PxTriangleMeshCookingResult::Enum
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	bool PxCookDeformableVolumeMesh(const physx::PxCookingParams& params,
	const physx::PxTetrahedronMeshDesc& simulationMeshDesc, const physx::PxTetrahedronMeshDesc& collisionMeshDesc,
	const physx::PxDeformableVolumeSimulationDataDesc& simulationDataDesc, physx::PxOutputStream& stream);

/**
\brief Cooks and creates a deformable volume mesh without going through a stream.

\note This method does the same as PxCookDeformableVolumeMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\note PxInsertionCallback can be obtained through PxPhysics::getPhysicsInsertionCallback()
or PxGetStandaloneInsertionCallback().

\param[in] params				The cooking parameters
\param[in] simulationMeshDesc	The tetrahedron mesh descriptor to read the simulation mesh from.
\param[in] collisionMeshDesc	The tetrahedron mesh descriptor to read the collision mesh from.
\param[in] simulationDataDesc	The deformable volume data descriptor to read mapping information from.
\param[in] insertionCallback	The insertion interface from PxPhysics.
\return PxDeformableVolumeMesh pointer on success.

\see PxCookTriangleMesh() PxPhysics.createTriangleMesh() PxInsertionCallback
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxDeformableVolumeMesh* PxCreateDeformableVolumeMesh(const physx::PxCookingParams& params, 
	const physx::PxTetrahedronMeshDesc& simulationMeshDesc, const physx::PxTetrahedronMeshDesc& collisionMeshDesc,
	const physx::PxDeformableVolumeSimulationDataDesc& simulationDataDesc, physx::PxInsertionCallback& insertionCallback);

/**
\brief Cooks and creates a deformable volume mesh without going through a stream. Convenience function for standalone objects.

\note This method does the same as PxCreateDeformableVolumeMesh, but the produced mesh is not stored
into a stream but is either directly inserted in PxPhysics, or created as a standalone
object. Use this method if you are unable to cook offline.

\param[in] params				The cooking parameters
\param[in] simulationMeshDesc	The tetrahedron mesh descriptor to read the simulation mesh from.
\param[in] collisionMeshDesc	The tetrahedron mesh descriptor to read the collision mesh from.
\param[in] simulationDataDesc	The deformable volume data descriptor to read mapping information from.
\return PxDeformableVolumeMesh pointer on success.

\see PxCookTriangleMesh() PxPhysics.createTriangleMesh() PxInsertionCallback
*/
PX_FORCE_INLINE	physx::PxDeformableVolumeMesh* PxCreateDeformableVolumeMesh(const physx::PxCookingParams& params,
	const physx::PxTetrahedronMeshDesc& simulationMeshDesc, const physx::PxTetrahedronMeshDesc& collisionMeshDesc,
	const physx::PxDeformableVolumeSimulationDataDesc& simulationDataDesc)
{
	return PxCreateDeformableVolumeMesh(params, simulationMeshDesc, collisionMeshDesc, simulationDataDesc, *PxGetStandaloneInsertionCallback());
}

/**
\brief Computes the mapping between collision and simulation mesh

The deformable volume deformation is computed on the simulation mesh. To deform the collision mesh accordingly
it needs to be specified how its vertices need to be placed and updated inside the deformation mesh.
This method computes that embedding information.

\param[in] params			The cooking parameters
\param[in] simulationMesh	A tetrahedral mesh that defines the shape of the simulation mesh which is used to compute the body's deformation			
\param[in] collisionMesh	A tetrahedral mesh that defines the shape of the collision mesh which is used for collision detection
\param[in] collisionData	A data container that contains acceleration structures and surface information of the collision mesh
\param[in] vertexToTet		Optional indices (array of integers) that specifies the index of the tetrahedron in the simulation mesh that 
	contains a collision mesh vertex. If not provided, the embedding will be computed internally. If the simulation mesh is obtained from 
	PxTetMaker::createVoxelTetrahedronMesh, then the vertexToTet map createVoxelTetrahedronMesh returned should be used here.
\return PxCollisionMeshMappingData pointer that describes how the collision mesh is embedded into the simulation mesh

\see PxTetMaker::createVoxelTetrahedronMesh
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxCollisionMeshMappingData* PxComputeModelsMapping(const physx::PxCookingParams& params,
	physx::PxTetrahedronMeshData& simulationMesh, const physx::PxTetrahedronMeshData& collisionMesh, 
	const physx::PxDeformableVolumeCollisionData& collisionData, const physx::PxBoundedData* vertexToTet = NULL);
	
/**
\brief Computes data to accelerate collision detection of tetrahedral meshes

Computes data structures to speed up collision detection with tetrahedral meshes.

\param[in] params				The cooking parameters
\param[in] collisionMeshDesc	Raw tetrahedral mesh descriptor which will be used for collision detection 
\return PxCollisionTetrahedronMeshData pointer that describes the collision mesh
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxCollisionTetrahedronMeshData* PxComputeCollisionData(const physx::PxCookingParams& params,
	const physx::PxTetrahedronMeshDesc& collisionMeshDesc);

/**
\brief Computes data to accelerate collision detection of tetrahedral meshes

Computes data to compute and store a deformable volume's deformation using FEM.

\param[in] params				The cooking parameters
\param[in] simulationMeshDesc	Raw tetrahedral mesh descriptor which will be used for FEM simulation
\return PxSimulationTetrahedronMeshData pointer that describes the simulation mesh
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxSimulationTetrahedronMeshData* PxComputeSimulationData(const physx::PxCookingParams& params,
	const physx::PxTetrahedronMeshDesc& simulationMeshDesc);

/**
\brief Bundles all data required for deformable volume simulation

Creates a container that provides everything to create a PxDeformableVolume

\param[in] simulationMesh		The geometry (tetrahedral mesh) to be used as simulation mesh
\param[in] simulationData		Additional non-tetmesh data that contains mass information etc. for the simulation mesh 
\param[in] collisionMesh		The geometry (tetrahedral mesh) to be used for collision detection
\param[in] collisionData		Additional non-tetmesh data that contains surface information, acceleration structures etc. for the simulation mesh 
\param[in] mappingData			Mapping that describes how the collision mesh's vertices are embedded into the simulation mesh
\param[in] insertionCallback	The insertion interface from PxPhysics.
\return PxDeformableVolumeMesh pointer that represents a deformable volume mesh bundling all data (simulation mesh, collision mesh etc.)

\see PxDeformableVolume createDeformableVolume()
*/
PX_C_EXPORT PX_PHYSX_COOKING_API	physx::PxDeformableVolumeMesh*	PxAssembleDeformableVolumeMesh(physx::PxTetrahedronMeshData& simulationMesh,
	physx::PxDeformableVolumeSimulationData& simulationData, physx::PxTetrahedronMeshData& collisionMesh, physx::PxDeformableVolumeCollisionData& collisionData,
	physx::PxCollisionMeshMappingData& mappingData, physx::PxInsertionCallback& insertionCallback);

#endif
