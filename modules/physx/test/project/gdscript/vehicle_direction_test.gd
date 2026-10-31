extends SceneTree

# Vehicle direction-matrix test (Phase 7 / VEHN-2): drives the 2-wheel and
# tracked compositions headless through their probe bridges and asserts the
# RIGHT-HANDED frame contract — positive throttle / forward ratios must roll
# the vehicle -Z (nose-first, Godot convention) with positive forward speed.
# The old left-handed 2W/track frames produced mirrored drive compensated by
# negated command writes; this test pins the corrected semantics at the probe
# boundary (positive in, nose-first out).
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/vehicle_direction_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const GROUND_Y := 0.0

var _checks := 0
var _failures: Array = []

func _chk(ok: bool, msg: String) -> void:
	_checks += 1
	if ok:
		print("  PASS  ", msg)
	else:
		_failures.append(msg)
		print("  FAIL  ", msg)

func _initialize() -> void:
	_run()

func _run() -> void:
	if not ClassDB.class_exists("PhysXMotorcycleProbe") or not ClassDB.class_exists("PhysXTankProbe"):
		print("[veh-dir] SKIP: vehicle probes not registered")
		TestReport.write(TestReport.json_path_from_args(), "vehicle", "PHYSX-VEHD-001",
				"2W + tracked direction matrix on the right-handed frame",
				"skip", 0, ["probes not registered"])
		quit(0)
		return

	# --- shared world: floor plane + gravity ---
	var space := PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)
	PhysicsServer3D.space_set_param(space, PhysicsServer3D.SPACE_PARAM_BODY_LINEAR_VELOCITY_SLEEP_THRESHOLD, 0.0)

	var ground_shape := PhysicsServer3D.world_boundary_shape_create()
	PhysicsServer3D.shape_set_data(ground_shape, Plane(Vector3.UP, GROUND_Y))
	var ground := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(ground, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_set_space(ground, space)
	PhysicsServer3D.body_add_shape(ground, ground_shape)
	PhysicsServer3D.body_set_collision_layer(ground, 1)

	# ================= 2W (motorcycle composition) =================
	print("[veh-dir] == 2W ==")
	var bike: RefCounted = ClassDB.instantiate("PhysXMotorcycleProbe")
	var ok2: bool = bike.initialize(space, Vector3(0, 1.0, 0))
	_chk(ok2, "2W probe initialized")
	if ok2:
		# Drive with POSITIVE throttle for ~2.5 s of engine ticks, keeping the
		# bike upright with a script-side lean assist (counter-roll about the
		# forward axis -- the probe's documented script-side controller hook).
		# The engine steps the active space each physics frame; probe.step()
		# composes the vehicle2 sequence for the same tick.
		for i in 150:
			await physics_frame
			bike.step(1.0 / 60.0, 1.0, 0.0, 0.0)
			var av: Vector3 = bike.get_angular_velocity()
			var fwd: Vector3 = bike.get_forward()
			bike.apply_torque_impulse(fwd * (-av.dot(fwd) * 0.5))
		var pos2: Vector3 = bike.get_position()
		var spd2: float = bike.get_forward_speed()
		_chk(pos2.z < -1.0, "2W drives nose-first -Z (dz=%.2f)" % pos2.z)
		_chk(spd2 > 1.0, "2W forward speed positive on +throttle (%.2f)" % spd2)
	bike = null

	# ================= tracked (tank composition) =================
	print("[veh-dir] == track ==")
	var tank: RefCounted = ClassDB.instantiate("PhysXTankProbe")
	# 8 wheels, 2 tracks of 4, +/-1.4 m lateral, z from -1.5 (front) to +1.5.
	var wheel_pos := PackedVector3Array()
	for side in [-1.0, 1.0]:
		for r in range(4):
			wheel_pos.append(Vector3(side * 1.1, -0.35, -1.5 + r * 1.0))
	var okT: bool = tank.initialize(space, Vector3(20, 1.0, 0), wheel_pos)
	_chk(okT, "track probe initialized")
	if okT:
		for i in 150:
			await physics_frame
			tank.step(1.0 / 60.0, 1.0, 1.0, 0.0)  # both tracks forward
		var posT: Vector3 = tank.get_position()
		var spdT: float = tank.get_forward_speed()
		_chk(posT.z < -1.0, "track drives nose-first -Z (dz=%.2f)" % posT.z)
		_chk(spdT > 1.0, "track forward speed positive on +ratios (%.2f)" % spdT)
	tank = null

	PhysicsServer3D.free_rid(ground)
	PhysicsServer3D.free_rid(ground_shape)
	PhysicsServer3D.free_rid(space)

	var ok := _failures.is_empty()
	print("[veh-dir] %d checks, %d failures -> %s" % [_checks, _failures.size(), "PASS" if ok else "FAIL"])
	TestReport.write(TestReport.json_path_from_args(), "vehicle", "PHYSX-VEHD-001",
			"2W + tracked direction matrix on the right-handed frame",
			"pass" if ok else "fail", _checks, _failures)
	quit(0 if ok else 1)
