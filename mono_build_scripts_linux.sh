#!/bin/bash

SOURCE_NAME="MyLocalNugetSource"
SOURCE_PATH="$HOME/MyLocalNugetSource"

if ! command -v dotnet &> /dev/null; then
    echo "Error: dotnet command not found. Please install .NET SDK."
    exit 1
fi

if ! dotnet nuget list source | grep -q "$SOURCE_NAME"; then
    echo "Local nuget store not found, making one in $HOME..."
    mkdir -p "$SOURCE_PATH"
    dotnet nuget add source "$SOURCE_PATH" --name "$SOURCE_NAME"
fi

# Build editor binary
scons compiledb=yes dev_build=yes platform=linuxbsd target=editor module_mono_enabled=yes || {
    echo "Error: scons build failed"
    exit 1
}

# Generate glue sources
./bin/redot.linuxbsd.editor.dev.*.mono --headless --generate-mono-glue modules/mono/glue

#Generate binaries
./modules/mono/build_scripts/build_assemblies.py --godot-output-dir=./bin --godot-platform=linuxbsd --push-nupkgs-local "$HOME/MyLocalNugetSource/"
