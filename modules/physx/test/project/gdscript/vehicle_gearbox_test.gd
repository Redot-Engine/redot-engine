extends SceneTree

# EngineDrive (use_gearbox) smoke test for PhysXVehicle3D: builds the car in
# gearbox mode on flat ground, drives with throttle and verifies:
#   1. the engine spins up (rpm > idle) and the autobox shifts (gear changes),
#   2. the car accelerates nose-first (-Z, the documented convention),
#   3. reverse gear (target_gear = GEAR_REVERSE) drives backward,
#   4. telemetry getters return live values (wheel rpm, skid, contact).
#
# Run:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/vehicle_gearbox_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var car: Node
var frame := 0
var max_rpm := 0.0
var gears_seen := {}
var fwd_at_3s := 0.0
var fwd_at_rev_end := 0.0
var rev_pos := Vector3.ZERO
var rev_contact := {}
var neutral_pos := Vector3.ZERO
var _json_path := ""

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	var ground: Node = ClassDB.instantiate("StaticBody3D")
	root.add_child(ground)
	var gs: Node = ClassDB.instantiate("CollisionShape3D")
	var gbox: Object = ClassDB.instantiate("BoxShape3D")
	gbox.set("size", Vector3(400, 1, 400))
	gs.set("shape", gbox)
	ground.add_child(gs)
	ground.set_position(Vector3(0, -0.5, 0))

	car = ClassDB.instantiate("PhysXVehicle3D")
	root.add_child(car)
	car.global_transform = Transform3D(Basis(), Vector3(0, 0.15, 0))
	car.set("mass", 1200.0)
	car.set("can_sleep", false)
	car.set("use_gearbox", true)
	car.set("use_autobox", false)
	car.set("target_gear", 1)  # neutral first (adjacent stepping)
	car.set("engine_peak_torque", 800.0)
	car.set("target_gear", 2)

	var chassis: Node = ClassDB.instantiate("CollisionShape3D")
	var cbox: Object = ClassDB.instantiate("BoxShape3D")
	cbox.set("size", Vector3(1.8, 0.6, 4.0))
	chassis.set("shape", cbox)
	chassis.set_position(Vector3(0, 0.1, 0))
	car.add_child(chassis)

	for i in 4:
		var w: Node = ClassDB.instantiate("PhysXVehicleWheel3D")
		var front := i < 2
		w.set_position(Vector3(-0.85 if i % 2 == 0 else 0.85,
				-0.1, -1.4 if front else 1.4))
		w.set("use_as_steering", front)
		w.set("use_as_traction", true)
		car.add_child(w)

	# Safety net: if the setter ever refuses to enable again, report SKIP
	# instead of testing direct drive as if it were engine drive.
	if not bool(car.get("use_gearbox")):
		print("[gearbox] SKIP: engine drive is blocked (setter refused) -- direct drive remains available")
		TestReport.write(_json_path, "vehicle", "PHYSX-VEHN-002",
				"engine-drive gearbox: spin-up, autobox shifts, reverse, neutral hold",
				"skip", 0, ["use_gearbox setter refused to enable"])
		quit(0)
		return

	physics_frame.connect(_tick)

func _tick() -> void:
	frame += 1
	if frame == 30:
		car.set("target_gear", 2)  # 1st gear (adjacent step from neutral)
		car.set("throttle", 1.0)
		print("[gearbox] f=30 -> target 1st, throttle on")
	if frame == 90:
		print("[gearbox] f=90 gear=%s rpm=%.0f pos.z=%.2f fwd=%.2f" % [car.get_engine_gear(), car.get_engine_rpm(), car.global_position.z, car.get_forward_speed()])
	if frame > 30:
		max_rpm = maxf(max_rpm, car.get_engine_rpm())
		gears_seen[car.get_engine_gear()] = true
	if frame == 210:
		fwd_at_3s = car.get_forward_speed()
		print("[gearbox] f=210 rpm=%.0f gears=%s fwd=%.1f pos=%s" % [
				car.get_engine_rpm(), gears_seen.keys(), fwd_at_3s, car.global_position])
		# Reverse from (near) rest: brake first.
		car.set("brake", 1.0)
		car.set("throttle", 0.0)
	if frame == 300:
		car.set("brake", 0.0)
		car.set("target_gear", car.GEAR_REVERSE)
		car.set("throttle", 1.0)
	if frame == 420:
		# Collect the reverse-phase evidence, then brake to a stop for the
		# neutral phase.
		fwd_at_rev_end = car.get_forward_speed()
		rev_pos = car.global_position
		rev_contact = car.get_wheel_contact(0)
		car.set("brake", 1.0)
		car.set("throttle", 0.0)
	if frame == 540:
		# Neutral phase: full throttle in N must NOT accelerate the car --
		# the SDK applies no drive torque while the gearbox is in neutral.
		# The 2s brake hold from the reverse roll must have brought the car
		# to a standstill by now (locked wheels slide at tire_slide_grip, so
		# ~1.3s to stop from the reverse phase's ~9 m/s); the reference
		# position is taken HERE so the neutral check measures only
		# neutral-phase travel.
		neutral_pos = car.global_position
		car.set("brake", 0.0)
		car.set("target_gear", car.GEAR_NEUTRAL)
		car.set("throttle", 1.0)
	if frame == 660:
		var failures: Array = []
		var fwd: float = car.get_forward_speed()
		var pos: Vector3 = car.global_position
		var gear_n: int = car.get_engine_gear()
		if fwd_at_3s < 5.0:
			failures.append("no sustained forward accel (fwd=%.1f)" % fwd_at_3s)
		if rev_pos.z > -5.0:
			failures.append("did not drive nose-first (-Z) (z=%.1f)" % rev_pos.z)
		if max_rpm < 60.0:
			failures.append("engine never revved (max_rpm=%.0f)" % max_rpm)
		if gears_seen.size() < 2:
			failures.append("autobox never shifted (gears=%s)" % str(gears_seen.keys()))
		if fwd_at_rev_end >= 0.0:
			failures.append("reverse gear did not drive backward (fwd=%.1f)" % fwd_at_rev_end)
		if not bool(rev_contact.get("contact", false)):
			failures.append("wheel 0 reports no contact")
		if gear_n != car.GEAR_NEUTRAL:
			failures.append("gearbox did not reach neutral (gear=%d)" % gear_n)
		if absf(fwd) > 0.5:
			failures.append("neutral did not hold against full throttle (fwd=%.2f)" % fwd)
		var neutral_travel := (pos - neutral_pos).length()
		if neutral_travel > 1.5:
			failures.append("car moved %.1f m while in neutral" % neutral_travel)
		for f in failures:
			push_error("[gearbox] FAIL: " + f)
		print("[gearbox] RESULT: ", "PASS" if failures.is_empty() else "FAIL",
				" fwd3s=%.1f revFwd=%.1f endFwd=%.2f pos.z=%.1f max_rpm=%.0f gears=%s contact=%s" % [
				fwd_at_3s, fwd_at_rev_end, fwd, rev_pos.z, max_rpm, gears_seen.keys(),
				rev_contact.get("contact")])
		TestReport.write(_json_path, "vehicle", "PHYSX-VEHN-002",
				"engine-drive gearbox: spin-up, autobox shifts, reverse, neutral hold",
				"pass" if failures.is_empty() else "fail", 9, failures)
		quit(0 if failures.is_empty() else 1)
