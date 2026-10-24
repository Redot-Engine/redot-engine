# PhysX source patches

`build_physx.py` applies every `*.patch` here (sorted by name) to the PhysX
checkout after clone/locate and before project generation. Each is a `-p1` diff
rooted at the repo (`a/physx/...`), and is skipped cleanly if already applied.

These are fixes that landed upstream *after* the pinned `PHYSX_REF`
(`ovphysx-0.5.11`). Drop a patch once the ref is bumped past it.

| Patch | Upstream | Fixes |
| --- | --- | --- |
| `0001-heightfield-gpu-boundary-crash.patch` | [PR #503](https://github.com/NVIDIA-Omniverse/PhysX/pull/503) / [issue #502](https://github.com/NVIDIA-Omniverse/PhysX/issues/502) | GPU sphere–height-field narrowphase reads ~8 GB out of bounds at a height-field rim triangle (the `BOUNDARY` adjacency sentinel is passed to `getTriangle()` unchecked) → `CUDA_ERROR_ILLEGAL_ADDRESS`, dead CUDA context. Latent until the bad address is unmapped, which a renderer sharing the GPU makes reliable. |
| `0002-gpu-turing-sm75-arch.patch` | none (build config) | The GPU arch list has no SASS for `sm_75` (Turing: RTX 20-series / GTX 16-series). Those cards JIT every PhysX CUDA kernel from PTX at load and run GPU dynamics ~8× slower. Adds `sm_75` to the SASS lists. |
| `0003-linux-clang-no-werror.patch` | none (build config) | The Linux clang build uses `-Werror -Weverything`, so every clang release that adds a warning breaks the SDK build (clang 22: `-Wmissing-include-dirs`, `-Wnrvo`, `-Wformat-signedness`). Drops `-Werror`; the warnings are still printed. |

---

Fork note (Phase 14 port, upstream da5ef59c84): this fork vendors PREBUILT
PhysX libraries under thirdparty/physx and does not build the SDK from
source, so these patches/presets are carried as reference material for a
from-source SDK rebuild (e.g. when refreshing the vendored SDK or building
for a platform without prebuilt libs). Apply 0003 before compiling the SDK
with clang, which otherwise treats PhysX's own warnings as errors.
