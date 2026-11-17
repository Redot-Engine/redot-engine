#!/usr/bin/env python
# -*- coding: utf-8 -*-

import glob
import os
import subprocess
import sys

if len(sys.argv) < 2:
    print("Invalid usage of dotnet_format.py, it should be called with a path to one or multiple files.")
    sys.exit(1)

# Create dummy generated files, if needed.
for path in [
    "modules/mono/SdkPackageVersions.props",
]:
    if os.path.exists(path):
        continue
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("<Project />")

# Avoid importing GeneratedIncludes.props.
os.environ["GodotSkipGenerated"] = "true"

# Projects whose restore needs packages that only build_assemblies.py produces
# locally (a custom SDK built from this engine). modules/mono/nuget is
# gitignored, so the static-checks runner never has them and dotnet format's
# implicit restore fails there ("Restore operation failed") -- even with
# --no-restore, loading the project would still fail at SDK resolution. Their
# C# sources are still compile-checked by the test job that builds them.
NO_RESTORE_PROJECTS = ("modules/physx/test/project",)

# Match all the input files to their respective C# project.
projects = {
    path: [f for f in sys.argv[1:] if os.path.commonpath([f, path]) == path]
    for path in [os.path.dirname(f) for f in glob.glob("**/*.csproj", recursive=True)]
}

# Run dotnet format on all projects with more than 0 modified files.
skip_paths = [os.path.normpath(p) for p in NO_RESTORE_PROJECTS]
for path, files in projects.items():
    if not files:
        continue
    # commonpath does not normalize separators: glob may hand back
    # backslash paths on Windows while the skip list uses forward ones.
    if any(os.path.commonpath([path, skip]) == skip for skip in skip_paths):
        print(f"Skipping {path}: restore needs repo-built SDK packages (see NO_RESTORE_PROJECTS).")
        continue
    result = subprocess.run(["dotnet", "format", path, "--include", *files], check=False)
    if result.returncode:
        sys.exit(result.returncode)
