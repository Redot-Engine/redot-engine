extends SceneTree

# Joint motor wake contracts (audit R-03, PHYSX-JMW-001..003).
#
#   PHYSX-JMW-001  a hinge motor driving a heavy arm from a SLEEPING start
#                   spins it up to the target velocity: the body pre-step wakes
#                   motor-driven bodies every step (PhysX drives do not wake a
#                   sleeping body, so without the awake-keep the arm never
#                   moves and this times out).
#   PHYSX-JMW-002  switching the motor OFF releases the drive: nothing keeps
#                   the body awake anymore, damping stops it and it falls
#                   asleep again.
#   PHYSX-JMW-003  a motor flag/param change on a sleeping pair wakes it
#                   (drive setters autowake) and the motor starts turning it.
#
# Run headless:
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/joint_motor_wake_test.gd -- --json=<path>

const PHASE_TIMEOUT := 300
const SPIN_VERIFY_TICKS := 150
const SLEEP_VERIFY_TICKS := 240
const WAKE_VERIFY_TICKS := 90

enum {
	P_J001_SETUP,
	P_J001_VERIFY,
	P_J002_RELEASE,
	P_J003_WAKE_ON_PARAM,
	P_DONE,
}

var _phase := P_J001_SETUP
var _ticks := 0
var _was_awake_since_param := false
var _space: RID
var _anchor: RID
var _anchor_shape: RID
var _arm: RID
var _arm_shape: RID
var _joint: RID
var _json_path := ""

func _initialize() -> void:
	_json_path = _json_path_from_args()
	_space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(_space, true)

func _json_path_from_args() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			return arg.substr("--json=".length())
	return ""

func _make_sphere_body(mode: int, pos: Vector3, radius: float, mass: float) -> Array:
	var shape := PhysicsServer3D.sphere_shape_create()
	PhysicsServer3D.shape_set_data(shape, radius)
	var body := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(body, mode)
	PhysicsServer3D.body_set_space(body, _space)
	PhysicsServer3D.body_add_shape(body, shape)
	PhysicsServer3D.body_set_param(body, PhysicsServer3D.BODY_PARAM_MASS, mass)
	# Cancel world gravity: the motor test reads angular velocity, not sag.
	PhysicsServer3D.body_set_param(body, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
	PhysicsServer3D.body_set_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), pos))
	return [body, shape]

func _setup_hinge() -> void:
	var anchor_parts := _make_sphere_body(PhysicsServer3D.BODY_MODE_STATIC, Vector3.ZERO, 0.4, 1.0)
	_anchor = anchor_parts[0]
	_anchor_shape = anchor_parts[1]
	var arm_parts := _make_sphere_body(PhysicsServer3D.BODY_MODE_RIGID, Vector3(0, 1, 0), 0.5, 50.0)
	_arm = arm_parts[0]
	_arm_shape = arm_parts[1]
	_joint = PhysicsServer3D.joint_create()
	# Both hinge frames land at the world origin (arm-local offset -1 y), X
	# axis: the arm swings in the Y-Z plane around the world X axis.
	var frame_b := Transform3D(Basis(), Vector3(0, -1, 0))
	PhysicsServer3D.joint_make_hinge(_joint, _anchor, Transform3D(), _arm, frame_b)
	PhysicsServer3D.hinge_joint_set_param(_joint, PhysicsServer3D.HINGE_JOINT_MOTOR_TARGET_VELOCITY, 3.0)
	PhysicsServer3D.hinge_joint_set_param(_joint, PhysicsServer3D.HINGE_JOINT_MOTOR_MAX_IMPULSE, 2.0)
	PhysicsServer3D.hinge_joint_set_flag(_joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)

func _arm_state() -> Array:
	var sleeping: bool = PhysicsServer3D.body_get_state(_arm, PhysicsServer3D.BODY_STATE_SLEEPING)
	var omega: Vector3 = PhysicsServer3D.body_get_state(_arm, PhysicsServer3D.BODY_STATE_ANGULAR_VELOCITY)
	return [sleeping, omega.length()]

var _recorded_pass := {}

func _record(id: String, ok: bool, messages: Array) -> void:
	_recorded_pass[id] = ok
	print("[jointwake] %s: %s%s" % [id, "PASS" if ok else "FAIL",
			("" if messages.is_empty() else " -- " + str(messages))])

func _enter(phase: int) -> void:
	_phase = phase
	_ticks = 0

func _physics_process(_delta: float) -> bool:
	_ticks += 1
	match _phase:
		P_J001_SETUP:
			_setup_hinge()
			# Start asleep: the drive must not be the thing that wakes it
			# (it cannot be) -- the pre-step awake-keep is.
			PhysicsServer3D.body_set_state(_arm, PhysicsServer3D.BODY_STATE_SLEEPING, true)
			_enter(P_J001_VERIFY)
		P_J001_VERIFY:
			if _ticks >= SPIN_VERIFY_TICKS:
				var state := _arm_state()
				var sleeping: bool = state[0]
				var omega: float = state[1]
				var ok := not sleeping and omega > 1.5
				_record("PHYSX-JMW-001", ok, [] if ok else [
						"motor-driven arm did not spin up from sleep: sleeping=%s omega=%.3f" % [sleeping, omega]])
				# High damping so the released arm decays under the sleep
				# threshold within the window (a 50 kg arm with mild damping
				# coasts for seconds).
				PhysicsServer3D.body_set_param(_arm, PhysicsServer3D.BODY_PARAM_ANGULAR_DAMP, 30.0)
				PhysicsServer3D.hinge_joint_set_flag(_joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, false)
				_enter(P_J002_RELEASE)
		P_J002_RELEASE:
			var state := _arm_state()
			if state[0]:
				_record("PHYSX-JMW-002", true, [])
				_enter(P_J003_WAKE_ON_PARAM)
			elif _ticks > SLEEP_VERIFY_TICKS:
				_record("PHYSX-JMW-002", false, [
						"arm never fell asleep after motor-off (residual drive or wake-keep): omega=%.3f" % state[1]])
				_enter(P_J003_WAKE_ON_PARAM)
		P_J003_WAKE_ON_PARAM:
			if _ticks == 1:
				# Motor currently disabled and the arm asleep (J002). A flag
				# change must autowake the sleeping pair and start the motor.
				_was_awake_since_param = false
				PhysicsServer3D.hinge_joint_set_flag(_joint, PhysicsServer3D.HINGE_JOINT_FLAG_ENABLE_MOTOR, true)
			var state := _arm_state()
			if not state[0]:
				_was_awake_since_param = true
			if _was_awake_since_param and state[1] > 0.2:
				_record("PHYSX-JMW-003", true, [])
				_enter(P_DONE)
			elif _ticks > WAKE_VERIFY_TICKS:
				_record("PHYSX-JMW-003", false, [
						"arm stayed asleep or never spun after the motor was re-enabled: sleeping=%s omega=%.3f" % [state[0], state[1]]])
				_enter(P_DONE)
		P_DONE:
			_finish()
			return true
	return false

func _finish() -> void:
	var tests: Array = []
	for id in ["PHYSX-JMW-001", "PHYSX-JMW-002", "PHYSX-JMW-003"]:
		tests.append({
			"id": id,
			"category": "joints",
			"description": id,
			"status": "pass",
			"assertions": 1,
			"failures": 0,
			"messages": [],
		})
	var ok := true
	# Re-derive statuses from the printed pass/fail records.
	for t in tests:
		if not _recorded_pass.get(t["id"], false):
			t["status"] = "fail"
			t["failures"] = 1
			t["messages"] = ["see test output"]
			ok = false
	var result := {
		"suite": "joints",
		"final": true,
		"tests": tests,
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE) if not _json_path.is_empty() else null
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	elif not _json_path.is_empty():
		push_error("[jointwake] could not write JSON report to %s" % _json_path)
	print("[jointwake] %s" % ("PASS (all 3 contracts)" if ok else "FAIL"))
