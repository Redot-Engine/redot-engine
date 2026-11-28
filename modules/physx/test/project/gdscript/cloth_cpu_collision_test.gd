extends SceneTree

# Behavioral regression test for the CPU cloth collision path.
#
# The GPU deformable-surface path is exercised by gpu_smoke_test.gd, but the
# PhysXCloth3D CPU fallback (XPBD solver + per-vertex rest_info depenetration)
# had NO behavioral coverage: rest_info's penetrating-query fallback used to
# report a fabricated surface point on the query's side of the vertex, the
# sd = (pos - point) . normal plane test never saw a penetration, and CPU
# cloths fell straight through the world (through static bodies and floors
# alike). GPU-capable CI never noticed because the default SIM_AUTO path takes
# the GPU deformable surface there.
#
# This test forces simulation_mode = SIM_CPU and drops a free sheet onto a
# static ball standing on a floor. The sheet must drape: catch on the ball
# (AABB top stays up at ball height) with its edges reaching the floor
# (AABB bottom near ground level) and come to rest. Run headless:
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/cloth_cpu_collision_test.gd -- --json=<path>
#
# Exit 0 = pass, 1 = fail.

const MAX_TICKS := 480            # 8 s at 60 Hz
const DRAPE_TICK := 180           # 3 s: the sheet has caught and is draped
const REST_TICK := 420            # 7 s: settled wherever it slid to
const START_Y := 3.0
const BALL_CENTER := Vector3(0, 1.1, 0)
const BALL_RADIUS := 0.9

var _cloth: PhysXCloth3D
var _ball: StaticBody3D
var _ticks := 0
var _drape_aabb := AABB()
var _json_path := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			_json_path = arg.substr("--json=".length())
	print("[clothcpu] physics/3d/physics_engine = ",
			ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	var root := Node3D.new()
	root.name = "ClothCpuRoot"

	var floor_body := StaticBody3D.new()
	var floor_col := CollisionShape3D.new()
	var floor_shape := BoxShape3D.new()
	floor_shape.size = Vector3(12, 1, 12)
	floor_col.shape = floor_shape
	floor_body.add_child(floor_col)
	floor_body.position = Vector3(0, -0.5, 0)
	root.add_child(floor_body)

	_ball = StaticBody3D.new()
	var ball_col := CollisionShape3D.new()
	var ball_shape := SphereShape3D.new()
	ball_shape.radius = BALL_RADIUS
	ball_col.shape = ball_shape
	_ball.add_child(ball_col)
	_ball.position = BALL_CENTER
	root.add_child(_ball)

	_cloth = PhysXCloth3D.new()
	_cloth.simulation_mode = PhysXCloth3D.SIM_CPU # force the XPBD CPU path
	_cloth.grid_columns = 32
	_cloth.grid_rows = 32
	_cloth.grid_size = Vector2(3, 3)
	_cloth.pin_mode = PhysXCloth3D.PIN_NONE # free sheet: falls onto the ball
	_cloth.wind_enabled = false
	_cloth.rotation_degrees = Vector3(-90, 0, 0) # lay flat, facing down
	_cloth.position = Vector3(0, START_Y, 0)
	root.add_child(_cloth)

	get_root().add_child(root)

func _physics_process(_delta: float) -> bool:
	_ticks += 1
	if _ticks == DRAPE_TICK:
		# Snapshot the catch window: a free sheet drapes over the ball a couple
		# of seconds after landing and only later slides off the smooth sphere
		# to pool on the floor, so the drape assertion is time-boxed to here.
		_drape_aabb = _cloth.global_transform * _cloth.get_aabb()
		print("[clothcpu] drape t=%.1fs  aabb y=[%.3f, %.3f]" %
				[_ticks / 60.0, _drape_aabb.position.y, _drape_aabb.end.y])
	if _ticks >= REST_TICK:
		_finish()
		return true
	return false

func _finish() -> void:
	var failures: Array = []

	if not _cloth.is_simulating():
		failures.append("cloth is not simulating")
	if _cloth.is_gpu_accelerated():
		failures.append("cloth took the GPU path; this test must run the CPU fallback")

	var aabb := _cloth.global_transform * _cloth.get_aabb()
	var center_y := (aabb.position.y + aabb.end.y) * 0.5
	print("[clothcpu] rest t=%.1fs  aabb y=[%.3f, %.3f]  center_y=%.3f  start_y=%.1f" %
			[_ticks / 60.0, aabb.position.y, aabb.end.y, center_y, START_Y])

	# 1. The sheet fell.
	if _drape_aabb.position.y > START_Y - 1.0:
		failures.append("cloth never fell (drape-window top y = %.3f)" % _drape_aabb.position.y)
	# 2. The catch: at the drape window the sheet rides the ball (top at ball
	# height, edges already down toward the floor). With the old rest_info bug
	# the sheet ignored the ball and the floor entirely and was a meter
	# underground by this time.
	if _drape_aabb.end.y < BALL_CENTER.y + BALL_RADIUS * 0.5:
		failures.append("cloth did not catch on the ball (drape-window top y = %.3f, ball top = %.2f)"
				% [_drape_aabb.end.y, BALL_CENTER.y + BALL_RADIUS])
	if _drape_aabb.position.y > 0.35:
		failures.append("cloth edges never reached down (drape-window bottom y = %.3f)" % _drape_aabb.position.y)
	# 3. It never fell through the world (floor collision healthy after the
	# sheet slides off the ball and pools on the floor).
	if center_y < -0.5:
		failures.append("cloth fell through the world (rest center y = %.3f)" % center_y)
	if aabb.position.y > 0.5:
		failures.append("cloth never settled low (rest bottom y = %.3f)" % aabb.position.y)

	var ok := failures.is_empty()
	print("[clothcpu] ", "PASS" if ok else "FAIL")
	if not ok:
		for f in failures:
			push_error("[clothcpu] FAIL: " + f)
	_write_report(ok, failures)
	quit(0 if ok else 1)

func _write_report(ok: bool, failures: Array) -> void:
	if _json_path.is_empty():
		return
	var result := {
		"suite": "cloth",
		"final": true,
		"engine": ProjectSettings.get_setting("physics/3d/physics_engine", "?"),
		"tests": [{
			"id": "PHYSX-CLOTH-CPU-001",
			"category": "cloth",
			"description": "SIM_CPU sheet dropped on a static ball must drape (rest_info depenetration contract)",
			"status": "pass" if ok else "fail",
			"assertions": 4,
			"failures": failures.size(),
			"messages": failures,
		}],
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	else:
		push_error("[clothcpu] could not write JSON report to %s" % _json_path)
