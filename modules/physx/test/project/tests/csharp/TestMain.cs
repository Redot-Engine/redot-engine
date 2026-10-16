// Entry point for the exhaustive C#/.NET PhysX 5.8 integration test suite.
//
//   dotnet build modules/physx/test/project/physx_ci.csproj
//   <godot> --headless --fixed-fps 60 --path modules/physx/test/project \
//       --script res://tests/csharp/TestMain.cs -- --tier=fast --json=report.json
//
// User args (after "--"): --tier=fast|extended|nightly, --category=<name>,
// --test=<ID>, --seed=<int>, --json=<path>, --list, --max-seconds=<n>.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;

namespace PhysxTestProject.Tests;

/// <summary>Category-scoped registration helper.</summary>
public sealed class SuiteBuilder
{
	readonly TestRunner _runner;
	readonly string _category;
	public SuiteBuilder(TestRunner runner, string category) { _runner = runner; _category = category; }

	public void Add(string id, string description, Func<IEnumerator> body,
					TestTier tier = TestTier.Fast, ulong maxFrames = 3600)
	{
		_runner.Add(new TestSpec
		{
			Id = id,
			Category = _category,
			Description = description,
			Tier = tier,
			MaxFrames = maxFrames,
			Body = body,
		});
	}
}

public partial class TestMain : SceneTree
{
	TestRunner _runner = new();
	readonly Stopwatch _wallClock = Stopwatch.StartNew();
	double _maxSeconds = 1800; // hard wall-clock cap; CI driver also enforces its own timeout
	bool _done;

	public override void _Initialize()
	{
		// Optional async stepping toggle for CI: PHYSX_ASYNC_STEP=1 flips the
		// project setting before the first tick (the server reads it live per
		// step, so a runtime set works).
		if (OS.GetEnvironment("PHYSX_ASYNC_STEP") == "1")
		{
			ProjectSettings.SetSetting("physics/physx_3d/simulation/async_step", true);
			GD.Print("----  PHYSX_ASYNC_STEP=1: async stepping enabled");
		}
		long seed = 123456;
		string json = null;
		var tier = TestTier.Fast;
		var categories = new HashSet<string>();
		var tests = new HashSet<string>();
		bool list = false;

		foreach (var arg in OS.GetCmdlineUserArgs())
		{
			string a = arg.TrimStart('-');
			int eq = a.IndexOf('=');
			string key = eq >= 0 ? a[..eq] : a;
			string val = eq >= 0 ? a[(eq + 1)..] : "";
			switch (key)
			{
				case "tier": tier = val == "nightly" ? TestTier.Nightly : val == "extended" ? TestTier.Extended : TestTier.Fast; break;
				case "category": categories.Add(val); break;
				case "test": tests.Add(val); break;
				case "seed": seed = long.TryParse(val, out var s) ? s : seed; break;
				case "json": json = val; break;
				case "list": list = true; break;
				case "max-seconds": _maxSeconds = double.TryParse(val, out var m) ? m : _maxSeconds; break;
			}
		}

		RegisterAll(_runner);
		_runner.Configure(tier, json, seed);

		// Filter application: mark non-selected specs by rebuilding the run set.
		var selected = _runner.Select(categories, tests, tier);
		_runner.Selector = selected;

		GD.Print("====================================================");
		GD.Print("  Godot PhysX 5.8 Integration Test Suite (C#/.NET)");
		GD.Print($"  Engine: {ProjectSettings.GetSettingWithOverride("physics/3d/physics_engine")}    " +
				 $"Physics FPS: {Engine.GetPhysicsTicksPerSecond()}    Seed: {seed}    Tier: {tier}");
		GD.Print($"  Vehicle API route: {VehicleApi.AccessPath}");
		GD.Print("====================================================");

		if (list)
		{
			foreach (var s in selected) GD.Print($"  {s.Id,-20} [{s.Category}] tier={s.Tier}  {s.Description}");
			_done = true;
			Quit(0);
			return;
		}
		GD.Print($"  {selected.Count} tests selected of {_runner.Count} registered");
	}

	static void RegisterAll(TestRunner r)
	{
		SmokeTests.Register(new SuiteBuilder(r, "smoke"));
		FoundationTests.Register(new SuiteBuilder(r, "foundation"));
		BindingTests.Register(new SuiteBuilder(r, "bindings"));
		SpaceTests.Register(new SuiteBuilder(r, "spaces"));
		ShapeTests.Register(new SuiteBuilder(r, "shapes"));
		BodyTests.Register(new SuiteBuilder(r, "bodies"));
		CollisionTests.Register(new SuiteBuilder(r, "collision"));
		QueryTests.Register(new SuiteBuilder(r, "queries"));
		MotionTests.Register(new SuiteBuilder(r, "motion"));
		AreaTests.Register(new SuiteBuilder(r, "areas"));
		JointTests.Register(new SuiteBuilder(r, "joints"));
		VehicleTests.Register(new SuiteBuilder(r, "vehicles"));
		ArticulationTests.Register(new SuiteBuilder(r, "articulations"));
		SoftBodyTests.Register(new SuiteBuilder(r, "soft_bodies"));
		LifecycleTests.Register(new SuiteBuilder(r, "lifecycle"));
		EdgeCaseTests.Register(new SuiteBuilder(r, "edge_cases"));
		DeterminismTests.Register(new SuiteBuilder(r, "determinism"));
		StressTests.Register(new SuiteBuilder(r, "stress"));
		RegressionTests.Register(new SuiteBuilder(r, "regressions"));
	}

	public override bool _PhysicsProcess(double delta)
	{
		if (_done) return true;

		if (_wallClock.Elapsed.TotalSeconds > _maxSeconds)
		{
			GD.PrintErr($"wall-clock cap of {_maxSeconds}s reached; aborting remaining tests");
			_runner.AbortRemaining("wall-clock cap reached");
			return Finish();
		}

		try
		{
			_runner.Tick();
		}
		catch (Exception e)
		{
			// A framework-level exception (not a test-level one) is fatal but must
			// still produce a report and a failure exit code.
			GD.PrintErr($"runner-level exception: {e}");
			_runner.AbortRemaining("runner-level exception: " + e.Message);
			return Finish();
		}
		if (_runner.Finished) return Finish();
		return false;
	}

	bool Finish()
	{
		_done = true;
		int code = _runner.ExitCode;
		Quit(code);
		return true;
	}
}
