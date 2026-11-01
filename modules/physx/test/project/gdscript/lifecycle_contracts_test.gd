extends SceneTree

# Lifecycle-contract regression tests (audit section 3.4, GAP-10 / Phase 8).
#
# Four contracts, each with its own report entry:
#   PHYSX-LIFEC-001  body free during overlap -> body_exited still fires
#                   (deferred monitor dispatch caches event identity at queue
#                   time; removed-actor trigger pairs are skipped by onTrigger,
#                   so the exit must come from the module's own bookkeeping).
#   PHYSX-LIFEC-002  static<->dynamic mode flip with a pin joint attached:
#                   the joint wrapper survives the actor recreation, keeps its
#                   RID and solver priority, and the rebuilt constraint still
#                   holds the two body origins together while they fall.
#   PHYSX-LIFEC-003  async stepping: freeing an overlapping body inside the
#                   simulate->fetch window queues the actor release until after
#                   the fetch, and the body_exited event still dispatches.
#   PHYSX-LIFEC-004  a monitor callback that frees its own area during dispatch:
#                   flush_pending_callbacks() must survive (freed dispatching
#                   areas are skipped by registration lookup) and the tree must
#                   keep simulating afterwards.
#
# Run headless:
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/lifecycle_contracts_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

const PHASE_TIMEOUT := 300      # ticks before a wait is declared failed
const SETTLE_TICKS := 30
const JOINT_VERIFY_TICKS := 90
const ASYNC_ITERATIONS := 3

# Phases (executed strictly in order).
enum {
	P_LC001_SETUP,
	P_LC001_WAIT_ENTER,
	P_LC001_FREE,
	P_LC001_WAIT_EXIT,
	P_LC002_BUILD,
	P_LC002_SETTLE,
	P_LC002_FLIP_STATIC,
	P_LC002_SETTLE_STATIC,
	P_LC002_FLIP_BACK,
	P_LC002_VERIFY,
	P_LC003_SETUP,
	P_LC003_WAIT_ENTER,
	P_LC003_FREE_IN_WINDOW,
	P_LC003_WAIT_EXIT,
	P_LC004_BUILD,
	P_LC004_WAIT_ENTER,
	P_LC004_QUIESCE,
	P_DONE,
}

var _phase := P_LC001_SETUP
var _ticks := 0
var _results := {}
var _space: RID

# LC-001 / LC-003 shared bookkeeping.
var _monitor_body: RID
var _monitor_body_shape: RID
var _monitor_area: RID
var _monitor_area_shape: RID
var _entered := false
var _exited := false
var _async_iteration := 0

# LC-002 joint pair.
var _joint: RID
var _joint_body_a: RID
var _joint_body_b: RID
var _joint_shape_a: RID
var _joint_shape_b: RID
var _joint_max_distance := 0.0
var _priority_before := 0

var _json_path := ""

func _initialize() -> void:
	_json_path = _json_path_from_args()
	_space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(_space, true)

# ---------------------------------------------------------------- helpers --

func _json_path_from_args() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			return arg.substr("--json=".length())
	return ""

func _make_box_body(mode: int, pos: Vector3, half: float) -> Array:
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3(half, half, half))
	var body := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(body, mode)
	PhysicsServer3D.body_set_space(body, _space)
	PhysicsServer3D.body_add_shape(body, shape)
	PhysicsServer3D.body_set_state(body, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), pos))
	return [body, shape]

func _make_area(pos: Vector3, half: float, callback: String) -> Array:
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3(half, half, half))
	var area := PhysicsServer3D.area_create()
	PhysicsServer3D.area_set_space(area, _space)
	PhysicsServer3D.area_add_shape(area, shape)
	PhysicsServer3D.area_set_transform(area, Transform3D(Basis(), pos))
	PhysicsServer3D.area_set_monitor_callback(area, Callable(self, callback))
	return [area, shape]

func _record(id: String, ok: bool, messages: Array) -> void:
	_results[id] = {
		"status": "pass" if ok else "fail",
		"messages": messages,
		"assertions": 1 + messages.size(),
	}

func _enter(phase: int) -> void:
	_phase = phase
	_ticks = 0

func _tick() -> void:
	_ticks += 1

func _timed_out() -> bool:
	return _ticks > PHASE_TIMEOUT

# ------------------------------------------------------------- callbacks --

func _on_monitor_a1(status: int, body_rid: RID, _instance_id: int, _body_shape: int, _area_shape: int) -> void:
	if body_rid == _monitor_body:
		if status == PhysicsServer3D.AREA_BODY_ADDED:
			_entered = true
		elif status == PhysicsServer3D.AREA_BODY_REMOVED:
			_exited = true

func _on_monitor_lc004(_status: int, _body_rid: RID, _instance_id: int, _body_shape: int, _area_shape: int) -> void:
	# Contract under test: freeing THIS area from inside its own monitor
	# dispatch must not crash the flush loop. Queued events for the freed
	# area are skipped by the registration lookup.
	PhysicsServer3D.free_rid(_monitor_area)
	PhysicsServer3D.free_rid(_monitor_area_shape)
	_monitor_area = RID()
	_monitor_area_shape = RID()

func _setup_overlap() -> void:
	_entered = false
	_exited = false
	# Area FIRST (with its monitor callback registered), then the body -- this
	# is the proven-good order from the binding suite; creating the body before
	# the area in sync mode can miss the spawn-overlap ADDED event.
	var parts := _make_area(Vector3.ZERO, 0.75, "_on_monitor_a1")
	_monitor_area = parts[0]
	_monitor_area_shape = parts[1]
	var parts2 := _make_box_body(PhysicsServer3D.BODY_MODE_RIGID, Vector3.ZERO, 0.25)
	_monitor_body = parts2[0]
	_monitor_body_shape = parts2[1]

func _free_all(parts: Array) -> void:
	for rid in parts:
		PhysicsServer3D.free_rid(rid)

# ------------------------------------------------------------------- flow --

func _physics_process(_delta: float) -> bool:
	_tick()
	match _phase:
		P_LC001_SETUP:
			_setup_overlap()
			_enter(P_LC001_WAIT_ENTER)
		P_LC001_WAIT_ENTER:
			if _entered:
				# Free the body while it overlaps the area (pre-step window;
				# sync mode). The exit must still be dispatched.
				_free_all([_monitor_body, _monitor_body_shape])
				_enter(P_LC001_WAIT_EXIT)
			elif _timed_out():
				_record("PHYSX-LIFEC-001", false, ["body_entered never fired"])
				_enter(P_LC002_BUILD)
		P_LC001_WAIT_EXIT:
			if _exited:
				_record("PHYSX-LIFEC-001", true, [])
				_enter(P_LC002_BUILD)
			elif _timed_out():
				_record("PHYSX-LIFEC-001", false,
						["body_exited did not fire after free-during-overlap"])
				_enter(P_LC002_BUILD)
		P_LC002_BUILD:
			var parts_a := _make_box_body(PhysicsServer3D.BODY_MODE_RIGID, Vector3.ZERO, 0.2)
			_joint_body_a = parts_a[0]
			_joint_shape_a = parts_a[1]
			var parts_b := _make_box_body(PhysicsServer3D.BODY_MODE_RIGID, Vector3.ZERO, 0.2)
			_joint_body_b = parts_b[0]
			_joint_shape_b = parts_b[1]
			PhysicsServer3D.body_add_collision_exception(_joint_body_a, _joint_body_b)
			PhysicsServer3D.body_add_collision_exception(_joint_body_b, _joint_body_a)
			_joint = PhysicsServer3D.joint_create()
			PhysicsServer3D.joint_make_pin(_joint, _joint_body_a, Vector3.ZERO, _joint_body_b, Vector3.ZERO)
			PhysicsServer3D.joint_set_solver_priority(_joint, 7)
			_priority_before = PhysicsServer3D.joint_get_solver_priority(_joint)
			_joint_max_distance = 0.0
			_enter(P_LC002_SETTLE)
		P_LC002_SETTLE:
			if _ticks >= SETTLE_TICKS:
				_enter(P_LC002_FLIP_STATIC)
		P_LC002_FLIP_STATIC:
			# Crosses the static/dynamic boundary: full actor recreation.
			PhysicsServer3D.body_set_mode(_joint_body_a, PhysicsServer3D.BODY_MODE_STATIC)
			_enter(P_LC002_SETTLE_STATIC)
		P_LC002_SETTLE_STATIC:
			if _ticks >= SETTLE_TICKS:
				PhysicsServer3D.body_set_mode(_joint_body_a, PhysicsServer3D.BODY_MODE_RIGID)
				_enter(P_LC002_FLIP_BACK)
		P_LC002_FLIP_BACK:
			if _ticks >= SETTLE_TICKS:
				_enter(P_LC002_VERIFY)
		P_LC002_VERIFY:
			var origin_a: Vector3 = PhysicsServer3D.body_get_state(_joint_body_a, PhysicsServer3D.BODY_STATE_TRANSFORM).origin
			var origin_b: Vector3 = PhysicsServer3D.body_get_state(_joint_body_b, PhysicsServer3D.BODY_STATE_TRANSFORM).origin
			_joint_max_distance = maxf(_joint_max_distance, origin_a.distance_to(origin_b))
			if _ticks >= SETTLE_TICKS + JOINT_VERIFY_TICKS:
				var priority_after: int = PhysicsServer3D.joint_get_solver_priority(_joint)
				var ok := _joint_max_distance < 0.25 and priority_after == _priority_before
				var messages: Array = []
				if _joint_max_distance >= 0.25:
					messages.append("joint did not hold origins together through the mode flip (max distance %f)" % _joint_max_distance)
				if priority_after != _priority_before:
					messages.append("solver priority lost across actor recreation")
				_record("PHYSX-LIFEC-002", ok, messages)
				_free_all([_joint, _joint_body_a, _joint_shape_a, _joint_body_b, _joint_shape_b])
				_enter(P_LC003_SETUP)
		P_LC003_SETUP:
			if _async_iteration == 0:
				# Async stepping: step() kicks the solve and returns; idle
				# frames then run inside the simulate->fetch window, which is
				# exactly where LC-003 frees the body.
				ProjectSettings.set_setting("physics/physx_3d/simulation/async_step", true)
				print("[lifecycle] async_step forced on")
			_setup_overlap()
			_enter(P_LC003_WAIT_ENTER)
		P_LC003_WAIT_ENTER:
			if _entered:
				# The free itself happens in _process (idle frame) -- i.e.
				# while the solve kicked this tick is still in flight.
				_enter(P_LC003_FREE_IN_WINDOW)
			elif _timed_out():
				_record("PHYSX-LIFEC-003", false,
						["iteration %d: body_entered never fired" % _async_iteration])
				_advance_async()
		P_LC003_WAIT_EXIT:
			if _exited:
				_advance_async()
			elif _timed_out():
				_record("PHYSX-LIFEC-003", false,
						["iteration %d: body_exited did not fire after free-mid-solve" % _async_iteration])
				_advance_async()
		P_LC004_BUILD:
			_setup_overlap()
			# Swap the callback for the self-freeing one (contract under test).
			PhysicsServer3D.area_set_monitor_callback(_monitor_area,
					Callable(self, "_on_monitor_lc004"))
			_enter(P_LC004_WAIT_ENTER)
		P_LC004_WAIT_ENTER:
			# The callback frees the area on the first event; the tick after
			# that the flush loop has already walked past the freed dispatch.
			if _ticks >= SETTLE_TICKS:
				_enter(P_LC004_QUIESCE)
		P_LC004_QUIESCE:
			# Survival is the assertion: reaching here means the flush loop,
			# the ongoing simulation and the frame loop all survived an area
			# freed mid-dispatch. run_suite.py additionally treats a missing
			# final report as CRASH, so reaching _finish with final:true is
			# the observable pass condition.
			if _ticks >= SETTLE_TICKS:
				_record("PHYSX-LIFEC-004", true, [])
				_enter(P_DONE)
		P_DONE:
			pass
	if _phase == P_DONE:
		_finish()
		return true
	return false

func _advance_async() -> void:
	_async_iteration += 1
	_free_all([_monitor_area, _monitor_area_shape])
	if _async_iteration >= ASYNC_ITERATIONS:
		# Record the pass only when no iteration already recorded a failure:
		# _record() replaces the entry, so an unconditional pass here would
		# overwrite a timeout failure from WAIT_ENTER/WAIT_EXIT and report a
		# broken async free path as PASS.
		if not _results.has("PHYSX-LIFEC-003"):
			_record("PHYSX-LIFEC-003", true, [])
		ProjectSettings.set_setting("physics/physx_3d/simulation/async_step", false)
		_enter(P_LC004_BUILD)
	else:
		_enter(P_LC003_SETUP)

func _process(_delta: float) -> bool:
	if _phase == P_LC003_FREE_IN_WINDOW:
		# Idle frame == the simulate->fetch window in async mode. Freeing here
		# queues the actor release until after the fetch.
		_free_all([_monitor_body, _monitor_body_shape])
		_enter(P_LC003_WAIT_EXIT)
		return false
	if _phase == P_DONE:
		_finish()
		return true
	return false

func _finish() -> void:
	var tests: Array = []
	for id in ["PHYSX-LIFEC-001", "PHYSX-LIFEC-002", "PHYSX-LIFEC-003", "PHYSX-LIFEC-004"]:
		var r: Dictionary = _results.get(id, {"status": "fail", "messages": ["not reached"], "assertions": 1})
		tests.append({
			"id": id,
			"category": "lifecycle",
			"description": id,
			"status": r["status"],
			"assertions": r["assertions"],
			"failures": r["messages"].size(),
			"messages": r["messages"],
		})
	var ok := true
	for t in tests:
		if t["status"] != "pass":
			ok = false
	var result := {
		"suite": "lifecycle",
		"final": true,
		"tests": tests,
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE) if not _json_path.is_empty() else null
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	elif not _json_path.is_empty():
		push_error("[lifecycle] could not write JSON report to %s" % _json_path)
	for t in tests:
		print("[lifecycle] %s: %s%s" % [t["id"], t["status"],
				(" -- " + str(t["messages"])) if t["messages"].size() > 0 else ""])
	print("[lifecycle] %s" % ("PASS (all 4 contracts)" if ok else "FAIL"))
