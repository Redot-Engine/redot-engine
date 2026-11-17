// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of NVIDIA CORPORATION nor the names of its
//    contributors may be used to endorse or promote products derived
//    from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
// OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Copyright (c) 2016-2024 NVIDIA Corporation. All rights reserved.

//! @file
//!
//! @brief Defines the API for the NvBlastExtAuthoring blast sdk extension

#ifndef NVBLASTAUTHORING_H
#define NVBLASTAUTHORING_H

#include "NvBlastExtAuthoringTypes.h"

namespace Nv
{
namespace Blast
{
class Mesh;
class VoronoiSitesGenerator;
class CutoutSet;
class RandomGeneratorBase;
class FractureTool;
class ConvexMeshBuilder;
class BlastBondGenerator;
class MeshCleaner;
class PatternGenerator;
class SpatialGrid;
class SpatialAccelerator;
class BooleanTool;
}  // namespace Blast
}  // namespace Nv

struct NvBlastExtAssetUtilsBondDesc;

/**
Constructs mesh object from array of triangles.
User should call release() after usage.

\param[in] positions        Array for vertex positions, 3 * verticesCount floats will be read
\param[in] normals          Array for vertex normals, 3 * verticesCount floats will be read
\param[in] uv               Array for vertex uv coordinates, 2 * verticesCount floats will be read
\param[in] verticesCount    Number of vertices in mesh
\param[in] indices          Array of vertex indices. Indices contain vertex index triplets which form a mesh triangle.
\param[in] indicesCount     Indices count (should be equal to numberOfTriangles * 3)

\return pointer to Nv::Blast::Mesh if it was created succefully otherwise return nullptr
*/
NV_C_API Nv::Blast::Mesh*
NvBlastExtAuthoringCreateMesh(const NvcVec3* positions, const NvcVec3* normals, const NvcVec2* uv,
                              uint32_t verticesCount, const uint32_t* indices, uint32_t indicesCount);

/**
Constructs mesh object from triangles represented as arrays of vertices, indices and per facet material.
User should call Mesh::release() after usage.

\param[in] vertices         Array for vertex positions, 3 * verticesCount floats will be read
\param[in] verticesCount    Number of vertices in mesh
\param[in] indices          Array of vertex indices. Indices contain vertex index triplets which form a mesh triangle.
\param[in] indicesCount     Indices count (should be equal to numberOfTriangles * 3)
\param[in] materials        Array of material indices per triangle. If not set default material (0) will be assigned.
\param[in] materialStride   Stride for material indices

\return pointer to Nv::Blast::Mesh if it was created succefully otherwise return nullptr
*/
NV_C_API Nv::Blast::Mesh*
NvBlastExtAuthoringCreateMeshOnlyTriangles(const void* vertices, uint32_t verticesCount, uint32_t* indices,
                                           uint32_t indexCount, void* materials = nullptr, uint32_t materialStride = 4);

/**
Constructs mesh object from array of vertices, edges and facets.
User should call release() after usage.

\param[in] vertices         Array for Nv::Blast::Vertex
\param[in] edges            Array for Nv::Blast::Edge
\param[in] facets           Array for Nv::Blast::Facet
\param[in] verticesCount    Number of vertices in mesh
\param[in] edgesCount       Number of edges in mesh
\param[in] facetsCount      Number of facets in mesh

\return pointer to Nv::Blast::Mesh if it was created succefully otherwise return nullptr
*/
NV_C_API Nv::Blast::Mesh*
NvBlastExtAuthoringCreateMeshFromFacets(const void* vertices, const void* edges, const void* facets,
                                        uint32_t verticesCount, uint32_t edgesCount, uint32_t facetsCount);

/**
Voronoi sites should not be generated outside of the fractured mesh, so VoronoiSitesGenerator
should be supplied with fracture mesh.
\param[in] mesh         Fracture mesh
\param[in] rnd          User supplied random value generator.
\return                 Pointer to VoronoiSitesGenerator. User's code should release it after usage.
*/
NV_C_API Nv::Blast::VoronoiSitesGenerator*
NvBlastExtAuthoringCreateVoronoiSitesGenerator(Nv::Blast::Mesh* mesh, Nv::Blast::RandomGeneratorBase* rng);

/** Instantiates a blank CutoutSet */
NV_C_API Nv::Blast::CutoutSet* NvBlastExtAuthoringCreateCutoutSet();

/**
Builds a cutout set (which must have been initially created by createCutoutSet()).
Uses a bitmap described by pixelBuffer, bufferWidth, and bufferHeight.  Each pixel is represented
by one byte in the buffer.

\param cutoutSet        the CutoutSet to build
\param pixelBuffer      pointer to be beginning of the pixel buffer
\param bufferWidth      the width of the buffer in pixels
\param bufferHeight     the height of the buffer in pixels
\param segmentationErrorThreshold   Reduce the number of vertices on curve untill segmentation error is smaller then
specified. By default set it to 0.001. \param snapThreshold the pixel distance at which neighboring cutout vertices and
segments may be fudged into alignment. By default set it to 1.
\param periodic         whether or not to use periodic boundary conditions when creating cutouts from the map
\param expandGaps       expand cutout regions to gaps or keep it as is

*/
NV_C_API void
NvBlastExtAuthoringBuildCutoutSet(Nv::Blast::CutoutSet& cutoutSet, const uint8_t* pixelBuffer, uint32_t bufferWidth,
                                  uint32_t bufferHeight, float segmentationErrorThreshold, float snapThreshold,
                                  bool periodic, bool expandGaps);

/**
Create FractureTool object.
\return Pointer to create FractureTool. User's code should release it after usage.
*/
NV_C_API Nv::Blast::FractureTool* NvBlastExtAuthoringCreateFractureTool();

/**
Create BlastBondGenerator
\return Pointer to created BlastBondGenerator. User's code should release it after usage.
*/
NV_C_API Nv::Blast::BlastBondGenerator* NvBlastExtAuthoringCreateBondGenerator(Nv::Blast::ConvexMeshBuilder* builder);

/**
Build convex mesh decomposition.
\param[in] mesh             Triangle mesh to decompose.
\param[in] triangleCount    Number of triangles in mesh.
\param[in] params           Parameters for convex mesh decomposition builder.
\param[out] convexes        The resulting convex hulls.

\return Number of created convex hulls.
*/
NV_C_API int32_t NvBlastExtAuthoringBuildMeshConvexDecomposition(Nv::Blast::ConvexMeshBuilder* cmb,
                                                                    const Nv::Blast::Triangle* mesh,
                                                                    uint32_t triangleCount,
                                                                    const Nv::Blast::ConvexDecompositionParams& params,
                                                                    Nv::Blast::CollisionHull**& convexes);


/**
    Convex geometry trimming.
    Using slicing with noised slicing surface can result in intersecting collision geometry.
    It leads to unstable behaviour of rigid body simulation.
    This method trims all intersecting parts of collision geometry.
    As a drawback, trimming collision geometry can lead to penetrating render meshes during simulation.

    \param[in]      chunksCount Number of chunks
    \param[in,out]  in          ConvexHull geometry which should be clipped.
    \param[in]      chunkDepth  Array of depth levels of convex hulls corresponding chunks.

*/
NV_C_API void NvBlastExtAuthoringTrimCollisionGeometry(Nv::Blast::ConvexMeshBuilder* cmb, uint32_t chunksCount,
                                                          Nv::Blast::CollisionHull** in, const uint32_t* chunkDepth);


/**
Transforms collision hull in place using scale, rotation, transform.
\param[in, out] hull        Pointer to the hull to be transformed (modified).
\param[in]      scale       Pointer to scale to be applied. Can be nullptr.
\param[in]      rotation    Pointer to rotation to be applied. Can be nullptr.
\param[in]      translation Pointer to translation to be applied. Can be nullptr.
*/
NV_C_API void NvBlastExtAuthoringTransformCollisionHullInPlace(Nv::Blast::CollisionHull* hull, const NvcVec3* scaling,
                                                                  const NvcQuat* rotation, const NvcVec3* translation);

/**
Transforms collision hull in place using scale, rotation, transform.
\param[in]  hull        Pointer to the hull to be transformed (modified).
\param[in]      scale       Pointer to scale to be applied. Can be nullptr.
\param[in]      rotation    Pointer to rotation to be applied. Can be nullptr.
\param[in]      translation Pointer to translation to be applied. Can be nullptr.
*/
NV_C_API Nv::Blast::CollisionHull*
NvBlastExtAuthoringTransformCollisionHull(const Nv::Blast::CollisionHull* hull, const NvcVec3* scaling,
                                          const NvcQuat* rotation, const NvcVec3* translation);

/**
Performs pending fractures and generates fractured asset, render and collision geometry

\param[in]  fTool               Fracture tool created by NvBlastExtAuthoringCreateFractureTool
\param[in]  bondGenerator       Bond generator created by NvBlastExtAuthoringCreateBondGenerator
\param[in]  collisionBuilder    Collision builder created by NvBlastExtAuthoringCreateConvexMeshBuilder
\param[in]  defaultSupportDepth All new chunks will be marked as support if its depth equal to defaultSupportDepth.
                                By default leaves (chunks without children) marked as support.
\param[in]  collisionParam      Parameters of collision hulls generation.
\return     Authoring result
*/
NV_C_API Nv::Blast::AuthoringResult*
NvBlastExtAuthoringProcessFracture(Nv::Blast::FractureTool& fTool, Nv::Blast::BlastBondGenerator& bondGenerator,
                                   Nv::Blast::ConvexMeshBuilder& collisionBuilder,
                                   const Nv::Blast::ConvexDecompositionParams& collisionParam,
                                   int32_t defaultSupportDepth = -1);


/**
Releases collision data for AuthoringResult. AuthoringResult should be created by NvBlast.
*/
NV_C_API void NvBlastExtAuthoringReleaseAuthoringResultCollision(Nv::Blast::ConvexMeshBuilder& collisionBuilder, Nv::Blast::AuthoringResult* ar);

/**
Releases AuthoringResult data. AuthoringResult should be created by NvBlast.
*/
NV_C_API void NvBlastExtAuthoringReleaseAuthoringResult(Nv::Blast::ConvexMeshBuilder& collisionBuilder, Nv::Blast::AuthoringResult* ar);


/**
Updates graphics mesh only

\param[in]  fTool               Fracture tool created by NvBlastExtAuthoringCreateFractureTool
\param[out] ares                AuthoringResult object which contains chunks, for which rendermeshes will be updated
(e.g. to tweak UVs). Initially should be created by NvBlastExtAuthoringProcessFracture.
*/
NV_C_API void NvBlastExtAuthoringUpdateGraphicsMesh(Nv::Blast::FractureTool& fTool, Nv::Blast::AuthoringResult& ares);

/**
Build collision meshes

\param[in,out]  ares                AuthoringResult object which contains chunks, for which collision meshes will be
built. \param[in]       collisionBuilder    Reference to ConvexMeshBuilder instance. \param[in]     collisionParam
Parameters of collision hulls generation.
\param[in]      chunksToProcessCount Number of chunk indices in chunksToProcess memory buffer.
\param[in]      chunksToProcess     Chunk indices for which collision mesh should be built.
*/
NV_C_API void NvBlastExtAuthoringBuildCollisionMeshes(Nv::Blast::AuthoringResult& ares,
                                                         Nv::Blast::ConvexMeshBuilder& collisionBuilder,
                                                         const Nv::Blast::ConvexDecompositionParams& collisionParam,
                                                         uint32_t chunksToProcessCount, uint32_t* chunksToProcess);

/**
    Creates MeshCleaner object
    \return pointer to Nv::Blast::Mesh if it was created succefully otherwise return nullptr
*/
NV_C_API Nv::Blast::MeshCleaner* NvBlastExtAuthoringCreateMeshCleaner();

/**
Finds bonds connecting chunks in a list of assets

New bond descriptors may be given to bond support chunks from different components.

An NvBlastAsset may appear more than once in the components array.

NOTE: This function allocates memory using the allocator in NvBlastGlobals, to create the new bond
descriptor arrays returned. The user must free this memory after use with NVBLAST_FREE

\param[in]  components          An array of assets to merge, of size componentCount.
\param[in]  scales              If not NULL, an array of size componentCount of scales to apply to the geometric data in
                                the chunks and bonds. If NULL, no scaling is applied.
\param[in]  rotations           If not NULL, an array of size componentCount of rotations to apply to the geometric data
                                in the chunks and bonds.  The quaternions MUST be normalized. If NULL, no rotations are applied.
\param[in]  translations        If not NULL, an array of of size componentCount of translations to apply to the
                                geometric data in the chunks and bonds.  If NULL, no translations are applied.
\param[in]  convexHullOffsets   For each component, an array of chunkSize+1 specifying the start of the convex hulls for that
                                chunk inside the chunkHulls array for that component.
\param[in]  chunkHulls          For each component, an array of CollisionHull* specifying the collision geometry for the
                                chunks in that component.
\param[in]  componentCount      The size of the components and relativeTransforms arrays.
\param[out] newBondDescs        Descriptors of type NvBlastExtAssetUtilsBondDesc for new bonds between components.
\param[in]  maxSeparation       Maximal distance between chunks which can be connected by bond.

\return the number of bonds in newBondDescs
*/
NV_C_API uint32_t NvBlastExtAuthoringFindAssetConnectingBonds(
    const NvBlastAsset** components, const NvcVec3* scales, const NvcQuat* rotations, const NvcVec3* translations,
    const uint32_t** convexHullOffsets, const Nv::Blast::CollisionHull*** chunkHulls, uint32_t componentCount,
    NvBlastExtAssetUtilsBondDesc*& newBondDescs, float maxSeparation = 0.0f);

/**
Returns pattern generator used for generating fracture patterns.
*/
NV_C_API Nv::Blast::PatternGenerator* NvBlastExtAuthoringCreatePatternGenerator();

/**
Create spatial grid for mesh.
Release using Nv::Blast::SpatialGrid::release()
*/
NV_C_API Nv::Blast::SpatialGrid* NvBlastExtAuthoringCreateSpatialGrid(uint32_t resolution, const Nv::Blast::Mesh* m);

/**
Create GridAccelerator - SpatialAccelerator which use Grid for faster mesh sampling.
Release using Nv::Blast::SpatialAccelerator::release()
*/
NV_C_API Nv::Blast::SpatialAccelerator* NvBlastExtAuthoringCreateGridAccelerator(Nv::Blast::SpatialGrid* parent);

/**
Create SweepingAccelerator - SpatialAccelerator which uses a sweep algorithm.
Release using Nv::Blast::SpatialAccelerator::release()
*/
NV_C_API Nv::Blast::SpatialAccelerator* NvBlastExtAuthoringCreateSweepingAccelerator(const Nv::Blast::Mesh* m);

/**
Create BBoxBasedAccelerator - SpatialAccelerator which uses a bbox/grid algorithm.
Release using Nv::Blast::SpatialAccelerator::release()
*/
NV_C_API Nv::Blast::SpatialAccelerator* NvBlastExtAuthoringCreateBBoxBasedAccelerator(uint32_t resolution, const Nv::Blast::Mesh* m);

#define kBBoxBasedAcceleratorDefaultResolution 10

/**
Create BooleanTool object.
\return Pointer to created BooleanTool. User's code should release it after usage.
*/
NV_C_API Nv::Blast::BooleanTool* NvBlastExtAuthoringCreateBooleanTool();


#endif  // ifndef NVBLASTAUTHORING_H
