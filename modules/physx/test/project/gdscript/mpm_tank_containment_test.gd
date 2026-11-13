extends SceneTree

# Containment test for godot-physx-example's gpu/physx_fluid demo on the MPM
# backend: the same tank geometry (floor + 4 walls, inner +-0.5 m, floor top at
# y = 0), the same faucet (diagonal-velocity stream), the tank bodies wired as
# MPM analytic colliders, and a domain covering the tank.
#
# Asserts after a fill + settle:
#   1. the water pooled INSIDE the tank (mean height near the floor),
#   2. particles stayed within the tank footprint (>= 90% inside +-0.6 m),
#   3. foam spawned during the pour.
#
# Windowed run (MPM needs a RenderingDevice):
#   <binary> --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/mpm_tank_containment_test.gd

const FAUCET := Vector3(-0.25, 2.3, -0.25)

var fluid: PhysXParticleFluid3D
var tank: Array[Node3D] = []
var ticks := 0
var max_foam := 0
var done := false

func _initialize() -> void:
	var root3d := Node3D.new()
	get_root().add_child(root3d)

	# Tank: identical dims to the demo (floor top at y = 0, walls +-0.5 inner).
	_wall(Vector3(0, -0.25, 0), Vector3(3.0, 0.5, 3.0))
	for offset in [Vector3(-0.575, 1.05, 0), Vector3(0.575, 1.05, 0)]:
		_wall(offset, Vector3(0.15, 2.85, 1.3)) # x walls span z
	for offset in [Vector3(0, 1.05, -0.575), Vector3(0, 1.05, 0.575)]:
		_wall(offset, Vector3(1.3, 2.85, 0.15)) # z walls span x

	fluid = PhysXParticleFluid3D.new()
	fluid.solver = PhysXParticleFluid3D.SOLVER_MPM
	fluid.spawn_on_ready = false
	fluid.particle_count = 40000
	fluid.particle_size = 0.035
	fluid.position = FAUCET
	fluid.emission_rate = 9000.0
	fluid.emission_radius = 0.06
	fluid.emission_velocity = Vector3(0.4, -2.5, 0.4)
	fluid.emitting = true
	fluid.foam_enabled = true
	fluid.foam_lifetime = 1.6
	fluid.foam_threshold = 120.0
	fluid.mpm_domain_size = Vector3(2.4, 5.0, 2.4)
	root3d.add_child(fluid)
	# Collider paths only resolve once every node is inside the tree; wire them
	# on the first physics tick (see mpm_foam_test for the same pattern).

func _physics_process(_d: float) -> bool:
	if done:
		return true
	ticks += 1
	if ticks == 1:
		var paths: Array[NodePath] = []
		for body in tank:
			paths.append(body.get_path())
		fluid.mpm_colliders = paths
	max_foam = maxi(max_foam, fluid.get_live_foam_count())
	if ticks == 600:
		# 10 s: pool filled and settled -- sample containment.
		var positions := fluid.get_particle_positions()
		var inside := 0
		var mean_y := 0.0
		for p in positions:
			mean_y += p.y
			if absf(p.x) <= 0.6 and absf(p.z) <= 0.6:
				inside += 1
		var frac := float(inside) / maxf(positions.size(), 1.0)
		mean_y /= maxf(positions.size(), 1.0)
		# Foam height profile: the buoyant-rise cap should keep foam near the
		# water surface; only genuine splash spray may fly high.
		var foam_pos := fluid.get_foam_positions()
		var foam_high := 0
		var foam_max_y := 0.0
		for fp in foam_pos:
			foam_max_y = maxf(foam_max_y, fp.y)
			if fp.y > 3.0:
				foam_high += 1
		var foam_high_frac := float(foam_high) / maxf(foam_pos.size(), 1)
		print("[tank] particles=%d  inside=%.1f%%  mean_y=%.2f  foam=%d  foam_max_y=%.2f  foam_above_3m=%.1f%%" % [
				positions.size(), frac * 100.0, mean_y, fluid.get_live_foam_count(),
				foam_max_y, foam_high_frac * 100.0])

		var failures: Array = []
		if frac < 0.9:
			failures.append("water escaped the tank footprint (%.1f%% inside)" % (frac * 100.0))
		if mean_y > 1.2:
			failures.append("water is not pooled at the tank floor (mean_y=%.2f)" % mean_y)
		if max_foam <= 0:
			failures.append("no foam spawned during the pour")
		if foam_high_frac > 0.2:
			failures.append("foam hangs in the air (%.1f%% above 3 m, max_y=%.2f)" % [foam_high_frac * 100.0, foam_max_y])
		for f in failures:
			push_error("[tank] FAIL: " + f)
		print("[tank] ", "PASS" if failures.is_empty() else "FAIL")
		done = true
		quit(0 if failures.is_empty() else 1)
		return true
	return false

func _wall(pos: Vector3, size: Vector3) -> StaticBody3D:
	var sb := StaticBody3D.new()
	var cs := CollisionShape3D.new()
	var box := BoxShape3D.new()
	box.size = size
	cs.shape = box
	sb.add_child(cs)
	sb.position = pos
	get_root().add_child(sb)
	tank.append(sb)
	return sb
