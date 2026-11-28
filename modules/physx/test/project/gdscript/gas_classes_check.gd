extends SceneTree

func _initialize() -> void:
	var gas := ClassDB.class_exists("PhysXGas3D")
	var emitter := ClassDB.class_exists("PhysXGasEmitter3D")
	print("[gas] PhysXGas3D exists=", gas, "  PhysXGasEmitter3D exists=", emitter)
	if not (gas and emitter):
		print("[gas] FAIL")
		quit(1)
		return
	# Instantiate both and let a couple of frames run (inert without a device).
	var g: Object = ClassDB.instantiate("PhysXGas3D")
	var e: Object = ClassDB.instantiate("PhysXGasEmitter3D")
	print("[gas] instantiated: ", g, " / ", e)
	root.add_child(g)
	if e is Node:
		g.add_child(e)
	var frames := 0
	while frames < 5:
		await process_frame
		frames += 1
	print("[gas] PASS")
	quit(0)
