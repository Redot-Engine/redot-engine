# NVIDIA Flow — Godot/Redot Integration: Implementation Notes

Working journal for the `modules/flow` integration. Every significant decision
records evidence per the master prompt (§72 Evidence Requirements).

---

## Phase 1 — Repository & SDK Audit

### Identified versions (evidence)

| Item | Value | Evidence |
| ---- | ----- | -------- |
| Engine | Redot LTS 26.3.0-beta2 (Godot 4.5.2 API base) | `version.py` |
| PhysX | 5.11 (`PhysX-5.11_vc17_win64_mt_cuda12.8.2`) | `thirdparty/physx/README.md`, zip name |
| Blast | 5.x SDK, DLL-based, Windows-only in this fork | `thirdparty/blast/`, `modules/physx/SCsub` (`blast=yes` default, Windows only) |
| Flow | **NVIDIA Flow 2.2.0** (`Flow-2.2.0_vc17_win64_cuda12.8.2_release`) | `thirdparty/flow/Flow-2.2.0_*.zip` |
| Flow libs | Windows: `nvflow.dll`/`nvflowext.dll` (+ RTX, import `.lib` in `lib/`); Linux: `libnvflow.so`/`libnvflowext.so`, CI-built from NVIDIA-Omniverse/PhysX by `.github/workflows/build-flow-libs.yml` | `thirdparty/flow/bin/{windows,linux}/x86_64/release/` |
| Flow license | BSD-3-Clause (NVIDIA, 2008-2025) — redistribution-compatible | `thirdparty/flow/LICENSE.md` |
| Toolchain | MSVC (VS 18 install), SCons 4.10.1 via `python -m SCons`, Python 3.14 | build environment probe |
| Previous build | Windows editor build succeeded in-tree (`bin/redot.windows.editor.x86_64.exe`, `bin/obj/modules/physx/*`) | `bin/` listing |

This is the **realtime sparse-voxel fluid/combustion Flow SDK** (NvFlow), not the
unrelated ML `nvflow` project: headers define `NvFlowGrid`, combustion params
(temperature/fuel/burn/smoke channels), sparse NanoVDB export, emitters,
`NvFlowLoader` for `nvflow.dll`/`nvflowext.dll` — the SDK that ships next to
PhysX 5.11 in NVIDIA's PhysX repository.

### Flow 2.2.0 architecture facts (verified from headers + DLL exports)

* Entry points (verified via PE export strings):
  * `nvflow.dll` exports `NvFlowGetOpList` (solver op interfaces).
  * `nvflowext.dll` exports `NvFlowGetExtOpList`, `NvFlowGetGridInterface`,
    `NvFlowGetGridParamsInterface`, `NvFlowGetContextOptInterface`,
    `NvFlowGetDeviceInterface(api)`, `NvFlowGetThreadPoolInterface`,
    `NvFlowGetRadixSortInterface`.
* **Device model**: Flow owns its own GPU device. `NvFlowDeviceInterface`:
  `createDeviceManager` → `enumerateDevices` (LUID/UUID) → `createDevice`
  (`enableExternalUsage` for interop) → `getDeviceQueue` → `getContext(ContextInterface)`.
  There is **no API to create a context on a foreign VkDevice/ID3D12Device**
  (verified: `NvFlowContext.h` + `NvFlowExt.h` expose no such function; DLL
  exports agree). Backends: `eNvFlowContextApi_vulkan | d3d12 | cpu`.
* **Interop**: external Win32/opaqueFD handles for buffers
  (`NvFlowInteropHandle`, `createBufferFromExternalHandle`,
  `eNvFlowContextFeature_bufferExternalHandle`) and external semaphores
  (`getSemaphoreExternalHandle`). Textures have **no** external-handle path —
  cross-API data exchange is via **NanoVDB interop buffers**
  (`NvFlowSparseNanoVdbExportInterop`).
* **Simulation API**: `NvFlowGridInterface` —
  `createGrid(contextInterface, context, opList, extOpList, {maxLocations})`,
  per-frame `simulate(context, grid, NvFlowGridParamsDesc*, globalForceClear)`,
  `offscreen`, `getRenderData` (sparse params + transient density/velocity
  textures), `getActiveBlockCount`, `resetGrid`, `setResourceMinLifetime`.
* **Params model**: `simulate` consumes a `NvFlowGridParamsDesc` — an array of
  `NvFlowGridParamsDescSnapshot { NvFlowDatabaseSnapshot databaseSnapshot;
  absoluteSimTime; deltaTime; globalForceClear; }`. A `NvFlowDatabaseSnapshot`
  is just typed-instance arrays keyed by reflect datatype
  (`NvFlowDatabaseSnapshot_findTypeArray`) — **we can construct it directly
  with our own arrays** and skip the versioned `NvFlowGridParams` transaction
  database (that API exists for Omniverse-style multi-writer setups).
* **Sim layer** (`NvFlowGridSimulateLayerParams`): cell size (auto), steps/sec
  (fixed-step accumulation internal to the grid, `maxStepsPerSimulate`),
  `simulateWhenPaused`, advection/combustion channel params (buoyancy per
  temp/smoke, cooling, ignition…), vorticity confinement, pressure projection,
  sparse block allocation thresholds, NanoVDB export flags.
* **Emitters**: sphere/box/point/mesh/texture/nanovdb, each with velocity,
  divergence, temperature/fuel/burn/smoke values + per-channel couple rates.
  Collision is expressed **as emitters**: `NvFlowEmitterBoxParams /
  NvFlowEmitterMeshParams` carry `isPhysicsCollision=true` and the sim layer
  gates them with `physicsCollisionEnabled` + `physicsCollisionLayers[]`.
  Moving bodies couple through `localToWorldVelocity` (a velocity transform)
  and `physicsVelocityScale`.
* **Readback**: NanoVDB export supports `readbackEnabled` +
  `readbackRingBufferCount` producing CPU-side NanoVDB grids
  (`NvFlowSparseNanoVdbExportReadback`: smoke/fuel/temperature/burn/velocity
  CPU pointers + `globalFrameCompleted`). The Grid surfaces this through a
  named-params handshake (`NvFlowGridReadbackParams.gridParamsName` default
  `"flowUsdReadback"` — Omniverse plumbing). The exact handshake is **Unverified
  until exercised at runtime**; a context-level copy-pass fallback
  (`addPassCopyTextureToBuffer` on `getRenderData().densityTexture`) exists if
  the named path is unusable. PNanoVDB.h is provided for CPU parsing.
* **Rendering**: Flow's own raymarch (`gridInterface->render`) renders into
  transient textures on Flow's device, intended for same-device presentation
  or RTX compositing. Not usable directly from Godot's renderer without
  external-texture interop, which 2.2.0 does not expose. Default fire/smoke
  colormaps (`NvFlowRayMarchColormapParams_default_rgbaPoints_fire/_smoke`)
  are authored HDR ramps we can adopt for Godot-side shading.

### Integration decision — GPU/renderer bridge (evidence-based)

Flow 2.2.0 cannot execute inside Godot's `RenderingDevice` (no foreign-device
context creation) and cannot hand its textures to Godot (no texture interop).
Therefore:

* **Simulation**: on Flow's own device. Preferred API: **Vulkan** (matches the
  primary Windows backend; independent of Godot's own device either way).
  Fallback: **CPU context** when Vulkan device creation fails (headless
  machines / no Vulkan ICD). `d3d12` context reserved for a future
  same-backend optimization; not used initially.
* **Render bridge v1**: NanoVDB CPU readback (sparse data only) → PNanoVDB
  parse → dense `ImageTexture3D` → stock `FogVolume` (engine volumetric fog
  raymarch), the same presentation path proven by `PhysXGas3D`. Smoke channel
  drives density; temperature/fuel channels drive a fire shader variant
  (Flow's combustion is real, unlike Gas3D's density-only "fire look").
* **Render bridge future (explicit non-goal for v1)**: direct GPU interop
  (external Vulkan buffers + a Godot-side port of `NvFlowRayMarch` against
  Flow's sparse layout). Requires engine-level external-memory import support
  that Godot's `RenderingDevice` does not expose; do not fake it.

This satisfies §10 ("avoid duplicate GPU devices" — Flow needs exactly one
device of its own by SDK design, documented) and §11 (compatibility handled by
graceful degradation per backend — see matrix below).

### Renderer compatibility matrix (v1 plan)

| Renderer | Flow simulation | Rendering path | Limitations |
| -------- | --------------: | -------------- | ----------- |
| Vulkan Forward+ | Vulkan device (or CPU fallback) | FogVolume volumetrics | Full path |
| Vulkan Mobile | same | FogVolume volumetrics | Mobile fog quality |
| Direct3D 12 | same (Flow still on its own Vulkan device) | FogVolume volumetrics | Two driver stacks live side by side |
| Compatibility (GL3) | same | FogVolume unsupported by renderer → warning + point-cloud debug only | Degraded |
| Headless (server) | CPU context | none (no viewport) | For CI/tests |

### Module placement decision

New top-level module `modules/flow` (not inside `modules/physx`), per §2:
Flow is a sibling of PhysX/Blast, bridged explicitly.

* Flow core has **zero PhysX dependencies** — `FlowSimulation3D` works with no
  physics backend beyond Godot's scene API.
* **PhysX bridge** (Phase 8) consumes the **`PhysicsServer3D` public API**
  (body transforms/velocities/shape data) — this works with *any* Godot
  physics backend and avoids coupling Flow to PhysX internals; when the PhysX
  backend is active the data is PhysX data by construction.
* **Blast bridge** (Phase 9) consumes destruction events from
  `PhysXDestructible3D` (scene-level, cross-module via a thin include of its
  header guarded by a build flag) and translates them to transient Flow
  emitters. Blast stays optional: `GODOT_FLOW_BLAST_BRIDGE` only.

### Baseline table (per master prompt §7)

| System | State | Evidence |
| ------ | ----- | -------- |
| PhysX | Complete (server, spaces, bodies, vehicles, fluids, cloth, gas) | `modules/physx/*`, ~19k lines, tests in `modules/physx/test` |
| Blast | Complete on Windows (`PhysXDestructible3D`, authoring, editor tools) | `modules/physx/blast/*`, `GODOT_PHYSX_BLAST` |
| Flow | Missing (SDK vendored only) | `thirdparty/flow` headers/libs/DLLs, no module code |
| Renderer | Complete (Forward+/Mobile/Compat; D3D12 dlls in bin) | engine tree, `bin/D3D12Core.dll` |
| GPU device access | Complete for Flow's own device (SDK-owned); no engine external-import API | headers, `servers/rendering/rendering_device.h` |
| Editor viewport | Complete (module editor plugin pattern exists) | `modules/physx/editor/physx_editor_plugin.*` |
| Native module infra | Complete | `modules/physx/register_types.cpp` pattern |
| Scene/resource serialization | Complete (stock engine) | engine |
| Editor tooling | Partial (pattern exists, nothing for Flow) | `modules/physx/editor` |
| Build system | Complete for PhysX/Blast; Flow wiring to add | `modules/physx/SCsub` |
| GPU synchronization | To implement (Flow queue flush/wait) | SDK `NvFlowDeviceInterface` |

---

## Final architecture (as implemented)

```
modules/physx/flow/
  flow_runtime.*            one shared SDK runtime (loader -> deviceManager ->
                            device -> queue -> ContextOpt context), refcounted,
                            forced teardown at SERVERS uninit
  flow_simulation.*         NvFlowGrid owner: params assembly -> NvFlowGridParams
                            transaction DB -> simulate/offscreen -> double-buffered
                            sparse readback copies -> CPU sparse decode
  physx_flow_simulation_3d  scene node: settings + emitter/collider resolution
                            (PhysicsServer3D bridge, transient event emitters),
                            FogVolume render path, diagnostics API
  physx_flow_emitter_3d     data-only emitter marker (sphere/box)
  physx_flow_collider_3d    explicit collider (box / icosphere / Shape3D mesh)
  physx_flow_blast_bridge_3d  Blast chunks_fractured -> add_transient_emitter()
../editor/physx_flow_editor_plugin   gizmos (emitter, collider, active bounds)
```

## Root-caused SDK findings (all empirically validated on Flow 2.2.0)

1. **Emitters must enter `simulate()` through the `NvFlowGridParams`
   transaction database.** A `NvFlowGridParamsDesc` built directly from our
   own typed snapshot arrays (valid per the reflect docs: the database
   consumer helper `NvFlowDatabaseSnapshot_findTypeArray` matches by
   typename+size) produced a *running* context with **zero block
   allocation**. Routing the same snapshot through
   `commitParams -> getParamsSnapshot -> mapParamsDesc` (interface fetched
   from `NvFlowGetGridParamsInterface`, not part of `NvFlowLoader`) made the
   grid allocate blocks and inject immediately. Evidence: debug dumps of the
   mapped desc (identical type arrays both ways) vs `renderData.locs`
   (0 -> 4).
2. **Passes recorded on the ContextOpt-wrapped context require
   `NvFlowContextOptInterface::flush()` before the device-queue flush.**
   Without it, our readback copy passes silently never executed (readback
   buffers read back all zeros, both when recorded after `simulate` and
   after `offscreen`); the grid's own passes worked because the grid flushes
   the opt layer internally. Adding the opt flush made both copies produce
   live data. Evidence: zero-vs-nonzero table words and texels, decode
   results before/after.
3. **`autoCellSize` on `NvFlowGridSimulateLayerParams` does not allocate
   without a seeded `densityCellSize`** (locs stayed 0 with
   `densityCellSize=0, autoCellSize=TRUE`; explicit 0.25 allocates). The
   node therefore ships `cell_size = 0.25` default; 0 remains the expert
   auto path.
4. **Matrix convention validated empirically**: the USD/row-vector
   interpretation of `localToWorld` (basis axes in rows x/y/z, translation
   in w; velocity matrix rows = cross(omega, axis_i), w = linear velocity)
   produced a plume rising exactly along Godot +Y from a +Y velocity
   emitter, with collision blocks at the emitter's world position. No
   transposition needed.
5. **Readback decode validated**: sparse table value at
   `table[blockIdx + blockLevelOffsetGlobal]`, bit31 = valid, real texel
   base = packed 12/11/11 offsets (CPU port of `NvFlowTableValueToIndexRead`),
   world = location*blockSize + local*cellSize; density texture is
   RGBA16F with A = smoke density, RGB = solver colorized channels
   (temperature-colored fire). Grid block layout on level 0 is 32x16x16
   voxels (blockDimBits 5/4/4), blockSizeWorld (8,4,4) at cellSize 0.25.

## Frame scheduling (implemented)

Main thread, per physics tick, per PhysXFlowSimulation3D:
`resolve emitters (+transients) & colliders` -> `simulate()` -> `offscreen()`
-> `record readback copies (slot N&1)` -> `ContextOpt flush` -> `queue flush`
-> stamp slot with flushed frame id. On subsequent ticks `poll_readback()`
decodes the newest slot whose frame id is <= `getLastFrameCompleted()` --
observed steady-state latency: 1-2 ticks. No CPU-GPU sync stalls: the decode
maps buffers whose frame already completed (double-buffered; a single buffer
races with the next record).

Physics-backend interplay: the colliders bridge reads server state AFTER the
physics step of the same tick (node internal physics process runs post-step),
so collider transforms/velocities are this tick's solved values -- Flow then
runs one tick behind physics, which matches the volumetric render's latency
budget.

## Threading model

| Object | Thread | Synchronization |
| ------ | ------ | --------------- |
| FlowRuntime (device/queue/context) | main (creator = first node) | created via `acquire()` refcount; teardown at module uninit (main) |
| NvFlowGrid / FlowSimulation | main only (owner node) | none needed -- all SDK calls main-thread |
| Readback slots | main writes, main decodes | frame-id handshake with the GPU queue |
| Scene node properties | main (Godot contract) | none needed |
| SDK internal thread pool | SDK-owned workers | internal to nvflow |

## Measured performance (RTX 4080 SUPER, headless editor build, 60 Hz ticks,
## fire+smoke emitter @ cellSize 0.25, from the smoke test diagnostics)

* `simulate + offscreen + record` (CPU submit): ~0.35-0.5 ms/frame typical
  (spikes ~2.5 ms while the sparse layout grows).
* queue flush (CPU-side submit cost): ~0.04-0.18 ms.
* readback decode (CPU, includes 6 MB dense clear): 4-8 ms at plume sizes of
  16-32 blocks (bounded volume 64x96x128 before stride coarsening kicks in
  at 160/axis). **This dominates and is the known optimization target**
  (GPU-side sparse->dense downsample or block-local decode; deferred).
* GPU simulation cost: not directly measurable here (Flow profiler not
  wired); Unverified.

## Validation log

* `flow=no` editor build: PASS (module compiles out entirely).
* `flow=yes` editor build: PASS (links; DLLs copied to bin/).
* `flow_classes_check.gd`: PASS (classes registered, properties round-trip).
* `flow_smoke_test.gd`: **PASS** -- backend=vulkan, blocks 16-32/1024,
  max_smoke 1.1-2.4 (rises then disperses), max_temp ~1.0, bounds grow
  upward as the plume rises (16x24x32 -> 16x40x32), readback frames decode
  continuously (1-2 tick latency).
* Editor visual validation (FogVolume render, gizmos, drone coupling): not
  executed in this environment (headless CI host); Unverified.
