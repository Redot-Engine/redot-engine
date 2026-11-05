extends SceneTree

# Runs the demo scene AS COMMITTED (windowed -- no --headless: the render
# path needs a real RenderingDevice) and asserts the scene's own wiring
# produces gas and frames:
#   redot --path modules/physx/test/project -s gdscript/flow_demo_check.gd

var flow: Node
var frames := 0

func _initialize() -> void:
	var packed: PackedScene = load("res://flow_demo.tscn")
	var scene: Node = packed.instantiate()
	root.add_child(scene)
	flow = scene.get_node("Flow")
	# Bisect switches: FLOW_OFF=1 drops the render effect, FOG_OFF=1 the
	# scene's volumetric fog.
	physics_frame.connect(_tick)

func _tick() -> void:
	frames += 1
	if frames == 240:
		var diag: Dictionary = flow.get_diagnostics()
		print("[flow-demo] blocks=", diag.get("active_blocks"),
				"/", diag.get("max_blocks"),
				" render_outputs=", diag.get("render_outputs"),
				" backend=", diag.get("backend"),
				" max_smoke=", "%.3f" % flow.get_last_max_smoke())
		var ok: bool = int(diag.get("active_blocks", 0)) > 0 \
				and int(diag.get("render_outputs", 0)) > 100 \
				and diag.get("backend", "") == "rd"
		print("[flow-demo] RESULT: ", "PASS" if ok else "FAIL")
		quit(0 if ok else 1)
