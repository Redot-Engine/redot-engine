extends SceneTree

# Vehicle collision exceptions + chassis force API (audit R-06/R-13, PHYSX-VEH-001).
#
#   PHYSX-VEH-001  add_collision_exception_with makes a vehicle pass through
#                   the excepted body, wheels included: two identical cars
#                   settle on a shared platform; once one gets an exception
#                   against the platform its wheel raycasts and chassis
#                   contacts skip it and the car drops through to the ground,
#                   while the control car stays supported. Also pins the
#                   chassis force API: apply_force_at_local_position lifts a
#                   free-falling car against gravity and apply_torque spins it.
#
# Run:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/vehicle_exception_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var control: Node3D
var dropper: Node3D
var flyer: Node3D
var platform: Node
var wheels: Array = []
var frame := 0
var _json_path := ""
var _exception_added := false
var _control_rest_y := 0.0
var _flyer_yaw0 := 0.0
var _done := false

func _make_car(x: float, y: float) -> Node3D:
	var car: Node3D = ClassDB.instantiate("PhysXVehicle3D")
	root.add_child(car)
	car.set_position(Vector3(x, y, 0))
	car.set("mass", 1200.0)
	car.set("can_sleep", false)
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
		w.set("use_as_traction", false)
		car.add_child(w)
		wheels.append(w)
	return car

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	for cls in ["PhysXVehicle3D", "PhysXVehicleWheel3D"]:
		if not ClassDB.class_exists(cls):
			print("[veh-exc] SKIP: ", cls, " not registered")
			TestReport.write(_json_path, "vehicle", "PHYSX-VEH-001",
					"vehicle collision exceptions + chassis force API",
					"skip", 0, [cls + " not registered in this build"])
			quit(0)
			return

	# Ground far below (the excepted car lands here after dropping through).
	var ground: Node = ClassDB.instantiate("StaticBody3D")
	root.add_child(ground)
	var gs: Node = ClassDB.instantiate("CollisionShape3D")
	var gbox: Object = ClassDB.instantiate("BoxShape3D")
	gbox.set("size", Vector3(200, 1, 200))
	gs.set("shape", gbox)
	ground.add_child(gs)
	ground.set_position(Vector3(0, -1.5, 0))

	# Floating platform both cars spawn above (top at y=0).
	platform = ClassDB.instantiate("StaticBody3D")
	root.add_child(platform)
	var ps: Node = ClassDB.instantiate("CollisionShape3D")
	var pbox: Object = ClassDB.instantiate("BoxShape3D")
	pbox.set("size", Vector3(10, 1, 10))
	ps.set("shape", pbox)
	platform.add_child(ps)
	platform.set_position(Vector3(0, -0.5, 0))

	control = _make_car(0.0, 0.5)
	dropper = _make_car(4.0, 0.5)
	flyer = _make_car(-8.0, 8.0) # nothing under this x: free fall + forces
	physics_frame.connect(_on_physics_frame)

func _on_physics_frame() -> void:
	frame += 1
	# Flyer: net upward force (30 kN vs 11.8 kN weight) + yaw torque.
	if frame == 11:
		_flyer_yaw0 = flyer.get_global_transform().basis.get_euler().y
	if frame > 10 and frame <= 130:
		flyer.apply_force_at_local_position(Vector3(0, 30000, 0), Vector3())
		flyer.apply_torque(Vector3(0, 50000, 0))

	if frame == 90 and not _exception_added:
		_exception_added = true
		dropper.add_collision_exception_with(platform)
		var listed: Array = dropper.get_collision_exceptions()
		print("[veh-exc] exception list: ", listed)
	if frame >= 60 and frame <= 90:
		_control_rest_y = control.get_position().y

	if frame >= 240 and not _done:
		_done = true
		var control_y: float = control.get_position().y
		var dropper_y: float = dropper.get_position().y
		var flyer_y: float = flyer.get_position().y
		var yaw: float = flyer.get_global_transform().basis.get_euler().y
		var supported: bool = _control_rest_y > 0.25 and control_y > 0.25
		var dropped: bool = dropper_y < control_y - 0.8
		var lifted: bool = flyer_y > 8.5
		var spun: bool = absf(yaw - _flyer_yaw0) > 0.5
		var ok := supported and dropped and lifted and spun
		print("[veh-exc] RESULT: ", "PASS" if ok else "FAIL",
				" (supported=", supported, " control_y=", "%.2f" % control_y,
				" dropped=", dropped, " dropper_y=", "%.2f" % dropper_y,
				" lifted=", lifted, " flyer_y=", "%.2f" % flyer_y,
				" spun=", spun, " yaw=%.2f" % (yaw - _flyer_yaw0), ")")
		TestReport.write(_json_path, "vehicle", "PHYSX-VEH-001",
				"vehicle collision exceptions + chassis force API",
				"pass" if ok else "fail", 4,
				[] if ok else ["supported=%s dropped=%s lifted=%s spun=%s (cy=%.2f dy=%.2f fy=%.2f yaw=%.2f)" % [
						supported, dropped, lifted, spun, control_y, dropper_y, flyer_y, yaw]])
		quit(0 if ok else 1)
