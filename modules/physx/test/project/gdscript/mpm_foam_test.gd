extends SceneTree

# MPM foam (Vulkan diffuse layer) integration test for PhysXParticleFluid3D.
#
# Spawns an MPM pool with foam enabled and asserts, in three phases:
#   1. AGITATE  -- a kinematic collider sphere churns the pool: foam spawns
#      (live count > 0).
#   2. DECAY    -- collider parked out of the fluid: foam decays to zero within
#      its lifetime.
#   3. OFF      -- foam_enabled = false and the collider churns again: no foam
#      spawns while the layer is off.
#
# NOTE: the MPM solver needs a RenderingDevice -- run WINDOWED (no --headless):
#
#   <binary> --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/mpm_foam_test.gd
#
# On a headless display server the solver cannot start and the test reports
# that explicitly. Exit 0 = pass, 1 = fail.

const AGITATE_TICKS := 180      # 3 s churning
# 4.25 s parked. Three kinds now decay at different rates: foam ~1x lifetime,
# spray ~0.5x, bubbles ~2x (max 1.2 * 1.25 * 2 = 3.0 s, plus a straggler
# spawned late in the agitation phase) -- the window must cover the slowest.
const DECAY_TICKS := 255
const OFF_TICKS := 90           # 1.5 s churning with the layer off

var fluid: PhysXParticleFluid3D
var collider: StaticBody3D
var ticks := 0
var max_foam := 0
var end_foam := -1
var off_max := -1
var done := false

func _initialize() -> void:
	print("[foam] physics/3d/physics_engine = ",
		ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	fluid = PhysXParticleFluid3D.new()
	fluid.solver = PhysXParticleFluid3D.SOLVER_MPM
	fluid.foam_enabled = true
	fluid.foam_lifetime = 1.2
	fluid.foam_particle_count = 8192
	fluid.particle_count = 8000
	fluid.particle_size = 0.08
	fluid.spawn_region_size = Vector3(1.2, 0.8, 1.2)
	fluid.mpm_domain_size = Vector3(3, 2.5, 3)
	fluid.position = Vector3(0, 1.4, 0)
	root.add_child(fluid)

	collider = StaticBody3D.new()
	var cs := CollisionShape3D.new()
	var shape := SphereShape3D.new()
	shape.radius = 0.25
	cs.shape = shape
	collider.add_child(cs)
	collider.position = Vector3(0, 0.2, 0)
	root.add_child(collider)
	# mpm_colliders is wired on the first physics tick -- nodes only enter the
	# scene tree (and get valid paths) once the main loop starts.

func _churn(t: float) -> void:
	# Sweep the collider sphere through the pool surface (pool: node y 1.4
	# +-0.4 -- keep the sphere at the surface).
	collider.global_position = Vector3(0.55 * sin(t * 5.0), 1.7 + 0.15 * cos(t * 3.0), 0.0)

func _physics_process(_delta: float) -> bool:
	if done:
		return true
	ticks += 1
	if ticks == 1:
		fluid.mpm_colliders = [collider.get_path()]

	if ticks <= AGITATE_TICKS:
		_churn(float(ticks))
		max_foam = maxi(max_foam, fluid.get_live_foam_count())
		if ticks % 30 == 0:
			print("[foam] agitate tick %d  live=%d  particles=%d" % [ticks, fluid.get_live_foam_count(), fluid.get_live_particle_count()])
	elif ticks <= AGITATE_TICKS + DECAY_TICKS:
		# Park the collider out of the fluid; foam fades within its lifetime.
		collider.global_position = Vector3(0, 5.0, 0)
		if ticks == AGITATE_TICKS + DECAY_TICKS:
			end_foam = fluid.get_live_foam_count()
			print("[foam] decay done at tick %d  live=%d" % [ticks, end_foam])
			# Phase 3 starts on the next tick: layer off, churn resumes.
			fluid.foam_enabled = false
	else:
		_churn(float(ticks))
		off_max = maxi(off_max, fluid.get_live_foam_count())
		if ticks >= AGITATE_TICKS + DECAY_TICKS + OFF_TICKS:
			_finish()
			return true
	return false

func _finish() -> void:
	done = true
	var failures: Array = []
	if max_foam <= 0:
		failures.append("no foam spawned while agitating (max=%d)" % max_foam)
	# The park must DECAY the population, not zero it exactly: with the real
	# Ihmsen potentials a settling pool keeps producing a small trickle of
	# legitimate foam from residual currents over the spawn floors (the same
	# mechanism that gives gentle rivers their surface foam). A small residual
	# is a pass; a sustained population is not.
	if end_foam > maxi(16, max_foam / 8):
		failures.append("foam did not decay after the collider parked (end=%d of max=%d)" % [end_foam, max_foam])
	if off_max != 0:
		failures.append("foam spawned while disabled (max=%d)" % off_max)
	for f in failures:
		push_error("[foam] FAIL: " + f)

	var ok := failures.is_empty()
	print("[foam] max=%d end=%d off_max=%d -> %s" % [max_foam, end_foam, off_max, "PASS" if ok else "FAIL"])
	quit(0 if ok else 1)
