// Reduced-coordinate articulations (module API on the inner PhysXServer3D):
// chain building, drives, per-link shapes and filtering, velocity readback.
// Link indices are 0-based; joint types 0 FIX / 2 REVOLUTE; drive types
// 0 FORCE / 1 ACCELERATION; axes 0 TWIST / 1 SWING1 / 2 SWING2 / 3 X / 4 Y / 5 Z.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class ArticulationTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-ART-001", "revolute acceleration drive holds the target angle", DriveHoldsTarget);
		s.Add("PHYSX-ART-002", "link with layer+mask 0 is non-solid (sphere passes through)", MaskZeroNotSolid);
		s.Add("PHYSX-ART-003", "set_link_shape replaces the box (sphere rests on the new shape)", LinkShapeReplaces);
		s.Add("PHYSX-ART-004", "link velocity matches finite difference of the transform", LinkVelocityFiniteDifference);
		s.Add("PHYSX-ART-P-005", "link layer/mask round-trip [plumbing]", LinkFilterRoundtrip);
		s.Add("PHYSX-ART-006", "links can be added while the articulation is in a space", AttachThenBuild);
	}

	/// <summary>Fixed-base two-link chain: base box at p_basePos, link 1 hanging
	/// 0.5 below it on a revolute (twist) joint at the base's bottom face.</summary>
	static Rid Chain(PhysxWorld w, Vector3 basePos, Vector3 linkBox)
	{
		var art = ArticulationApi.Create();
		ArticulationApi.AddLink(art, -1, Transform3D.Identity,
			new Transform3D(Basis.Identity, basePos), 0, 1000f, new Vector3(0.1f, 0.5f, 0.1f));
		ArticulationApi.AddLink(art, 0,
			new Transform3D(Basis.Identity, new Vector3(0, -0.5f, 0)),
			new Transform3D(Basis.Identity, new Vector3(0, -0.5f, 0)),
			2, 1000f, linkBox);
		ArticulationApi.SetFixBase(art, true);
		return art;
	}

	static IEnumerator DriveHoldsTarget()
	{
		using var w = new PhysxWorld();
		var art = Chain(w, new Vector3(0, 3, 0), new Vector3(0.1f, 0.5f, 0.1f));
		ArticulationApi.SetDrive(art, 1, 0 /*TWIST*/, 200f, 20f, 0.5f, 0f, 1 /*ACCELERATION*/);
		// Links and flags first; the articulation joins the scene last.
		ArticulationApi.SetSpace(art, w.Space);
		ArticulationApi.Wake(art);
		Assert.Expect(ArticulationApi.GetLinkCount(art) == 2, "chain has two links");
		yield return Wait.Frames(180);
		Transform3D t = ArticulationApi.GetLinkTransform(art, 1);
		Quaternion q = t.Basis.GetRotationQuaternion();
		Assert.ExpectNear(q.AngleTo(Quaternion.Identity), 0.5f, 0.2f,
			$"revolute drive holds the target angle (got {q.AngleTo(Quaternion.Identity):F3})");
		// Rotation about the twist (X) axis: q = (sin(θ/2), 0, 0, cos(θ/2)).
		Assert.Expect(Mathf.Abs(q.X) > 0.1f && Mathf.Abs(q.Y) < 0.15f && Mathf.Abs(q.Z) < 0.15f,
			"rotation is about the twist (X) axis");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
	static IEnumerator MaskZeroNotSolid()
	{
		using var w = new PhysxWorld(); // floor top at y=0
		var art = Chain(w, new Vector3(0, 2, 0), new Vector3(0.5f, 0.1f, 0.5f)); // platform top ~1.6
																				 // Godot's layer test is an asymmetric OR: zeroing only the link's MASK
																				 // still leaves (link.layer & sphere.mask) matching. Zero both so no
																				 // direction of the test can select the pair. Both links get the zero
																				 // filter: with solid defaults, the sphere would otherwise rest on the
																				 // base post instead of testing the child platform's mask.
		ArticulationApi.SetLinkCollisionLayer(art, 0, 0);
		ArticulationApi.SetLinkCollisionMask(art, 0, 0);
		ArticulationApi.SetLinkCollisionLayer(art, 1, 0);
		ArticulationApi.SetLinkCollisionMask(art, 1, 0);
		ArticulationApi.SetSpace(art, w.Space);
		ArticulationApi.Wake(art);
		yield return Wait.Frames(10);
		var sphere = w.MakeBody(w.Sphere(0.4f), new Vector3(0, 3, 0));
		yield return Wait.UntilOrFail(() => w.Pos(sphere).Origin.Y < 1.0f, 300, "sphere passes through the masked-out link");
		yield return Wait.Frames(60);
		Assert.ExpectNear(w.Pos(sphere).Origin.Y, 0.4f, 0.25f, "sphere fell through the link and rests on the floor");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
	static IEnumerator LinkShapeReplaces()
	{
		using var w = new PhysxWorld();
		var art = Chain(w, new Vector3(0, 2, 0), new Vector3(0.5f, 0.1f, 0.5f));
		var box = w.Box(0.5f, 0.1f, 0.5f);
		ArticulationApi.SetLinkShape(art, 1, box, Transform3D.Identity);
		// The base link's default box is solid now (default layer/mask 1/1);
		// move its shape aside so the center drop path reaches the child
		// platform the test actually exercises.
		ArticulationApi.SetLinkShape(art, 0, w.Box(0.1f, 0.5f, 0.1f),
			new Transform3D(Basis.Identity, new Vector3(0.9f, 0, 0)));
		ArticulationApi.SetSpace(art, w.Space);
		ArticulationApi.Wake(art);
		yield return Wait.Frames(10);
		var sphere = w.MakeBody(w.Sphere(0.4f), new Vector3(0, 3, 0));
		yield return Wait.UntilOrFail(() => w.Sleeping(sphere) || w.Pos(sphere).Origin.Y < 1.3f, 300, "sphere reaches the link shape");
		yield return Wait.Frames(60);
		float y = w.Pos(sphere).Origin.Y;
		Assert.Expect(y > 1.3f && y < 2.3f, $"sphere rests on the replaced link shape (y={y})");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
	static IEnumerator LinkVelocityFiniteDifference()
	{
		using var w = new PhysxWorld();
		var art = ArticulationApi.Create();
		ArticulationApi.AddLink(art, -1, Transform3D.Identity,
			new Transform3D(Basis.Identity, new Vector3(0, 6, 0)), 0, 1000f, new Vector3(0.1f, 0.5f, 0.1f));
		ArticulationApi.AddLink(art, 0,
			new Transform3D(Basis.Identity, new Vector3(0, -0.5f, 0)),
			new Transform3D(Basis.Identity, new Vector3(0, -0.5f, 0)),
			2, 1000f, new Vector3(0.1f, 0.5f, 0.1f));
		// No fix_base: the whole chain free-falls (no drive; revolute left free).
		ArticulationApi.SetSpace(art, w.Space);
		ArticulationApi.Wake(art);
		yield return Wait.Frames(20);
		Vector3 p1 = ArticulationApi.GetLinkTransform(art, 0).Origin;
		float v1 = ((Godot.Collections.Dictionary)ArticulationApi.GetLinkVelocity(art, 0))["linear"].AsVector3().Y;
		yield return Wait.Frames(10);
		Vector3 p2 = ArticulationApi.GetLinkTransform(art, 0).Origin;
		float v2 = ((Godot.Collections.Dictionary)ArticulationApi.GetLinkVelocity(art, 0))["linear"].AsVector3().Y;
		// Under uniform gravity the mean of the endpoint velocities equals the
		// finite-difference velocity of the transform.
		float fd = (p2.Y - p1.Y) / (10f * PhysxWorld.Dt);
		float mean = 0.5f * (v1 + v2);
		Assert.ExpectNear(mean, fd, 0.4f, $"reported velocity matches d(pos)/dt (mean={mean:F2}, fd={fd:F2})");
		Assert.Expect(v2 < -1f, "chain is falling under gravity");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
	static IEnumerator LinkFilterRoundtrip()
	{
		using var w = new PhysxWorld();
		var art = Chain(w, new Vector3(0, 3, 0), new Vector3(0.1f, 0.5f, 0.1f));
		Assert.Expect(ArticulationApi.GetLinkCollisionLayer(art, 1) == 1u, "default link layer 1");
		Assert.Expect(ArticulationApi.GetLinkCollisionMask(art, 1) == 1u, "default link mask 1");
		ArticulationApi.SetLinkCollisionLayer(art, 1, 0x20u);
		ArticulationApi.SetLinkCollisionMask(art, 1, 0x40u);
		Assert.Expect(ArticulationApi.GetLinkCollisionLayer(art, 1) == 0x20u, "layer round-trip");
		Assert.Expect(ArticulationApi.GetLinkCollisionMask(art, 1) == 0x40u, "mask round-trip");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
	// The SDK forbids createLink on an in-scene articulation; the wrapper now
	// removes/re-adds the articulation around the call, so building a chain
	// after articulation_set_space must succeed and stay simulated.
	static IEnumerator AttachThenBuild()
	{
		using var w = new PhysxWorld();
		var art = ArticulationApi.Create();
		ArticulationApi.SetSpace(art, w.Space);
		int baseIdx = ArticulationApi.AddLink(art, -1, Transform3D.Identity,
			new Transform3D(Basis.Identity, new Vector3(0, 4, 0)), 0 /*FIX*/, 1000f,
			new Vector3(0.5f, 0.5f, 0.5f));
		Assert.Expect(baseIdx == 0, $"base link created while attached (idx={baseIdx})");
		int child = ArticulationApi.AddLink(art, baseIdx,
			new Transform3D(Basis.Identity, new Vector3(0, -0.5f, 0)),
			new Transform3D(Basis.Identity, new Vector3(0, 0.5f, 0)),
			2 /*REVOLUTE*/, 1000f, new Vector3(0.3f, 0.3f, 0.3f));
		Assert.Expect(child == 1, $"child link created while attached (idx={child})");
		Assert.Expect(ArticulationApi.GetLinkCount(art) == 2, "both links registered");
		ArticulationApi.SetFixBase(art, true);
		ArticulationApi.Wake(art);
		yield return Wait.Frames(30);
		var t = ArticulationApi.GetLinkTransform(art, child);
		Assert.Expect(PhysxWorld.Finite(t.Origin), "link transforms finite after in-scene build");
		Assert.Expect(t.Origin.Y < 4.2f, "child link hangs below the base (scene is simulating)");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}
}
