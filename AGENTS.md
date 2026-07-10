# AGENTS.md — Redot Engine LTS / Zodot

## Project identity

- **Redot Engine LTS** v26.3.0-alpha (fork of Godot 4.5.2-stable, Sept 2024). MIT license.
- **Zodot** = planned Zig migration of Redot (Phase 0: no Zig code yet). See `docs/` for vision docs; directory map in [docs/WORK_TREE.md](docs/WORK_TREE.md).
- Pure C++20 codebase with SCons build system, ~77K commits.

## Build system

```
# Quick start (with Nix):
nix run .                                          # auto-builds & runs editor
nix run . -- target=editor dev_build=yes -- --path /tmp/project

# Without Nix:
scons platform=linuxbsd target=editor              # editor build
scons platform=linuxbsd target=template_release    # release template
scons platform=linuxbsd target=editor tests=yes    # with unit tests

# Common flags:
dev_mode=yes    # verbose + warnings=extra + werror + tests + strict_checks
production=yes  # LTO + static cpp + no debug symbols
compiledb=yes   # compile_commands.json for LSP
ninja=yes       # faster rebuilds via ninja backend
dev_build=yes   # DEV_ENABLED code, debug opt level
lto=thin        # link-time optimization
precision=double
```

Binary output: `bin/redot.{platform}.{target}[.dev][.double].{arch}`

## Running tests

```bash
# Build with tests flag first time:
scons platform=linuxbsd target=editor tests=yes

# Run all tests:
./bin/redot.linuxbsd.editor.x86_64 --test

# Run specific test suite:
./bin/redot.linuxbsd.editor.x86_64 --test --test-file=test_physics_3d
```

Tests live in `tests/` (core, scene, servers). Test framework is custom C++ (see `tests/test_macros.h`).

## Build prerequisites

- Python >= 3.8, SCons >= 4.0
- GCC >= 12 / Clang >= 16 / MSVC >= 2022 (C++20 with GNU extensions)
- Vulkan SDK 1.3+
- Jolt Physics compiled by default; GodotPhysics3D is the default engine (`disable_physics_3d=yes` omits all 3D physics)
- No git submodules; all deps vendored in `thirdparty/`
- Nix flake available for reproducible dev env (`use flake` in `.envrc`)

## Linting & formatting

```bash
# Pre-commit (runs automatically on CI):
pre-commit run --all-files

# Check without pre-commit:
ruff check .              # Python
codespell                 # spelling
```

- `.clang-format`: LLVM style with offset -4 for access modifiers
- `.clang-tidy`: modernize/readability checks only
- `pyproject.toml`: ruff + mypy + codespell config for Python
- `custom_dict.txt`: project-specific codespell dictionary
- Python must support 3.8+ (ruff target-version = py38)

## Key environment files

| File | Purpose |
|---|---|
| `.envrc` | `use flake` (direnv + Nix) |
| `flake.nix` | Nix dev shell with all deps |
| `version.py` | `short_name = "redot"`, `major = 26`, etc. |
| `SConstruct` | Root build script (1159 lines) |
| `.gitignore` | Comprehensive (394 lines) |

## Codebase architecture

Full directory tree with per-folder descriptions:
**[docs/WORK_TREE.md](docs/WORK_TREE.md)**

Quick reference:

| Directory | Contents |
|---|---|
| `core/` | Math, Variant, IO, OS, String, containers (no STL — custom `Vector`, `Map`, `Set`, `List`) |
| `scene/` | Node types, scene tree, GUI, 2D/3D/Audio/Animation nodes |
| `servers/` | Rendering (Vulkan/GLES3), Physics (Jolt/Godot3D), Audio, Navigation, XR |
| `editor/` | Docks, inspector, canvas, debugger, asset library |
| `modules/` | 61 optional modules (Jolt physics, GDScript, Mono/C#, text servers, etc.) |
| `drivers/` | Platform-specific rendering backends |
| `platform/` | LinuxBSD, Windows, macOS, Android, iOS, Web, VisionOS |
| `thirdparty/` | All deps vendored — no submodules |
| `tests/` | C++ unit tests (core, scene, servers) |
| `doc/` | Class reference XML (911 files), doc tools, translations |
| `docs/` | Zodot planning docs + [WORK_TREE.md](docs/WORK_TREE.md) |

## Important C++ conventions

- **No exceptions** (`-fno-exceptions`). Saves ~20% binary size.
- **No STL** in engine code. Uses custom `Vector<T>`, `List<T>`, `Map<K,V>`, `Set<T>`, `String`.
- **Reference counting** via `Ref<T>` / `RefCounted` (intrusive ptr).
- **Variant** system for dynamic typing (boxed values).
- `GDExtension` API for C++ plugins (see `gdextension_interface.h`).
- All `SCsub` files define sub-project builds. Each module has `config.py`.
- Module registration: `modules/modules_builders.py` + `register_module_types.h`.

## CI & releasing

- `static_checks.yml`: pre-commit style checks, xmllint doc schema validation, C compile of gdextension_interface.h
- Builds: linux/windows/macos/ios/android/web, each with editor + template variants
- Mono/C# builds use .NET 8.0
- ASAN/LSAN/TSAN/UBSAN suppression files in `misc/error_suppressions/`
- Artifacts stored after each build run
- CodeRabbit: automated PR review (`.coderabbit.yaml`, profile: chill)

## Contributing conventions

- AI contributions allowed per `AI_POLICY.md` (contributor must understand and own all submitted code)
- Pre-commit required for PRs (`CONTRIBUTING.md`)
- Commit messages: follow [docs/GIT_WORKFLOW.md](docs/GIT_WORKFLOW.md) (Conventional Commits, e.g. `feat(ecs): add query API`)
- Use `git pull --rebase`, avoid merge commits
- Class reference XML must be updated when adding exposed methods/properties/signals

## MCP (AI integration)

- `redot-mcp.sh` wrapper runs editor in headless MCP server mode via Nix dev shell
- Launch: `./redot-mcp.sh /path/to/project`
- Details in `doc/mcp-integration.md`
