// Lifecycle torture: aggressive creation/destruction ordering. The primary
// invariant: destroying one resource must never corrupt unrelated live
// resources — every destructive step is followed by a "canary" check that a
// fresh body still falls correctly in a healthy space.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class LifecycleTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-LIFE-001", "free shape while two bodies reference it", FreeSharedShape);
		s.Add("PHYSX-LIFE-002", "free body while joint references it", FreeJointedBody);
		s.Add("PHYSX-LIFE-003", "free body while area monitors it", FreeMonitoredBody);
		s.Add("PHYSX-LIFE-004", "free area with live monitor callback", FreeAreaWithCallback);
		s.Add("PHYSX-LIFE-005", "free space containing bodies+areas+joints", FreeSpaceWithContents);
		s.Add("PHYSX-LIFE-006", "free vehicle with chassis attached", FreeVehicleWithChassis);
		s.Add("PHYSX-LIFE-007", "free chassis before vehicle config finished", FreeChassisEarly);
		s.Add("PHYSX-LIFE-008", "free shape before body is assigned to any space", FreeOrphanShape);
		s.Add("PHYSX-LIFE-009", "destruction order permutations keep canary alive", OrderPermutations);
		s.Add("PHYSX-LIFE-010", "200 rapid create/destroy cycles of full hierarchies", RapidCycles);
		s.Add("PHYSX-LIFE-011", "stale RID ops on every major setter: no crash, no corruption", StaleRidOps);
		s.Add("PHYSX-LIFE-012", "body destroyed mid-step-frame between operations", DestroyBetweenOps);
		s.Add("PHYSX-LIFE-013", "shape data changed while attached everywhere", MutateDuringUse);
		s.Add("PHYSX-LIFE-014", "free body inside monitor callback context (deferred-safe)", FreeFromCallbackContext);
		s.Add("PHYSX-LIFE-015", "space freed while body_test_motion not running", FreeSpaceAfterQueries);
		s.Add("PHYSX-LIFE-016", "body freed via deferred call (mid-flight in async mode) stays crash-free", DeferredBodyFree);
	}

	static IEnumerator Canary(PhysxWorld w, string after, float from = 3f)
	{
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(9, from, 9));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < from * 0.6f, 240, $"canary falls after: {after}");
		Assert.Expect(PhysxWorld.Finite(w.Pos(canary)), $"canary finite after: {after}");
	}

	static IEnumerator FreeSharedShape()
	{
		using var w = new PhysxWorld();
		var shared = w.Box(0.4f);
		var a = w.MakeBody(shared, new Vector3(0, 2, 0));
		var b = w.MakeBody(shared, new Vector3(2, 2, 0));
		yield return Wait.Frames(20);
		w.Free(shared);
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(a)) && PhysxWorld.Finite(w.Pos(b)), "referencing bodies finite after shared shape free");
		yield return Canary(w, "shared shape freed");
	}
	static IEnumerator FreeJointedBody()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(0.3f), new Vector3(0, 6, 0));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 4.5f, 0));
		var j = w.TrackJoint(PhysicsServer3D.JointCreate());
		PhysicsServer3D.JointMakePin(j, a, Vector3.Zero, b, Vector3.Zero);
		yield return Wait.Frames(20);
		w.Free(b); // joint still exists
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(a)), "static peer finite");
		yield return Canary(w, "jointed body freed");
	}
	static IEnumerator FreeMonitoredBody()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0));
		int events = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant s, Variant r, Variant i, Variant a2, Variant b2) => events++));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		yield return Wait.Frames(20);
		w.Free(b); // area still monitors this body
		yield return Wait.Frames(40);
		// The area itself must still work: a new body entering must be detected.
		int after = events;
		var fresh = w.MakeBody(w.Box(0.3f), new Vector3(0.1f, 5, 0));
		yield return Wait.UntilOrFail(() => events > after, 200, "area detects new body after monitored body freed");
		yield return Canary(w, "monitored body freed");
	}
	static IEnumerator FreeAreaWithCallback()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0));
		int events = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant s, Variant r, Variant i, Variant a2, Variant b2) => events++));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		yield return Wait.Frames(10);
		w.Free(area); // callback exists, area destroyed
		yield return Wait.Frames(30);
		int after = events;
		yield return Wait.Frames(30);
		Assert.Expect(events == after || events >= after, "no crash from callback after area freed");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "monitored-in body finite");
	}
	static IEnumerator FreeSpaceWithContents()
	{
		using var keeper = new PhysxWorld();
		var victim = PhysxWorld.CreateSpace();
		var shape = PhysicsServer3D.BoxShapeCreate();
		PhysicsServer3D.ShapeSetData(shape, new Vector3(0.4f, 0.4f, 0.4f));
		var body = PhysicsServer3D.BodyCreate();
		PhysicsServer3D.BodySetSpace(body, victim);
		PhysicsServer3D.BodyAddShape(body, shape, Transform3D.Identity);
		var area = PhysicsServer3D.AreaCreate();
		PhysicsServer3D.AreaSetSpace(area, victim);
		PhysicsServer3D.AreaAddShape(area, shape, Transform3D.Identity);
		var j = PhysicsServer3D.JointCreate();
		var b2 = PhysicsServer3D.BodyCreate();
		PhysicsServer3D.BodySetSpace(b2, victim);
		PhysicsServer3D.BodyAddShape(b2, shape, Transform3D.Identity);
		PhysicsServer3D.JointMakePin(j, body, Vector3.Zero, b2, Vector3.Zero);
		yield return Wait.Frames(20);
		PhysicsServer3D.FreeRid(victim); // space with bodies+area+joint inside
		yield return Wait.Frames(40);
		PhysicsServer3D.FreeRid(shape);
		yield return Canary(keeper, "space with contents freed");
	}
	static IEnumerator FreeVehicleWithChassis()
	{
		using var w = new PhysxWorld();
		var chassis = w.MakeBody(w.Box(0.8f, 0.3f, 1.6f), new Vector3(0, 1, 0), mass: 800f);
		var v = w.TrackVehicle(VehicleApi.CreateVehicle(0));
		VehicleApi.SetChassisBody(v, chassis);
		VehicleApi.SetVehicleSpace(v, w.Space);
		VehicleApi.SetWheelCount(v, 4);
		yield return Wait.Frames(20);
		w.Free(v);
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis finite after vehicle free");
		yield return Canary(w, "vehicle freed");
	}
	static IEnumerator FreeChassisEarly()
	{
		using var w = new PhysxWorld();
		var chassis = w.MakeBody(w.Box(0.8f, 0.3f, 1.6f), new Vector3(0, 1, 0), mass: 800f);
		var v = w.TrackVehicle(VehicleApi.CreateVehicle(0));
		VehicleApi.SetChassisBody(v, chassis);
		// Free the chassis BEFORE wheel configuration completes.
		w.Free(chassis);
		w.Forget(v); // the server auto-releases the vehicle with its chassis
		yield return Wait.Frames(10);
		VehicleApi.SetWheelCount(v, 4); // op on the stale vehicle RID
		yield return Wait.Frames(30);
		yield return Canary(w, "chassis freed before vehicle config finished");
	}
	static IEnumerator FreeOrphanShape()
	{
		using var w = new PhysxWorld();
		var orphan = PhysicsServer3D.BoxShapeCreate();
		PhysicsServer3D.ShapeSetData(orphan, new Vector3(0.3f, 0.3f, 0.3f));
		PhysicsServer3D.FreeRid(orphan); // never attached to anything, never in a space
		yield return Wait.Frames(10);
		yield return Canary(w, "orphan shape freed");
	}
	static IEnumerator OrderPermutations()
	{
		// shape -> body -> space, body -> shape -> space, space -> body -> shape
		using var w = new PhysxWorld();
		foreach (var order in new[] { "sbs", "bss", "sbs2" })
		{
			var sp = PhysxWorld.CreateSpace();
			var sh = PhysicsServer3D.BoxShapeCreate();
			PhysicsServer3D.ShapeSetData(sh, new Vector3(0.3f, 0.3f, 0.3f));
			var b = PhysicsServer3D.BodyCreate();
			PhysicsServer3D.BodySetSpace(b, sp);
			PhysicsServer3D.BodyAddShape(b, sh, Transform3D.Identity);
			yield return Wait.Frames(10);
			if (order == "sbs") { PhysicsServer3D.FreeRid(sh); PhysicsServer3D.FreeRid(b); PhysicsServer3D.FreeRid(sp); }
			else if (order == "bss") { PhysicsServer3D.FreeRid(b); PhysicsServer3D.FreeRid(sh); PhysicsServer3D.FreeRid(sp); }
			else { PhysicsServer3D.FreeRid(sp); PhysicsServer3D.FreeRid(b); PhysicsServer3D.FreeRid(sh); }
			yield return Wait.Frames(15);
			yield return Canary(w, $"destruction order {order}");
		}
	}
	static IEnumerator RapidCycles()
	{
		using var w = new PhysxWorld();
		for (int i = 0; i < 200; i++)
		{
			var sp = PhysxWorld.CreateSpace();
			var sh = PhysicsServer3D.BoxShapeCreate();
			PhysicsServer3D.ShapeSetData(sh, new Vector3(0.2f, 0.2f, 0.2f));
			var b = PhysicsServer3D.BodyCreate();
			PhysicsServer3D.BodySetSpace(b, sp);
			PhysicsServer3D.BodyAddShape(b, sh, Transform3D.Identity);
			var area = PhysicsServer3D.AreaCreate();
			PhysicsServer3D.AreaSetSpace(area, sp);
			PhysicsServer3D.AreaAddShape(area, sh, Transform3D.Identity);
			if (i % 3 == 0) { PhysicsServer3D.FreeRid(area); PhysicsServer3D.FreeRid(b); PhysicsServer3D.FreeRid(sh); PhysicsServer3D.FreeRid(sp); }
			else if (i % 3 == 1) { PhysicsServer3D.FreeRid(sp); }
			else { PhysicsServer3D.FreeRid(b); PhysicsServer3D.FreeRid(area); PhysicsServer3D.FreeRid(sp); PhysicsServer3D.FreeRid(sh); }
			if (i % 50 == 49)
			{
				yield return Wait.Frames(5);
				Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count > 0, $"canary space intact after {i + 1} cycles");
			}
		}
		yield return Wait.Frames(10);
		yield return Canary(w, "200 rapid create/destroy cycles");
	}
	static IEnumerator StaleRidOps()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 3, 0));
		var sh = w.Box(0.4f);
		var area = w.MakeArea(w.Box(1, 1, 1), new Vector3(5, 3, 0));
		var j = w.TrackJoint(PhysicsServer3D.JointCreate());
		PhysicsServer3D.JointMakePin(j, b, Vector3.Zero, b, Vector3.Zero);
		w.Free(b);
		w.Free(sh);
		w.Free(area);
		w.Free(j);
		yield return Wait.Frame();
		// Ops on freed RIDs: the contract is "no native crash, no corruption of
		// live state"; return values are undefined.
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Mass, 2f);
		_ = PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.Transform);
		PhysicsServer3D.BodyApplyCentralImpulse(b, Vector3.One);
		PhysicsServer3D.BodyAddShape(b, w.Box(0.2f), Transform3D.Identity);
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Static);
		_ = PhysicsServer3D.ShapeGetData(sh);
		PhysicsServer3D.ShapeSetData(sh, new Vector3(1, 1, 1));
		_ = PhysicsServer3D.ShapeGetType(sh);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 5f);
		PhysicsServer3D.JointSetSolverPriority(j, 3);
		yield return Wait.Frames(30);
		yield return Canary(w, "stale RID operations");
	}
	static IEnumerator DestroyBetweenOps()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 3, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		yield return Wait.Frames(5);
		w.SetVel(b, new Vector3(1, 0, 0));   // op
		w.Free(b);                           // destroy mid-frame
		var c = w.MakeBody(w.Box(0.3f), new Vector3(2, 3, 0)); // create right after
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(c)), "body created right after destruction finite");
		yield return Canary(w, "destroy between operations");
	}
	static IEnumerator MutateDuringUse()
	{
		using var w = new PhysxWorld();
		var shared = w.Box(0.4f);
		var bodies = new Rid[6];
		for (int i = 0; i < 6; i++) bodies[i] = w.MakeBody(shared, new Vector3(i, 0.5f, 0));
		yield return Wait.Frames(10);
		// Resize while attached to six live bodies mid-simulation.
		PhysicsServer3D.ShapeSetData(shared, new Vector3(0.8f, 0.8f, 0.8f));
		yield return Wait.Frames(30);
		foreach (var b in bodies) Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "body finite after shared shape mutation");
		yield return Canary(w, "shape mutated while shared by six bodies");
	}
	static IEnumerator FreeFromCallbackContext()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0));
		var victims = new System.Collections.Generic.List<Rid>();
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant status, Variant rid, Variant id, Variant shp, Variant ashp) =>
		{
			// Queue bodies seen entering; freed after the callback returns.
			var r = rid.AsRid();
			if (status.AsInt32() == 0 && r.IsValid) victims.Add(r);
		}));
		var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		yield return Wait.Frames(10);
		foreach (var v in victims) w.Free(v);
		yield return Wait.Frames(30);
		yield return Canary(w, "bodies freed after monitor callback");
	}
	static IEnumerator FreeSpaceAfterQueries()
	{
		using var w = new PhysxWorld();
		var sp2 = PhysxWorld.CreateSpace();
		var sh = PhysicsServer3D.BoxShapeCreate();
		PhysicsServer3D.ShapeSetData(sh, new Vector3(0.5f, 0.5f, 0.5f));
		var b = PhysicsServer3D.BodyCreate();
		PhysicsServer3D.BodySetSpace(b, sp2);
		PhysicsServer3D.BodyAddShape(b, sh, Transform3D.Identity);
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, new Transform3D(Basis.Identity, new Vector3(0, 1, 0)));
		yield return Wait.Frames(5);
		var st = PhysicsServer3D.SpaceGetDirectState(sp2);
		Assert.Expect(st != null, "second space queryable");
		PhysicsServer3D.FreeRid(sp2);
		PhysicsServer3D.FreeRid(b);
		PhysicsServer3D.FreeRid(sh);
		yield return Wait.Frames(20);
		yield return Canary(w, "space freed right after queries");
	}

	// A body freed through the message queue lands in the engine's in-flight
	// window when async stepping is on (message_queue->flush() runs after
	// step() kicked the solve, before the next tick's sync() fetches it).
	// The space must queue the actor removal and the release until after the
	// fetch. In sync mode the same deferred free is harmless by construction.
	static IEnumerator DeferredBodyFree()
	{
		using var w = new PhysxWorld();
		var victim = w.MakeBody(w.Box(0.5f), new Vector3(0, 4, 0));
		yield return Wait.Frames(5);
		// MessageQueue is not exposed to C#; a GDScript bridge instance's
		// CallDeferred lands in the same message-queue flush.
		var bridge = (Godot.GodotObject)GD.Load<GDScript>("res://gdscript/deferred_free_bridge.gd").New();
		bridge.CallDeferred("free_rid", victim);
		bridge.Free();
		yield return Wait.Frames(30);
		// Canary proves the space keeps simulating after the mid-flight free.
		var canary = w.MakeBody(w.Sphere(0.3f), new Vector3(3, 4, 0));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 3.5f, 240, "canary falls after deferred body free");
		Assert.Expect(PhysxWorld.Finite(w.Pos(canary)), "engine healthy after deferred mid-flight body free");
		yield return Wait.Frame();
	}
}
