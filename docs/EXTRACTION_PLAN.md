# Extraction Plan: Zodot ↔ zGameLib Boundaries

## Purpose

Define what moves between Zodot and zGameLib, when, and why. This prevents scope creep on both sides and keeps the "lean engine" principle enforceable.

## Current State of zGameLib

zGameLib already exists as an independent project at [`private/zGameLib`](../zGameLib). It is **not** hypothetical — it has working code today:

| Component | Status | Description |
|---|---|---|
| Platform adapter | ✅ **Working** | SDL3 windowing + input (`zig-cpp-platform-stack-adapter`) |
| Vulkan stack | ✅ **Working** | vk + volk + VMA + shaderc (`zig-cpp-vulkan-stack-adapter`) |
| zClip animation | ✅ **Working** | Sprite-atlas + skeletal-from-glTF |
| Gpu helper | ✅ **Working** | Vulkan bring-up (Gpu.init) |
| FrameRing | ✅ **Working** | Frames-in-flight ring |
| Swapchain | ✅ **Working** | Reusable swapchain policy |
| Surface bridge | ✅ **Working** | Comptime platform↔vulkan surface |
| Animation API | ⏳ **Stub** | Unified API over zClip (scaffold only; zClip itself is working) |
| App harness | ⏳ **Stub** | Being built out |
| Math library | ❌ **Not yet** | Planned (`zmath`) |
| Audio (zaudio) | ❌ **Not yet** | Planned |
| Assets (zassets) | ❌ **Not yet** | Planned |
| Physics adapter | ❌ **Not yet** | This doc's design |
| Hot-reload utils | ❌ **Not yet** | This doc's design |
| Allocators | ❌ **Not yet** | Beyond what Zig std provides |

**License:** zGameLib is Apache-2.0. The sibling libs are MIT.

**Zig version:** Requires Zig 0.16+.

## The Bidirectional Relationship

```
    ┌──────────────────────────────────────────────────────────────────┐
    │                     zGameLib (already exists)                     │
    │                                                                  │
    │  PROVIDES TODAY:   platform · vk/vma/volk · zClip · Gpu         │
    │                     FrameRing · swapchain · surface · animation  │
    │                                                                  │
    │  FUTURE ADDITIONS (from Zodot or independently):                 │
    │                     math  · allocators · hot-reload utils        │
    │                     Physics Adapter · audio · assets             │
    ├──────────────────────────────────────────────────────────────────┤
    │                     Zodot (consumes zGameLib)                     │
    │                                                                  │
    │  New Zig code in Zodot imports `zgame.platform`, `zgame.vk`,     │
    │  `zgame.zclip`, etc. directly. No duplicate abstractions.        │
    │                                                                  │
    │  STAYS IN ZODOT FOREVER:                                         │
    │  - Redot compatibility layer (scene tree, Variant, GDExtension)  │
    │  - Editor (C++, unchanged)                                       │
    │  - High-level mod loader + capability system                     │
    │  - Deep integration: PhysicsServer + ECS bridge                  │
    │  - Rendering server (scene graph traversal, materials)           │
    │  - Audio server (wrap via @cImport)                              │
    │  - Navigation server (wrap via @cImport)                         │
    └──────────────────────────────────────────────────────────────────┘
```

## Phase 1-2: Consumption from zGameLib

Zodot consumes existing zGameLib components for **new Zig code only**. Existing C++ systems continue to work as-is.

### What We Can Use Today

These are ready to import in Zodot's Zig code right now:

```zig
const zgame = @import("zgame");

// Platform layer — windowing + input via SDL3
const window = try zgame.platform.Window.create(.{
    .title = "Zodot",
    .size = .{ .w = 1280, .h = 720 },
});
const input = zgame.platform.input;

// Vulkan stack — vk + VMA + shaderc
const vk = zgame.vk;
const vma = zgame.vma;

// Animation — zClip for sprite/skeletal
const clip = try zgame.zclip.SpriteAtlas.load(arena, "player.atlas");
```

**Do not duplicate these in Zodot.** If Zodot needs platform access from Zig, it uses `zgame.platform`. If it needs Vulkan helpers, it uses `zgame.Gpu` / `zgame.FrameRing`.

### What Must Be Built (in zGameLib or Zodot)

| Need | Where It Lives | Current Status |
|---|---|---|
| **Platform layer** | `zgame.platform` (zGameLib) | Ready |
| **Vulkan stack** | `zgame.vk` / `zgame.vma` (zGameLib) | Ready |
| **Animation primitives** | `zgame.zclip` (zGameLib) | Ready |
| **Animation API** | `zgame.animation` (zGameLib) | Stub (scaffold only) |
| **Physics adapter** | Needs design (this doc) | Not started |
| **Hot-reload utilities** | Needs design (this doc) | Not started |
| **Math / linear algebra** | zGameLib (planned `zmath`) | Not started |
| **Allocators** | Zig std + zGameLib additions | Not started |
| **Audio** | zGameLib (planned `zaudio`) | Not started |

## Phase 3: Extraction into zGameLib

Components that start in Zodot, mature, and then move into zGameLib.

### Physics Adapter (Design)

The Physics Adapter is a **zGameLib abstraction** that provides a clean Zig physics API with a swappable backend:

```zig
// zGameLib physics adapter (future) — interface definition
pub const PhysicsBackend = struct {
    ptr: *anyopaque,
    vtable: *const VTable,

    pub const VTable = struct {
        create_rigid_body: *const fn (ctx: *anyopaque, params: RigidBodyParams) callconv(.C) u64,
        remove_body: *const fn (ctx: *anyopaque, id: u64) callconv(.C) void,
        step: *const fn (ctx: *anyopaque, dt: f32) callconv(.C) void,
        // ...
    };
};

// Zodot uses it:
const backend = try zgame.physics.PhysicsBackend.initJolt(allocator);
backend.create_rigid_body(.{ .position = .{0, 5, 0}, .shape = .box });
```

**Why an adapter?** Zodot can switch backends (Jolt, Godot Physics, or a future Zig-native one) without changing engine code.

**Where it starts:** In Zodot (Phase 1-2), as a thin wrapper. Once stable, extract to zGameLib.

### Math / Linear Algebra

| Component | Why Extract | Extraction Signal |
|---|---|---|
| Vec2/3/4, Mat4, Quaternion | Non-engine-specific; every Zig game needs these | Stable API, tested, no Redot dependencies |
| Transform (affine, 3D) | Game-agnostic if designed without scene tree coupling | Review for Redot-isms |
| Intersection / collision queries | Useful for any game or tool | Must be separable from Jolt types |

**Where it starts:** Could start in zGameLib directly (as `zmath`) or in Zodot. Prefer starting in zGameLib to avoid a move later.

### Memory Allocators

| Component | Why Extract |
|---|---|
| Arena allocator | Every system uses it; no engine coupling |
| Pool allocator (fixed-size) | Independent utility |
| Stack allocator | Same |

**Where it starts:** Zig std provides basic arenas. Zodot may need specialized pools. Start in Zodot, extract when stable.

### Hot-Reload Utilities

| Component | Why Extract |
|---|---|
| `DynLib` wrapper (open, lookup, close) | General-purpose |
| Symbol table management | Applies to any native-reload project |
| Copy-and-swap pattern helper | Reusable pattern |

**Where it starts:** In Zodot (hot-reload is a core Zodot feature). Extract after the pattern is proven.

## Extraction Checklist

Before any component moves from Zodot → zGameLib:

- [ ] **Minimal engine dependencies:** Does it depend on `Variant`, `Node`, `Object`, `SceneTree`, or any Redot C++ type? If yes, it stays in Zodot or needs an abstraction layer.
- [ ] **Explicit allocators:** All public functions take an allocator parameter. No hidden `g_malloc` or global state.
- [ ] **Clear lifecycle:** `init(allocator)` / `deinit()` pair. Types are not just dumped into the library without ownership semantics.
- [ ] **Clean, documented Zig API:** Documented with `///` doc comments. Example in the doc comment. No C++ heritage artifacts (weird naming, redundant types).
- [ ] **Version stability:** The API should not change every week. Extraction is a sign of maturity, not experimentation.

## Bloat Guardrails for zGameLib

zGameLib must also stay lean. It is not "everything Zodot might ever want." It is "things many Zig game projects would need."

| Belongs in zGameLib | Does Not Belong |
|---|---|
| Math types | Full rendering engine |
| Platform abstraction (SDL3) | Editor or asset pipeline |
| Physics interface (not implementation) | Engine-specific game frameworks |
| Hot-reload primitives | Runtime type system or reflection |
| Allocators | Script language runtime |

## What Stays in Zodot (Permanently)

| Component | Why |
|---|---|
| Redot compatibility layer | Scene tree, Variant, GDExtension — these are Zodot's identity as a Redot fork |
| Editor (inspector, docks, debugger) | ~670K lines of C++ (`editor/` + `scene/`); editor is not the performance bottleneck |
| Mod loader + capability system | Deeply tied to Zodot's specific modding vision and runtime |
| ECS + PhysicsServer integration | Wiring between ECS and Jolt is Zodot-specific |
| High-level rendering server | Scene graph traversal, material system, lighting — engine-specific |
| Audio server | Stable, low churn, works; wrap via `@cImport` |
| Navigation server | Same as audio |

## Open Questions

- Should the ECS framework live in Zodot or zGameLib? (Current leaning: Zodot until stable [Phase 2], then extract.)
- zGameLib's App harness is a stub — should Zodot help drive its design or wait?
- How do we handle versioning across Zodot ↔ zGameLib during the transition? (SemVer + pinned commits.)

## Next Documents to Read

- [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) — How extraction fits into phases
- [ROADMAP.md](./ROADMAP.md) — When extraction milestones are targeted
- [VISION.md](./VISION.md) — Why this ecosystem approach matters
- [MISSION.md](./MISSION.md) — Operational principles for Zodot ↔ zGameLib
