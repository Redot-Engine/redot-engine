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
//! @brief NvBlastExtStressSolver blast extension, provides functionality to calculate stress on a destructible

#ifndef NVBLASTEXTSTRESSSOLVER_H
#define NVBLASTEXTSTRESSSOLVER_H

#include "NvBlastTypes.h"
#include "NvCTypes.h"


namespace Nv
{
namespace Blast
{

/**
Stress Solver Settings

Stress on every bond is calculated with these components:
    compression/tension (parallel to bond normal)
    shear (perpendicular to bond normal)
Damage is done based on the limits defined in this structure to simulate micro bonds in the material breaking
Units for all limits are in pascals

Support graph reduction:
graphReductionLevel is the number of node merge passes.  The resulting graph will be
roughly 2^graphReductionLevel times smaller than the original.
NOTE: the reduction is currently fairly random and can lead to interlocked actors when solver bonds break.
If we are going to keep the feature, the algorithm for combining bonds should be revisited to take locality into account.
*/
struct ExtStressSolverSettings
{
    uint32_t    maxSolverIterationsPerFrame;//!<    the maximum number of iterations to perform per frame
    uint32_t    graphReductionLevel;        //!<    graph reduction level

    // stress pressure limits
    float       compressionElasticLimit;    //!<    below this compression pressure no damage is done to bonds.  Also used as the default for shear and tension if they aren't provided.
    float       compressionFatalLimit;      //!<    above this compression pressure the bond is immediately broken.  Also used as the default for shear and tension if they aren't provided.
    float       tensionElasticLimit;        //!<    below this tension pressure no damage is done to bonds.  Use a negative value to fall back on compression limit.
    float       tensionFatalLimit;          //!<    above this tension pressure the bond is immediately broken.  Use a negative value to fall back on compression limit.
    float       shearElasticLimit;          //!<    below this shear pressure no damage is done to bonds.  Use a negative value to fall back on compression limit.
    float       shearFatalLimit;            //!<    above this shear pressure the bond is immediately broken.  Use a negative value to fall back on compression limit.

    ExtStressSolverSettings() :
        maxSolverIterationsPerFrame(25),
        graphReductionLevel(0),

        // stress pressure limits
        compressionElasticLimit(1.0f),
        compressionFatalLimit(2.0f),
        tensionElasticLimit(-1.0f),
        tensionFatalLimit(-1.0f),
        shearElasticLimit(-1.0f),
        shearFatalLimit(-1.0f)
    {}
};


/**
Parameter to addForce() calls, determines the exact operation that is carried out.

@see ExtStressSolver.addForce()
*/
struct ExtForceMode
{
    enum Enum
    {
        FORCE,          //!< parameter has unit of mass * distance / time^2
        ACCELERATION,   //!< parameter has unit of distance / time^2, i.e. the effect is mass independent
    };
};


/**
Stress Solver.

Uses NvBlastFamily, allocates and prepares its graph once when it's created. Then it's being quickly updated on every
actor split.
It uses NvBlastAsset support graph, you can apply forces on nodes and stress on bonds will be calculated as the result.
When stress on bond exceeds it's health bond is considered broken (overstressed).
Basic usage:
1. Create it with create function once for family
2. Fill node info for every node in support graph or use setAllNodesInfoFromLL() function.
3. Use notifyActorCreated / notifyActorDestroyed whenever actors are created and destroyed in family.
4. Every frame: Apply forces (there are different functions for it see @addForce)
5. Every frame: Call update() for actual solver to process.
6. If getOverstressedBondCount() > 0 use generateFractureCommands() functions to get FractureCommands with bonds fractured
*/
class NV_DLL_EXPORT ExtStressSolver
{
public:
    //////// creation ////////

    /**
    Create a new ExtStressSolver.

    \param[in]  family          The NvBlastFamily instance to calculate stress on.
    \param[in]  settings        The settings to be set on ExtStressSolver.

    \return the new ExtStressSolver if successful, NULL otherwise.
    */
    static ExtStressSolver*                 create(const NvBlastFamily& family, const ExtStressSolverSettings& settings = ExtStressSolverSettings());


    //////// interface ////////

    /**
    Release this stress solver.
    */
    virtual void                            release() = 0;

    /**
    Set node info.

    All the required info per node for stress solver is set with this function. Call it for every node in graph or use setAllNodesInfoFromLL().

    \param[in]  graphNodeIndex  Index of the node in support graph. see NvBlastSupportGraph.
    \param[in]  mass            Node mass. For static node it is must be zero.
    \param[in]  volume          Node volume. For static node it is irrelevant.
    \param[in]  localPosition   Node local position.
    */
    virtual void                            setNodeInfo(uint32_t graphNodeIndex, float mass, float volume, NvcVec3 localPosition) = 0;

    /**
    Set all nodes info using low level NvBlastAsset data.
    Uses NvBlastChunk's centroid and volume.
    Uses 'external' node to mark nodes as static.

    \param[in]  density         Density. Used to convert volume to mass.
    */
    virtual void                            setAllNodesInfoFromLL(float density = 1.0f) = 0;

    /**
    Set stress solver settings.
    Changing graph reduction level will lead to graph being rebuilt (which is fast, but still not recommended).
    All other settings are applied instantly and can be changed every frame.

    \param[in]  settings        The settings to be set on ExtStressSolver.
    */
    virtual void                            setSettings(const ExtStressSolverSettings& settings) = 0;

    /**
    Get stress solver settings.

    \return the pointer to stress solver settings currently set.
    */
    virtual const ExtStressSolverSettings&  getSettings() const = 0;

    /**
    Notify stress solver on newly created actor.

    Call this function for all initial actors present in family and later upon every actor split.

    \param[in]  actor           The actor created.

    \return true if actor will take part in stress solver process.  false if actor doesn't contain any bonds.
    */
    virtual bool                            notifyActorCreated(const NvBlastActor& actor) = 0;

    /**
    Notify stress solver on destroyed actor.

    Call this function when actor is destroyed (split futher) or deactivated.

    \param[in]  actor           The actor destroyed.
    */
    virtual void                            notifyActorDestroyed(const NvBlastActor& actor) = 0;

    /**
    Apply external impulse on particular actor of family. This function will find nearest actor's graph node to apply impulse on.

    \param[in]  actor           The actor to apply impulse on.
    \param[in]  localPosition   Local position in actor's coordinates to apply impulse on.
    \param[in]  localForce      Force to apply in local actor's coordinates.
    \param[in]  mode            The mode to use when applying the force/impulse(see #ExtForceMode)

    \return true iff node was found and force applied.
    */
    virtual bool                            addForce(const NvBlastActor& actor, NvcVec3 localPosition, NvcVec3 localForce, ExtForceMode::Enum mode = ExtForceMode::FORCE) = 0;

    /**
    Apply external impulse on particular node.

    \param[in]  graphNodeIndex  The graph node index to apply impulse on. See #NvBlastSupportGraph.
    \param[in]  localForce      Force to apply in local actor's coordinates.
    \param[in]  mode            The mode to use when applying the force/impulse(see #ExtForceMode)
    */
    virtual void                            addForce(uint32_t graphNodeIndex, NvcVec3 localForce, ExtForceMode::Enum mode = ExtForceMode::FORCE) = 0;

    /**
    Apply external gravity on particular actor of family. This function applies gravity on every node withing actor, so it makes sense only for static actors.

    \param[in]  actor           The actor to apply gravitational acceleration on.
    \param[in]  localGravity    Gravity to apply in local actor's coordinates. ExtForceMode::ACCELERATION is used.

    \return true iff acceleration was applied on at least one node.
    */
    virtual bool                            addGravity(const NvBlastActor& actor, NvcVec3 localGravity) = 0;

    /**
    Apply centrifugal acceleration produced by actor's angular movement.

    \param[in]  actor                   The actor to apply impulse on.
    \param[in]  localCenterMass         Actor's local center of mass.
    \param[in]  localAngularVelocity    Local angular velocity of an actor.

    \return true iff force was applied on at least one node.
    */
    virtual bool                            addCentrifugalAcceleration(const NvBlastActor& actor, NvcVec3 localCenterMass, NvcVec3 localAngularVelocity) = 0;

    /**
    Update stress solver.

    Actual performance of stress calculation happens there. Call it after all relevant forces were applied, usually every frame.
    */
    virtual void                            update() = 0;

    /**
    Get overstressed/broken bonds count.

    This count is updated after every update() call. Number of overstressed bond directly hints if any bond fracture is recommended by stress solver.

    \return the overstressed bonds count.
    */
    virtual uint32_t                        getOverstressedBondCount() const = 0;

    /**
    Generate fracture commands for particular actor.

    Calling this function if getOverstressedBondCount() == 0 or actor has no bond doesn't make sense, bondFractureCount will be '0'.
    Filled fracture commands buffer can be passed directly to NvBlastActorApplyFracture.

    IMPORTANT: NvBlastFractureBuffers::bondFractures will point to internal stress solver memory which will be valid till next call
    of any of generateFractureCommands() functions or stress solver release() call.

    \param[in]  actor                   The actor to fill fracture commands for.
    \param[in]  commands                Pointer to command buffer to fill.
    */
    virtual void                            generateFractureCommands(const NvBlastActor& actor, NvBlastFractureBuffers& commands) = 0;

    /**
    Generate fracture commands for every actor in family.

    Actors and commands buffer must be passed in order to be filled. It's recommended for bufferSize to be the count of actor with more then one bond in family.

    Calling this function if getOverstressedBondCount() == 0 or actor has no bond doesn't make sense, '0' will be returned.

    IMPORTANT: NvBlastFractureBuffers::bondFractures will point to internal stress solver memory which will be valid till next call
    of any of generateFractureCommands() functions or stress solver release() call.

    \param[out] buffer          A user-supplied array of NvBlastActor pointers to fill.
    \param[out] commandsBuffer  A user-supplied array of NvBlastFractureBuffers to fill.
    \param[in]  bufferSize      The number of elements available to write into buffer.

    \return the number of actors and command buffers written to the buffer.
    */
    virtual uint32_t                        generateFractureCommandsPerActor(const NvBlastActor** actorBuffer, NvBlastFractureBuffers* commandsBuffer, uint32_t bufferSize) = 0;

    /**
    Reset stress solver.

    Stress solver uses warm start internally, calling this function will flush all previous data calculated and also zeros frame count.
    This function is to be used for debug purposes.
    */
    virtual void                            reset() = 0;

    /**
    Get stress solver linear error.

    \return the total linear error of stress calculation.
    */
    virtual float                           getStressErrorLinear() const = 0;

    /**
    Get stress solver angular error.

    \return the total angular error of stress calculation.
    */
    virtual float                           getStressErrorAngular() const = 0;

    /**
    Whether or not the solver converged to a solution within the desired error.

    \return true iff the solver converged.
    */
    virtual bool                            converged() const = 0;

    /**
    Get stress solver total frames count (update() calls) since it was created (or reset).

    \return the frames count.
    */
    virtual uint32_t                        getFrameCount() const = 0;

    /**
    Get stress solver bonds count, after graph reduction was applied.

    \return the bonds count.
    */
    virtual uint32_t                        getBondCount() const = 0;

    /**
    Get stress solver excess force related to broken bonds for the given actor.
    This is intended to be called after damage is applied to bonds and actors are split, but before the next call to 'update()'.
    Force is intended to be applied to the center of mass, torque due to linear forces that happen away from the COM are converted
    to torque as part of this function.

    \return true if data was gathered, false otherwise.
    */
    virtual bool                            getExcessForces(uint32_t actorIndex, const NvcVec3& com, NvcVec3& force, NvcVec3& torque) = 0;


    /**
    Debug Render Mode
    */
    enum DebugRenderMode
    {
        STRESS_PCT_MAX = 0,         //!<    render the maximum of the compression, tension, and shear stress percentages
        STRESS_PCT_COMPRESSION = 1, //!<    render the compression stress percentage
        STRESS_PCT_TENSION = 2,     //!<    render the tension stress percentage
        STRESS_PCT_SHEAR = 3,       //!<    render the shear stress percentage
    };

    /**
    Used to store a single line and colour for debug rendering.
    */
    struct DebugLine
    {
        DebugLine(const NvcVec3& p0, const NvcVec3& p1, const uint32_t& c)
            : pos0(p0), color0(c), pos1(p1), color1(c) {}

        NvcVec3 pos0;
        uint32_t        color0;
        NvcVec3 pos1;
        uint32_t        color1;
    };

    /**
    Debug Buffer
    */
    struct DebugBuffer
    {
        const DebugLine* lines;
        uint32_t         lineCount;
    };

    /**
    Fill debug render for passed array of support graph nodes.

    NOTE: Returned DebugBuffer points into internal memory which is valid till next fillDebugRender() call.

    \param[in]  nodes           Node indices of support graph to debug render for.
    \param[in]  nodeCount       Node indices count.
    \param[in]  mode            Debug render mode.
    \param[in]  scale           Scale to be applied on impulses.

    \return debug buffer with array of lines
    */
    virtual const DebugBuffer               fillDebugRender(const uint32_t* nodes, uint32_t nodeCount, DebugRenderMode mode, float scale = 1.0f) = 0;
};

} // namespace Blast
} // namespace Nv


#endif // ifndef NVBLASTEXTSTRESSSOLVER_H
