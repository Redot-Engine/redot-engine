extends SceneTree

# Skinned-cloth gravity contracts (audit R-05, PHYSX-CLOTH-001).
#
#   PHYSX-CLOTH-001  PhysXSkinnedCloth3D's gravity resolution: the world's
#                   default gravity by default (project settings via the
#                   space's default area), fixed_gravity with
#                   use_world_gravity off, extra_acceleration added on top,
#                   and get_effective_gravity() reporting the resolved value.
#                   The solver consumes the same value (s.gravity) per step.
#
# Headless-safe: asserts the resolution, not the GPU simulation.

const TestReport := preload("res://gdscript/test_report.gd")

var _cloth: PhysXSkinnedCloth3D
var _json_path := ""
var _ran := false

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	_cloth = PhysXSkinnedCloth3D.new()
	root.add_child(_cloth)

func _physics_process(_d: float) -> bool:
	if _ran:
		return true
	_ran = true
	var messages: Array = []
	var ok := true

	var check := func(id: String, cond: bool, detail: String) -> void:
		if not cond:
			ok = false
			messages.append("%s: %s" % [id, detail])

	# World gravity: the project's default_gravity (9.81) down the default
	# vector, resolved through the space's default area.
	var g: Vector3 = _cloth.get_effective_gravity()
	check.call("world_gravity", g.is_equal_approx(Vector3(0, -9.81, 0)), "effective gravity is %s" % g)
	check.call("use_world_gravity_default", _cloth.get_use_world_gravity(), "default should be true")

	# Round-trips.
	_cloth.set_use_world_gravity(false)
	check.call("use_world_gravity_roundtrip", not _cloth.get_use_world_gravity(), "off round-trip")
	_cloth.set_fixed_gravity(Vector3(1, 2, 3))
	check.call("fixed_roundtrip", _cloth.get_fixed_gravity().is_equal_approx(Vector3(1, 2, 3)), "fixed round-trip")
	g = _cloth.get_effective_gravity()
	check.call("fixed_applied", g.is_equal_approx(Vector3(1, 2, 3)), "fixed_gravity stepped with: %s" % g)

	# extra_acceleration adds on top of either source.
	_cloth.set_extra_acceleration(Vector3(0, 5, 0))
	g = _cloth.get_effective_gravity()
	check.call("extra_on_fixed", g.is_equal_approx(Vector3(1, 7, 3)), "extra on fixed: %s" % g)
	_cloth.set_use_world_gravity(true)
	g = _cloth.get_effective_gravity()
	check.call("extra_on_world", g.is_equal_approx(Vector3(0, -4.81, 0)), "extra on world: %s" % g)
	check.call("extra_roundtrip", _cloth.get_extra_acceleration().is_equal_approx(Vector3(0, 5, 0)), "extra round-trip")

	TestReport.write(_json_path, "cloth", "PHYSX-CLOTH-001",
			"skinned cloth gravity resolution (world/fixed/extra + get_effective_gravity)",
			"pass" if ok else "fail", messages.size() + 1, messages)
	print("[skinnedcloth] PHYSX-CLOTH-001: %s%s" % ["PASS" if ok else "FAIL",
			("" if messages.is_empty() else " -- " + str(messages))])
	print("[skinnedcloth] %s" % ("PASS" if ok else "FAIL"))
	return true
