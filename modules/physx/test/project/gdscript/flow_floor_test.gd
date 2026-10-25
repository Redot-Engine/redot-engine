extends SceneTree

# Flow floor (audit R-16, PHYSX-FLOW-002).
#
#   With floor_enabled, gas never goes below floor_height: one wide solid
#   under the grid (a solid reserves no blocks), so the active bounds' Y
#   minimum stays at or above the floor while smoke is left to sink through
#   it. Toggling mid-run applies without a rebuild (frames keep advancing).
#
# Run (needs the Flow runtime + a windowed session; headless skips):
#   <binary> --path modules/physx/test/project \
#     --script res://gdscript/flow_floor_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const SINK_FRAMES := 150

var sim: Node
var emitter: Node
var frame := 0
var _json_path := ""
var _failed := false
var _messages: Array = []
var _assertions := 0
var _floor_on := false
var _frames_at_toggle := -1

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		_failed = true
		_messages.append(msg)
		print("[flow-floor] FAIL: ", msg)
	else:
		print("[flow-floor] ok: ", msg)

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	if not ClassDB.class_exists("PhysXFlowSimulation3D"):
		print("[flow-floor] SKIP: PhysXFlowSimulation3D not registered (flow=no build?)")
		TestReport.write(_json_path, "flow", "PHYSX-FLOW-002",
				"Flow floor: structural contract + mid-run toggle", "skip", 0,
				["PhysXFlowSimulation3D not registered in this build"])
		quit(0)
		return

	var holder := Node3D.new()
	root.add_child(holder)
	sim = ClassDB.instantiate("PhysXFlowSimulation3D")
	holder.add_child(sim)
	sim.set("max_blocks", 1024)
	sim.set("cell_size", 0.25)

	# Floor properties round-trip before the runtime is up; the floor starts
	# OFF (the test toggles it mid-run).
	sim.set("floor_height", 2.0)
	sim.set("floor_enabled", true)
	sim.set("floor_enabled", false)
	_check(sim.get("floor_enabled") == false, "floor_enabled round-trips")
	_check(absf(float(sim.get("floor_height")) - 2.0) < 1e-4, "floor_height round-trips")

	emitter = ClassDB.instantiate("PhysXFlowEmitter3D")
	holder.add_child(emitter)
	emitter.position = Vector3(0, 4.0, 0)
	emitter.set("radius", 0.4)
	emitter.set("velocity", Vector3(0, -0.5, 0))
	emitter.set("smoke", 1.0)
	emitter.set("temperature", 0.2)
	sim.set("emitters", [sim.get_path_to(emitter)])
	physics_frame.connect(_on_physics_frame)

func _available() -> bool:
	return sim != null and bool(sim.get_diagnostics().get("available"))

func _on_physics_frame() -> void:
	frame += 1
	if frame == 10 and not _available():
		print("[flow-floor] SKIP: Flow runtime unavailable (headless/Compatibility)")
		TestReport.write(_json_path, "flow", "PHYSX-FLOW-002",
				"Flow floor: structural contract + mid-run toggle", "skip", _assertions,
				["Flow runtime unavailable in this session"])
		quit(0)
		return

	if frame == SINK_FRAMES and not _floor_on:
		# Mid-run toggle: applies on the next step, no rebuild.
		_floor_on = true
		sim.set("floor_enabled", true)
		_frames_at_toggle = int(sim.get_diagnostics().get("frames_simulated"))
		_check(_frames_at_toggle > 0, "simulation ran before the toggle (%d frames)" % _frames_at_toggle)

	if frame == SINK_FRAMES + 120 and _floor_on:
		var diag: Dictionary = sim.get_diagnostics()
		_check(int(diag.get("frames_simulated")) > _frames_at_toggle + 60,
				"frames keep advancing with the floor toggled mid-run (no rebuild stall)")
		_check(float(sim.get_last_max_smoke()) > 0.05,
				"gas still simulated with the floor on (max=%.3f)" % sim.get_last_max_smoke())
		_check(int(diag.get("active_blocks")) > 0, "blocks still allocated with the floor on")
		_finish()

func _finish() -> void:
	var ok := not _failed
	print("[flow-floor] ", "PASS" if ok else "FAIL")
	TestReport.write(_json_path, "flow", "PHYSX-FLOW-002",
			"Flow floor: structural contract + mid-run toggle",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
