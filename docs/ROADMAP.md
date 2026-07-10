# Roadmap

## Dependency Graph of Milestones

```
Phase 0 (Current)
│
├── Discovery docs complete ────────────────────────────────► Project known
│
▼
Phase 1: Build + Foundations
│
├── 1.1 build.zig orchestrates C++ ─────────────────────► SCons replaced
├── 1.2 Zig code linked into Redot binary ──────────────► First Zig in the engine
├── 1.3 zGameLib dependency added to build.zig ────────► Zodot can import zgame.*
├── 1.4 Jolt wrapped via @cImport ─────────────────────► Physics accessible from Zig
├── 1.5 Native hot-reload spike (debug overlay) ───────► Hot-reload mechanism proven
├── 1.6 ECS spike (new code only) ────────────────────► Minimal ECS running
│     │
│     └─── these can run in parallel ─────────────────────
│
▼
Phase 2: Systems & Modability
│
├── 2.1 Hybrid hot-reload (native + WASM) ─────────────► Full reload suite
├── 2.2 Capability-based mod API ──────────────────────► Mods load safely
├── 2.3 ECS + scene tree coexistence ──────────────────► Hybrid projects work
├── 2.4 Rendering server Zig abstraction (hot paths) ──► Leaner rendering path
│     │
│     └─── 2.4 draws on existing zGameLib Vulkan stack ──
│
▼
Phase 3: Extraction & Ecosystem
│
├── 3.1 New component extracted from Zodot → zGameLib ─► zGameLib gains from Zodot
├── 3.2 Pure-Zig project uses zGameLib independently ──► zGameLib is independently useful
├── 3.3 Existing Redot project runs with Zig systems ──► Migration validated
```

## Timeline

```
H2 2026                      H1 2027                      H2 2027
├────────────────────────────┼────────────────────────────┼─────────────────────►
│                            │                            │
Phase 0 ──► (done)          │                            │
│                            │                            │
Phase 1 ────────────────────►│                            │
│  1.1 build.zig             │                            │
│  1.2 First Zig linked      │                            │
│  1.3 zGameLib dep          │                            │
│  1.4 Jolt @cImport         │                            │
│  1.5 Hot-reload spike      │                            │
│  1.6 ECS spike             │                            │
│                            │                            │
│              Phase 2 ──────┼───────────────────────────►│
│              2.1 Hybrid HR │                            │
│              2.2 WASM mods │                            │
│              2.3 ECS coex. │                            │
│              2.4 Render ab.│                            │
│                            │                            │
│                           Phase 3 ──────────────────────┼──────►
│                           3.1 Extraction -> zGameLib    │
│                           3.2 Indep. zGameLib           │
│                           3.3 Redot project on Zig      │
```

## Milestones

### M1: First Zig in the Binary

| Aspect | Detail |
|---|---|
| **What** | `zig build` produces a Redot editor binary with a small Zig static library linked in. A Zig `hello` function is callable from C++. |
| **Acceptance** | `strings bin/redot.* | grep "Hello from Zig"` works |
| **Depends on** | Toolchain validation, `build.zig` skeleton |
| **Risk** | Zig's C++ interop may struggle with Redot's template-heavy headers |

### M2: Jolt Accessible from Zig

| Aspect | Detail |
|---|---|
| **What** | Zig code calls Jolt physics functions via `@cImport`. A simple smoke test (create rigid body, step, read position) passes. |
| **Acceptance** | Zig test calls `JPH_CreateBody` and reads back transform |
| **Depends on** | M1 (Zig in binary), Jolt C API discovery |
| **Risk** | Jolt's C++ API is header-heavy; wrapping may need a C bridge header |

### M3: Native Hot-Reload Proven

| Aspect | Detail |
|---|---|
| **What** | A non-critical system (e.g., debug overlay) can be rebuilt and swapped at runtime without restarting the engine. |
| **Acceptance** | Change source, recompile shared library, press hotkey → new behavior visible |
| **Depends on** | M1 |
| **Pattern** | Madrigal Games / Traction Point: copy `.so` aside, `dlopen` new version, swap function pointer, `dlclose` old |

### M4: Minimal ECS Running

| Aspect | Detail |
|---|---|
| **What** | Zig-based ECS can spawn entities, add/remove components, and run systems. No Redot integration yet. |
| **Acceptance** | `zig test` runs an ECS query that modifies component data |
| **Depends on** | None (pure Zig, can be developed independently) |
| **Note** | This can start in parallel with M1-M3 — it is a standalone Zig subproject |

### M5: Hybrid Hot-Reload (Native + WASM)

| Aspect | Detail |
|---|---|
| **What** | Engine code hot-reloads natively; game mods load via WASM. Both paths work in the same host process. |
| **Acceptance** | Change engine system → hotkey reload. Load WASM mod → it spawns entities, reading input, writing log |
| **Depends on** | M3, WASM runtime integration |
| **Risk** | WASM instantiation overhead; capability enforcement gaps |

### M6: ECS + Scene Tree Coexistence

| Aspect | Detail |
|---|---|
| **What** | A game project can use ECS for some entities and the scene tree for others. Transforms synchronize on frame boundaries. |
| **Acceptance** | A Redot project with mixed ECS + scene tree nodes renders correctly |
| **Depends on** | M4, M2 (Jolt for ECS physics) |
| **Risk** | Synchronization bugs between ECS and scene tree state |

### M7: First zGameLib Component Consumed by Zodot

| Aspect | Detail |
|---|---|
| **What** | Zodot Zig code imports a zGameLib component (platform layer, Vulkan stack, or zClip) instead of re-implementing it. |
| **Acceptance** | `const zgame = @import("zgame");` works from within Zodot's Zig codebase |
| **Depends on** | zGameLib repo exists **and is already usable** — it does, so this is primarily about Zodot's `build.zig` adding the dependency |
| **Note** | zGameLib already provides platform, vulkan stack, zClip, Gpu/FrameRing, swapchain, surface. M7 is "Zodot successfully depends on one of them." |

### M8: First Component Extracted to zGameLib

| Aspect | Detail |
|---|---|
| **What** | A reusable system (e.g., hot-reload utilities, allocators, or math) moves from Zodot into zGameLib. Zodot pins a version. |
| **Acceptance** | zGameLib publishes the component; Zodot imports it via package manager |
| **Depends on** | M7 |
| **Signal** | A non-Zodot project stars or forks zGameLib |

### M9: Redot Project on Zig Systems

| Aspect | Detail |
|---|---|
| **What** | An existing Redot project (e.g., Kaetram or the MesaDriven demo) opens and plays with Zig ECS + Jolt integration active. |
| **Acceptance** | Project loads, player moves, physics responds |
| **Depends on** | M6, M5 |
| **Impact** | This is the ultimate validation of the migration strategy |

## Lean Guardrails

Use these criteria to decide where any new system or component belongs:

| Question | Stay in Zodot? | Move to zGameLib? |
|---|---|---|
| Does it depend on Redot-specific types (Variant, Node, SceneTree)? | Yes | No |
| Is it a general-purpose utility (allocator, math, hot-reload)? | No | Yes |
| Does it need deep editor integration? | Yes | No |
| Can its interface be described in pure Zig with no C++ dependency? | Maybe | Yes |
| Is it still experimental / rapidly changing? | Yes (extract later) | No |
| Would a non-Zodot project find it useful? | No | Yes |

**Bloat filter:** Before adding any new system, ask:
1. Does this solve a problem our target users actually have?
2. Is there a simpler way to solve it?
3. If every engine had this, would it matter?
4. Are we adding it because Unreal does?

## Open Questions

- Should the ECS be in Zodot or zGameLib initially? (Tracked in MIGRATION_PLAN.md)
- When does the zGameLib repo get created? (Likely after M1, before M7.)
- What is the trigger for moving from Phase 1 → Phase 2? (Suggested: M1 + M2 + M3 + M4 all complete.)

## Next Documents to Read

- [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) — Detailed phase-by-phase plan
- [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) — zGameLib boundary decisions
- [VISION.md](./VISION.md) — Why we are doing this
- [MISSION.md](./MISSION.md) — How we execute on it daily
