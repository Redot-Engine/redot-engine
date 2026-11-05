#!/usr/bin/env python3
"""Collect a built PhysX SDK into the vendor layout.

Scans the PhysX source tree for the built libraries, groups them by SDK
config (checked / release) and library name, collapses multi-arch duplicates
into a universal binary on macOS (lipo), and lays everything out under
out/<config>/ -- ready to drop into thirdparty/physx/lib/<platform>/...

Also collected:
  - libPVDRuntime_static_64.a  (linked by the module's Linux SCsub)
  - shared GPU runtimes (libPhysXGpu_64.so, libPhysXArticulationGpu_64.so):
    RUNPATH is set to $ORIGIN (the module stages them beside the engine
    binary and preloads the CUDA driver before load)
  - include/ -> out/include (the SDK headers, for thirdparty/physx/include)

Deliberately NOT collected:
  - the GPU solver static archives (libPhysX*Gpu_static_64.a,
    libPhysXCudaContextManager_static_64.a): the Linux engine never links
    them -- it resolves its GPU dispatch entry points from the shared
    libPhysXGpu_64.so, so collecting them only adds ~700 MB of dead weight
  - a stale libPhysXCooking_static_64.a: PhysX 5.12 removed the cooking
    library and exports its functions from PhysXCommon

Usage: python3 .github/workflows/scripts/collect_physx_libs.py <linux|macos>
"""

import glob
import os
import shutil
import subprocess
import sys

EXPECTED = {
    "PhysXVehicle_static_64",
    "PhysXCharacterKinematic_static_64",
    # PhysX 5.12 removed the PhysXCooking library; its functions are exported
    # from PhysXCommon now, which the module already links.
    "PhysXExtensions_static_64",
    "PhysXPvdSDK_static_64",
    "PhysX_static_64",
    "PhysXCommon_static_64",
    "PhysXFoundation_static_64",
    "PVDRuntime_static_64",
}

# The GPU solver components are built as intermediate static archives on the
# way to the shared PhysXGpu runtime, but the module never links them: the
# Linux link resolves its GPU dispatch entry points from the shared
# libPhysXGpu_64.so. Collecting them added ~700 MB of dead weight per
# artifact, so they are deliberately left out of the vendor layout. The
# removed PhysXCooking archive is skipped too (5.11 leftovers in a build
# tree): 5.12 exports its functions from PhysXCommon.
GPU_STATIC_SKIP = {
    "libPhysXArticulationGpu_static_64.a",
    "libPhysXBroadphaseGpu_static_64.a",
    "libPhysXCommonGpu_static_64.a",
    "libPhysXNarrowphaseGpu_static_64.a",
    "libPhysXSimulationControllerGpu_static_64.a",
    "libPhysXSolverGpu_static_64.a",
    "libPhysXCudaContextManager_static_64.a",
    "libPhysXCooking_static_64.a",
}


def config_of(path):
    parts = path.replace("\\", "/").split("/")
    for seg in parts:
        if seg in ("checked", "release", "profile", "debug"):
            return seg
    return "unknown"


def collect(groups, patterns):
    for pattern in patterns:
        for f in glob.glob(pattern, recursive=True):
            f = f.replace("\\", "/")
            # Skip intermediates and the checked-out engine repo (it vendors
            # the PREVIOUS SDK version's libs under the same names -- "engine"
            # sorts before "physx", so a naive glob could shadow fresh ones).
            if "/obj/" in f or "/engine/" in f or f.startswith("engine/"):
                continue
            name = os.path.basename(f)
            if name in GPU_STATIC_SKIP:
                continue
            groups.setdefault((config_of(f), name), []).append(f)


def main():
    platform = sys.argv[1] if len(sys.argv) > 1 else "linux"
    groups = {}
    collect(groups, ["**/libPhysX*.a", "**/libPVDRuntime_static_64.a"])
    shared = {}
    collect(shared, ["**/libPhysX*.so", "**/libPhysX*.so.*"])

    if not groups:
        sys.exit("ERROR: no libPhysX*.a found -- did the build produce output?")

    for (config, name), paths in sorted(groups.items()):
        dest = os.path.join("out", config)
        os.makedirs(dest, exist_ok=True)
        out = os.path.join(dest, name)
        if len(paths) > 1 and shutil.which("lipo") and platform == "macos":
            subprocess.run(["lipo", "-create", "-output", out] + paths, check=True)
            print(f"universal ({len(paths)} arch): {name}")
        else:
            shutil.copy(paths[0], out)
            print(f"copied: {name}  <-  {paths[0]}")

    has_patchelf = shutil.which("patchelf") is not None
    for (config, name), paths in sorted(shared.items()):
        dest = os.path.join("out", config)
        os.makedirs(dest, exist_ok=True)
        out = os.path.join(dest, name)
        shutil.copy(paths[0], out)
        os.chmod(out, 0o755)
        if has_patchelf:
            # The module stages these beside the engine binary with a bare
            # soname lookup; RUNPATH=$ORIGIN is what makes that resolve.
            subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", out], check=False)
        print(f"shared: {name}  <-  {paths[0]}" + ("" if has_patchelf else "  (patchelf not found; RUNPATH not set)"))

    if os.path.isdir("include"):
        dest = os.path.join("out", "include")
        if os.path.isdir(dest):
            shutil.rmtree(dest)
        shutil.copytree("include", dest)
        print("copied: include/  ->  out/include")

    print("\nExpected-library check per config:")
    for config in sorted(os.listdir("out")):
        if config == "include":
            continue
        have = set(os.listdir(os.path.join("out", config)))
        missing = {"lib" + n + ".a" for n in EXPECTED} - have
        extra = have - {"lib" + n + ".a" for n in EXPECTED}
        print(f"  {config}: {len(have)} files; missing={sorted(missing) or 'none'}; extra={sorted(extra) or 'none'}")

    print(
        "\nDrop-in:\n"
        "  out/checked/*.a -> thirdparty/physx/lib/linux/x86_64/checked/\n"
        "  out/release/*.a -> thirdparty/physx/lib/linux/x86_64/release/\n"
        "  out/<config>/libPhysX*.so -> thirdparty/physx/bin/linux/x86_64/release/\n"
        "  out/include/    -> thirdparty/physx/include/"
    )


if __name__ == "__main__":
    main()
