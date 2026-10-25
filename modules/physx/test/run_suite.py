#!/usr/bin/env python3
"""CI driver for the PhysX 5.11 integration & torture test suite.

Usage (from the repository root):

    python modules/physx/test/run_suite.py --godot bin/redot.windows.editor.x86_64.console.exe
    python modules/physx/test/run_suite.py --godot <exe> --suite csharp --category joints
    python modules/physx/test/run_suite.py --godot <exe> --tier nightly
    python modules/physx/test/run_suite.py --godot <exe> --test PHYSX-CCD-001
    python modules/physx/test/run_suite.py --godot <exe> --test PHYSX-STRS-002 --seed 123456
    python modules/physx/test/run_suite.py --godot <exe> --suite flow --suite gpu

Responsibilities:
  * builds the C# test assembly (dotnet build) when running the C# suite;
  * launches the engine headless with the right --script entry point;
  * enforces a wall-clock timeout (kills the process -> TIMEOUT result);
  * detects native crashes (process dies without a final JSON marker) and
    reports CRASH -- a crash is never reported as a pass;
  * merges per-suite JSON reports into one machine-readable summary.

Exit codes: 0 = all required tests passed, 1 = failure/crash/timeout,
2 = driver-level error (bad invocation, missing engine, build failure).
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
PROJECT = HERE / "project"
CSHARP_SCRIPT = "res://tests/csharp/TestMain.cs"
GDSCRIPT_SCRIPT = "res://gdscript/gdscript_binding_tests.gd"
SMOKE_SCRIPT = "res://gdscript/physics_smoke_test.gd"
BLAST_SCRIPT = "res://gdscript/blast_smoke_test.gd"
BLAST_PROBE_SCRIPT = "res://gdscript/blast_probe_test.gd"
VEHICLE_SCRIPTS = [
    "res://gdscript/vehicle_server_check.gd",
    "res://gdscript/vehicle_node_test.gd",
    "res://gdscript/vehicle_gearbox_test.gd",
    "res://gdscript/vehicle_direction_test.gd",
	"res://gdscript/vehicle_exception_test.gd",
]
ASYNC_SCRIPT = "res://gdscript/async_stress_test.gd"
LIFECYCLE_SCRIPT = "res://gdscript/lifecycle_contracts_test.gd"
RUNTIME_SCRIPT = "res://gdscript/runtime_cost_test.gd"
FLOW_SCRIPT = "res://gdscript/flow_smoke_test.gd"
GPU_SCRIPT = "res://gdscript/gpu_smoke_test.gd"
WATER_PARITY_SCRIPT = "res://gdscript/water_parity_test.gd"
SOFT_ATTACH_SCRIPT = "res://gdscript/soft_body_attachment_test.gd"
CLOTH_CPU_SCRIPT = "res://gdscript/cloth_cpu_collision_test.gd"
CLOTH_GRAVITY_SCRIPT = "res://gdscript/skinned_cloth_gravity_test.gd"
JOINT_MOTOR_SCRIPT = "res://gdscript/joint_motor_wake_test.gd"
DEPEN_SCRIPT = "res://gdscript/depenetration_default_test.gd"

# Suites run by "--suite all" (the default, blocking CI tier): everything
# deterministic, CPU-only, and headless-safe. "flow" and "gpu" are opt-in —
# they need a usable CUDA/Vulkan runtime on the machine running them — and
# the CI job runs them as a separate, non-blocking step.
DEFAULT_SUITES = ["csharp", "gdscript", "smoke", "blast", "vehicle", "async", "lifecycle", "runtime", "cloth", "joints", "depenetration"]
SUITES = {
    "csharp": [CSHARP_SCRIPT],
    "gdscript": [GDSCRIPT_SCRIPT],
    "smoke": [SMOKE_SCRIPT],
    "blast": [BLAST_SCRIPT, BLAST_PROBE_SCRIPT],
    "vehicle": VEHICLE_SCRIPTS,
    "async": [ASYNC_SCRIPT],
    "lifecycle": [LIFECYCLE_SCRIPT],
    "runtime": [RUNTIME_SCRIPT],
    "flow": [FLOW_SCRIPT],
    "gpu": [GPU_SCRIPT, SOFT_ATTACH_SCRIPT],
    "water": [WATER_PARITY_SCRIPT],
    "cloth": [CLOTH_CPU_SCRIPT, CLOTH_GRAVITY_SCRIPT],
    "joints": [JOINT_MOTOR_SCRIPT],
    "depenetration": [DEPEN_SCRIPT],
}
# Suites whose tests carry categories and can run one category per process.
CATEGORIZABLE_SUITES = ("csharp", "gdscript")

DEFAULT_GODOT = pathlib.Path("bin/redot.windows.editor.x86_64.console.exe")

TIER_TIMEOUTS = {"fast": 1500, "extended": 3600, "nightly": 6 * 3600}


def parse_args():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--godot", default=str(DEFAULT_GODOT), help="engine binary (default: %(default)s)")
    ap.add_argument(
        "--suite",
        action="append",
        choices=sorted(SUITES.keys() | {"all"}),
        default=None,
        help="suite to run (repeatable; default: all = %s)" % " ".join(DEFAULT_SUITES),
    )
    ap.add_argument("--tier", choices=["fast", "extended", "nightly"], default="fast")
    ap.add_argument("--category", action="append", default=[], help="restrict to category (repeatable)")
    ap.add_argument("--test", action="append", default=[], help="restrict to test ID (repeatable)")
    ap.add_argument("--seed", type=int, default=None, help="seed for randomized tests")
    ap.add_argument(
        "--out",
        default=str(HERE / "test_results"),
        help="directory for JSON reports (default: modules/physx/test/test_results)",
    )
    ap.add_argument("--timeout", type=int, default=None, help="wall-clock seconds per process (overrides tier)")
    ap.add_argument("--skip-build", action="store_true", help="skip dotnet build of the C# assembly")
    ap.add_argument(
        "--per-category",
        action="store_true",
        help="run each category in its own process (crash isolation; default for nightly)",
    )
    ap.add_argument("--list", action="store_true", help="list tests instead of running them")
    return ap.parse_args()


def build_csharp() -> bool:
    csproj = PROJECT / "physx_ci.csproj"
    if not (PROJECT / ".godot").exists():
        # Stamp a minimal .godot dir so the SDK treats the folder as the project root.
        (PROJECT / ".godot").mkdir(exist_ok=True)
    print(f"[driver] building {csproj}")
    try:
        r = subprocess.run(
            ["dotnet", "build", str(csproj), "-v", "q", "-nologo"], cwd=str(PROJECT), capture_output=True, text=True
        )
    except FileNotFoundError:
        print(
            "[driver] ERROR: dotnet not found -- C# suite requires the .NET SDK "
            "(engine must be built with module_mono_enabled=yes)"
        )
        return False
    if r.returncode != 0:
        print("[driver] C# build FAILED:\n" + (r.stdout or "") + (r.stderr or ""))
        return False
    print("[driver] C# build OK")
    return True


def run_engine(godot: str, script: str, extra_user_args, out_json: pathlib.Path, timeout_s: int):
    """Runs one engine process. Returns (exit_code, status, json_dict_or_None)."""
    cmd = [godot, "--headless", "--fixed-fps", "60", "--path", str(PROJECT), "--script", script, "--"]
    cmd += extra_user_args + [f"--json={out_json}"]
    print(f"[driver] {' '.join(str(c) for c in cmd)}")
    if out_json.exists():
        out_json.unlink()
    try:
        p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout_s)
        out, err, code = p.stdout, p.stderr, p.returncode
        if out:
            print(out)
        if err:
            sys.stderr.write(err)
    except subprocess.TimeoutExpired as e:
        print(f"[driver] TIMEOUT after {timeout_s}s")
        out = (e.stdout or b"").decode(errors="replace") if isinstance(e.stdout, bytes) else (e.stdout or "")
        if out:
            print(out)
        code, status = None, "timeout"
        return code, status, read_json(out_json)

    report = read_json(out_json)
    # Bullet-proof rule: a run only counts as "done" when the engine wrote a
    # report AND flagged it final. Anything else (native crash, abort, exit 0
    # without a finished report) is a crash — never a silent pass.
    if report is None or not report.get("final", False):
        status = "crash"
    else:
        status = "done"
    return code, status, report


def read_json(path: pathlib.Path):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except Exception:
        return None


def count_status(report, status):
    if not report:
        return 0
    return sum(1 for t in report.get("tests", []) if t.get("status") == status)


def suite_totals(report, suite_name, status_override=None):
    tests = report.get("tests", []) if report else []
    return {
        "suite": suite_name,
        "status": status_override or ("done" if report and report.get("final") else "crash"),
        "total": len(tests),
        "passed": sum(1 for t in tests if t.get("status") == "pass"),
        "failed": sum(1 for t in tests if t.get("status") == "fail"),
        "skipped": sum(1 for t in tests if t.get("status") == "skip"),
        "timeout": sum(1 for t in tests if t.get("status") == "timeout"),
        "assertions": sum(t.get("assertions", 0) for t in tests),
        "engine_exit_code": report.get("engine_exit_code") if report else None,
        "tests": tests,
    }


def main():
    args = parse_args()
    godot = args.godot
    if not os.path.isfile(godot):
        print(f"[driver] ERROR: engine binary not found: {godot}")
        return 2
    godot = str(pathlib.Path(godot).resolve())

    out_dir = pathlib.Path(args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    timeout = args.timeout or TIER_TIMEOUTS[args.tier]
    per_category = args.per_category or (args.tier == "nightly")

    suites = []
    for requested in args.suite or ["all"]:
        for s in DEFAULT_SUITES if requested == "all" else [requested]:
            if s not in suites:
                suites.append(s)

    if "csharp" in suites and not args.skip_build:
        if not build_csharp():
            return 2

    user_args = [f"--tier={args.tier}"]
    for c in args.category:
        user_args += [f"--category={c}"]
    for t in args.test:
        user_args += [f"--test={t}"]
    if args.seed is not None:
        user_args += [f"--seed={args.seed}"]
    if args.list:
        user_args += ["--list"]

    all_reports = []
    failed_processes = 0

    for suite in suites:
        for script in SUITES[suite]:
            # Single-script suites keep the historical report_<suite>.json
            # name; multi-script suites tag each process with the script stem.
            if len(SUITES[suite]) == 1:
                out_json = out_dir / f"report_{suite}.json"
                label = suite
            else:
                out_json = out_dir / f"report_{suite}_{pathlib.Path(script).stem}.json"
                label = f"{suite}/{pathlib.Path(script).stem}"

            if per_category and suite in CATEGORIZABLE_SUITES and not args.list:
                categories = (
                    args.category if args.category else list_categories(godot, script, user_args, timeout, out_dir)
                )
                for cat in categories:
                    cat_args = [a for a in user_args if not a.startswith("--category=")] + [f"--category={cat}"]
                    # '*' (and None) are display fallbacks, not legal filename
                    # characters everywhere -- map them to 'all'.
                    cat_tag = "all" if cat in (None, "*") else str(cat)
                    cat_json = out_dir / f"report_{suite}_{cat_tag}.json"
                    code, status, report = run_engine(godot, script, cat_args, cat_json, timeout)
                    totals = suite_totals(report, f"{suite}/{cat}")
                    if report is not None:
                        report["engine_exit_code"] = code
                    all_reports.append((suite, cat, totals, report))
                    if status != "done" or code not in (0,):
                        failed_processes += 1
                    if status in ("crash", "timeout"):
                        print(f"[driver] {suite}/{cat}: {status.upper()} -- see partial JSON at {cat_json}")
            else:
                code, status, report = run_engine(godot, script, user_args, out_json, timeout)
                if report is not None:
                    report["engine_exit_code"] = code
                    with open(out_json, "w", encoding="utf-8") as f:
                        json.dump(report, f, indent=2)
                totals = suite_totals(report, label)
                all_reports.append(
                    (suite, "*" if len(SUITES[suite]) == 1 else pathlib.Path(script).stem, totals, report)
                )
                if status != "done" or code not in (0,):
                    failed_processes += 1
                if status in ("crash", "timeout"):
                    print(f"[driver] {label}: {status.upper()} -- partial JSON at {out_json}")

    # Merged machine-readable summary.
    merged = {
        "driver_version": 2,
        "tier": args.tier,
        "seed": args.seed,
        "godot": godot,
        "generated_unix": int(time.time()),
        "suites": [],
    }
    any_failed = failed_processes > 0
    for suite, cat, totals, report in all_reports:
        merged["suites"].append(totals)
        if totals["failed"] > 0 or totals["timeout"] > 0 or totals["status"] in ("crash", "timeout"):
            any_failed = True
    # Only a fully clean run is "final": consumers keying on this flag alone
    # must never mistake a crashed suite run for a completed one.
    merged["final"] = not any_failed

    merged_path = out_dir / "report_merged.json"
    with open(merged_path, "w", encoding="utf-8") as f:
        json.dump(merged, f, indent=2)

    print("\n" + "=" * 52)
    print("  MERGED RESULT")
    print("=" * 52)
    for suite, cat, totals, _ in all_reports:
        print(
            f"  {suite + ('/' + cat if cat != '*' else ''):<22} "
            f"{totals['status'].upper():<8} {totals['passed']}/{totals['total']} "
            f"(fail={totals['failed']} skip={totals['skipped']} timeout={totals['timeout']})"
        )
    print(f"  merged JSON: {merged_path}")
    print("=" * 52)
    print(f"RESULT: {'FAILURE' if any_failed else 'SUCCESS'}")
    return 1 if any_failed else 0


def list_categories(godot, script, user_args, timeout, out_dir):
    """Runs the suite once with --list to enumerate categories."""
    out_json = out_dir / "report_list.json"
    _, _, report = run_engine(godot, script, user_args + ["--list"], out_json, min(timeout, 300))
    cats = set()
    if report:
        for t in report.get("tests", []):
            cats.add(t.get("category"))
    if not cats:
        cats = {None}
    cats.discard(None)
    return sorted(cats) or ["*"]


if __name__ == "__main__":
    sys.exit(main())
