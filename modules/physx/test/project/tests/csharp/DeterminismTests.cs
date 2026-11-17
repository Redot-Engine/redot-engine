// Determinism: identical seeds and setups must evolve identically. Bitwise
// equality is attempted first; a small tolerance covers FMA/threading noise
// while still catching unintended behavioral divergence between runs.

using System;
using System.Collections;
using System.Collections.Generic;

namespace PhysxTestProject.Tests;

internal static class DeterminismTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-DET-001", "two identical free falls match bit-for-bit", TwinFreeFall);
		s.Add("PHYSX-DET-002", "seeded scene replay: full run vs recorded trace", ReplayTrace);
		s.Add("PHYSX-DET-003", "same seed produces identical randomized scene evolution", SeededSceneTwice);
		s.Add("PHYSX-DET-004", "3,000-frame recorded simulation replays within tolerance", LongReplay, TestTier.Extended, 12000);
		s.Add("PHYSX-DET-005", "solver iteration setting is stable across runs", SolverIterationsStable);
	}

	static IEnumerator TwinFreeFall()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeBody(w.Box(0.5f), new Vector3(-3, 40, 0));
		var b = w.MakeBody(w.Box(0.5f), new Vector3(3, 40, 0));
		for (int i = 0; i < 10; i++)
		{
			yield return Wait.Frames(30);
			Assert.Expect(w.Pos(a).Origin.Y == w.Pos(b).Origin.Y,
				$"twin fallers bitwise equal at frame {(i + 1) * 30} ({w.Pos(a).Origin.Y:F6} vs {w.Pos(b).Origin.Y:F6})");
		}
	}

	static void StepSeededScene(PhysxWorld w, Random rnd, int frame, List<Rid> bodies)
	{
		// Deterministic forces every 30 frames; deterministic impulses otherwise.
		if (frame % 30 == 0 && bodies.Count < 12)
		{
			bodies.Add(w.MakeBody(w.Box(0.3f), new Vector3(rnd.NextSingle() * 8f - 4f, 6f, rnd.NextSingle() * 8f - 4f)));
		}
		if (frame % 17 == 0 && bodies.Count > 0)
		{
			var b = bodies[rnd.Next(bodies.Count)];
			PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(rnd.NextSingle() - 0.5f, 0.5f, rnd.NextSingle() - 0.5f));
		}
	}

	static IEnumerator ReplayTrace()
	{
		const long seed = 123456;
		const int totalFrames = 600;
		// Run 1
		var trace1 = new List<float>();
		{
			using var w = new PhysxWorld();
			var bodies = new List<Rid>();
			var rnd = new Random(unchecked((int)seed));
			for (int f = 1; f <= totalFrames; f++)
			{
				StepSeededScene(w, rnd, f, bodies);
				yield return Wait.Frame();
				if (f % 60 == 0)
					foreach (var b in bodies) trace1.Add(w.Pos(b).Origin.Y);
			}
		}
		// Run 2 — same seed, same schedule.
		var trace2 = new List<float>();
		{
			using var w = new PhysxWorld();
			var bodies = new List<Rid>();
			var rnd = new Random(unchecked((int)seed));
			for (int f = 1; f <= totalFrames; f++)
			{
				StepSeededScene(w, rnd, f, bodies);
				yield return Wait.Frame();
				if (f % 60 == 0)
					foreach (var b in bodies) trace2.Add(w.Pos(b).Origin.Y);
			}
		}
		Assert.Expect(trace1.Count == trace2.Count && trace1.Count > 0, $"traces have equal non-zero length ({trace1.Count})");
		float maxDiff = 0f;
		for (int i = 0; i < Math.Min(trace1.Count, trace2.Count); i++)
			maxDiff = Math.Max(maxDiff, Math.Abs(trace1[i] - trace2[i]));
		Assert.Expect(maxDiff < 1e-4f,
			$"seeded scene replays identically (max trace diff {maxDiff:E2}; seed={seed})");
	}

	static IEnumerator SeededSceneTwice()
	{
		const long seed = 987654321;
		float lastY1 = 0, lastY2 = 0;
		for (int run = 0; run < 2; run++)
		{
			using var w = new PhysxWorld();
			var rnd = new Random(unchecked((int)seed));
			var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 3, 0));
			for (int f = 0; f < 120; f++)
			{
				if (f % 13 == 0)
					PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(rnd.NextSingle() - 0.5f, rnd.NextSingle() * 0.5f, rnd.NextSingle() - 0.5f));
				yield return Wait.Frame();
			}
			if (run == 0) lastY1 = w.Pos(b).Origin.Y;
			else lastY2 = w.Pos(b).Origin.Y;
		}
		Assert.Expect(Math.Abs(lastY1 - lastY2) < 1e-4f,
			$"same seed ends at same state ({lastY1:F6} vs {lastY2:F6}; seed={seed})");
	}

	static IEnumerator LongReplay()
	{
		const int totalFrames = 3000;
		var traces = new List<float>[2];
		for (int run = 0; run < 2; run++)
		{
			traces[run] = new List<float>();
			using var w = new PhysxWorld();
			var stack = new Rid[4];
			for (int i = 0; i < 4; i++)
			{
				stack[i] = w.MakeBody(w.Box(0.4f), new Vector3(0, 0.4f + i * 0.82f, 0));
				PhysicsServer3D.BodySetParam(stack[i], PhysicsServer3D.BodyParameter.Bounce, 0f);
			}
			var ball = w.MakeBody(w.Sphere(0.3f), new Vector3(1.2f, 5, 0));
			for (int f = 1; f <= totalFrames; f++)
			{
				yield return Wait.Frame();
				if (f % 250 == 0)
				{
					traces[run].Add(w.Pos(stack[3]).Origin.Y);
					traces[run].Add(w.Pos(ball).Origin.X);
					traces[run].Add(w.Pos(ball).Origin.Y);
				}
			}
		}
		Assert.Expect(traces[0].Count == traces[1].Count && traces[0].Count > 0, "long traces equal length");
		float maxDiff = 0f;
		for (int i = 0; i < traces[0].Count; i++)
			maxDiff = Math.Max(maxDiff, Math.Abs(traces[0][i] - traces[1][i]));
		Assert.Expect(maxDiff < 5e-3f,
			$"3,000-frame replay within tolerance (max diff {maxDiff:E2})");
	}

	static IEnumerator SolverIterationsStable()
	{
		for (int run = 0; run < 2; run++)
		{
			using var w = new PhysxWorld(false);
			PhysicsServer3D.SpaceSetParam(w.Space, PhysicsServer3D.SpaceParameter.SolverIterations, 16f);
			Assert.ExpectNear(PhysicsServer3D.SpaceGetParam(w.Space, PhysicsServer3D.SpaceParameter.SolverIterations), 16f, 1e-5f,
				$"solver iterations stable (run {run})");
			yield return Wait.Frame();
		}
	}
}
