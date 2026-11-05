// Foundation: backend availability, server identity, space basics, frame
// advancement, and the canonical analytic free-fall experiments that every
// later category implicitly depends on.

using System.Collections;

namespace PhysxTestProject.Tests;

internal static class FoundationTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-FOUND-001", "active 3D physics engine is PhysX", BackendIsPhysX);
		s.Add("PHYSX-FOUND-002", "PhysXServer3D class registered in ClassDB", PhysxClassRegistered);
		s.Add("PHYSX-FOUND-003", "space create/activate/direct-state", SpaceBasics);
		s.Add("PHYSX-FOUND-004", "physics frames advance with simulation", FramesAdvance);
		s.Add("PHYSX-FOUND-005", "basic shape create returns valid typed RID", BasicShape);
		s.Add("PHYSX-FOUND-006", "static body transform round-trips through steps", StaticTransformRoundtrip);
		s.Add("PHYSX-FOUND-007", "free fall matches 1/2*g*t^2 within solver tolerance", FreeFallDistance);
		s.Add("PHYSX-FOUND-008", "free fall velocity reaches ~g after 1 s", FreeFallVelocity);
		s.Add("PHYSX-FOUND-009", "gravity_scale=0 body floats in place", ZeroGravityScaleFloats);
		s.Add("PHYSX-FOUND-010", "transforms and velocities stay finite over 300 frames", StaysFinite);
		s.Add("PHYSX-FOUND-011", "direct state of invalid RID is null, process stable", InvalidRidDirectState);
		s.Add("PHYSX-FOUND-012", "direct body state reports the actual applied gravity", TotalGravityMeasured);
		s.Add("PHYSX-FOUND-013", "two identical bodies fall identically (bit-stable pair)", IdenticalFallers);
		s.Add("PHYSX-FOUND-014", "simulation with only inactive spaces stays responsive", InactiveSpaceTicks);
		s.Add("PHYSX-FOUND-015", "space activation is observable via space_is_active", SpaceActivationFlag);
		s.Add("PHYSX-FOUND-016", "process info: collision pairs reflect contacts; islands documented 0", ProcessInfoStats);
	}

	static IEnumerator BackendIsPhysX()
	{
		string engine = ProjectSettings.GetSettingWithOverride("physics/3d/physics_engine").ToString();
		Assert.Expect(engine == "PhysX", $"physics/3d/physics_engine == 'PhysX' (got '{engine}')");
		yield return Wait.Frame();
	}

	static IEnumerator PhysxClassRegistered()
	{
		Assert.Expect(Godot.ClassDB.ClassExists("PhysXServer3D"), "ClassDB knows PhysXServer3D");
		Assert.Expect(Godot.ClassDB.ClassExists("PhysXDirectBodyState3D"), "ClassDB knows PhysXDirectBodyState3D");
		yield return Wait.Frame();
	}

	static IEnumerator SpaceBasics()
	{
		using var w = new PhysxWorld(withFloor: false);
		Assert.Expect(PhysicsServer3D.SpaceIsActive(w.Space), "new space is active after space_set_active(true)");
		var dss = w.Dss();
		Assert.Expect(dss != null, "space_get_direct_state returns an object");
		// A downward ray in the empty space must miss.
		var miss = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
		Assert.Expect(miss.Count == 0, "empty space raycast misses");
		yield return Wait.Frame();
	}

	static IEnumerator FramesAdvance()
	{
		ulong start = Engine.GetPhysicsFrames();
		yield return Wait.Frames(5);
		ulong end = Engine.GetPhysicsFrames();
		Assert.Expect(end - start >= 5, $"physics frames advanced by >=5 (got {end - start})");
	}

	static IEnumerator BasicShape()
	{
		using var w = new PhysxWorld(false);
		var box = w.Box(0.5f, 0.5f, 0.5f);
		Assert.Expect(box.IsValid, "box_shape_create returns valid RID");
		Assert.Expect(PhysicsServer3D.ShapeGetType(box) == PhysicsServer3D.ShapeType.Box, "shape_get_type reports Box");
		yield return Wait.Frame();
	}

	static IEnumerator StaticTransformRoundtrip()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(1, 1, 1), new Vector3(7, 3, -2));
		yield return Wait.Frames(30);
		var t = w.Pos(b);
		Assert.ExpectVecNear(t.Origin, new Vector3(7, 3, -2), 1e-4f, "static body transform unchanged after 30 steps");
	}

	static IEnumerator FreeFallDistance()
	{
		using var w = new PhysxWorld(false); // no floor: pure free fall
		var b = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(0, 10, 0));
		w.SetVel(b, Vector3.Zero);
		yield return Wait.Frames(60); // 1.0 s at 60 Hz
		float y = w.Pos(b).Origin.Y;
		// Analytic: 10 - 0.5*9.81*1^2 = 5.095. Tolerance 0.25 covers semi-implicit
		// Euler (one extra dv*g*dt^2 term) and solver noise.
		Assert.ExpectNear(y, 5.095f, 0.25f, "free-fall distance matches 1/2*g*t^2");
	}

	static IEnumerator FreeFallVelocity()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(0, 30, 0));
		w.SetVel(b, Vector3.Zero);
		yield return Wait.Frames(60);
		// Tolerance 0.6: the backend needs ~3 ticks after body creation before
		// gravity integration starts (registration warmup), so v lags g*t.
		Assert.ExpectNear(w.Vel(b).Y, -PhysxWorld.G, 0.6f, "free-fall velocity reaches ~g after 1 s");
	}

	static IEnumerator ZeroGravityScaleFloats()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(1, 5, 1));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		w.SetVel(b, Vector3.Zero);
		yield return Wait.Frames(90);
		Assert.ExpectVecNear(w.Pos(b).Origin, new Vector3(1, 5, 1), 0.005f, "gravity_scale=0 body does not move");
		Assert.ExpectVecNear(w.Vel(b), Vector3.Zero, 0.001f, "gravity_scale=0 body accumulates no velocity");
	}

	static IEnumerator StaysFinite()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f, 0.4f, 0.4f), new Vector3(0, 2, 0));
		for (int i = 0; i < 6; i++)
		{
			yield return Wait.Frames(50);
			Assert.Expect(PhysxWorld.Finite(w.Pos(b)), $"transform finite at checkpoint {i}");
			Assert.Expect(PhysxWorld.Finite(w.Vel(b)), $"velocity finite at checkpoint {i}");
		}
	}

	static IEnumerator InvalidRidDirectState()
	{
		var bogus = new Rid();
		var st = PhysicsServer3D.SpaceGetDirectState(bogus);
		Assert.Expect(st == null, "direct state of default RID is null");
		yield return Wait.Frame();
		// Engine must still be alive.
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y < 1.9f, "simulation alive after invalid-RID query");
	}

	static IEnumerator TotalGravityMeasured()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(0, 20, 0));
		yield return Wait.Frames(3);
		var g = w.GravityOf(b);
		Assert.ExpectNear(g.Length(), PhysxWorld.G, 0.2f, $"direct state total gravity == {PhysxWorld.G} (got {g.Length():F3})");
		Assert.Expect(g.Y < -9f, "gravity points down");
	}

	static IEnumerator IdenticalFallers()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(-3, 25, 0));
		var b = w.MakeBody(w.Box(0.5f, 0.5f, 0.5f), new Vector3(3, 25, 0));
		for (int i = 0; i < 5; i++)
		{
			yield return Wait.Frames(40);
			Assert.ExpectNear(w.Pos(a).Origin.Y, w.Pos(b).Origin.Y, 1e-5f,
				$"identical bodies at same height after {(i + 1) * 40} frames");
		}
	}

	static IEnumerator InactiveSpaceTicks()
	{
		var sp = PhysicsServer3D.SpaceCreate();
		Assert.Expect(!PhysicsServer3D.SpaceIsActive(sp), "fresh space inactive before activation");
		yield return Wait.Frames(30);
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y < 1.9f, "active-space simulation unaffected by inactive space");
		PhysicsServer3D.FreeRid(sp);
	}

	static IEnumerator SpaceActivationFlag()
	{
		var sp = PhysicsServer3D.SpaceCreate();
		PhysicsServer3D.SpaceSetActive(sp, false);
		Assert.Expect(!PhysicsServer3D.SpaceIsActive(sp), "space inactive after set_active(false)");
		PhysicsServer3D.SpaceSetActive(sp, true);
		Assert.Expect(PhysicsServer3D.SpaceIsActive(sp), "space active after set_active(true)");
		PhysicsServer3D.SpaceSetActive(sp, false);
		Assert.Expect(!PhysicsServer3D.SpaceIsActive(sp), "space inactive again");
		PhysicsServer3D.FreeRid(sp);
		yield return Wait.Frame();
	}
	static IEnumerator ProcessInfoStats()
	{
		using var w = new PhysxWorld();
		Assert.Expect(PhysicsServer3D.GetProcessInfo(PhysicsServer3D.ProcessInfo.CollisionPairs) == 0,
			"no collision pairs in an empty scene");
		var body = w.MakeBody(w.Sphere(0.5f), new Vector3(0, 4, 0));
		// Keep the body awake so the contact pair is processed every step and
		// the counter is stable regardless of when the poll happens.
		PhysicsServer3D.BodySetState(body, PhysicsServer3D.BodyState.CanSleep, false);
		yield return Wait.UntilOrFail(() => w.Pos(body).Origin.Y < 0.6f, 240, "sphere lands on the floor");
		yield return Wait.Frames(10);
		Assert.Expect(PhysicsServer3D.GetProcessInfo(PhysicsServer3D.ProcessInfo.CollisionPairs) >= 1,
			"collision pairs reported once contacts exist");
		Assert.Expect(PhysicsServer3D.GetProcessInfo(PhysicsServer3D.ProcessInfo.IslandCount) == 0,
			"island count documented 0 (PhysX exposes no island counter)");
		yield return Wait.Frame();
	}
}
