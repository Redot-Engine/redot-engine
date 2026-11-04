# PhysX module

Native NVIDIA PhysX 5 implementation of Redot's 3D physics backend
(`Physics > 3D > Physics Engine > PhysX`), with Blast destruction, NVIDIA Flow
gas/fluids, GPU particle fluids, cloth, soft bodies, vehicles and water. Class
reference: the `PhysX*` classes in the editor help (doc_classes).

## Simulation settings note

`physics/physx_3d/simulation/max_depenetration_velocity` defaults to **0
(off) for godot_physics parity**: Godot ejects embedded bodies positionally
with no cap, at the same recovery rate as PhysX's own uncapped default
(measured side by side: per-step ejection 1.19 vs 1.19 m/step on a half-embedded
body, 0.68 vs 0.67 m/step on a deeply embedded one). Set a value > 0 (for
example 4) as a comfort option for a gentler, slower pop-out — a 4 m/s cap
cleared a deep embed about 7x slower than Godot in the same measurement.

Setting `physics/physx_3d/simulation/gpu_dynamics` (default on) to off
disables GPU dynamics **without** enhanced determinism: no CUDA context is
created at all and every scene runs the CPU solver with its full worker pool
-- the same path as a machine without CUDA. Useful when the GPU is shared
with other work, since every step waits on it.
`physics/physx_3d/simulation/enhanced_determinism` instead keeps the CUDA
context (soft-body CUDA paths stay available) and only forces the CPU
simulation.

## Engine parity notes

A few `PhysicsServer3D` observables have no PhysX counterpart and read as
documented constants or round-trip caches:

- `PhysicsServer3D.get_process_info(INFO_ISLAND_COUNT)` reports **0** — PhysX
  exposes no island count (`PxSimulationStatistics` carries only pair, body,
  shape and constraint counters).
- `SPACE_PARAM_CONTACT_RECYCLE_RADIUS`, `SPACE_PARAM_CONTACT_DEFAULT_BIAS` and
  `SPACE_PARAM_CONTACT_MAX_SEPARATION` are cached for `space_get_param`
  round-trips only (PhysX's contact-offset machinery supersedes them; the
  module warns once when they are set). `SPACE_PARAM_CONTACT_MAX_ALLOWED_PENETRATION`
  maps onto shape rest offsets, applied only when set explicitly.
- `physics/physx_3d/simulation/async_step` (default off) overlaps the solve
  with the rest of the frame. Frames that mutate the scene (actor add/remove,
  shape/transform writes, joint changes, scene queries) fetch the in-flight
  solve first and lose the overlap for that frame. With
  `physics/3d/run_on_separate_thread`, a module-API mutation arriving from a
  non-simulating thread proceeds against the last fetched state and warns
  once — keep async stepping off for projects that mutate module objects
  (vehicles, GPU fluids/cloths, articulations) from scripts.

## Exporting a game

The downloadable export templates do not contain this module, so export with a
template you build yourself, **with the same options as the editor** — use
`physx_config=release` for release templates (the editor pairs with the
"checked" SDK libs by default):

```
scons platform=windows target=template_release physx_config=release
scons platform=linuxbsd target=template_release physx_config=release
```

The built template binaries land in `bin/` (`redot.windows.template_release.x86_64.exe`,
`redot.linuxbsd.template_release.x86_64`). In the export preset, set
**Custom Template → Release / Debug** to those binaries.

The editor then ships the runtime libraries with the game by itself: when
exporting for Windows or Linux, the PhysX export plugin copies them from the
**folder of the export template** the preset uses (the custom template's, or
else the editor's templates folder for this version) next to the exported
executable, along with `PHYSX-LICENSE.txt` (the PhysX SDK's BSD-3-Clause
notice, which the binaries must carry). Because they come from the template's
folder, a Windows editor exporting with a Linux template folder ships the
Linux `.so` files.

| Library | Without it |
| --- | --- |
| `NvBlast.dll`, `NvBlastGlobals.dll`, `NvBlastExt*.dll`, `NvBlastTk.dll` (`libNvBlast*.so`) | Blast destruction (PhysXDestructible3D) cannot work; the game may fail to start |
| `PhysXGpu_64.dll` (`libPhysXGpu_64.so`, ~350 MB) | PhysX runs on the CPU, without GPU dynamics |
| `nvflow.dll`, `nvflowext.dll` (`libnvflow*.so`) | the NVIDIA Flow nodes stay inert |

A library missing from the template folder becomes an export warning naming
the files, what the game loses and where to copy them from. Manual install
works the same way: copy the library set from your editor's folder (or the
redot-physx-runtime bundle) next to the exported executable.

On Linux the template binary finds the `.so` files next to itself (the build
sets an `$ORIGIN` rpath; the Flow loader opens them by absolute path). Other
platforms (Android, macOS, web) are not supported by the module yet.

## Credits
**Credits** to [**Uno "Wild-ox"**](https://github.com/uno1982) for:
MPM shaders, a lot of fixes and implementations for water, 2W/4W vehicles, tank, boat, foam, compositor
and more. Visit his Godot upstream (4.7+) PhysX module at:
https://github.com/uno1982/godot/tree/feature/physx5-module
Wild-ox Studios website:
https://wildoxstudios.com

### Port lineage

The features credited above were ported from these upstream commits
([uno1982/godot, `feature/physx5-module`](https://github.com/uno1982/godot/tree/feature/physx5-module)):

| Feature | Upstream commit |
| --- | --- |
| MPM compute-fluid solver + shaders | [2913238b1](https://github.com/uno1982/godot/commit/2913238b165fccf6f26e05c17b5fce6a49670253) |
| FFT/ripple water solver (Phase 1) | [8c48dd9a5](https://github.com/uno1982/godot/commit/8c48dd9a57d4764ec6a44e01567ad3bad2f89a93) |
| Buoyancy + Boat | [8a4b2c9ff](https://github.com/uno1982/godot/commit/8a4b2c9ff4564bef36c1a0ece160a21974e18e65) |
| Water spray | [93841cb17](https://github.com/uno1982/godot/commit/93841cb17b9b47c3745ceb4e12fc0a5ecfed63a9) |
| PxVehicle2 4-wheel node | [7bf6b705d](https://github.com/uno1982/godot/commit/7bf6b705d3e9a2606ba5591b760b6f162fd2c6b5) |
| 2-wheel motorcycle node | [eef1d6114](https://github.com/uno1982/godot/commit/eef1d61141d20149aae4491c969dc6702190e42d) |
| Skid-steer tank | [d53ca4375](https://github.com/uno1982/godot/commit/d53ca4375c7e3d0160d0dee0d856afa5cf20cd7a) |
| Screen-space fluid surface | [00416829bf](https://github.com/uno1982/godot/commit/00416829bff04a13d73ce8f1afd277c48716cf77) |
| Screen-space foam + shared compositor install | [5badd1389](https://github.com/uno1982/godot/commit/5badd13891ef3646fa72882e5053736c80f0974d) |
