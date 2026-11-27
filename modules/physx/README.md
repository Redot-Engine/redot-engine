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

## Diagnosing runtime problems

A PhysX or Blast runtime library with the right **name** but the wrong
**content** — a leftover from another project, an SDK reinstall, an antivirus
quarantine restore — fails in confusing ways. The classic case on Linux: an
older `libNvBlastExtShaders.so` on `LD_LIBRARY_PATH` or in a system directory
is searched **before** the binary's own folder, shadows the correct copy, and
the editor dies at startup with

```
symbol lookup error: ./redot.linuxbsd.editor.x86_64.mono: undefined symbol: NvBlastExtFalloffSubgraphShader
```

even though the correct library sits right next to it.

The editor defends on its own: every runtime check logs which file each
loaded PhysX/Blast library resolved from and verifies it against the
manifest's hashes (`PhysX: runtime library ... loaded from ...`), a mismatch
is reported as a warning naming the offending path, and a file beside the
editor binary that no longer matches the installed file list is offered a
hash-verified reinstall (`Download and Restart`) that replaces only the
mismatching files.

To diagnose by hand on Linux:

```bash
# Which copy of each library the loader actually picked up. Search order is
# LD_LIBRARY_PATH first, then the binary's own folder ($ORIGIN), then the
# system paths -- so a stale copy in any of the other locations wins over
# the one next to the binary:
ldd ./redot.linuxbsd.editor.x86_64.mono | grep -iE 'blast|physx|nvflow'

# The loader's full decision trace, including every candidate it rejected:
LD_DEBUG=libs ./redot.linuxbsd.editor.x86_64.mono 2>&1 | grep -iE 'blast|physx|nvflow'
```

Fix: remove the stale copies the trace points at (or unset
`LD_LIBRARY_PATH` for the editor), and if the file next to the binary is
itself the wrong one, reinstall it — either through the editor's offer or by
hand from the [redot-physx-runtime Releases](https://github.com/Vlado-VG/redot-physx-runtime/releases)
bundle, which verifies every file's SHA-256 against the manifest on the way
in. The installed bundle's expected hashes are recorded next to the libraries
(`.physx-runtime-files`), and `PhysXRuntimeManager.get_stale_runtime_files()`
reports any required file that no longer matches them.

## Credits
**Credits** to [**Uno "Wild-ox"**](https://github.com/uno1982) for:
MPM shaders, a lot of fixes and implementations for water, 2W/4W vehicles, tank, boat, foam, CompositorEffects and more features. Visit his Godot upstream (4.7+) PhysX module at:
https://github.com/uno1982/godot/tree/feature/physx5-module
Wild-ox Studios website:
https://wildoxstudios.com
