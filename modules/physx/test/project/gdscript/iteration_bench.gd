extends SceneTree

# Phase D benchmark: solver iteration A/B (SPACE_PARAM_SOLVER_ITERATIONS).
# Stack of 160 boxes + a 20-body pin chain; measures avg tick wall time and
# pool stability per iteration config. Run headless:
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/iteration_bench.gd

const STACK_ITERS := 8
const MEASURE_TICKS := 200

var space: RID
var boxes: Array = []
var chain: Array = []
var chain_joints: Array = []
var config_idx := 0
var configs := [8, 4]
var results := {} # 8/8 (current) vs 4/4 (cost probe)
var tick := 0
var last_us := 0
var samples: Array = []

func _make_stack() -> void:
	for b in boxes:
		PhysicsServer3D.free_rid(b)
	boxes.clear()
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3(0.25, 0.25, 0.25))
	for row in range(20):
		for col in range(8):
			var b := PhysicsServer3D.body_create()
			PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_RIGID)
			PhysicsServer3D.body_set_space(b, space)
			PhysicsServer3D.body_add_shape(b, shape)
			PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
					Transform3D(Basis(), Vector3(col * 0.55 - 2.0, 0.3 + row * 0.55, 0)))
			PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.0)
			boxes.append(b)

func _make_chain() -> void:
	for j in chain_joints:
		PhysicsServer3D.free_rid(j)
	chain_joints.clear()
	for b in chain:
		PhysicsServer3D.free_rid(b)
	chain.clear()
	var prev: RID
	for i in range(20):
		var b := PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_RIGID)
		PhysicsServer3D.body_set_space(b, space)
		var s := PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(s, Vector3(0.1, 0.1, 0.1))
		PhysicsServer3D.body_add_shape(b, s)
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(8, 6 - i * 0.25, 0)))
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.0)
		chain.append(b)
		if i > 0:
			var j := PhysicsServer3D.joint_create()
			PhysicsServer3D.joint_make_pin(j, chain[i - 1], Vector3(0, -0.125, 0), b, Vector3(0, 0.125, 0))
			PhysicsServer3D.joint_set_solver_priority(j, 1)
			chain_joints.append(j)

func _initialize() -> void:
	print("[bench] engine = ", ProjectSettings.get_setting("physics/3d/physics_engine", "?"))
	space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)
	var floor_shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(floor_shape, Vector3(10, 0.1, 10))
	var floor := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(floor, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_add_shape(floor, floor_shape)
	PhysicsServer3D.body_set_state(floor, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, -0.1, 0)))
	PhysicsServer3D.body_set_space(floor, space)
	_make_stack()
	_make_chain()

func _physics_process(_d: float) -> bool:
	var now := Time.get_ticks_usec()
	var dt := now - last_us
	last_us = now
	tick += 1

	# Skip the first 20 ticks of each config (warmup), sample the rest.
	if tick > 20:
		samples.append(dt)

	if tick >= MEASURE_TICKS:
		var avg := 0.0
		for s in samples:
			avg += s
		avg /= maxf(samples.size(), 1)
		var iters: int = configs[config_idx]
		# Stability: mean |velocity| of the stack.
		var vel := 0.0
		for b in boxes:
			var v: Vector3 = PhysicsServer3D.body_get_state(b, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY)
			vel += v.length()
		vel /= maxf(boxes.size(), 1)
		print("[bench] iters=%d/%d  avg_tick=%.3f ms  stack_mean_vel=%.4f m/s  samples=%d" % [
				iters, iters, avg / 1000.0, vel, samples.size()])
		results[iters] = {"avg_ms": avg / 1000.0, "vel": vel}

		config_idx += 1
		if config_idx >= configs.size():
			var a: Dictionary = results[8]
			var b: Dictionary = results[4]
			var delta: float = (b["avg_ms"] - a["avg_ms"]) / maxf(a["avg_ms"], 0.001) * 100.0
			print("[bench] 4/4 vs 8/8 tick delta: %.1f%%  (vel %.4f vs %.4f)" % [delta, b["vel"], a["vel"]])
			print("[bench] DONE")
			quit(0)
			return true
		# Next config.
		samples = []
		tick = 0
		PhysicsServer3D.space_set_param(space, PhysicsServer3D.SPACE_PARAM_SOLVER_ITERATIONS, configs[config_idx])
		_make_stack()
		_make_chain()
	return false
