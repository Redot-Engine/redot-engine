# Decision Rationale

This document captures the reasoning behind the major architectural and strategic decisions made for the Zodot project.

---

## 1. Why Fork Redot Instead of Mainline Godot

| Factor | Redot | Godot (upstream) |
|---|---|---|
| **License** | MIT (unchanged from Godot) | MIT |
| **Community** | Focused, responsive maintainers | Large but slower moving |
| **C++20** | Already enabled | Not yet (Godot 4.x still targets C++17) |
| **Jolt integration** | Default, deeply tested | Optional module, less integrated |
| **Release cadence** | LTS-focused (26.x) | Feature-driven (4.x) |
| **MCP / AI integration** | Actively explored | Not a priority |

**Verdict:** Redot's C++20 baseline and Jolt-first stance align with Zodot's target architecture. Forking upstream Godot would require re-doing these modifications. Redot is a closer starting point.

**Trade-off:** We inherit Redot's delta from Godot (some API differences, version numbering). This is manageable.

---

## 2. Why Zig for Core Systems

Zig was chosen over C, Rust, and C++ for the following reasons:

| Criterion | Zig | Rust | C | C++ |
|---|---|---|---|---|
| **C FFI** | First-class (`@cImport`, ABI compat) | Via `unsafe` bindings | Native | Via `extern "C"` |
| **Build system** | Built-in (`build.zig`) | Cargo (package mgmt) | Make/CMake | CMake/Meson |
| **Allocator model** | Explicit, no hidden allocs | `alloc` via `#[global_allocator]` | Manual | Hidden (new/make_shared) |
| **comptime** | Comptime metaprogramming | Macros (limited) | Preprocessor | Templates (complex) |
| **Safety** | Optional runtime checks | Borrow checker (strict) | Manual | Manual (+ smart ptrs) |
| **Learning curve** | Moderate (small language) | Steep (borrow checker) | Moderate | High (large language) |
| **Hot-reload** | Trivial (`@export` + dlopen) | Complex (drop checks) | Manual | Manual |
| **Binary size** | Excellent (no runtime) | Larger (monomorphization) | Excellent | Large |

### Key References

- **[Traction Point](https://www.tractionpoint.games/)**: Commercial game studio shipping Zig game engine code in production.
- **[zig-gamedev](https://github.com/zig-gamedev/zig-gamedev)**: Production-quality Zig game development libraries (Vulkan, audio, etc.).
- **[ReX](https://github.com/nickmqb/rex)**: Zig engine example showing ECS + rendering patterns.

---

## 3. Why Keep Jolt Initially Instead of Rewriting Physics

### Considerations

- **Jolt is high quality**: Used in production (Horizon Forbidden West, Tears of the Kingdom). Its stepped job system and determinism are battle-tested.
- **Integration cost is low**: Jolt already builds as a C++ static lib. Exposing to Zig via `@cImport` is straightforward.
- **ECS-physics coupling is the real challenge**: Rewriting physics while also designing the ECS integration doubles the risk. Better to get ECS right first with Jolt as a known constant.
- **Zig-native physics is a future option**: Once ECS patterns are stable, a pure-Zig physics engine could replace Jolt. But this is not a current goal.

### Example: Jolt + ECS Integration Pattern

```zig
// Zig wraps Jolt's C++ interface
const JPH = @cImport({
    @cInclude("Jolt/Jolt.h");
    @cInclude("Jolt/Physics/PhysicsSystem.h");
});

pub fn PhysicsSystem.init(…) !PhysicsSystem {
    // Jolt's job system maps to Zig thread pool
    const jph_sys = JPH.JPH_PhysicsSystem_Create(…);
    return PhysicsSystem{ .handle = jph_sys };
}

// ECS systems call Jolt through this wrapper
pub fn step(ecs_world: *World, dt: f32) void {
    jph_sys.step(dt);
    // Read back body transforms → ECS Position components
}
```

---

## 4. Why Hybrid Hot-Reload (Native + WASM)

### Single-technology hot-reload is insufficient

| Technology | Strengths | Weaknesses |
|---|---|---|
| **Native (dlopen)** | Max performance, full engine access | Process-level isolation, crashes affect host |
| **WASM** | Full sandboxing, cross-platform, crash-safe | Limited performance, no SIMD access, syscall overhead |

**Hybrid approach**: Engine systems hot-reload natively (speed). Game scripts / mods run in WASM (safety).

This matches how AAA engines handle hot-reload (native DLL reload for engine, Lua/WASM for game code).

---

## 5. Why Self-Contained Zig Migration First Before zGameLib Coupling

### The Dependency Trap

If Zodot depends on zGameLib from day one, every change to zGameLib requires coordinated updates to both repos. During early rapid iteration, this is a productivity killer.

### Phased Approach

```
Phase 1: All Zig code lives in Zodot repo.
         Patterns are documented (see KEY_PATTERNS_AND_CONVENTIONS.md).
         No external package dependencies beyond Zig std.

Phase 2: Systems that are clearly generic are proto-extracted.
         A zGameLib repo is created, but Zodot pins a specific commit.

Phase 3: Mature systems are extracted into zGameLib.
         Zodot becomes a consumer.
         zGameLib is independently usable.
```

**Real-world parallel**: Bevy engine extracted `bevy_ecs`, `bevy_render`, etc. into separately versioned crates only after the engine and the ECS patterns were stable.

---

## 6. Why ECS Over Node Tree for Game Logic

| Concern | Node Tree (Godot) | ECS (Target) |
|---|---|---|
| **Cache behavior** | Pointer-chasing through inheritance hierarchy | Sequential iteration over component arrays |
| **Composability** | Single inheritance; mixins via signals | Entities are composition of components |
| **Serialization** | Scene files coupled to class hierarchy | Component blueprints, data-driven |
| **Parallelism** | Limited (tree traversal is serial) | Systems can run in parallel over disjoint component sets |
| **Modding** | Full engine API surface | Capability-controlled component access |

**Caveat**: The node tree stays for UI and editor tooling. ECS replaces only the *game logic* layer.

---

## 7. Why Not Rust

Rust was seriously considered but rejected:

1. **C++ interop is painful**: Every Godot API call needs an `unsafe` binding. Zig's `@cImport` makes this nearly seamless.
2. **Hot-reload is difficult**: Rust's drop check and lifetime model fight against dlopen-style reloading.
3. **Build time**: Rust's compilation is significantly slower than Zig's, which hurts iteration during an engine migration.
4. **Team background**: The project assumes a C/C++ audience. Zig's syntax is much closer to C than Rust's is.

---

## 8. Why Not Build a New Engine from Scratch

Building an engine from scratch would take 5-10 years to reach feature parity with Godot/Redot. By forking and gradually replacing C++ with Zig, we:

- Ship a working product at every phase.
- Use the Godot editor (best-in-class 2D/3D editor) throughout.
- Incrementally validate each subsystem replacement.
- Attract contributors who can use the engine while it's being rewritten.

---

## Related Documents

- [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md) — High-level strategy
- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — How decisions shape the architecture
- [DEPENDENCIES.md](./DEPENDENCIES.md) — Impact on dependency choices
- [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md) — How decisions translate to code patterns

## External References

- [Traction Point (Zig game engine in production)](https://www.tractionpoint.games/)
- [zig-gamedev project](https://github.com/zig-gamedev/zig-gamedev)
- [ReX: Zig game engine example](https://github.com/nickmqb/rex)
- [Jolt Physics official](https://github.com/jrouwe/JoltPhysics)
- [Bevy Engine (ECS pattern reference)](https://bevyengine.org/)
- [Godot Engine (upstream)](https://github.com/godotengine/godot)
- [Redot Engine (our fork base)](https://github.com/Redot-Engine/redot-engine)

## Questions This Document Answers

- Why fork Redot and not Godot?
- Why Zig instead of Rust, C, or C++?
- Why keep Jolt instead of rewriting physics?
- Why hybrid hot-reload instead of one approach?
- Why self-contained migration before depending on zGameLib?
- Why ECS instead of the Godot node tree?
- Why not build from scratch?
