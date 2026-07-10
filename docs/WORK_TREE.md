# Repository Work Tree

Directory-only map of the Zodot / Redot Engine LTS codebase. Use this to orient
before diving into source files. File-level detail lives in each directory's
`SCsub` and headers.

For Zodot planning docs, see the [docs index](#docs--zodot-planning) below.

---

## Root layout

```
Zodot-engine/
├── core/           Engine foundation — types, memory, IO, math (no scene/editor)
├── scene/          Runtime node types, scene tree, 2D/3D/GUI/audio nodes
├── servers/        Backend servers — rendering, physics, audio, display, XR
├── main/           Application entry point, main loop, performance counters
├── editor/         Editor UI — docks, inspector, import/export, debugger
├── modules/        61 optional compile-time features (GDScript, Jolt, Mono, …)
├── drivers/        Low-level platform drivers (audio, GPU context, input hooks)
├── platform/       OS ports — LinuxBSD, Windows, macOS, Android, iOS, Web, …
├── thirdparty/     ~68 vendored C/C++ libraries (no git submodules)
├── tests/          C++ unit tests (core, scene, servers)
├── doc/            Class reference XML, doc tools, translations
├── docs/           Zodot vision, migration plan, and contributor guides
├── misc/           CI helpers, export templates, API validation, scripts
├── .github/        GitHub Actions workflows and issue templates
├── bin/            Build output (gitignored) — `redot.{platform}.{target}.{arch}`
├── SConstruct      Root SCons build script
├── flake.nix       Nix dev shell and `nix run` entry point
└── version.py      Engine version metadata (`redot`, 26.3.0-alpha)
```

---

## `core/` — engine foundation

No dependency on `scene/` or `editor/`. Custom containers (`Vector`, `Map`, `Set`,
`List`) instead of STL. Reference counting via `Ref<T>` / `RefCounted`.

```
core/
├── config/         ProjectSettings, engine.cfg parsing
├── crypto/         Hashing, encryption helpers
├── debugger/       Remote debugger protocol plumbing
├── error/          Error macros, crash handlers
├── extension/      GDExtension loading and API glue
├── input/          Input event types and mapping
├── io/             File access, JSON, compression, networking primitives
├── math/           Vectors, transforms, AABB, geometry helpers
├── object/         Object, ClassDB, method bindings, signals
├── os/             Threads, time, memory, OS abstraction
├── string/         String, StringName, Unicode helpers
├── templates/      RidOwner, HashMap, SelfList, other internal containers
└── variant/        Dynamic Variant type system
```

---

## `scene/` — runtime scene graph

Everything that runs inside a game project: nodes, resources, and scene-tree
logic. The node-tree model Zodot plans to augment (not immediately replace) with
ECS.

```
scene/
├── 2d/             2D nodes — sprites, tilemaps, physics bodies, cameras
├── 3d/             3D nodes — meshes, lights, skeletons, physics bodies
├── animation/      AnimationPlayer, AnimationTree, tweening
├── audio/          Audio stream players and buses (node side)
├── debugger/       Scene debugger hooks
├── gui/            Control nodes — buttons, labels, containers, themes
├── main/           SceneTree, Viewport, Window, node lifecycle
├── resources/      Resource types — meshes, materials, shaders, packed scenes
└── theme/          Theme resources and default styling
```

---

## `servers/` — backend services

Singleton servers accessed by nodes and the editor. Rendering and physics hot
paths live here.

```
servers/
├── audio/          AudioServer implementation and drivers interface
├── camera/         CameraServer (camera texture feeds)
├── debugger/       Server-side debugger support
├── display/        DisplayServer — windows, input, screens, clipboard
├── extensions/     RenderingServer / PhysicsServer extension points
├── movie_writer/   Movie capture / frame writing
├── navigation/     NavigationServer 2D/3D (Recast/Detour integration)
├── rendering/      Vulkan/GLES3/Metal render pipeline (~123K lines)
├── text/           TextServer implementations (shaping, fonts, BiDi)
└── xr/             XRServer — OpenXR and VR/AR interfaces
```

Top-level `physics_server_*.cpp` and `rendering_server.cpp` define the abstract
server APIs; concrete backends are in `modules/` (Jolt, GodotPhysics, etc.).

---

## `main/` — application bootstrap

```
main/
└── (entry)         `main.cpp` — OS init, server registration, editor or game loop
```

---

## `editor/` — tooling UI

C++ editor retained through Zodot Phase 1+. Not targeted for Zig rewrite.

```
editor/
├── animation/      Animation editor panels
├── asset_library/  AssetLib browser
├── audio/          Audio bus and import UI
├── debugger/       Debugger panels, profiler, remote debug
├── doc/            In-editor help browser
├── docks/          Side docks (FileSystem, Scene, Import, …)
├── export/         Export presets and platform export logic
├── file_system/    FileSystem dock backend
├── gui/            Shared editor widgets and controls
├── icons/          Editor icon SVG sources
├── import/         Asset import plugins (glTF, textures, audio, …)
├── inspector/      Property inspector and sub-inspectors
├── plugins/        EditorPlugin API and built-in plugins
├── project_manager/ Project list / create / import UI
├── project_upgrade/ Project format migration tools
├── run/            Play-scene, run instances, debug run
├── scene/          2D/3D editor viewports, gizmos, placement tools
├── script/         Script editor, code completion
├── settings/       EditorSettings, EditorBuildProfile
├── shader/         Visual shader and text shader editors
├── themes/         Editor theme generation
├── translations/   Editor UI translations
└── version_control/ VCS integration (Git, etc.)
```

---

## `modules/` — optional features

Each subdirectory is an optional module with `config.py` (enable/disable rules)
and `register_types.cpp`. Built into the binary when enabled. **61 modules**
total.

| Module | Purpose |
|---|---|
| `gdscript` | GDScript language, compiler, VM |
| `mono` | C# / .NET scripting (optional build) |
| `jolt_physics` | Jolt Physics 3D backend |
| `godot_physics_2d` / `godot_physics_3d` | Built-in 2D/3D physics (default 3D engine) |
| `navigation_2d` / `navigation_3d` | Navigation mesh baking and pathfinding |
| `gltf` | glTF import/export |
| `glslang` | Shader compilation |
| `text_server_adv` / `text_server_fb` | Advanced / fallback text shaping |
| `openxr` / `webxr` / `mobile_vr` | XR platform support |
| `multiplayer` / `websocket` / `webrtc` / `enet` | Networking |
| `mcp` | MCP server integration (headless AI tooling) |
| `freetype` / `msdfgen` / `svg` | Font and vector rendering |
| `basis_universal` / `etcpak` / `bcdec` / … | Texture compression codecs |
| `mbedtls` / `upnp` | TLS and UPnP |
| *(others)* | Image formats, audio codecs, CSG, lightmapper, raycast, zip, … |

Full list: `ls modules/` (excludes `modules_builders.py`, `SCsub`,
`register_module_types.h`).

---

## `drivers/` — platform I/O backends

Low-level bindings selected per platform at build time.

```
drivers/
├── vulkan/         Vulkan rendering driver
├── gles3/          OpenGL ES 3 renderer
├── metal/          Metal renderer (macOS/iOS)
├── d3d12/          Direct3D 12 renderer (Windows)
├── egl/            EGL context management
├── gl_context/     OpenGL context helpers
├── sdl/            SDL integration hooks
├── accesskit/      Accessibility (AccessKit)
├── alsa/           Linux ALSA audio
├── pulseaudio/     Linux PulseAudio
├── coreaudio/      macOS/iOS CoreAudio
├── wasapi/         Windows WASAPI audio
├── xaudio2/        Windows XAudio2
├── coremidi/       macOS CoreMIDI
├── alsamidi/       Linux ALSA MIDI
├── winmidi/        Windows MIDI
├── apple/          Apple-specific helpers
├── apple_embedded/ iOS/visionOS embedded helpers
├── unix/           Unix signal handling, paths
├── windows/        Windows-specific helpers
├── backtrace/      Stack trace support
└── png/            PNG driver helpers
```

---

## `platform/` — OS ports

Each subdirectory is a full platform target with its own `detect.py`, `SCsub`,
and export templates.

```
platform/
├── linuxbsd/       Linux and *BSD
├── windows/        Windows (MSVC)
├── macos/          macOS
├── android/        Android
├── ios/            iOS
├── web/            Web (WASM + HTML5)
└── visionos/       Apple visionOS
```

---

## `thirdparty/` — vendored dependencies

~68 top-level vendored libraries. No git submodules — everything is copied in
tree. Key entries:

| Directory | Purpose |
|---|---|
| `jolt_physics/` | Jolt Physics engine (C++) |
| `embree/` | Ray tracing |
| `glslang/` / `spirv-cross/` | Shader compilation and reflection |
| `freetype/` / `harfbuzz/` / `graphite/` / `icu4c/` | Text rendering and shaping |
| `mbedtls/` | TLS/crypto |
| `volk/` / `vulkan/` / `rendering_device` deps | Vulkan ecosystem |
| `enet/` / `websocket/` deps | Networking |
| `openxr/` | XR runtime loader |
| `sdl/` | SDL3 (where used) |
| *(others)* | Image/audio codecs, compression, mesh tools, etc. |

---

## `tests/` — C++ unit tests

```
tests/
├── core/           Core type and container tests
├── scene/          Scene tree and node tests
├── servers/        Server and rendering tests
└── data/           Test fixtures and golden files
```

Run: `./bin/redot.linuxbsd.editor.x86_64 --test` (requires `tests=yes` build).
See [ONBOARDING.md](./ONBOARDING.md).

---

## `doc/` — class reference and doc tooling

```
doc/
├── classes/        911 class reference XML files (editor help source)
├── tools/          Doc generation scripts
└── translations/   Class-reference translations
```

Also contains `mcp-integration.md`, `nix.md`.

---

## `docs/` — Zodot planning

Zodot-specific documentation. Vision and migration strategy — not upstream
Redot docs.

| File | Purpose |
|---|---|
| [WORK_TREE.md](./WORK_TREE.md) | This file — directory map |
| [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md) | Vision, phases, success criteria |
| [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) | Current vs target architecture |
| [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) | Phase-by-phase migration plan |
| [ROADMAP.md](./ROADMAP.md) | Milestones and timeline |
| [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) | Zodot ↔ zGameLib boundaries |
| [VISION.md](./VISION.md) | Core values and non-goals |
| [MISSION.md](./MISSION.md) | Day-to-day commit principles |
| [DECISION_RATIONALE.md](./DECISION_RATIONALE.md) | Why Zig, ECS, Jolt, etc. |
| [DEPENDENCIES.md](./DEPENDENCIES.md) | Current and planned dependencies |
| [ONBOARDING.md](./ONBOARDING.md) | Build, run, test, IDE setup |
| [GIT_WORKFLOW.md](./GIT_WORKFLOW.md) | Commits, branches, merge policy |
| [CODING_CONVENTIONS.md](./CODING_CONVENTIONS.md) | C++ and Zig style rules |
| [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md) | Planned Zig/ECS patterns |
| [TESTING_STRATEGY.md](./TESTING_STRATEGY.md) | Future Zig test hierarchy |

---

## `misc/` — tooling and packaging

```
misc/
├── dist/           Export template shells (Windows, macOS, Linux, iOS, …)
├── error_suppressions/  ASAN/LSAN/TSAN/UBSAN suppressions for CI
├── extension_api_validation/  GDExtension API compatibility checks
├── msvs/           Visual Studio project templates
├── scripts/        Maintainer Python scripts
└── utility/        Misc build/packaging utilities
```

---

## `.github/` — CI

```
.github/
├── workflows/      GitHub Actions (builds, static checks, CodeRabbit)
├── actions/        Reusable composite actions
└── ISSUE_TEMPLATE/ Bug report and proposal templates
```

---

## Related

- [ONBOARDING.md](./ONBOARDING.md) — how to build and run
- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — layered architecture diagram
- [AGENTS.md](../AGENTS.md) — AI agent quick reference (links here)