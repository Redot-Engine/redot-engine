# Migration Plan

## Overall Strategy

Three-step migration policy:

1. **Zig becomes the build system / entry point.** `build.zig` orchestrates everything. **SCons is fully removed**, not augmented. Python is never used for engine builds.
2. **Wrap stable C++ libraries via `@cImport`** where it enables hot-reload or cleaner integration.
3. **Only rewrite/port to pure Zig** when there is clear benefit: hot-reload capability, major performance win, data-oriented redesign (ECS), or new modability features.

We do **not** rewrite things that are already working well just because they are C++.

**Python policy:** After SCons removal, Python is restricted to MCP servers, AI tooling, and supporting scripts. It is never used for building the engine.

```
Current:  SCons → C++ compiler → redot binary
          All C++, all in one process.

Phase 1:  build.zig → C++ compiler (via zig c++) → redot binary + Zig static lib
          Zig code coexists, linked into the same binary.

Phase 2:  build.zig → C++ compiler + zig build → redot binary
          Hot-reload systems in Zig. C++ wrapped via @cImport where strategic.

Phase 3:  build.zig → zig build (C++ deps via @cImport) → redot binary
          Core systems in Zig. C++ only for editor + legacy compat.
```

---

## Phase 0: Discovery & Planning (Current)

**Status:** Complete.

### Deliverables
- [x] `PROJECT_OVERVIEW.md` — Vision, roadmap, success criteria
- [x] `ARCHITECTURE_OVERVIEW.md` — Current vs. target architecture
- [x] `DEPENDENCIES.md` — Full dependency inventory
- [x] `ONBOARDING.md` — Build and run guide
- [x] `KEY_PATTERNS_AND_CONVENTIONS.md` — Coding conventions
- [x] `DECISION_RATIONALE.md` — Major decisions with reasoning
- [x] `VISION.md` — Philosophical north star (what we become)
- [x] `MISSION.md` — Operational mission (what we do every day)
- [x] `MIGRATION_PLAN.md` — This document
- [x] `ROADMAP.md` — Milestone-driven timeline
- [x] `EXTRACTION_PLAN.md` — zGameLib boundary plan

### Next (In Phase 0)
- Validate Zig toolchain against Redot build requirements
- Spike: `build.zig` that compiles a minimal C++ file from the existing codebase
- Audit all C++ subsystems for "keep / wrap / rewrite" classification

---

## Phase 1: Build System + Incremental Foundations (Self-Contained)

**Goal:** Zig is the entry point. C++ still does most of the work, but we ship a working binary with initial Zig code linked in.

### 1.1 Replace SCons with `build.zig` (Phase 1 Priority)

SCons will be **fully removed**, not kept alongside `build.zig`. Maintaining two build systems doubles burden and confuses contributors.

| Concern | Current (SCons) | Target (build.zig) |
|---|---|---|
| Entry point | `scons platform=linuxbsd target=editor` | `zig build` |
| C++ compilation | SCons CXX task | `zig c++` (same compiler, same flags) |
| Module system | Python-based `config.py` + `SCsub` | Zig modules + `@cImport` |
| Binary output | `bin/redot.{plat}.{target}.{arch}` | Same (preserve convention) |

**Strategy:**
- Create `build.zig` as the single entry point.
- Initially `build.zig` calls `zig c++` directly on existing C++ source files, replicating the compiler flags SCons currently generates.
- Alternatively, shell out to SCons during transition while handling Zig code directly.
- Over time, fold all C++ compilation into `build.zig` proper.
- Once `build.zig` can produce an equivalent binary, **delete `SConstruct`, `SCsub` files, and all Python build scripts**.

**Risk:** SCons has 1159 lines of platform detection, module discovery, and flag computation. We do not replicate all of it at once. Run SCons first to generate a flag dump, then port piece by piece.

**Python's future:** After SCons removal, Python is only used for MCP servers, AI tooling, and supporting scripts. No Python in the engine build pipeline.

### 1.2 Consume Existing zGameLib Components for New Zig Code

zGameLib already provides working Zig modules that Zodot should use immediately for new Zig code:

| Component | What It Provides | Import |
|---|---|---|
| Platform adapter | SDL3 windowing + input | `zgame.platform` |
| Vulkan stack | vk + volk + VMA + shaderc | `zgame.vk`, `zgame.vma`, etc. |
| zClip | Sprite-atlas + skeletal animation | `zgame.zclip` |
| Gpu / FrameRing | Vulkan bring-up, frames-in-flight | `zgame.Gpu`, `zgame.FrameRing` |
| Surface / Swapchain | Comptime platform↔vulkan bridge | `zgame.surface`, `zgame.swapchain` |

```zig
// New Zig code in Zodot uses zGameLib directly:
const zgame = @import("zgame");

const window = try zgame.platform.Window.create(.{
    .title = "Zodot",
    .size = .{ .w = 1280, .h = 720 },
});
const gpu = try zgame.Gpu.init(window, .{});
```

**Do not re-implement these inside Zodot.** Duplicating zGameLib components wastes effort and fragments the ecosystem.

### 1.3 Zig Wrappers Around Stable C++ Libraries (Where zGameLib Does Not Reach)

For C++ libraries zGameLib does not cover, use `@cImport` to create thin Zig wrappers:

```zig
// Jolt Physics Zig wrapper (thin)
const jolt = @cImport({
    @cInclude("Jolt/Jolt.h");
    @cInclude("Jolt/Physics/PhysicsSystem.h");
});

pub const PhysicsSystem = struct {
    handle: *jolt.JPH_PhysicsSystem,
    pub fn init(…) !PhysicsSystem { … }
    pub fn step(self: *PhysicsSystem, dt: f32) void { … }
    pub fn deinit(self: *PhysicsSystem) void { … }
};
```

**Candidate libraries for FFI wrapping:**
| Library | Priority | Why |
|---|---|---|
| Jolt Physics | High | Enables hot-reload of physics; ECS integration |
| embree | Medium | Raycasting queries for ECS |
| FreeType / HarfBuzz | Low | Text is stable, low churn |

### 1.4 Minimal Native Hot-Reload

Implement the Madrigal/Traction Point pattern for one non-critical system first:

```zig
// Hot-reload runtime (simplified)
const HotReloadedSystem = struct {
    lib: std.DynLib,
    update: *const fn (world: *World, dt: f32) void,
    deinit: *const fn () void,

    pub fn load(path: [:0]const u8) !HotReloadedSystem {
        const lib = try std.DynLib.open(path);
        const update = lib.lookup(*const fn (world: *World, dt: f32) void, "system_update") orelse return error.MissingSymbol;
        const deinit = lib.lookup(*const fn () void, "system_deinit") orelse return error.MissingSymbol;
        return .{ .lib = lib, .update = update, .deinit = deinit };
    }
};
```

**Target:** Hot-reload of the audio system or a debug rendering overlay. Not the critical path. Prove the mechanism works.

### 1.5 ECS Spike (New Code Only)

Write the ECS core in Zig alongside the existing node tree. New game logic can optionally use ECS; old projects still work via the scene tree.

```zig
// ECS core — minimal, no dependencies on Redot types
pub const World = struct {
    entities: std.ArrayListUnmanaged(Entity),
    components: ComponentStore,
    systems: std.ArrayListUnmanaged(SystemDef),
};

pub fn query(comptime Cs: type) Query(Cs) { … }
```

**Do NOT attempt to replace the node tree yet.** The ECS lives beside it.

### 1.6 Success Criteria

- [ ] `zig build` produces a working Redot editor binary
- [ ] Jolt Physics accessible from Zig via `@cImport` (smoke test passes)
- [ ] One system hot-reloads (non-critical, e.g. debug overlay)
- [ ] ECS can spawn entities and run a system (no rendering, no Redot integration)
- [ ] All existing C++ tests still pass

### 1.7 Risks

| Risk | Mitigation |
|---|---|
| `build.zig` cannot replicate SCons platform detection | Run SCons to generate flag list; parse and inject into zig build |
| Zig C++ interop with complex C++ templates (Variant, Ref) | Wrap at a higher level; avoid cImport for template-heavy headers |
| Hot-reload causes subtle state corruption | Copy state before swap; validate with test suite after each reload |

---

## Phase 2: Systems & Modability

**Goal:** Hot-reload is the default development mode. WASM mods load and run. ECS handles game logic alongside legacy scene tree.

### 2.1 Hybrid Hot-Reload

| Aspect | Native (Zig) | WASM (Mods) |
|---|---|---|
| Use case | Engine systems, core game code | Community mods, user scripts |
| Reload mechanism | `std.DynLib` open/close | WASM runtime instantiation |
| Safety | Full process memory | Sandboxed, capability-gated |
| Performance | Native | ~10-30% slower (FFI boundary) |
| Distribution | Not for end users | Shipped with game |

### 2.2 Capability-Based Mod API

```zig
// In zGameLib (or Zodot):
pub const ModManifest = struct {
    name: []const u8,
    version: SemVer,
    permissions: struct {
        networking: bool = false,
        file_read: ?[]const u8 = null,   // specific path
        spawn_entities: bool = false,
        debug_log: bool = false,
    },
};

// Host functions exposed to WASM:
pub const HostAPI = struct {
    spawn_entity: *const fn (template_id: u32) callconv(.C) u64,
    read_input: *const fn () callconv(.C) InputState,
    write_log: *const fn (msg: [*:0]u8) callconv(.C) void,
};
```

### 2.3 Rendering Server Modernization

The existing Vulkan renderer is ~150K lines of C++. We do not rewrite it.

Instead:
- Create a **Zig rendering abstraction layer** that wraps specific hot paths (draw call submission, material batching).
- Use `@cImport` to call into the existing C++ renderer for complex operations (shader compilation, pipeline state objects).
- Over time, replace individual subsystems (e.g., implement a Zig-based batching system that outperforms the C++ one).

### 2.4 Audio / Navigation Integration

- Audio: Wrap via `@cImport` (existing Godot audio server is stable; not worth rewriting).
- Navigation: Wrap via `@cImport` (Recast/Detour is vendored and stable).
- Only port if a specific hot-reload or performance need arises.

### 2.5 Success Criteria

- [ ] Native hot-reload works for at least one gameplay system (e.g., player controller)
- [ ] WASM mod can spawn entities, read input, and write to log
- [ ] Capability system prevents mod from accessing undeclared resources
- [ ] ECS + scene tree coexist — a hybrid project runs

### 2.6 Risks

| Risk | Mitigation |
|---|---|
| WASM runtime overhead too high for hot paths | Profile early; cache compiled WASM modules |
| Capability model too restrictive for useful mods | Iterate with real mod authors |
| ECS + scene tree dual state gets out of sync | Synchronization pass between ECS and scene tree transforms on frame boundaries |

---

## Phase 3: Extraction & Ecosystem

**Goal:** Mature systems extracted to zGameLib. Zodot becomes a consumer of zGameLib adapters.

### 3.1 Identify Extraction Candidates

See [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) for full details.

**Likely candidates:**
- Math / linear algebra helpers
- Memory allocators (arena, pool, stack)
- GPU abstractions (Vulkan wrappers, frame ring, descriptor management)
- Hot-reload utilities
- Animation primitives

### 3.2 Create zGameLib Adapters

The adapter pattern:

```
┌──────────────────────────────────────────────┐
│              zGameLib (already exists)         │
│                                               │
│  ✅ Platform  │ ✅ GPU/Vulkan │ ✅ zClip     │
│     (SDL3)    │  (vk/VMA/volk)│  (animation)  │
│                                               │
│  🔜 Physics Adapter (not yet)                 │
│     - zig physics types (rigid body, etc.)    │
│     - Abstract PhysicsBackend interface       │
│     - JoltBackend (cImport impl)              │
│                                               │
│  🔜 Math / Allocators / Hot-reload utils      │
└──────────────────────────────────────────▲────┘
         ▲                      ▲          │
         │ consumers            │ consumers│future
┌────────┴──────────────────────┴──────────┴─────┐
│                   Zodot                         │
│  - Redot compatibility layer                   │
│  - High-level ECS + PhysicsServer integration  │
│  - Mod loader + capability system              │
│  - Editor (C++, unchanged)                     │
└────────────────────────────────────────────────┘
```

### 3.3 What Stays in Zodot Forever

| Component | Why It Stays |
|---|---|
| Redot compatibility layer (scene tree, Variant bridge, GDExtension) | Core identity of the fork |
| Editor (inspector, docks, debugger, canvas) | 900K+ lines; works; not worth touching |
| High-level mod loader and capability system | Tied to Zodot's specific modding vision |
| Deep integration: Physics Server + ECS bridge | Zodot-specific wiring; no value in zGameLib |
| Rendering server (high-level) | Scene graph traversal, material system are engine-specific |

### 3.4 Success Criteria

- [ ] At least one adapter in zGameLib consumed by Zodot (e.g., Platform adapter)
- [ ] At least one component extracted from Zodot into zGameLib
- [ ] A pure-Zig project (not Zodot) uses a zGameLib adapter
- [ ] Existing Redot project loads and runs with partial Zig systems

---

## What Stays in Zodot vs Moves to zGameLib (Summary)

```
┌──────────────────────────────────────────────────────────────┐
│                    ZODOT (Engine)                             │
│  Redot compat │ Editor (C++) │ Mod loader │ ECS + bridge     │
│  PhysicsServer integration │ High-level rendering server     │
├──────────────────────────────────────────────────────────────┤
│                    ZGAMELIB (Library)                         │
│                                                              │
│  ✅ EXISTS TODAY:   Platform (SDL3) · Vulkan stack           │
│                     zClip animation · Gpu/FrameRing          │
│                     Surface/swapchain · Animation API        │
│                                                              │
│  🔜 FUTURE:         Math library · Allocators                │
│                     Hot-reload utils · Physics Adapter       │
│                     Audio (zaudio) · Assets (zassets)        │
└──────────────────────────────────────────────────────────────┘
```

## Open Questions

- Should the ECS live in Zodot or in zGameLib from the start? (Current leaning: Zodot until stable, then extract.)
- How do we handle Visual Studio / Xcode project generation without SCons? (Zig's `build.zig` has `installVsProject` support to investigate.)
- When should the existing C++ test suite (SCons-era) be converted to Zig-driven tests? (Likely after Phase 1.1, before 1.2.)

## Next Documents to Read

- [ROADMAP.md](./ROADMAP.md) — Timeline and milestones
- [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) — Detailed zGameLib boundary plan
- [VISION.md](./VISION.md) — Philosophical foundation (what we become)
- [MISSION.md](./MISSION.md) — Operational mission (what we do every day)
