extends SceneTree

# Phase 8 benchmark: rigid-core hot paths A/B (SPACE-10 pair-notification
# gating, motion-query scratch reuse, _finish_step sync-set buffers,
# area-override scratch, scene-gravity cache, server space-list reuse).
#
# Headless run (see iteration_bench.gd for the pattern):
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/bench_rigid_core.gd
#
# Phases:
#   sim_stack  : 1000-box pile + one gravity-override area covering it, 20
#                boxes re-dropped per tick (constant pair churn so new-pair
#                filtering stays hot). Frame wall time over 240 ticks.
#   queries    : same scene; 10k rays + 1k body_test_motion per tick, batch
#                wall time per op (motion queries are the SPACE-10-adjacent
#                allocation target: shape list + quaternion were re-fetched
#                per phase per call).
#   joint_grid : 300 bodies in 50 pinned chains (can_sleep off), frame wall
#                time over 240 ticks.
#
# BENCH_TAG env labels the run ("baseline" / "fixed") in the JSON lines.
# Fixed RNG seed keeps the churn identical across builds.

const STACK_N := 10 # boxes per axis (10^3 = 1000)
const WARMUP_TICKS := 40
const SIM_TICKS := 240
const CHURN_PER_TICK := 20
const RAYS_PER_TICK := 10000
const MOTIONS_PER_TICK := 1000
const QUERY_WARMUP_TICKS := 10
const QUERY_TICKS := 120
const CHAINS := 50
const CHAIN_LINKS := 6

var space: RID
var stack: Array = []
var stack_shape: RID
var prober: RID
var floor_body: RID
var area_body: RID
var chains: Array = []
var chain_joints: Array = []

var phase := 0 # 0 sim_stack, 1 queries, 2 joint_grid
var tick := 0
var last_us := 0
var samples: Array = [] # frame deltas (us) for the active sim phase
var ray_us: Array = [] # per-tick raycast batch totals (us)
var motion_us: Array = [] # per-tick body_test_motion batch totals (us)
var ray_hits := 0
var motion_hits := 0
var ray_params: PhysicsRayQueryParameters3D
var motion_params: PhysicsTestMotionParameters3D
var motion_result: PhysicsTestMotionResult3D
var probe_spots: Array = [] # Transform3D starts for the motion prober
var rng := RandomNumberGenerator.new()
var tag := "run"

func _stat(arr: Array) -> Dictionary:
	if arr.is_empty():
		return { "mean": 0.0, "p50": 0.0, "p95": 0.0, "min": 0.0 }
	var s := arr.duplicate()
	s.sort()
	var mean := 0.0
	for v in s:
		mean += v
	mean /= s.size()
	return {
		"mean": snappedf(mean, 0.01),
		"p50": snappedf(s[int(s.size() * 0.5)], 0.01),
		"p95": snappedf(s[int(s.size() * 0.95)], 0.01),
		"min": snappedf(s[0], 0.01),
	}

func _report(phase_name: String, extra := {}) -> void:
	var d := _stat(samples)
	var line := {
		"tag": tag, "phase": phase_name, "samples": samples.size(),
		"mean_us": d["mean"], "p50_us": d["p50"], "p95_us": d["p95"], "min_us": d["min"],
	}
	for k in extra:
		line[k] = extra[k]
	print("[bench-core] ", JSON.stringify(line))

func _make_floor() -> void:
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3(12, 0.5, 12))
	floor_body = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(floor_body, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_add_shape(floor_body, shape)
	PhysicsServer3D.body_set_state(floor_body, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, -0.5, 0)))
	PhysicsServer3D.body_set_space(floor_body, space)

func _make_stack() -> void:
	stack_shape = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(stack_shape, Vector3(0.5, 0.5, 0.5))
	var spacing := 1.02
	for iz in range(STACK_N):
		for iy in range(STACK_N):
			for ix in range(STACK_N):
				var b: RID = PhysicsServer3D.body_create()
				PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_RIGID)
				PhysicsServer3D.body_set_space(b, space)
				PhysicsServer3D.body_add_shape(b, stack_shape)
				PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
						Transform3D(Basis(), Vector3(
								(ix - STACK_N * 0.5) * spacing,
								0.55 + iy * spacing,
								(iz - STACK_N * 0.5) * spacing)))
				PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.0)
				stack.append(b)
	# One gravity-override area over the whole pile: with the same magnitude
	# and direction as scene gravity this is behavior-neutral, but it routes
	# every body's pre-step through the area-override resolution (the per-body
	# order-scratch path).
	var ashape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(ashape, Vector3(7, 8, 7))
	area_body = PhysicsServer3D.area_create()
	PhysicsServer3D.area_set_space(area_body, space)
	PhysicsServer3D.area_add_shape(area_body, ashape, Transform3D(Basis(), Vector3(0, 5.5, 0)))
	PhysicsServer3D.area_set_collision_layer(area_body, 1)
	PhysicsServer3D.area_set_collision_mask(area_body, 0xFFFFFFFF)
	PhysicsServer3D.area_set_param(area_body, PhysicsServer3D.AREA_PARAM_GRAVITY_OVERRIDE_MODE,
			PhysicsServer3D.AREA_SPACE_OVERRIDE_REPLACE)
	PhysicsServer3D.area_set_param(area_body, PhysicsServer3D.AREA_PARAM_GRAVITY, 9.8)
	PhysicsServer3D.area_set_param(area_body, PhysicsServer3D.AREA_PARAM_GRAVITY_VECTOR, Vector3(0, -1, 0))

func _make_prober() -> void:
	var capsule := PhysicsServer3D.capsule_shape_create()
	PhysicsServer3D.shape_set_data(capsule, { "radius": 0.3, "height": 1.2 })
	prober = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(prober, PhysicsServer3D.BODY_MODE_KINEMATIC)
	PhysicsServer3D.body_set_space(prober, space)
	PhysicsServer3D.body_add_shape(prober, capsule)
	PhysicsServer3D.body_set_collision_mask(prober, 0xFFFFFFFF)
	# Motion-probe start poses hovering over the pile (body_test_motion is a
	# query; the kinematic body itself never moves).
	for i in range(50):
		var x := fmod(i * 0.37, 9.0) - 4.5
		var z := fposmod(i * 0.73, 9.0) - 4.5
		probe_spots.append(Transform3D(Basis(), Vector3(x, 7.0, z)))

func _free_stack() -> void:
	PhysicsServer3D.free_rid(area_body)
	area_body = RID()
	PhysicsServer3D.free_rid(prober)
	prober = RID()
	for b in stack:
		PhysicsServer3D.free_rid(b)
	stack.clear()
	PhysicsServer3D.free_rid(stack_shape)
	stack_shape = RID()

func _make_chains() -> void:
	var link_shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(link_shape, Vector3(0.08, 0.08, 0.08))
	for c in range(CHAINS):
		var col := c % 10
		var row := c / 10
		var anchor_x := (col - 4.5) * 1.6
		var anchor_z := (row - 2.0) * 1.6
		var anchor: RID = PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(anchor, PhysicsServer3D.BODY_MODE_STATIC)
		PhysicsServer3D.body_set_space(anchor, space)
		PhysicsServer3D.body_add_shape(anchor, link_shape)
		PhysicsServer3D.body_set_state(anchor, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(anchor_x, 6.0, anchor_z)))
		chains.append(anchor)
		var prev := anchor
		for i in range(CHAIN_LINKS):
			var b: RID = PhysicsServer3D.body_create()
			PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_RIGID)
			PhysicsServer3D.body_set_space(b, space)
			PhysicsServer3D.body_add_shape(b, link_shape)
			PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
					Transform3D(Basis(), Vector3(anchor_x, 6.0 - (i + 1) * 0.3, anchor_z)))
			PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.0)
			PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_CAN_SLEEP, false)
			chains.append(b)
			var j: RID = PhysicsServer3D.joint_create()
			PhysicsServer3D.joint_make_pin(j, prev, Vector3(0, -0.15, 0), b, Vector3(0, 0.15, 0))
			PhysicsServer3D.joint_set_solver_priority(j, 1)
			chain_joints.append(j)
			prev = b
	# Chains hang from a shared static rail; give the anchors a floor so the
	# swinging links never reach the ground plane.
	PhysicsServer3D.free_rid(link_shape)

func _free_chains() -> void:
	for j in chain_joints:
		PhysicsServer3D.free_rid(j)
	chain_joints.clear()
	for b in chains:
		PhysicsServer3D.free_rid(b)
	chains.clear()

func _initialize() -> void:
	tag = OS.get_environment("BENCH_TAG")
	if tag.is_empty():
		tag = "run"
	rng.seed = 0x50e9ce
	print("[bench-core] engine=", ProjectSettings.get_setting("physics/3d/physics_engine", "?"), " tag=", tag)
	space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)
	_make_floor()
	_make_stack()
	_make_prober()
	ray_params = PhysicsRayQueryParameters3D.create(Vector3(), Vector3(0, -1, 0), 0xFFFFFFFF)
	ray_params.collide_with_bodies = true
	motion_params = PhysicsTestMotionParameters3D.new()
	motion_result = PhysicsTestMotionResult3D.new()
	last_us = Time.get_ticks_usec()

func _physics_process(_d: float) -> bool:
	var now := Time.get_ticks_usec()
	var dt := now - last_us
	last_us = now
	tick += 1

	if phase == 0:
		# Pair-churn: re-drop a slice of the pile every tick so new contact
		# pairs (and therefore new-pair filter callbacks) stay hot.
		if tick > WARMUP_TICKS:
			for i in range(CHURN_PER_TICK):
				var b: RID = stack[rng.randi_range(0, stack.size() - 1)]
				PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
						Transform3D(Basis(), Vector3(
								rng.randf_range(-4.5, 4.5), rng.randf_range(8.0, 11.0), rng.randf_range(-4.5, 4.5))))
				PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY, Vector3())
				PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_ANGULAR_VELOCITY, Vector3())
			samples.append(dt)
		if tick >= WARMUP_TICKS + SIM_TICKS:
			_report("sim_stack", { "pairs": PhysicsServer3D.get_process_info(PhysicsServer3D.INFO_COLLISION_PAIRS) })
			phase = 1
			tick = 0
	elif phase == 1:
		if tick > QUERY_WARMUP_TICKS:
			var dss: PhysicsDirectSpaceState3D = PhysicsServer3D.space_get_direct_state(space)
			var t0 := Time.get_ticks_usec()
			var hits := 0
			for i in range(RAYS_PER_TICK):
				var fx := fmod(i * 0.017, 9.0) - 4.5
				var fz := fposmod(i * 0.031, 9.0) - 4.5
				ray_params.from = Vector3(fx, 8.0, fz)
				ray_params.to = Vector3(fx, -2.0, fz)
				if not dss.intersect_ray(ray_params).is_empty():
					hits += 1
			var t1 := Time.get_ticks_usec()
			var mhits := 0
			for i in range(MOTIONS_PER_TICK):
				motion_params.from = probe_spots[i % probe_spots.size()]
				motion_params.motion = Vector3(0, -0.6 - 0.2 * (i % 4), 0)
				if PhysicsServer3D.body_test_motion(prober, motion_params, motion_result):
					mhits += 1
			var t2 := Time.get_ticks_usec()
			ray_us.append(t1 - t0)
			motion_us.append(t2 - t1)
			ray_hits += hits
			motion_hits += mhits
		if tick >= QUERY_WARMUP_TICKS + QUERY_TICKS:
			var r := _stat(ray_us)
			var m := _stat(motion_us)
			print("[bench-core] ", JSON.stringify({
				"tag": tag, "phase": "queries", "samples": ray_us.size(),
				"ray_batch_us": r, "motion_batch_us": m,
				"ray_hit_ratio": snappedf(float(ray_hits) / maxf(1.0, RAYS_PER_TICK * ray_us.size()), 0.0001),
				"motion_hit_ratio": snappedf(float(motion_hits) / maxf(1.0, MOTIONS_PER_TICK * motion_us.size()), 0.0001),
			}))
			_free_stack()
			_make_chains()
			phase = 2
			tick = 0
			samples = []
			last_us = Time.get_ticks_usec()
	elif phase == 2:
		if tick > WARMUP_TICKS:
			samples.append(dt)
		if tick >= WARMUP_TICKS + SIM_TICKS:
			_report("joint_grid", {})
			_free_chains()
			print("[bench-core] DONE")
			quit(0)
			return true
	return false
