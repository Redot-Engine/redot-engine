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
//! @brief Defines the API for the NvBlastExtAuthoring blast sdk extension's FractureTool

#ifndef NVBLASTAUTHORINGFRACTURETOOL_H
#define NVBLASTAUTHORINGFRACTURETOOL_H

#include "NvBlastExtAuthoringTypes.h"

namespace Nv
{
namespace Blast
{

class SpatialAccelerator;
class Triangulator;
class Mesh;
class CutoutSet;

/*
    Transform used for chunk scaling (uniform scale + translation only)
*/
struct TransformST
{
    NvcVec3 t;  // Translation
    float s;    // Uniform scale

    static TransformST identity() { return {{0.0f, 0.0f, 0.0f}, 1.0f}; }

    /* Point and vector transformations.  Note, normals are invariant (up to normalization) under TransformST transformations. */
    NvcVec3 transformPos(const NvcVec3& p) const { return {s * p.x + t.x, s * p.y + t.y, s * p.z + t.z}; }
    NvcVec3 transformDir(const NvcVec3& d) const { return {s * d.x, s * d.y, s * d.z}; }

    NvcVec3 invTransformPos(const NvcVec3& p) const { return {(p.x - t.x) / s, (p.y - t.y) / s, (p.z - t.z) / s}; }
    NvcVec3 invTransformDir(const NvcVec3& d) const { return {d.x / s, d.y / s, d.z / s}; }
};

/*
    Chunk data, chunks with parentChunkId == -1 are the source meshes.
*/
struct ChunkInfo
{
    ChunkInfo();

    enum ChunkFlags
    {
        NO_FLAGS            = 0,
        APPROXIMATE_BONDING = 1 // Created by island splitting or chunk merge, etc. and should check for inexact bonds
    };

protected:
    /**
     * The mesh is transformed to fit within a unit cube centered at the origin.
     * This transform puts the mesh back into its original space.
     * These fields are protected so that only an authoring class can access them.
     * It is important that the tmToWorld be set based upon the mesh bounds and parent tmToWorld.
     */
    TransformST tmToWorld;
    Mesh* meshData;

    /**
     * Parent ID is set to this value initially, as opposed to -1 (which is a valid parent ID denoting "no parent")
     */
    enum { UninitializedID = INT32_MIN };

public:
    int32_t parentChunkId;
    int32_t chunkId;
    uint32_t flags;
    bool isLeaf;
    bool isChanged;

    const TransformST& getTmToWorld() const { return tmToWorld; }
    Mesh* getMesh() const { return meshData; }
};

inline ChunkInfo::ChunkInfo() :
    tmToWorld(TransformST::identity()),
    meshData(nullptr),
    parentChunkId(UninitializedID),
    chunkId(-1),
    flags(NO_FLAGS),
    isLeaf(false),
    isChanged(true)
{
}


/**
    Abstract base class for user-defined random value generator.
*/
class RandomGeneratorBase
{
  public:
    // Generates uniformly distributed value in [0, 1] range.
    virtual float getRandomValue() = 0;
    // Seeds random value generator
    virtual void seed(int32_t seed) = 0;
    virtual ~RandomGeneratorBase(){};
};

/*
    Noise fracturing configuration for chunks's faces
*/
struct NoiseConfiguration
{
    /**
    Noisy slicing configutaion:

    Amplitude of cutting surface noise. If it is 0 - noise is disabled.
    */
    float amplitude = 0.f;

    /**
    Frequencey of cutting surface noise.
    */
    float frequency = 1.f;

    /**
    Octave number in slicing surface noise.
    */
    uint32_t octaveNumber = 1;

    /**
    Sampling interval for surface grid.
    */
    NvcVec3 samplingInterval = { 1, 1, 1 };
};

/*
    Slicing fracturing configuration
*/
struct SlicingConfiguration
{
    /**
        Number of slices in each direction
    */
    int32_t x_slices = 1, y_slices = 1, z_slices = 1;

    /**
        Offset variation, value in [0, 1]
    */
    float offset_variations = 0.f;

    /**
        Angle variation, value in [0, 1]
    */
    float angle_variations = 0.f;

    /*
        Noise parameters for faces between sliced chunks
    */
    NoiseConfiguration noise;
};

/**
    Cutout fracturing configuration
*/
struct CutoutConfiguration
{
    /**
        Set of grouped convex loop patterns for cutout in normal direction.
        Not required for PLANE_ONLY mode
    */
    CutoutSet* cutoutSet = nullptr;

    /**
        Transform for initial pattern position and orientation.
        By default 2d pattern lies in XY plane (Y is up) the center of pattern is (0, 0)
    */
    NvcTransform transform = {{0, 0, 0, 1}, {0, 0, 0}};

    /**
        Scale for pattern. Unscaled pattern has size (1, 1).
        For negative scale pattern will be placed at the center of chunk and scaled with max distance between points of
       its AABB
    */
    NvcVec2 scale = { -1, -1 };

    /**
        Conic aperture in degree, for cylindric cutout set it to 0.
    */
    float aperture = 0.f;

    /**
        If relative transform is set - position will be displacement vector from chunk's center. Otherwise from global
       origin.
    */
    bool isRelativeTransform = true;

    /**
    Add generatad faces to the same smoothing group as original face without noise
    */
    bool useSmoothing = false;

    /**
        Noise parameters for cutout surface, see NoiseConfiguration.
    */
    NoiseConfiguration noise;
};

/**
    Class for voronoi sites generation inside supplied mesh.
*/
class VoronoiSitesGenerator
{
  public:
    virtual ~VoronoiSitesGenerator() {}

    /**
        Release VoronoiSitesGenerator memory
    */
    virtual void release() = 0;

    /**
        Set base fracture mesh
    */
    virtual void setBaseMesh(const Mesh* mesh) = 0;

    /**
        Access to generated voronoi sites.
        \param[out]             Pointer to generated voronoi sites
        \return                 Count of generated voronoi sites.
    */
    virtual uint32_t getVoronoiSites(const NvcVec3*& sites) = 0;

    /**
        Add site in particular point
        \param[in] site     Site coordinates
    */
    virtual void addSite(const NvcVec3& site) = 0;
    /**
        Uniformly generate sites inside the mesh
        \param[in] numberOfSites    Number of generated sites
    */
    virtual void uniformlyGenerateSitesInMesh(uint32_t numberOfSites) = 0;

    /**
        Generate sites in clustered fashion
        \param[in] numberOfClusters Number of generated clusters
        \param[in] sitesPerCluster  Number of sites in each cluster
        \param[in] clusterRadius    Voronoi cells cluster radius
    */
    virtual void clusteredSitesGeneration(uint32_t numberOfClusters, uint32_t sitesPerCluster, float clusterRadius) = 0;

    /**
        Radial pattern of sites generation
        \param[in] center       Center of generated pattern
        \param[in] normal       Normal to plane in which sites are generated
        \param[in] radius       Pattern radius
        \param[in] angularSteps Number of angular steps
        \param[in] radialSteps  Number of radial steps
        \param[in] angleOffset  Angle offset at each radial step
        \param[in] variability  Randomness of sites distribution
    */
    virtual void radialPattern(const NvcVec3& center, const NvcVec3& normal, float radius, int32_t angularSteps,
                               int32_t radialSteps, float angleOffset = 0.0f, float variability = 0.0f) = 0;

    /**
        Generate sites inside sphere
        \param[in] count        Count of generated sites
        \param[in] radius       Radius of sphere
        \param[in] center       Center of sphere
    */
    virtual void generateInSphere(const uint32_t count, const float radius, const NvcVec3& center) = 0;

    /**
        Set stencil mesh. With stencil mesh sites are generated only inside both of fracture and stencil meshes.
        \param[in] stencil      Stencil mesh.
    */
    virtual void setStencil(const Mesh* stencil) = 0;

    /**
        Removes stencil mesh
    */
    virtual void clearStencil() = 0;

    /**
        Deletes sites inside supplied sphere
        \param[in] radius               Radius of sphere
        \param[in] center               Center of sphere
        \param[in] eraserProbability    Probability of removing some particular site
    */
    virtual void deleteInSphere(const float radius, const NvcVec3& center, const float eraserProbability = 1) = 0;
};

/**
    FractureTool class provides methods to fracture provided mesh and generate Blast asset data
*/
class FractureTool
{

  public:
    virtual ~FractureTool() {}

    /**
        Release FractureTool memory
    */
    virtual void release() = 0;

    /**
        Reset FractureTool state.
    */
    virtual void reset() = 0;


    /**
        Set input meshes which will be fractured, FractureTool will be reset.
        If ids != nullptr, it must point to an array of length meshSizes.
        Each mesh will be assigned to a chunk with ID given by the corresponding element in ids.
        If the corresponding element is negative, or ids is NULL, then the chunk will be assigned
        an arbitrary (but currently unused) ID.
        Returns true iff all meshes were assigned chunks with valid IDs.
    */
    virtual bool setSourceMeshes(Mesh const * const * meshes, uint32_t meshesSize, const int32_t* ids = nullptr) = 0;

    /**
        Set chunk mesh, parentId should be valid, return id of new chunk.
    */
    virtual int32_t setChunkMesh(const Mesh* mesh, int32_t parentId, int32_t chunkId = -1) = 0;

    /**
    Set the material id to use for new interior faces. Defaults to kMaterialInteriorId
    */
    virtual void setInteriorMaterialId(int32_t materialId) = 0;

    /**
    Gets the material id to use for new interior faces
    */
    virtual int32_t getInteriorMaterialId() const = 0;

    /**
    Replaces an material id on faces with a new one
    */
    virtual void replaceMaterialId(int32_t oldMaterialId, int32_t newMaterialId) = 0;

    /**
        Get chunk mesh in polygonal representation. User's code should release it after usage.
        This function welds vertices based upon vertex position and normal.  If splitUVs == true,
        UV coordinates are also considered in vertex welding.
    */
    virtual Mesh* createChunkMesh(int32_t chunkInfoIndex, bool splitUVs = true) = 0;

    /**
        Fractures specified chunk with voronoi method.
        \param[in] chunkId              Chunk to fracture
        \param[in] cellPoints           Array of voronoi sites
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
       generated chunks will be at next depth level, source chunk will be parent for them. Case replaceChunk == true &&
       chunkId == 0 considered as wrong input parameters \return   If 0, fracturing is successful.
    */
    virtual int32_t
    voronoiFracturing(uint32_t chunkId, uint32_t cellCount, const NvcVec3* cellPoints, bool replaceChunk) = 0;

    /**
        Fractures specified chunk with voronoi method. Cells can be scaled along x,y,z axes.
        \param[in] chunkId              Chunk to fracture
        \param[in] cellPoints           Array of voronoi sites
        \param[in] cellPoints           Array of voronoi sites
        \param[in] scale                Voronoi cells scaling factor
        \param[in] rotation             Voronoi cells rotation. Has no effect without cells scale factor
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
       generated chunks will be at next depth level, source chunk will be parent for them. Case replaceChunk == true &&
       chunkId == 0 considered as wrong input parameters \return   If 0, fracturing is successful.
    */
    virtual int32_t voronoiFracturing(uint32_t chunkId, uint32_t cellCount, const NvcVec3* cellPoints,
                                      const NvcVec3& scale, const NvcQuat& rotation, bool replaceChunk) = 0;


    /**
        Fractures specified chunk with slicing method.
        \param[in] chunkId              Chunk to fracture
        \param[in] conf                 Slicing parameters, see SlicingConfiguration.
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
                                        generated chunks will be at next depth level, source chunk will be parent for
                                        them. Case replaceChunk == true && chunkId == 0 considered as wrong input parameters
        \param[in] rnd                  User supplied random number generator

        \return   If 0, fracturing is successful.
    */
    virtual int32_t
    slicing(uint32_t chunkId, const SlicingConfiguration& conf, bool replaceChunk, RandomGeneratorBase* rnd) = 0;

    /**
        Cut chunk with plane.
        \param[in] chunkId              Chunk to fracture
        \param[in] normal               Plane normal
        \param[in] position             Point on plane
        \param[in] noise                Noise configuration for plane-chunk intersection, see NoiseConfiguration.
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
                                        generated chunks will be at next depth level, source chunk will be parent for
                                        them. Case replaceChunk == true && chunkId == 0 considered as wrong input parameters
        \param[in] rnd                  User supplied random number generator

        \return   If 0, fracturing is successful.
    */
    virtual int32_t cut(uint32_t chunkId, const NvcVec3& normal, const NvcVec3& position,
                        const NoiseConfiguration& noise, bool replaceChunk, RandomGeneratorBase* rnd) = 0;

    /**
        Cutout fracture for specified chunk.
        \param[in] chunkId              Chunk to fracture
        \param[in] conf                 Cutout parameters, see CutoutConfiguration.
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
        \param[in] replaceChunk         if 'true', newly generated chunks will replace source chunk, if 'false', newly
                                        generated chunks will be at next depth level, source chunk will be parent for
                                        them. Case replaceChunk == true && chunkId == 0 considered as wrong input parameters
        \param[in] rnd                  User supplied random number generator

        \return   If 0, fracturing is successful.
    */
    virtual int32_t cutout(uint32_t chunkId, CutoutConfiguration conf, bool replaceChunk, RandomGeneratorBase* rnd) = 0;


    /**
        Creates resulting fractured mesh geometry from intermediate format
    */
    virtual void finalizeFracturing() = 0;

    /**
        Returns overall number of chunks in fracture.
    */
    virtual uint32_t getChunkCount() const = 0;

    /**
        Get chunk information
    */
    virtual const ChunkInfo& getChunkInfo(int32_t chunkInfoIndex) = 0;

    /**
        Get percentage of mesh overlap.
        percentage computed as volume(intersection(meshA , meshB)) / volume (meshA)
        \param[in] meshA Mesh A
        \param[in] meshB Mesh B
        \return mesh overlap percentage
    */
    virtual float getMeshOverlap(const Mesh& meshA, const Mesh& meshB) = 0;

    /**
        Get chunk base mesh
        \param[in] chunkIndex Chunk index
        \param[out] output Array of triangles to be filled
        \return number of triangles in base mesh
    */
    virtual uint32_t getBaseMesh(int32_t chunkIndex, Triangle*& output) = 0;

    /**
        Update chunk base mesh
        \note Doesn't allocates output array, Triangle* output should be preallocated by user
        \param[in] chunkIndex Chunk index
        \param[out] output Array of triangles to be filled
        \return number of triangles in base mesh
    */
    virtual uint32_t updateBaseMesh(int32_t chunkIndex, Triangle* output) = 0;

    /**
        Return info index of chunk with specified chunkId
        \param[in] chunkId Chunk ID
        \return Chunk info index in internal buffer, if not exist -1 is returned.
    */
    virtual int32_t getChunkInfoIndex(int32_t chunkId) const = 0;

    /**
        Return id of chunk with specified info index.
        \param[in] chunkInfoIndex Chunk info index
        \return Chunk id or -1 if there is no such chunk.
    */
    virtual int32_t getChunkId(int32_t chunkInfoIndex) const = 0;

    /**
        Return depth level of the given chunk
        \param[in] chunkId Chunk ID
        \return Chunk depth or -1 if there is no such chunk.
    */
    virtual int32_t getChunkDepth(int32_t chunkId) const = 0;

    /**
        Return array of chunks IDs with given depth.
        \param[in]  depth Chunk depth
        \param[out] Pointer to array of chunk IDs
        \return Number of chunks in array
    */
    virtual uint32_t getChunksIdAtDepth(uint32_t depth, int32_t*& chunkIds) const = 0;

    /**
        Get result geometry without noise as vertex and index buffers, where index buffers contain series of triplets
        which represent triangles.
        \param[out] vertexBuffer Array of vertices to be filled
        \param[out] indexBuffer Array of indices to be filled
        \param[out] indexBufferOffsets Array of offsets in indexBuffer for each base mesh.
                    Contains getChunkCount() + 1 elements. Last one is indexBuffer size
        \return Number of vertices in vertexBuffer
    */
    virtual uint32_t
    getBufferedBaseMeshes(Vertex*& vertexBuffer, uint32_t*& indexBuffer, uint32_t*& indexBufferOffsets) = 0;

    /**
        Set automatic islands removing. May cause instabilities.
        \param[in] isRemoveIslands Flag whether remove or not islands.
    */
    virtual void setRemoveIslands(bool isRemoveIslands) = 0;

    /**
        Try find islands and remove them on some specifical chunk. If chunk has childs, island removing can lead to
       wrong results! Apply it before further chunk splitting. \param[in] chunkId Chunk ID which should be checked for
       islands \return Number of found islands is returned
    */
    virtual int32_t islandDetectionAndRemoving(int32_t chunkId, bool createAtNewDepth = false) = 0;

    /**
        Check if input mesh contains open edges. Open edges can lead to wrong fracturing results.
        \return true if mesh contains open edges
    */
    virtual bool isMeshContainOpenEdges(const Mesh* input) = 0;

    /**
        Delete all children for specified chunk (also recursively delete chidren of children).
        \param[in] chunkId Chunk ID which children should be deleted
        \param[in] deleteRoot (optional) If true, deletes the given chunk too
        \return true if one or more chunks were removed
    */
    virtual bool deleteChunkSubhierarchy(int32_t chunkId, bool deleteRoot = false) = 0;

    /**
        Optimize chunk hierarhy for better runtime performance.
        It tries to unite chunks to groups of some size in order to transform flat hierarchy (all chunks are children of
        single root) to tree like hieracrhy with limited number of children for each chunk.
        \param[in] threshold    If number of children of some chunk less then maxAtLevel then it would be considered as already
            optimized and skipped.
        \param[in] targetClusterSize Target number of children for processed chunks.
        \param[in] chunksToMerge Which chunks are merge candidate.  If NULL, all chunks will be a merge candidate.
        \param[in] mergeChunkCount size of chunksToMerge array, if chunksToMerge != NULL.
        \param[in] adjChunks    Optional index pairs to describe chunk adjacency.  May be NULL.
        \param[in] adjChunksSize If 'adjChunks' is not NULL, the number of index pairs in the adjChunks array.
        \param[in] removeOriginalChunks If true, original chunks that are merged are removed.
    */
    virtual void uniteChunks(uint32_t threshold, uint32_t targetClusterSize,
                             const uint32_t* chunksToMerge, uint32_t mergeChunkCount,
                             const NvcVec2i* adjChunks, uint32_t adjChunksSize,
                             bool removeOriginalChunks = false) = 0;

    /**
        Set the APPROXIMATE_BONDING flag in the chunk's ChunkInfo
        \param[in] chunkInfoIndex chunk info index - use getChunkInfoIndex(ID)
        \param[in] useApproximateBonding value of flag to set
        \return true if the chunk ID is found, false otherwise
    */
    virtual bool setApproximateBonding(uint32_t chunkInfoIndex, bool useApproximateBonding) = 0;

    /**
        Rescale interior uv coordinates of given chunk to fit square of given size.
        \param[in] side Size of square side
        \param[in] chunkId Chunk ID for which UVs should be scaled.
    */
    virtual void fitUvToRect(float side, uint32_t chunkId) = 0;

    /**
        Rescale interior uv coordinates of all existing chunks to fit square of given size, relative sizes will be
       preserved. \param[in] side Size of square side
    */
    virtual void fitAllUvToRect(float side) = 0;
};

}  // namespace Blast
}  // namespace Nv

#endif  // ifndef NVBLASTAUTHORINGFRACTURETOOL_H
