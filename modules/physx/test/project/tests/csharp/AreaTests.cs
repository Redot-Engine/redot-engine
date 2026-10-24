// Areas: lifecycle, shape bookkeeping, monitoring behavior (enter/exit via
// managed delegates), filtering, and — most importantly — measured space
// overrides (gravity/damping modes, priority) rather than getter round-trips.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class AreaTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-AREA-001", "area create/space/shapes/transform plumbing", AreaPlumbing);
		s.Add("PHYSX-AREA-002", "area detects body entering and exiting", MonitorEnterExit);
		s.Add("PHYSX-AREA-003", "area detects monitorable area (area-area callback)", AreaAreaMonitor);
		s.Add("PHYSX-AREA-004", "zero-mask area detects nothing", MonitorMaskZero);
		s.Add("PHYSX-AREA-005", "relocating an area re-triggers enter/exit", MonitorRelocation);
		s.Add("PHYSX-AREA-006", "gravity REPLACE override: measured acceleration == area gravity", GravityReplace);
		s.Add("PHYSX-AREA-007", "gravity COMBINE adds area gravity to space gravity", GravityCombine);
		s.Add("PHYSX-AREA-008", "DISABLED override mode leaves space gravity", GravityDisabledMode);
		s.Add("PHYSX-AREA-009", "gravity vector direction honored (sideways gravity)", GravityVector);
		s.Add("PHYSX-AREA-010", "linear damp REPLACE override measured", DampReplace);
		s.Add("PHYSX-AREA-011", "overlapping replace-areas: priority decides winner", PriorityOverlap);
		s.Add("PHYSX-AREA-012", "area param round-trips [plumbing]", ParamRoundtrips);
		s.Add("PHYSX-AREA-013", "monitor callback removed: no further events", MonitorCallbackCleared);
		s.Add("PHYSX-AREA-014", "area shape add/remove/clear bookkeeping", ShapeBookkeeping);
		s.Add("PHYSX-AREA-015", "area body leaves area when falling through", ExitByFalling);
		s.Add("PHYSX-AREA-016", "monitoring-only moving area keeps a sleeping body asleep", MonitoringMoveKeepsSleep);
	}

	static IEnumerator AreaPlumbing()
	{
		using var w = new PhysxWorld(false);
		var area = PhysicsServer3D.AreaCreate();
		w.TrackArea(area);
		PhysicsServer3D.AreaSetSpace(area, w.Space);
		Assert.Expect(PhysicsServer3D.AreaGetSpace(area) == w.Space, "area space round-trip");
		var sh = w.Box(1, 1, 1);
		PhysicsServer3D.AreaAddShape(area, sh, new Transform3D(Basis.Identity, new Vector3(0, 5, 0)));
		Assert.Expect(PhysicsServer3D.AreaGetShapeCount(area) == 1, "shape count");
		Assert.Expect(PhysicsServer3D.AreaGetShape(area, 0) == sh, "shape rid readback");
		PhysicsServer3D.AreaSetTransform(area, new Transform3D(Basis.Identity, new Vector3(2, 5, 0)));
		Assert.ExpectVecNear(PhysicsServer3D.AreaGetTransform(area).Origin, new Vector3(2, 5, 0), 1e-4f, "area transform round-trip");
		PhysicsServer3D.AreaSetCollisionLayer(area, 5);
		PhysicsServer3D.AreaSetCollisionMask(area, 9);
		Assert.Expect(PhysicsServer3D.AreaGetCollisionLayer(area) == 5, "layer round-trip");
		Assert.Expect(PhysicsServer3D.AreaGetCollisionMask(area) == 9, "mask round-trip");
		PhysicsServer3D.AreaSetMonitorable(area, true);
		yield return Wait.Frame();
	}
	static IEnumerator MonitorEnterExit()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 10, 0)); // floating detection box
		int enters = 0, exits = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant status, Variant rid, Variant id, Variant shp, Variant ashp) =>
		{
			if (status.AsInt32() == 0) enters++; else exits++;
		}));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10.5f, 0)); // spawned inside
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		yield return Wait.Frames(10);
		Assert.Expect(enters >= 1, "body entering area detected");
		// Now let it fall through.
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 1f);
		yield return Wait.UntilOrFail(() => exits >= 1, 300, "body exits area while falling");
		Assert.Expect(enters >= 1 && exits >= 1, $"enter/exit pair reported ({enters}/{exits})");
	}
	static IEnumerator AreaAreaMonitor()
	{
		using var w = new PhysxWorld(false);
		var watcher = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0)); // spans y in [3, 7]
		PhysicsServer3D.AreaSetMonitorable(watcher, false);
		// Probe center y=7 so its box (y in [6, 8]) actually overlaps the
		// watcher — at y=9 the volumes are 1 unit apart and no monitor event
		// can ever fire (the original position predates the REG-0013 full-size
		// box semantics, where it did overlap).
		var probe = w.MakeArea(w.Box(1, 1, 1), new Vector3(0, 7, 0), layer: 1, mask: 0xFFFFFFFF);
		PhysicsServer3D.AreaSetMonitorable(probe, true);
		int events = 0;
		PhysicsServer3D.AreaSetAreaMonitorCallback(watcher, Callable.From((Variant status, Variant rid, Variant id, Variant a, Variant b) => events++));
		yield return Wait.Frames(10);
		Assert.Expect(events >= 1, $"area-area overlap detected (got {events})");
	}
	static IEnumerator MonitorMaskZero()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0), layer: 1, mask: 0);
		int events = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant s, Variant r, Variant i, Variant a, Variant b) => events++));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		yield return Wait.Frames(30);
		Assert.Expect(events == 0, "zero-mask area never fires");
	}
	static IEnumerator MonitorRelocation()
	{
		using var w = new PhysxWorld(false);
		var body = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		PhysicsServer3D.BodySetParam(body, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		yield return Wait.Frames(5);
		// The area's OWN transform must carry the position (shape at local
		// origin): MakeArea bakes `pos` into the shape's LOCAL transform, so
		// AreaSetTransform() would then move the actor and the shape would end
		// up at actor_pose + local_offset instead of where the test expects.
		var area = PhysicsServer3D.AreaCreate();
		w.TrackArea(area);
		PhysicsServer3D.AreaSetSpace(area, w.Space);
		PhysicsServer3D.AreaAddShape(area, w.Box(1.5f, 1.5f, 1.5f), Transform3D.Identity);
		PhysicsServer3D.AreaSetTransform(area, new Transform3D(Basis.Identity, new Vector3(10, 5, 0)));
		int enters = 0, exits = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant status, Variant rid, Variant id, Variant a, Variant b) =>
		{
			if (status.AsInt32() == 0) enters++; else exits++;
		}));
		yield return Wait.Frames(5);
		Assert.Expect(enters == 0, "no events while area is elsewhere");
		PhysicsServer3D.AreaSetTransform(area, new Transform3D(Basis.Identity, new Vector3(0, 5, 0)));
		yield return Wait.Frames(10);
		Assert.Expect(enters >= 1, "enter fires after area moves onto body");
		PhysicsServer3D.AreaSetTransform(area, new Transform3D(Basis.Identity, new Vector3(10, 5, 0)));
		yield return Wait.Frames(10);
		Assert.Expect(exits >= 1, "exit fires after area moves away");
	}
	static IEnumerator GravityReplace()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 4, 2), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 20f);
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		w.SetVel(b, Vector3.Zero);
		yield return Wait.Frames(30); // 0.5 s -> v = 20*0.5 = 10
		Assert.ExpectNear(w.Vel(b).Y, -10f, 1.0f, "REPLACE gravity: measured v = a*t with a=20");
	}
	static IEnumerator GravityCombine()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 4, 2), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Combine);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 10f);
		var inside = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		var outside = w.MakeBody(w.Box(0.3f), new Vector3(30, 10, 0));
		yield return Wait.Frames(30);
		float vIn = Math.Abs(w.Vel(inside).Y), vOut = Math.Abs(w.Vel(outside).Y);
		Assert.ExpectNear(vOut, PhysxWorld.G * 0.5f, 0.2f, "outside body: space gravity only");
		// COMBINE adds area gravity to default: |a| >= 9.81+10 directionally.
		Assert.Expect(vIn > vOut + 4f, $"COMBINE gravity accelerates inside body more ({vIn:F2} vs {vOut:F2})");
	}
	static IEnumerator GravityDisabledMode()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 4, 2), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Disabled);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 40f);
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		yield return Wait.Frames(30);
		Assert.ExpectNear(Math.Abs(w.Vel(b).Y), PhysxWorld.G * 0.5f, 0.25f, "DISABLED mode ignores area gravity");
	}
	static IEnumerator GravityVector()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(4, 2, 4), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 10f);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityVector, new Vector3(1, 0, 0)); // sideways
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		yield return Wait.Frames(30);
		Assert.Expect(Mathf.Abs(w.Vel(b).X) > 3f, $"sideways gravity accelerates +x (vx={w.Vel(b).X:F2})");
		Assert.Expect(Math.Abs(w.Vel(b).Y) < 1f, "no downward gravity inside REPLACE area");
	}
	static IEnumerator DampReplace()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 6, 2), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.LinearDampOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.LinearDamp, 5f);
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		w.SetVel(b, new Vector3(8, 0, 0));
		yield return Wait.Frames(60); // 8 * e^-5 = 0.054
		Assert.Expect(w.Vel(b).X < 0.5f, $"REPLACE linear damp 5 brakes velocity (vx={w.Vel(b).X:F3})");
		Assert.Expect(w.Vel(b).X > 0f, "damping does not reverse velocity");
	}
	static IEnumerator PriorityOverlap()
	{
		using var w = new PhysxWorld(false);
		var low = w.MakeArea(w.Box(4, 6, 4), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(low, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(low, PhysicsServer3D.AreaParameter.Gravity, 5f);
		PhysicsServer3D.AreaSetParam(low, PhysicsServer3D.AreaParameter.Priority, 1f);
		var high = w.MakeArea(w.Box(4, 6, 4), new Vector3(0, 10, 0));
		PhysicsServer3D.AreaSetParam(high, PhysicsServer3D.AreaParameter.GravityOverrideMode, (int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(high, PhysicsServer3D.AreaParameter.Gravity, 30f);
		PhysicsServer3D.AreaSetParam(high, PhysicsServer3D.AreaParameter.Priority, 8f);
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 10, 0));
		yield return Wait.Frames(30); // 0.5 s
		Assert.ExpectNear(Math.Abs(w.Vel(b).Y), 15f, 6f, "higher-priority REPLACE area wins (a=30 -> v=15)");
	}
	static IEnumerator ParamRoundtrips()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeArea(w.Box(1, 1, 1), new Vector3(0, 5, 0));
		PhysicsServer3D.AreaSetParam(a, PhysicsServer3D.AreaParameter.Gravity, 25f);
		PhysicsServer3D.AreaSetParam(a, PhysicsServer3D.AreaParameter.GravityVector, new Vector3(0, -1, 0));
		PhysicsServer3D.AreaSetParam(a, PhysicsServer3D.AreaParameter.Priority, 4f);
		PhysicsServer3D.AreaSetParam(a, PhysicsServer3D.AreaParameter.WindForceMagnitude, 2f);
		Assert.ExpectNear(PhysicsServer3D.AreaGetParam(a, PhysicsServer3D.AreaParameter.Gravity).AsSingle(), 25f, 1e-4f, "gravity round-trip");
		Assert.ExpectVecNear(PhysicsServer3D.AreaGetParam(a, PhysicsServer3D.AreaParameter.GravityVector).AsVector3(), new Vector3(0, -1, 0), 1e-4f, "gravity vector round-trip");
		Assert.ExpectNear(PhysicsServer3D.AreaGetParam(a, PhysicsServer3D.AreaParameter.Priority).AsSingle(), 4f, 1e-4f, "priority round-trip");
		Assert.ExpectNear(PhysicsServer3D.AreaGetParam(a, PhysicsServer3D.AreaParameter.WindForceMagnitude).AsSingle(), 2f, 1e-4f, "wind magnitude stored (documented no-op)");
		yield return Wait.Frame();
	}
	static IEnumerator MonitorCallbackCleared()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0));
		int events = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant s, Variant r, Variant i, Variant a, Variant b) => events++));
		var b1 = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		yield return Wait.Frames(10);
		int afterClear0 = events;
		PhysicsServer3D.AreaSetMonitorCallback(area, new Callable()); // remove
		var b2 = w.MakeBody(w.Box(0.3f), new Vector3(0.2f, 5, 0));
		yield return Wait.Frames(20);
		Assert.Expect(events == afterClear0, "no events after callback cleared");
	}
	static IEnumerator ShapeBookkeeping()
	{
		using var w = new PhysxWorld(false);
		var area = PhysicsServer3D.AreaCreate();
		w.TrackArea(area);
		PhysicsServer3D.AreaSetSpace(area, w.Space);
		var s1 = w.Box(1, 1, 1);
		var s2 = w.Sphere(1f);
		PhysicsServer3D.AreaAddShape(area, s1, Transform3D.Identity);
		PhysicsServer3D.AreaAddShape(area, s2, new Transform3D(Basis.Identity, new Vector3(5, 0, 0)));
		Assert.Expect(PhysicsServer3D.AreaGetShapeCount(area) == 2, "two shapes added");
		PhysicsServer3D.AreaSetShapeTransform(area, 1, new Transform3D(Basis.Identity, new Vector3(7, 0, 0)));
		Assert.ExpectVecNear(PhysicsServer3D.AreaGetShapeTransform(area, 1).Origin, new Vector3(7, 0, 0), 1e-4f, "shape transform round-trip");
		PhysicsServer3D.AreaSetShapeDisabled(area, 0, true);
		PhysicsServer3D.AreaSetShapeDisabled(area, 0, false);
		PhysicsServer3D.AreaRemoveShape(area, 0);
		Assert.Expect(PhysicsServer3D.AreaGetShapeCount(area) == 1, "one shape after remove");
		PhysicsServer3D.AreaClearShapes(area);
		Assert.Expect(PhysicsServer3D.AreaGetShapeCount(area) == 0, "no shapes after clear");
		yield return Wait.Frame();
	}
	static IEnumerator ExitByFalling()
	{
		using var w = new PhysxWorld(false);
		w.AddFloor(-30f);
		var area = w.MakeArea(w.Box(1, 1, 1), new Vector3(0, 5, 0));
		int exits = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant status, Variant rid, Variant id, Variant a, Variant b) =>
		{
			if (status.AsInt32() != 0) exits++;
		}));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 5.2f, 0));
		yield return Wait.UntilOrFail(() => exits >= 1 && w.Pos(b).Origin.Y < 2f, 400, "body falls out of area");
		Assert.Expect(exits >= 1, "exit fired when body left the volume");
	}
	// F-13: a monitoring-only area sweeping across a sleeping body must track
	// the overlap (monitor callbacks keep firing) WITHOUT waking it.
	static IEnumerator MonitoringMoveKeepsSleep()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Sphere(0.25f), new Vector3(0, 3, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, true);

		int events = 0;
		var area = w.MakeArea(w.Box(1f, 2f, 1f), new Vector3(0, 3, 0));
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From(
			(Variant s, Variant r, Variant i, Variant a, Variant b2) => { events++; }));

		// Sweep the area across the sleeping body and back: MakeArea baked the
		// position into the shape's LOCAL transform, so the sweep pose carries
		// only the x offset.
		for (int f = 0; f < 90; f++)
		{
			float x = -3f + (f % 30) * 0.2f;
			PhysicsServer3D.AreaSetTransform(area, new Transform3D(Basis.Identity, new Vector3(x, 0, 0)));
			yield return Wait.Frame();
		}
		Assert.Expect(events > 0, $"monitoring still tracks the sleeping body ({events} events)");
		bool asleep = (bool)PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.Sleeping);
		Assert.Expect(asleep, "sleeping body stays asleep under a monitoring-only moving area");
	}
}
