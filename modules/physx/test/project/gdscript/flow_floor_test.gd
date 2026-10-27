extends SceneTree

# Flow floor (audit R-16, PHYSX-FLOW-002).
#
#   With floor_enabled, gas never goes below floor_height: one wide solid
#   under the grid couples every channel to zero inside itself, so injected
#   gas cannot exist there. The physical half exploits exactly that: an
#   emitter straddling the floor line fills the cells below it while the
#   floor is off, and the same injection reads ~0 below the floor once it is
#   on (cell-level; the block-granular bounds cannot see a 2 m floor inside
#   4 m blocks). Toggling mid-run applies without a rebuild (frames keep
#   advancing).
#
# Run (needs the Flow runtime; runs headless on the fallback device):
#   <binary> --path modules/physx/test/project \
#     --script res://gdscript/flow_floor_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const FILL_FRAMES := 150
const CHECK_FRAMES := 270
const FLOOR_HEIGHT := 2.0
const UNDER := Vector3(0, 1.75, 0) # inside the emitter's sphere, below the floor

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
				"Flow floor: structural contract, mid-run toggle, physical floor", "skip", 0,
				["PhysXFlowSimulation3D not registered in this build"])
		quit(0)
		return

	var holder := Node3D.new()
	root.add_child(holder)
	sim = ClassDB.instantiate("PhysXFlowSimulation3D")
	holder.add_child(sim)
	sim.set("max_blocks", 1024)
	sim.set("cell_size", 0.25)
	sim.set("cpu_readback", true) # the physical floor check samples the readback

	# Floor properties round-trip before the runtime is up; the floor starts
	# OFF (the test toggles it mid-run).
	sim.set("floor_height", FLOOR_HEIGHT)
	sim.set("floor_enabled", true)
	sim.set("floor_enabled", false)
	_check(sim.get("floor_enabled") == false, "floor_enabled round-trips")
	_check(absf(float(sim.get("floor_height")) - FLOOR_HEIGHT) < 1e-4, "floor_height round-trips")

	emitter = ClassDB.instantiate("PhysXFlowEmitter3D")
	holder.add_child(emitter)
	# The sphere straddles the floor line: its lower half sits where the
	# solid will be.
	emitter.position = Vector3(0, FLOOR_HEIGHT, 0)
	emitter.set("radius", 0.4)
	emitter.set("smoke", 1.0)
	sim.set("emitters", [sim.get_path_to(emitter)])
	physics_frame.connect(_on_physics_frame)

func _available() -> bool:
	return sim != null and bool(sim.get_diagnostics().get("available"))

func _on_physics_frame() -> void:
	frame += 1
	if frame == 10 and not _available():
		print("[flow-floor] SKIP: Flow runtime unavailable (headless/Compatibility)")
		TestReport.write(_json_path, "flow", "PHYSX-FLOW-002",
				"Flow floor: structural contract, mid-run toggle, physical floor", "skip", _assertions,
				["Flow runtime unavailable in this session"])
		quit(0)
		return

	if frame == FILL_FRAMES - 10 and not _floor_on:
		# Contrast data point: without the floor the same injection fills the
		# cells below the floor line.
		var below := float(sim.sample_smoke(UNDER))
		_check(below > 0.2, "floor off: gas sits below the floor line (sample_smoke(0,1.75,0)=%.3f)" % below)

	if frame == FILL_FRAMES and not _floor_on:
		# Mid-run toggle: applies on the next step, no rebuild.
		_floor_on = true
		sim.set("floor_enabled", true)
		_frames_at_toggle = int(sim.get_diagnostics().get("frames_simulated"))
		_check(_frames_at_toggle > 0, "simulation ran before the toggle (%d frames)" % _frames_at_toggle)

	if frame == CHECK_FRAMES and _floor_on:
		var diag: Dictionary = sim.get_diagnostics()
		_check(int(diag.get("frames_simulated")) > _frames_at_toggle + 60,
				"frames keep advancing with the floor toggled mid-run (no rebuild stall)")
		_check(float(sim.get_last_max_smoke()) > 0.05,
				"gas still simulated with the floor on (max=%.3f)" % sim.get_last_max_smoke())
		_check(int(diag.get("active_blocks")) > 0, "blocks still allocated with the floor on")
		# Physical half (needs the world-correct readback): the same injection
		# that filled the cells below the floor line before the toggle reads
		# ~0 there now, while the gas above the floor keeps flowing.
		var under := float(sim.sample_smoke(UNDER))
		var deep := float(sim.sample_smoke(Vector3(0, 0.25, 0)))
		var above := float(sim.sample_smoke(Vector3(0, FLOOR_HEIGHT + 0.25, 0)))
		var temp := float(sim.sample_temperature(Vector3(0, FLOOR_HEIGHT + 0.25, 0)))
		_check(under >= 0.0 and under <= 0.05,
				"no gas below the floor (sample_smoke(0,1.75,0)=%.3f)" % under)
		_check(deep >= 0.0 and deep <= 0.05,
				"no gas deep below the floor (sample_smoke(0,0.25,0)=%.3f)" % deep)
		_check(above > 0.05,
				"gas keeps flowing above the floor (sample_smoke(0,2.25,0)=%.3f)" % above)
		_check(temp >= 0.0,
				"temperature samples alongside the smoke (%.3f)" % temp)
		_finish()

func _finish() -> void:
	var ok := not _failed
	print("[flow-floor] ", "PASS" if ok else "FAIL")
	TestReport.write(_json_path, "flow", "PHYSX-FLOW-002",
			"Flow floor: structural contract, mid-run toggle, physical floor",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
