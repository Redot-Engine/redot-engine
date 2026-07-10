# Testing Strategy

## Philosophy

Test what can be tested **headlessly**. Prefer **fast, container-friendly tests** that run without a display, GPU, or human interaction. Every Zig module should be testable with `zig test` alone.

```
Test hierarchy:
  Level 1 — Unit tests (inline `test` blocks)    fastest, no deps
  Level 2 — Integration tests (tests/ directory)  cross-module, FFI stubs
  Level 3 — Behavioral / display tests            needs display + Vulkan/GL
```

## Test Levels

### Level 1: Unit Tests

Inline `test` blocks in the same file as the implementation. Pure Zig logic only — no engine dependencies, no FFI, no file system.

```zig
// src/ecs/world.zig
test "spawning entity returns valid ID" {
    var world = try World.init(testing.allocator);
    defer world.deinit();
    const id = try world.spawn(.{});
    try testing.expect(id != .invalid);
}

test "destroyed entity is removed from queries" {
    var world = try World.init(testing.allocator);
    defer world.deinit();
    const id = try world.spawn(.{});
    world.destroy(id);
    try testing.expectEqual(@as(usize, 0), world.queryCount());
}
```

**Run:** `zig test src/ecs/world.zig`
**CI:** Always. No special setup needed.

### Level 2: Integration Tests

Cross-module tests that may use FFI stubs or minimal test doubles. Live in `tests/` directory. Can include `@cImport` for wrapped C++ libraries, but should not require a display.

```zig
// tests/jolt_integration_test.zig
test "Jolt rigid body falls under gravity" {
    var world = try JoltWorld.init(testing.allocator);
    defer world.deinit();

    const body = try world.createRigidBody(.{
        .shape = .box,
        .position = .{ 0, 10, 0 },
    });
    defer world.destroyBody(body);

    for (0..60) |_| { world.step(1.0 / 60.0); }

    const pos = world.getBodyPosition(body);
    try testing.expect(pos[1] < 9.0); // fell
}
```

**Run:** `zig build test`
**CI:** Always. May need Vulkan loader (`libvulkan.so`) but not a GPU (software rendering works).

### Level 3: Behavioral / Display Tests

Tests that need a display, GPU, or real hardware driver. Run under **Xvfb** in CI. Skipped when no display is available.

```zig
// tests/gpu_test.zig
test "Gpu initializes and presents frame" {
    // Requires display + Vulkan loader
    if (builtin.os.tag == .linux and std.os.getenv("DISPLAY") == null) {
        return error.SkipZigTest;
    }
    // ...
}
```

**Run:** `zig build test-tdd`
**CI:** Ubuntu 24.04 runner with `xvfb-run` and Mesa software Vulkan (`lavapipe`).

## Container-Based Testing

### What Can Be Tested in Containers

| Test Type | Container? | Notes |
|---|---|---|
| Level 1 unit tests | ✅ Yes | Pure Zig, no system deps |
| Level 2 integration tests | ✅ Yes | Needs `libvulkan.so` + Mesa (`lavapipe`) in container |
| Level 3 display tests | ✅ Yes | Needs `xvfb-run` inside container |
| Full editor with GUI | ❌ No | Requires real display server / GPU passthrough |
| Platform-specific (Windows, macOS) | ⚠️ Limited | Cross-compilation only; native testing needs host OS |

### Example Dockerfile

```dockerfile
FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    zig \
    libvulkan-dev \
    mesa-vulkan-drivers \
    xvfb \
    pkg-config \
    libx11-dev \
    libxcursor-dev \
    libxinerama-dev \
    libgl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workspace
COPY . .

# Level 1 + 2 (no display needed)
RUN zig build test

# Level 3 (needs Xvfb)
RUN xvfb-run zig build test-tdd
```

### Podman / Docker Commands

```bash
# Build test container
podman build -t zodot-test -f Dockerfile.test .

# Run all tests
podman run --rm zodot-test

# Run specific test suite
podman run --rm zodot-test zig build test-integration

# Interactive session
podman run --rm -it zodot-test /bin/bash
```

## Bash Helper Scripts

### `scripts/test.sh` — Run all tests

```bash
#!/usr/bin/env bash
# Run all Zodot tests
set -euo pipefail

echo "=== Level 1: Unit tests ==="
zig test src/ecs/world.zig
zig test src/physics/body.zig

echo "=== Level 2: Integration tests ==="
zig build test

echo "=== Level 3: Display tests (if display available) ==="
if [ -n "${DISPLAY:-}" ] || [ -n "${WAYLAND_DISPLAY:-}" ]; then
    zig build test-tdd
elif command -v xvfb-run &>/dev/null; then
    xvfb-run zig build test-tdd
else
    echo "  Skipped — no display and no xvfb-run"
fi

echo "=== All tests passed ==="
```

### `scripts/test-container.sh` — Run tests in container

```bash
#!/usr/bin/env bash
# Run tests inside Podman/Docker container
set -euo pipefail

CONTAINER_RUNTIME="${CONTAINER_RUNTIME:-podman}"
IMAGE_NAME="${IMAGE_NAME:-zodot-test:latest}"

cd "$(dirname "$0")/.."

# Build image if not present
if ! $CONTAINER_RUNTIME image exists "$IMAGE_NAME" 2>/dev/null; then
    $CONTAINER_RUNTIME build -t "$IMAGE_NAME" -f Dockerfile.test .
fi

# Run tests
$CONTAINER_RUNTIME run --rm "$IMAGE_NAME" "$@"
```

### `scripts/test-focused.sh` — Run a specific test file

```bash
#!/usr/bin/env bash
# Run tests from a specific file with optional filter
# Usage: ./scripts/test-focused.sh src/ecs/world.zig [filter]
set -euo pipefail

FILE="${1:?Usage: $0 <file> [filter]}"
FILTER="${2:-}"

if [ -n "$FILTER" ]; then
    zig test "$FILE" --test-filter "$FILTER"
else
    zig test "$FILE"
fi
```

### `scripts/build-clean.sh` — Verify clean build

```bash
#!/usr/bin/env bash
# Verify the project builds cleanly with build.zig
set -euo pipefail

echo "=== Clean build verification ==="
zig build

echo "=== Binary exists ==="
ls -lh zig-out/bin/

echo "=== No Python build scripts invoked ==="
echo "(SCons is not used)"
```

## CI Pipeline

### GitHub Actions Workflow (Planned)

```yaml
name: Tests
on: [push, pull_request]

jobs:
  unit-and-integration:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
      - uses: mlugg/setup-zig@v1
        with:
          version: 0.16.0
      - run: sudo apt-get install libvulkan-dev mesa-vulkan-drivers
      - run: zig build test

  display-tests:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v6
      - uses: mlugg/setup-zig@v1
        with:
          version: 0.16.0
      - run: sudo apt-get install libvulkan-dev mesa-vulkan-drivers xvfb
      - run: xvfb-run zig build test-tdd
```

**Long-term:** Once SCons is removed, all engine testing is driven from Zig + container scripts. No Python for engine testing.

## What Cannot Be Tested Headlessly

| Test | Alternative |
|---|---|
| Full editor with GUI | Manual testing, or scripted GUI tests via `--headless` mode |
| Specific GPU hardware features | CI with physical GPU runner (future). For now, `lavapipe` software rasterizer |
| Platform-specific behavior (Windows registry, macOS bundles) | Native CI runners per platform |
| Real-time audio output | Verification of buffer processing logic via unit tests, no hardware output |

## Related Documents

- [CODING_CONVENTIONS.md](./CODING_CONVENTIONS.md) — TDD-focused conventions for Zig code
- [KEY_PATTERNS_AND_CONVENTIONS.md](./KEY_PATTERNS_AND_CONVENTIONS.md) — TDD patterns section
- [ONBOARDING.md](./ONBOARDING.md) — How to run tests today (SCons era)
