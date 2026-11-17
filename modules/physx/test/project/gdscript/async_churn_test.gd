extends SceneTree

# Phase B validation: async-step GPU churn. Runs with async_step forced on and
# continuously creates/destroys GPU cloths and PBD fluids so that destruction
# and host->device writes land while a solve is in flight -- the exact window
# the fetch-first guards cover. Pass = process survives the loop with no PhysX
# errors. Windowed run (cloth needs a RenderingDevice, fluid needs CUDA):
#
#   <binary> --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/async_churn_test.gd

var cloths: Array = []
var fluids: Array = []
var ticks := 0

func _initialize() -> void:
	ProjectSettings.set_setting("physics/physx_3d/simulation/async_step", true)
	print("[churn] async_step forced on")

func _physics_process(_d: float) -> bool:
	ticks += 1
	if ticks == 1:
		print("[churn] physics/3d/physics_engine = ",
				ProjectSettings.get_setting("physics/3d/physics_engine", "?"))

	# Create cloths for the first 240 ticks, every 10th tick.
	if ticks % 10 == 0 and ticks <= 240:
		var c := PhysXCloth3D.new()
		c.position = Vector3(float(ticks % 40) * 0.2 - 4.0, 3.0 + float(ticks % 7), 0.0)
		root.add_child(c)
		cloths.append(c)

	# Create PBD fluids every 5th tick (up to 240).
	if ticks % 5 == 0 and ticks <= 240:
		var f := PhysXParticleFluid3D.new()
		f.solver = PhysXParticleFluid3D.SOLVER_PBD
		f.spawn_on_ready = false
		f.particle_count = 1500
		f.particle_size = 0.08
		f.emitting = true
		f.foam_enabled = true
		f.position = Vector3(float(ticks % 30) * 0.3 - 4.0, 2.0, float(ticks % 20) * 0.3 - 3.0)
		root.add_child(f)
		f.spawn()
		# Exercise the H2D write path immediately (emit), possibly mid-solve.
		f.emit([Vector3(0, 0.5, 0), Vector3(0.05, 0.55, 0)], Vector3(0, -2, 0))
		fluids.append(f)

	# Free the oldest fluid/cloth while the next solve may already be running.
	if ticks % 10 == 5 and fluids.size() > 4:
		var f: Node = fluids.pop_front()
		if is_instance_valid(f):
			f.queue_free()
	if ticks % 10 == 7 and cloths.size() > 4:
		var c: Node = cloths.pop_front()
		if is_instance_valid(c):
			c.queue_free()

	if ticks >= 480:
		# Drain everything, then give the async pipeline a few frames to flush.
		for f in fluids:
			if is_instance_valid(f):
				f.queue_free()
		for c in cloths:
			if is_instance_valid(c):
				c.queue_free()
		print("[churn] survived %d churned frames -> PASS" % ticks)
		quit(0)
		return true
	return false
