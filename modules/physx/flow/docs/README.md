# NVIDIA Flow integration (modules/physx/flow)

NVIDIA Flow 2.2.0 — the realtime sparse-voxel fluid / fire / smoke solver that
ships alongside PhysX 5.11 and Blast in NVIDIA's SDK family — integrated into
the PhysX module as scene-level nodes with a shared GPU runtime.

## Architectural position

Flow is a **sibling of PhysX and Blast**, not a PhysX subsystem (per NVIDIA's
own packaging: one SDK family, three independent engines). Concretely:

* `flow/` has **zero PhysX dependencies**. It loads `nvflow.dll` +
  `nvflowext.dll` at runtime through `NvFlowLoader` (reflection-duplicated
  interface tables; nothing links against `nvflow*.lib`), creates **its own**
  GPU device/queue/context (Vulkan, with a CPU fallback), and knows nothing
  about `PxScene`.
* The PhysX **collision bridge** consumes the public `PhysicsServer3D` API
  (body transforms, velocities, shape data) — so it works with any Godot
  physics backend, and when this module's PhysX backend is active that data
  is PhysX data by construction.
* The Blast bridge is a pure **event adapter**: `PhysXDestructible3D` emits
  `chunks_fractured`, `PhysXFlowBlastBridge3D` forwards it to
  `PhysXFlowSimulation3D.add_transient_emitter()`. Flow never learns what
  Blast is; either system works without the other.

## Files

| File | Role |
| ---- | ---- |
| `flow_runtime.{h,cpp}` | Shared SDK runtime: loader, device manager/device/queue, ContextOpt wrapper, thread pool. One per process, refcounted; `release_for_shutdown()` at module teardown. |
| `flow_simulation.{h,cpp}` | One NvFlowGrid + parameter assembly (emitters/colliders/layer) routed through the SDK's `NvFlowGridParams` transaction database, plus the double-buffered sparse readback + CPU decode pipeline. |
| `physx_flow_simulation_3d.{h,cpp}` | Scene node: settings, emitter/collider resolution (incl. the PhysicsServer3D bridge and transient event emitters), FogVolume rendering, diagnostics. |
| `physx_flow_emitter_3d.{h,cpp}` | Data-only emitter marker (sphere/box; velocity/divergence/temperature/fuel/smoke). |
| `physx_flow_collider_3d.{h,cpp}` | Explicit collider (analytic box / icosphere / Shape3D-derived mesh) with transform-differenced velocity coupling. |
| `physx_flow_blast_bridge_3d.{h,cpp}` | Blast fracture → Flow puff adapter (compiled only with `GODOT_PHYSX_BLAST`). |
| `../editor/physx_flow_editor_plugin.{h,cpp}` | Viewport gizmos (emitter shape+velocity, collider footprint, active-volume bounds). |

## Build

* `scons ... flow=no` compiles the module exactly as before (all Flow code is
  `GODOT_PHYSX_FLOW`-guarded). Default `flow=yes`.
* Runtime needs `nvflow.dll` + `nvflowext.dll` (Windows) or `libnvflow.so` +
  `libnvflowext.so` (Linux) next to the engine binary — SCons copies them
  from `thirdparty/flow` automatically, and the runtime dlopens them by
  absolute path. A missing library at runtime is **not fatal**: nodes report
  unavailable (see `is_flow_available()` / `get_diagnostics()`), scenes run
  normally.
* Platforms: Windows x64 (verified); Linux x86_64 wired — CI builds the
  `.so` pair into `thirdparty/flow/bin/linux` (workflow
  `build-flow-libs.yml`), pending on-metal verification. macOS would need
  the loader given `.dylib` builds — the code is portable C++.
* Settings: `physics/physx_3d/flow/device_api` (Auto/Vulkan/CPU),
  `device_index`, `device_validation`, `verbose_logs`.

## Rendering model

Flow 2.2.0 exposes **no texture interop** (external handles exist for buffers
only), so v1 renders through a CPU readback of the *sparse* volume only:
per frame the grid's density texture + sparse table are copied to readback
buffers on Flow's queue; once the frame id completes, the sparse layout is
decoded on the CPU (the exact mapping the SDK's own raymarch does on the GPU,
`NvFlowTableValueToIndexRead`) into a dense RGBA16F volume, uploaded to an
`ImageTexture3D` and presented through a stock **FogVolume** — the same
presentation path as PhysXGas3D. Smoke channel (A) shades fog; the solver's
colorized fire channels (RGB) drive emission. Cost: one frame of latency and
a CPU decode of 0.5–8 ms depending on plume size (measured on an RTX 4080S;
larger volumes hit the stride-coarsening cap). Direct GPU interop is the
documented next step, blocked on engine external-memory support.

## Frame / thread model

* Everything Flow runs on the **main thread**, inside the node's internal
  physics tick (same convention as PhysXGas3D). Flow's SDK may use its
  internal CPU thread pool for (de)compression/alloc work.
* Per tick: resolve emitters/colliders → `simulate` + `offscreen` (records
  GPU work) → record readback copies (alternating buffer slot) → ContextOpt
  flush + queue flush → stamps the slot with the flushed frame id.
* Next ticks: `poll_readback()` decodes the newest slot whose frame id
  completed (≥1 frame of latency, no stalls). `readback frame N` data shows
  up ~2 ticks after `simulate frame N`.
* Editor vs runtime: the same path; the shared runtime survives Play/Stop
  (grid state persists across scene reloads by design — use
  `reset_simulation()` for a clean slate). Runtime teardown is forced at
  module shutdown with a diagnostic if anything leaked a reference.

## Tests

* `test/project/gdscript/flow_classes_check.gd` — class/property surface.
* `test/project/gdscript/flow_smoke_test.gd` — end-to-end pipeline (loader →
  device → grid → combustion → readback decode → plume rises). **PASS** =
  available, blocks > 0, `max_smoke > 0` after 3 simulated seconds.
* `test/project/flow_demo.tscn` — visual demo: fire + smoke emitters, static
  ground + dynamic ball colliders, and a scripted FlowCollider3D "drone"
  orbiting the plume (moving-collider coupling).

## Known limitations (v1)

* SDK `autoCellSize` (node `cell_size = 0`) fails to allocate in 2.2.0
  without a seeded size — the node ships a real 0.25 m default.
* Convex-hull bodies collide as their bounding box (server stores points,
  not faces); concave/trimesh bodies voxelize exactly. Sphere/capsule/
  cylinder approximate via icosphere/box.
* `world_boundary` and `heightmap` shapes are skipped (no compact Flow
  representation).
* Decode cost grows with the plume's bounding box (stride-coarsening caps
  at 160 voxels/axis); not yet GPU-accelerated.
* The RTX DLL variants (`nvflow_rtx`, `nvflowext_rtx`) are not loaded — they
  serve Flow's own raytraced composite path, unused by the FogVolume bridge.
