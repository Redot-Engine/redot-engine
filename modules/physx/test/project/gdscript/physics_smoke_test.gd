extends SceneTree

# Node-level physics smoke test for the PhysX module.
#
# Spawns a RigidBody3D (box) above a StaticBody3D floor as real nodes, lets the
# engine drive physics, and asserts the body falls and then settles resting on
# the floor. Run headless:
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/physics_smoke_test.gd -- --json=<path>
#
# Writes a suite-style JSON report when --json is given. Exit 0 = pass, 1 = fail.

const MAX_TICKS := 600           # 10 s at 60 Hz
const SETTLE_EPS := 0.01
const FLOOR_TOP_Y := 0.0
const BOX_HALF := 0.5
const START_Y := 5.0

var _box: RigidBody3D
var _ticks := 0
var _y_start := START_Y
var _y_prev := START_Y
var _min_y := START_Y
var _settled_at := -1
var _done := false
var _json_path := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			_json_path = arg.substr("--json=".length())
	print("[smoke] physics/3d/physics_engine = ",
		ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	var root := Node3D.new()
	root.name = "SmokeRoot"

	var floor_body := StaticBody3D.new()
	var floor_col := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(10, 1, 10)
	floor_col.shape = floor_shape
	floor_body.add_child(floor_col)
	floor_body.position = Vector3(0, FLOOR_TOP_Y - 0.5, 0)
	root.add_child(floor_body)

	_box = RigidBody3D.new()
	var box_col := CollisionShape3D.new()
	var box_shape := BoxShape3D.new()
	box_shape.size = Vector3(BOX_HALF * 2, BOX_HALF * 2, BOX_HALF * 2)
	box_col.shape = box_shape
	_box.add_child(box_col)
	_box.position = Vector3(0, START_Y, 0)
	root.add_child(_box)

	get_root().add_child(root)
	_y_start = START_Y
	_y_prev = _y_start
	_min_y = _y_start

func _physics_process(_delta: float) -> bool:
	if _done:
		return true
	var y := _box.global_position.y
	_min_y = minf(_min_y, y)
	_ticks += 1
	if _ticks <= 5 or _ticks % 60 == 0:
		print("[smoke] tick %d  y=%.3f" % [_ticks, y])
	if _ticks > 2 and absf(y - _y_prev) < SETTLE_EPS and y < _y_start - 0.5:
		_settled_at = _ticks
		_finish()
		return true
	if _ticks >= MAX_TICKS:
		_finish()
		return true
	_y_prev = y
	return false

func _finish() -> void:
	_done = true
	var y_end := _box.global_position.y
	var expected_rest := FLOOR_TOP_Y + BOX_HALF
	print("[smoke] y: start=%.3f min=%.3f end=%.3f  settled_tick=%d  expected_rest~%.3f  ticks=%d" %
		[_y_start, _min_y, y_end, _settled_at, expected_rest, _ticks])

	var failures: Array = []
	if not (y_end < _y_start - 1.0):
		failures.append("box did not fall")
		push_error("[smoke] FAIL: box did not fall")
	if _settled_at < 0:
		failures.append("box never settled within %d ticks" % MAX_TICKS)
		push_error("[smoke] FAIL: box never settled within %d ticks" % MAX_TICKS)
	elif absf(y_end - expected_rest) > 0.2:
		failures.append("rest height off (got %.3f want ~%.3f)" % [y_end, expected_rest])
		push_error("[smoke] FAIL: rest height off (got %.3f want ~%.3f)" % [y_end, expected_rest])

	var ok := failures.is_empty()
	print("[smoke] ", "PASS" if ok else "FAIL")
	_write_report(ok, failures)
	quit(0 if ok else 1)

func _write_report(ok: bool, failures: Array) -> void:
	if _json_path.is_empty():
		return
	var result := {
		"suite": "smoke",
		"final": true,
		"engine": ProjectSettings.get_setting("physics/3d/physics_engine", "?"),
		"tests": [{
			"id": "PHYSX-SMOKE-001",
			"category": "smoke",
			"description": "node-level drop: box falls from 5 m and settles on the floor",
			"status": "pass" if ok else "fail",
			"assertions": 3,
			"failures": failures.size(),
			"messages": failures,
		}],
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	else:
		push_error("[smoke] could not write JSON report to %s" % _json_path)
