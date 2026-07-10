# Onboarding Guide

This guide covers building and running the **current Redot Engine LTS** codebase (pre-Zig migration).

## Prerequisites

### Minimum Requirements

| Requirement | Version / Notes |
|---|---|
| **Python** | >= 3.8 |
| **SCons** | >= 4.0 (`pip install scons`) |
| **C++ compiler** | GCC 12+ / Clang 16+ / MSVC 2022+ (C++20 enabled) |
| **Vulkan SDK** | 1.3+ (Linux: `libvulkan-dev`, macOS: MoltenVK, Windows: LunarG SDK) |
| **Git** | Any modern version |
| **Optional: Nix** | Reproducible environment (see `flake.nix`) |

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install build-essential scons pkg-config libx11-dev \
  libxcursor-dev libxinerama-dev libgl-dev libvulkan-dev \
  libasound2-dev libpulse-dev libudev-dev libdbus-1-dev \
  libfontconfig-dev libfreetype-dev libharfbuzz-dev
```

### macOS

```bash
brew install scons vulkan-headers molten-vk
```

### Windows (MSVC)

1. Install Visual Studio 2022+ with "Desktop development with C++" workload.
2. Install [Vulkan SDK](https://vulkan.lunarg.com/).
3. Install Python + SCons.
4. Open "x64 Native Tools Command Prompt for VS 2022" and proceed.

## Getting the Code

```bash
git clone https://github.com/Redot-Engine/redot-engine.git
cd redot-engine
```

If you are working on the Zodot fork specifically, use the appropriate remote.

## Building

### Quick Build (Linux/macOS)

```bash
scons platform=linuxbsd target=editor
```

### Common Build Variants

```bash
# Editor build (debug, includes editor)
scons platform=linuxbsd target=editor

# Template build (release, no editor)
scons platform=linuxbsd target=template_release

# Debug template (for testing)
scons platform=linuxbsd target=template_debug
```

### Windows

```bash
scons platform=windows target=editor
```

### macOS

```bash
scons platform=macos target=editor
```

### Web

```bash
scons platform=web target=editor
```

## Jolt Physics

Jolt is **compiled into the editor by default** alongside GodotPhysics3D. The
default project setting (`physics/3d/physics_engine = "DEFAULT"`) resolves to
**GodotPhysics3D**, not Jolt. To use Jolt, set **Project Settings > Physics > 3D >
Physics Engine** to `Jolt Physics`.

To disable all 3D physics (both backends):

```bash
scons platform=linuxbsd target=editor disable_physics_3d=yes
```

## Recommended IDE Setup

### VS Code

```json
{
  "recommendations": [
    "llvm-vs-code-extensions.vscode-clangd",
    "ms-python.python",
    "ms-vscode.cpptools-extension-pack"
  ]
}
```

- Use `.clangd` file (in repo root) for clangd LSP configuration.
- The `.clang-format` file enforces the project's C++ formatting.

### CLion

- Open the root directory as a project.
- Configure toolchain to use the system GCC/Clang.
- SCons build commands can be configured as external tools.

### Zig IDE Support (for future work)

When the Zig migration begins, recommended setup:

- **VS Code** with `ziglang.vscode-zig` extension.
- **Zig LSP** (`zls`) configured for build.zig integration.
- **Neovim** with `zig.vim` and `zls`.

## Running

```bash
# After building editor:
./bin/redot.linuxbsd.editor.x86_64

# Or on macOS:
./bin/redot.macos.editor.x86_64

# Or on Windows:
./bin/redot.windows.editor.x86_64.exe
```

## Running Tests

```bash
# Build and run unit tests:
scons platform=linuxbsd target=editor tests=yes
./bin/redot.linuxbsd.editor.x86_64 --test
```

Specific test suites can be filtered:

```bash
./bin/redot.linuxbsd.editor.x86_64 --test --test-file=test_physics_3d
```

## Common Gotchas

| Issue | Solution |
|---|---|
| `scons: command not found` | `pip install scons` or use `python -m SCons` |
| Vulkan SDK not detected | Set `VULKAN_SDK` environment variable to SDK path |
| Missing X11 headers (Linux) | Install `libx11-dev`, `libxcursor-dev`, `libxinerama-dev` |
| LTO link errors | Add `lto=none` to SCons flags |
| Python version too old | Use `pyenv` to install Python 3.10+ |
| Build is very slow | Use `scons -j$(nproc)` to parallelize |
| "jolt_physics" build errors | Check `thirdparty/jolt_physics/` is intact (no submodules needed) |

## How to Contribute or Explore the Codebase

### Navigating the Source

```
core/           — Engine core: Variant, math, IO, OS, containers
scene/          — Node types, scene tree, GUI
servers/        — Rendering, physics, audio, navigation
editor/         — Editor UI and tools
modules/        — Optional features (jolt_physics, gdscript, mono, etc.)
drivers/        — Platform-specific rendering backends
platform/       — OS platform ports
thirdparty/     — Vendored libraries
tests/          — Unit tests
```

### Understanding the Build System

- `SConstruct` — Root build script.
- `methods.py` — Shared build utilities.
- Each `SCsub` file defines a sub-project's build.
- To add a new module, see `modules/` for examples and `modules/modules_builders.py` for registration.

### Contribution Workflow

1. Read `CONTRIBUTING.md` in the repo root.
2. Read [GIT_WORKFLOW.md](./GIT_WORKFLOW.md) for commit conventions and branching model.
3. Fork the repository on GitHub.
4. Create a feature branch (`feature/<short-description>`).
5. Make atomic commits with descriptive messages.
6. Rebase onto master frequently.
7. Submit a pull request.

### Useful Commands

```bash
# Find all current Zig references (none expected):
rg "\.zig" --type file

# List all modules:
ls modules/

# View build options:
scons -h
```

## Zig-Era Development (Future)

Once the build system migration is underway, development will shift:

```bash
# Future build commands (once build.zig replaces SCons):
zig build                    # build editor + Zig code
zig build test               # run all Zig tests
zig build test-tdd           # run behavioral suite (needs display)
```

**Python policy:** Python will be restricted to MCP servers, AI tooling, and supporting scripts. It will **never** be used for engine builds — `build.zig` is the sole build entry point.

**SCons removal:** SCons will be fully removed from the project. No Python-based build scripts remain in the engine build pipeline.

## Discovery Documents

These documents explain the project vision and architecture:

- [WORK_TREE.md](./WORK_TREE.md) — Directory map (dirs only, with descriptions)
- [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md)
- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md)
- [DEPENDENCIES.md](./DEPENDENCIES.md)
- [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md)
- [DECISION_RATIONALE.md](./DECISION_RATIONALE.md)
- [CODING_CONVENTIONS.md](./CODING_CONVENTIONS.md)
- [TESTING_STRATEGY.md](./TESTING_STRATEGY.md)

## Related Documents

- [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md) — Why this project exists
- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — How the engine is structured

## Questions This Document Answers

- How do I build and run the current Redot fork?
- What dependencies do I need to install?
- How do I switch to Jolt Physics (vs the default GodotPhysics3D)?
- Which IDE tools should I use?
- Where is the test executable and how do I run it?
- How do I navigate the existing codebase?
