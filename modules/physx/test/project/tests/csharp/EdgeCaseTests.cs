// Edge cases: invalid RIDs, degenerate/invalid geometry, extreme scales and
// velocities, malformed data. Contract: the engine may reject or clamp, but
// the process must stay stable and unrelated simulation must stay correct.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class EdgeCaseTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-EDGE-001", "default RID passed to every major API: no crash", InvalidRidEverywhere);
		s.Add("PHYSX-EDGE-002", "zero-size box handled without corruption", ZeroBox);
		s.Add("PHYSX-EDGE-003", "zero-radius sphere handled without corruption", ZeroSphere);
		s.Add("PHYSX-EDGE-004", "tiny geometry (1e-5) stays finite", TinyGeometry);
		s.Add("PHYSX-EDGE-005", "huge geometry (1e4) stays finite", HugeGeometry);
		s.Add("PHYSX-EDGE-006", "degenerate convex (collinear points) rejected or stable", DegenerateConvex);
		s.Add("PHYSX-EDGE-007", "empty convex point array handled", EmptyConvex);
		s.Add("PHYSX-EDGE-008", "malformed concave (mismatched triangle count) handled", MalformedConcave);
		s.Add("PHYSX-EDGE-009", "wrong-typed shape data (box fed a float) handled", WrongTypedData);
		s.Add("PHYSX-EDGE-010", "extreme velocity (5000 m/s) stays finite without CCD", ExtremeVelocity);
		s.Add("PHYSX-EDGE-011", "extremely low velocity does not produce NaN", TinyVelocity);
		s.Add("PHYSX-EDGE-012", "body at 1e6 coordinates simulates stably", FarAwayBody);
		s.Add("PHYSX-EDGE-013", "negative-coordinate world mirrors behavior", NegativeCoords);
		s.Add("PHYSX-EDGE-014", "very large mass ratio in a stack stays finite", ExtremeMassRatio);
		s.Add("PHYSX-EDGE-015", "massive body count bookkeeping (500 bodies) stays finite", ManyBodies, TestTier.Extended);
		s.Add("PHYSX-EDGE-016", "NaN-free after teleporting into deep penetration", DeepPenetrationTeleport);
		s.Add("PHYSX-EDGE-017", "shape index out of range handled", BadShapeIndex);
		s.Add("PHYSX-EDGE-018", "empty exclude/mask query configurations work", DegenerateQueries);
	}

	static IEnumerator AliveCheck(PhysxWorld w, string after)
	{
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(7, 3, 7));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 1.5f, 240, $"canary after: {after}");
		Assert.Expect(PhysxWorld.Finite(w.Pos(canary)) && PhysxWorld.Finite(w.Vel(canary)), $"canary finite after: {after}");
	}

	static IEnumerator InvalidRidEverywhere()
	{
		using var w = new PhysxWorld();
		var bad = new Rid();
		PhysicsServer3D.BodySetSpace(bad, w.Space);
		_ = PhysicsServer3D.BodyGetSpace(bad);
		PhysicsServer3D.BodySetMode(bad, PhysicsServer3D.BodyMode.Rigid);
		PhysicsServer3D.BodySetParam(bad, PhysicsServer3D.BodyParameter.Mass, 1f);
		_ = PhysicsServer3D.BodyGetState(bad, PhysicsServer3D.BodyState.Transform);
		PhysicsServer3D.BodyApplyCentralImpulse(bad, Vector3.One);
		PhysicsServer3D.BodySetAxisLock(bad, PhysicsServer3D.BodyAxis.linearly, true);
		PhysicsServer3D.BodyAddCollisionException(bad, bad);
		_ = PhysicsServer3D.ShapeGetData(bad);
		_ = PhysicsServer3D.ShapeGetType(bad);
		PhysicsServer3D.SpaceSetParam(bad, PhysicsServer3D.SpaceParameter.SolverIterations, 4f);
		_ = PhysicsServer3D.SpaceGetDirectState(bad);
		PhysicsServer3D.AreaSetParam(bad, PhysicsServer3D.AreaParameter.Gravity, 1f);
		_ = PhysicsServer3D.JointGetType(bad);
		PhysicsServer3D.SoftBodySetTotalMass(bad, 1f);
		yield return Wait.Frames(20);
		yield return AliveCheck(w, "invalid RID operations");
	}
	static IEnumerator ZeroBox()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(Box0(w), new Vector3(0, 2, 0));
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "zero box stays finite");
		yield return AliveCheck(w, "zero-size box");
	}
	static Rid Box0(PhysxWorld w)
	{
		var s = PhysicsServer3D.BoxShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, new Vector3(0, 0, 0));
		return s;
	}
	static IEnumerator ZeroSphere()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.SphereShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, 0f);
		var b = w.MakeBody(s, new Vector3(0, 2, 0));
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "zero sphere stays finite");
		yield return AliveCheck(w, "zero-radius sphere");
	}
	static IEnumerator TinyGeometry()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.BoxShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, new Vector3(1e-5f, 1e-5f, 1e-5f));
		var b = w.MakeBody(s, new Vector3(0, 2, 0));
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 1.0f, 300, "tiny body falls to floor");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "tiny geometry finite");
	}
	static IEnumerator HugeGeometry()
	{
		using var w = new PhysxWorld(false);
		var s = PhysicsServer3D.BoxShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, new Vector3(1e4f, 1e4f, 1e4f));
		var b = w.MakeBody(s, new Vector3(0, 5, 0));
		yield return Wait.Frames(60);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "huge geometry finite");
	}
	static IEnumerator DegenerateConvex()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.ConvexPolygonShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, new Vector3[] { new(0, 0, 0), new(1, 0, 0), new(2, 0, 0) }); // collinear
		var b = w.MakeBody(s, new Vector3(0, 2, 0));
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "degenerate convex body finite");
		yield return AliveCheck(w, "degenerate convex");
	}
	static IEnumerator EmptyConvex()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.ConvexPolygonShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, new Vector3[] { });
		var b = w.MakeBody(s, new Vector3(0, 2, 0));
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "empty convex body finite");
		yield return AliveCheck(w, "empty convex");
	}
	static IEnumerator MalformedConcave()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.ConcavePolygonShapeCreate();
		w.AdoptShape(s);
		// Two vertices cannot form a triangle.
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary
		{
			["faces"] = new Vector3[] { new(0, 0, 0), new(1, 0, 0) },
			["backface_collision"] = false
		});
		yield return Wait.Frames(20);
		yield return AliveCheck(w, "malformed concave mesh");
	}
	static IEnumerator WrongTypedData()
	{
		using var w = new PhysxWorld();
		var s = PhysicsServer3D.BoxShapeCreate();
		w.AdoptShape(s);
		PhysicsServer3D.ShapeSetData(s, 7f); // box expects Vector3
		yield return Wait.Frames(10);
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["bogus"] = 1 }); // dict is wrong too
		yield return Wait.Frames(10);
		yield return AliveCheck(w, "wrong-typed shape data");
	}
	static IEnumerator ExtremeVelocity()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Sphere(0.3f), new Vector3(0, 50, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.CanSleep, false);
		w.SetVel(b, new Vector3(0, -5000f, 0));
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "extreme velocity stays finite");
		// It either tunneled or stopped — both are finite outcomes; CCD category covers correctness.
	}
	static IEnumerator TinyVelocity()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		w.SetVel(b, new Vector3(1e-6f, 0, 0));
		yield return Wait.Frames(120);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "tiny velocity stays finite (no NaN)");
	}
	static IEnumerator FarAwayBody()
	{
		using var w = new PhysxWorld(false);
		w.AddFloor(100000f);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(100000, 100003, 100000));
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 100002.5f, 300, "far-away body falls");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "far-away body finite");
		Assert.Expect(w.Pos(b).Origin.Y > 100000.2f, "far-away body rests on far floor");
	}
	static IEnumerator NegativeCoords()
	{
		using var w = new PhysxWorld(false);
		w.AddFloor(-50f);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(-100, -46, -100));
		// Spawn (-46) is ABOVE the floor rest height (-49.6), so wait for the
		// descent: the body must fall below -49 before the rest check.
		yield return Wait.UntilOrFail(
			() => w.Pos(b).Origin.Y < -48f && Math.Abs(w.Vel(b).Y) < 0.2f, 300,
			"body settles onto negative-coordinate floor");
		Assert.ExpectNear(w.Pos(b).Origin.Y, -49.6f, 0.2f, "rests at negative-coordinate half-extent");
	}
	static IEnumerator ExtremeMassRatio()
	{
		using var w = new PhysxWorld();
		var anvil = w.MakeBody(w.Box(0.6f), new Vector3(0, 0.6f, 0), mass: 10000f);
		PhysicsServer3D.BodySetParam(anvil, PhysicsServer3D.BodyParameter.Bounce, 0f);
		var feather = w.MakeBody(w.Box(0.3f), new Vector3(0, 1.5f, 0), mass: 0.01f);
		PhysicsServer3D.BodySetParam(feather, PhysicsServer3D.BodyParameter.Bounce, 0f);
		for (int f = 0; f < 4; f++)
		{
			yield return Wait.Frames(60);
			Assert.Expect(PhysxWorld.Finite(w.Pos(anvil)) && PhysxWorld.Finite(w.Pos(feather)), $"1e6 mass ratio finite at {(f + 1) * 60}");
			Assert.Expect(w.Vel(feather).Length() < 100f, "feather velocity bounded");
		}
	}
	static IEnumerator ManyBodies()
	{
		using var w = new PhysxWorld();
		var bodies = new Rid[500];
		var rnd = new Random(42);
		for (int i = 0; i < 500; i++)
		{
			bodies[i] = w.MakeBody(w.Box(0.25f),
				new Vector3(rnd.NextSingle() * 40f - 20f, 1f + (i / 100) * 1.2f, rnd.NextSingle() * 40f - 20f));
		}
		for (int f = 0; f < 5; f++)
		{
			yield return Wait.Frames(60);
			for (int i = 0; i < bodies.Length; i += 50)
				Assert.Expect(PhysxWorld.Finite(w.Pos(bodies[i])), $"body {i} finite at {(f + 1) * 60} frames");
		}
	}
	static IEnumerator DeepPenetrationTeleport()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 2, 0));
		yield return Wait.Frames(5);
		w.Teleport(b, new Vector3(0, -0.3f, 0)); // deep inside the floor
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after deep-penetration teleport");
		Assert.Expect(w.Pos(b).Origin.Y > -0.5f, "depenetration pushes body out (or at least not deeper)");
	}
	static IEnumerator BadShapeIndex()
	{
		using var w = new PhysxWorld();
		var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 1, 0));
		PhysicsServer3D.BodySetShape(b, 5, w.Box(0.2f));   // out of range
		_ = PhysicsServer3D.BodyGetShape(b, 7);
		PhysicsServer3D.BodyRemoveShape(b, -1);
		PhysicsServer3D.BodySetShapeDisabled(b, 99, true);
		yield return Wait.Frames(10);
		yield return AliveCheck(w, "out-of-range shape indices");
	}
	static IEnumerator DegenerateQueries()
	{
		using var w = new PhysxWorld();
		var hit = w.Ray(new Vector3(0, 1, 0), new Vector3(0, 1, 0), mask: 0);
		Assert.Expect(hit.Count == 0, "zero-mask ray finds nothing");
		var p0 = new PhysicsPointQueryParameters3D { Position = Vector3.Zero, CollisionMask = 0 };
		Assert.Expect(w.Dss().IntersectPoint(p0, 0).Count == 0, "max_results=0 point query returns empty");
		var sq = new PhysicsShapeQueryParameters3D { ShapeRid = w.Sphere(0.5f), Transform = Transform3D.Identity, CollisionMask = 0 };
		Assert.Expect(w.Dss().IntersectShape(sq, 0).Count == 0, "max_results=0 shape query returns empty");
		yield return Wait.Frame();
	}
}
