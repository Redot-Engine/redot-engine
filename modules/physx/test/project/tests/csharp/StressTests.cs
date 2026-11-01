// Stress: soak, randomized operation storms (seeded, reproducible), large
// stacks, crowds, and joint grids. Expensive tests are tier=nightly with
// small fast-tier counterparts where useful.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace PhysxTestProject.Tests;

internal static class StressTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-STRS-001", "soak: mixed scene 10,000 frames, periodic integrity checks", Soak, TestTier.Nightly, 30000);
		s.Add("PHYSX-STRS-002", "randomized operation storm (seeded, 4,000 ops)", RandomOpStorm);
		s.Add("PHYSX-STRS-003", "randomized operation storm — extended (8,000 ops)", () => OpStorm(8000, extra: true), TestTier.Nightly, 30000);
		s.Add("PHYSX-STRS-004", "20-box stack settles without explosion", BigStack);
		s.Add("PHYSX-STRS-005", "joint grid 5x5 under gravity stays finite", JointGrid, TestTier.Extended, 9000);
		s.Add("PHYSX-STRS-006", "32 parallel spaces step independently", ManySpaces);
		s.Add("PHYSX-STRS-007", "pyramid of 55 boxes remains stable (nightly)", Pyramid, TestTier.Nightly, 15000);
	}

	static IEnumerator Soak()
	{
		using var w = new PhysxWorld();
		// Mixed scene: stacks, joints, areas, movers, queries.
		var watched = new List<Rid>();
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 5; j++)
			{
				var b = w.MakeBody(w.Box(0.4f), new Vector3(i * 10f, 0.4f + j * 0.83f, 0));
				PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
				watched.Add(b);
			}
		var anchor = w.MakeStatic(w.Box(0.3f), new Vector3(-10, 6, 0));
		var swinger = w.MakeBody(w.Box(0.25f), new Vector3(-10, 4.5f, 0));
		watched.Add(swinger);
		var pin = w.TrackJoint(PhysicsServer3D.JointCreate());
		PhysicsServer3D.JointMakePin(pin, anchor, Vector3.Zero, swinger, new Vector3(0, 0.5f, 0));
		var area = w.MakeArea(w.Box(4, 4, 4), new Vector3(20, 2, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 5f);
		var mover = w.MakeBody(w.Box(0.4f), new Vector3(20, 5, 0));
		PhysicsServer3D.BodySetState(mover, PhysicsServer3D.BodyState.CanSleep, false);
		watched.Add(mover);

		int frames = 0;
		while (frames < 10000)
		{
			for (int i = 0; i < 250; i++) { yield return Wait.Frame(); frames++; }
			// Periodic integrity checks.
			foreach (var b in watched)
			{
				Assert.Expect(PhysxWorld.Finite(w.Pos(b)), $"soak frame {frames}: transform finite");
				Assert.Expect(PhysxWorld.Finite(w.Vel(b)), $"soak frame {frames}: velocity finite");
				Assert.Expect(w.Vel(b).Length() < 200f, $"soak frame {frames}: velocity bounded");
				Assert.Expect(w.Pos(b).Origin.Y > -5f && w.Pos(b).Origin.Y < 100f, $"soak frame {frames}: body in sane range");
			}
			var hit = w.Ray(new Vector3(30, 5, 0), new Vector3(30, -5, 0));
			Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == w.FloorRid, $"soak frame {frames}: queries healthy");
			Assert.Expect(hit["normal"].AsVector3().Y > 0.9f, $"soak frame {frames}: query data healthy");
		}
	}

	static IEnumerator RandomOpStorm() => OpStorm(4000, extra: false);

	static IEnumerator OpStorm(int ops, bool extra)
	{
		// Seed comes from the runner (CLI --seed, default 123456) so CI can replay.
		long seed = 123456;
		var args = OS.GetCmdlineUserArgs();
		foreach (var a in args) if (a.StartsWith("--seed=")) long.TryParse(a[7..], out seed);
		var skipOps = new HashSet<int>();
		foreach (var a in args) if (a.StartsWith("--skip-ops="))
			foreach (var s in a[11..].Split(',')) if (int.TryParse(s, out int v)) skipOps.Add(v);
		var rnd = new Random(unchecked((int)seed));

		using var w = new PhysxWorld();
		var bodies = new List<Rid>();
		var shapes = new List<Rid>();
		var areas = new List<Rid>();
		var joints = new List<Rid>();
		var log = new Queue<string>(); // rolling op history for failure forensics

		for (int op = 0; op < ops; op++)
		{
			int what = rnd.Next(extra ? 16 : 14);
			if (skipOps.Contains(what)) continue;
			string desc = "";
			try
			{
				switch (what)
				{
					case 0: { var b = w.Box(0.2f + rnd.NextSingle() * 0.6f); shapes.Add(b); desc = $"create shape {b}"; break; }
					case 1:
						{
							if (shapes.Count > 0)
							{
								int i = rnd.Next(shapes.Count); var s = shapes[i]; shapes.RemoveAt(i);
								w.Free(s); desc = $"free shape idx {i}";
							}
							break;
						}
					case 2:
						{
							var sh = shapes.Count > 0 ? shapes[rnd.Next(shapes.Count)] : w.Box(0.3f);
							var b = w.MakeBody(sh, new Vector3(rnd.NextSingle() * 20f - 10f, 2f + rnd.NextSingle() * 6f, rnd.NextSingle() * 20f - 10f));
							bodies.Add(b); desc = $"create body {b}"; break;
						}
					case 3:
						{
							if (bodies.Count > 0)
							{
								int i = rnd.Next(bodies.Count); var b = bodies[i]; bodies.RemoveAt(i);
								w.Free(b); desc = $"free body idx {i}";
							}
							break;
						}
					case 4:
						{
							if (bodies.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(rnd.NextSingle() * 6f - 3f, rnd.NextSingle() * 4f, rnd.NextSingle() * 6f - 3f));
								desc = "impulse";
							}
							break;
						}
					case 5:
						{
							if (bodies.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								w.Teleport(b, new Vector3(rnd.NextSingle() * 20f - 10f, 5f, rnd.NextSingle() * 20f - 10f));
								desc = "teleport";
							}
							break;
						}
					case 6:
						{
							if (bodies.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								w.SetVel(b, new Vector3(rnd.NextSingle() * 10f - 5f, rnd.NextSingle() * 6f - 3f, rnd.NextSingle() * 10f - 5f));
								desc = "set velocity";
							}
							break;
						}
					case 7:
						{
							if (bodies.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								PhysicsServer3D.BodySetCollisionMask(b, (uint)rnd.Next(0, 16));
								desc = "change mask";
							}
							break;
						}
					case 8: { var a = w.MakeArea(w.Box(1.5f), new Vector3(rnd.NextSingle() * 20f - 10f, 3f, rnd.NextSingle() * 20f - 10f)); areas.Add(a); desc = "create area"; break; }
					case 9:
						{
							if (areas.Count > 0)
							{
								int i = rnd.Next(areas.Count); var a = areas[i]; areas.RemoveAt(i);
								w.Free(a); desc = "free area";
							}
							break;
						}
					case 10:
						{
							if (bodies.Count >= 2)
							{
								var j2 = w.TrackJoint(PhysicsServer3D.JointCreate());
								// Note: the two picks can coincide; the backend keeps
								// such self-joints dormant instead of creating a
								// degenerate PhysX constraint (see PhysXJoint3D::_create).
								PhysicsServer3D.JointMakePin(j2, bodies[rnd.Next(bodies.Count)], Vector3.Zero, bodies[rnd.Next(bodies.Count)], Vector3.Zero);
								joints.Add(j2); desc = "create joint";
							}
							break;
						}
					case 11:
						{
							if (joints.Count > 0)
							{
								int i = rnd.Next(joints.Count); var j2 = joints[i]; joints.RemoveAt(i);
								w.Free(j2); desc = "free joint";
							}
							break;
						}
					case 12:
						{
							var hit = w.Ray(new Vector3(rnd.NextSingle() * 10f - 5f, 8, rnd.NextSingle() * 10f - 5f), new Vector3(0, -8, 0));
							desc = $"query ({hit.Count} hits)"; break;
						}
					case 13:
						{
							if (bodies.Count > 0 && shapes.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								PhysicsServer3D.BodyAddShape(b, shapes[rnd.Next(shapes.Count)], Transform3D.Identity);
								desc = "attach shape";
							}
							break;
						}
					case 14:
						{
							if (bodies.Count > 0)
							{
								var b = bodies[rnd.Next(bodies.Count)];
								PhysicsServer3D.BodySetMode(b, (PhysicsServer3D.BodyMode)rnd.Next(0, 4)); desc = "change mode";
							}
							break;
						}
					case 15:
						{
							if (bodies.Count > 0 && bodies.Count > 1)
							{
								PhysicsServer3D.BodyAddCollisionException(bodies[rnd.Next(bodies.Count)], bodies[rnd.Next(bodies.Count)]);
								desc = "add exception";
							}
							break;
						}
				}
			}
			catch (Exception e)
			{
				Assert.Expect(false, $"RANDOM STRESS TEST FAILED seed = {seed} operation = {op} ({desc}) exception: {e.Message}");
				Assert.Expect(false, "op history (last 20): " + string.Join(" | ", log.ToArray()));
				yield break;
			}
			log.Enqueue($"#{op}:{desc}");
			while (log.Count > 20) log.Dequeue();

			if (op % 10 == 0)
			{
				yield return Wait.Frame();
				foreach (var b in bodies.Take(5))
				{
					if (!b.IsValid) continue;
					Assert.Expect(PhysxWorld.Finite(w.Pos(b)),
						$"RANDOM STRESS TEST FAILED seed = {seed} operation = {op} ({desc}): non-finite transform. History: {string.Join(" | ", log.ToArray())}");
				}
			}
		}
		// Final integrity: canary + most resources still alive.
		yield return Wait.Frames(30);
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(0, 3, 0));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 1.5f, 240, $"canary after {ops} random ops (seed {seed})");
		Assert.Expect(bodies.Count > 0 || shapes.Count > 0, "storm produced resources");
	}

	static IEnumerator BigStack()
	{
		using var w = new PhysxWorld();
		var stack = new Rid[20];
		for (int i = 0; i < 20; i++)
		{
			stack[i] = w.MakeBody(w.Box(0.45f), new Vector3(0, 0.45f + i * 0.92f, 0));
			PhysicsServer3D.BodySetParam(stack[i], PhysicsServer3D.BodyParameter.Bounce, 0f);
		}
		for (int f = 0; f < 8; f++)
		{
			yield return Wait.Frames(90);
			for (int i = 0; i < 20; i += 4)
			{
				Assert.Expect(PhysxWorld.Finite(w.Pos(stack[i])), $"stack box {i} finite at {(f + 1) * 90}");
				Assert.Expect(w.Pos(stack[i]).Origin.Y < 0.5f + i * 0.92f + 1.0f, $"stack box {i} has not teleported upward");
				Assert.Expect(w.Pos(stack[i]).Origin.Y > 0.1f, $"stack box {i} not through floor");
			}
		}
		int asleep = stack.Count(w.Sleeping);
		Assert.Expect(asleep >= 12, $"most of the stack sleeps ({asleep}/20)");
	}

	static IEnumerator JointGrid()
	{
		using var w = new PhysxWorld(false);
		const int n = 5;
		var grid = new Rid[n, n];
		for (int x = 0; x < n; x++)
			for (int z = 0; z < n; z++)
				grid[x, z] = w.MakeBody(w.Box(0.3f), new Vector3(x * 1.5f, 10f, z * 1.5f), mass: 2f);
		for (int x = 0; x < n; x++)
			for (int z = 0; z < n; z++)
			{
				if (x + 1 < n)
				{
					var j = w.TrackJoint(PhysicsServer3D.JointCreate());
					PhysicsServer3D.JointMakePin(j, grid[x, z], Vector3.Zero, grid[x + 1, z], Vector3.Zero);
				}
				if (z + 1 < n)
				{
					var j = w.TrackJoint(PhysicsServer3D.JointCreate());
					PhysicsServer3D.JointMakePin(j, grid[x, z], Vector3.Zero, grid[x, z + 1], Vector3.Zero);
				}
			}
		for (int f = 0; f < 6; f++)
		{
			yield return Wait.Frames(90);
			for (int x = 0; x < n; x += 2)
				for (int z = 0; z < n; z += 2)
				{
					Assert.Expect(PhysxWorld.Finite(w.Pos(grid[x, z])), $"grid node ({x},{z}) finite at {(f + 1) * 90}");
					Assert.Expect(w.Vel(grid[x, z]).Length() < 80f, $"grid node ({x},{z}) velocity bounded");
				}
		}
	}

	static IEnumerator ManySpaces()
	{
		var spaces = new List<Rid>();
		var bodies = new List<Rid>();
		for (int i = 0; i < 32; i++)
		{
			var sp = PhysxWorld.CreateSpace();
			spaces.Add(sp);
			var shape = PhysicsServer3D.BoxShapeCreate();
			PhysicsServer3D.ShapeSetData(shape, new Vector3(0.3f, 0.3f, 0.3f));
			var b = PhysicsServer3D.BodyCreate();
			PhysicsServer3D.BodySetSpace(b, sp);
			PhysicsServer3D.BodyAddShape(b, shape, Transform3D.Identity);
			PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, new Transform3D(Basis.Identity, new Vector3(0, 5, 0)));
			bodies.Add(b);
		}
		for (int f = 0; f < 4; f++)
		{
			yield return Wait.Frames(60);
			foreach (var b in bodies)
				Assert.Expect(PhysxWorld.Finite(PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.Transform).AsTransform3D().Origin),
					$"32-space body finite at {(f + 1) * 60}");
		}
		Assert.Expect(PhysicsServer3D.BodyGetState(bodies[15], PhysicsServer3D.BodyState.Transform).AsTransform3D().Origin.Y < 4.5f,
			"space 15's body actually fell");
		foreach (var sp in spaces) PhysicsServer3D.FreeRid(sp);
	}

	static IEnumerator Pyramid()
	{
		using var w = new PhysxWorld();
		const int baseCount = 10;
		var blocks = new List<Rid>();
		int level = 0;
		for (int n = baseCount; n >= 1; n--)
		{
			for (int i = 0; i < n; i++)
			{
				var b = w.MakeBody(w.Box(0.45f), new Vector3((i - n / 2f) * 0.95f, 0.45f + level * 0.92f, 0));
				PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
				blocks.Add(b);
			}
			level++;
		}
		for (int f = 0; f < 8; f++)
		{
			yield return Wait.Frames(120);
			foreach (var b in blocks.Take(1).Concat(blocks.TakeLast(1)))
			{
				Assert.Expect(PhysxWorld.Finite(w.Pos(b)), $"pyramid block finite at {(f + 1) * 120}");
				Assert.Expect(w.Pos(b).Origin.Y < 12f && w.Pos(b).Origin.Y > 0f, "pyramid block in sane band");
			}
		}
		int asleep = blocks.Count(w.Sleeping);
		Assert.Expect(asleep >= blocks.Count * 3 / 4, $"pyramid mostly asleep ({asleep}/{blocks.Count})");
	}
}
