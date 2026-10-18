// Bodies: modes and transitions, state access cross-checks, force/impulse
// integration (analytic where possible), parameter behavior (mass, gravity
// scale, damping, restitution, friction, COM, inertia), axis locks, sleeping,
// and CCD. Round-trip-only params are marked as plumbing in their IDs.

using System;
using System.Collections;
using System.Linq;

namespace PhysxTestProject.Tests;

internal static class BodyTests
{
	public static void Register(SuiteBuilder s)
	{
		// --- modes ---
		s.Add("PHYSX-BODY-001", "static body ignores gravity", StaticIgnoresGravity);
		s.Add("PHYSX-BODY-002", "rigid body integrates gravity", RigidFalls);
		s.Add("PHYSX-BODY-003", "kinematic body ignores gravity but accepts teleports", KinematicTeleport);
		s.Add("PHYSX-BODY-004", "rigid-linear body moves but never rotates", RigidLinearNoRotation);
		s.Add("PHYSX-BODY-005", "static -> rigid mid-air starts falling", StaticToRigidFalls);
		s.Add("PHYSX-BODY-006", "rigid -> kinematic freezes at current pose", RigidToKinematicFreezes);
		s.Add("PHYSX-BODY-007", "kinematic -> rigid resumes falling", KinematicToRigidFalls);
		s.Add("PHYSX-BODY-008", "mode switches while resting on floor keep body sane", ModeSwitchResting);
		// --- states ---
		s.Add("PHYSX-BODY-009", "velocity set/get and integration agree", VelocityIntegration);
		s.Add("PHYSX-BODY-010", "direct state mirrors body_get_state (transform+velocities)", DirectStateCrossCheck);
		s.Add("PHYSX-BODY-011", "teleport reflected in queries immediately", TeleportQuerySync);
		s.Add("PHYSX-BODY-012", "sleeping-body teleport moves queries too", SleepingTeleport);
		s.Add("PHYSX-BODY-013", "large/negative coordinate teleports stay finite", LargeCoordTeleport);
		// --- params: behavioral ---
		s.Add("PHYSX-BODY-014", "mass ratio: same impulse gives inverse-mass-proportional dV", MassRatioImpulse);
		s.Add("PHYSX-BODY-015", "gravity scale 2 falls twice as fast", GravityScale2);
		s.Add("PHYSX-BODY-016", "negative gravity scale falls upward", GravityScaleNegative);
		s.Add("PHYSX-BODY-017", "linear damping decays velocity (REPLACE mode)", LinearDampDecay);
		s.Add("PHYSX-BODY-018", "angular damping decays spin", AngularDampDecay);
		s.Add("PHYSX-BODY-019", "damping mode switch takes effect mid-flight", DampModeSwitch);
		s.Add("PHYSX-BODY-020", "restitution orders rebound heights (0 < 0.5 < 1)", RestitutionOrdering);
		s.Add("PHYSX-BODY-021", "zero restitution settles after drop", RestitutionZeroSettles);
		s.Add("PHYSX-BODY-022", "friction stops high-friction slider sooner", FrictionSlide);
		s.Add("PHYSX-BODY-023", "friction difference at resting contact resists push", FrictionRestPush);
		s.Add("PHYSX-BODY-024", "off-center COM makes resting box tip over", ComOffsetTips);
		s.Add("PHYSX-BODY-025", "larger inertia resists angular impulse", InertiaResistsSpin);
		s.Add("PHYSX-BODY-026", "reset_mass_properties restores shape-derived behavior", ResetMassProps);
		// --- params: plumbing ---
		s.Add("PHYSX-BODY-P-027", "param round-trips (mass/bounce/friction/COM/inertia) [plumbing]", ParamRoundtrips);
		s.Add("PHYSX-BODY-P-028", "collision priority round-trips [plumbing]", CollisionPriorityNoop);
		s.Add("PHYSX-BODY-P-029", "user flags / CCD flag / instance-free bookkeeping [plumbing]", FlagsRoundtrip);
		// --- forces ---
		s.Add("PHYSX-BODY-030", "central impulse dV = J/m", CentralImpulseExact);
		s.Add("PHYSX-BODY-031", "off-center impulse creates angular response", OffCenterImpulseSpins);
		s.Add("PHYSX-BODY-032", "torque impulse changes angular velocity", TorqueImpulse);
		s.Add("PHYSX-BODY-033", "one-shot force accelerates for exactly one step", ForceOneStep);
		s.Add("PHYSX-BODY-034", "constant central force integrates F/m*t", ConstantForce);
		s.Add("PHYSX-BODY-035", "add_constant_force accumulates", ConstantForceAccumulates);
		s.Add("PHYSX-BODY-036", "constant torque spins body up", ConstantTorque);
		s.Add("PHYSX-BODY-037", "off-center constant force creates spin", OffCenterForceSpins);
		s.Add("PHYSX-BODY-038", "set_axis_velocity replaces one axis only", AxisVelocity);
		s.Add("PHYSX-BODY-039", "force integration callback fires and can edit state", ForceIntegrationCallback);
		s.Add("PHYSX-BODY-040", "omit_force_integration suspends acceleration", OmitForceIntegration);
		// --- axis locks ---
		s.Add("PHYSX-BODY-041", "all six single axis locks constrain their DOF", AllAxisLocks);
		s.Add("PHYSX-BODY-042", "combined linear locks leave one free axis", CombinedLinearLocks);
		s.Add("PHYSX-BODY-043", "combined angular locks leave one free axis", CombinedAngularLocks);
		s.Add("PHYSX-BODY-044", "full linear lock freezes body under gravity", FullLinearLock);
		// --- sleeping ---
		s.Add("PHYSX-BODY-045", "resting body eventually sleeps", GoesToSleep);
		s.Add("PHYSX-BODY-046", "can_sleep=false body never sleeps", CanSleepFalse);
		s.Add("PHYSX-BODY-047", "impulse wakes sleeping body", WakeOnImpulse);
		s.Add("PHYSX-BODY-048", "force-integrated wake: sleeping body wakes on velocity set", WakeOnVelocitySet);
		s.Add("PHYSX-BODY-049", "sleeping body ignores gravity (frozen mid-air)", SleepingIgnoresGravity);
		s.Add("PHYSX-BODY-050", "stack of 6 boxes settles and sleeps (no jitter explosion)", StackSettles);
		s.Add("PHYSX-BODY-051", "sleeping body does not tunnel after wake burst", SleepWakeCycleStable);
		s.Add("PHYSX-BODY-051A", "removing a support wakes a sleeping stack", WakeAfterSupportRemoval);
		// --- CCD ---
		s.Add("PHYSX-BODY-052", "CCD off: fast sphere tunnels through thin wall", CcdOffTunnels);
		s.Add("PHYSX-BODY-053", "CCD on: fast sphere is blocked by thin wall", CcdOnBlocks);
		s.Add("PHYSX-BODY-054", "CCD on: multiple speeds and diagonal impacts stay this side", CcdSpeedsAndDiagonal);
		s.Add("PHYSX-BODY-055", "CCD flag toggling changes behavior of same body", CcdToggle);
		// --- mode round-trip / shape-flag regressions ---
		s.Add("PHYSX-BODY-056", "static->rigid round-trip keeps convex shapes colliding (rebuild_shapes)", ModeRoundTripKeepsConvexCollision);
		s.Add("PHYSX-BODY-057", "disable/re-enable: convex restores collision, concave stays query-only on dynamic", DisabledShapeRoundTrip);
		s.Add("PHYSX-BODY-058", "querying force-integration callback does not re-enter the post pipeline", QueryInIntegratorNoReentry);
		s.Add("PHYSX-BODY-059", "static body node scale bakes into queries and collision", ScaledStaticBoxQueries);
		s.Add("PHYSX-BODY-060", "scaled sphere rests at the scaled radius; inertia finite", ScaledSphereRest);
		s.Add("PHYSX-BODY-061", "mirrored body scale flips asymmetric convex collision", MirroredConvexCollision);
		s.Add("PHYSX-BODY-062", "scaled area covers the scaled volume (gravity override)", ScaledAreaGravityOverride);
		s.Add("PHYSX-BODY-063", "sleeping body emits no state-sync callbacks until woken", SleepGatesStateSync);
		// --- Phase 3: body-state fidelity (OBJ-1/2/3/6/9) ---
		s.Add("PHYSX-BODY-064", "scaled BODY_STATE_TRANSFORM round-trips rotation and scale", ScaledTransformRoundTrip);
		s.Add("PHYSX-BODY-065", "world inverse inertia tensor matches analytic R·diag·Rᵀ through the COM frame", InverseInertiaTensorWorldFrame);
		s.Add("PHYSX-BODY-066", "force integration callback stops while asleep, resumes on wake", FiCallbackSleepCadence);
		s.Add("PHYSX-BODY-067", "param writes on a sleeping body do not wake it", ParamSetKeepsSleeping);
		s.Add("PHYSX-BODY-068", "damp-mode params round-trip", DampModeParamRoundTrip);
		s.Add("PHYSX-BODY-069", "tilted scaled body with custom COM keeps origin-relative COM and round-trips", ScaledCustomComTiltRoundTrip);
		s.Add("PHYSX-BODY-070", "axis locks are WORLD-frame on rotated bodies (OBJ-13 probe)", AxisLockFrameProbe);
	}

	// ------------------------------------------------------------------ modes
	static IEnumerator StaticIgnoresGravity()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 5, 0));
		yield return Wait.Frames(90);
		Assert.ExpectVecNear(w.Pos(b).Origin, new Vector3(0, 5, 0), 1e-4f, "static body never moves");
		Assert.ExpectVecNear(w.Vel(b), Vector3.Zero, 1e-5f, "static body has no velocity");
	}
	static IEnumerator RigidFalls()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 4, 0));
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 3f, 120, "rigid body starts falling");
		yield return Wait.UntilOrFail(() => w.Vel(b).Y < -3f, 60, "rigid body gains velocity");
		Assert.Expect(w.Pos(b).Origin.Y > 0.2f, "rigid body has not fallen through floor yet");
	}
	static IEnumerator KinematicTeleport()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeKinematic(w.Box(0.5f), new Vector3(0, 5, 0));
		yield return Wait.Frames(30);
		Assert.ExpectNear(w.Pos(b).Origin.Y, 5f, 1e-3f, "kinematic body ignores gravity");
		w.Teleport(b, new Vector3(3, 2.5f, -1));
		yield return Wait.Frames(2);
		Assert.ExpectVecNear(w.Pos(b).Origin, new Vector3(3, 2.5f, -1), 0.06f, "kinematic teleport applied");
	}
	static IEnumerator RigidLinearNoRotation()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 5, 0), mode: PhysicsServer3D.BodyMode.RigidLinear);
		PhysicsServer3D.BodyApplyImpulse(b, new Vector3(0, 0, 0.4f), new Vector3(0.6f, 0, 0));
		yield return Wait.Frames(60);
		Assert.Expect(w.AngVel(b).Length() < 1e-3f, $"rigid-linear body has no angular velocity (got {w.AngVel(b).Length():E2})");
		Assert.Expect(w.Pos(b).Origin.Z > 0.1f, "rigid-linear body still translates");
	}
	static IEnumerator StaticToRigidFalls()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 6, 0), mode: PhysicsServer3D.BodyMode.Static);
		yield return Wait.Frames(30);
		Assert.ExpectNear(w.Pos(b).Origin.Y, 6f, 1e-3f, "static phase: no fall");
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Rigid);
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y < 5.5f, "falls after static->rigid");
	}
	static IEnumerator RigidToKinematicFreezes()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 10, 0));
		yield return Wait.Frames(30);
		float y = w.Pos(b).Origin.Y;
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Kinematic);
		yield return Wait.Frames(60);
		Assert.ExpectNear(w.Pos(b).Origin.Y, y, 0.02f, "kinematic switch freezes fall");
	}
	static IEnumerator KinematicToRigidFalls()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeKinematic(w.Box(0.5f), new Vector3(0, 6, 0));
		yield return Wait.Frames(20);
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Rigid);
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y < 5.5f, "falls after kinematic->rigid");
	}
	static IEnumerator ModeSwitchResting()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.6f, 200, "body lands first");
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Static);
		yield return Wait.Frames(20);
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Kinematic);
		yield return Wait.Frames(20);
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Rigid);
		yield return Wait.Frames(40);
		Assert.ExpectNear(w.Pos(b).Origin.Y, 0.4f, 0.25f, "body stays rested after mode cycles");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after mode cycles");
	}

	// Static <-> dynamic switches recreate the PxRigidActor, and rebuild_shapes()
	// re-creates every PxShape against it. The convex box must come back as a
	// SIMULATION shape each time — a body that was ever static still collides
	// after the switch (and does not fall through the floor).
	static IEnumerator ModeRoundTripKeepsConvexCollision()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 6, 0), mode: PhysicsServer3D.BodyMode.Static);
		yield return Wait.Frames(10);
		Assert.ExpectNear(w.Pos(b).Origin.Y, 6f, 1e-3f, "static phase: frozen mid-air");

		// static -> rigid #1: first actor recreation.
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Rigid);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.7f, 240, "lands after static->rigid");
		yield return Wait.Frames(90); // settle (or fall through with the regression)
		float y1 = w.Pos(b).Origin.Y;
		Assert.Expect(y1 > 0.2f, $"rests above floor after static->rigid (y={y1})");
		Assert.ExpectNear(y1, 0.5f, 0.25f, "rest height matches box half-extent");

		// rigid -> static -> rigid #2: second actor recreation round-trip.
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Static);
		yield return Wait.Frames(10);
		PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Rigid);
		yield return Wait.Frames(120);
		float y2 = w.Pos(b).Origin.Y;
		Assert.Expect(y2 > 0.2f, $"still rests above floor after rigid->static->rigid (y={y2})");
		Assert.Expect(y2 < 1.0f, $"did not launch after round-trip (y={y2})");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after round-trip");
	}

	// set_shape_disabled strips eSIMULATION_SHAPE+eSCENE_QUERY_SHAPE; re-enabling
	// must restore them — except on a concave shape attached to a non-kinematic
	// dynamic body, where PhysX forbids the simulation flag (REG-0014): that one
	// comes back query-only (still ray-hittable, still non-solid).
	static IEnumerator DisabledShapeRoundTrip()
	{
		using var w = new PhysxWorld();

		// Convex: disable mid-air (no collision, body keeps falling), re-enable,
		// and the body must land on the floor again.
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 8, 0));
		yield return Wait.Frames(5);
		PhysicsServer3D.BodySetShapeDisabled(b, 0, true);
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y < 7.5f, "disabled shape: body falls uncollided");
		PhysicsServer3D.BodySetShapeDisabled(b, 0, false);
		yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.7f, 240, "lands after re-enable");
		yield return Wait.Frames(90);
		float y = w.Pos(b).Origin.Y;
		Assert.ExpectNear(y, 0.5f, 0.25f, "convex re-enable restores collision (rests on floor)");

		// Concave on a dynamic body: created query-only (REG-0014). A
		// disable/re-enable cycle must keep it query-only (ray still hits) and
		// must never make the falling body solid.
		var quad = w.Concave(
			new Vector3(-1, 0, -1), new Vector3(1, 0, -1), new Vector3(-1, 0, 1),
			new Vector3(-1, 0, 1), new Vector3(1, 0, -1), new Vector3(1, 0, 1));
		var c = w.MakeBody(quad, new Vector3(5, 6, 5));
		yield return Wait.Frames(5);
		PhysicsServer3D.BodySetShapeDisabled(c, 0, true);
		yield return Wait.Frames(5);
		PhysicsServer3D.BodySetShapeDisabled(c, 0, false);
		yield return Wait.Frames(5);
		float qy = w.Pos(c).Origin.Y;
		var hit = w.Ray(new Vector3(5, qy + 1, 5), new Vector3(5, qy - 1, 5));
		Assert.Expect(hit.Count > 0, "re-enabled concave shape is still queryable (ray hits)");
		yield return Wait.Frames(90);
		Assert.Expect(w.Pos(c).Origin.Y < 0f, $"concave body on dynamic actor never becomes solid (y={w.Pos(c).Origin.Y})");
	}

	// ------------------------------------------------------------------ states
	static IEnumerator VelocityIntegration()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		w.SetVel(b, new Vector3(2, -1, 3));
		Assert.ExpectVecNear(w.Vel(b), new Vector3(2, -1, 3), 1e-4f, "velocity round-trip");
		yield return Wait.Frames(30);
		Assert.ExpectVecNear(w.Pos(b).Origin, new Vector3(2f * 0.5f, 20 - 0.5f, 3f * 0.5f), 0.05f,
			"position = v*t after 0.5 s (tol covers 1-tick registration warmup)");
	}
	static IEnumerator DirectStateCrossCheck()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(1, 20, 2));
		yield return Wait.Frames(37);
		var st = w.Direct(b);
		Assert.Require(st != null, "body_get_direct_state returns object");
		Assert.ExpectVecNear(st.Transform.Origin, w.Pos(b).Origin, 1e-5f, "direct transform == body_get_state transform");
		Assert.ExpectVecNear(st.LinearVelocity, w.Vel(b), 1e-5f, "direct velocity == body_get_state velocity");
		Assert.ExpectVecNear(st.AngularVelocity, w.AngVel(b), 1e-5f, "direct angular velocity matches");
		Assert.ExpectNear(st.TotalGravity.Length(), PhysxWorld.G, 0.2f, "direct total gravity magnitude");
		Assert.ExpectNear(st.InverseMass, 1f, 1e-4f, "inverse mass for mass=1");
		Assert.ExpectVecNear(st.CenterOfMassLocal, Vector3.Zero, 0.05f, "default local COM at origin");
	}
	static IEnumerator TeleportQuerySync()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 1, 0));
		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(4, 5, 0), new Vector3(4, -5, 0)).Count == 0, "target spot empty before teleport");
		w.Teleport(b, new Vector3(4, 1, 0));
		yield return Wait.Frames(2);
		var hit = w.Ray(new Vector3(4, 5, 0), new Vector3(4, -5, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == b, "queries see teleported body at new location");
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0, "old location empty after teleport");
	}
	static IEnumerator SleepingTeleport()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 0.4f, 0));
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 500, "body asleep on floor");
		w.Teleport(b, new Vector3(5, 0.4f, 0));
		yield return Wait.Frames(2);
		var hit = w.Ray(new Vector3(5, 3, 0), new Vector3(5, -3, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == b, "queries see sleeping body at teleported position");
	}
	static IEnumerator LargeCoordTeleport()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(100000, 100000, -50000));
		yield return Wait.Frames(10);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "transform finite at 1e5 coordinates");
		w.Teleport(b, new Vector3(-100000, 50000, 100000));
		yield return Wait.Frames(40);
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after cross-map teleport");
		Assert.Expect(w.Vel(b).Y < -5f, "gravity still applies at large coordinates");
	}

	// ------------------------------------------------------------------ params: behavioral
	static IEnumerator MassRatioImpulse()
	{
		using var w = new PhysxWorld(false);
		var light = w.MakeBody(w.Box(0.5f), new Vector3(-3, 10, 0), mass: 1f);
		var heavy = w.MakeBody(w.Box(0.5f), new Vector3(3, 10, 0), mass: 2f);
		foreach (var b in new[] { light, heavy }) PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyCentralImpulse(light, new Vector3(5, 0, 0));
		PhysicsServer3D.BodyApplyCentralImpulse(heavy, new Vector3(5, 0, 0));
		yield return Wait.Frames(2);
		float dvL = w.Vel(light).X, dvH = w.Vel(heavy).X;
		Assert.ExpectNear(dvL, 5f, 0.05f, "dV of mass-1 body = J/m = 5");
		Assert.ExpectNear(dvH, 2.5f, 0.05f, "dV of mass-2 body = J/m = 2.5");
		Assert.ExpectNear(dvL / dvH, 2.0f, 0.05f, "dV ratio = inverse mass ratio");
	}
	static IEnumerator GravityScale2()
	{
		using var w = new PhysxWorld(false);
		var normal = w.MakeBody(w.Box(0.4f), new Vector3(-2, 30, 0));
		var fast = w.MakeBody(w.Box(0.4f), new Vector3(2, 30, 0));
		PhysicsServer3D.BodySetParam(fast, PhysicsServer3D.BodyParameter.GravityScale, 2f);
		yield return Wait.Frames(30); // 0.5 s
		float v1 = Math.Abs(w.Vel(normal).Y), v2 = Math.Abs(w.Vel(fast).Y);
		Assert.ExpectNear(v1, PhysxWorld.G * 0.5f, 0.2f, "scale-1 velocity = g*t");
		Assert.ExpectNear(v2, PhysxWorld.G * 1.0f, 0.4f, "scale-2 velocity = 2*g*t");
		Assert.ExpectNear(v2 / v1, 2.0f, 0.15f, "gravity scale ratio holds");
	}
	static IEnumerator GravityScaleNegative()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 5, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, -1f);
		yield return Wait.Frames(60);
		Assert.Expect(w.Pos(b).Origin.Y > 5.5f, $"negative gravity scale lifts body (y={w.Pos(b).Origin.Y:F2})");
		Assert.Expect(w.Vel(b).Y > 5f, "upward velocity from reversed gravity");
	}
	static IEnumerator LinearDampDecay()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 30, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 2f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		w.SetVel(b, new Vector3(10, 0, 0));
		yield return Wait.Frames(60); // v(1s) = 10 * e^-2 = 1.353; PhysX per-step implicit damping gives ~1.40
		Assert.ExpectNear(w.Vel(b).X, 1.353f, 0.35f, "linear damping decay matches e^(-lambda*t)");
	}
	static IEnumerator AngularDampDecay()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 30, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDamp, 3f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		w.SetAngVel(b, new Vector3(0, 8, 0));
		yield return Wait.Frames(60); // 8 * e^-3 = 0.398
		Assert.ExpectNear(Math.Abs(w.AngVel(b).Y), 0.398f, 0.15f, "angular damping decay matches e^(-lambda*t)");
	}
	static IEnumerator DampModeSwitch()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 30, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 4f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		w.SetVel(b, new Vector3(10, 0, 0));
		yield return Wait.Frames(60);
		float afterReplace = w.Vel(b).X; // ~0.18
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Combine);
		w.SetVel(b, new Vector3(10, 0, 0));
		yield return Wait.Frames(60);
		float afterCombine = w.Vel(b).X; // combine adds to scene default (0) — same value here
		Assert.Expect(afterReplace < 1.0f, "REPLACE damping decayed velocity");
		Assert.ExpectNear(afterCombine, afterReplace, 0.3f, "COMBINE with zero area damp behaves the same here");
		// Now an area with damp will differentiate the modes (see AreaTests).
	}
	static IEnumerator RestitutionOrdering()
	{
		// Drop three balls with restitution 0 / 0.5 / 1 from the same height and
		// compare rebound heights. Combination with the floor's default material
		// may soften the effective value, so the assertion is strict *ordering*
		// plus a meaningful minimum for the bounciest ball.
		using var w = new PhysxWorld(false);
		w.AddFloor();
		float[] bounce = { 0.0f, 0.5f, 1.0f };
		var bodies = new Rid[3];
		for (int i = 0; i < 3; i++)
		{
			bodies[i] = w.MakeBody(w.Sphere(0.3f), new Vector3(i * 2f - 2f, 3f, 0));
			PhysicsServer3D.BodySetParam(bodies[i], PhysicsServer3D.BodyParameter.Bounce, bounce[i]);
		}
		yield return Wait.UntilOrFail(() => w.Vel(bodies[2]).Y < -3.5f, 120, "balls reach the floor");
		// Track the apex of the FIRST REBOUND only. Peak tracking must begin
		// after every ball has touched the floor — starting earlier just
		// records the pre-impact fall position (~2.3 m), which masks any
		// restitution difference.
		yield return Wait.UntilOrFail(() =>
			w.Pos(bodies[0]).Origin.Y < 0.6f && w.Pos(bodies[1]).Origin.Y < 0.6f && w.Pos(bodies[2]).Origin.Y < 0.6f,
			120, "all balls touched the floor");
		var peak = new float[3];
		for (int f = 0; f < 90; f++)
		{
			yield return Wait.Frame();
			for (int i = 0; i < 3; i++) peak[i] = Math.Max(peak[i], w.Pos(bodies[i]).Origin.Y);
		}
		// Expected apexes from 7.3 m/s impact: e=0 -> ~0.3 (no rebound),
		// e=0.5 -> ~0.95, e=1 -> ~2.8. Strict ordering plus a minimum for the
		// bounciest ball; tolerances leave solver slack.
		Assert.Expect(peak[2] > peak[1] + 0.15f, $"restitution 1 bounces higher than 0.5 ({peak[2]:F2} vs {peak[1]:F2})");
		Assert.Expect(peak[1] > peak[0] + 0.1f, $"restitution 0.5 bounces higher than 0 ({peak[1]:F2} vs {peak[0]:F2})");
		Assert.Expect(peak[2] > 0.6f, $"bounciest ball rebounds at least 0.3 m above rest ({peak[2]:F2})");
		Assert.Expect(peak[0] < 0.75f, $"zero-restitution ball stays put ({peak[0]:F2})");
	}
	static IEnumerator RestitutionZeroSettles()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 3, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "zero-bounce body settles to sleep");
		Assert.ExpectNear(w.Pos(b).Origin.Y, 0.4f, 0.1f, "resting height correct");
	}
	static IEnumerator FrictionSlide()
	{
		using var w = new PhysxWorld();
		var hi = w.MakeBody(w.Box(0.4f), new Vector3(-5, 0.45f, 0));
		var lo = w.MakeBody(w.Box(0.4f), new Vector3(5, 0.45f, 0));
		PhysicsServer3D.BodySetParam(hi, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(lo, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(hi, PhysicsServer3D.BodyParameter.Friction, 0.9f);
		PhysicsServer3D.BodySetParam(lo, PhysicsServer3D.BodyParameter.Friction, 0.02f);
		PhysicsServer3D.BodySetState(hi, PhysicsServer3D.BodyState.CanSleep, false);
		PhysicsServer3D.BodySetState(lo, PhysicsServer3D.BodyState.CanSleep, false);
		yield return Wait.Frames(15); // establish contact
		w.SetVel(hi, new Vector3(6, 0, 0));
		w.SetVel(lo, new Vector3(6, 0, 0));
		float xHi0 = w.Pos(hi).Origin.X, xLo0 = w.Pos(lo).Origin.X;
		yield return Wait.Frames(90); // 1.5 s
		float dHi = w.Pos(hi).Origin.X - xHi0, dLo = w.Pos(lo).Origin.X - xLo0;
		Assert.Expect(dLo - dHi > 1.5f, $"low-friction slider travels farther ({dLo:F2} vs {dHi:F2})");
		Assert.Expect(Math.Abs(w.Vel(hi).X) < Math.Abs(w.Vel(lo).X), "high-friction slider is slower after coasting");
	}
	static IEnumerator FrictionRestPush()
	{
		using var w = new PhysxWorld();
		var crate = w.MakeBody(w.Box(0.5f), new Vector3(0, 0.5f, 0), mass: 5f);
		PhysicsServer3D.BodySetParam(crate, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(crate, PhysicsServer3D.BodyParameter.Friction, 1.0f);
		PhysicsServer3D.BodySetState(crate, PhysicsServer3D.BodyState.CanSleep, false);
		yield return Wait.UntilOrFail(() => w.Pos(crate).Origin.Y < 0.7f, 200, "crate on floor");
		// A small push should barely move the high-friction crate.
		PhysicsServer3D.BodyApplyCentralImpulse(crate, new Vector3(0.7f, 0, 0));
		yield return Wait.Frames(45);
		float moved = w.Pos(crate).Origin.X;
		Assert.Expect(moved < 0.35f, $"high static friction limits push displacement ({moved:F2})");
	}
	static IEnumerator ComOffsetTips()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f, 0.4f, 0.4f), new Vector3(0, 0.45f, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.CenterOfMass, new Vector3(0.45f, -0.1f, 0));
		yield return Wait.Frames(240);
		Assert.Expect(w.AngVel(b).Length() > 0.05f || Math.Abs(w.Pos(b).Origin.X) > 0.3f,
			"off-center COM makes resting box tip/rotate");
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)), "tipping stays finite");
	}
	static IEnumerator InertiaResistsSpin()
	{
		using var w = new PhysxWorld(false);
		var small = w.MakeBody(w.Box(0.5f), new Vector3(-3, 10, 0));
		var big = w.MakeBody(w.Box(0.5f), new Vector3(3, 10, 0));
		foreach (var b in new[] { small, big }) PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetParam(big, PhysicsServer3D.BodyParameter.Inertia, new Vector3(50f, 50f, 50f));
		PhysicsServer3D.BodyApplyTorqueImpulse(small, new Vector3(0, 5, 0));
		PhysicsServer3D.BodyApplyTorqueImpulse(big, new Vector3(0, 5, 0));
		yield return Wait.Frames(2);
		float wS = Math.Abs(w.AngVel(small).Y), wB = Math.Abs(w.AngVel(big).Y);
		Assert.Expect(wS > wB * 10f, $"default inertia spins much faster than scaled inertia ({wS:F2} vs {wB:F2})");
		Assert.Expect(wB > 1e-4f, "heavy-inertia body still responds to torque impulse");
	}
	static IEnumerator ResetMassProps()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 10, 0), mass: 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Mass, 4f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Inertia, new Vector3(9, 9, 9));
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(4, 0, 0));
		yield return Wait.Frames(2);
		Assert.ExpectNear(w.Vel(b).X, 1.0f, 0.02f, "dV honors modified mass (4/4)");
		PhysicsServer3D.BodyResetMassProperties(b);
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(4, 0, 0));
		yield return Wait.Frames(2);
		// After reset the mass reverts to the set mass value (Godot keeps mass, recomputes inertia).
		Assert.Expect(w.Vel(b).X > 1.5f && w.Vel(b).X < 3.2f, $"post-reset impulse response sane ({w.Vel(b).X:F2})");
	}
	static IEnumerator ParamRoundtrips()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 10, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Mass, 3.5f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0.4f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Friction, 0.65f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.CenterOfMass, new Vector3(0.1f, -0.2f, 0.3f));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Inertia, new Vector3(2, 3, 4));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDamp, 1.25f);
		Assert.ExpectNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.Mass).AsSingle(), 3.5f, 1e-4f, "mass round-trip");
		Assert.ExpectNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.Bounce).AsSingle(), 0.4f, 1e-4f, "bounce round-trip");
		Assert.ExpectNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.Friction).AsSingle(), 0.65f, 1e-4f, "friction round-trip");
		Assert.ExpectVecNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.CenterOfMass).AsVector3(), new Vector3(0.1f, -0.2f, 0.3f), 1e-4f, "COM round-trip");
		Assert.ExpectVecNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.Inertia).AsVector3(), new Vector3(2, 3, 4), 1e-3f, "inertia round-trip");
		Assert.ExpectNear(PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.AngularDamp).AsSingle(), 1.25f, 1e-4f, "angular damp round-trip");
		yield return Wait.Frame();
	}
	static IEnumerator CollisionPriorityNoop()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 10, 0));
		// C-10: priority round-trips (Godot weights test-motion recovery by it
		// upstream; the module stores it). 8 is the value the old no-op test
		// pinned; 3 proves the getter returns what was SET, not a constant.
		PhysicsServer3D.BodySetCollisionPriority(b, 3f);
		Assert.ExpectNear(PhysicsServer3D.BodyGetCollisionPriority(b), 3f, 1e-5f,
			"collision priority round-trips");
		yield return Wait.Frame();
	}
	static IEnumerator FlagsRoundtrip()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 10, 0));
		PhysicsServer3D.BodySetEnableContinuousCollisionDetection(b, true);
		Assert.Expect(PhysicsServer3D.BodyIsContinuousCollisionDetectionEnabled(b), "CCD flag round-trip");
		PhysicsServer3D.BodySetMaxContactsReported(b, 12);
		Assert.Expect(PhysicsServer3D.BodyGetMaxContactsReported(b) == 12, "max contacts round-trip");
		yield return Wait.Frame();
	}

	// ------------------------------------------------------------------ forces
	static IEnumerator CentralImpulseExact()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 20, 0), mass: 2f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(3, 0, 0));
		yield return Wait.Frames(2);
		Assert.ExpectNear(w.Vel(b).X, 1.5f, 0.01f, "central impulse dV = J/m");
	}
	static IEnumerator OffCenterImpulseSpins()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.6f), new Vector3(0, 20, 0), mass: 2f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyImpulse(b, new Vector3(0, 0, 2), new Vector3(0.6f, 0, 0));
		yield return Wait.Frames(2);
		Assert.Expect(Mathf.Abs(w.AngVel(b).Y) > 0.15f, $"off-center impulse creates angular velocity ({w.AngVel(b).Y:F2})");
		Assert.Expect(w.AngVel(b).Y < 0, "angular velocity sign matches torque = r x F (r=+x, F=+z -> -y)");
	}
	static IEnumerator TorqueImpulse()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.6f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyTorqueImpulse(b, new Vector3(0, 4, 0));
		yield return Wait.Frames(2);
		Assert.Expect(w.AngVel(b).Y > 0.5f, "torque impulse spins about +Y");
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(b).Origin.Y > 19.9f, "pure torque does not translate CoM");
	}
	static IEnumerator ForceOneStep()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 20, 0), mass: 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyCentralForce(b, new Vector3(60, 0, 0));
		yield return Wait.Frames(2);
		// One-shot force acts for exactly one step: dV = F*dt = 60/60 = 1.
		Assert.ExpectNear(w.Vel(b).X, 1.0f, 0.05f, "apply_central_force is cleared after one step");
	}
	static IEnumerator ConstantForce()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 20, 0), mass: 2f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetConstantForce(b, new Vector3(4, 0, 0));
		Assert.ExpectVecNear(PhysicsServer3D.BodyGetConstantForce(b), new Vector3(4, 0, 0), 1e-4f, "constant force round-trip");
		yield return Wait.Frames(60);
		Assert.ExpectNear(w.Vel(b).X, 2.0f, 0.1f, "constant force integrates F/m*t = 2 m/s");
	}
	static IEnumerator ConstantForceAccumulates()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 20, 0), mass: 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyAddConstantCentralForce(b, new Vector3(1, 0, 0));
		PhysicsServer3D.BodyAddConstantCentralForce(b, new Vector3(1, 0, 0));
		PhysicsServer3D.BodyAddConstantCentralForce(b, new Vector3(1, 0, 0));
		yield return Wait.Frames(2);
		Assert.ExpectVecNear(PhysicsServer3D.BodyGetConstantForce(b), new Vector3(3, 0, 0), 1e-4f, "add_constant_central_force accumulates");
	}
	static IEnumerator ConstantTorque()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.6f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetConstantTorque(b, new Vector3(0, 2, 0));
		Assert.ExpectVecNear(PhysicsServer3D.BodyGetConstantTorque(b), new Vector3(0, 2, 0), 1e-4f, "constant torque round-trip");
		yield return Wait.Frames(60);
		Assert.Expect(w.AngVel(b).Y > 2f, $"constant torque spins body up ({w.AngVel(b).Y:F2})");
	}
	static IEnumerator OffCenterForceSpins()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.6f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyAddConstantForce(b, new Vector3(0, 0, 3), new Vector3(0.5f, 0, 0));
		yield return Wait.Frames(30);
		// Torque = r x F = (0.5,0,0) x (0,0,3) = (0,-1.5,0): spin about -Y.
		Assert.Expect(Mathf.Abs(w.AngVel(b).Y) > 0.2f, $"off-center constant force creates spin (wy={w.AngVel(b).Y:F2})");
	}
	static IEnumerator AxisVelocity()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 20, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		w.SetVel(b, new Vector3(1, 1, 1));
		PhysicsServer3D.BodySetAxisVelocity(b, new Vector3(0, 5, 0));
		yield return Wait.Frames(2);
		Assert.ExpectVecNear(w.Vel(b), new Vector3(1, 5, 1), 0.05f, "set_axis_velocity replaces only the given axis");
	}
	static IEnumerator ForceIntegrationCallback()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 30, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		int calls = 0;
		PhysicsServer3D.BodySetForceIntegrationCallback(b,
			Callable.From<PhysicsDirectBodyState3D>(st =>
			{
				calls++;
				if (calls == 5) st.LinearVelocity = new Vector3(0, 0, 3); // delegate crossing managed/native boundary
			}));
		yield return Wait.Frames(10);
		Assert.Expect(calls >= 8 && calls <= 12, $"force integration callback fired once per step (got {calls}/10)");
		Assert.ExpectNear(w.Vel(b).Z, 3f, 0.05f, "callback-written velocity persisted");
		PhysicsServer3D.BodySetForceIntegrationCallback(b, new Callable()); // remove
		int after = calls;
		yield return Wait.Frames(10);
		Assert.Expect(calls == after, "callback stops after being cleared");
	}
	static IEnumerator OmitForceIntegration()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 30, 0));
		// can_sleep=false: a floating zero-velocity body auto-sleeps within the
		// omit phase, and sleeping bodies are frozen by contract (gravity
		// skipped while asleep) — that interplay would mask the omit semantics
		// under test here.
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.CanSleep, false);
		PhysicsServer3D.BodySetOmitForceIntegration(b, true);
		Assert.Expect(PhysicsServer3D.BodyIsOmittingForceIntegration(b), "omit flag round-trip");
		yield return Wait.Frames(60);
		float vy = w.Vel(b).Y;
		Assert.Expect(Math.Abs(vy) < 0.5f, $"omitted force integration accumulates no gravity (vy={vy:F2})");
		PhysicsServer3D.BodySetOmitForceIntegration(b, false);
		yield return Wait.Frames(30);
		Assert.Expect(w.Vel(b).Y < -3f, "gravity resumes after omit disabled");
	}

	// ------------------------------------------------------------------ axis locks
	static IEnumerator AllAxisLocks()
	{
		using var w = new PhysxWorld(false);
		var axes = new (PhysicsServer3D.BodyAxis axis, Vector3 impulse, char comp, bool angular)[] {
			(PhysicsServer3D.BodyAxis.LinearX, new Vector3(3, 0, 0), 'x', false),
			(PhysicsServer3D.BodyAxis.LinearY, new Vector3(0, 3, 0), 'y', false),
			(PhysicsServer3D.BodyAxis.LinearZ, new Vector3(0, 0, 3), 'z', false),
			(PhysicsServer3D.BodyAxis.AngularX, new Vector3(2, 0, 0), 'x', true),
			(PhysicsServer3D.BodyAxis.AngularY, new Vector3(0, 2, 0), 'y', true),
			(PhysicsServer3D.BodyAxis.AngularZ, new Vector3(0, 0, 2), 'z', true),
		};
		float x = 0;
		foreach (var (axis, imp, comp, angular) in axes)
		{
			var b = w.MakeBody(w.Box(0.5f), new Vector3(x, 15, 0));
			PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
			PhysicsServer3D.BodySetAxisLock(b, axis, true);
			Assert.Expect(PhysicsServer3D.BodyIsAxisLocked(b, axis), $"axis {axis} lock flag round-trip");
			if (angular) PhysicsServer3D.BodyApplyTorqueImpulse(b, imp);
			else PhysicsServer3D.BodyApplyCentralImpulse(b, imp);
			yield return Wait.Frames(3);
			var v = angular ? w.AngVel(b) : w.Vel(b);
			float locked = comp == 'x' ? v.X : comp == 'y' ? v.Y : v.Z;
			Assert.Expect(Math.Abs(locked) < 1e-3f,
				$"{axis} locked: {imp} {'t'}orque/impulse produces no {comp} response (got {locked:E2})");
			x += 3;
		}
	}
	static IEnumerator CombinedLinearLocks()
	{
		using var w = new PhysxWorld(false);
		// Lock X+Z: only Y motion allowed.
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 15, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetAxisLock(b, PhysicsServer3D.BodyAxis.LinearX, true);
		PhysicsServer3D.BodySetAxisLock(b, PhysicsServer3D.BodyAxis.LinearZ, true);
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(3, 2, 3));
		yield return Wait.Frames(3);
		var v = w.Vel(b);
		Assert.Expect(Math.Abs(v.X) < 1e-3f && Math.Abs(v.Z) < 1e-3f, "locked axes stay zero under combined lock");
		Assert.ExpectNear(v.Y, 2f, 0.05f, "free axis still moves under combined lock");
	}
	static IEnumerator CombinedAngularLocks()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 15, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetAxisLock(b, PhysicsServer3D.BodyAxis.AngularX, true);
		PhysicsServer3D.BodySetAxisLock(b, PhysicsServer3D.BodyAxis.AngularZ, true);
		PhysicsServer3D.BodyApplyTorqueImpulse(b, new Vector3(2, 2, 2));
		yield return Wait.Frames(3);
		var wv = w.AngVel(b);
		Assert.Expect(Math.Abs(wv.X) < 1e-3f && Math.Abs(wv.Z) < 1e-3f, "locked angular axes stay zero");
		Assert.Expect(wv.Y > 0.5f, "free angular axis spins");
	}
	static IEnumerator FullLinearLock()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(1, 15, 2));
		foreach (var ax in new[] { PhysicsServer3D.BodyAxis.LinearX, PhysicsServer3D.BodyAxis.LinearY, PhysicsServer3D.BodyAxis.LinearZ })
			PhysicsServer3D.BodySetAxisLock(b, ax, true);
		yield return Wait.Frames(90);
		Assert.ExpectVecNear(w.Pos(b).Origin, new Vector3(1, 15, 2), 0.01f, "fully linear-locked body frozen under gravity");
	}

	// ------------------------------------------------------------------ sleeping
	static IEnumerator GoesToSleep()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 8f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "resting body goes to sleep");
		Assert.ExpectNear(w.Pos(b).Origin.Y, 0.4f, 0.12f, "slept body at rest height");
	}
	static IEnumerator CanSleepFalse()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 8f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.CanSleep, false);
		Assert.Expect(PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.CanSleep).AsBool() == false, "can_sleep round-trip");
		yield return Wait.Frames(500);
		Assert.Expect(!w.Sleeping(b), "can_sleep=false body never sleeps");
	}
	static IEnumerator WakeOnImpulse()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 8f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "body asleep");
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(0, 4, 0));
		yield return Wait.Frames(5);
		Assert.Expect(!w.Sleeping(b), "sleeping body wakes on impulse");
		yield return Wait.Frames(40);
		Assert.Expect(w.Pos(b).Origin.Y > 0.3f && w.Pos(b).Origin.Y < 3f, "woken body jumped and re-settled");
	}
	static IEnumerator WakeOnVelocitySet()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 8f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "body asleep");
		w.SetVel(b, new Vector3(0, 3, 0));
		yield return Wait.Frames(5);
		Assert.Expect(!w.Sleeping(b), "sleeping body wakes on velocity set");
	}
	static IEnumerator SleepingIgnoresGravity()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 8, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, true);
		yield return Wait.Frames(90);
		Assert.Expect(w.Sleeping(b), "body stays asleep mid-air");
		Assert.ExpectNear(w.Pos(b).Origin.Y, 8f, 0.02f, "sleeping body ignores gravity (frozen mid-air)");
	}
	static IEnumerator StackSettles()
	{
		using var w = new PhysxWorld();
		var stack = new Rid[6];
		for (int i = 0; i < 6; i++)
		{
			stack[i] = w.MakeBody(w.Box(0.45f), new Vector3(0, 0.45f + i * 0.92f, 0));
			PhysicsServer3D.BodySetParam(stack[i], PhysicsServer3D.BodyParameter.Bounce, 0f);
		}
		yield return Wait.UntilOrFail(() => stack.All(w.Sleeping), 900, "stack sleeps");
		for (int i = 0; i < 6; i++)
		{
			Assert.Expect(w.Sleeping(stack[i]), $"stack box {i} asleep");
			Assert.ExpectNear(w.Pos(stack[i]).Origin.X, 0f, 0.35f, $"stack box {i} not laterally exploded");
			Assert.Expect(PhysxWorld.Finite(w.Vel(stack[i])), $"stack box {i} velocity finite");
		}
	}
	static IEnumerator SleepWakeCycleStable()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		for (int i = 0; i < 5; i++)
		{
			PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, true);
			yield return Wait.Frames(10);
			Assert.Expect(w.Sleeping(b), $"cycle {i}: sleeps on command");
			PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(0, 3, 0));
			yield return Wait.Frames(10);
			Assert.Expect(!w.Sleeping(b), $"cycle {i}: wakes on impulse");
		}
		Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after sleep/wake cycles");
	}
	static IEnumerator WakeAfterSupportRemoval()
	{
		using var w = new PhysxWorld();
		// PhysicsServer3D box data is half-extents, so this is a 0.7 m cube.
		var box = w.AdoptShape(PhysicsServer3D.BoxShapeCreate());
		PhysicsServer3D.ShapeSetData(box, new Vector3(0.35f, 0.35f, 0.35f));
		var support = w.MakeBody(box, new Vector3(0, 0.35f, 0));
		var dependent = w.MakeBody(box, new Vector3(0, 1.05f, 0));
		yield return Wait.UntilOrFail(() => w.Sleeping(support) && w.Sleeping(dependent), 900, "two-box stack sleeps");

		var startY = w.Pos(dependent).Origin.Y;
		PhysicsServer3D.FreeRid(support);
		yield return Wait.UntilOrFail(() => w.Pos(dependent).Origin.Y < startY - 0.5f, 240,
			"dependent wakes and falls after its support is removed");
		Assert.Expect(PhysxWorld.Finite(w.Pos(dependent)), "dependent remains finite after support removal");
	}

	// ------------------------------------------------------------------ CCD
	static IEnumerator CcdOffTunnels()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.025f, 4, 4), new Vector3(0, 2, 0)); // 5 cm wall
		var p = w.MakeBody(w.Sphere(0.1f), new Vector3(-4, 2, 0));
		PhysicsServer3D.BodySetParam(p, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetState(p, PhysicsServer3D.BodyState.CanSleep, false);
		w.SetVel(p, new Vector3(150, 0, 0)); // 2.5 m per 60 Hz step
		yield return Wait.Frames(12);
		Assert.Expect(w.Pos(p).Origin.X > 1f,
			$"150 m/s sphere tunnels through thin wall without CCD (x={w.Pos(p).Origin.X:F2})");
	}
	static IEnumerator CcdOnBlocks()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.025f, 4, 4), new Vector3(0, 2, 0));
		var p = w.MakeBody(w.Sphere(0.1f), new Vector3(-4, 2, 0));
		PhysicsServer3D.BodySetParam(p, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetEnableContinuousCollisionDetection(p, true);
		PhysicsServer3D.BodySetState(p, PhysicsServer3D.BodyState.CanSleep, false);
		w.SetVel(p, new Vector3(150, 0, 0));
		yield return Wait.Frames(12);
		Assert.Expect(w.Pos(p).Origin.X < -0.05f,
			$"150 m/s sphere with CCD stopped by thin wall (x={w.Pos(p).Origin.X:F2})");
	}
	static IEnumerator CcdSpeedsAndDiagonal()
	{
		using var w = new PhysxWorld(false);
		foreach (var (speed, dx, dy) in new[] { (60f, 1f, 0f), (120f, 1f, 0f), (90f, 0.7071f, 0.7071f) })
		{
			w.MakeStatic(w.Box(0.03f, 6, 6), new Vector3(0, 3, 0));
			var dir = new Vector3(dx, dy, 0);
			var p = w.MakeBody(w.Sphere(0.1f), -dir * 4f + new Vector3(0, 3, 0));
			PhysicsServer3D.BodySetParam(p, PhysicsServer3D.BodyParameter.GravityScale, 0f);
			PhysicsServer3D.BodySetEnableContinuousCollisionDetection(p, true);
			PhysicsServer3D.BodySetState(p, PhysicsServer3D.BodyState.CanSleep, false);
			w.SetVel(p, dir * speed);
			yield return Wait.Frames(12);
			var pos = w.Pos(p).Origin;
			// Blocking is measured on the wall-normal (x) axis for every
			// trajectory: a blocked projectile's x parks at the contact
			// standoff (-0.03 face - 0.1 radius = -0.13), while its tangential
			// component may legitimately keep sliding along the wall face. The
			// old along-projection assertion wrongly failed that slide.
			Assert.Expect(pos.X < 0.2f,
				$"{speed} m/s {dir} projectile with CCD stays this side of wall (x={pos.X:F2}, pos={pos})");
		}
	}
	static IEnumerator CcdToggle()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(0.025f, 4, 4), new Vector3(0, 2, 0));
		var p = w.MakeBody(w.Sphere(0.1f), new Vector3(-4, 2, 0));
		PhysicsServer3D.BodySetParam(p, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodySetState(p, PhysicsServer3D.BodyState.CanSleep, false);
		w.SetVel(p, new Vector3(150, 0, 0));
		yield return Wait.Frames(12);
		bool tunneled = w.Pos(p).Origin.X > 1f;
		Assert.Expect(tunneled, "pre-toggle run tunneled");
		// Bring it back and enable CCD on the same body.
		w.Teleport(p, new Vector3(-4, 2, 0));
		w.SetVel(p, Vector3.Zero);
		PhysicsServer3D.BodySetEnableContinuousCollisionDetection(p, true);
		yield return Wait.Frames(2);
		w.SetVel(p, new Vector3(150, 0, 0));
		yield return Wait.Frames(12);
		Assert.Expect(w.Pos(p).Origin.X < -0.05f,
			$"same body now blocked after enabling CCD (x={w.Pos(p).Origin.X:F2})");
	}

	// Regression (Phase 11 review, C1): a query issued from inside the force
	// integrator used to re-enter sync()/_finish_step while the stepping flag
	// was still set — the whole post pipeline (including state-sync callbacks)
	// ran mid-step, and a CPU soft body in the space would have recursed
	// unboundedly. Stepping now spans exactly simulate→fetch, so the
	// integrator's query runs against the idle scene and the post pipeline
	// fires exactly once per tick.
	static IEnumerator QueryInIntegratorNoReentry()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
		int integrations = 0;
		int syncs = 0;
		PhysicsServer3D.BodySetForceIntegrationCallback(b, Callable.From((Variant state, Variant userdata) =>
		{
			integrations++;
			w.Ray(new Vector3(0, 10, 0), new Vector3(0, -10, 0));
		}), 0);
		PhysicsServer3D.BodySetStateSyncCallback(b, Callable.From((Variant state) =>
		{
			syncs++;
		}));
		yield return Wait.Frames(5);
		integrations = 0;
		syncs = 0;
		yield return Wait.Frames(30);
		Assert.Expect(integrations >= 28, $"integrator ran each tick ({integrations}/30)");
		Assert.Expect(Mathf.Abs(syncs - integrations) <= 1,
			$"exactly one post pipeline per tick (state syncs={syncs}, integrations={integrations})");
	}
	// F-07 regression: the node scale carried on BODY_STATE_TRANSFORM must
	// bake into the collision geometry (godot_physics scales the whole body
	// transform). Half-extents 1 * scale (4,2,3) -> world half-extents (4,2,3).
	static IEnumerator ScaledStaticBoxQueries()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(1f), new Vector3(0, 5, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform,
			new Transform3D(new Basis(new Vector3(4, 0, 0), new Vector3(0, 2, 0), new Vector3(0, 0, 3)), new Vector3(0, 5, 0)));
		yield return Wait.Frames(2);

		// Ray down onto the scaled top face (y = 5 + 2*1 = 7).
		var hit = w.Ray(new Vector3(0, 14, 0), new Vector3(0, 0, 0));
		Assert.Expect(hit.Count > 0, "ray hits the scaled box");
		if (hit.Count > 0)
		{
			float topY = ((Vector3)hit["position"]).Y;
			Assert.ExpectNear(topY, 7f, 0.06f, $"scaled top face height (got {topY:F2}, expected 7.0)");
		}

		// Scaled extents: inside/outside the scaled x extent (±4).
		Assert.Expect(w.Point(new Vector3(3.9f, 5f, 0f)).Count > 0, "point inside the scaled extent hits");
		Assert.Expect(w.Point(new Vector3(4.1f, 5f, 0f)).Count == 0, "point outside the scaled extent misses");
		// Unscaled half-extent would have ended at ±1: the scale took effect.
		Assert.Expect(w.Point(new Vector3(0, 5f, 0f)).Count > 0, "point at the center hits");
	}

	// A sphere of radius 0.5 on a (2,2,2)-scaled body collides at world
	// radius 1.0, so it comes to rest one meter above the floor; mass stays
	// finite and the inertia tensor stays finite (mass is preserved, inertia
	// follows the scaled geometry).
	static IEnumerator ScaledSphereRest()
	{
		using var w = new PhysxWorld(true);
		var floor = w.MakeStatic(w.Box(20f, 0.5f, 20f), new Vector3(0, -0.5f, 0));
		var s = w.MakeBody(w.Sphere(0.5f), new Vector3(0, 5f, 0));
		PhysicsServer3D.BodySetState(s, PhysicsServer3D.BodyState.Transform,
			new Transform3D(new Basis(new Vector3(2, 0, 0), new Vector3(0, 2, 0), new Vector3(0, 0, 2)), new Vector3(0, 5f, 0)));
		PhysicsServer3D.BodySetParam(s, PhysicsServer3D.BodyParameter.Mass, 2f);
		yield return Wait.Frames(120);

		float restY = w.Pos(s).Origin.Y;
		Assert.ExpectNear(restY, 1.0f, 0.12f, $"scaled sphere rests at world radius 1.0 (y={restY:F2})");
		Assert.Expect(PhysxWorld.Finite(w.Pos(s)), "scaled body state finite");
		Assert.ExpectNear(PhysicsServer3D.BodyGetParam(s, PhysicsServer3D.BodyParameter.Mass).AsSingle(), 2f, 1e-3f,
			"mass preserved across scale bake");
	}

	// A mirrored (-x) body scale flips asymmetric convex collision geometry:
	// an off-center hull that extends toward +x extends toward -x instead.
	static IEnumerator MirroredConvexCollision()
	{
		using var w = new PhysxWorld(false);
		// Asymmetric hull: a box from x 0.2..1.2, y/z ±0.5 (offset toward +x).
		var pts = new System.Collections.Generic.List<Vector3>();
		for (int i = 0; i < 8; i++)
		{
			pts.Add(new Vector3(
				(i & 1) == 0 ? 0.2f : 1.2f,
				(i & 2) == 0 ? -0.5f : 0.5f,
				(i & 4) == 0 ? -0.5f : 0.5f));
		}
		var shape = PhysicsServer3D.ConvexPolygonShapeCreate();
		w.AdoptShape(shape);
		PhysicsServer3D.ShapeSetData(shape, new Vector3[] { pts[0], pts[1], pts[2], pts[3], pts[4], pts[5], pts[6], pts[7] });

		var b = w.MakeStatic(shape, new Vector3(0, 0, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform,
			new Transform3D(new Basis(new Vector3(-1, 0, 0), new Vector3(0, 1, 0), new Vector3(0, 0, 1)), new Vector3(0, 0, 0)));
		yield return Wait.Frames(2);

		// Mirrored extent: x -1.2..-0.2. A ray from +x must first touch -0.2.
		var hit = w.Ray(new Vector3(3, 0, 0), new Vector3(-3, 0, 0));
		Assert.Expect(hit.Count > 0, "mirrored convex is hit");
		if (hit.Count > 0)
		{
			float hitX = ((Vector3)hit["position"]).X;
			Assert.ExpectNear(hitX, -0.2f, 0.06f, $"mirrored front face at -0.2 (got {hitX:F2})");
		}
		// The unmirrored +x side must be empty: a +x-directed ray from the
		// origin misses the mirrored hull entirely (it would hit the 0.2 face
		// of the unmirrored hull).
		Assert.Expect(w.Ray(new Vector3(0, 0, 0), new Vector3(2, 0, 0)).Count == 0,
			"the unmirrored +x side is empty after mirroring");
	}

	// A scaled area covers the scaled volume: a 1 m box area scaled (4,4,4)
	// replaces gravity with zero-g for anything inside ±4 of its center,
	// while a control body outside still falls.
	static IEnumerator ScaledAreaGravityOverride()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(1f), new Vector3(0, 6, 0));
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.GravityOverrideMode,
			(int)PhysicsServer3D.AreaSpaceOverrideMode.Replace);
		PhysicsServer3D.AreaSetParam(area, PhysicsServer3D.AreaParameter.Gravity, 0f);
		PhysicsServer3D.AreaSetMonitorable(area, true);

		var inside = w.MakeBody(w.Sphere(0.25f), new Vector3(0, 6f, 0));
		var outside = w.MakeBody(w.Sphere(0.25f), new Vector3(7, 6f, 0)); // beyond the ±4 extent
		yield return Wait.Frames(60);

		Assert.Expect(Mathf.Abs(w.Pos(inside).Origin.Y - 6f) < 0.8f,
			$"inside the scaled zero-g area: no fall (y={w.Pos(inside).Origin.Y:F2})");
		Assert.Expect(w.Pos(outside).Origin.Y < 4.5f,
			$"control body outside the scaled area falls (y={w.Pos(outside).Origin.Y:F2})");
	}
	// F-11: a continuously sleeping body must not receive state-sync callbacks
	// (zero per-step cost); waking resumes them.
	static IEnumerator SleepGatesStateSync()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Sphere(0.25f), new Vector3(0, 3, 0));
		int syncs = 0;
		PhysicsServer3D.BodySetStateSyncCallback(b, Callable.From((Variant state) => { syncs++; }));
		yield return Wait.Frames(10); // falling, awake
		Assert.Expect(syncs > 0, "awake body syncs");
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, true);
		yield return Wait.Frames(2);
		syncs = 0;
		yield return Wait.Frames(30);
		Assert.Expect(syncs == 0, $"continuously sleeping body emits no state syncs (got {syncs})");
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(0.3f, 0, 0));
		yield return Wait.Frames(5);
		Assert.Expect(syncs > 0, "waking body resumes state syncs");
	}

	// ------------------------------------------------- Phase 3: state fidelity

	// OBJ-1: body_get_state(TRANSFORM) must round-trip the set-path
	// decomposition — get_scale() (per-axis lengths baked into the shapes) and
	// get_rotation_quaternion(). The old code composed the scale and then
	// overwrote the basis with set_quaternion, silently returning a pure
	// rotation.
	static IEnumerator ScaledTransformRoundTrip()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 5, 0));
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f); // hold the pose
		var basis = new Basis(new Vector3(0, 1, 0), Mathf.DegToRad(30f)).Scaled(new Vector3(2, 3, 4));
		var xf = new Transform3D(basis, new Vector3(1, 6, -2));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, xf);
		yield return Wait.Frames(2);
		var back = w.Pos(b);
		Assert.ExpectVecNear(back.Origin, xf.Origin, 1e-3f, "origin round-trips");
		Assert.ExpectVecNear(back.Basis.Scale, xf.Basis.Scale, 1e-3f, "axis scale round-trips (get_scale)");
		Assert.Expect(back.Basis.GetRotationQuaternion().AngleTo(xf.Basis.GetRotationQuaternion()) < 1e-3f,
			"rotation round-trips (get_rotation_quaternion)");
		Assert.Expect(back.Basis.Scale.Length() > 1.01f, "returned basis actually carries scale (not a pure rotation)");
		// Fixed point: setting the read-back transform again must be stable.
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, back);
		yield return Wait.Frames(2);
		var back2 = w.Pos(b);
		Assert.ExpectVecNear(back2.Basis.Scale, back.Basis.Scale, 1e-4f, "second round-trip is a fixed point (scale)");
		Assert.Expect(back2.Basis.GetRotationQuaternion().AngleTo(back.Basis.GetRotationQuaternion()) < 1e-4f,
			"second round-trip is a fixed point (rotation)");
	}

	// OBJ-2: the mass-space diagonal lives in the principal-axes (COM) frame,
	// whose rotation relative to the actor is getCMassLocalPose().q. World
	// inverse inertia = (R_actor·R_com)·diag·(R_actor·R_com)ᵀ. A rotated box
	// shape gives the actor a rotated principal frame, so using only the
	// actor rotation (the old code) misses by the shape's 30°.
	static IEnumerator InverseInertiaTensorWorldFrame()
	{
		using var w = new PhysxWorld(false);
		var shapeRot = new Basis(new Vector3(0, 0, 1), Mathf.DegToRad(30f));
		var b = w.MakeBody(w.Box(0.5f, 1.0f, 1.5f), new Vector3(3, 5, 0), mass: 2f,
			shapeXf: new Transform3D(shapeRot, Vector3.Zero));
		var bodyRot = new Basis(new Vector3(0, 1, 0), Mathf.DegToRad(40f));
		w.Teleport(b, new Vector3(3, 5, 0), bodyRot);
		yield return Wait.Frames(2);
		var st = w.Direct(b);
		// Box full extents (1, 2, 3), m = 2: I = m/12·(sum of squared other extents).
		var diag = new Basis(
			new Vector3(6f / 13f, 0, 0),   // 1/Ix, Ix = 2/12·(4+9)
			new Vector3(0, 3f / 5f, 0),    // 1/Iy, Iy = 2/12·(1+9)
			new Vector3(0, 0, 6f / 5f));   // 1/Iz, Iz = 2/12·(1+4)
		var R = bodyRot * shapeRot;
		var expected = R * diag * R.Transposed();
		var inv = st.InverseInertiaTensor;
		for (int i = 0; i < 3; i++)
		{
			Assert.ExpectVecNear(inv[i], expected[i], 2e-3f, $"world inverse inertia column {i} matches analytic");
		}
		// Principal axes reported by the state must align with the shape
		// frame (order-insensitive: each shape axis matches some principal
		// axis; sign flips from the eigen-decomposition are fine).
		var pia = st.PrincipalInertiaAxes;
		for (int i = 0; i < 3; i++)
		{
			bool aligned = false;
			for (int j = 0; j < 3; j++)
			{
				if (Mathf.Abs(pia[j].Normalized().Dot(shapeRot[i].Normalized())) > 0.999f)
				{
					aligned = true;
					break;
				}
			}
			Assert.Expect(aligned, $"shape axis {i} aligns with a principal inertia axis");
		}
	}

	// OBJ-3: Godot stops _integrate_forces while a body sleeps. The last call
	// fires on the last awake step (sleep happens during the solve that
	// follows its pre-step), then the callback must go silent until wake.
	static IEnumerator FiCallbackSleepCadence()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Sphere(0.25f), new Vector3(0, 3, 0));
		int calls = 0;
		PhysicsServer3D.BodySetForceIntegrationCallback(b,
			Callable.From<PhysicsDirectBodyState3D>(st => calls++));
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "body falls asleep");
		yield return Wait.Frames(5);
		int atSleep = calls;
		Assert.Expect(atSleep > 0, "callback fired while the body was awake");
		yield return Wait.Frames(60);
		Assert.Expect(calls == atSleep, $"no FI callbacks while asleep (got {calls - atSleep} in 60 frames)");
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, false);
		yield return Wait.Frames(10);
		Assert.Expect(calls > atSleep, "waking resumes FI callbacks");
	}

	// OBJ-6: re-assigning the same param (inspector/scene-reload churn) and
	// damping changes must not re-arm the wake counter of a sleeping body.
	static IEnumerator ParamSetKeepsSleeping()
	{
		using var w = new PhysxWorld();
		var b = w.MakeBody(w.Sphere(0.25f), new Vector3(0, 3, 0));
		yield return Wait.UntilOrFail(() => w.Sleeping(b), 600, "body falls asleep");
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDamp, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Bounce, 0f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Friction, 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Combine);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDampMode, (int)PhysicsServer3D.BodyDampMode.Combine);
		yield return Wait.Frames(30);
		Assert.Expect(w.Sleeping(b), "no-op param writes leave the body asleep");
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDamp, 0.5f);
		yield return Wait.Frames(30);
		Assert.Expect(w.Sleeping(b), "a damping change does not wake a sleeping body either");
	}

	// OBJ-9: LINEAR/ANGULAR_DAMP_MODE must round-trip through get_param.
	static IEnumerator DampModeParamRoundTrip()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 5, 0));
		Assert.Expect((int)PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode)
			== (int)PhysicsServer3D.BodyDampMode.Combine, "linear damp mode defaults to COMBINE");
		Assert.Expect((int)PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.AngularDampMode)
			== (int)PhysicsServer3D.BodyDampMode.Combine, "angular damp mode defaults to COMBINE");
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDampMode, (int)PhysicsServer3D.BodyDampMode.Replace);
		Assert.Expect((int)PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.LinearDampMode)
			== (int)PhysicsServer3D.BodyDampMode.Replace, "linear damp mode round-trips");
		Assert.Expect((int)PhysicsServer3D.BodyGetParam(b, PhysicsServer3D.BodyParameter.AngularDampMode)
			== (int)PhysicsServer3D.BodyDampMode.Replace, "angular damp mode round-trips");
		yield break;
	}

	// Completion-criteria scenario: a tilted, scaled body with a custom COM
	// keeps the origin-relative COM under rotation and round-trips scale.
	// (DirectBodyState3D.CenterOfMass is a WORLD-frame OFFSET from the
	// transform origin — the built-in godot_physics_3d stores and returns it
	// exactly that way (basis.xform(center_of_mass_local)); it is NOT the
	// global COM position.)
	static IEnumerator ScaledCustomComTiltRoundTrip()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 5, 0), mass: 1f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f); // hold the pose
		var com = new Vector3(0.25f, -0.1f, 0.05f);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.CenterOfMass, com);
		var basis = new Basis(new Vector3(1, 0, 0), Mathf.DegToRad(90f)).Scaled(new Vector3(2, 1, 1));
		var xf = new Transform3D(basis, new Vector3(0, 5, 0));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, xf);
		yield return Wait.Frames(2);
		var st = w.Direct(b);
		var R = new Basis(basis.GetRotationQuaternion());
		Assert.ExpectVecNear(st.CenterOfMass, R * com, 1e-3f,
			"custom COM origin-relative offset follows the tilted frame");
		var back = w.Pos(b);
		Assert.ExpectVecNear(back.Basis.Scale, xf.Basis.Scale, 1e-3f, "scale survives the tilt round-trip (get_scale)");
		Assert.Expect(back.Basis.GetRotationQuaternion().AngleTo(xf.Basis.GetRotationQuaternion()) < 1e-3f, "tilt orientation round-trips");
	}

	// OBJ-13 (Phase 13): the SDK documents no frame for PxRigidDynamicLockFlag
	// — this probe pins the actual semantics. The body is rotated 90° about Z
	// (local Y = world -X, local X = world Y) and LINEAR_Y is locked; a world-Y
	// impulse then separates the two hypotheses: world-frame locks block the
	// motion entirely (matching Godot's world-axis lock semantics), actor-frame
	// locks would let it through as local -X.
	static IEnumerator AxisLockFrameProbe()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 0, 0));
		var rot = new Basis(new Vector3(0, 0, 1), Mathf.DegToRad(90f));
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, new Transform3D(rot, new Vector3(0, 0, 0)));
		PhysicsServer3D.BodySetAxisLock(b, PhysicsServer3D.BodyAxis.LinearY, true);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.GravityScale, 0f);
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(0, 3, 0));
		yield return Wait.Frames(30);
		Vector3 v = w.Vel(b);
		Assert.Expect(Mathf.Abs(v.Y) < 0.01f,
			$"world-Y motion is blocked by LINEAR_Y lock on a 90°-rotated body (vy={v.Y:F3}) — locks are WORLD-frame");
		// Control: the perpendicular world axis (body-local Y, unlocked) moves.
		PhysicsServer3D.BodyApplyCentralImpulse(b, new Vector3(2, 0, 0));
		yield return Wait.Frames(30);
		Vector3 v2 = w.Vel(b);
		Assert.Expect(Mathf.Abs(v2.X) > 0.1f, "world-X motion stays free (body-local Y)");
	}
}
