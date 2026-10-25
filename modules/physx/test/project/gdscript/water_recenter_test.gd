extends SceneTree

# Water in-place mutations (audit R-15, PHYSX-WATER-R-001).
#
#   recenter(world_position) moves the ripple square in place on a built
#   solver: the grid center follows the node, WITHOUT a rebuild (structural
#   invariant -- the in-place path never calls _rebuild(); with no solver,
#   headless, the fallback rebuilds and the node still moves). The waves'
#   phases are time-based, so the sea keeps its pattern across a recenter.
#   set_sea_state() updates the four spectrum properties and, on a built
#   solver, regenerates the spectrum in place (the individual sea-state
#   setters share the same path -- none of them rebuilds).
#
# Run (windowed; the solver needs a RenderingDevice -- headless skips):
#   <binary> --path modules/physx/test/project \
#     --script res://gdscript/water_recenter_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const SETTLE_FRAMES := 20

var surface: Node3D
var frame := 0
var _json_path := ""
var _failed := false
var _messages: Array = []
var _assertions := 0
var _recentered := false
var _sea_state_set := false
var _available := false

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		_failed = true
		_messages.append(msg)
		print("[water-recenter] FAIL: ", msg)
	else:
		print("[water-recenter] ok: ", msg)

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	if not ClassDB.class_exists("PhysXWaterSurface3D"):
		print("[water-recenter] SKIP: PhysXWaterSurface3D not registered")
		TestReport.write(_json_path, "water", "PHYSX-WATER-R-001",
				"water recenter + set_sea_state in-place", "skip", 0,
				["PhysXWaterSurface3D not registered in this build"])
		quit(0)
		return
	surface = ClassDB.instantiate("PhysXWaterSurface3D")
	root.add_child(surface)
	surface.set_position(Vector3(0, 0, 0))
	physics_frame.connect(_on_physics_frame)

func _on_physics_frame() -> void:
	frame += 1
	if frame == SETTLE_FRAMES:
		_available = surface.call("is_solver_available")
		print("[water-recenter] solver available: ", _available)

		# recenter: the node (and the grid with it) moves in XZ.
		var center_before: Vector2 = surface.call("get_grid_center")
		surface.call("recenter", Vector3(40, 0, -30))
		var center_after: Vector2 = surface.call("get_grid_center")
		if _available:
			# In-place path: the grid center is the node's new XZ.
			_check(center_after.is_equal_approx(Vector2(40, -30)),
					"recenter moved the grid center in place (%s)" % center_after)
		else:
			# Headless fallback: a rebuild re-derives the center from the node.
			_check(center_after.is_equal_approx(Vector2(40, -30)),
					"recenter (rebuild fallback) moved the grid center (%s)" % center_after)
		_check(surface.get_position().x == 40.0 and surface.get_position().z == -30.0,
				"recenter moved the node, keeping its height")
		_check(not center_before.is_equal_approx(center_after), "the grid actually moved")
		_recentered = true

	if frame == SETTLE_FRAMES + 10 and _recentered and not _sea_state_set:
		_sea_state_set = true
		# One in-place spectrum update: the four properties change together.
		surface.call("set_sea_state", 12.0, Vector2(0, 1), 2.0, 50000.0)
		_check(absf(surface.get("wind_speed") - 12.0) < 1e-4, "set_sea_state set wind_speed")
		_check(surface.get("wind_direction") == Vector2(0, 1), "set_sea_state set wind_direction")
		_check(absf(surface.get("wave_amplitude") - 2.0) < 1e-4, "set_sea_state set wave_amplitude")
		_check(absf(surface.get("fetch") - 50000.0) < 1e-4, "set_sea_state set fetch")
		# The solver stays available: no rebuild tore it down (an in-place
		# spectrum regen keeps every buffer bound). Headless: it was never
		# available, so only the windowed run can assert this.
		if _available:
			_check(surface.call("is_solver_available"), "solver still available after set_sea_state")
		# The grid center is untouched by a sea-state change.
		_check((surface.call("get_grid_center") as Vector2).is_equal_approx(Vector2(40, -30)),
				"sea-state change did not move the grid")

	if frame == SETTLE_FRAMES + 30:
		_finish()

func _finish() -> void:
	var ok := not _failed
	print("[water-recenter] ", "PASS" if ok else "FAIL")
	TestReport.write(_json_path, "water", "PHYSX-WATER-R-001",
			"water recenter + set_sea_state in-place",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
