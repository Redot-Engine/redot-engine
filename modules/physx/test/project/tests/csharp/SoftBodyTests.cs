// Soft bodies: lifecycle/parameter plumbing plus behavioral tests. Each soft
// body resolves to a GPU PxDeformableVolume (CUDA) or the CPU XPBD solver; the
// assertions below hold on either path.

using System.Collections;
using Godot;
using Godot.Collections;

namespace PhysxTestProject.Tests;

internal static class SoftBodyTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-SOFT-001", "create/space/params plumbing", Plumbing);
		s.Add("PHYSX-SOFT-002", "collision layer/mask/exceptions plumbing", FilterPlumbing);
		s.Add("PHYSX-SOFT-003", "pin/unpin point bookkeeping", PinPlumbing);
		s.Add("PHYSX-SOFT-004", "deforms under gravity", DeformsUnderGravity);
		s.Add("PHYSX-SOFT-005", "collides with rigid bodies", CollidesWithRigid);
		s.Add("PHYSX-SOFT-006", "pinned point stays fixed while body sags", PinnedPointHolds);
		s.Add("PHYSX-SOFT-007", "collision exception: soft body ignores the excepted body, rests on others", ExceptionBehavior);
		s.Add("PHYSX-SOFT-008", "pin query reflects after the body builds (GPU path)", PinQueryAfterBuild);
		s.Add("PHYSX-SOFT-009", "solver-mode override forces CPU / GPU and round-trips", SolverModeOverride);
		s.Add("PHYSX-SOFT-010", "GPU cloth layer/mask matrix: matching layers catch the ball, mismatched pass through", ClothLayerMatrix);
	}

	// ------------------------------------------------- GPU cloth layer matrix
	// Builds a 3x3 world-space cloth grid through the server RID API, pins its
	// top row, and drops a rigid ball onto it. The module filter shader
	// collides a pair iff (layer0 & mask1) | (layer1 & mask0):
	//   match:    cloth L2/M1 + ball L1/M2 -> ball is caught
	//   mismatch: cloth L2/M2 + ball L1/M1 -> ball falls through
	static IEnumerator ClothLayerMatrix()
	{
		// ---- matching layers ----
		float caughtY;
		{
			using var w = new PhysxWorld(false);
			var cloth = VehicleApi.Call("cloth_create").AsRid();
			Assert.Require(cloth.IsValid, "GPU cloth created (CUDA build required)");
			VehicleApi.Call("cloth_set_space", cloth, w.Space);
			VehicleApi.Call("cloth_set_params", cloth, 0.02f, 0.5f, 0.9f, 0.1f, 0.03f, 1u);
			VehicleApi.Call("cloth_set_collision_layer_and_mask", cloth, 2u, 1u);

			// 3x3 grid, 2 m across, top row at y = 3 (world space).
			var verts = new Vector3[9];
			var idx = new System.Collections.Generic.List<int>();
			for (int y = 0; y < 3; y++)
			{
				for (int x = 0; x < 3; x++)
				{
					verts[y * 3 + x] = new Vector3(x - 1f, 3f - y, 0f);
				}
			}
			for (int y = 0; y < 2; y++)
			{
				for (int x = 0; x < 2; x++)
				{
					int v = y * 3 + x;
					idx.AddRange(new[] { v, v + 3, v + 1, v + 1, v + 3, v + 4 });
				}
			}
			VehicleApi.Call("cloth_build", cloth, verts, idx.ToArray(), Transform3D.Identity);
			// Pin the top row (input indices 0..2).
			VehicleApi.Call("cloth_set_pinned", cloth, new int[] { 0, 1, 2 });

			// Ball: layer 1, mask 2 -> matches the cloth pair.
			var ballShape = w.Sphere(0.3f);
			var ball = w.MakeBody(ballShape, new Vector3(0, 4.4f, 0), 1f, layer: 1, mask: 2u);
			PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.Mass, 1f);

			yield return Wait.Frames(120);
			caughtY = w.Pos(ball).Origin.Y;
			PhysicsServer3D.FreeRid(cloth);
		}

		// ---- mismatched layers ----
		using (var w = new PhysxWorld(false))
		{
			var cloth = VehicleApi.Call("cloth_create").AsRid();
			Assert.Require(cloth.IsValid, "GPU cloth created (second world)");
			VehicleApi.Call("cloth_set_space", cloth, w.Space);
			VehicleApi.Call("cloth_set_params", cloth, 0.02f, 0.5f, 0.9f, 0.1f, 0.03f, 1u);
			// Mismatch: cloth L2/M2 + ball L1/M1 -> no pair.
			VehicleApi.Call("cloth_set_collision_layer_and_mask", cloth, 2u, 2u);

			var verts = new Vector3[9];
			var idx = new System.Collections.Generic.List<int>();
			for (int y = 0; y < 3; y++)
			{
				for (int x = 0; x < 3; x++)
				{
					verts[y * 3 + x] = new Vector3(x - 1f, 3f - y, 0f);
				}
			}
			for (int y = 0; y < 2; y++)
			{
				for (int x = 0; x < 2; x++)
				{
					int v = y * 3 + x;
					idx.AddRange(new[] { v, v + 3, v + 1, v + 1, v + 3, v + 4 });
				}
			}
			VehicleApi.Call("cloth_build", cloth, verts, idx.ToArray(), Transform3D.Identity);
			VehicleApi.Call("cloth_set_pinned", cloth, new int[] { 0, 1, 2 });

			var ballShape = w.Sphere(0.3f);
			var ball = w.MakeBody(ballShape, new Vector3(0, 4.4f, 0), 1f, layer: 1, mask: 1u);
			PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.Mass, 1f);

			yield return Wait.Frames(120);
			float fellY = w.Pos(ball).Origin.Y;
			Assert.Expect(fellY < caughtY - 0.5f,
				$"layer matrix: caught at y={caughtY:F2} vs fell through to y={fellY:F2}");
			PhysicsServer3D.FreeRid(cloth);
		}
	}

	/// <summary>Unit cube mesh (8 verts / 12 tris) centered on the origin —
	/// watertight, so it tetrahedralizes for the GPU path.</summary>
	public static Rid MakeCubeMesh()
	{
		var mesh = RenderingServer.MeshCreate();
		var verts = new Vector3[] {
			new(-0.5f, -0.5f, -0.5f), new(0.5f, -0.5f, -0.5f), new(0.5f, 0.5f, -0.5f), new(-0.5f, 0.5f, -0.5f),
			new(-0.5f, -0.5f, 0.5f), new(0.5f, -0.5f, 0.5f), new(0.5f, 0.5f, 0.5f), new(-0.5f, 0.5f, 0.5f),
		};
		// Outward-facing triangles (right-hand rule), watertight cube surface.
		int[][] quads = {
			new[] { 1, 0, 3 }, new[] { 1, 3, 2 }, // -Z
            new[] { 4, 5, 6 }, new[] { 4, 6, 7 }, // +Z
            new[] { 5, 1, 2 }, new[] { 5, 2, 6 }, // +X
            new[] { 0, 4, 7 }, new[] { 0, 7, 3 }, // -X
            new[] { 3, 7, 6 }, new[] { 3, 6, 2 }, // +Y
            new[] { 0, 1, 5 }, new[] { 0, 5, 4 }, // -Y
        };
		var indices = new int[36];
		int k = 0;
		foreach (var q in quads)
		{
			indices[k++] = q[0]; indices[k++] = q[1]; indices[k++] = q[2];
		}
		var arrays = new Godot.Collections.Array();
		arrays.Resize((int)Mesh.ArrayType.Max);
		arrays[(int)Mesh.ArrayType.Vertex] = verts;
		arrays[(int)Mesh.ArrayType.Index] = indices;
		RenderingServer.MeshAddSurfaceFromArrays(mesh, RenderingServer.PrimitiveType.Triangles, arrays);
		return mesh;
	}

	static IEnumerator Plumbing()
	{
		using var w = new PhysxWorld(false);
		var sb = PhysicsServer3D.SoftBodyCreate();
		Assert.Require(sb.IsValid, "soft_body_create returns valid RID");
		PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
		Assert.Expect(PhysicsServer3D.SoftBodyGetSpace(sb) == w.Space, "soft body space round-trip");
		PhysicsServer3D.SoftBodySetTotalMass(sb, 3f);
		Assert.ExpectNear(PhysicsServer3D.SoftBodyGetTotalMass(sb), 3f, 1e-4f, "total mass round-trip");
		PhysicsServer3D.SoftBodySetLinearStiffness(sb, 0.8f);
		Assert.ExpectNear(PhysicsServer3D.SoftBodyGetLinearStiffness(sb), 0.8f, 1e-4f, "stiffness round-trip");
		PhysicsServer3D.SoftBodySetSimulationPrecision(sb, 7);
		Assert.Expect(PhysicsServer3D.SoftBodyGetSimulationPrecision(sb) == 7, "precision round-trip");
		PhysicsServer3D.SoftBodySetPressureCoefficient(sb, 0.5f);
		Assert.ExpectNear(PhysicsServer3D.SoftBodyGetPressureCoefficient(sb), 0.5f, 1e-4f, "pressure round-trip");
		PhysicsServer3D.SoftBodySetDampingCoefficient(sb, 0.2f);
		Assert.ExpectNear(PhysicsServer3D.SoftBodyGetDampingCoefficient(sb), 0.2f, 1e-4f, "damping round-trip");
		var bounds = PhysicsServer3D.SoftBodyGetBounds(sb);
		Assert.Expect(bounds.HasVolume() || bounds.Size.Length() >= 0f, "bounds readable");
		yield return Wait.Frames(10);
		Assert.Expect(PhysicsServer3D.SoftBodyGetSpace(sb) == w.Space, "space still set after stepping");
	}
	static IEnumerator FilterPlumbing()
	{
		using var w = new PhysxWorld(false);
		var sb = PhysicsServer3D.SoftBodyCreate();
		PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
		PhysicsServer3D.SoftBodySetCollisionLayer(sb, 3);
		PhysicsServer3D.SoftBodySetCollisionMask(sb, 7);
		Assert.Expect(PhysicsServer3D.SoftBodyGetCollisionLayer(sb) == 3, "layer round-trip");
		Assert.Expect(PhysicsServer3D.SoftBodyGetCollisionMask(sb) == 7, "mask round-trip");
		var other = w.MakeBody(w.Box(0.3f), new Vector3(0, 5, 0));
		PhysicsServer3D.SoftBodyAddCollisionException(sb, other);
		PhysicsServer3D.SoftBodyRemoveCollisionException(sb, other);
		yield return Wait.Frames(5);
		Assert.Expect(PhysxWorld.Finite(w.Pos(other)), "engine healthy with soft-body exception churn");
	}
	static IEnumerator PinPlumbing()
	{
		using var w = new PhysxWorld(false);
		var sb = PhysicsServer3D.SoftBodyCreate();
		PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
		PhysicsServer3D.SoftBodyPinPoint(sb, 2, true);
		Assert.Expect(PhysicsServer3D.SoftBodyIsPointPinned(sb, 2), "pin flag set");
		PhysicsServer3D.SoftBodyPinPoint(sb, 2, false);
		Assert.Expect(!PhysicsServer3D.SoftBodyIsPointPinned(sb, 2), "pin flag cleared");
		PhysicsServer3D.SoftBodyPinPoint(sb, 0, true);
		PhysicsServer3D.SoftBodyPinPoint(sb, 1, true);
		PhysicsServer3D.SoftBodyRemoveAllPinnedPoints(sb);
		Assert.Expect(!PhysicsServer3D.SoftBodyIsPointPinned(sb, 0) && !PhysicsServer3D.SoftBodyIsPointPinned(sb, 1),
			"remove_all_pinned_points clears pins");
		yield return Wait.Frame();
	}
	static IEnumerator DeformsUnderGravity()
	{
		using var w = new PhysxWorld(false);
		var mesh = MakeCubeMesh();
		var sb = PhysicsServer3D.SoftBodyCreate();
		try
		{
			PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
			PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(0, 3, 0)));
			PhysicsServer3D.SoftBodySetMesh(sb, mesh);

			yield return Wait.Frames(5);
			var b0 = PhysicsServer3D.SoftBodyGetBounds(sb);
			Assert.Expect(b0.Size.Length() > 0.01f, "bounds reflect the mesh (got size " + b0.Size + ")");
			float startY = b0.GetCenter().Y;
			Assert.Require(Mathf.Abs(startY - 3f) < 0.6f, "bounds placed near the transform (center y=" + startY + ")");

			yield return Wait.Frames(60);
			var b1 = PhysicsServer3D.SoftBodyGetBounds(sb);
			Assert.Expect(PhysxWorld.Finite(b1.Position) && PhysxWorld.Finite(b1.Size), "bounds stay finite while falling");
			Assert.Expect(b1.GetCenter().Y < startY - 1f,
				$"soft body fell under gravity (center y {startY:F2} -> {b1.GetCenter().Y:F2})");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sb);
			RenderingServer.FreeRid(mesh);
		}
	}
	// SOFT-1 regression: pin_point before + after the body BUILDS must be
	// reflected by is_point_pinned on BOTH paths — the old code queried the CPU
	// solver, which is never built on the GPU (PxDeformableVolume) path, so
	// pins looked lost there.
	static IEnumerator PinQueryAfterBuild()
	{
		using var w = new PhysxWorld(false);
		var mesh = MakeCubeMesh();
		var sb = PhysicsServer3D.SoftBodyCreate();
		try
		{
			PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
			PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(0, 3, 0)));
			PhysicsServer3D.SoftBodySetMesh(sb, mesh);
			PhysicsServer3D.SoftBodyPinPoint(sb, 0, true);
			PhysicsServer3D.SoftBodyPinPoint(sb, 7, true);
			yield return Wait.Frames(10); // build + a few steps
			Assert.Expect(PhysicsServer3D.SoftBodyIsPointPinned(sb, 0), "point 0 pinned after build");
			Assert.Expect(PhysicsServer3D.SoftBodyIsPointPinned(sb, 7), "point 7 pinned after build");
			Assert.Expect(!PhysicsServer3D.SoftBodyIsPointPinned(sb, 3), "unpinned point reads unpinned");
			PhysicsServer3D.SoftBodyPinPoint(sb, 0, false);
			Assert.Expect(!PhysicsServer3D.SoftBodyIsPointPinned(sb, 0), "unpin after build clears the query");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sb);
			RenderingServer.FreeRid(mesh);
		}
	}

	// soft_body_set_solver_mode: per-body override beats the project setting,
	// round-trips, and re-resolves the path on a built body.
	static IEnumerator SolverModeOverride()
	{
		using var w = new PhysxWorld(false);
		var mesh = MakeCubeMesh();
		var sb = PhysicsServer3D.SoftBodyCreate();
		try
		{
			PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
			PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(0, 3, 0)));
			PhysicsServer3D.SoftBodySetMesh(sb, mesh);
			yield return Wait.Frames(5); // built on the default (Auto) path

			VehicleApi.Call("soft_body_set_solver_mode", sb, 1); // force CPU
			yield return Wait.Frames(3);
			Assert.Expect(VehicleApi.Call("soft_body_get_solver_mode", sb).AsInt32() == 1, "CPU override round-trips");

			VehicleApi.Call("soft_body_set_solver_mode", sb, 2); // force GPU
			yield return Wait.Frames(3);
			Assert.Expect(VehicleApi.Call("soft_body_get_solver_mode", sb).AsInt32() == 2, "GPU override round-trips");

			VehicleApi.Call("soft_body_set_solver_mode", sb, -1); // back to Auto
			yield return Wait.Frames(3);
			Assert.Expect(VehicleApi.Call("soft_body_get_solver_mode", sb).AsInt32() == -1, "Auto override round-trips");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sb);
			RenderingServer.FreeRid(mesh);
		}
	}

	static IEnumerator CollidesWithRigid()
	{
		using var w = new PhysxWorld(true); // floor top at y=0
		var mesh = MakeCubeMesh();
		var sb = PhysicsServer3D.SoftBodyCreate();
		try
		{
			PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
			PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(0, 2.5f, 0)));
			PhysicsServer3D.SoftBodySetMesh(sb, mesh);
			PhysicsServer3D.SoftBodySetDampingCoefficient(sb, 0.3f);

			yield return Wait.Frames(180);
			var b = PhysicsServer3D.SoftBodyGetBounds(sb);
			Assert.Expect(PhysxWorld.Finite(b.Position) && PhysxWorld.Finite(b.Size), "bounds stay finite while settling");
			Assert.Expect(b.Position.Y > -0.25f,
				$"soft body rests on the floor instead of sinking through (bounds min y={b.Position.Y:F3})");
			Assert.Expect(b.GetCenter().Y < 2f, $"soft body actually dropped toward the floor (center y={b.GetCenter().Y:F3})");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sb);
			RenderingServer.FreeRid(mesh);
		}
	}
	static IEnumerator PinnedPointHolds()
	{
		using var w = new PhysxWorld(false);
		var mesh = MakeCubeMesh();
		var sb = PhysicsServer3D.SoftBodyCreate();
		try
		{
			PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
			PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(0, 3, 0)));
			PhysicsServer3D.SoftBodySetMesh(sb, mesh);
			yield return Wait.Frames(3);

			// Pin render vertex 0 (a bottom corner). Its world position at the
			// placed transform is origin + (-0.5, -0.5, -0.5).
			PhysicsServer3D.SoftBodyPinPoint(sb, 0, true);
			var pinnedStart = PhysicsServer3D.SoftBodyGetPointGlobalPosition(sb, 0);
			Assert.Expect(PhysxWorld.Finite(pinnedStart), "pinned point position readable");
			Assert.Expect(Mathf.Abs(pinnedStart.Y - 2.5f) < 0.2f, "pinned point starts at its placed position");

			yield return Wait.Frames(90);
			var now = PhysicsServer3D.SoftBodyGetPointGlobalPosition(sb, 0);
			var bounds = PhysicsServer3D.SoftBodyGetBounds(sb);
			Assert.Expect(PhysxWorld.Finite(now), "pinned point stays finite");
			Assert.Expect(now.DistanceTo(pinnedStart) < 0.35f,
				$"pinned point holds (moved {(now - pinnedStart).Length():F3} m) while bounds center y={bounds.GetCenter().Y:F2}");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sb);
			RenderingServer.FreeRid(mesh);
		}
	}
	static IEnumerator ExceptionBehavior()
	{
		using var w = new PhysxWorld(); // floor top at y=0
										// Two identical platforms; the soft body above the first excepts it.
		var excepted = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(0, 1, 0));
		w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(3, 1, 0));
		var mesh = MakeCubeMesh();
		var sbEx = PhysicsServer3D.SoftBodyCreate();
		var sbCtl = PhysicsServer3D.SoftBodyCreate();
		try
		{
			foreach (var (sb, x) in new[] { (sbEx, 0f), (sbCtl, 3f) })
			{
				PhysicsServer3D.SoftBodySetSpace(sb, w.Space);
				PhysicsServer3D.SoftBodySetTransform(sb, new Transform3D(Basis.Identity, new Vector3(x, 3, 0)));
				PhysicsServer3D.SoftBodySetMesh(sb, mesh);
			}
			// Works both before/after the GPU volume builds (slot baked at
			// build time and pushed on add).
			PhysicsServer3D.SoftBodyAddCollisionException(sbEx, excepted);

			yield return Wait.Frames(240);
			var bEx = PhysicsServer3D.SoftBodyGetBounds(sbEx);
			var bCtl = PhysicsServer3D.SoftBodyGetBounds(sbCtl);
			Assert.Expect(PhysxWorld.Finite(bEx.Position) && PhysxWorld.Finite(bCtl.Position), "bounds finite");
			Assert.Expect(bEx.Position.Y < 0.6f,
				$"excepted platform ignored: soft body fell through to the floor (min y={bEx.Position.Y:F3})");
			Assert.Expect(bCtl.Position.Y > 1.0f && bCtl.Position.Y < 2.0f,
				$"control platform supports the soft body (min y={bCtl.Position.Y:F3})");
		}
		finally
		{
			PhysicsServer3D.FreeRid(sbEx);
			PhysicsServer3D.FreeRid(sbCtl);
			RenderingServer.FreeRid(mesh);
		}
	}

}
