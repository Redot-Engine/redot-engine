extends SceneTree

# GPU-mode rigid collision-exception test for the PhysX module.
#
# Validates that body_add_collision_exception suppresses SIMULATION contact
# (not just on the CPU pair-filter path but on GPU dynamics scenes, where the
# enforcement must come from the simulation filter shader's word2 slot
# registry). Two dynamic spheres are stacked above a static floor:
#
#   pass 1 (no exception): A settles on top of B  -> A.y ~ 1.5
#   pass 2 (A excludes B): A falls THROUGH B      -> A.y ~ 0.5 (floor)
#
# Run headless (GPU path is active whenever the build has physx_gpu=yes and a
# CUDA device is present -- see the "CUDA context ready" log line):
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/exception_gpu_test.gd -- --json=<path>
#
# Exit 0 = pass, 1 = fail.

const RADIUS := 0.5
const SETTLE_TICKS := 300      # 5 s at 60 Hz per pass
const FLOOR_REST_Y := RADIUS   # sphere resting on the floor (floor top at y=0)
const STACK_REST_Y := 2.0 * RADIUS + RADIUS  # A resting on B resting on floor

var _space: RID
var _floor_body: RID
var _sphere_shape: RID
var _body_a: RID
var _body_b: RID
var _pass := 0
var _ticks := 0
var _results := []
var _done := false
var _json_path := ""

func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			_json_path = arg.substr("--json=".length())
	print("[exception] physics/3d/physics_engine = ",
		ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	_space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(_space, true)

	# Static floor: box top at y = 0.
	var floor_shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(floor_shape, Vector3(4, 0.1, 4))  # half extents
	_floor_body = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_floor_body, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_set_collision_layer(_floor_body, 1)
	PhysicsServer3D.body_set_collision_mask(_floor_body, 1)
	PhysicsServer3D.body_add_shape(_floor_body, floor_shape)
	PhysicsServer3D.body_set_state(_floor_body, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, -0.1, 0)))
	PhysicsServer3D.body_set_space(_floor_body, _space)

	_sphere_shape = PhysicsServer3D.sphere_shape_create()
	PhysicsServer3D.shape_set_data(_sphere_shape, RADIUS)
	_start_pass(1)

func _start_pass(p_pass: int) -> void:
	_pass = p_pass
	_ticks = 0
	if _body_a.is_valid():
		PhysicsServer3D.free_rid(_body_a)
	if _body_b.is_valid():
		PhysicsServer3D.free_rid(_body_b)

	# Sphere B: dynamic, resting on the floor.
	_body_b = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_body_b, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_set_collision_layer(_body_b, 1)
	PhysicsServer3D.body_set_collision_mask(_body_b, 1)
	PhysicsServer3D.body_add_shape(_body_b, _sphere_shape)
	PhysicsServer3D.body_set_state(_body_b, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, FLOOR_REST_Y, 0)))
	PhysicsServer3D.body_set_space(_body_b, _space)

	# Sphere A: dynamic, dropped from above B.
	_body_a = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(_body_a, PhysicsServer3D.BODY_MODE_RIGID)
	PhysicsServer3D.body_set_collision_layer(_body_a, 1)
	PhysicsServer3D.body_set_collision_mask(_body_a, 1)
	PhysicsServer3D.body_add_shape(_body_a, _sphere_shape)
	PhysicsServer3D.body_set_state(_body_a, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, 3.0, 0)))
	PhysicsServer3D.body_set_space(_body_a, _space)

	if p_pass == 2:
		PhysicsServer3D.body_add_collision_exception(_body_a, _body_b)
		print("[exception] pass 2: exception added A-excludes-B")

func _physics_process(_delta: float) -> bool:
	if _done:
		return true
	var y_a: float = (PhysicsServer3D.body_get_state(_body_a, PhysicsServer3D.BODY_STATE_TRANSFORM) as Transform3D).origin.y
	_ticks += 1
	if _ticks == 1 or _ticks % 60 == 0:
		print("[exception] pass %d tick %d  yA=%.3f" % [_pass, _ticks, y_a])
	if _ticks >= SETTLE_TICKS:
		var y_b: float = (PhysicsServer3D.body_get_state(_body_b, PhysicsServer3D.BODY_STATE_TRANSFORM) as Transform3D).origin.y
		_results.append({"pass": _pass, "y_a": y_a, "y_b": y_b})
		print("[exception] pass %d settled: yA=%.3f yB=%.3f" % [_pass, y_a, y_b])
		if _pass == 1:
			_start_pass(2)
		else:
			_finish()
	return false

func _finish() -> void:
	_done = true
	var failures: Array = []
	var y_a_no_exc: float = _results[0]["y_a"]
	var y_a_exc: float = _results[1]["y_a"]

	# Pass 1: A must come to rest ON B (stack), proving the pair collides.
	if absf(y_a_no_exc - STACK_REST_Y) > 0.2:
		failures.append("pass 1: A did not rest on B (yA=%.3f, want ~%.3f)" % [y_a_no_exc, STACK_REST_Y])
		push_error("[exception] FAIL: " + failures[-1])
	# Pass 2: with the exception, A must fall past B and rest on the floor.
	if absf(y_a_exc - FLOOR_REST_Y) > 0.2:
		failures.append("pass 2: exception did not suppress the pair (yA=%.3f, want ~%.3f)" % [y_a_exc, FLOOR_REST_Y])
		push_error("[exception] FAIL: " + failures[-1])
	# And the outcome must actually differ between the passes.
	if y_a_exc >= y_a_no_exc - 0.5:
		failures.append("pass outcomes indistinguishable (%.3f vs %.3f)" % [y_a_no_exc, y_a_exc])
		push_error("[exception] FAIL: " + failures[-1])

	var ok := failures.is_empty()
	print("[exception] ", "PASS" if ok else "FAIL")
	var result := {
		"suite": "exception_gpu",
		"final": true,
		"engine": ProjectSettings.get_setting("physics/3d/physics_engine", "?"),
		"tests": [{
			"id": "PHYSX-EXC-GPU-001",
			"category": "collision_exceptions",
			"description": "excepted body pair falls through on a GPU dynamics scene",
			"status": "pass" if ok else "fail",
			"assertions": 3,
			"failures": failures.size(),
			"messages": failures,
		}],
	}
	if not _json_path.is_empty():
		var f := FileAccess.open(_json_path, FileAccess.WRITE)
		if f:
			f.store_string(JSON.stringify(result, "  "))
			f.close()
	quit(0 if ok else 1)
