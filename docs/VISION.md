# Vision

**Zodot becomes the leanest high-performance game engine that ships native Zig — a platform where developers get Godot-grade tooling with ECS-level runtime performance, hot-reload measured in milliseconds, and modding that is safe by default.**

We are not building the next Unreal Engine. We are building the engine we wish existed: fast to iterate in, easy to mod safely, transparent about what it does, and small enough that a single developer can understand most of the core.

## Core Values

### 1. Performance Through Data-Oriented Design

ECS is not a buzzword here — it is a cache-efficiency strategy. We replace the Godot scene tree for game logic with flat component arrays because pointer-chasing through `Node*` inheritance costs real frames.

```
Node tree:   Player → Character → KinematicBody → CollisionShape
             Each step: dereference pointer, cache miss.

ECS query:   for (positions, velocities) |pos, vel| { ... }
             Sequential memory access, prefetcher-friendly.
```

We do not rewrite working C++ for ideological purity. We port to Zig only when there is a measurable gain: hot-reload, safety, ECS architecture, or modability.

### 2. Modability & Safety

Two-tier hot-reload inspired by **Madrigal Games / Traction Point**:

| Tier | Technology | Purpose | Reload Speed |
|---|---|---|---|
| Native | Zig `@export` + dlopen | Engine systems, core gameplay | < 500ms |
| Sandboxed | WASM | User mods, community scripts | < 50ms |

Native hot-reload is for development speed. WASM is for shipping mods that cannot crash the host. Every WASM mod declares capabilities at load time — no implicit access to memory, files, or network.

### 3. Lean & Maintainable

We reject bloat explicitly. Every new system must answer:

- Does this belong in the engine core, or could it be a module / zGameLib component?
- Does it make the engine harder to understand for a single contributor?
- Are we adding this because Unreal has it, or because our users need it?

**Influences:**
- **Hazel** — Clean system separation, plugin thinking, strong hot-reload culture. But we avoid Hazel's trap of over-abstracting early.
- **Unreal** — Modular plugin system, excellent tooling. But we reject the "kitchen sink" philosophy. No forced feature bloat.
- **Madrigal / Traction Point** — Hot-reload-first mindset, explicit allocators, clear engine/game boundary. This is our closest real-world reference.

### 4. Raw-First Transparency

Inspired by zGameLib design philosophy: the default should be simple, explicit, and reveal what actually happens.

```zig
// GOOD — caller sees allocation
pub fn load_mesh(arena: *ArenaAllocator, path: []const u8) !Mesh

// AVOID — hidden allocation
pub fn load_mesh(path: string) Mesh  // Where is the allocator?
```

No global singletons. No hidden mallocs. Every allocation is visible in the call chain.

### 5. Long-Term Ecosystem Thinking

We are building two things simultaneously:

- **Zodot**: A complete game engine (editor + runtime).
- **zGameLib**: A separate, independently-useful Zig game library that already exists ([`private/zGameLib`](../zGameLib), v0.1.0, Apache-2.0).

The relationship is bidirectional. A strong zGameLib makes the entire Zig game ecosystem stronger. See [MISSION.md](./MISSION.md) for the operational details.

## Non-Goals / Anti-Vision

| What We Are NOT Doing | Why |
|---|---|
| Rewriting the editor in Zig | The existing Redot editor works. Editor UI is not the bottleneck. |
| Full physics rewrite | Jolt is battle-tested (Horizon Forbidden West, Tears of the Kingdom). Wrapping via `@cImport` costs near-zero performance. |
| Chasing Godot feature parity | If a Godot 4.x feature is not useful for our target audience, we skip it. |
| Supporting every platform equally | Linux + Windows + macOS + Web are first-class. Consoles are partner-only. |
| Building a "Unreal killer" | We are building a focused, lean engine. If you need AAA cinematic tools, use Unreal. |
| Replacing GDScript in Phase 1 | WASM mod API replaces GDScript long-term, but GDScript compatibility stays during migration. |

## Open Questions

- When does the editor need Zig integration? (Not Phase 1, likely Phase 2.)
- How do we handle the GDScript → WASM transition for existing projects?
- What is the minimum Vulkan abstraction zGameLib should provide before Zodot depends on it?

## Next Documents to Read

- [MISSION.md](./MISSION.md) — What we do every day to realize this vision
- [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) — How we get there, phase by phase
- [ROADMAP.md](./ROADMAP.md) — Milestone-driven timeline
