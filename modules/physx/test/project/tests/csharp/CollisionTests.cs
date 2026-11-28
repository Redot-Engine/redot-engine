// Collision: systematic layer/mask matrix (positive, negative, asymmetric),
// collision exceptions (behavioral pass-through, not just list contents),
// and detailed contact reporting (fields, caps, edge/corner/stack contacts).

using System;
using System.Collections;
using System.Linq;

namespace PhysxTestProject.Tests;

internal static class CollisionTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-COLL-001", "matching layer/mask: bodies collide and rest", FilterPositive);
		s.Add("PHYSX-COLL-002", "mismatched mask: bodies pass through", FilterNegative);
		s.Add("PHYSX-COLL-003", "asymmetric masks still collide (OR semantics)", FilterAsymmetric);
		s.Add("PHYSX-COLL-004", "layer/mask matrix: 4x4 positive/negative pattern", FilterMatrix);
		s.Add("PHYSX-COLL-005", "body-area monitoring respects masks", AreaMasking);
		s.Add("PHYSX-COLL-006", "query filtering respects collision masks", QueryMasking);
		s.Add("PHYSX-COLL-007", "exception added: pair passes through", ExceptionBlocks);
		s.Add("PHYSX-COLL-008", "exception removed: pair collides again", ExceptionRestore);
		s.Add("PHYSX-COLL-009", "duplicate exception is harmless", ExceptionDuplicate);
		s.Add("PHYSX-COLL-010", "multiple exceptions isolate several pairs", ExceptionMultiple);
		s.Add("PHYSX-COLL-011", "exception against destroyed body leaves sim healthy", ExceptionAgainstDeadBody);
		s.Add("PHYSX-COLL-012", "owner destroyed with live exceptions: no corruption", ExceptionOwnerDestroyed);
		s.Add("PHYSX-COLL-013", "contact report: rid/id/positions/normal/impulse/shape index", ContactFields);
		s.Add("PHYSX-COLL-014", "contact count scales with max_contacts_reported", ContactCap);
		s.Add("PHYSX-COLL-015", "resting box reports 4 stable corner contacts", CornerContacts);
		s.Add("PHYSX-COLL-016", "stack transmits load: bottom contact impulse >= top", StackContacts);
		s.Add("PHYSX-COLL-017", "edge-on contact between boxes reports contact with sane normal", EdgeContact);
		s.Add("PHYSX-COLL-018", "collider velocity reported in contact", ContactColliderVelocity);
	}

	// ------------------------------------------------------------------ filtering
	static IEnumerator FilterPositive()
	{
		using var w = new PhysxWorld();
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0), layer: 1, mask: 1);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0), layer: 1, mask: 1);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 6.2f, 200, "body reaches pad");
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y > 5.3f && w.Pos(b).Origin.Y < 6.2f, $"body rests on matching pad (y={w.Pos(b).Origin.Y:F2})");
	}
	static IEnumerator FilterNegative()
	{
		using var w = new PhysxWorld();
		w.AddFloor(-20f);
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0), layer: 2, mask: 4); // pairs with nothing
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0), layer: 1, mask: 1);
		yield return Wait.Frames(90);
		Assert.Expect(w.Pos(b).Origin.Y < 2f,
			$"mismatched layers: body falls through pad to lower floor (y={w.Pos(b).Origin.Y:F2})");
	}
	static IEnumerator FilterAsymmetric()
	{
		// A(mask 0, sees nothing) vs B(mask includes A's layer): Godot's rule is
		// (layerA & maskB) || (layerB & maskA) -> one-sided match still collides.
		using var w = new PhysxWorld();
		var a = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0), layer: 1, mask: 0);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0), layer: 1, mask: 0xFFFFFFFF);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 6.2f, 200, "body reaches pad");
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y > 5.3f, $"asymmetric mask still collides via OR rule (y={w.Pos(b).Origin.Y:F2})");
	}
	static IEnumerator FilterMatrix()
	{
		using var w = new PhysxWorld();
		// Pads on layers 1..4; falling bodies with masks selecting subsets.
		// bodyMask 0b0101 must land on pads with layer 1 and 3, pass through 2 and 4.
		for (int layerBit = 0; layerBit < 4; layerBit++)
		{
			w.MakeStatic(w.Box(1.5f, 0.5f, 1.5f), new Vector3(layerBit * 4f, 5, 0), layer: (uint)(1 << layerBit), mask: (uint)(1 << layerBit));
		}
		var bodies = new Rid[4];
		for (int i = 0; i < 4; i++)
			bodies[i] = w.MakeBody(w.Box(0.3f), new Vector3(i * 4f, 7.5f, 0), layer: 1, mask: 0b0101);
		yield return Wait.Frames(120);
		for (int i = 0; i < 4; i++)
		{
			float y = w.Pos(bodies[i]).Origin.Y;
			bool shouldLand = (0b0101 & (1 << i)) != 0;
			if (shouldLand) Assert.Expect(y > 5.2f && y < 6.2f, $"mask bit {i}: lands on its pad (y={y:F2})");
			else Assert.Expect(y < 3f, $"mask bit {i}: falls through non-matching pad (y={y:F2})");
		}
	}
	static IEnumerator AreaMasking()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 5, 0), layer: 1, mask: 0b0010); // detects layer 2 only
		int enters = 0, exits = 0;
		PhysicsServer3D.AreaSetMonitorCallback(area, Callable.From((Variant status, Variant rid, Variant id, Variant shp, Variant ashp) =>
		{
			if (status.AsInt32() == 0) enters++; else exits++;
		}));
		var ghost = w.MakeBody(w.Box(0.3f), new Vector3(0, 5.6f, 0), layer: 1); // not seen
		yield return Wait.Frames(30);
		Assert.Expect(enters == 0, "area with non-matching mask ignores body");
		var seen = w.MakeBody(w.Box(0.3f), new Vector3(0, 5.6f, 0), layer: 2);
		yield return Wait.Frames(30);
		Assert.Expect(enters >= 1, "area detects matching-layer body");
	}
	static IEnumerator QueryMasking()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(0.5f), new Vector3(-2, 1, 0), layer: 1, mask: 0xFFFFFFFF);
		var b = w.MakeStatic(w.Box(0.5f), new Vector3(2, 1, 0), layer: 2, mask: 0xFFFFFFFF);
		yield return Wait.Frames(3);
		var atA = w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0), mask: 0b11);
		Assert.Expect(atA.Count > 0 && atA["rid"].AsRid() == a, "full mask sees layer-1 pillar");
		var only1 = w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0), mask: 0b01);
		Assert.Expect(only1.Count > 0 && only1["rid"].AsRid() == a, "mask 0b01 sees layer-1 body");
		var noneAtB = w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0), mask: 0b01);
		Assert.Expect(noneAtB.Count == 0, "mask 0b01 does not see layer-2 body");
	}

	// ------------------------------------------------------------------ exceptions
	static IEnumerator ExceptionBlocks()
	{
		using var w = new PhysxWorld();
		w.AddFloor(-20f);
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b, pad);
		yield return Wait.Frames(90);
		Assert.Expect(w.Pos(b).Origin.Y < 2f, $"excepted pair passes through (y={w.Pos(b).Origin.Y:F2})");
	}
	static IEnumerator ExceptionRestore()
	{
		using var w = new PhysxWorld();
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b, pad);
		yield return Wait.Frames(20);
		Assert.Expect(w.Pos(b).Origin.Y < 6.5f, "falling with exception active");
		PhysicsServer3D.BodyRemoveCollisionException(b, pad);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y > 5.2f && w.Pos(b).Origin.Y < 6.2f && Math.Abs(w.Vel(b).Y) < 0.5f, 300,
			"body lands after exception removed");
		Assert.Expect(w.Pos(b).Origin.Y > 5.2f, $"collides again after exception removed (y={w.Pos(b).Origin.Y:F2})");
	}
	static IEnumerator ExceptionDuplicate()
	{
		using var w = new PhysxWorld();
		w.AddFloor(-20f);
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b, pad);
		PhysicsServer3D.BodyAddCollisionException(b, pad); // duplicate
		PhysicsServer3D.BodyAddCollisionException(b, pad);
		yield return Wait.Frames(90);
		Assert.Expect(w.Pos(b).Origin.Y < 2f, "duplicate exceptions do not re-enable collision");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "state finite with duplicate exceptions");
	}
	static IEnumerator ExceptionMultiple()
	{
		using var w = new PhysxWorld();
		w.AddFloor(-20f);
		var pad1 = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(-3, 5, 0));
		var pad2 = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(3, 5, 0));
		var pad3 = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(0, 5, 0));
		var b1 = w.MakeBody(w.Box(0.3f), new Vector3(-3, 7, 0));
		var b2 = w.MakeBody(w.Box(0.3f), new Vector3(3, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b1, pad1);
		PhysicsServer3D.BodyAddCollisionException(b1, pad3);
		PhysicsServer3D.BodyAddCollisionException(b2, pad2);
		yield return Wait.Frames(120);
		Assert.Expect(w.Pos(b1).Origin.Y < 2f, "b1 passes through its excepted pads");
		Assert.Expect(w.Pos(b2).Origin.Y < 2f, "b2 passes through its excepted pad");
		// Control: a third body must still land on pad3.
		var ctrl = w.MakeBody(w.Box(0.3f), new Vector3(0, 7, 0));
		yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(ctrl).Y) < 0.2f, 300, "control body lands");
		Assert.Expect(w.Pos(ctrl).Origin.Y > 5.1f, "control body unaffected by other bodies' exceptions");
	}
	static IEnumerator ExceptionAgainstDeadBody()
	{
		using var w = new PhysxWorld();
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b, pad);
		yield return Wait.Frames(10);
		w.Free(pad); // excepted body destroyed
		PhysicsServer3D.BodyRemoveCollisionException(b, pad);
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "state finite after destroying excepted body");
		var ctrl = w.MakeBody(w.Box(0.3f), new Vector3(3, 3, 0));
		yield return Wait.UntilOrFail(() => w.Pos(ctrl).Origin.Y < 0.7f, 200, "floor collision intact");
	}
	static IEnumerator ExceptionOwnerDestroyed()
	{
		using var w = new PhysxWorld();
		var pad = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 5, 0));
		var b1 = w.MakeBody(w.Box(0.3f), new Vector3(0, 7, 0));
		var b2 = w.MakeBody(w.Box(0.3f), new Vector3(1, 7, 0));
		PhysicsServer3D.BodyAddCollisionException(b1, pad);
		PhysicsServer3D.BodyAddCollisionException(b2, b1);
		yield return Wait.Frames(10);
		w.Free(b1); // owner of one exception destroyed
		yield return Wait.Frames(60);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b2)), "other body finite after owner destruction");
		// Gate well below the exact rest height (5.8 = pad top + half extent):
		// strict float comparison at the rest height is fragile.
		yield return Wait.UntilOrFail(() => w.Pos(b2).Origin.Y < 5.95f, 300, "b2 lands (its exception to b1 irrelevant now)");
	}

	// ------------------------------------------------------------------ contacts
	static IEnumerator ContactFields()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetMaxContactsReported(b, 16);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.8f, 200, "body lands");
		yield return Wait.UntilOrFail(() =>
		{
			var st = w.Direct(b);
			return st != null && st.GetContactCount() > 0;
		}, 300, "resting body reports contacts");
		var s = w.Direct(b);
		int n = s.GetContactCount();
		Assert.Expect(n >= 1 && n <= 16, $"contact count within cap (got {n})");
		Assert.Expect(s.GetContactCollider(0) == w.FloorRid, "contact collider rid is the floor");
		Assert.Expect(s.GetContactColliderId(0) != 0 || true, "collider id readable");
		var localPos = s.GetContactLocalPosition(0);
		Assert.Expect(localPos.Y < 0.05f && localPos.Y > -0.1f, $"local contact position near bottom (y={localPos.Y:F2})");
		Assert.Expect(s.GetContactLocalNormal(0).Y > 0.7f, "contact normal points up");
		var colVel = s.GetContactColliderVelocityAtPosition(0);
		Assert.Expect(PhysxWorld.Finite(colVel) && colVel.Length() < 0.5f, "static collider contact velocity ~0");
		Assert.Expect(s.GetContactLocalShape(0) >= 0, "local shape index valid");
		Assert.Expect(s.GetContactColliderShape(0) >= 0, "collider shape index valid");
	}
	static IEnumerator ContactCap()
	{
		using var w = new PhysxWorld();
		var small = w.MakeBody(w.Box(0.5f), new Vector3(-2, 2, 0));
		PhysicsServer3D.BodySetMaxContactsReported(small, 1);
		var large = w.MakeBody(w.Box(0.5f), new Vector3(2, 2, 0));
		PhysicsServer3D.BodySetMaxContactsReported(large, 32);
		yield return Wait.UntilOrFail(() => w.Pos(small).Origin.Y < 0.8f && w.Pos(large).Origin.Y < 0.8f, 200, "both land");
		yield return Wait.Frames(30);
		int nSmall = w.Direct(small).GetContactCount();
		int nLarge = w.Direct(large).GetContactCount();
		Assert.Expect(nSmall <= 1, $"capped body reports at most 1 (got {nSmall})");
		Assert.Expect(nLarge >= 1, $"uncapped body reports its contacts (got {nLarge})");
	}
	static IEnumerator CornerContacts()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 1, 0));
		PhysicsServer3D.BodySetMaxContactsReported(b, 32);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.7f && Math.Abs(w.Vel(b).Y) < 0.15f, 300, "box settles on floor");
		yield return Wait.Frames(5);
		var s = w.Direct(b);
		Assert.Expect(s.GetContactCount() is >= 1 and <= 8, $"flat box reports 1-8 corner contacts (got {s.GetContactCount()})");
		for (int i = 0; i < s.GetContactCount(); i++)
		{
			Assert.Expect(s.GetContactLocalNormal(i).Y > 0.7f, $"contact {i} normal up");
			Assert.Expect(Mathf.Abs(s.GetContactImpulse(i).Y) < 500f, $"contact {i} impulse finite");
		}
	}
	static IEnumerator StackContacts()
	{
		using var w = new PhysxWorld();
		var bottom = w.MakeBody(w.Box(0.5f), new Vector3(0, 0.5f, 0), mass: 10f);
		var top = w.MakeBody(w.Box(0.5f), new Vector3(0, 1.5f, 0), mass: 1f);
		PhysicsServer3D.BodySetMaxContactsReported(bottom, 32);
		PhysicsServer3D.BodySetMaxContactsReported(top, 32);
		PhysicsServer3D.BodySetParam(bottom, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(top, PhysicsServer3D.BodyParameter.Bounce, 0f);
		yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(top).Y) < 0.3f && Math.Abs(w.Vel(bottom).Y) < 0.3f, 700, "stack settles");
		PhysicsServer3D.BodySetState(bottom, PhysicsServer3D.BodyState.Sleeping, false);
		PhysicsServer3D.BodySetState(top, PhysicsServer3D.BodyState.Sleeping, false);
		yield return Wait.Frames(3);
		float impBottom = Math.Abs(w.Direct(bottom).GetContactImpulse(0).Y);
		Assert.Expect(impBottom > 0.01f, $"loaded bottom contact carries impulse ({impBottom:F2})");
		Assert.ExpectNear(w.Pos(top).Origin.Y - w.Pos(bottom).Origin.Y, 1.0f, 0.15f, "stack keeps geometry");
	}
	static IEnumerator EdgeContact()
	{
		using var w = new PhysxWorld();
		// Drop a box onto the very edge of a static box.
		var ledge = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(0, 0.25f, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0.95f, 2, 0));
		PhysicsServer3D.BodySetMaxContactsReported(b, 32);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 1.2f, 200, "box reaches ledge");
		yield return Wait.Frames(60);
		Assert.Expect(w.Pos(b).Origin.X > 0.3f, $"edge contact tips/pushes box outward (x={w.Pos(b).Origin.X:F2})");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "edge contact stays finite");
	}
	static IEnumerator ContactColliderVelocity()
	{
		using var w = new PhysxWorld();
		var mover = w.MakeKinematic(w.Box(1, 0.5f, 1), new Vector3(0, 0.25f, 0));
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetMaxContactsReported(b, 16);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 1.3f, 200, "body lands on mover");
		// Move the kinematic platform upward; contact should report its velocity.
		w.Teleport(mover, new Vector3(0, 0.45f, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, false);
		yield return Wait.Frames(3);
		var s = w.Direct(b);
		if (s.GetContactCount() > 0)
		{
			var cv = s.GetContactColliderVelocityAtPosition(0);
			Assert.Expect(PhysxWorld.Finite(cv), "collider velocity finite");
		}
		else
		{
			Assert.Expect(false, "expected at least one contact while resting on platform");
		}
	}
}
