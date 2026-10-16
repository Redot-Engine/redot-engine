#!/usr/bin/env python3
"""Collect PhysX static libs built by build.sh into the vendor layout.

Scans the PhysX source tree for libPhysX*.a, groups them by SDK config
(checked / release) and library name, collapses multi-arch duplicates
into a universal binary on macOS (lipo), and lays everything out under
out/<config>/ -- ready to drop into thirdparty/physx/lib/<platform>/...

Usage: python3 collect_physx_libs.py <linux|macos>
"""

import glob
import os
import shutil
import subprocess
import sys

EXPECTED = {
    "PhysXVehicle_static_64",
    "PhysXCharacterKinematic_static_64",
    "PhysXCooking_static_64",
    "PhysXExtensions_static_64",
    "PhysXPvdSDK_static_64",
    "PhysX_static_64",
    "PhysXCommon_static_64",
    "PhysXFoundation_static_64",
}


def config_of(path):
    parts = path.replace("\\", "/").split("/")
    for seg in parts:
        if seg in ("checked", "release", "profile", "debug"):
            return seg
    return "unknown"


def main():
    platform = sys.argv[1] if len(sys.argv) > 1 else "linux"
    groups = {}
    for f in glob.glob("**/libPhysX*.a", recursive=True):
        if "/obj/" in f.replace("\\", "/"):
            continue  # intermediate archives
        groups.setdefault((config_of(f), os.path.basename(f)), []).append(f)

    if not groups:
        sys.exit("ERROR: no libPhysX*.a found -- did build.sh produce output?")

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

    print("\nExpected-library check per config:")
    for config in sorted(os.listdir("out")):
        have = set(os.listdir(os.path.join("out", config)))
        missing = {n + ".a" for n in EXPECTED} - have
        extra = have - {n + ".a" for n in EXPECTED}
        print(f"  {config}: {len(have)} files; missing={sorted(missing) or 'none'}; extra={sorted(extra) or 'none'}")


if __name__ == "__main__":
    main()
