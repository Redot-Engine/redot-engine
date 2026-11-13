extends SceneTree

# Class/binding existence check for the Flow integration (mirrors
# gas_classes_check.gd). Run:
#   redot --headless --path test/project -s gdscript/flow_classes_check.gd

func _initialize() -> void:
	var names := [
		"PhysXFlowSimulation3D",
		"PhysXFlowEmitter3D",
		"PhysXFlowCollider3D",
	]
	var missing := 0
	for n in names:
		var ok := ClassDB.class_exists(n)
		print("[flow] ", n, " exists=", ok)
		if not ok:
			missing += 1
	# The Blast bridge only exists in blast+flow builds; report but don't fail.
	var bridge := ClassDB.class_exists("PhysXFlowBlastBridge3D")
	print("[flow] PhysXFlowBlastBridge3D exists=", bridge, " (optional, blast-gated)")
	if missing > 0:
		print("[flow] FAIL")
		quit(1)
		return

	# Instantiate the trio and let a few frames run (inert without a GPU).
	var sim: Object = ClassDB.instantiate("PhysXFlowSimulation3D")
	var em: Object = ClassDB.instantiate("PhysXFlowEmitter3D")
	var col: Object = ClassDB.instantiate("PhysXFlowCollider3D")
	print("[flow] instantiated: ", sim, " / ", em, " / ", col)
	root.add_child(sim)
	sim.add_child(em)
	sim.add_child(col)

	var frames := 0
	while frames < 5:
		await process_frame
		frames += 1

	# Property round-trips (serialization surface).
	sim.set("max_blocks", 2048)
	assert(sim.get("max_blocks") == 2048)
	sim.set("cell_size", 0.25)
	assert(absf(float(sim.get("cell_size")) - 0.25) < 0.0001)
	sim.set("combustion_enabled", false)
	assert(bool(sim.get("combustion_enabled")) == false)
	print("[flow] property round-trips ok")
	print("[flow] PASS")
	quit(0)
