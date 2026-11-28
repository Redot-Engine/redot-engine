// Regressions: minimal reproductions of known bug classes and documented
// module quirks. Every future integration bug becomes PHYSX-REG-XXXX here and
// is never removed after the fix.

using System;
using System.Collections;
using System.IO;

namespace PhysxTestProject.Tests;

internal static class RegressionTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-REG-0001", "REG-0001: high-speed projectile tunnels without CCD, blocked with CCD", CcdTunneling);
		s.Add("PHYSX-REG-0002", "REG-0002: kinematic teleport is visible to queries next frame", KinematicTeleportQuery);
		s.Add("PHYSX-REG-0003", "REG-0003: sleeping body frozen mid-air (gravity skipped while asleep)", SleepingFrozen);
		s.Add("PHYSX-REG-0004", "REG-0004: cast_motion with initial overlap reports unobstructed (documented)", CastMotionOverlapQuirk);
		s.Add("PHYSX-REG-0005", "REG-0005: hit_from_inside synthetic hit has zero normal (documented)", InsideHitNormalQuirk);
		s.Add("PHYSX-REG-0006", "REG-0006: collision priority getter round-trips (RETRACTED no-op pin 2026-09-22 — C-10 made the getter return the stored value)", PriorityNoopQuirk);

		s.Add("PHYSX-REG-0007", "REG-0007: bodies in separate spaces cannot interact (isolation)", SpaceIsolation);
		s.Add("PHYSX-REG-0008", "REG-0008: freeing a shared shape does not corrupt users", SharedShapeFree);
		// ---- discovered by this suite against the current build; these FAIL on
		// ---- purpose and must be fixed in the module, not the tests:
		// REG-0022 space-destruction phantom: RETRACTED 2026-08-30 — the 'phantom' was the
		//        canary mid-fall: a fixed 40-frame wait sampled it at exactly y=0.82 (free-fall
		//        position at that time), deterministically. SPACE-009 now waits for touchdown.
		// ---- FIXED (kept as permanent regression guards; must stay PASSING):
		s.Add("PHYSX-REG-0009", "REG-0009: resting bodies auto-sleep (FIXED — guard)", NeverSleeps);
		s.Add("PHYSX-REG-0013", "REG-0013: box shape data follows Godot full-size semantics (FIXED — guard)", BoxSemantics);
		// REG-0022 space-destruction phantom: RETRACTED 2026-08-30 — the 'phantom' was the
		//        canary mid-fall: a fixed 40-frame wait sampled it at exactly y=0.82 (free-fall
		//        position at that time), deterministically. SPACE-009 now waits for touchdown.
		s.Add("PHYSX-REG-0014", "REG-0014: mesh shape on a default-rigid body must not crash the engine (FIXED — guard)", MeshOnDynamicNoCrash);
		s.Add("PHYSX-REG-0034", "REG-0034: heightmap attached data-less collides once shape_set_data arrives (WorldScape3D fall-through — FIXED — guard)", HeightmapLateDataAttach);
		s.Add("PHYSX-RT-002", "RT-002: a same-named runtime file whose content mismatches the installed list is reported stale (F-36 shadowing hardening — guard)", RuntimeFileStaleDetection);
		s.Add("PHYSX-GPU-REASON-001", "GPU-REASON-001: GPU-dynamics status/reason API contract + cached rd probe (F-37 clarity — guard)", GpuDynamicsReasonApi);
		s.Add("PHYSX-REG-0035", "REG-0035: empty convex/concave RIDs attach silently and recover on shape_set_data (F-34 — guard)", EmptyMeshShapesLateDataAttach);
		s.Add("PHYSX-REG-0036", "REG-0036: articulation link shape attached data-less retries on shape_set_data (F-33a — guard)", ArticulationLinkLateDataAttach);
		s.Add("PHYSX-HEIGHTMAP-NAN-001", "HEIGHTMAP-NAN-001: NaN holes keep finite bounds; an all-NaN map refuses to cook (F-35 — guard)", HeightmapNanHoles);
		// ---- FIXED (kept as permanent regression guards; must stay PASSING):
		// REG-0010 vehicle subsystem ...... FIXED 2026-08-21 (missing RigidBodyComponent in
		//        the vehicle2 sequence + vehicle not registered with the chassis's space;
		//        gravity now integrated by the sequence, manual addForce removed)
		// REG-0013 box data semantics ..... FIXED 2026-08-21 (module converts full size
		//        <-> PxBoxGeometry half extents; test harness flipped to full size)
		// REG-0014 mesh-on-dynamic crash .. FIXED 2026-08-21 (see MeshOnDynamicNoCrash)
		// REG-0015 kinematic->rigid ....... FIXED 2026-08-21 (body resumes falling)
		// REG-0017 RIGID_LINEAR rotation .. FIXED 2026-08-21 (angular axes locked)
		// REG-0030 contacts after sleep ... FIXED 2026-08-20 (pre-sleep contact snapshot)
		// REG-0031/0032 joint release ..... FIXED 2026-08-20 (release() wakes connected bodies)
		// REG-0033 vehicle_set_space ...... FIXED 2026-08-21 (idempotent + adoption registers)
		// REG-0034 data-less heightmap ... FIXED 2026-11-03 (WorldScape3D fall-through: the
		//        eager cook at body_add_shape() failed for a data-less HeightMapShape3D RID
		//        and shape_changed() skipped slots without a PxShape instance, so
		//        shape_set_data() never attached the collider. Module now retries failed
		//        slots via PhysXShapedObject3D::_try_attach_shape and stays silent for
		//        data-less shapes via PhysXShape3D::has_geometry_data)
		// ---- still open (primary failing test keeps each one visible):
		// REG-0011 area-area events ...... FIXED 2026-08-29 (areas are kinematic actors + detection shapes)
		// REG-0012 motion depenetration ... FIXED 2026-08-30 (MTD-aware cast; travel = recovery + swept motion)
		// REG-0016 restitution: RETRACTED 2026-08-29 — module combiner verified correct
		//        via contact-modify diagnostics (userData read-back + recontact pattern);
		//        the failure was a suite measurement bug (rebound apex tracked from
		//        pre-impact position). BODY-020 measurement fixed accordingly.
		// REG-0018 CCD diagonal: RETRACTED 2026-08-30 — CCD blocks the diagonal correctly
		//        (x parks at contact standoff); the old assertion measured the along-
		//        projection, which grows during the legitimate tangential wall slide
		// REG-0019 separation rays queried FIXED 2026-08-30 (preFilter returns eNONE for sep-ray shapes)
		// REG-0020 slanted concave rays: RETRACTED 2026-08-30 — module raycast correct;
		//        the diagnostic ray was cast at the resting test ball's column and hit the
		//        ball's top (normal 0,1,0). SHAPE-021 now raycasts the clean roof first
		// REG-0021 cone/convex pair narrowphase: RETRACTED 2026-08-30 — trajectory probe
		//        proved contacts ARE generated for every pair (impact arrest + deflection);
		//        old test demanded eternal balance on pointy/curved geometry. Pair protocol
		//        rewritten as impact + fall-arrest at the contact zone (PHYSX-SHAPE-P-*)
		// REG-0023 negative coords: RETRACTED 2026-08-30 — probe proved identical fall/
		//        rest at corner, negative-interior and positive-interior spawns; the test's
		//        wait predicate (Y > -49.6) was already true at spawn (-46)
		// REG-0024 area relocation ........ FIXED 2026-08-29 (setKinematicTarget on kinematic areas)
		// REG-0025 6dof free axis immovable: FIXED 2026-08-30 — module: fresh 6DOF axes
		//        now FREE and ENABLE_*_LIMIT flag-off means FREE (was LOCKED, silently
		//        welding bodies); JOINT-014 now sets the flag per Godot semantics
		// REG-0026 30-joint hub instability: FIXED 2026-08-30 — solver iterations 4→8
		//        (Godot parity), pin anchors preserve spoke radius, hub-spoke collision
		//        disabled (Godot Joint3D default)
		// REG-0027 RETRACTED (test bug: assertion checked the wrong axis; module code correct)
		// REG-0028 omit_force_integration: RETRACTED 2026-08-30 — module correct; the
		//        floating zero-velocity body auto-slept during the omit phase and sleeping
		//        bodies are frozen by contract (REG-0003). BODY-040 now sets can_sleep=false
		// REG-0029 heightmap gap: RETRACTED 2026-08-30 — probe spheres settled in the
		//        valley from every tested column; the test's settle predicate (|vx|<0.1)
		//        was trivially true at spawn, so it asserted the un-simulated spawn pose
		// Template for future entries:
		// s.Add("PHYSX-REG-XXXX", "REG-XXXX: one-line repro", ReproXXXX);
	}

	static IEnumerator CcdTunneling()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.025f, 4, 4), new Vector3(0, 2, 0));
		// Without CCD the 150 m/s sphere ends up beyond the wall.
		var p1 = w.MakeBody(w.Sphere(0.1f), new Vector3(-4, 2, 0));
		PhysicsServer3D.BodySetParam(p1, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetState(p1, PhysicsServer3D.BodyState.CanSleep, false);
		w.SetVel(p1, new Vector3(150, 0, 0));
		// With CCD the same setup stops at the wall.
		var p2 = w.MakeBody(w.Sphere(0.1f), new Vector3(-4, 2, 0.5f));
		PhysicsServer3D.BodySetParam(p2, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetState(p2, PhysicsServer3D.BodyState.CanSleep, false);
		PhysicsServer3D.BodySetEnableContinuousCollisionDetection(p2, true);
		w.SetVel(p2, new Vector3(150, 0, 0));
		yield return Wait.Frames(15);
		Assert.Expect(w.Pos(p1).Origin.X > 1f, $"no-CCD projectile tunneled (x={w.Pos(p1).Origin.X:F2})");
		Assert.Expect(w.Pos(p2).Origin.X < -0.05f, $"CCD projectile blocked (x={w.Pos(p2).Origin.X:F2})");
	}
	static IEnumerator KinematicTeleportQuery()
	{
		using var w = new PhysxWorld(false);
		var k = w.MakeKinematic(w.Box(0.6f), new Vector3(0, 1, 0));
		yield return Wait.Frames(3);
		w.Teleport(k, new Vector3(6, 1, 0));
		yield return Wait.Frames(2);
		var hit = w.Ray(new Vector3(6, 5, 0), new Vector3(6, -5, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == k, "queries see kinematic body at teleported pose");
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0, "old pose no longer occupied");
	}
	static IEnumerator SleepingFrozen()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 6, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, true);
		yield return Wait.Frames(120);
		Assert.ExpectNear(w.Pos(b).Origin.Y, 6f, 0.02f, "sleeping body suspended mid-air");
		Assert.Expect(w.Sleeping(b), "body reports sleeping");
	}
	static IEnumerator CastMotionOverlapQuirk()
	{
		using var w = new PhysxWorld();
		var frac = w.CastMotion(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(2, -0.2f, 0)), new Vector3(0, -1, 0));
		Assert.ExpectNear(frac.X, 1f, 1e-4f, "initial overlap treated as unobstructed (documented module behavior)");
		Assert.ExpectNear(frac.Y, 1f, 1e-4f, "unsafe fraction also 1.0");
		yield return Wait.Frame();
	}
	static IEnumerator InsideHitNormalQuirk()
	{
		using var w = new PhysxWorld();
		var hit = w.Ray(new Vector3(2, -0.4f, 0), new Vector3(2, -8, 0), fromInside: true);
		Assert.Require(hit.Count > 0, "inside hit present");
		Assert.ExpectVecNear(hit["normal"].AsVector3(), Vector3.Zero, 1e-6f, "synthetic inside-hit normal is exactly zero (documented)");
		Assert.ExpectVecNear(hit["position"].AsVector3(), new Vector3(2, -0.4f, 0), 1e-4f, "hit position equals ray origin");
		yield return Wait.Frame();
	}
	static IEnumerator PriorityNoopQuirk()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 5, 0));
		// C-10: the getter returns the stored value now (was pinned to 1.0 as a
		// no-op; the 42 value from the original quirk is the round-trip probe).
		PhysicsServer3D.BodySetCollisionPriority(b, 42f);
		Assert.ExpectNear(PhysicsServer3D.BodyGetCollisionPriority(b), 42f, 1e-6f, "priority round-trips (was no-op)");
		yield return Wait.Frame();
	}
	static IEnumerator SpaceIsolation()
	{
		using var a = new PhysxWorld();
		using var b = new PhysxWorld(false);
		var bodyInA = a.MakeStatic(a.Box(2, 0.5f, 2), new Vector3(0, 0.25f, 0));
		yield return Wait.Frames(5);
		Assert.Expect(a.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count > 0, "body visible in own space");
		Assert.Expect(b.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0, "body invisible in other space");
		var faller = b.MakeBody(b.Box(0.4f), new Vector3(0, 5, 0));
		yield return Wait.Frames(60);
		Assert.Expect(b.Pos(faller).Origin.Y < 1f, "no cross-space collision with A's floor (falls freely)");
	}
	static IEnumerator SharedShapeFree()
	{
		using var w = new PhysxWorld();
		var shared = w.Box(0.4f);
		var a = w.MakeBody(shared, new Vector3(0, 2, 0));
		var b = w.MakeBody(shared, new Vector3(3, 2, 0));
		yield return Wait.Frames(20);
		w.Free(shared);
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(a)) && PhysxWorld.Finite(w.Pos(b)), "both users finite after shared shape free");
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(6, 3, 0));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 1.5f, 240, "canary after shared shape free");
	}

	// ------------------------------------------------ REG repros (currently failing)
	static IEnumerator NeverSleeps()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 8f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		yield return Wait.Frames(600); // 10 s: Godot default time_to_sleep is 0.5 s
		Assert.Expect(w.Sleeping(b), "resting body with v~1e-4 auto-sleeps within 10 s (per-step addForce gravity appears to reset the PhysX wake counter)");
	}

	static IEnumerator BoxSemantics()
	{
		// SHAPE_BOX server data is HALF-extents (core BoxShape3D passes
		// size / 2; godot_physics and Jolt read it as half-extents too).
		// A floor of data (20, 1, 20) at y=-0.5 is 2 m tall and presents its
		// top at y=+0.5, so a unit-radius sphere rests at y=1.0. A module
		// that halved the data (full-size semantics) would rest it at 0.5.
		using var w = new PhysxWorld(false);
		var floorShape = PhysicsServer3D.BoxShapeCreate();
		w.AdoptShape(floorShape);
		PhysicsServer3D.ShapeSetData(floorShape, new Vector3(20, 1, 20));
		var floor = w.MakeStatic(floorShape, new Vector3(0, -0.5f, 0));
		var b = w.MakeBody(w.Sphere(0.5f), new Vector3(0, 3, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 1.5f, 400, "sphere reaches the floor");
		yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(b).Y) < 0.1f, 400, "sphere settles");
		Assert.ExpectNear(w.Pos(b).Origin.Y, 1.0f, 0.15f,
			$"box data treated as HALF-extents: sphere rests at 1.0 (module currently yields {w.Pos(b).Origin.Y:F2} = full-size semantics)");
	}

	static IEnumerator SpaceDestructionPhantom()
	{
		using var keeper = new PhysxWorld();
		var victim = PhysxWorld.CreateSpace();
		var shape = PhysicsServer3D.BoxShapeCreate();
		PhysicsServer3D.ShapeSetData(shape, new Vector3(0.4f, 0.4f, 0.4f));
		Rid first = default;
		for (int i = 0; i < 4; i++)
		{
			var b = PhysicsServer3D.BodyCreate();
			if (i == 0) first = b;
			PhysicsServer3D.BodySetSpace(b, victim);
			PhysicsServer3D.BodyAddShape(b, shape, Transform3D.Identity);
			PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform,
				new Transform3D(Basis.Identity, new Vector3(i * 2f, 5f, 0)));
		}
		// Area + joint inside the victim space (both present in the SPACE-009 trigger).
		var area = PhysicsServer3D.AreaCreate();
		PhysicsServer3D.AreaSetSpace(area, victim);
		PhysicsServer3D.AreaAddShape(area, shape, Transform3D.Identity);
		var j2 = PhysicsServer3D.JointCreate();
		var b2 = PhysicsServer3D.BodyCreate();
		PhysicsServer3D.BodySetSpace(b2, victim);
		PhysicsServer3D.BodyAddShape(b2, shape, Transform3D.Identity);
		PhysicsServer3D.JointMakePin(j2, first, Vector3.Zero, b2, Vector3.Zero);
		yield return Wait.Frames(20);
		PhysicsServer3D.FreeRid(victim);
		PhysicsServer3D.FreeRid(shape);
		var canary = keeper.MakeBody(keeper.Box(0.4f), new Vector3(9, 3, 9));
		yield return Wait.UntilOrFail(() => Math.Abs(keeper.Vel(canary).Y) < 0.05f, 400, "canary settles");
		Assert.ExpectNear(keeper.Pos(canary).Origin.Y, 0.4f, 0.15f,
			$"sibling space unaffected by space destruction (canary rests {keeper.Pos(canary).Origin.Y:F2}; phantom +0.4 offset observed)");
	}

	static IEnumerator MeshOnDynamicNoCrash()
	{
		// The original crash: attach a triangle-mesh shape while the body is
		// still default-rigid (BodyCreate returns BODY_MODE_RIGID), then touch
		// the body again. The module must reject/demote the shape without
		// leaving a half-attached actor behind (native 0xC0000005 before the
		// fix). The harness orders mode-first everywhere else, so this test
		// deliberately exercises the raw hostile order.
		using var w = new PhysxWorld();
		var mesh = w.Concave(
			new Vector3(-2, 0, -2), new Vector3(2, 0, -2), new Vector3(0, 0, 2),
			new Vector3(-2, 0, -2), new Vector3(0, 0, 2), new Vector3(-2, 0, 2));
		var hostile = PhysicsServer3D.BodyCreate(); // default BODY_MODE_RIGID
		w.TrackBody(hostile);
		PhysicsServer3D.BodySetSpace(hostile, w.Space);
		PhysicsServer3D.BodyAddShape(hostile, mesh, Transform3D.Identity); // mesh on dynamic!
		PhysicsServer3D.BodySetCollisionMask(hostile, 0xFFFFFFFF);          // the call that used to crash
		_ = PhysicsServer3D.BodyGetCollisionLayer(hostile);
		yield return Wait.Frames(30);
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(5, 3, 5));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 1.5f, 300, "canary after mesh-on-dynamic attach");
		PhysicsServer3D.BodySetMode(hostile, PhysicsServer3D.BodyMode.Static);
		yield return Wait.Frames(10);
		Assert.Expect(PhysxWorld.Finite(w.Pos(canary)), "engine healthy after mesh-on-dynamic attach + mode change");
	}

	static IEnumerator HeightmapLateDataAttach()
	{
		// WorldScape3D's game-mode terrain sequence: a
		// HeightMapShape3D RID is attached to a static body BEFORE any height
		// data exists (disabled, parked at V3_MAX) and shape_set_data() fills
		// it afterwards. The data-less attach must stay silent (no per-shape
		// error spam) and the slot must attach when the data arrives; before
		// the fix the body kept zero attached shapes and everything fell
		// through the terrain.
		using var w = new PhysxWorld(false);
		var hf = PhysicsServer3D.HeightmapShapeCreate();
		w.AdoptShape(hf);
		var body = PhysicsServer3D.BodyCreate(); // raw WS3D order: data-less + disabled
		w.TrackBody(body);
		PhysicsServer3D.BodySetMode(body, PhysicsServer3D.BodyMode.Static);
		PhysicsServer3D.BodySetSpace(body, w.Space);
		PhysicsServer3D.BodyAddShape(body, hf, Transform3D.Identity, true);
		PhysicsServer3D.BodySetShapeDisabled(body, 0, false);
		const int n = 65;
		var heights = new float[n * n];
		for (int i = 0; i < heights.Length; i++)
			heights[i] = 2f;
		PhysicsServer3D.ShapeSetData(hf, new Godot.Collections.Dictionary
		{
			["width"] = n,
			["depth"] = n,
			["heights"] = heights,
		});
		yield return Wait.Frames(3);
		var hit = w.Ray(new Vector3(0, 10, 0), new Vector3(0, -10, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == body, "late-data heightmap collides after shape_set_data");
		Assert.ExpectNear(hit.Count > 0 ? hit["position"].AsVector3().Y : -1f, 2f, 0.15f, "ray hits the heightfield surface");
	}

	static IEnumerator RuntimeFileStaleDetection()
	{
		// F-36 hardening: the Linux reporter's editor died at exec with
		// "undefined symbol: NvBlastExtFalloffSubgraphShader" because a stale
		// libNvBlastExtShaders.so shadowed the correct one. Existence checks
		// cannot see wrong-content files, so get_stale_runtime_files()
		// compares the required files' content against the known-good hashes
		// -- the installed file list an install records beside them
		// (".physx-runtime-files"). This test tamper-records the first
		// required file's hash beside the executable and expects the
		// detection, then proves the real hash reports nothing and unknown
		// files are skipped, restoring the directory afterwards.
		string exeDir = OS.GetExecutablePath().GetBaseDir();
		string listPath = Path.Join(exeDir, ".physx-runtime-files");
		string saved = File.Exists(listPath) ? File.ReadAllText(listPath) : null;
		try
		{
			var required = PhysXRuntimeManager.GetRequiredRuntimeFiles();
			Assert.Expect(required.Length > 0, "this build requires at least one runtime file");
			string probeName = required[0];
			string probePath = Path.Join(exeDir, probeName);
			Assert.Expect(File.Exists(probePath), probeName + " is present beside the executable");

			WriteFileList(listPath, probeName, new string('0', 64));
			var stale = PhysXRuntimeManager.GetStaleRuntimeFiles();
			Assert.Expect(Contains(stale, probeName), "content mismatching the installed list is reported stale");

			WriteFileList(listPath, probeName, PhysXRuntimeManager.Sha256File(probePath));
			stale = PhysXRuntimeManager.GetStaleRuntimeFiles();
			Assert.Expect(!Contains(stale, probeName), "the installed list holding the file's real hash reports nothing");

			WriteFileList(listPath, "definitely-not-a-runtime-file.dll", new string('a', 64));
			stale = PhysXRuntimeManager.GetStaleRuntimeFiles();
			Assert.Expect(!Contains(stale, probeName), "files without a known expected hash are not reported stale");
		}
		finally
		{
			if (saved != null)
			{
				File.WriteAllText(listPath, saved);
			}
			else
			{
				File.Delete(listPath);
			}
		}
		yield break;
	}

	static void WriteFileList(string path, string name, string hash)
	{
		// The same shape extract_bundle records: {runtime_version, files:{name:sha}}.
		string json = "{\n  \"runtime_version\": \"0.0.0-test\",\n  \"files\": {\n    \"" + name + "\": \"" + hash + "\"\n  }\n}";
		File.WriteAllText(path, json);
	}

	static IEnumerator GpuDynamicsReasonApi()
	{
		// F-37 (the GTX-1060 report): GPU-dynamics unavailability used to be a
		// bare "no usable CUDA device" warning with nothing script-facing. The
		// reason API classifies the cause (setting, determinism, driver,
		// runtime, compute capability, no device, CPU-only build) and
		// rd_compute_available() reports the RenderingDevice-compute half of
		// the fallback ladder. The classification itself is machine-dependent,
		// so the contract is checked, not a specific value: ACTIVE iff the
		// enabled flag is set, the reason names the cause iff unavailable, the
		// value within the enum range, and the probe cached.
		int status = ArticulationApi.Call("get_gpu_dynamics_status").AsInt32();
		string reason = ArticulationApi.Call("get_gpu_dynamics_unavailable_reason").AsString();
		bool enabled = ArticulationApi.Call("is_gpu_dynamics_enabled").AsBool();
		Assert.Expect(status >= 0 && status <= 7, $"gpu dynamics status {status} is a known GpuDynamicsStatus value");
		Assert.Expect(enabled == (status == 0), "GPU_DYNAMICS_ACTIVE iff is_gpu_dynamics_enabled()");
		Assert.Expect(status == 0 ? reason.Length == 0 : reason.Length > 0, "the reason names the cause iff GPU dynamics is unavailable");
		bool rd1 = ArticulationApi.Call("rd_compute_available").AsBool();
		bool rd2 = ArticulationApi.Call("rd_compute_available").AsBool();
		Assert.Expect(rd1 == rd2, $"rd_compute_available is cached (same answer twice: {rd1})");
		yield break;
	}

	static Vector3[] ConvexBoxPoints(float half)
	{
		return new Vector3[] {
			new(-half, -half, -half), new(half, -half, -half),
			new(-half, -half, half), new(half, -half, half),
			new(-half, half, -half), new(half, half, -half),
			new(-half, half, half), new(half, half, half),
		};
	}

	static Godot.Collections.Dictionary ConcavePlateData()
	{
		// Two triangles forming a 1x1 m plate at y = 0 (the dictionary form the
		// concave set_data requires: faces + backface_collision).
		var a = new Vector3(-0.5f, 0, -0.5f);
		var b = new Vector3(0.5f, 0, -0.5f);
		var c = new Vector3(0.5f, 0, 0.5f);
		var d = new Vector3(-0.5f, 0, 0.5f);
		return new Godot.Collections.Dictionary
		{
			["faces"] = new Vector3[] { a, b, c, a, c, d },
			["backface_collision"] = false,
		};
	}

	static IEnumerator EmptyMeshShapesLateDataAttach()
	{
		// F-34: an empty ConvexPolygonShape3D / ConcavePolygonShape3D RID (the
		// procedural-mesh pattern: attach first, set the geometry later) used
		// to print the generic "failed to generate geometry" error per shape.
		// Both now report has_geometry_data() == false, so the data-less
		// attach stays silent and the shape_changed() retry attaches the slot
		// when set_data supplies the geometry (the REG-0034 recovery, which
		// previously covered only the heightmap).
		using var w = new PhysxWorld(false);
		var convex = PhysicsServer3D.ConvexPolygonShapeCreate();
		w.AdoptShape(convex);
		var concave = PhysicsServer3D.ConcavePolygonShapeCreate();
		w.AdoptShape(concave);

		var bodyConvex = PhysicsServer3D.BodyCreate();
		w.TrackBody(bodyConvex);
		PhysicsServer3D.BodySetMode(bodyConvex, PhysicsServer3D.BodyMode.Static);
		PhysicsServer3D.BodySetSpace(bodyConvex, w.Space);
		PhysicsServer3D.BodySetState(bodyConvex, PhysicsServer3D.BodyState.Transform,
			new Transform3D(Basis.Identity, new Vector3(5, 0, 0)));
		PhysicsServer3D.BodyAddShape(bodyConvex, convex, Transform3D.Identity); // data-less: must stay silent
		var bodyConcave = PhysicsServer3D.BodyCreate();
		w.TrackBody(bodyConcave);
		PhysicsServer3D.BodySetMode(bodyConcave, PhysicsServer3D.BodyMode.Static);
		PhysicsServer3D.BodySetSpace(bodyConcave, w.Space);
		PhysicsServer3D.BodySetState(bodyConcave, PhysicsServer3D.BodyState.Transform,
			new Transform3D(Basis.Identity, new Vector3(-5, 0, 0)));
		PhysicsServer3D.BodyAddShape(bodyConcave, concave, Transform3D.Identity); // data-less: must stay silent

		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(5, 10, 0), new Vector3(5, -10, 0)).Count == 0, "data-less convex slot is not in the broadphase");
		Assert.Expect(w.Ray(new Vector3(-5, 10, 0), new Vector3(-5, -10, 0)).Count == 0, "data-less concave slot is not in the broadphase");

		PhysicsServer3D.ShapeSetData(convex, ConvexBoxPoints(0.3f));
		PhysicsServer3D.ShapeSetData(concave, ConcavePlateData());
		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(5, 10, 0), new Vector3(5, -10, 0)).Count > 0, "convex slot attaches once set_data supplies the points");
		Assert.Expect(w.Ray(new Vector3(-5, 10, 0), new Vector3(-5, -10, 0)).Count > 0, "concave slot attaches once set_data supplies the faces");
	}

	static IEnumerator ArticulationLinkLateDataAttach()
	{
		// F-33a: an articulation link shape attached data-less (the WS3D
		// RID-shape-then-data sequence) failed its cook with
		// "failed to create articulation link shape" and never retried -- the
		// link kept no collider forever. The articulation now registers as a
		// shape owner: shape_set_data() fires shape_changed() and the slot
		// attaches (mirroring the REG-0034 body/area recovery).
		//
		// Observed via simulation (the ART-003 idiom): direct-state rays and
		// overlaps do not report articulation-link shapes (pre-existing
		// behavior across every articulation setup, including the implicit
		// link boxes), so a dropped sphere is the honest witness. Pre-data the
		// sphere falls through to the floor; once the data lands and the slot
		// attaches, it rests on the shape.
		using var w = new PhysxWorld(); // floor top at y=0
		var shape = PhysicsServer3D.ConvexPolygonShapeCreate();
		w.AdoptShape(shape);
		var art = ArticulationApi.Create();
		ArticulationApi.SetFixBase(art, true);
		int link = ArticulationApi.AddLink(art, -1, Transform3D.Identity,
			new Transform3D(Basis.Identity, new Vector3(0, 2, 0)), 0, 1000f,
			new Vector3(0.02f, 0.02f, 0.02f)); // tiny implicit box, off the drop path
		Assert.Expect(link >= 0, "articulation base link created");
		ArticulationApi.SetSpace(art, w.Space);
		ArticulationApi.Wake(art);
		// Attach the convex data-less, offset to the side of the implicit box.
		ArticulationApi.SetLinkShape(art, link, shape,
			new Transform3D(Basis.Identity, new Vector3(0.5f, 0, 0)));
		var sphere = w.MakeBody(w.Sphere(0.4f), new Vector3(0.5f, 3, 0));
		yield return Wait.UntilOrFail(() => w.Pos(sphere).Origin.Y < 1.0f, 300,
			"sphere falls through the data-less slot to the floor");
		PhysicsServer3D.ShapeSetData(shape, ConvexBoxPoints(0.3f)); // fires shape_changed -> retry attaches
		yield return Wait.Frames(3);
		// Drop onto the freshly attached slot from above (a resting sphere
		// would never lift on its own).
		PhysicsServer3D.BodySetState(sphere, PhysicsServer3D.BodyState.Transform,
			new Transform3D(Basis.Identity, new Vector3(0.5f, 4, 0)));
		PhysicsServer3D.BodySetState(sphere, PhysicsServer3D.BodyState.LinearVelocity, Vector3.Zero);
		yield return Wait.Frames(90);
		Assert.ExpectNear(w.Pos(sphere).Origin.Y, 2.7f, 0.3f,
			$"sphere rests on the retried link shape (y={w.Pos(sphere).Origin.Y:F2}; box top 2.3 + radius 0.4)");
		PhysicsServer3D.FreeRid(art);
		yield return Wait.Frame();
	}

	static IEnumerator HeightmapNanHoles()
	{
		// F-35: WorldScape3D writes NaN for heightmap holes. heights[0] == NaN
		// made the min/max scan produce NaN bounds, every quantized sample
		// collapsed through an undefined float->int conversion, and
		// get_local_pose() handed PhysX a NaN transform. Policy: NaN is
		// excluded from the bounds and clamped to the lowest finite height at
		// cook time (a pit -- PhysX heightfields have no hole support; Jolt
		// skips NaN quads via its FLT_MAX marker), and an all-NaN map refuses
		// to cook with a guarded error instead of building a NaN field.
		using var w = new PhysxWorld(false);
		var hf = PhysicsServer3D.HeightmapShapeCreate();
		w.AdoptShape(hf);
		var body = PhysicsServer3D.BodyCreate();
		w.TrackBody(body);
		PhysicsServer3D.BodySetMode(body, PhysicsServer3D.BodyMode.Static);
		PhysicsServer3D.BodySetSpace(body, w.Space);
		PhysicsServer3D.BodyAddShape(body, hf, Transform3D.Identity, true);
		PhysicsServer3D.BodySetShapeDisabled(body, 0, false);
		const int n = 33;
		var heights = new float[n * n];
		for (int i = 0; i < heights.Length; i++)
		{
			heights[i] = 2f;
		}
		heights[0] = float.NaN; // heights[0] == NaN is the pre-fix NaN-bounds trigger
		for (int z = 6; z <= 10; z++)
		{
			for (int x = 6; x <= 10; x++)
			{
				heights[z * n + x] = float.NaN; // a hole patch (grid -> world -10..-6)
			}
		}
		PhysicsServer3D.ShapeSetData(hf, new Godot.Collections.Dictionary
		{
			["width"] = n,
			["depth"] = n,
			["heights"] = heights,
		});
		yield return Wait.Frames(3);
		var hitFlat = w.Ray(new Vector3(0, 10, 0), new Vector3(0, -10, 0));
		Assert.Expect(hitFlat.Count > 0 && Math.Abs(hitFlat["position"].AsVector3().Y - 2f) < 0.15f,
			"finite bounds: the field surfaces at its heights (NaN bounds would break the whole field)");
		var hitHole = w.Ray(new Vector3(-8, 10, -8), new Vector3(-8, -10, -8));
		Assert.Expect(hitHole.Count > 0 && Math.Abs(hitHole["position"].AsVector3().Y - 2f) < 0.15f,
			"NaN hole clamps to the finite field height (no undefined conversion, no NaN transform)");

		// All-NaN region: the field refuses to cook (guarded error, no NaN
		// transform); queries and other bodies stay healthy.
		var hfAllNan = PhysicsServer3D.HeightmapShapeCreate();
		w.AdoptShape(hfAllNan);
		var bodyAllNan = PhysicsServer3D.BodyCreate();
		w.TrackBody(bodyAllNan);
		PhysicsServer3D.BodySetMode(bodyAllNan, PhysicsServer3D.BodyMode.Static);
		PhysicsServer3D.BodySetSpace(bodyAllNan, w.Space);
		PhysicsServer3D.BodySetState(bodyAllNan, PhysicsServer3D.BodyState.Transform,
			new Transform3D(Basis.Identity, new Vector3(40, 0, 0)));
		PhysicsServer3D.BodyAddShape(bodyAllNan, hfAllNan, Transform3D.Identity, true);
		PhysicsServer3D.BodySetShapeDisabled(bodyAllNan, 0, false);
		var allNan = new float[n * n];
		for (int i = 0; i < allNan.Length; i++)
		{
			allNan[i] = float.NaN;
		}
		PhysicsServer3D.ShapeSetData(hfAllNan, new Godot.Collections.Dictionary
		{
			["width"] = n,
			["depth"] = n,
			["heights"] = allNan,
		});
		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(40, 10, 0), new Vector3(40, -10, 0)).Count == 0, "the all-NaN field never enters the broadphase");
		var canary = w.MakeBody(w.Sphere(0.3f), new Vector3(0, 5, 0));
		yield return Wait.UntilOrFail(() => w.Pos(canary).Origin.Y < 2.5f, 300, "canary settles on the healthy field");
		Assert.Expect(PhysxWorld.Finite(w.Pos(canary)), "engine healthy after the all-NaN field");
	}

	static bool Contains(string[] list, string value)
	{
		foreach (string item in list)
		{
			if (item == value)
			{
				return true;
			}
		}
		return false;
	}
}
