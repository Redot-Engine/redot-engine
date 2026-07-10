# Key Patterns & Conventions

This document describes the coding patterns and architectural conventions that Zodot plans to follow. These are forward-looking — they apply to new Zig code written during the migration.

---

## 1. Data-Oriented Design / ECS Principles

### Core Idea

Systems iterate over flat arrays of components, not deeply nested object trees. Cache misses are minimized; iteration is predictable.

### Pattern

```zig
// GOOD — ECS query with flat arrays
fn physics_system(queries: *QueryStore) void {
    const positions = queries.read(Position);
    const velocities = queries.read(Velocity);
    for (positions, velocities) |*pos, *vel| {
        pos.* = pos.* + vel.* * delta_time;
    }
}

// BAD — Godot-style node tree traversal
// for each child in root.get_children():
//     child.get_node("Collision").position += ...
```

### Rules

- Systems are **pure functions** of their component inputs.
- No implicit global state. All data flows through ECS storage.
- Components are **plain structs** (no methods beyond constructors / defaults).
- Systems receive a **read/write declaration** so the scheduler can parallelize.

---

## 2. Hot-Reloading Strategy

### Two Tiers

| Tier | Technology | Use Case | Reload Speed |
|---|---|---|---|
| **Native** | Zig's `@export` + dlopen/dlsym | Engine systems, core | < 500ms |
| **Sandboxed** | WASM (wasmtime / wasm3) | Game scripts, mods | < 50ms |

### Native Hot-Reload Pattern

```zig
// Each reloadable system exports an update function
const System = struct {
    name: [:0]const u8,
    update: *const fn (world: *World, dt: f32) void,
    deinit: *const fn () void,
};

comptime {
    @export(physics_update, .{ .name = "sys_physics_update" });
    @export(physics_deinit, .{ .name = "sys_physics_deinit" });
}
```

### WASM Sandbox Pattern

```zig
const ModAPI = struct {
    allocate: *const fn (size: usize) callconv(.C) ?[*]u8,
    spawn_entity: *const fn (template_id: u32) callconv(.C) EntityID,
    write_log: *const fn (msg: [*:0]u8) callconv(.C) void,
    // No raw memory access, no file system — only declared capabilities
};
```

---

## 3. Raw-First / Opt-In Philosophy

Inspired by zGameLib design goals. The default should be simple, explicit, and allocation-minimizing.

### Guidelines

```zig
// PREFERRED — caller controls allocation
pub fn compute_contacts(arena: *ArenaAllocator, bodies: []const Body) []Contact {
    // Use arena for temporary data
}

// ACCEPTABLE — allocator parameter
pub fn load_mesh(allocator: Allocator, path: []const u8) !Mesh {
    // Explicit allocator, caller owns cleanup
}

// AVOID — global allocations, hidden mallocs
// pub fn load_mesh(path: string) Mesh  // hidden allocation
```

- All public APIs take an **explicit allocator** (or arena).
- Prefer **slice returns** over dynamic arrays where size is known.
- No global singletons. Pass dependencies explicitly.
- Zig's `comptime` is used for code generation, not to hide complexity.

---

## 4. Modability & Sandboxing Approach

### Capability Model

Each WASM mod declares what it needs at load time:

```zig
const ModManifest = struct {
    name: []const u8,
    version: SemVer,
    permissions: struct {
        networking: bool = false,
        file_read: [:0]const u8 = "",  // specific path or ""
        file_write: [:0]const u8 = "",
        spawn_entities: bool = false,
        debug_log: bool = false,
    },
};
```

The engine enforces these at runtime via the WASM sandbox. Host functions check capabilities before doing work.

### ECS-Only Mod API

Mods cannot touch the scene tree or Variant system. They interact exclusively through ECS:

- Spawn entities with component templates.
- Add/remove components.
- Register system callbacks.

This ensures mods cannot corrupt engine state.

---

## 5. Obfuscation & Release Engineering

### Techniques

```zig
// 1. Comptime string obfuscation
fn obfuscate(comptime s: []const u8) [s.len]u8 {
    var result: [s.len]u8 = undefined;
    for (s, 0..) |c, i| result[i] = c ^ 0xAA;
    return result;
}

// 2. Strip all symbols at release
// build.zig: exe.strip = true;

// 3. No GDScript strings in release binary
// (GDScript module not compiled in)
```

### Release Build Goals

- Strip all non-essential symbols (Zig's `strip`).
- Obfuscate string literals (function names, error messages) via comptime.
- No embedded script source (WASM mods are separate binaries).
- Link-time optimization (LTO) to flatten call graphs.

---

## 6. Error Handling & Safety in Zig

### Pattern

```zig
// Prefer error unions over optional returns
pub fn load_shader(device: *Device, path: []const u8) !Shader {
    const file = try fs.open(path);
    defer file.close();
    const data = try file.readAll(allocator);
    return try device.createShader(data);
}

// Use typed errors, not opaque error sets
const ShaderError = error{
    FileNotFound,
    InvalidFormat,
    CompilationFailed,
};

pub fn compile(…) ShaderError!CompiledShader { … }
```

### Safety Rules

- `undefined` is never used for "null state" — use `?Type` instead.
- No `@ptrCast` from integer unless alignment is guaranteed.
- No global `var` without explicit synchronization.
- Panic handler reports file/line and stack trace, even in release builds.

---

## 7. Conventions for zGameLib Extraction

Every system should be written as if it will be extracted into zGameLib later.

### Checklist for New Zig Systems

1. Does this system depend on any Zodot-specific type (e.g., `SceneTree`, `Variant`, `RedotConfig`)?
   - If yes, abstract it behind a generic interface.
2. Does this system use `std.heap.GeneralPurposeAllocator` or similar?
   - Yes? Good. Pass allocator explicitly.
3. Does this system have a clear `.init()` / `.deinit()` lifecycle?
   - Must have. No implicit global init.
4. Are all public types namespaced under a clear module?
   - `physics.contact`, not `PhysicsContact`.
5. Does every public function take an explicit allocator?
   - If no, refactor before extraction. Hidden allocations prevent zGameLib from controlling memory.
6. Are there any references to Zodot globals (`_globals`, `Engine::get_singleton()`, etc.)?
   - If yes, the system is not extractable. Pass these as parameters.
7. Does the system have complete test coverage (Level 1 unit tests)?
   - If no, extraction is blocked. Tests validate the API before publishing.
8. Is the API documented with `///` doc comments including an example?
   - If no, the API is not ready for external consumption.

### Module Layout Pattern (Planned)

```zig
// zGameLib/physics/contact.zig
pub const Contact = struct { … };
pub const ContactManifold = struct { … };

// zGameLib/physics/broadphase.zig
pub const Broadphase = struct { … };

// Zodot-specific: wraps zGameLib physics into engine
// src/physics_wrapper.zig
const zg = @import("zgameLib");
pub const PhysicsServer = struct {
    broadphase: zg.physics.Broadphase,
    // Zodot-specific integration code only
};
```

---

## 8. TDD Patterns for Zig Code

### Philosophy

Test the behavior, not the implementation. Prefer fast, deterministic, headless tests. Every module should be testable with `zig test` alone — no engine binary required.

### Test Structure

```zig
// In the same file as the implementation:
test "empty world has no entities" {
    var world = try World.init(testing.allocator);
    defer world.deinit();
    try testing.expectEqual(@as(usize, 0), world.entityCount());
}

test "spawning entity increments count" {
    var world = try World.init(testing.allocator);
    defer world.deinit();
    _ = try world.spawn(.{});
    try testing.expectEqual(@as(usize, 1), world.entityCount());
}
```

### Rules

| Rule | Why |
|---|---|
| One `test` block per behavior | Clear failure messages, easy to isolate |
| Use `testing.allocator` | Detects leaks, no hidden state |
| `defer` cleanup in the test | Ensures teardown even on panic |
| Test error paths explicitly | `try testing.expectError(error.FileNotFound, shader.compile(...))` |
| No test depends on another | Tests run in arbitrary order |
| No external resources (disk, network) | Tests must run in a container headlessly |

### Test Hierarchy

```
Level 1 — Unit tests (inline `test` blocks)
  ├── Pure Zig logic, no engine dependencies
  ├── Run with: `zig test src/ecs/world.zig`
  └── Fastest, container-safe

Level 2 — Integration tests (tests/ directory)
  ├── Cross-module behavior (e.g. ECS + physics wrapper)
  ├── Run with: `zig build test`
  └── May need minimal FFI stubs

Level 3 — Behavioral / display tests
  ├── Needs a display + Vulkan/GL driver
  ├── Run with: `zig build test-tdd`
  └── Runs under Xvfb in CI, skipped when no display
```

### CI Gate

Before merge, all Level 1 + Level 2 tests must pass. Level 3 tests run in CI under Xvfb.

## 9. Coding Conventions (Zig)

These apply once Zig code is introduced:

| Convention | Rule |
|---|---|
| **Naming** | Types: `PascalCase`. Functions/vars: `snake_case`. Constants: `SCREAMING_SNAKE`. |
| **File per type** | One public type per file, named after the type. |
| **Imports** | Group: std → thirdparty → internal. Use `@import` not `@cInclude` for Zig deps. |
| **Error handling** | Use `!` error unions. Avoid `catch unreachable` in production code. |
| **Documentation** | `///` doc comments on all public APIs. Examples in doc comments. |
| **Allocators** | Arena for frame-temporary, `GeneralPurposeAllocator` for persistent. |
| **comptime** | Use for code generation, type specialization, and assertions — not for logic branching. |

## Related Documents

- [ARCHITECTURE_OVERVIEW.md](./ARCHITECTURE_OVERVIEW.md) — How patterns map to engine layers
- [DECISION_RATIONALE.md](./DECISION_RATIONALE.md) — Why these patterns were chosen
- [PROJECT_OVERVIEW.md](./PROJECT_OVERVIEW.md) — High-level vision

## Questions This Document Answers

- What ECS patterns should new Zig systems follow?
- How does hot-reload work (native vs. WASM)?
- What does "raw-first / opt-in" mean in practice?
- How are mods sandboxed?
- What obfuscation techniques are planned?
- How are errors handled in Zig code?
- How should code be structured for future extraction into zGameLib?
