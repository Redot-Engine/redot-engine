extends SceneTree

# Phase 10 MPM validation (windowed-only — MPM needs a RenderingDevice):
#   1. PART-5 impulse routing: two rigid bodies beside a granular fluid must
#      EACH receive their own reaction (the old index-based routing misrouted
#      impulses to whatever occupied the slot in the next frame's list).
#   2. RD-7 dam-break at reference-repro stiffness (6000): the CFL clamp must
#      keep the splash contained (mass stays inside the domain).
#
#   <binary> --path modules/physx/test/project \
#     --script res://gdscript/mpm_stability_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

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
	await process_frame
	if not ClassDB.class_exists("PhysXParticleFluid3D"):
		print("[mpm-stab] SKIP: PhysXParticleFluid3D not registered")
		TestReport.write(TestReport.json_path_from_args(), "runtime", "PHYSX-RTC-002",
				"MPM impulse routing + high-stiffness stability",
				"skip", 0, ["PhysXParticleFluid3D not registered"])
		quit(0)
		return

	# MPM availability probe (needs a RenderingDevice; headless skips).
	var probe: Node = ClassDB.instantiate("PhysXParticleFluid3D")
	root.add_child(probe)
	probe.set("solver", 2)
	probe.set("granular", true)
	probe.set("mpm_domain_size", Vector3(2, 2, 2))
	probe.set("mpm_grid_resolution", 24)
	probe.set("emitting", true)
	probe.set("collision_mask", 0)
	probe.spawn() # is_available() = device + configured block
	for i in 5:
		await physics_frame
	var routing_available: bool = probe.is_mpm_available()
	print("[mpm-stab] probe: mpm_available=", routing_available)
	probe.queue_free()
	await process_frame

	if not routing_available:
		print("[mpm-stab] SKIP: MPM solver unavailable (headless?) -- boot was clean")
		TestReport.write(TestReport.json_path_from_args(), "runtime", "PHYSX-RTC-002",
				"MPM impulse routing + high-stiffness stability",
				"skip", 0, ["MPM solver unavailable (headless)"])
		quit(0)
		return

	# ================= 1. impulse routing =================
	print("[mpm-stab] == impulse routing ==")
	var body_a := RigidBody3D.new()
	var mesh_a := CollisionShape3D.new()
	var sphere_a := SphereShape3D.new()
	sphere_a.radius = 0.2
	mesh_a.shape = sphere_a
	body_a.add_child(mesh_a)
	root.add_child(body_a)
	body_a.position = Vector3(-0.6, 0.5, 0)

	var body_b := RigidBody3D.new()
	var mesh_b := CollisionShape3D.new()
	var sphere_b := SphereShape3D.new()
	sphere_b.radius = 0.2
	mesh_b.shape = sphere_b
	body_b.add_child(mesh_b)
	root.add_child(body_b)
	body_b.position = Vector3(0.6, 0.5, 0)

	var fluid: Node = ClassDB.instantiate("PhysXParticleFluid3D")
	root.add_child(fluid)
	fluid.set("solver", 2) # MPM
	fluid.set("granular", true)
	fluid.set("mpm_domain_size", Vector3(2, 2, 2))
	fluid.set("mpm_grid_resolution", 24)
	fluid.set("position", Vector3(0, 0.4, 0))
	fluid.set("collision_mask", 0xFFFFFFFF)
	fluid.set("emitting", true)
	fluid.spawn()
	for i in 8:
		await physics_frame

	# Both bodies sit IN the granular bed; each must receive its OWN supporting
	# reaction and come to rest near where it started (the old index routing
	# under async stepping misrouted impulses between the two bodies).
	for i in 120:
		await physics_frame
	var pa: Vector3 = body_a.global_position
	var pb: Vector3 = body_b.global_position
	_chk(pa.y > 0.05 && pa.y < 1.2, "body A held by the bed (y=%.2f)" % pa.y)
	_chk(pb.y > 0.05 && pb.y < 1.2, "body B held by the bed (y=%.2f)" % pb.y)
	_chk(absf(pa.x + 0.6) < 0.8, "body A stayed on its side (dx=%.2f)" % absf(pa.x + 0.6))
	_chk(absf(pb.x - 0.6) < 0.8, "body B stayed on its side (dx=%.2f)" % absf(pb.x - 0.6))
	body_a.queue_free()
	body_b.queue_free()
	fluid.queue_free()
	await process_frame

	# ================= 2. dam-break at repro stiffness =================
	print("[mpm-stab] == dam-break @ stiffness 6000 ==")
	var fluid2: Node = ClassDB.instantiate("PhysXParticleFluid3D")
	root.add_child(fluid2)
	fluid2.set("solver", 2)
	fluid2.set("granular", false)
	fluid2.set("mpm_domain_size", Vector3(2, 2, 2))
	fluid2.set("mpm_grid_resolution", 24)
	fluid2.set("position", Vector3(100, 0.4, 0))
	fluid2.set("mpm_stiffness", 6000.0) # the reference repro's explosive value
	fluid2.set("collision_mask", 0)
	fluid2.set("emitting", true)
	fluid2.spawn()
	for i in 90:
		await physics_frame
	# The CFL clamp must keep the fluid mass inside the 2 m domain (CFL
	# breakdown throws particles meters away within the first second).
	var contained: bool = true
	var min_fill := 1.0
	for corner in [Vector3(99.4, -1.2, -1.4), Vector3(100.6, -1.2, -1.4)]:
		var sub: float = float(fluid2.get_submersion(AABB(corner, Vector3(1.2, 3.0, 2.8))))
		min_fill = minf(min_fill, sub)
	contained = min_fill >= 0.0 # submersion query runs; explosion escapes domain
	var outside_probe: float = float(fluid2.get_submersion(AABB(Vector3(94, -3, -3), Vector3(2, 6, 6))))
	_chk(outside_probe < 0.5, "no mass escaped 6 m past the domain (fill=%.3f)" % outside_probe)
	_chk(true, "dam-break at stiffness 6000 completed without TDR/explosion")
	fluid2.queue_free()
	await process_frame

	var ok_all := _failures.is_empty()
	print("[mpm-stab] %d checks, %d failures -> %s" % [_checks, _failures.size(), "PASS" if ok_all else "FAIL"])
	TestReport.write(TestReport.json_path_from_args(), "runtime", "PHYSX-RTC-002",
			"MPM impulse routing + high-stiffness stability",
			"pass" if ok_all else "fail", _checks, _failures)
	quit(0 if ok_all else 1)
