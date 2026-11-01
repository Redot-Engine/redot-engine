# PhysX 5.11 Backend — Automated Integration & Torture Test Suite

Automated, headless, CI-ready test suite for the custom PhysX 5.11
`PhysicsServer3D` backend (`modules/physx`). This is **not** a visual debugging
laboratory — there is no UI, no interactive controls, no rendered inspection.
Every check is programmatic and every run produces a console report, a
machine-readable JSON report, and a non-zero process exit code on failure.

The chain under test:

```
C# / .NET  ─┐
            ├─> Godot PhysicsServer3D API ─> custom PhysX module ─> RID/resource
GDScript  ─┘                                 management ─> Godot/PhysX conversion
                                              ─> PhysX 5.11 ─> simulation ─> sync
                                              ─> PhysicsServer3D results
```

## Layout

```
modules/physx/test/
  project/                        Godot project used by all suites (PhysX engine)
    project.godot                 selects physics/3d/physics_engine="PhysX"
    physx_ci.csproj               C# test assembly (Godot.NET.Sdk)
    tests/csharp/                 EXHAUSTIVE C#/.NET SUITE (canonical)
      framework/                  runner, assertions, world harness, reporting
      TestMain.cs                 SceneTree entry point
      FoundationTests.cs  SpaceTests.cs     ShapeTests.cs    BodyTests.cs
      CollisionTests.cs   QueryTests.cs     MotionTests.cs  AreaTests.cs
      JointTests.cs       VehicleTests.cs   SoftBodyTests.cs LifecycleTests.cs
      EdgeCaseTests.cs    DeterminismTests.cs StressTests.cs RegressionTests.cs
    gdscript/
      gdscript_binding_tests.gd   BINDING-VALIDATION SUITE (GDScript, smaller)
      physx_vehicle_bridge.gd     GDScript bridge for module vehicle API
      test_report.gd              shared --json= report writer for the
                                  standalone smoke suites below
      physics_smoke_test.gd       node-level drop/settle smoke test
      blast_smoke_test.gd         Blast suite: authoring + .tres round-trip +
                                  destructible radial damage (SKIP on blast=no)
      vehicle_server_check.gd     vehicle suite: server-RID drive + telemetry
      vehicle_node_test.gd        vehicle suite: node-level PhysXVehicle3D drive
      vehicle_gearbox_test.gd     vehicle suite: engine drive/autobox/reverse/neutral
      async_stress_test.gd        async suite: mid-flight mutation churn under
                                  async_step (bodies, shape transforms, params,
                                  exceptions, vehicle adopt/release)
      flow_smoke_test.gd          Flow suite: emitter -> simulate -> readback
                                  (SKIP on flow=no; needs a Vulkan device)
      gpu_smoke_test.gd           GPU suite: cloth GPU/CPU paths + PBD fluid
                                  (needs a CUDA device; opt-in)
      cloth_cpu_collision_test.gd CPU-fallback cloth drape/no-fall-through
                                  (behavioral, headless-safe, default tier)
      mpm_foam_test.gd            windowed-only MPM/foam/fluid experiments
      mpm_foam_stream_test.gd     (NOT wired into the driver: MPM needs a
      mpm_tank_containment_test.gd rendering device, i.e. a windowed run)
      async_churn_test.gd         windowed async-step GPU churn soak
      iteration_bench.gd          solver-iteration A/B benchmark (manual)
  run_suite.py                    CI driver: tiers, crash detection, JSON merge
  README.md                       this file
```

## Suites (what `--suite` selects; `--suite all` = the blocking default set)

| Suite     | Scripts | Purpose | Runs in the blocking default set |
|-----------|---------|---------|----------------------------------|
| `csharp`  | `tests/csharp/TestMain.cs` | Exhaustive behavioral/torture/regression backend testing (canonical) | yes |
| `gdscript`| `gdscript_binding_tests.gd` | Same backend through the GDScript binding layer | yes |
| `smoke`   | `physics_smoke_test.gd` | Node-level drop/settle end-to-end sanity | yes |
| `blast`   | `blast_smoke_test.gd` | Blast authoring + destructible lifecycle (skips itself on `blast=no` builds) | yes |
| `vehicle` | `vehicle_server_check.gd`, `vehicle_node_test.gd`, `vehicle_gearbox_test.gd`, `vehicle_direction_test.gd` | Server-RID + node vehicle stacks, engine drive/gearbox, 2W/track direction matrix | yes |
| `async`   | `async_stress_test.gd` | Mid-flight mutation churn under `async_step` (fetch-guard coverage) | yes |
| `lifecycle` | `lifecycle_contracts_test.gd` | Overlap exits, joint survival across mode changes, mid-solve frees, area free during dispatch | yes |
| `runtime` | `runtime_cost_test.gd` | Per-tick runtime cost ceiling of the server API surface | yes |
| `cloth`   | `cloth_cpu_collision_test.gd` | CPU-fallback cloth behavior: a forced-`SIM_CPU` sheet must drape over a static ball and never fall through the world (rest_info depenetration contract; the GPU cloth path is covered by the `gpu` suite) | yes |
| `platform_spin` | `platform_spin_test.gd` | A CharacterBody3D standing on a platform that stopped rotating and sleeps must not be carried | yes |
| `flow`    | `flow_smoke_test.gd` | NVIDIA Flow end-to-end (needs a Vulkan runtime) | opt-in |
| `gpu`     | `gpu_smoke_test.gd` | CUDA/GPU object family (needs a CUDA runtime) | opt-in |

Every wired suite parses `--json=<path>` and writes a `final: true` report —
the driver treats a missing report as `CRASH` even when the process exits 0.
The `mpm_*`, `async_churn_test.gd`, and `iteration_bench.gd` scripts need a
rendering device (windowed run) or are benchmarks, and stay manual.

A failure in C# but not GDScript (or vice versa) isolates a marshaling/binding
regression from an underlying physics regression. All suites write their own
JSON report; `run_suite.py` merges them and tags each with its suite origin.

## Running

All commands assume the repo root as CWD. `GODOT` is a C#-enabled build of this
engine (`module_mono_enabled=yes` for the C# suite). The GDScript suite runs on
any build with the PhysX module.

```bash
# Everything (fast tier): csharp + gdscript + smoke + blast + vehicle, with
# crash detection and merged JSON:
python modules/physx/test/run_suite.py --godot bin/redot.windows.editor.x86_64.console.exe

# Individual suites (repeatable --suite):
python modules/physx/test/run_suite.py --godot <exe> --suite csharp
python modules/physx/test/run_suite.py --godot <exe> --suite blast --suite vehicle

# GPU tier (opt-in; needs a CUDA/Vulkan runtime on this machine):
python modules/physx/test/run_suite.py --godot <exe> --suite flow --suite gpu

# Extended tier (fast + extended tests):
python modules/physx/test/run_suite.py --godot <exe> --tier extended

# Nightly tier (adds soak + heavy stress, one process per category):
python modules/physx/test/run_suite.py --godot <exe> --tier nightly

# Single category / single test / custom seed:
python modules/physx/test/run_suite.py --godot <exe> --suite csharp --category joints
python modules/physx/test/run_suite.py --godot <exe> --suite csharp --test PHYSX-BODY-053
python modules/physx/test/run_suite.py --godot <exe> --suite csharp --seed 987654321

# Deterministic randomized stress (reproducible; prints seed + op index on failure):
python modules/physx/test/run_suite.py --godot <exe> --suite csharp --test PHYSX-STRS-002 --seed 123456

# Regression suite only:
python modules/physx/test/run_suite.py --godot <exe> --suite csharp --category regressions
```

Direct engine invocation (what the driver does under the hood):

```bash
# C# (requires `dotnet build modules/physx/test/project/physx_ci.csproj` first):
<godot> --headless --fixed-fps 60 --path modules/physx/test/project \
    --script res://tests/csharp/TestMain.cs -- --tier=fast --json=out.json

# GDScript (works on non-C# builds too):
<godot> --headless --fixed-fps 60 --path modules/physx/test/project \
    --script res://gdscript/gdscript_binding_tests.gd -- --json=out.json
```

User arguments (after `--`): `--tier=fast|extended|nightly`,
`--category=<name>` (repeatable), `--test=<ID>` (repeatable), `--seed=<int>`,
`--json=<path>`, `--list`, `--max-seconds=<wall clock cap>`.

## Recommended CI tiers

| Tier     | Trigger      | Contents                                                     | Budget |
|----------|--------------|--------------------------------------------------------------|--------|
| `fast`   | every commit | default suites (csharp, gdscript, smoke, blast, vehicle, async) + a non-blocking GPU step (flow, gpu) | ~10 min |
| `extended` | pre-merge  | fast + extended-tagged tests (bigger stacks, longer runs)    | ~30 min |
| `nightly`| scheduled    | extended + soak (10,000+ frames) + randomized op storm, run per-category for crash isolation | hours |

`run_suite.py --tier nightly` launches each category as a **separate process**
so a native crash takes out only its category and is reported as `CRASH`,
never as a pass. A missing JSON `final: true` marker or a non-zero/negative
engine exit code is treated as `CRASH`/`TIMEOUT` respectively. The CI job
(`gitlab-ci-physx.yml`) runs the GPU suites (flow, gpu) as a trailing step
whose test failures are reported but do not fail the pipeline — a driver-level
error (exit 2) still does.

## Result model

Per test: `PASS` / `FAIL` / `SKIP` / `TIMEOUT` / `CRASH`. Console summary:

```
====================================================
  Godot PhysX 5.11 Integration Test Suite (C#)
====================================================
  Backend: PhysX    Physics FPS: 60    Seed: 123456
  Foundation       PASS   14/14
  Spaces           PASS   13/13
  ...
TOTAL: 331   PASSED: 329   FAILED: 1   SKIPPED: 1
FAILURES:
  [PHYSX-JOINT-017] expected anchor separation < 0.05, got 0.312
====================================================
RESULT: FAILURE
====================================================
```

JSON report (one entry per test): `id`, `category`, `status`, `duration_ms`,
`assertions`, `failures[]`, `seed` (when relevant), `diagnostics`, plus a
document-level `final` flag that is only `true` after the runner finished
cleanly (crash forensics for CI).

## Test philosophy

* **Behavior over storage.** Whenever a parameter is supposed to affect
  simulation, the test builds a physical experiment (drop, slide, spin,
  collide, constrain) and measures the outcome. Setter/getter round-trips are
  used only as API plumbing checks and are labeled as such.
* **Tolerances are justified** per assertion (solver softness, sleeping
  thresholds, 60 Hz quantization). No exact float equality on physical
  quantities; no tolerance wide enough to hide a broken behavior.
* **Determinism**: fixed timestep via `--fixed-fps 60`, explicit seeds for all
  randomized tests; failing randomized tests print `seed` and operation index.
* **Isolation**: every test runs in its own freshly created space, frees all
  resources in `finally`, and a shared "canary" pattern verifies that
  destroying one resource never corrupts unrelated live ones.
* **No fake tests**: functionality that is not implemented (soft-body
  simulation is currently an API skeleton in the module) is reported as
  `SKIP` with a reason, not silently passed.

## Coverage map of the original `physx_test_suite.gd` (predecessor)

The original single-file GDScript suite (~60 checks) was catalogued before
this suite was built:

* **Already strong (behavioral)** — ported and kept: backend selection, free
  fall + velocity gain, impulse Δv = J/m, constant force/torque integration,
  area gravity override effect, area monitoring (match/no-match/area-area),
  kinematic teleport, linear-Y axis lock, sleep/can-sleep/wake-on-impulse,
  ray/point/shape/rest-info queries, body_test_motion (hit/free/depenetration),
  layer filtering + collision exceptions via sweeps, contact reporting with
  up-normal, vehicle drive + 4-wheel telemetry, mode-switch fall, mid-sim
  frees (shared shape, jointed body, chassis, whole space).
* **API round-trip only** — kept as plumbing checks and paired with new
  behavioral counterparts: all shape data getters, body params (mass, bounce,
  friction, gravity scale, CCD flag, COM, inertia), all joint params/flags,
  area params, solver iterations, shape margin.
* **False-positive risks found** — `check(true, ...)` placeholder for
  exception add/remove (always passed); soft-body stubs counted as PASS; CCD
  verified as flag storage only, never as tunneling prevention.
* **Missing entirely** — added by this suite: pairwise shape collision
  matrix, rotated/transformed shape collision, shape mutation while shared,
  restitution & friction measurements, damping curves, mass-ratio response,
  gravity scale measurement, all axis-lock combinations, off-center force →
  angular response, joint constraint/limit/motor behavior + chains + loops,
  CCD tunneling regression, space isolation, stale-RID and invalid-input
  handling, numerical robustness (NaN/INF/large/small), deterministic replay,
  soak, seeded randomized stress, regression category, machine-readable
  output, per-test IDs/timeouts/crash isolation.

The old files (`physx_test_suite.gd`, `physx_quick_test.gd`,
`physx_vehicle_test.gd`, `PhysXVehicleEcsTest.cs`) were removed after their
coverage was absorbed into the two new suites.

## Test results & artifacts

All JSON reports and logs land in `modules/physx/test/test_results/`
(regardless of the directory the driver is invoked from) and that directory is
`.gitignore`d — reports are CI artifacts, never committed. `make_tarball.sh`
in `misc/scripts/` is the upstream release-packaging script, unrelated to
testing.

## Known module limitations encoded by tests

* `body_set_collision_priority` — stored and returned by the getter; weights
  `body_test_motion` recovery contacts (godot_physics parity).
* Pin joint `BIAS/DAMPING/IMPULSE_CLAMP`, joint solver priority — stored,
  round-tripped, not simulated; tested as storage only.
* Slider angular limits — stored only; linear limits enforced.
* `cast_motion` / `body_test_motion` follow godot_physics per-object overlap disregard: objects the query starts inside of are skipped (a forward blocker behind them still bounds the motion); an overlap deeper than the recovery slack is "stuck" (safe = unsafe = 0).
* Soft-body — API skeleton, no `PxDeformableVolume`; behavioral tests SKIP.
* Wind area params — stored, not applied.
