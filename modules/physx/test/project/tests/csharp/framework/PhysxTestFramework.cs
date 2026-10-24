// Core framework for the PhysX 5.8 integration/torture test suite.
//
// Tests are C# iterator coroutines: they yield to advance physics frames, so
// simulation tests read like linear scripts:
//
//     IEnumerator Fall(PhysxWorld w) {
//         var body = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(0, 10, 0));
//         yield return Wait.Frames(60);
//         Assert.Expect(w.Pos(body).Y < 9f, "body fell");
//     }
//
// Every test runs in its own world (fresh space); the runner guarantees
// cleanup runs (iterator finally-blocks execute on Dispose), records
// PASS/FAIL/SKIP/TIMEOUT with durations, prints the console summary, writes
// an incrementally-updated JSON report ("final" flag only set on clean
// completion — a native crash therefore leaves a visibly unfinished report),
// and quits with a non-zero exit code when required tests fail.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace PhysxTestProject.Tests;

public enum TestTier { Fast = 0, Extended = 1, Nightly = 2 }

public enum TestStatus { NotRun, Pass, Fail, Skip, Timeout, Crash }

/// <summary>A wait instruction yielded from a test coroutine.</summary>
public sealed class Wait
{
	public readonly ulong FrameCount;
	public readonly Func<bool> Until;
	public readonly string Desc;
	Wait(ulong frames, Func<bool> until, string desc) { FrameCount = frames; Until = until; Desc = desc; }

	public static Wait Frame() => new(1, null, "1 frame");
	public static Wait Frames(ulong n) => new(n, null, $"{n} frames");
	/// <summary>Polls every physics frame until <paramref name="pred"/> is true; fails the test after <paramref name="maxFrames"/>.</summary>
	public static Wait UntilOrFail(Func<bool> pred, ulong maxFrames, string what) =>
		new(0, () => pred(), $"{what} (budget {maxFrames})") { FailBudget = maxFrames, What = what };
	internal ulong FailBudget;
	internal string What;
}

/// <summary>Thrown by Require/Skip to abort the current test coroutine.</summary>
public sealed class TestAbortException : Exception
{
	public readonly bool AsSkip;
	public TestAbortException(string message, bool skip = false) : base(message) { AsSkip = skip; }
}

public sealed class TestSpec
{
	public string Id, Category, Description;
	public TestTier Tier = TestTier.Fast;
	public ulong MaxFrames = 3600; // per-test physics-frame budget (60 Hz -> 60 s)
	public Func<IEnumerator> Body;
}

public sealed class TestResult
{
	public string Id, Category, Description;
	public TestStatus Status = TestStatus.NotRun;
	public long DurationMs;
	public int Assertions;
	public int Failures;
	public List<string> Messages = new();
	public long? Seed;
	public string Diagnostics; // e.g. "seed=18372891 op=1837" for randomized tests
}

public static class Assert
{
	internal static TestResult Current;

	/// <summary>Records a pass/fail and keeps executing the test.</summary>
	public static void Expect(bool ok, string message)
	{
		if (Current == null) throw new InvalidOperationException("Expect() outside a running test");
		Current.Assertions++;
		if (!ok) { Current.Failures++; Current.Messages.Add(message); Log.Fail($"    [{Current.Id}] {message}"); }
	}

	public static void ExpectNear(double actual, double expected, double tol, string message)
	{
		Expect(Math.Abs(actual - expected) <= tol,
			$"{message} (expected ~{expected:F4} ±{tol:F4}, got {actual:F4})");
	}

	public static void ExpectVecNear(Vector3 actual, Vector3 expected, double tol, string message)
	{
		Expect((actual - expected).Length() <= tol,
			$"{message} (expected ~({expected.X:F3},{expected.Y:F3},{expected.Z:F3}) ±{tol:F3}, got ({actual.X:F3},{actual.Y:F3},{actual.Z:F3}))");
	}

	public static void ExpectFinite(Vector3 v, string message)
	{
		Expect(float.IsFinite(v.X) && float.IsFinite(v.Y) && float.IsFinite(v.Z),
			$"{message} (got ({v.X},{v.Y},{v.Z}))");
	}

	public static void ExpectTrue(bool b, string message) => Expect(b, message);
	public static void ExpectFalse(bool b, string message) => Expect(!b, message);

	/// <summary>Aborts the test as failed when the condition does not hold (for fatal preconditions).</summary>
	public static void Require(bool ok, string message)
	{
		if (!ok) throw new TestAbortException(message);
	}

	/// <summary>Aborts the test as skipped (unsupported functionality, missing feature).</summary>
	public static void Skip(string reason) => throw new TestAbortException(reason, skip: true);
}

public static class Log
{
	public static void Info(string s) => GD.Print(s);
	public static void Note(string s) => GD.Print("----  " + s);
	public static void Fail(string s) => GD.PrintErr(s);
}

/// <summary>Registers and executes tests sequentially with per-test state isolation.</summary>
public sealed class TestRunner
{
	readonly List<TestSpec> _specs = new();
	readonly List<TestResult> _results = new();
	readonly Dictionary<string, List<TestResult>> _byCategory = new();

	public string SeedInfo;
	public long GlobalSeed;

	int _index = -1;
	IEnumerator _coroutine;
	TestResult _current;
	TestSpec _currentSpec;
	Wait _wait;
	ulong _waitStart;
	Stopwatch _currentClock;
	string _jsonPath;
	TestTier _tier;

	/// <summary>The specs selected for this run (set by the entry point after CLI filtering).</summary>
	public List<TestSpec> Selector;

	public bool Finished { get; private set; }
	public string CurrentTest => _currentSpec?.Id ?? "-";

	public void Add(TestSpec spec) => _specs.Add(spec);

	public IEnumerable<string> Ids => _specs.Select(s => s.Id);
	public int Count => _specs.Count;

	public void Configure(TestTier tier, string jsonPath, long seed)
	{
		_tier = tier;
		_jsonPath = jsonPath;
		GlobalSeed = seed;
		SeedInfo = seed.ToString();
	}

	/// <summary>Selects specs matching CLI filters. Returns specs that were filtered out (reported as intentional non-runs).</summary>
	public List<TestSpec> Select(HashSet<string> categories, HashSet<string> testIds, TestTier tier) =>
		_specs.Where(s => (testIds == null || testIds.Count == 0 || testIds.Contains(s.Id))
					   && (categories == null || categories.Count == 0 || categories.Contains(s.Category))
					   && s.Tier <= tier).ToList();

	/// <summary>Advances the active test coroutine. Called once per physics frame from TestMain.</summary>
	public void Tick()
	{
		if (Finished) return;
		if (_current == null)
		{
			var run = Selector ?? _specs;
			_index++;
			if (_index >= run.Count) { FinishUp(); return; }
			StartTest(run[_index]);
			return;
		}
		StepTest();
	}

	/// <summary>Marks every not-yet-run selected test as TIMEOUT with the given reason and finalizes the run.</summary>
	public void AbortRemaining(string reason)
	{
		if (Finished) return;
		if (_current != null)
		{
			DisposeCoroutine();
			_current.DurationMs = _currentClock?.ElapsedMilliseconds ?? 0;
			_current.Status = TestStatus.Timeout;
			_current.Messages.Add("aborted: " + reason);
			FinalizeTest();
		}
		var run = Selector ?? _specs;
		for (int i = _index + 1; i < run.Count; i++)
		{
			var spec = run[i];
			_results.Add(new TestResult
			{
				Id = spec.Id,
				Category = spec.Category,
				Description = spec.Description,
				Status = TestStatus.Timeout,
				Messages = { "not run: " + reason },
			});
		}
		FinishUp();
	}

	void StartTest(TestSpec spec)
	{
		_currentSpec = spec;
		_current = new TestResult { Id = spec.Id, Category = spec.Category, Description = spec.Description, Seed = GlobalSeed };
		_currentClock = Stopwatch.StartNew();
		_testStartFrame = Engine.GetPhysicsFrames();
		Assert.Current = _current;
		Log.Note($"RUN  {spec.Id} [{spec.Category}] {spec.Description}");
		try
		{
			_coroutine = spec.Body();
		}
		catch (Exception e)
		{
			AbortStart(e);
			return;
		}
		_wait = null;
		StepTest();
	}

	void AbortStart(Exception e)
	{
		_current.DurationMs = _currentClock.ElapsedMilliseconds;
		if (e is TestAbortException tae && tae.AsSkip)
		{
			_current.Status = TestStatus.Skip;
			_current.Messages.Add(tae.Message);
		}
		else
		{
			_current.Status = TestStatus.Fail;
			_current.Failures++;
			_current.Messages.Add("setup threw: " + e.Message);
		}
		FinalizeTest();
	}

	void StepTest()
	{
		// Advance the active wait.
		if (_wait != null)
		{
			bool done;
			if (_wait.Until != null)
			{
				try { done = _wait.Until(); }
				catch (Exception e) { WaitFailed(e); return; }
				if (!done && Engine.GetPhysicsFrames() - _waitStart >= _wait.FailBudget)
				{
					_current.Failures++;
					_current.Messages.Add($"timed out waiting for: {_wait.What}");
					Log.Fail($"    [{_current.Id}] timed out waiting for: {_wait.What}");
					_wait = null; // keep executing so the test can report details
					return;
				}
			}
			else
			{
				done = Engine.GetPhysicsFrames() - _waitStart >= _wait.FrameCount;
			}
			if (!done) return;
			_wait = null;
		}

		try
		{
			if (_coroutine.MoveNext())
			{
				var yielded = _coroutine.Current;
				if (yielded is Wait w)
				{
					_wait = w;
					_waitStart = Engine.GetPhysicsFrames();
					if (w.Until != null) { w.FailBudget = Math.Max(w.FailBudget, 1); }
				}
				else if (yielded is int n) { _wait = Wait.Frames((ulong)Math.Max(1, n)); _waitStart = Engine.GetPhysicsFrames(); }
				else if (yielded == null) { _wait = Wait.Frame(); _waitStart = Engine.GetPhysicsFrames(); }
				// Any other yield value: resume next frame.
				else { _wait = Wait.Frame(); _waitStart = Engine.GetPhysicsFrames(); }

				if (Engine.GetPhysicsFrames() - _testStartFrame > (ulong)_currentSpec.MaxFrames)
				{
					_current.Status = TestStatus.Timeout;
					_current.Messages.Add($"exceeded per-test frame budget {_currentSpec.MaxFrames} (stuck in: {_wait?.Desc ?? "body"})");
					DisposeCoroutine();
					_current.DurationMs = _currentClock.ElapsedMilliseconds;
					FinalizeTest();
				}
			}
			else
			{
				_current.DurationMs = _currentClock.ElapsedMilliseconds;
				_current.Status = _current.Failures > 0 ? TestStatus.Fail : TestStatus.Pass;
				FinalizeTest();
			}
		}
		catch (TestAbortException tae)
		{
			DisposeCoroutine();
			_current.DurationMs = _currentClock.ElapsedMilliseconds;
			if (tae.AsSkip) { _current.Status = TestStatus.Skip; _current.Messages.Add("SKIP: " + tae.Message); }
			else { _current.Status = TestStatus.Fail; _current.Failures++; _current.Messages.Add("aborted: " + tae.Message); }
			FinalizeTest();
		}
		catch (Exception e)
		{
			DisposeCoroutine();
			_current.DurationMs = _currentClock.ElapsedMilliseconds;
			_current.Status = TestStatus.Fail;
			_current.Failures++;
			_current.Messages.Add("exception: " + e.GetType().Name + ": " + e.Message);
			Log.Fail($"    [{_current.Id}] exception {e.GetType().Name}: {e.Message}");
			FinalizeTest();
		}
	}

	void WaitFailed(Exception e)
	{
		DisposeCoroutine();
		_current.DurationMs = _currentClock.ElapsedMilliseconds;
		_current.Status = TestStatus.Fail;
		_current.Failures++;
		_current.Messages.Add($"predicate threw: {e.Message}");
		FinalizeTest();
	}

	void DisposeCoroutine()
	{
		try { (_coroutine as IDisposable)?.Dispose(); } catch (Exception e) { Log.Fail($"    [{_current.Id}] cleanup threw: {e.Message}"); }
		_coroutine = null;
	}

	ulong _testStartFrame;

	void FinalizeTest()
	{
		_results.Add(_current);
		if (!_byCategory.TryGetValue(_current.Category, out var list)) _byCategory[_current.Category] = list = new();
		list.Add(_current);
		string status = _current.Status.ToString().ToUpperInvariant();
		string extra = _current.Status == TestStatus.Pass ? "" : $"  ({string.Join("; ", _current.Messages.Take(3))})";
		Log.Info($"{(_current.Status == TestStatus.Pass ? "PASS" : _current.Status == TestStatus.Skip ? "SKIP" : "FAIL")}  {_current.Id}  " +
				 $"{_current.Assertions} asserts, {_current.DurationMs} ms{extra}");
		Assert.Current = null;
		_current = null;
		_currentSpec = null;
		_wait = null;
		WriteJson(false);
	}

	public void MarkCrash()
	{
		// Called by TestMain's process-exit hook if we die elsewhere; best effort.
		WriteJson(false);
	}

	void FinishUp()
	{
		Finished = true;
		WriteJson(true);
		PrintSummary();
	}

	void WriteJson(bool final)
	{
		if (string.IsNullOrEmpty(_jsonPath)) return;
		try
		{
			var dir = Path.GetDirectoryName(Path.GetFullPath(_jsonPath));
			if (!string.IsNullOrEmpty(dir)) Directory.CreateDirectory(dir);
			using var fs = File.Create(_jsonPath);
			using var writer = new Utf8JsonWriter(fs, new JsonWriterOptions { Indented = true });
			writer.WriteStartObject();
			writer.WriteString("suite", "csharp");
			writer.WriteBoolean("final", final);
			writer.WriteNumber("seed", GlobalSeed);
			writer.WriteString("engine", ProjectSettings.GetSettingWithOverride("physics/3d/physics_engine").ToString());
			writer.WriteNumber("physics_fps", Engine.GetPhysicsTicksPerSecond());
			writer.WriteStartArray("tests");
			foreach (var r in _results)
			{
				writer.WriteStartObject();
				writer.WriteString("id", r.Id);
				writer.WriteString("category", r.Category);
				writer.WriteString("description", r.Description);
				writer.WriteString("status", r.Status.ToString().ToLowerInvariant());
				writer.WriteNumber("duration_ms", r.DurationMs);
				writer.WriteNumber("assertions", r.Assertions);
				writer.WriteNumber("failures", r.Failures);
				if (r.Seed is long s) writer.WriteNumber("seed", s);
				if (r.Diagnostics != null) writer.WriteString("diagnostics", r.Diagnostics);
				writer.WriteStartArray("messages");
				foreach (var m in r.Messages) writer.WriteStringValue(m);
				writer.WriteEndArray();
				writer.WriteEndObject();
			}
			writer.WriteEndArray();
			writer.WriteEndObject();
		}
		catch (Exception e)
		{
			Log.Fail($"failed to write JSON report: {e.Message}");
		}
	}

	public int PrintSummary()
	{
		int total = _results.Count;
		int passed = _results.Count(r => r.Status == TestStatus.Pass);
		int failed = _results.Count(r => r.Status == TestStatus.Fail || r.Status == TestStatus.Timeout || r.Status == TestStatus.Crash);
		int skipped = _results.Count(r => r.Status == TestStatus.Skip);
		int asserts = _results.Sum(r => r.Assertions);

		var sb = new System.Text.StringBuilder();
		sb.AppendLine("====================================================");
		sb.AppendLine("  Godot PhysX 5.8 Integration Test Suite (C#/.NET)");
		sb.AppendLine("====================================================");
		sb.AppendLine($"  Backend: {ProjectSettings.GetSettingWithOverride("physics/3d/physics_engine")}    Physics FPS: {Engine.GetPhysicsTicksPerSecond()}    Seed: {GlobalSeed}");
		foreach (var (cat, list) in _byCategory)
		{
			int p = list.Count(r => r.Status == TestStatus.Pass);
			string st = list.All(r => r.Status == TestStatus.Pass) ? "PASS" :
						list.Any(r => r.Status == TestStatus.Fail || r.Status == TestStatus.Timeout || r.Status == TestStatus.Crash) ? "FAIL" : "SKIP";
			sb.AppendLine($"  {cat,-16} {st}  {p}/{list.Count}");
		}
		sb.AppendLine($"TOTAL: {total}   PASSED: {passed}   FAILED: {failed}   SKIPPED: {skipped}   ASSERTIONS: {asserts}");
		if (failed > 0)
		{
			sb.AppendLine("FAILURES:");
			foreach (var r in _results.Where(r => r.Status == TestStatus.Fail || r.Status == TestStatus.Timeout))
			{
				foreach (var m in r.Messages) sb.AppendLine($"  [{r.Id}] {m}");
				if (r.Diagnostics != null) sb.AppendLine($"  [{r.Id}] {r.Diagnostics}");
			}
		}
		sb.AppendLine("====================================================");
		sb.AppendLine($"RESULT: {(failed > 0 ? "FAILURE" : "SUCCESS")}");
		sb.AppendLine("====================================================");
		Log.Info(sb.ToString());
		return failed > 0 ? 1 : 0;
	}

	public int ExitCode => _results.Any(r => r.Status is TestStatus.Fail or TestStatus.Timeout or TestStatus.Crash) ? 1 : 0;
}
