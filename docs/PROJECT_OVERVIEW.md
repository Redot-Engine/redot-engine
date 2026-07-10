# Project Overview: Zodot Engine

## Vision

**Zodot** is a planned fork of the Redot Engine LTS (itself a community fork of Godot 4.x) with a long-term goal of replacing its C++ core with Zig — layer by layer, system by system — while maintaining full compatibility with the existing Godot/Redot ecosystem.

The ultimate target is a self-contained game engine that:
- Uses **Zig** for all core engine systems (physics, rendering pipeline, ECS framework, asset pipeline).
- Ships a **hybrid hot-reload** system (native Zig for performance, WASM for mod/script sandboxing).
- Consumes architecture patterns from an independent reusable game library called **zGameLib**.
- Preserves the ability to open and run existing Godot/Redot projects wherever practical.

## Why This Fork Exists

| Motivation | Explanation |
|---|---|
| **Performance** | C++ node trees and GDScript interpretation leave significant performance on the table. Zig's comptime, explicit allocators, and tight control over memory layout enable data-oriented design by default. |
| **ECS Architecture** | The Godot/Redot scene tree (inheritance-heavy, reference-counted) is being augmented — and eventually replaced — by an Entity Component System (ECS) for game logic, while retaining a compatibility layer for legacy scenes. |
| **Modability** | Godot's GDScript/C# modding story is fragile. We target a WASM-first sandboxed mod API with explicit capability declarations, making multiplayer and user-generated-content scenarios safer and more performant. |
| **Obfuscation & IP Protection** | Zig's powerful comptime and binary stripping make it significantly harder to reverse-engineer release builds compared to C++/GDScript-based engines. |
| **Native Code Focus** | The engine is built from the ground up with native-code-first developers in mind — the people who reach for Rust, Zig, C++, or C when shipping a game. |

## Strategic Roadmap (Summary)

```
Phase 0 — Discovery & Planning (current)
  ├── Create foundation documents (this set)
  ├── Establish Zig toolchain, build.zig, and CI
  └── Port build system (SCons → Zig build)

Phase 1 — Zig Self-Contained Core
  ├── Port core data structures (Variant-free replacements)
  ├── Implement ECS framework (inspired by zflecs / Flecs patterns)
  ├── Port Jolt Physics binding layer to Zig
  └── Wire up minimal render path (Vulkan via zig-gamedev or custom)

Phase 2 — Hybrid Hot-Reload & Modding
  ├── Native hot-reload for engine systems
  ├── WASM sandbox for game scripts / mods
  └── Capability-based mod API

Phase 3 — zGameLib Extraction
  ├── Extract reusable systems → zGameLib
  ├── Zodot consumes zGameLib as a dependency
  └── zGameLib becomes independently usable without Zodot
```

## Target Audience & Use Cases

- **Indie to mid-size game teams** shipping native-code games who want Godot's editor UX without Godot's runtime overhead.
- **Modding communities** building UGC-heavy games that need safe, sandboxed script execution.
- **Engine programmers** interested in a real-world Zig game engine case study.
- **Researchers** exploring ECS performance in production game engines.

## High-Level Success Criteria

1. A minimal Redot project (MesaDriven demo or similar) compiles and runs with the Zig-based core.
2. Jolt Physics integrated and passing existing test scenes within 10% of C++ baseline performance.
3. Hot-reload cycle under 500ms for native system changes.
4. WASM mod runs with no access to host memory or file system except through declared API.
5. Release binary is obfuscated enough that simple `strings` / `nm` analysis reveals minimal internal structure.

## What This Project Is NOT

- **NOT** a rewrite of the Godot editor. The existing editor (in C++) will continue to be used; we are replacing the engine runtime, not the tooling.
- **NOT** a replacement for GDScript or C# in the short term. Legacy script support via a compatibility layer is planned but not a priority.
- **NOT** a drop-in replacement for Godot 4.x at launch. Migrating projects will require some work.
- **NOT** a fully independent engine yet. Phase 0-1 depend heavily on the existing Redot C++ codebase as scaffolding.

## Current State

**As of this writing (July 2026), the project is in Phase 0.** The repository contains the full Redot Engine LTS codebase (C++17/C++20, SCons build, ~77K commits) with zero Zig code. All documentation below describes the *planned* architecture and vision, not the current implementation.

## Related Documents

- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — Current vs. target architecture in detail
- [ONBOARDING.md](./ONBOARDING.md) — How to build and run the current fork
- [DECISION_RATIONALE.md](./DECISION_RATIONALE.md) — Why each major architectural decision was made

## Questions This Document Answers

- What is the Zodot project trying to achieve?
- Why fork Redot instead of building from scratch or using another engine?
- What are the high-level migration phases?
- Who is this engine for?
- What is the project *not* trying to be?
