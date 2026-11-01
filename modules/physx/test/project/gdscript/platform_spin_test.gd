extends SceneTree

# Phase-9 investigation (AUDIT R-22/F-22): contact velocities include the
# collider's spin term unconditionally (getVelocityAtPos = linear + wxr), so
# a character standing on a platform that has STOPPED rotating must not be
# carried by residual solver spin. PhysX zeroes velocities at sleep, which
# should make this benign -- this test decides port/no-port for the
# zero-spin-guard the reference added (4e6946bc56).
#
# Scenario: a dynamic box platform spins in place (linear axes locked), a
# CharacterBody3D stands on it off-axis, then the platform stops and sleeps.
# PASS = after the stop, the character's horizontal drift over 2 s stays
# below 10 cm while it keeps standing on the platform.
#
# Run headless:
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/platform_spin_test.gd -- --json=<path>

const SPIN_TICKS := 180          # 3 s of spinning
const MEASURE_TICKS := 120       # 2 s after the stop
const DRIFT_LIMIT_M := 0.1

var platform: RigidBody3D
var character: CharacterBody3D
var tick := 0
var measure_origin := Vector3.ZERO
var max_drift := 0.0
var measure_started := false
var was_sleeping_at_stop := false
var done := false
var json_path := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			json_path = arg.substr("--json=".length())
	print("[spin] physics/3d/physics_engine = ",
			ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	var floor_body := StaticBody3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(20, 1, 20)
	var floor_col := CollisionShape3D.new()
	floor_col.shape = floor_shape
	floor_body.add_child(floor_col)
	floor_body.position = Vector3(0, -0.5, 0)
	root.add_child(floor_body)

	platform = RigidBody3D.new()
	var platform_shape := BoxShape3D.new()
	platform_shape.size = Vector3(2, 0.4, 2)
	var platform_col := CollisionShape3D.new()
	platform_col.shape = platform_shape
	platform.add_child(platform_col)
	platform.mass = 20.0
	platform.can_sleep = true
	platform.axis_lock_linear_x = true
	platform.axis_lock_linear_y = true
	platform.axis_lock_linear_z = true
	platform.position = Vector3(0, 0.2, 0)
	root.add_child(platform)

	character = CharacterBody3D.new()
	var capsule := CapsuleShape3D.new()
	capsule.radius = 0.3
	capsule.height = 1.4
	var char_col := CollisionShape3D.new()
	char_col.shape = capsule
	character.add_child(char_col)
	# Off-axis: spin would carry the character along an arc if residual spin
	# leaked into the platform velocity.
	character.position = Vector3(0.6, 1.2, 0)
	root.add_child(character)

	platform.angular_velocity = Vector3(0, 3, 0)

func _physics_process(_delta: float) -> bool:
	tick += 1
	character.velocity = Vector3(0, -1, 0) # stay grounded, no input
	character.move_and_slide()

	if tick == SPIN_TICKS:
		platform.angular_velocity = Vector3.ZERO
		platform.sleeping = true # the stopped-and-at-rest end state
		was_sleeping_at_stop = platform.is_sleeping()
		measure_origin = character.global_position
		measure_started = true
		print("[spin] stop at tick %d; platform sleeping=%s char=%v" %
				[tick, was_sleeping_at_stop, measure_origin])
	elif measure_started and tick > SPIN_TICKS:
		var drift := (character.global_position - measure_origin) * Vector3(1, 0, 1)
		max_drift = maxf(max_drift, drift.length())
		if tick >= SPIN_TICKS + MEASURE_TICKS:
			_finish()
			return true
	return false

func _finish() -> void:
	var failures: Array = []
	if not was_sleeping_at_stop:
		failures.append("platform did not sleep at the stop")
	var still_on_top := absf(character.global_position.y - 1.2) < 0.5
	if not still_on_top:
		failures.append("character left the platform (y = %.3f)" % character.global_position.y)
	if max_drift >= DRIFT_LIMIT_M:
		failures.append("character carried after stop: drift %.3f m >= %.2f" % [max_drift, DRIFT_LIMIT_M])
	var ok := failures.is_empty()
	print("[spin] max_drift=%.4f m -> %s" % [max_drift, "PASS" if ok else "FAIL"])
	for f in failures:
		push_error("[spin] FAIL: " + f)
	_write_report(ok, failures)
	quit(0 if ok else 1)

func _write_report(ok: bool, failures: Array) -> void:
	if json_path.is_empty():
		return
	var result := {
		"suite": "platform_spin",
		"final": true,
		"engine": ProjectSettings.get_setting("physics/3d/physics_engine", "?"),
		"tests": [{
			"id": "PHYSX-SPIN-001",
			"category": "platform_spin",
			"description": "CharacterBody3D on a stopped rotating platform must not be carried (R-22 spin-velocity guard decision)",
			"status": "pass" if ok else "fail",
			"assertions": 3,
			"failures": failures.size(),
			"messages": failures,
		}],
	}
	var f := FileAccess.open(json_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
