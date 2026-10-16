// Motion (body_test_motion): sweeps used by character controllers — hit
// fractions, normals, collider ids, depenetration recovery, slopes, corners,
// and multi-collision results.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class MotionTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-MOVE-001", "sweep hits floor at sane fraction with up normal", SweepHitsFloor);
		s.Add("PHYSX-MOVE-002", "free sweep reports no collision", SweepFree);
		s.Add("PHYSX-MOVE-003", "sweep reports collider rid, safe/unsafe fractions, travel", SweepFields);
		s.Add("PHYSX-MOVE-004", "penetrating start recovers upward", SweepDepenetration);
		s.Add("PHYSX-MOVE-005", "initial penetration reports remainder collision", SweepInitialPenetration);
		s.Add("PHYSX-MOVE-006", "sweep into wall is blocked laterally", SweepWall);
		s.Add("PHYSX-MOVE-007", "sweep down a slope hits with slanted normal", SweepSlope);
		s.Add("PHYSX-MOVE-008", "corner sweep between two walls picks a deflection", SweepCorner);
		s.Add("PHYSX-MOVE-009", "sweep respects collision exceptions", SweepException);
		s.Add("PHYSX-MOVE-010", "kinematic body sweeps use its own shapes", SweepOwnShape);
		s.Add("PHYSX-MOVE-011", "sweep across tiled concave floor seam blocks at the surface, not the seam", SweepMeshSeam);
		s.Add("PHYSX-MOVE-012", "sweep into concave wall seam reports the face normal at contact", SweepMeshWallNormal);
		s.Add("PHYSX-MOVE-013", "mesh-hit normal matches rest_info on the same setup", SweepMeshNormalMatchesRestInfo);
		s.Add("PHYSX-MOVE-014", "recovery ignores rest-separation within the margin slack", RecoverMarginSlack);
		s.Add("PHYSX-MOVE-015", "recovery applies 0.4 per pass (partial per call)", RecoverScaled);
		s.Add("PHYSX-MOVE-016", "overlapped start is disregarded: motion reaches the wall behind it", SweepOverlapDisregard);
		s.Add("PHYSX-MOVE-017", "deeply embedded start is stuck: blocked at fraction 0 with contact depth", SweepStuck);
		s.Add("PHYSX-MOVE-018", "multi-shape body reports the swept shape's local index", SweepLocalShape);
		s.Add("PHYSX-MOVE-019", "recovery weights contacts by collision_priority", RecoverPriorityWeighted);
		s.Add("PHYSX-MOVE-020", "contact collider velocity includes the angular lever arm", SweepColliderContactVelocity);
	}

	static IEnumerator SweepHitsFloor()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 2, 0)), new Vector3(0, -3, 0));
		Assert.Expect(hit, "sweep hits floor");
		Assert.Expect(r.GetCollisionSafeFraction() > 0.1f && r.GetCollisionSafeFraction() < 0.9f,
			$"safe fraction mid-range (got {r.GetCollisionSafeFraction():F3})");
		Assert.Expect(r.GetCollisionNormal(0).Y > 0.7f, "floor normal up");
		Assert.Expect(r.GetColliderRid(0) == w.FloorRid, "collider rid is floor");
		yield return Wait.Frame();
	}
	static IEnumerator SweepFree()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		var (hit, _) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 2, 0)), new Vector3(0, 0.5f, 0));
		Assert.Expect(!hit, "upward sweep in open air reports no collision");
		yield return Wait.Frame();
	}
	static IEnumerator SweepFields()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 2, 0)), new Vector3(0, -3, 0));
		Assert.Require(hit, "hit expected");
		Assert.Expect(r.GetCollisionUnsafeFraction() >= r.GetCollisionSafeFraction(), "unsafe >= safe fraction");
		Assert.Expect(r.GetTravel().Y < 0f, "travel is downward");
		Assert.Expect(Math.Abs(r.GetTravel().Y + 3f * r.GetCollisionSafeFraction()) < 0.25f, "travel consistent with safe fraction");
		Assert.Expect(r.GetColliderShape(0) >= 0, "collider shape index valid");
		Assert.Expect(PhysxWorld.Finite(r.GetRemainder()), "remainder finite");
		yield return Wait.Frame();
	}
	static IEnumerator SweepDepenetration()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		// recovery_as_collision mirrors CharacterBody3D: recovery counts as a
		// collision only when explicitly requested, and then only with actual
		// contacts (Godot contract: body_test_motion never returns true with
		// an empty collision list).
		//
		// The module's recovery ejects ACTUAL penetration with 0.4-scaled
		// passes (~0.174 of the 0.2 embedment; margin-inflated speculative
		// contacts are a documented module gap), so a deeper-than-slack
		// overlap survives into the cast phase and the body is stuck: travel
		// is the recovery alone and the collision carries the residual depth.
		// (godot_physics margin-inflates its recovery, fully ejects, and
		// reports a free motion here.)
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 0.2f, 0)), new Vector3(0, 0.5f, 0), recoveryAsCollision: true);
		Assert.Expect(hit, "penetrating motion reports collision");
		Assert.Expect(r.GetCollisionCount() > 0, "reported collision has contacts");
		Assert.Expect(r.GetTravel().Y > 0.1f && r.GetTravel().Y < 0.25f,
			$"travel is the partial recovery eject (travel.y={r.GetTravel().Y:F2})");
		yield return Wait.Frame();
	}
	static IEnumerator SweepInitialPenetration()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		// Start half-inside the floor; the remainder of the motion must still resolve.
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 0.1f, 0)), new Vector3(0, -0.5f, 0));
		Assert.Expect(hit, "initially-penetrating sweep reports collision");
		Assert.Expect(PhysxWorld.Finite(r.GetTravel()), "travel finite after initial penetration");
		yield return Wait.Frame();
	}
	static IEnumerator SweepWall()
	{
		using var w = new PhysxWorld();
		w.MakeStatic(w.Box(0.5f, 4, 4), new Vector3(4, 2, 0));
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(0, 2, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(0, 2, 0)), new Vector3(4, 0, 0));
		Assert.Expect(hit, "sweep into wall blocked");
		Assert.Expect(r.GetCollisionSafeFraction() > 0.1f && r.GetCollisionSafeFraction() < 1f, "fraction sane");
		Assert.Expect(Mathf.Abs(r.GetCollisionNormal(0).X) > 0.7f, "wall normal horizontal");
		yield return Wait.Frame();
	}
	static IEnumerator SweepSlope()
	{
		using var w = new PhysxWorld(false);
		var rot = Basis.FromEuler(new Vector3(0, 0, Mathf.DegToRad(30)));
		w.MakeStatic(w.Box(10, 0.5f, 4), new Vector3(0, 1, 0), shapeXf: new Transform3D(rot, Vector3.Zero));
		var body = w.MakeKinematic(w.Box(0.3f), new Vector3(0, 5, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(-3, 5, 0)), new Vector3(0, -6, 0));
		Assert.Require(hit, "sweep reaches slope");
		var n = r.GetCollisionNormal(0);
		Assert.Expect(n.Y > 0.5f && n.Y < 0.99f, $"slope normal is slanted (got {n})");
		Assert.Expect(Mathf.Abs(n.X) > 0.15f, "slope normal has lateral component");
		yield return Wait.Frame();
	}
	static IEnumerator SweepCorner()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.25f, 3, 3), new Vector3(2, 1.5f, 0));  // wall ahead (+x)
		w.MakeStatic(w.Box(3, 3, 0.25f), new Vector3(1, 1.5f, 2));   // wall right (+z)
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(0, 1, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(0, 1, 0)), new Vector3(4, 0, 2));
		Assert.Expect(hit, "corner sweep collides");
		Assert.Expect(r.GetCollisionCount() >= 1, "at least one collision reported");
		Assert.Expect(PhysxWorld.Finite(r.GetTravel()), "corner travel finite");
		yield return Wait.Frame();
	}
	static IEnumerator SweepException()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		PhysicsServer3D.BodyAddCollisionException(body, w.FloorRid);
		var (hit, _) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 2, 0)), new Vector3(0, -3, 0));
		Assert.Expect(!hit, "sweep ignores excepted floor");
		PhysicsServer3D.BodyRemoveCollisionException(body, w.FloorRid);
		var (hit2, _) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(3, 2, 0)), new Vector3(0, -3, 0));
		Assert.Expect(hit2, "sweep hits floor after removing exception");
		yield return Wait.Frame();
	}
	static IEnumerator SweepOwnShape()
	{
		using var w = new PhysxWorld();
		var tall = w.MakeKinematic(w.Capsule(0.3f, 1.8f), new Vector3(6, 5, 0));
		var (hit, r) = w.TestMotion(tall, new Transform3D(Basis.Identity, new Vector3(6, 2, 0)), new Vector3(0, -3, 0));
		Assert.Expect(hit, "capsule sweep hits floor");
		Assert.Expect(r.GetCollisionSafeFraction() > 0.05f && r.GetCollisionSafeFraction() < 0.6f,
			"capsule (long shape) contacts earlier than a box would");
		yield return Wait.Frame();
	}

	// ---------------------------------------------------- mesh internal edges
	// Flat floor from 4 triangles sharing a straight internal seam along Z at
	// x=0 (plus two diagonal seams) so an X-travel path crosses it head-on.
	static Rid TiledFloor(PhysxWorld w) => w.Concave(
		new Vector3(-8, 0, -8), new Vector3(0, 0, -8), new Vector3(-8, 0, 8),
		new Vector3(-8, 0, 8), new Vector3(0, 0, -8), new Vector3(0, 0, 8),
		new Vector3(0, 0, -8), new Vector3(8, 0, -8), new Vector3(0, 0, 8),
		new Vector3(0, 0, 8), new Vector3(8, 0, -8), new Vector3(8, 0, 8));

	// Wall plane x=5 built from 2 triangles whose shared diagonal passes
	// through (5, 0.5, 6.8) — exactly where the wall test's contact lands.
	static Rid SeamedWall(PhysxWorld w) => w.Concave(
		new Vector3(5, 0, -8), new Vector3(5, 4, -8), new Vector3(5, 0, 8),
		new Vector3(5, 0, 8), new Vector3(5, 4, -8), new Vector3(5, 4, 8));

	static IEnumerator SweepMeshSeam()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(TiledFloor(w), new Vector3(0, 0, 0));
		var body = w.MakeKinematic(w.Sphere(0.5f), new Vector3(3, 5, 0));
		yield return Wait.Frames(3);
		// Sphere bottom starts 0.05 above the floor and the motion drops 1 m
		// over 12 m, so the true contact fraction is ~0.05. The seam at x=0
		// is crossed at fraction 0.5 — a premature block there is the artifact.
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(-6, 0.55f, 0)), new Vector3(12, -1, 0));
		Assert.Expect(hit, "descending sweep across the seam hits the floor");
		Assert.Expect(r.GetCollisionSafeFraction() < 0.2f,
			$"blocked at the floor contact, not the seam (safe={r.GetCollisionSafeFraction():F3})");
		Assert.Expect(r.GetCollisionNormal(0).Y > 0.9f, "floor face normal, not an edge normal");
		yield return Wait.Frame();
	}
	static IEnumerator SweepMeshWallNormal()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(SeamedWall(w), new Vector3(0, 0, 0));
		var body = w.MakeKinematic(w.Sphere(0.5f), new Vector3(3, 5, 0));
		yield return Wait.Frames(3);
		// Approach the wall with a slight descent; the sphere contacts it at
		// center x=4.5 (fraction 0.75), right on the wall's internal diagonal
		// seam. The reported normal must be the face normal (-1,0,0).
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(0, 0.5f, 6.8f)), new Vector3(6, -0.001f, 0));
		Assert.Expect(hit, "sweep into the seamed wall is blocked");
		Assert.Expect(r.GetCollisionSafeFraction() > 0.6f && r.GetCollisionSafeFraction() < 0.9f,
			$"contact at the wall, not earlier (safe={r.GetCollisionSafeFraction():F3})");
		Assert.Expect(r.GetCollisionNormal(0).X < -0.9f, "wall face normal (-1,0,0), not an edge normal");
		yield return Wait.Frame();
	}
	static IEnumerator SweepMeshNormalMatchesRestInfo()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(TiledFloor(w), new Vector3(0, 0, 0));
		w.MakeStatic(SeamedWall(w), new Vector3(0, 0, 0));
		var body = w.MakeKinematic(w.Sphere(0.5f), new Vector3(3, 5, 0));
		yield return Wait.Frames(3);
		// Floor: descending sweep normal vs rest_info normal for a sphere
		// penetrating the same mesh floor.
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(0, 0.55f, 0)), new Vector3(0, -1, 0));
		Assert.Expect(hit, "floor sweep hits");
		Assert.Expect(r.GetCollisionNormal(0).Y > 0.9f, "sweep normal up on mesh floor");
		var restFloor = w.RestInfo(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(0, 0.45f, 0)));
		Assert.Expect(restFloor.Count > 0 && restFloor["normal"].AsVector3().Y > 0.9f,
			"rest_info normal up on mesh floor (agrees with sweep)");

		// Wall: lateral sweep normal vs rest_info normal against the same wall.
		// The sphere starts 0.05 above the floor so the floor's zero-distance
		// touch cannot shadow the wall hit in the single closest-hit sweep.
		var (hitW, rW) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(0, 0.55f, 0)), new Vector3(6, 0, 0));
		Assert.Expect(hitW, "wall sweep hits");
		Assert.Expect(rW.GetCollisionNormal(0).X < -0.9f, "sweep normal -X on mesh wall");
		var restWall = w.RestInfo(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(4.6f, 0.55f, 0)));
		Assert.Expect(restWall.Count > 0 && restWall["normal"].AsVector3().X < -0.9f,
			"rest_info normal -X on mesh wall (agrees with sweep)");
		yield return Wait.Frame();
	}
	// godot_physics contract (godot_space_3d.cpp): recovery ignores penetration
	// shallower than margin * 0.05 -- that is rest separation, not a stuck
	// body -- so body_test_motion must report a clean, unmodified motion.
	static IEnumerator RecoverMarginSlack()
	{
		using var w = new PhysxWorld(false);
		var wall = w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(0, 5, 0));
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		// Embedded 0.002 into the wall face at x = 0.5: center 0.5 + 0.4 - 0.002.
		// Margin 0.1 -> slack 0.005 -> this overlap is within the slack.
		var from = new Transform3D(Basis.Identity, new Vector3(0.898f, 5, 0));
		var p = new PhysicsTestMotionParameters3D
		{
			From = from,
			Motion = new Vector3(0.01f, 0, 0),
			RecoveryAsCollision = false,
			Margin = 0.1f,
		};
		var r = new PhysicsTestMotionResult3D();
		PhysicsServer3D.BodyTestMotion(body, p, r);
		Assert.ExpectNear(r.GetTravel().X, 0.01f, 1e-4f,
			$"slack-covered penetration is not recovered (travel.x={r.GetTravel().X:F4})");
		Assert.Expect(r.GetCollisionCount() == 0, "slack-covered overlap is not reported as a collision");
		yield return Wait.Frame();
	}

	// godot_physics contract: each recovery pass applies 40% of the remaining
	// MTD (4 passes), so one call recovers most -- but never all -- of a deep
	// embedment. Embedded 0.2 -> expect roughly 0.2 * (1 - 0.6^4) ~ 0.174.
	static IEnumerator RecoverScaled()
	{
		using var w = new PhysxWorld(false);
		var wall = w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(0, 5, 0));
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		// Embedded 0.2 into the wall face at x = 0.5: center 0.5 + 0.4 - 0.2.
		var from = new Transform3D(Basis.Identity, new Vector3(0.7f, 5, 0));
		var p = new PhysicsTestMotionParameters3D
		{
			From = from,
			Motion = new Vector3(0.01f, 0, 0),
			RecoveryAsCollision = false,
			Margin = 0.01f,
		};
		var r = new PhysicsTestMotionResult3D();
		PhysicsServer3D.BodyTestMotion(body, p, r);
		float recovered = r.GetTravel().X - 0.01f;
		Assert.Expect(recovered > 0.12f && recovered < 0.195f,
			$"0.4-scaled recovery recovers part of the embedment ({recovered:F3} of 0.2)");
		yield return Wait.Frame();
	}

	// godot_physics contract (godot_space_3d test_body_motion): objects the
	// mover already overlaps are DISREGARDED in the cast phase ("ignore
	// objects it's inside of"), so a forward blocker behind the overlapped one
	// still bounds the motion. Here the overlap sits within the recovery slack
	// (margin 0.1 -> slack 0.005 > the 0.002 embedment) so it survives into
	// the cast phase; the far wall 8 m ahead must be what blocks.
	static IEnumerator SweepOverlapDisregard()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(0, 5, 0));   // face at x = 0.5
		w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(9, 5, 0));   // face at x = 8.5
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		var p = new PhysicsTestMotionParameters3D
		{
			From = new Transform3D(Basis.Identity, new Vector3(0.898f, 5, 0)),
			Motion = new Vector3(8, 0, 0),
			RecoveryAsCollision = false,
			Margin = 0.1f,
		};
		var r = new PhysicsTestMotionResult3D();
		bool hit = PhysicsServer3D.BodyTestMotion(body, p, r);
		Assert.Expect(hit, "motion is blocked by the far wall");
		// Contact at body front x = 8.5: travel 7.202 of 8 -> fraction ~0.900.
		float safe = r.GetCollisionSafeFraction();
		Assert.Expect(safe > 0.85f && safe < 0.95f,
			$"blocked at the far wall, not the overlapped face (safe={safe:F3})");
		Assert.Expect(r.GetCollisionNormal(0).X < -0.9f, "far wall normal (-X)");
		yield return Wait.Frame();
	}

	// godot_physics contract: when an overlap deeper than the recovery slack
	// survives into the cast phase the body is STUCK -- safe = unsafe = 0 and
	// the collision carries the actual contact depth.
	static IEnumerator SweepStuck()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(0, 5, 0));
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(3, 5, 0));
		// Embedded 0.2 into the wall face at x = 0.5: recovery ejects only
		// ~0.174, so ~0.026 of overlap survives (slack is 5e-5 at margin 1e-3).
		var p = new PhysicsTestMotionParameters3D
		{
			From = new Transform3D(Basis.Identity, new Vector3(0.7f, 5, 0)),
			Motion = new Vector3(0.5f, 0, 0),
			RecoveryAsCollision = false,
			Margin = 0.001f,
		};
		var r = new PhysicsTestMotionResult3D();
		bool hit = PhysicsServer3D.BodyTestMotion(body, p, r);
		Assert.Expect(hit, "stuck body reports a collision");
		Assert.ExpectNear(r.GetCollisionSafeFraction(), 0f, 1e-3f,
			$"stuck: safe fraction 0 (got {r.GetCollisionSafeFraction():F3})");
		Assert.ExpectNear(r.GetCollisionUnsafeFraction(), 0f, 1e-3f, "stuck: unsafe fraction 0");
		Assert.Expect(r.GetCollisionCount() > 0, "stuck collision carries contacts");
		Assert.Expect(r.GetCollisionDepth(0) > 0.001f,
			$"contact depth reported (got {r.GetCollisionDepth(0):F3})");
		Assert.Expect(r.GetCollisionLocalShape(0) == 0, "local shape index of the single-shape body");
		yield return Wait.Frame();
	}

	// The reported local_shape must be the MOVER shape that produced the
	// contact (godot_space_3d tracks best_shape per mover shape), not a
	// constant 0.
	static IEnumerator SweepLocalShape()
	{
		using var w = new PhysxWorld();
		var body = w.MakeKinematic(w.Sphere(0.3f), new Vector3(6, 5, 0));
		// Shape 1 sits 1 m lower -> a downward sweep contacts it first.
		PhysicsServer3D.BodyAddShape(body, w.Box(0.4f), new Transform3D(Basis.Identity, new Vector3(0, -1, 0)));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, new Vector3(6, 4, 0)), new Vector3(0, -3, 0));
		Assert.Expect(hit, "downward sweep hits the floor");
		Assert.Expect(r.GetCollisionLocalShape(0) == 1,
			$"local shape is the lower box (got {r.GetCollisionLocalShape(0)})");
		yield return Wait.Frame();
	}

	// godot_space_3d weights each recovery contact by the collider's
	// collision_priority (normalized to average 1): a priority-0 wall
	// contributes nothing, so the body depenetrates fully toward it.
	static IEnumerator RecoverPriorityWeighted()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(0.1f, 5, 0));    // face at x = 0.6
		var wallR = w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(1.9f, 5, 0)); // face at x = 1.4
		PhysicsServer3D.BodySetCollisionPriority(wallR, 0f);
		var body = w.MakeKinematic(w.Box(0.45f), new Vector3(3, 5, 0));
		// Body half 0.45 at x = 1.0 spans 0.55..1.45: embedded 0.05 into BOTH faces.
		var p = new PhysicsTestMotionParameters3D
		{
			From = new Transform3D(Basis.Identity, new Vector3(1.0f, 5, 0)),
			Motion = new Vector3(0.01f, 0, 0),
			RecoveryAsCollision = false,
			Margin = 0.001f,
		};
		var r = new PhysicsTestMotionResult3D();
		PhysicsServer3D.BodyTestMotion(body, p, r);
		// Priority-1 left wall dominates: recovery_x ~ 0.05 * 0.4 * (2 contacts / weight 1) ~ 0.04.
		Assert.Expect(r.GetTravel().X > 0.03f,
			$"priority-0 wall contributes no recovery (travel.x={r.GetTravel().X:F3})");
		yield return Wait.Frame();
	}

	// Godot reports the collider's velocity AT the contact point, including
	// the angular lever arm (linear + omega x r) -- a spinning platform must
	// drag the contact tangentially.
	static IEnumerator SweepColliderContactVelocity()
	{
		using var w = new PhysxWorld(false);
		var platform = w.MakeBody(w.Box(1f, 0.5f, 1f), new Vector3(5, 0, 0), mass: 100f);
		w.SetAngVel(platform, new Vector3(0, 10, 0));
		var body = w.MakeKinematic(w.Box(0.4f), new Vector3(0, 0, 0));
		var (hit, r) = w.TestMotion(body, new Transform3D(Basis.Identity, Vector3.Zero), new Vector3(4, 0, 0));
		Assert.Expect(hit, "sweep reaches the platform (face at x = 4)");
		// Contact at x ~ 3.6: lever arm (-1.4, 0, 0) from the platform center,
		// omega = (0, 10, 0) -> omega x r = (0, 0, +14).
		var v = r.GetColliderVelocity(0);
		Assert.Expect(v.Z > 5f, $"contact velocity includes omega x r (got {v})");
		yield return Wait.Frame();
	}
}
