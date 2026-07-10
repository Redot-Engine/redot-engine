# Coding Conventions

This document establishes coding conventions for both C++ (remaining/modernized parts of the Redot fork) and new Zig code. These rules are designed for long-term maintainability of a mixed C++/Zig codebase.

---

## C++ Code (Remaining / Modernized Parts)

We follow the **Google C++ Style Guide** ([link](https://google.github.io/styleguide/cppguide.html)) with the following adoptions and exceptions:

### Adopted from Google Style

| Rule | Detail |
|---|---|
| **Naming** | Classes: `CamelCase`. Functions: `CamelCase`. Variables: `snake_case`. Class members: `snake_case_` with trailing underscore. Constants: `kCamelCase`. |
| **Namespaces** | Use `namespace` with small scope. No `using namespace` in headers. |
| **Includes** | In order: related header → C system headers → C++ std → external libs → internal headers. Each group separated by blank line. |
| **Comments** | `//` for inline. `/* */` for block. Describe *why*, not *what*. |
| **Formatting** | **LLVM style** with `AccessModifierOffset: -4` (already in `.clang-format`). |
| **Header guards** | `#ifndef PATH_TO_FILE_H`, `#define PATH_TO_FILE_H`, `#endif // PATH_TO_FILE_H` (enforced by pre-commit). |
| **Const correctness** | Mark methods and parameters `const` whenever possible. |
| **nullptr** | Use `nullptr`, never `NULL` or `0`. |
| **auto** | Use only when the type is obvious from context or for iterators. |
| **RTTI** | Not used. `dynamic_cast` is forbidden. |
| **Exceptions** | Forbidden (`-fno-exceptions`). See DECISION_RATIONALE. |

### Exceptions to Google Style

| Google Rule | Our Rule | Why |
|---|---|---|
| 80-column line limit | **120 columns** (`.clang-format` setting) | Godot/Redot codebase uses 120; reduces artificial wrapping |
| `using namespace` forbidden | Permitted in `.cpp` files for `godot` namespace | Matches existing patterns |
| `// namespace comments` | Optional | Not enforced by `.clang-format` |
| `#pragma once` | Not used | We use traditional include guards (enforced by pre-commit) |
| C++ exceptions | Forbidden entirely | Saves ~20% binary size (see SConstruct) |

### STL Usage

**Do not use STL containers in engine code.** Use the project's custom containers:

| Redot Type | STL Equivalent |
|---|---|
| `Vector<T>` | `std::vector` |
| `List<T>` | `std::list` |
| `Map<K, V>` | `std::map` |
| `Set<T>` | `std::set` |
| `String` | `std::string` |
| `Ref<T>` | `std::shared_ptr` |

STL is permitted only in:
- Tooling scripts (Python)
- Thirdparty libraries (vendored)
- GDExtension interface code (API boundary)

### C++ File Layout

```
// Copyright header (MIT)
// Brief file-level comment (1-3 lines)

#include "header.h"

#include <algorithm>

#include "core/io/file.h"
#include "scene/node.h"

namespace zodot {

// Public API first, private details last
void MyClass::do_thing() {
    // implementation
}

} // namespace zodot
```

---

## Zig Code

### Naming

| Category | Convention | Example |
|---|---|---|
| Types | `PascalCase` | `RigidBody`, `PhysicsWorld` |
| Functions | `snake_case` | `create_body`, `step_simulation` |
| Variables | `snake_case` | `body_count`, `delta_time` |
| Constants | `SCREAMING_SNAKE` | `MAX_BODIES`, `DEFAULT_GRAVITY` |
| File names | `snake_case.zig` | `rigid_body.zig`, `physics_world.zig` |
| Test names | Descriptive string | `test "empty world has no entities"` |

### Module Layout

```
src/physics/
├── rigid_body.zig    // pub const RigidBody = struct { ... };
├── shape.zig         // pub const Shape = union(enum) { ... };
├── world.zig         // pub const World = struct { ... };
└── contact.zig       // pub const Contact = struct { ... };
```

One public type per file, named after the file.

### init / deinit Lifecycle

Every resource-owning type must provide `init` and `deinit`:

```zig
pub const Buffer = struct {
    handle: vk.Buffer,
    memory: vk.DeviceMemory,
    allocator: Allocator,

    pub fn init(allocator: Allocator, device: *Device, size: u64) !Buffer {
        // ...
    }

    pub fn deinit(self: *Buffer, device: *Device) void {
        device.destroyBuffer(self.handle, null);
        device.freeMemory(self.memory, null);
    }
};
```

### Allocator Convention

- Every public function that allocates takes an explicit `Allocator` parameter.
- Use `ArenaAllocator` for frame-temporary data (no individual frees).
- Use `GeneralPurposeAllocator` for persistent data.
- Never use a hidden global allocator.

```zig
// GOOD
pub fn load_scene(arena: *ArenaAllocator, path: []const u8) !Scene

// AVOID
pub fn load_scene(path: []const u8) !Scene  // where does the allocation come from?
```

### comptime Usage

Use `comptime` for:
- Code generation (e.g. generating ECS query functions per component type)
- Type specialization (e.g. `fn max(comptime T: type, a: T, b: T) T`)
- Compile-time assertions (`comptime std.debug.assert(...)`)

Do **not** use `comptime` for:
- Logic branching that could be runtime (makes code harder to read)
- Hiding allocation paths

### Error Handling

```zig
// Prefer error unions over optional returns
pub fn compile(src: []const u8) !Shader {
    const spirv = try compiler.compile(src);
    return try device.createShader(spirv);
}

// Use typed error sets for public APIs
pub const ShaderError = error{
    CompilationFailed,
    InvalidEntryPoint,
    DeviceLost,
};
```

Safety rules:
- `undefined` is never used where `?Type` would work.
- No `@ptrCast` from integer unless alignment is guaranteed.
- No global `var` without explicit synchronization.
- Panic handler reports file/line even in release builds.

### Imports Ordering

```zig
const std = @import("std");
const zgame = @import("zgame");

const ecs = @import("../ecs/world.zig");
const physics = @import("physics.zig");
```

Group: Zig std → zGameLib / external → internal. One blank line between groups.

---

## Mixed C++ / Zig FFI Boundaries

### C++ Export Headers (Bridge)

When exposing C++ code to Zig via `@cImport`, create a thin C-compatible header:

```c
// jolt_bridge.h — C-compatible wrapper for Jolt Physics
#ifdef __cplusplus
extern "C" {
#endif

typedef struct JoltWorld JoltWorld;

JoltWorld* jolt_world_create(void);
void jolt_world_step(JoltWorld* world, float dt);
void jolt_world_destroy(JoltWorld* world);

#ifdef __cplusplus
}
#endif
```

Rules:
- Use C calling convention (`extern "C"`).
- Pass only C-compatible types (pointers, integers, floats, structs without methods).
- No exceptions across the boundary.
- No C++ objects by value across FFI.

### Zig Side

```zig
const jolt = @cImport({
    @cInclude("jolt_bridge.h");
});

pub const World = struct {
    handle: *jolt.JoltWorld,

    pub fn init() !World {
        const h = jolt.jolt_world_create() orelse return error.InitFailed;
        return .{ .handle = h };
    }

    pub fn step(self: *World, dt: f32) void {
        jolt.jolt_world_step(self.handle, dt);
    }

    pub fn deinit(self: *World) void {
        jolt.jolt_world_destroy(self.handle);
    }
};
```

### Naming at FFI Boundary

| Side | Convention | Example |
|---|---|---|
| C/C++ bridge header | `snake_case` prefixed | `jolt_world_create` |
| Zig wrapper type | `PascalCase` | `pub const World = struct { ... };` |
| Zig wrapper method | `snake_case` | `pub fn step(self: *World, dt: f32) void` |

The Zig wrapper owns the C++ handle's lifecycle (`init`/`deinit`). The C++ side never calls into Zig for cleanup.

---

## Pre-Commit Hooks (Both Languages)

See `.pre-commit-config.yaml` for the full list. Key hooks:

| Hook | Language | Enforces |
|---|---|---|
| `clang-format` | C++, GLSL | LLVM style with offset -4 |
| `clang-tidy` | C++ | modernize/readability checks (manual stage) |
| `ruff` | Python | Formatting + lint (target-version py38) |
| `ruff-format` | Python | Auto-format |
| `mypy` | Python | Type checking |
| `codespell` | Text | Spelling (uses `custom_dict.txt`) |
| `header-guards` | C++ headers | Traditional `#ifndef` guards |

No Zig-specific pre-commit hooks exist yet. When Zig code enters the repo, consider adding `zig fmt` verification.

## Related Documents

- [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md) — Architectural patterns (ECS, hot-reload, modding)
- [TESTING_STRATEGY.md](./TESTING_STRATEGY.md) — How tests are structured and run
- [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) — Extraction checklist for zGameLib
