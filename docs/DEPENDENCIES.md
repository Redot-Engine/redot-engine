# Dependencies

## Current Dependencies (Redot Engine LTS — C++)

All thirdparty libraries are vendored in `thirdparty/`. No Git submodules are used. Key dependencies:

| Dependency | Version / Ref | Purpose | License |
|---|---|---|---|
| **Redot Engine** | 26.3.0-alpha (Godot 4.5.2 base) | Upstream engine fork | MIT |
| **Jolt Physics** | Git head (vendored) | 3D physics simulation | MIT |
| **embree** | 3.x (vendored) | Ray tracing kernels | Apache 2.0 |
| **etcpak** | latest (vendored) | Texture compression (ETC) | Apache 2.0 |
| **miniupnpc** | latest (vendored) | UPnP for networking | BSD-3 |
| **mbedTLS** | 2.x (vendored) | TLS/crypto | Apache 2.0 / GPL |
| **Vulkan SDK** | 1.3+ | Graphics API | (system dep) |
| **glslang** | vendored | GLSL-to-SPIR-V compilation | BSD-3 |
| **SPIRV-Cross** | vendored | SPIR-V reflection/cross-compile | Apache 2.0 |
| **WASM (binaryen)** | vendored | WebAssembly tooling (for web export) | Apache 2.0 |
| **pcre2** | vendored | Regex engine | BSD-3 |
| **zlib** / **minizip** | vendored | Compression | Zlib |
| **freetype** | vendored | Font rendering | FTL / GPL |
| **opus** / **vorbis** | vendored | Audio codecs | BSD |
| **graphite** | vendored | Advanced text shaping | Apache 2.0 |
| **harfbuzz** | vendored | Text shaping | MIT |
| **ICU** | vendored | Unicode support | Unicode |
| **Mono / .NET** | 6.x+ (optional) | C# scripting runtime | MIT / LGPL |

### Build-Time Tools

| Tool | Purpose |
|---|---|
| **SCons** >= 4.0 | Build system (Python) |
| **Python** >= 3.8 | Build scripts, tooling |
| **C++20 compiler** (GCC, Clang, MSVC) | C++ compilation |
| **Nix** (optional) | Reproducible dev environment |

## Planned Zig Ecosystem Dependencies

These are targets for Phase 1-2, not yet integrated:

| Dependency | Purpose | Status | License |
|---|---|---|---|
| **Zig** (master / 0.14+) | Primary implementation language | Not yet used | MIT |
| **build.zig** | Zig-native build system | Planned | Built-in |
| **zig-gamedev** (or similar) | Vulkan/GPU abstractions, windowing | Under evaluation | MIT |
| **zflecs** (or custom ECS) | Flecs-inspired ECS framework in Zig | Under evaluation | MIT |
| **zGameLib** | Reusable Zig game library (our own, exists at [`private/zGameLib`](../zGameLib)) | v0.1.0, ready now | Apache-2.0 |
| **wasmtime** / **wasm3** (via Zig FFI) | WASM runtime for mod sandbox | Under evaluation | Apache 2.0 |
| **Jolt Physics** (C++ via `@cImport`) | 3D physics (unchanged) | Kept through Phase 1-2 | MIT |

### Dependency Graph (Planned)

```
+-------------------------------------------+
|               Zodot Engine                |
|                                           |
|  +--------+  +---------+  +-----------+  |
|  | zGameLib|  | WASM    |  | Jolt      |  |
|  | (Phase3)|  | Runtime |  | Physics   |  |
|  |         |  | (FFI)   |  | (@cImport)|  |
|  +----+----+  +---------+  +-----+-----+  |
|       |                            |       |
|       v                            v       |
|  +---------------------------------------+ |
|  |           Zig Standard Library        | |
|  +---------------------------------------+ |
|  |           Vulkan SDK / drivers        | |
|  +---------------------------------------+ |
|  |    C++ Runtime (for existing deps)    | |
|  +---------------------------------------+ |
+-------------------------------------------+
                          ^
                          |
  (In Phase 0-1, this    |
   entire box is the     |
   Redot C++ codebase)   |
                          |
+-------------------------------------------+
|         Redot C++ Engine (scaffold)       |
|  - Core, Servers, Scene, Editor, Modules |
|  - SCons build, Jolt module, GDScript    |
+-------------------------------------------+
```

## Relationship with zGameLib

```
Phase 0-1:  zGameLib [already exists]
            Zodot can consume zgame.platform, zgame.vk, zgame.zclip
            for new Zig code. C++ parts of Zodot stay independent.

Phase 2:    Zodot [depends on] Zig std + optional WASM runtime
            zGameLib [may gain] math, allocators, hot-reload utils
            (some internal patterns proto-extracted from Zodot)

Phase 3:    Zodot [depends on] zGameLib more heavily (extracted components)
            zGameLib [depends on] Zig std + sibling libs
            Bidirectional flow: Zodot → zGameLib and zGameLib → Zodot

Final:      Zodot = zGameLib + engine-specific layers (editor, scene compat, etc.)
```

**Current status:** zGameLib exists at [`private/zGameLib`](../zGameLib) (v0.1.0, Apache-2.0, Zig 0.16+). It already provides a platform adapter (SDL3), Vulkan stack (vk + volk + VMA + shaderc), and zClip animation library. New Zig code in Zodot should consume these directly rather than re-implementing. Future extraction of Zodot components into zGameLib is planned for Phase 3.

## External Tools We Keep vs. Replace

| Tool / Library | Decision | Reasoning |
|---|---|---|
| **Jolt Physics** | Keep (through Phase 2) | High quality, permissive license, works. Replacing it would delay ECS migration. |
| **Vulkan** | Keep | Industry standard. No viable Zig-native replacement with equivalent driver support. |
| **GLES3 / Metal / D3D12** | Keep (as compatibility) | Too much value to discard. Port rendering layer to Zig but keep backends. |
| **GDScript** | Deprecate (compat layer only) | Interpreted overhead, hard to sandbox. WASM replaces it. |
| **C# (Mono)** | Drop | Runtime complexity, GC conflicts with ECS data model. |
| **SCons** | Replace with `build.zig` | Phase 1 goal. Native Zig integration. |
| **Variant** | Replace | Central performance tax. Replace with tagged unions / Zig comptime. |
| **Node tree** | Replace (compat layer) | ECS supersedes it for game logic. UI remains node-based. |

## License & Compatibility Notes

- Zodot inherits the **MIT license** from Redot/Godot.
- Jolt Physics is MIT. No license conflicts.
- WASM runtime options (wasmtime: Apache 2.0, wasm3: MIT) are compatible.
- Vulkan SDK, SPIRV-Cross, glslang are all permissively licensed.
- The only potential concern is **ICU** (Unicode License) for text — retained for compatibility.

## Related Documents

- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — Layered architecture and dependency placement
- [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md) — Coding patterns for extraction
- [DECISION_RATIONALE.md](./DECISION_RATIONALE.md) — Why keep Jolt, why drop Mono, etc.

## Questions This Document Answers

- What does the current codebase depend on?
- What Zig ecosystem dependencies are planned?
- What is the relationship between Zodot and zGameLib?
- Which external tools are kept, replaced, or deprecated?
- Are there any license conflicts?
