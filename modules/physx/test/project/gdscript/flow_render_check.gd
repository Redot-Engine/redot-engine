extends SceneTree

# Windowed render-path check for the Flow integration's RenderingDevice
# bridge: builds a lit scene with a wired emitter and camera, runs the
# simulation on Godot's own RenderingDevice through PhysXFlowRenderEffect,
# and asserts the effect actually produced composited frames.
#
# Run WINDOWED (no --headless -- the RD path needs a real device):
#   redot --path modules/physx/test/project -s gdscript/flow_render_check.gd

var sim: Node
var frame := 0

func _initialize() -> void:
	if not ClassDB.class_exists("PhysXFlowSimulation3D"):
		print("[flow-rd] SKIP: PhysXFlowSimulation3D not registered (flow=no build?)")
		quit(0)
		return

	var holder := Node3D.new()
	root.add_child(holder)

	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color(0.1, 0.1, 0.12)
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color(0.4, 0.45, 0.5)
	env.ambient_light_energy = 0.6
	var world_env := WorldEnvironment.new()
	world_env.environment = env
	holder.add_child(world_env)

	var sun := DirectionalLight3D.new()
	sun.rotation_degrees = Vector3(-45.0, 30.0, 0.0)
	holder.add_child(sun)

	var cam := Camera3D.new()
	cam.position = Vector3(0, 1.6, 4.0)
	cam.fov = 60.0
	holder.add_child(cam)
	cam.make_current()

	sim = ClassDB.instantiate("PhysXFlowSimulation3D") as Node
	holder.add_child(sim)
	# Known-good readback budget: the sparse-copy readback decodes reliably
	# at this block count (see READBACK_BYTES_LIMIT in flow_simulation.cpp).
	sim.set("max_blocks", 1024)
	sim.set("cell_size", 0.2)
	sim.set("cpu_readback", true) # the check asserts on the query surface
	sim.set("fog_density", 2.4)

	var emitter: Node3D = ClassDB.instantiate("PhysXFlowEmitter3D") as Node3D
	sim.add_child(emitter)
	emitter.position = Vector3(0, 0.3, 0)
	emitter.set("radius", 0.35)
	emitter.set("velocity", Vector3(0, 3.0, 0))
	emitter.set("smoke", 0.8)
	emitter.set("temperature", 0.6)
	emitter.set("fuel", 0.3)
	sim.set("emitters", [sim.get_path_to(emitter)])

	physics_frame.connect(_on_physics_frame)

func _on_physics_frame() -> void:
	frame += 1
	if frame == 60 or frame == 240:
		var diag: Dictionary = sim.get_diagnostics()
		print("[flow-rd] frame=", frame,
				" backend=", diag.get("backend"),
				" blocks=", diag.get("active_blocks"), "/", diag.get("max_blocks"),
				" render_calls=", diag.get("render_calls"),
				" render_outputs=", diag.get("render_outputs"),
				" max_smoke=", "%.3f" % sim.get_last_max_smoke())
	if frame >= 420:
		var diag: Dictionary = sim.get_diagnostics()
		var on_rd: bool = diag.get("backend", "") == "rd"
		var drew: bool = int(diag.get("render_outputs", 0)) > 100
		var smoked: bool = sim.get_last_max_smoke() > 0.0
		var ok: bool = on_rd and drew and smoked
		print("[flow-rd] RESULT: ", "PASS" if ok else "FAIL",
				" (backend=", diag.get("backend"),
				" render_calls=", diag.get("render_calls"),
				" render_outputs=", diag.get("render_outputs"),
				" blocks=", diag.get("active_blocks"),
				" max_smoke=", "%.3f" % sim.get_last_max_smoke(), ")")
		quit(0 if ok else 1)
