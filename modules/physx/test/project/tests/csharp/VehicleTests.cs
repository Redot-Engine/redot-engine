// Vehicles (module vehicle2 wrapper): both archetypes, deterministic flat
// track, measured driving/braking/steering behavior, wheel telemetry, and
// chassis/vehicle destruction interplay. Access route is reported per run
// (strongly-typed C# bindings when available, GDScript bridge otherwise).

using System;
using System.Collections;
using System.Linq;

namespace PhysxTestProject.Tests;

internal static class VehicleTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-VEHI-001", "vehicle create (both archetypes) + wheel count", CreateAndWheels);
		s.Add("PHYSX-VEHI-002", "DirectDrive vehicle drives forward under throttle", DrivesForward);
		s.Add("PHYSX-VEHI-003", "braking stops the vehicle", BrakingStops);
		s.Add("PHYSX-VEHI-004", "steering yaws the chassis", SteeringYaws);
		s.Add("PHYSX-VEHI-005", "reverse drives backward", ReverseDrives);
		s.Add("PHYSX-VEHI-006", "wheel telemetry reports contact on flat ground", TelemetryContact);
		s.Add("PHYSX-VEHI-007", "wheel telemetry fields are present and finite", TelemetryFields);
		s.Add("PHYSX-VEHI-008", "engine state dict for EngineDrive archetype", EngineState);
		s.Add("PHYSX-VEHI-009", "chassis settles and sleeps when idle", IdleSettles);
		s.Add("PHYSX-VEHI-010", "freeing chassis releases vehicle safely", FreeChassis);
		s.Add("PHYSX-VEHI-011", "freeing vehicle leaves chassis usable", FreeVehicle);
		s.Add("PHYSX-VEHI-012", "vehicle_set_space explicit assignment works", ExplicitSpace);
		s.Add("PHYSX-VEHI-013", "response params retune steer lock (per-vehicle tuning)", ResponseTuning);
		s.Add("PHYSX-VEHI-014", "Ackermann splits inner/outer steer angles, mean preserved", AckermannAngles);
		s.Add("PHYSX-VEHI-015", "Ackermann percent=0 falls back to parallel steer", AckermannParallel);
		s.Add("PHYSX-VEHI-016", "Ackermann inner wheel matches the measured turn side", AckermannTurnDirection);
		s.Add("PHYSX-VEHI-017", "2-wheeler balance assist keeps the bike upright", TwoWheelerBalances);
		s.Add("PHYSX-VEHI-018", "balance telemetry reports lean/roll-rate/assist", BalanceTelemetry);
		s.Add("PHYSX-VEHI-019", "Ackermann negative steer mirrors left-turn geometry, yaws toward +X", AckermannNegative);
		s.Add("PHYSX-VEHI-020", "surface_frictions grip table survives freeing the ground body", SurfaceFrictionsGroundFree);
		s.Add("PHYSX-VEHI-021", "counter-rotating wheel torques yaw the chassis (skid steer)", SkidSteerYaws);
		s.Add("PHYSX-VEHI-022", "vehicle follows the chassis into a new space", ChassisSpaceFollow);
		s.Add("PHYSX-VEHI-023", "per-wheel torque raise does not stick in scalar mode (maxResponse restore)", PerWheelResponseRestore);
		s.Add("PHYSX-VEHI-024", "anti-roll configuration survives a wheel-count rebuild", AntiRollSurvivesRebuild);
	}

	static (Rid chassis, Rid vehicle) MakeVehicle(PhysxWorld w, int archetype = 0, float mass = 800f)
	{
		var chassis = w.MakeBody(w.Box(0.9f, 0.3f, 2.0f), new Vector3(0, 1.0f, 0), mass: mass);
		PhysicsServer3D.BodySetParam(chassis, PhysicsServer3D.BodyParameter.Bounce, 0f);
		var vehicle = VehicleApi.CreateVehicle(archetype);
		w.TrackVehicle(vehicle);
		VehicleApi.SetChassisBody(vehicle, chassis);
		VehicleApi.SetVehicleSpace(vehicle, w.Space);
		VehicleApi.SetWheelCount(vehicle, 4);
		for (int i = 0; i < 4; i++)
		{
			VehicleApi.SetWheelParams(vehicle, i, new Godot.Collections.Dictionary
			{
				["radius"] = 0.4f,
				["suspension_travel"] = 0.3f,
				["local_pose"] = new Transform3D(Basis.Identity, new Vector3(-0.7f + 1.4f * (i % 2), -0.05f, -0.7f + 1.4f * (i / 2))),
				["steer"] = i < 2,       // front axle steers
				["front"] = i < 2,
				["traction"] = true,
				["brake"] = true,
			});
		}
		return (chassis, vehicle);
	}

	static IEnumerator CreateAndWheels()
	{
		using var w = new PhysxWorld();
		var (chassis, direct) = MakeVehicle(w, 0);
		Assert.Expect(direct.IsValid, "DirectDrive vehicle RID valid");
		Assert.Expect(VehicleApi.GetWheelCount(direct) == 4, "wheel count = 4");
		var (c2, engine) = MakeVehicle(w, 1);
		Assert.Expect(engine.IsValid, "EngineDrive vehicle RID valid");
		Assert.Expect(VehicleApi.GetWheelCount(engine) == 4, "EngineDrive wheel count = 4");
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis finite while mounted");
	}
	static IEnumerator DrivesForward()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(20); // let it settle on wheels
		float z0 = w.Pos(chassis).Origin.Z;
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(180); // 3 s of throttle
		float dz = w.Pos(chassis).Origin.Z - z0;
		Assert.Expect(Mathf.Abs(dz) > 2f, $"vehicle travels under throttle (|dz|={Mathf.Abs(dz):F2} m)");
		Assert.Expect(w.Vel(chassis).Length() < 60f, "vehicle speed sane");
	}
	static IEnumerator BrakingStops()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(20);
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(120);
		VehicleApi.SetControlInputs(v, 0f, 1f, 0f, 0f);
		yield return Wait.UntilOrFail(() => w.Vel(chassis).Length() < 0.5f, 300, "vehicle stops under braking");
		Assert.Expect(w.Vel(chassis).Length() < 0.5f, "brake brings vehicle to a stop");
	}
	static IEnumerator SteeringYaws()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(20);
		VehicleApi.SetControlInputs(v, 0.6f, 0f, 0.5f, 0f);
		float heading0 = w.Pos(chassis).Basis.GetEuler().Y;
		yield return Wait.Frames(240);
		float heading1 = w.Pos(chassis).Basis.GetEuler().Y;
		float dYaw = Math.Abs(heading1 - heading0);
		dYaw = Math.Min(dYaw, Mathf.Tau - dYaw);
		Assert.Expect(dYaw > 0.2f, $"steering changes heading ({dYaw:F2} rad)");
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis finite while steering");
	}
	static IEnumerator ReverseDrives()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(20);
		float z0 = w.Pos(chassis).Origin.Z;
		VehicleApi.SetControlInputs(v, -1f, 0f, 0f, 0f);
		yield return Wait.Frames(180);
		float dz = w.Pos(chassis).Origin.Z - z0;
		Assert.Expect(Mathf.Abs(dz) > 1.5f, $"reverse throttle moves vehicle (|dz|={Mathf.Abs(dz):F2})");
	}
	static IEnumerator TelemetryContact()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		// Telemetry becomes valid after the first step, but the wheels only
		// reach the ground once the chassis has settled — wait for the chassis
		// to descend before asserting contact.
		yield return Wait.UntilOrFail(() =>
		{
			var states = VehicleApi.GetWheelStates(v);
			return states.Count == 4 && states.Count > 0 && ((Godot.Collections.Dictionary)states[0]).Count > 0;
		}, 120, "telemetry becomes available after first step");
		yield return Wait.UntilOrFail(() => w.Pos(chassis).Origin.Y < 1.0f, 300, "chassis settles toward the ground");
		yield return Wait.Frames(30);
		var wheelStates = VehicleApi.GetWheelStates(v);
		Assert.Expect(wheelStates.Count == 4, $"4 wheel states (got {wheelStates.Count})");
		int inContact = 0;
		foreach (var stObj in wheelStates)
		{
			var st = (Godot.Collections.Dictionary)stObj;
			if (st["in_contact"].AsBool()) inContact++;
		}
		Assert.Expect(inContact >= 3, $"{inContact}/4 wheels in contact on flat ground");
	}
	static IEnumerator TelemetryFields()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetControlInputs(v, 0.8f, 0f, 0f, 0f);
		yield return Wait.Frames(120);
		foreach (var stObj in VehicleApi.GetWheelStates(v))
		{
			var st = (Godot.Collections.Dictionary)stObj;
			Assert.Expect(st.ContainsKey("in_contact"), "field in_contact");
			Assert.Expect(st.ContainsKey("contact_point"), "field contact_point");
			Assert.Expect(st.ContainsKey("contact_normal"), "field contact_normal");
			Assert.Expect(st.ContainsKey("rpm"), "field rpm");
			Assert.Expect(st.ContainsKey("skid"), "field skid");
			Assert.Expect(st.ContainsKey("rotation"), "field rotation");
			Assert.Expect(float.IsFinite(st["rpm"].AsSingle()), "rpm finite");
			Assert.Expect(float.IsFinite(st["rotation"].AsSingle()), "rotation finite");
			if (st["in_contact"].AsBool())
				Assert.Expect(PhysxWorld.Finite(st["contact_point"].AsVector3()), "contact point finite");
		}
	}
	static IEnumerator EngineState()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w, archetype: 1);
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(120);
		var es = VehicleApi.GetEngineState(v);
		Assert.Expect(es.Count > 0, "engine state dictionary non-empty");
		Assert.Expect(es.ContainsKey("rpm"), "engine field rpm");
		Assert.Expect(es.ContainsKey("gear"), "engine field gear");
		Assert.Expect(es.ContainsKey("clutch"), "engine field clutch");
		Assert.Expect(float.IsFinite(es["rpm"].AsSingle()), "engine rpm finite");
	}
	static IEnumerator IdleSettles()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetControlInputs(v, 0f, 0f, 0f, 1f); // handbrake on
		yield return Wait.UntilOrFail(() => w.Sleeping(chassis), 900, "idle vehicle chassis sleeps");
		Assert.ExpectNear(w.Pos(chassis).Origin.Y, 0.75f, 0.35f, "chassis resting height sane");
	}
	static IEnumerator FreeChassis()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(30);
		w.Free(chassis); // vehicle must be auto-released
		w.Forget(v);     // the server releases the vehicle with its chassis
		yield return Wait.Frames(60);
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(5, 3, 0));
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(canary).Origin.Y < 2.5f, "simulation healthy after freeing chassis with vehicle attached");
	}
	static IEnumerator FreeVehicle()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(30);
		w.Free(v); // chassis must survive
		yield return Wait.Frames(30);
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis finite after vehicle freed");
		yield return Wait.UntilOrFail(() => w.Pos(chassis).Origin.Y < 1.0f, 300, "chassis falls/rests as plain body");
	}
	static IEnumerator ExplicitSpace()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetVehicleSpace(v, w.Space); // idempotent re-set
		yield return Wait.Frames(30);
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(120);
		Assert.Expect(w.Vel(chassis).Length() > 0.5f, "vehicle drives after explicit space assignment");
	}
	static IEnumerator ResponseTuning()
	{
		using var w = new PhysxWorld();
		// Two identical cars: restricted steer lock vs the built-in default.
		var (chassisA, weak) = MakeVehicle(w);
		VehicleApi.SetResponseParams(weak, new Godot.Collections.Dictionary
		{
			["max_steer_angle"] = 0.05f, // ~1/12th of the default 0.6 rad lock
		});
		var (chassisB, full) = MakeVehicle(w);
		w.Teleport(chassisB, new Vector3(6, 1.0f, 0));
		yield return Wait.Frames(20); // settle on wheels
		VehicleApi.SetControlInputs(weak, 0.3f, 0f, 1f, 0f);
		VehicleApi.SetControlInputs(full, 0.3f, 0f, 1f, 0f);
		float yawA0 = SteerYaw(chassisA, w);
		float yawB0 = SteerYaw(chassisB, w);
		yield return Wait.Frames(90);
		float dYawA = YawTravel(yawA0, SteerYaw(chassisA, w));
		float dYawB = YawTravel(yawB0, SteerYaw(chassisB, w));
		Assert.Expect(dYawB > 0.5f, $"default steer lock turns the car ({dYawB:F2} rad)");
		Assert.Expect(dYawA < 0.5f * dYawB, $"restricted steer lock turns slower ({dYawA:F2} vs {dYawB:F2} rad)");
		// A negative entry reverts the channel to the built-in default.
		VehicleApi.SetResponseParams(weak, new Godot.Collections.Dictionary { ["max_steer_angle"] = -1.0f });
		w.Teleport(chassisA, new Vector3(0, 1.0f, 0), Basis.Identity);
		w.SetVel(chassisA, Vector3.Zero);
		w.SetAngVel(chassisA, Vector3.Zero);
		yield return Wait.Frames(20);
		VehicleApi.SetControlInputs(weak, 0.3f, 0f, 1f, 0f);
		float yawR0 = SteerYaw(chassisA, w);
		yield return Wait.Frames(90);
		float dYawR = YawTravel(yawR0, SteerYaw(chassisA, w));
		Assert.Expect(dYawR > 2.0f * dYawA, $"negative entry reverts to default steer lock ({dYawR:F2} vs {dYawA:F2} rad)");
	}

	static float SteerYaw(Rid chassis, PhysxWorld w) => w.Pos(chassis).Basis.GetEuler().Y;

	static float YawTravel(float from, float to)
	{
		float d = Math.Abs(to - from);
		return Math.Min(d, Mathf.Tau - d);
	}

	// ------------------------------------------------------------------
	// Ackermann steering (PHYSX-VEHI-014/015/016/019)
	// ------------------------------------------------------------------

	static IEnumerator AckermannAngles()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetAckermannParams(v, new Godot.Collections.Dictionary
		{
			["enabled"] = true,
			["percent"] = 100.0f,
		});
		VehicleApi.SetControlInputs(v, 0f, 0f, 0.5f, 0f); // steer 0.5 -> 0.3 rad with the 0.6 rad lock
		yield return Wait.Frames(2);
		var angles = VehicleApi.GetWheelSteerAngles(v);
		Assert.Expect(angles.Length == 4, $"4 per-wheel steer angles returned (got {angles.Length})");
		Assert.Expect(Mathf.Abs(angles[2]) < 1e-4f && Mathf.Abs(angles[3]) < 1e-4f,
				"rear (non-steering) wheels report zero steer angle");
		float aL = Math.Abs(angles[0]), aR = Math.Abs(angles[1]);
		Assert.Expect(Mathf.Abs(aL - aR) > 1e-3f,
				$"Ackermann splits inner/outer angles (|left|={aL:F4} vs |right|={aR:F4})");
		// Pure Ackermann keeps the mean road-wheel angle ~equal to the command.
		float commanded = 0.5f * 0.6f;
		float mean = 0.5f * (angles[0] + angles[1]);
		Assert.Expect(Mathf.Abs(mean - commanded) < 0.02f,
				$"mean road-wheel angle tracks the command (mean={mean:F4} vs {commanded:F4})");
	}

	static IEnumerator AckermannParallel()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetAckermannParams(v, new Godot.Collections.Dictionary
		{
			["enabled"] = true,
			["percent"] = 0.0f, // fully parallel: both steered wheels get the same angle
		});
		VehicleApi.SetControlInputs(v, 0f, 0f, 0.5f, 0f);
		yield return Wait.Frames(2);
		var angles = VehicleApi.GetWheelSteerAngles(v);
		Assert.Expect(Mathf.Abs(angles[0] - angles[1]) < 1e-4f,
				$"percent=0 keeps the steered wheels parallel ({angles[0]:F4} vs {angles[1]:F4})");
		Assert.Expect(Mathf.Abs(Mathf.Abs(angles[0]) - 0.3f) < 0.01f,
				$"parallel angle equals the commanded 0.3 rad (got {Mathf.Abs(angles[0]):F4})");
	}

	static IEnumerator AckermannTurnDirection()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetAckermannParams(v, new Godot.Collections.Dictionary
		{
			["enabled"] = true,
			["percent"] = 100.0f,
		});
		yield return Wait.Frames(20);
		// Drive forward, then hold a steer command and measure which way the
		// chassis actually turned; the wheel on that side must carry the
		// larger |angle|. This validates the turn-direction sign end-to-end
		// instead of trusting the frame-convention comment in the wrapper.
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(90);
		VehicleApi.SetControlInputs(v, 0.6f, 0f, 1f, 0f);
		yield return Wait.Frames(90);
		var fwd = -w.Pos(chassis).Basis.Z;
		var angles = VehicleApi.GetWheelSteerAngles(v);
		bool turnedTowardNegX = fwd.X < 0f;
		float inner = turnedTowardNegX ? Math.Abs(angles[0]) : Math.Abs(angles[1]);
		float outer = turnedTowardNegX ? Math.Abs(angles[1]) : Math.Abs(angles[0]);
		Assert.Expect(inner > outer,
				$"inner wheel steers more than the outer on the measured turn side " +
				$"(inner={inner:F4} outer={outer:F4} turnedNegX={turnedTowardNegX})");
	}

	// Regression for the right-hand-steer quadrant bug: the Ackermann pair was
	// computed with atan2(1, cot(delta)), and cot(delta) goes negative for a
	// right command, producing second-quadrant (~pi) wheel angles that spun
	// the car instead of turning it. MakeVehicle wheel order: 0 = left-front,
	// 1 = right-front.
	static IEnumerator AckermannNegative()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		VehicleApi.SetAckermannParams(v, new Godot.Collections.Dictionary
		{
			["enabled"] = true,
			["percent"] = 100.0f,
		});
		VehicleApi.SetControlInputs(v, 0f, 0f, -0.5f, 0f);
		yield return Wait.Frames(2);
		var angles = VehicleApi.GetWheelSteerAngles(v);
		Assert.Expect(angles.Length == 4, $"4 per-wheel steer angles (got {angles.Length})");
		Assert.Expect(Mathf.Abs(angles[0]) < 0.5f && Mathf.Abs(angles[1]) < 0.5f,
				$"steer angles stay sane for a right command (l={angles[0]:F3} r={angles[1]:F3})");
		float mean = 0.5f * (angles[0] + angles[1]);
		Assert.Expect(Mathf.Abs(mean + 0.3f) < 0.02f,
				$"mean road-wheel angle tracks the commanded -0.3 rad (mean={mean:F4})");
		Assert.Expect(Mathf.Abs(angles[1]) > Mathf.Abs(angles[0]),
				$"inner wheel is on the turn side (+X) (l={angles[0]:F4} r={angles[1]:F4})");
		// End-to-end: driving forward with a held right command must yaw the
		// chassis toward +X.
		yield return Wait.Frames(20);
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(90);
		VehicleApi.SetControlInputs(v, 0.6f, 0f, -1f, 0f);
		yield return Wait.Frames(90);
		var fwd = -w.Pos(chassis).Basis.Z;
		Assert.Expect(fwd.X > 0.1f, $"right command yaws the chassis toward +X (fwd.X={fwd.X:F2})");
	}

	// ------------------------------------------------------------------
	// 2-wheeler balance assist (PHYSX-VEHI-017/018)
	// ------------------------------------------------------------------

	static (Rid chassis, Rid vehicle) MakeTwoWheeler(PhysxWorld w, bool balance = true)
	{
		var chassis = w.MakeBody(w.Box(0.4f, 0.3f, 1.8f), new Vector3(0, 1.0f, 0), mass: 200f);
		var vehicle = VehicleApi.CreateVehicle(0);
		w.TrackVehicle(vehicle);
		VehicleApi.SetChassisBody(vehicle, chassis);
		VehicleApi.SetVehicleSpace(vehicle, w.Space);
		VehicleApi.SetWheelCount(vehicle, 2);
		for (int i = 0; i < 2; i++)
		{
			VehicleApi.SetWheelParams(vehicle, i, new Godot.Collections.Dictionary
			{
				["radius"] = 0.35f,
				["suspension_travel"] = 0.25f,
				["local_pose"] = new Transform3D(Basis.Identity, new Vector3(0, -0.05f, -0.7f + 1.4f * i)),
				["steer"] = i == 0, // front wheel only (motorcycle)
				["front"] = i == 0,
				["traction"] = true,
				["brake"] = true,
			});
		}
		if (balance)
		{
			VehicleApi.SetBalanceParams(vehicle, new Godot.Collections.Dictionary
			{
				["enabled"] = true,
				["kp"] = 10.0f,
				["kd"] = 1.5f,
				["max_steer_assist"] = 0.3f,
				["low_speed_torque"] = 300.0f,
			});
		}
		return (chassis, vehicle);
	}

	static IEnumerator TwoWheelerBalances()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeTwoWheeler(w);
		yield return Wait.Frames(20);
		VehicleApi.SetControlInputs(v, 0.6f, 0f, 0f, 0f);
		yield return Wait.Frames(60); // ride up to speed first
									  // Perturbation: shove the bike sideways — the assist must steer into
									  // the resulting fall and recover (up.y back near 1 within ~2 s).
		w.SetVel(chassis, new Vector3(2.0f, 0, w.Vel(chassis).Z));
		float worstUp = 1f;
		for (int f = 0; f < 180; f++)
		{ // 3 s after the shove
			yield return Wait.Frame();
			float up = w.Pos(chassis).Basis.Y.Y;
			if (up < worstUp) worstUp = up;
		}
		Assert.Expect(worstUp > 0.7f,
				$"assisted 2-wheeler recovers from a lateral shove (min up.y={worstUp:F2})");
		float endUp = w.Pos(chassis).Basis.Y.Y;
		Assert.Expect(endUp > 0.9f, $"2-wheeler near upright at the end (up.y={endUp:F2})");
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "assisted 2-wheeler stays finite");
	}

	static IEnumerator BalanceTelemetry()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeTwoWheeler(w);
		yield return Wait.Frames(30);
		var st = VehicleApi.GetBalanceState(v);
		Assert.Expect(st.ContainsKey("lean_angle") && st.ContainsKey("roll_rate") && st.ContainsKey("steer_assist"),
				"balance state exposes lean_angle/roll_rate/steer_assist");
		float lean = (float)st["lean_angle"];
		float rate = (float)st["roll_rate"];
		float assist = (float)st["steer_assist"];
		Assert.Expect(Mathf.IsFinite(lean) && Mathf.IsFinite(rate) && Mathf.IsFinite(assist),
				$"balance telemetry is finite (lean={lean:F4} rate={rate:F4} assist={assist:F4})");
		Assert.Expect(Mathf.Abs(lean) < 0.5f, $"settled 2-wheeler leans near upright (lean={lean:F4})");
	}

	// The "surface_frictions" grip table stores raw PxMaterial pointers
	// resolved from ground bodies. Freeing a ground body mid-run must drop
	// those entries server-side, not leave a dangling material pointer for the
	// per-step suspension/tire update to consume (crashes or corrupts friction
	// on the next step). Regression for the Phase 2 lifetime fix.
	static IEnumerator SurfaceFrictionsGroundFree()
	{
		using var w = new PhysxWorld();
		var ground = w.MakeBody(w.Box(40f, 0.5f, 40f), new Vector3(0, -0.5f, 0),
				mode: PhysicsServer3D.BodyMode.Static);
		var (chassis, v) = MakeVehicle(w);
		for (int i = 0; i < 4; i++)
		{
			VehicleApi.SetWheelParams(v, i, new Godot.Collections.Dictionary
			{
				["surface_friction_default"] = 1.0f,
				["surface_frictions"] = new Godot.Collections.Dictionary
				{
					[ground] = 1.2f, // non-default grip while the ground lives
				},
			});
		}
		yield return Wait.Frames(30); // wheels settle onto the ground
		VehicleApi.SetControlInputs(v, 0.8f, 0f, 0f, 0f);
		yield return Wait.Frames(30); // drive with the grip table active
		w.Free(ground); // dangle the table pre-fix
		yield return Wait.Frames(90); // keep simulating on the (now default) grip
		var canary = w.MakeBody(w.Box(0.3f), new Vector3(5, 3, 0));
		yield return Wait.Frames(30);
		Assert.Expect(w.Pos(canary).Origin.Y < 2.5f,
				"simulation healthy after freeing a surface_frictions ground body");
		Assert.Expect(VehicleApi.GetWheelCount(v) == 4, "vehicle still valid after ground free");
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis state stays finite after ground free");
	}
	// Signed per-wheel drive: left wheels forward (+), right wheels backward
	// (-) must spin the chassis in place (tank turn) instead of clamping every
	// wheel to the dominant direction.
	static IEnumerator SkidSteerYaws()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(25); // settle on wheels
		float yaw0 = w.Pos(chassis).Basis.GetRotationQuaternion().GetEuler().Y;
		// Wheels 0,1 = front axle (i%2 pattern: 0/1 left-right? MakeVehicle
		// places x = -0.7 + 1.4*(i%2) -> even = left, odd = right).
		for (int i = 0; i < 4; i++)
		{
			float dir = (i % 2 == 0) ? 1f : -1f; // left forward, right backward
			VehicleApi.SetWheelDriveTorque(v, i, 900f * dir);
		}
		yield return Wait.Frames(120);
		float yaw1 = w.Pos(chassis).Basis.GetRotationQuaternion().GetEuler().Y;
		float dyaw = yaw1 - yaw0;
		dyaw = Mathf.Abs(Mathf.Atan2(Mathf.Sin(dyaw), Mathf.Cos(dyaw)));
		Assert.Expect(dyaw > 0.25f, $"counter-rotating wheels yaw the chassis (dyaw={dyaw:F2} rad)");
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "skid-steering state finite");
		for (int i = 0; i < 4; i++)
		{
			VehicleApi.SetWheelDriveTorque(v, i, 0f);
		}
	}

	// VEH-1: a per-wheel drive torque above the tuned maxResponse raises the
	// live maxResponse (response contract caps multipliers at 1). When the
	// channel returns to scalar mode the TUNED baseline must be restored --
	// the old code left the raised value stuck, over-driving scalar throttle.
	static IEnumerator PerWheelResponseRestore()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(25);

		float baseDrive = VehicleApi.GetResponseParams(v)["drive_max_response"].AsSingle();
		Assert.ExpectNear(baseDrive, 1000f, 1e-3f, "untuned drive baseline is 1000");

		// Raise via per-wheel demand (5x the baseline); write_commands runs on
		// the next physics step, so give it a frame before reading. Then
		// return to scalar and give that a frame too.
		for (int i = 0; i < 4; i++)
		{
			VehicleApi.SetWheelDriveTorque(v, i, 5000f);
		}
		yield return Wait.Frames(2);
		float raised = VehicleApi.GetResponseParams(v)["drive_max_response"].AsSingle();
		Assert.Expect(raised > 4000f, $"per-wheel demand raised maxResponse (got {raised:F0})");
		for (int i = 0; i < 4; i++)
		{
			VehicleApi.SetWheelDriveTorque(v, i, 0f);
		}
		yield return Wait.Frames(2);
		float restored = VehicleApi.GetResponseParams(v)["drive_max_response"].AsSingle();
		Assert.ExpectNear(restored, baseDrive, 1e-3f,
			$"scalar mode restored the tuned baseline (got {restored:F0})");
	}

	// VEH-2: anti-roll configuration must survive a wheel-count rebuild
	// (the rebuild assembles a fresh vehicle2 state; the old code only
	// re-applied wheel/engine params and silently dropped the bars).
	static IEnumerator AntiRollSurvivesRebuild()
	{
		using var w = new PhysxWorld();
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(5);

		var cfg = new Godot.Collections.Dictionary
		{
			["wheel_ids"] = new int[] { 0, 1, 2, 3 }, // left-right pairs per axle
			["stiffness"] = new float[] { 5000f, 5000f },
		};
		VehicleApi.SetAntiRollParams(v, cfg);
		var live0 = VehicleApi.GetAntiRollParams(v);
		Assert.Expect(live0["nb_bars"].AsInt32() == 2, "2 bars configured pre-rebuild");

		// Rebuild: adding a wheel re-assembles the whole vehicle2 state.
		VehicleApi.AddWheel(v);
		yield return Wait.Frames(5);

		var live1 = VehicleApi.GetAntiRollParams(v);
		Assert.Expect(live1["nb_bars"].AsInt32() == 2,
			$"anti-roll bars survive the rebuild (got {live1["nb_bars"].AsInt32()})");
		var stiffness = live1["stiffness"].AsFloat32Array();
		Assert.Expect(stiffness.Length == 2 && Mathf.Abs(stiffness[0] - 5000f) < 1e-3f,
			"stiffness values survive the rebuild");
	}

	// The vehicle must follow its chassis into a new space: after
	// body_set_space(chassis, other), driving still produces motion in the
	// chassis's (new) world -- the vehicle's update loop re-homed with it.
	static IEnumerator ChassisSpaceFollow()
	{
		using var w = new PhysxWorld();
		var ground = w.MakeStatic(w.Box(40f, 0.5f, 40f), new Vector3(0, -0.5f, 0));
		var (chassis, v) = MakeVehicle(w);
		yield return Wait.Frames(25);

		// Move the chassis to a fresh space; the vehicle must follow. The
		// ground moves with it so the wheels still have something to grip.
		var otherSpace = PhysicsServer3D.SpaceCreate();
		PhysicsServer3D.SpaceSetActive(otherSpace, true);
		PhysicsServer3D.BodySetSpace(ground, otherSpace);
		PhysicsServer3D.BodySetSpace(chassis, otherSpace);
		yield return Wait.Frames(5);

		float z0 = w.Pos(chassis).Origin.Z;
		VehicleApi.SetControlInputs(v, 1f, 0f, 0f, 0f);
		yield return Wait.Frames(120);
		float dz = w.Pos(chassis).Origin.Z - z0;
		Assert.Expect(Mathf.Abs(dz) > 1.5f,
			$"vehicle still drives after the chassis moved spaces (|dz|={Mathf.Abs(dz):F2} m)");
		Assert.Expect(PhysxWorld.Finite(w.Pos(chassis)), "chassis state finite after space move");
	}
}
