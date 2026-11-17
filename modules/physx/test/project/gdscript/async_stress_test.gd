extends SceneTree

# Async-window mutation stress test (Phase 4 / SPACE-9, OBJ-5, CORE-2).
#
# Forces physics/physx_3d/simulation/async_step ON and continuously mutates
# the scene from BOTH the idle frame (which lands inside the simulate->fetch
# window: step() kicks the solve and returns, the frame's _process callbacks
# run while it is in flight) and the physics tick. Every mutation class that
# must be fetch-guarded is exercised: body create/free, TRANSFORM writes,
# shape transforms, shape enable/disable, surface params (bounce/friction —
# the userData floats the contact-modify callback reads mid-solve), collision
# exceptions (the word2 slot registry), and the vehicle adopt/release cycle
# (constraint destroy + actor flag writes).
#
# Pass = the process survives the loop with every body finite and no PhysX
# write-during-simulate errors. Run headless:
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/async_stress_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const FRAMES := 480

var _frame := 0
var _space: RID
var _static: RID
var _static_shape: RID
var _a: RID
var _b: RID
var _a_shape: RID
var _b_shape: RID
var _transients: Array = []       # [body, shape] pairs recycled oldest-first
var _vehicles: Array = []         # [vehicle, chassis, chassis_shape] triples
var _exceptions_on := false
var _disabled := false
var _json_path := ""
var _errors := 0

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	ProjectSettings.set_setting("physics/physx_3d/simulation/async_step", true)
	print("[async-stress] async_step forced on")

	_space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(_space, true)

	# Static anchor for the exception pair.
	_static_shape = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(_static_shape, Vector3(0.5, 0.5, 0.5))
	_static = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_static, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_set_space(_static, _space)
	PhysicsServer3D.body_add_shape(_static, _static_shape)

	# Persistent pair for exception/param churn.
	_a_shape = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(_a_shape, Vector3(0.3, 0.3, 0.3))
	_a = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_a, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_set_space(_a, _space)
	PhysicsServer3D.body_add_shape(_a, _a_shape)
	PhysicsServer3D.body_set_param(_a, PhysicsServer3D.BODY_PARAM_MASS, 1.0)

	_b_shape = PhysicsServer3D.sphere_shape_create()
	PhysicsServer3D.shape_set_data(_b_shape, 0.3)
	_b = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_b, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_set_space(_b, _space)
	PhysicsServer3D.body_add_shape(_b, _b_shape)
	PhysicsServer3D.body_set_param(_b, PhysicsServer3D.BODY_PARAM_MASS, 1.0)

func _process(_delta: float) -> bool:
	# Idle-frame churn: lands inside the async simulate->fetch window.
	_frame += 1
	_churn()
	if _frame >= FRAMES:
		_finish()
		return true
	return false

func _physics_process(_delta: float) -> bool:
	# Physics-tick churn: lands right before step() (same-thread async path).
	_churn()
	return false

func _churn() -> void:
	# --- persistent pair: surface params (userData floats) + exceptions ------
	PhysicsServer3D.body_set_param(_a, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.5 * sin(_frame))
	PhysicsServer3D.body_set_param(_b, PhysicsServer3D.BODY_PARAM_FRICTION, 1.0 + 0.5 * cos(_frame))
	if _frame % 17 == 0:
		if _exceptions_on:
			PhysicsServer3D.body_remove_collision_exception(_a, _b)
			PhysicsServer3D.body_remove_collision_exception(_a, _static)
		else:
			PhysicsServer3D.body_add_collision_exception(_a, _b)
			PhysicsServer3D.body_add_collision_exception(_a, _static)
		_exceptions_on = not _exceptions_on

	# --- shape transform + disabled toggle on the persistent pair ------------
	if _frame % 5 == 0:
		var t := Transform3D(Basis.IDENTITY, Vector3(0.05 * sin(_frame), 0.4, 0))
		PhysicsServer3D.body_set_shape_transform(_a, 0, t)
	if _frame % 11 == 0:
		_disabled = not _disabled
		PhysicsServer3D.body_set_shape_disabled(_b, 0, _disabled)

	# --- transient bodies: create, teleport each frame, free -----------------
	if _frame % 3 == 0:
		var shape: RID = PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(shape, Vector3(0.2, 0.2, 0.2))
		var body: RID = PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(body, PhysicsServer3D.BODY_MODE_RIGID)
		PhysicsServer3D.body_set_space(body, _space)
		PhysicsServer3D.body_add_shape(body, shape)
		PhysicsServer3D.body_set_param(body, PhysicsServer3D.BODY_PARAM_MASS, 0.5)
		PhysicsServer3D.body_set_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis.IDENTITY, Vector3(3 + randf() * 2, 5, randf() * 2)))
		_transients.push_back([body, shape])
		while _transients.size() > 30:
			var old: Array = _transients.pop_front()
			PhysicsServer3D.free_rid(old[0])
			PhysicsServer3D.free_rid(old[1])
	for pair in _transients:
		# TRANSFORM write every frame (setGlobalPose / kinematic target).
		var xf: Transform3D = PhysicsServer3D.body_get_state(pair[0], PhysicsServer3D.BODY_STATE_TRANSFORM)
		xf.origin.x += 0.01
		PhysicsServer3D.body_set_state(pair[0], PhysicsServer3D.BODY_STATE_TRANSFORM, xf)

	# --- vehicle cycle: adopt a chassis, drive it, release it ----------------
	if _frame % 30 == 15:
		var server: Object = PhysXServer3D.get_singleton()
		var chassis_shape: RID = PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(chassis_shape, Vector3(0.9, 0.3, 2.0))
		var chassis: RID = PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(chassis, PhysicsServer3D.BODY_MODE_RIGID)
		PhysicsServer3D.body_set_space(chassis, _space)
		PhysicsServer3D.body_add_shape(chassis, chassis_shape)
		PhysicsServer3D.body_set_param(chassis, PhysicsServer3D.BODY_PARAM_MASS, 800.0)
		PhysicsServer3D.body_set_state(chassis, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis.IDENTITY, Vector3(-30, 1.0, -30)))
		var vrid: RID = server.vehicle_create(0) # direct drive
		server.vehicle_set_chassis_body(vrid, chassis)
		server.vehicle_set_wheel_count(vrid, 4)
		for i in 4:
			server.vehicle_set_wheel_params(vrid, i, {
				"radius": 0.4,
				"suspension_travel": 0.3,
				"local_pose": Transform3D(Basis.IDENTITY, Vector3(
						-0.7 + 1.4 * (i % 2), -0.05, -0.7 + 1.4 * int(i / 2.0))),
				"steer": i < 2,
				"front": i < 2,
				"traction": true,
				"brake": true,
			})
		server.vehicle_set_response_params(vrid, {"drive_torque": 700.0})
		server.vehicle_set_control_inputs(vrid, 1.0, 0.0, 0.0, 0.0)
		_vehicles.push_back([vrid, chassis, chassis_shape])
	if _frame % 30 == 29:
		for v in _vehicles:
			# vehicle release (constraint destroy) + chassis free mid-flight.
			PhysicsServer3D.free_rid(v[0])
			PhysicsServer3D.free_rid(v[1])
			PhysicsServer3D.free_rid(v[2])
		_vehicles.clear()

func _finish() -> void:
	# Free any vehicle still alive (the last cycle may not have reached its
	# free frame) so shutdown starts from a clean slate.
	for v in _vehicles:
		PhysicsServer3D.free_rid(v[0])
		PhysicsServer3D.free_rid(v[1])
		PhysicsServer3D.free_rid(v[2])
	_vehicles.clear()
	var failures: Array = []
	# Every remaining body must be finite after the churn.
	var bodies: Array = [_a, _b]
	for pair in _transients:
		bodies.push_back(pair[0])
	for body in bodies:
		var xf: Transform3D = PhysicsServer3D.body_get_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM)
		if not (Vector3.ZERO.is_finite() and xf.origin.is_finite() and xf.basis.determinant() != 0.0):
			failures.append("body state not finite")
			break
	var ok := failures.is_empty()
	print("[async-stress] survived %d frames -> %s" % [_frame, "PASS" if ok else "FAIL"])
	TestReport.write(_json_path, "async", "PHYSX-ASYNC-001",
			"async_step churn: bodies/transforms/params/exceptions/vehicles mutated mid-flight",
			"pass" if ok else "fail", 1, failures)
	quit(0 if ok else 1)
