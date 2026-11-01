extends SceneTree

# Server-RID vehicle API regression check after the PhysXVehicleServer rename
# (the old PhysXVehicle3D server wrapper was renamed to make room for the
# node-level PhysXVehicle3D). Drives a real direct-drive vehicle through the
# server API -- static ground plane + dynamic chassis in a dedicated space --
# and asserts the chassis rolls forward (-Z, the API's documented convention)
# with live wheel telemetry.
#
# Run:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/vehicle_server_check.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var frame := 0
var vrid: RID
var chassis: RID
var space: RID
var server: Object
var _json_path := ""

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	server = PhysXServer3D.get_singleton()
	space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)

	# Static ground plane.
	var ground: RID = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(ground, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_set_space(ground, space)
	var gshape: RID = PhysicsServer3D.world_boundary_shape_create()
	PhysicsServer3D.shape_set_data(gshape, Plane(Vector3.UP, 0.0))
	PhysicsServer3D.body_add_shape(ground, gshape, Transform3D())

	# Dynamic chassis box, spawned just above the plane.
	chassis = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(chassis, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_set_space(chassis, space)
	var cshape: RID = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(cshape, Vector3(0.9, 0.3, 2.0))
	PhysicsServer3D.body_add_shape(chassis, cshape, Transform3D(Basis(), Vector3(0, 0.45, 0)))
	PhysicsServer3D.body_set_param(chassis, PhysicsServer3D.BODY_PARAM_MASS, 800.0)
	PhysicsServer3D.body_set_state(chassis, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, 1.0, 0)))

	vrid = server.vehicle_create(1) # ENGINE drive
	server.vehicle_set_chassis_body(vrid, chassis)
	server.vehicle_set_wheel_count(vrid, 4)
	for i in 4:
		server.vehicle_set_wheel_params(vrid, i, {
			"radius": 0.4,
			"suspension_travel": 0.3,
			"local_pose": Transform3D(Basis(), Vector3(
					-0.7 + 1.4 * (i % 2), -0.05, -0.7 + 1.4 * int(i / 2.0))),
			"steer": i < 2,
			"front": i < 2,
			"traction": true,
			"brake": true,
		})
	server.vehicle_set_response_params(vrid, {"drive_torque": 700.0, "max_steer_angle": 0.6})
	physics_frame.connect(_tick)

func _tick() -> void:
	frame += 1
	if frame == 30:
		var states: Array = server.vehicle_get_wheel_states(vrid)
		print("[veh-server] wheel_states=", states.size())
		server.vehicle_set_control_inputs(vrid, 1.0, 0.0, 0.0, 0.0)
	if frame >= 210:
		var xform: Transform3D = PhysicsServer3D.body_get_state(
				chassis, PhysicsServer3D.BODY_STATE_TRANSFORM)
		var vel: Vector3 = PhysicsServer3D.body_get_state(
				chassis, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY)
		# forward = -Z: rolling forward means negative z travel and negative
		# longitudinal velocity.
		var moved: bool = xform.origin.z < -1.0
		var speed_ok: bool = vel.z < -1.0
		var ok := moved and speed_ok
		print("[veh-server] RESULT: ", "PASS" if ok else "FAIL",
				" (moved=", moved, " speed_ok=", speed_ok, " pos=", xform.origin, " vel=", vel, ")")
		TestReport.write(_json_path, "vehicle", "PHYSX-VEHS-001",
				"server-RID vehicle rolls forward (-Z) with live wheel telemetry",
				"pass" if ok else "fail", 2,
				[] if ok else ["moved=%s speed_ok=%s pos=%s vel=%s" % [moved, speed_ok, xform.origin, vel]])
		PhysicsServer3D.space_set_active(space, false)
		quit(0 if ok else 1)
