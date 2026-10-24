# PhysX module

Native NVIDIA PhysX 5 implementation of Redot's 3D physics backend
(`Physics > 3D > Physics Engine > PhysX`), with Blast destruction, NVIDIA Flow
gas/fluids, GPU particle fluids, cloth, soft bodies, vehicles and water. Class
reference: the `PhysX*` classes in the editor help (doc_classes).

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
