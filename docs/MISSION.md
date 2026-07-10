# Mission

**Incrementally and safely migrate Redot Engine from C++ to Zig, keeping existing projects working at every step, while extracting reusable components into zGameLib so the broader Zig game development ecosystem benefits.**

## The Three Commit Tests

Every commit to Zodot must pass at least one of these tests:

1. **Moves us closer to a Zig-native engine core** — new Zig code, C++ → Zig port with measurable benefit, or infrastructure that enables Zig adoption.
2. **Improves the bridge between C++ and Zig** — better `@cImport` wrappers, build system integration, or interop patterns that reduce friction.
3. **Extracts something reusable** — moving a component into zGameLib or improving a zGameLib interface so others benefit.

A commit that does none of these is probably noise.

## The zGameLib Relationship

We are building two things simultaneously, and the relationship is bidirectional from the start:

```
Current (Phase 0): zGameLib already provides:
  ─ Platform adapter (SDL3 windowing + input)
  ─ Vulkan stack adapter (vk + volk + VMA + shaderc)
  ─ zClip animation lib (sprite + skeletal from glTF)
  ─ Shared middleware: Gpu, FrameRing, swapchain, surface

Phase 1-2:         Zodot consumes zGameLib components for new Zig code.
                   Existing C++ stays. zGameLib gains new features
                   (math, allocators, hot-reload utils).

Phase 3:           Zodot extracts mature, non-engine-specific systems
                   back INTO zGameLib. Bidirectional flow stabilizes.

Long-term:         zGameLib is independently useful for non-Zodot projects.
                   Zodot = zGameLib + engine-specific layers (editor,
                   compat, mod loader, deep system integration).
```

### Operational Principles

| Principle | What It Means Day-to-Day |
|---|---|
| **Consume first** | New Zig code in Zodot uses `zgame.platform`, `zgame.vk`, etc. rather than re-implementing |
| **Don't duplicate** | If zGameLib has it, use it. If it's missing and general-purpose, consider adding it to zGameLib first |
| **Extract when stable** | A component moves to zGameLib only after its API is proven in Zodot |
| **Keep working** | No commit breaks existing C++ code paths. Old projects keep building |
| **Editor untouched** | The C++ editor stays as-is through Phase 1. No Zig in the editor workflow |

This is not NIH syndrome — it is deliberate. A strong zGameLib makes the entire Zig game ecosystem stronger.

## Cross-References

- [VISION.md](./VISION.md) — The "what we become" and core values
- [MIGRATION_PLAN.md](./MIGRATION_PLAN.md) — Phase-by-phase technical plan
- [EXTRACTION_PLAN.md](./EXTRACTION_PLAN.md) — Detailed zGameLib boundary plan
