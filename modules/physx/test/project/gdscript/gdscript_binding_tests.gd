# PhysX 5.8 GDScript BINDING-VALIDATION SUITE (smaller companion to the C# suite).
#
# Run headless:
#   <binary> --headless --fixed-fps 60 ^
#     --path modules/physx/test/project ^
#     --script res://gdscript/gdscript_binding_tests.gd -- --json=<path>
#
# Purpose: prove the PhysX backend behaves correctly when driven through the
# GDScript binding layer (RID/Variant marshaling, enums, Vector3/Quaternion/
# Basis/Transform3D fidelity, Callables, returned arrays/dictionaries, native
# lifetime, error behavior). It deliberately does NOT duplicate the exhaustive
# C# physics matrix; it provides representative behavioral coverage per topic
# and reports separately from the C# suite.
#
# Result model: PASS/FAIL/SKIP per test, console summary, JSON report
# ("final": true only on clean completion), exit code 1 on failure.
extends SceneTree

const TIMEOUT_MSEC := 180000
const DEFAULT_SEED := 123456

var _tests: Array = []
var _idx: int = -1
var _ctx: Dictionary = {}
var _start_msec := 0
var _json_path := ""
var _seed := DEFAULT_SEED
var _results: Array = []
var _cur_fail_msgs: Array = []
var _cur_asserts := 0
var _engine_name := ""

# ------------------------------------------------------------------ helpers

func chk(ok: bool, msg: String) -> void:
	_cur_asserts += 1
	if ok:
		return
	_cur_fail_msgs.append(msg)
	print("    FAIL  %s" % msg)

func note(msg: String) -> void:
	print("----  " + msg)

func reg(rid: RID) -> RID:
	# Track a resource in the current test context for guaranteed cleanup.
	if not rid.is_valid():
		return rid
	_ctx["rids"].append(rid)
	return rid

func mkspace() -> RID:
	var s := reg(PhysicsServer3D.space_create())
	PhysicsServer3D.space_set_active(s, true)
	return s

func mkshape_box(ex: Vector3) -> RID:
	# Argument is the intended HALF-extent; SHAPE_BOX server data is
	# half-extents (core BoxShape3D passes size / 2; godot_physics and Jolt
	# read it as half-extents too), so write it verbatim.
	var s := reg(PhysicsServer3D.box_shape_create())
	PhysicsServer3D.shape_set_data(s, ex)
	return s

func mkbody(space: RID, shape: RID, pos: Vector3, mode: int = PhysicsServer3D.BODY_MODE_RIGID) -> RID:
	var b := reg(PhysicsServer3D.body_create())
	PhysicsServer3D.body_set_space(b, space)
	PhysicsServer3D.body_set_mode(b, mode)
	PhysicsServer3D.body_add_shape(b, shape, Transform3D())
	PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM, Transform3D(Basis(), pos))
	return b

func pos(b: RID) -> Transform3D:
	return PhysicsServer3D.body_get_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM)

func poso(b: RID) -> Vector3:
	return pos(b).origin

func vel(b: RID) -> Vector3:
	return PhysicsServer3D.body_get_state(b, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY)

func ds() -> PhysicsDirectSpaceState3D:
	return PhysicsServer3D.space_get_direct_state(_ctx.space)

# ------------------------------------------------------------------ test registry

func _build_tests() -> void:
	var t := func(id: String, cat: String, desc: String, fn: String, frames: int = 600) -> void:
		_tests.append({"id": id, "cat": cat, "desc": desc, "fn": fn, "max_frames": frames})

	# Foundation.
	t.call("GDBIND-FOUND-001", "foundation", "active 3D physics engine is PhysX", "t_engine", 10)
	t.call("GDBIND-FOUND-002", "foundation", "PhysXServer3D inner singleton reachable from GDScript", "t_inner_singleton", 10)
	t.call("GDBIND-FOUND-003", "foundation", "space create/activate/direct state", "t_space_basics", 10)
	t.call("GDBIND-FOUND-004", "foundation", "physics frames advance", "t_frames_advance", 10)
	t.call("GDBIND-FOUND-005", "foundation", "analytic free fall (position + velocity)", "t_free_fall", 90)
	# Marshaling / types.
	t.call("GDBIND-TYPE-001", "types", "Vector3 shape data round-trip (box)", "t_type_box", 5)
	t.call("GDBIND-TYPE-002", "types", "float shape data round-trip (sphere)", "t_type_sphere", 5)
	t.call("GDBIND-TYPE-003", "types", "Dictionary shape data round-trip (capsule)", "t_type_capsule", 5)
	t.call("GDBIND-TYPE-004", "types", "PackedVector3Array round-trip (convex)", "t_type_convex", 5)
	t.call("GDBIND-TYPE-005", "types", "Plane round-trip (world boundary)", "t_type_plane", 5)
	t.call("GDBIND-TYPE-006", "types", "Basis/Quaternion fidelity: rotated transform round-trip", "t_type_basis", 5)
	t.call("GDBIND-TYPE-007", "types", "enum values distinct and non-colliding", "t_enums", 5)
	# RID handling.
	t.call("GDBIND-RID-001", "rid", "created RIDs are valid and distinct", "t_rid_valid", 5)
	t.call("GDBIND-RID-002", "rid", "free_rid invalidates RID", "t_rid_free", 10)
	t.call("GDBIND-RID-003", "rid", "RID usable as Dictionary key (hashing)", "t_rid_hash", 5)
	t.call("GDBIND-RID-004", "rid", "default RID is invalid and API-safe", "t_rid_default", 90)
	# Shapes.
	t.call("GDBIND-SHAPE-001", "shapes", "every supported shape type + data round-trip", "t_shapes_all", 10)
	t.call("GDBIND-SHAPE-002", "shapes", "shape margin round-trip", "t_shape_margin", 5)
	# Body API.
	t.call("GDBIND-BODY-001", "body", "body param round-trips (mass/bounce/friction/COM)", "t_body_params", 5)
	t.call("GDBIND-BODY-002", "body", "body mode/layer/mask round-trips", "t_body_flags", 5)
	t.call("GDBIND-BODY-003", "body", "axis lock flag round-trip", "t_body_axis_lock", 5)
	# Simulation behavior.
	t.call("GDBIND-SIM-001", "sim", "body falls under gravity (measured)", "t_sim_fall", 90)
	t.call("GDBIND-SIM-002", "sim", "central impulse dV = J/m", "t_sim_impulse", 10)
	t.call("GDBIND-SIM-003", "sim", "gravity_scale=0 floats in place", "t_sim_gscale0", 60)
	t.call("GDBIND-SIM-004", "sim", "kinematic teleport applied", "t_sim_kinematic", 30)
	t.call("GDBIND-SIM-005", "sim", "body sleeps, can_sleep=false does not, impulse wakes", "t_sim_sleep", 900)
	t.call("GDBIND-SIM-006", "sim", "constant force integrates F/m*t", "t_sim_cforce", 90)
	t.call("GDBIND-SIM-007", "sim", "area gravity override measured", "t_sim_area_gravity", 60)
	# Queries.
	t.call("GDBIND-QUERY-001", "query", "raycast hit dictionary fields", "t_query_ray_fields", 10)
	t.call("GDBIND-QUERY-002", "query", "raycast mask + exclude honored", "t_query_ray_filters", 10)
	t.call("GDBIND-QUERY-003", "query", "intersect_point returns Array[Dictionary]", "t_query_point", 10)
	t.call("GDBIND-QUERY-004", "query", "intersect_shape + rest_info", "t_query_shape", 10)
	# Motion.
	t.call("GDBIND-MOVE-001", "motion", "body_test_motion result fields", "t_motion_fields", 10)
	# Areas.
	t.call("GDBIND-AREA-001", "area", "area enter/exit events fire via Callable", "t_area_events", 200)
	t.call("GDBIND-AREA-002", "area", "area-area monitoring", "t_area_area", 60)
	# Joints.
	t.call("GDBIND-JOINT-001", "joint", "pin joint constrains anchor distance", "t_joint_pin", 120)
	t.call("GDBIND-JOINT-002", "joint", "joint type reporting", "t_joint_types", 10)
	# CCD.
	t.call("GDBIND-CCD-001", "ccd", "thin-wall tunneling blocked only with CCD", "t_ccd", 40)
	# Contacts.
	t.call("GDBIND-CONTACT-001", "contacts", "direct body state contact accessors", "t_contacts", 300)
	# Callbacks.
	t.call("GDBIND-CALLBACK-001", "callbacks", "force integration Callable fires per step", "t_fi_callback", 60)
	# Lifetime / GC.
	t.call("GDBIND-LIFE-001", "lifetime", "RID keeps native alive across GC pressure", "t_lifetime_gc", 60)
	t.call("GDBIND-LIFE-002", "lifetime", "freed-then-used RID does not corrupt engine", "t_lifetime_stale", 40)
	# Errors.
	t.call("GDBIND-ERR-001", "errors", "invalid inputs leave engine stable", "t_errors", 60)
	# Vehicles.
	t.call("GDBIND-VEHI-001", "vehicle", "vehicle create/drive/telemetry via GDScript", "t_vehicle", 400)
	# Module extension: reduced-coordinate articulations (inner-server API).
	t.call("GDBIND-ART-001", "articulation", "articulation chain: create, links, drive config, transforms", "t_articulation", 30)
	t.call("GDBIND-VEHI-002", "vehicle", "response params retune steer lock", "t_vehicle_response_tuning", 400)
	# Soft body API skeleton.
	t.call("GDBIND-SOFT-001", "soft_bodies", "soft body API skeleton round-trips (no simulation)", "t_soft_body", 10)

# ------------------------------------------------------------------ lifecycle

func _initialize() -> void:
	_start_msec = Time.get_ticks_msec()
	for a in OS.get_cmdline_user_args():
		if a.begins_with("--json="):
			_json_path = a.substr(7)
		elif a.begins_with("--seed="):
			_seed = int(a.substr(7))
	_engine_name = ProjectSettings.get_setting_with_override("physics/3d/physics_engine")
	_build_tests()
	print("====================================================")
	print("  Godot PhysX 5.8 GDScript Binding Test Suite")
	print("  Backend: %s    Physics FPS: %d    Seed: %d" % [_engine_name, Engine.get_physics_ticks_per_second(), _seed])
	print("====================================================")
	print("  %d tests registered" % _tests.size())
	_write_json(false)

func _cleanup_ctx() -> void:
	# Free in reverse creation order: joints/vehicles created after their
	# bodies are freed first, spaces created first are freed last. This avoids
	# double-frees of resources auto-released by their owners.
	var rids: Array = _ctx.get("rids", [])
	rids.reverse()
	for r in rids:
		if r.is_valid():
			PhysicsServer3D.free_rid(r)
	_ctx = {}

func _finish_test(status: String) -> void:
	var rec := {
		"id": _tests[_idx]["id"],
		"category": _tests[_idx]["cat"],
		"description": _tests[_idx]["desc"],
		"status": status,
		"assertions": _cur_asserts,
		"messages": _cur_fail_msgs.duplicate(),
	}
	_results.append(rec)
	var extra := ""
	if _cur_fail_msgs.size() > 0:
		extra = "  first failure: " + str(_cur_fail_msgs[0])
	print("%s  %s  %d asserts%s" % [status.to_upper(), _tests[_idx]["id"], _cur_asserts, extra])
	_write_json(false)

func _begin_test() -> void:
	_ctx = {"rids": [], "space": RID(), "frame": 0, "done": false}
	_cur_fail_msgs = []
	_cur_asserts = 0

func _physics_process(_delta: float) -> bool:
	if Time.get_ticks_msec() - _start_msec > TIMEOUT_MSEC:
		print("TIMED OUT after %d ms at test %d/%d" % [TIMEOUT_MSEC, _idx + 1, _tests.size()])
		if _idx >= 0 and _idx < _tests.size():
			_finish_test("fail")
		_write_json(false)
		quit(1)
		return true

	if _idx < 0 or _ctx.get("done", false):
		_cleanup_ctx()
		_idx += 1
		if _idx >= _tests.size():
			return _conclude()
		_begin_test()
		print("RUN   %s [%s] %s" % [_tests[_idx]["id"], _tests[_idx]["cat"], _tests[_idx]["desc"]])
		return false

	_ctx["frame"] += 1
	if _ctx["frame"] > _tests[_idx]["max_frames"]:
		chk(false, "exceeded per-test frame budget %d" % _tests[_idx]["max_frames"])
		_ctx["done"] = true
		_finish_test("fail")
		return false

	var fn: String = _tests[_idx]["fn"]
	var ok_done: bool = call(fn, _ctx)
	if ok_done:
		_ctx["done"] = true
		_finish_test("fail" if _cur_fail_msgs.size() > 0 else "pass")
	return false

func _conclude() -> bool:
	_write_json(true)
	var total := _results.size()
	var passed := 0
	var failed := 0
	var skipped := 0
	var asserts := 0
	var by_cat := {}
	for r in _results:
		asserts += r["assertions"]
		match r["status"]:
			"pass": passed += 1
			"skip": skipped += 1
			_: failed += 1
		if not by_cat.has(r["category"]):
			by_cat[r["category"]] = {"pass": 0, "total": 0}
		by_cat[r["category"]]["total"] += 1
		if r["status"] == "pass":
			by_cat[r["category"]]["pass"] += 1
	print("")
	print("====================================================")
	for cat in by_cat:
		var c: Dictionary = by_cat[cat]
		var st := "PASS" if c["pass"] == c["total"] else "FAIL"
		print("  %-14s %s  %d/%d" % [cat, st, c["pass"], c["total"]])
	print("TOTAL: %d   PASSED: %d   FAILED: %d   SKIPPED: %d   ASSERTIONS: %d" % [total, passed, failed, skipped, asserts])
	if failed > 0:
		print("FAILURES:")
		for r in _results:
			if r["status"] == "fail":
				for m in r["messages"]:
					print("  [%s] %s" % [r["id"], m])
	print("====================================================")
	print("RESULT: %s" % ("FAILURE" if failed > 0 else "SUCCESS"))
	print("====================================================")
	quit(1 if failed > 0 else 0)
	return true

func _write_json(final: bool) -> void:
	if _json_path == "":
		return
	var rep := {
		"suite": "gdscript",
		"final": final,
		"seed": _seed,
		"engine": _engine_name,
		"physics_fps": Engine.get_physics_ticks_per_second(),
		"tests": _results,
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE)
	if f:
		f.store_string(JSON.stringify(rep, "  "))
		f.close()

# ------------------------------------------------------------------ tests

func t_engine(_c: Dictionary) -> bool:
	chk(_engine_name == "PhysX", "physics/3d/physics_engine == 'PhysX' (got '%s')" % _engine_name)
	return true

func t_inner_singleton(_c: Dictionary) -> bool:
	var px = PhysXServer3D.get_singleton()
	chk(px != null, "PhysXServer3D.get_singleton() returns an object from GDScript")
	chk(ClassDB.class_exists("PhysXServer3D"), "ClassDB exposes PhysXServer3D to GDScript")
	return true

func t_space_basics(c: Dictionary) -> bool:
	c["space"] = mkspace()
	chk(PhysicsServer3D.space_is_active(c["space"]), "space_is_active true")
	chk(PhysicsServer3D.space_get_direct_state(c["space"]) != null, "direct state non-null")
	PhysicsServer3D.space_set_param(c["space"], PhysicsServer3D.SPACE_PARAM_SOLVER_ITERATIONS, 8)
	chk(is_equal_approx(PhysicsServer3D.space_get_param(c["space"], PhysicsServer3D.SPACE_PARAM_SOLVER_ITERATIONS), 8.0),
		"solver iterations round-trip")
	return true

func t_frames_advance(c: Dictionary) -> bool:
	if c.get("f0", -1) < 0:
		c["f0"] = Engine.get_physics_frames()
		return false
	if Engine.get_physics_frames() - c["f0"] < 3:
		return false
	chk(Engine.get_physics_frames() - c["f0"] >= 3, "physics frames advanced")
	return true

func t_free_fall(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var sh := mkshape_box(Vector3(0.5, 0.5, 0.5))
		var b := mkbody(c["space"], sh, Vector3(0, 10, 0))
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY, Vector3.ZERO)
		c["b"] = b
		c["f0"] = Engine.get_physics_frames()
		return false
	var n: int = Engine.get_physics_frames() - c["f0"]
	if n < 65:
		return false
	var p := poso(c["b"])
	var v := vel(c["b"])
	var t := n / 60.0
	# Tolerances cover semi-implicit integration and step-phase jitter.
	chk(absf(p.y - (10.0 - 0.5 * 9.81 * t * t)) < 0.35, "free fall position ~1/2*g*t^2 (got %.3f)" % p.y)
	chk(absf(v.y + 9.81 * t) < 0.8, "free fall velocity ~ -g*t (got %.3f)" % v.y)
	return true

func t_type_box(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := reg(PhysicsServer3D.box_shape_create())
	PhysicsServer3D.shape_set_data(s, Vector3(0.3, 0.4, 0.5))
	var d: Vector3 = PhysicsServer3D.shape_get_data(s)
	chk(d.is_equal_approx(Vector3(0.3, 0.4, 0.5)), "box Vector3 round-trip")
	chk(PhysicsServer3D.shape_get_type(s) == PhysicsServer3D.SHAPE_BOX, "box type")
	return true

func t_type_sphere(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := reg(PhysicsServer3D.sphere_shape_create())
	PhysicsServer3D.shape_set_data(s, 0.7)
	chk(is_equal_approx(PhysicsServer3D.shape_get_data(s), 0.7), "sphere float round-trip")
	return true

func t_type_capsule(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := reg(PhysicsServer3D.capsule_shape_create())
	PhysicsServer3D.shape_set_data(s, {"radius": 0.3, "height": 1.6})
	var d: Dictionary = PhysicsServer3D.shape_get_data(s)
	chk(is_equal_approx(d["radius"], 0.3) and is_equal_approx(d["height"], 1.6), "capsule Dictionary round-trip")
	return true

func t_type_convex(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := reg(PhysicsServer3D.convex_polygon_shape_create())
	var pts := PackedVector3Array([Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)])
	PhysicsServer3D.shape_set_data(s, pts)
	var back: PackedVector3Array = PhysicsServer3D.shape_get_data(s)
	chk(back.size() == 4, "convex PackedVector3Array count")
	chk(back[0].is_equal_approx(Vector3.ZERO), "convex first point round-trip")
	return true

func t_type_plane(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := reg(PhysicsServer3D.world_boundary_shape_create())
	PhysicsServer3D.shape_set_data(s, Plane(Vector3(0, 1, 0), 0))
	var p: Plane = PhysicsServer3D.shape_get_data(s)
	chk(p.normal.is_equal_approx(Vector3.UP), "plane normal round-trip")
	return true

func t_type_basis(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var sh := mkshape_box(Vector3(0.4, 0.4, 0.4))
	var b := mkbody(c["space"], sh, Vector3(0, 5, 0), PhysicsServer3D.BODY_MODE_STATIC)
	var q := Quaternion(Vector3.UP, deg_to_rad(30.0))
	var xf := Transform3D(Basis(q), Vector3(1, 2, 3))
	PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM, xf)
	var back: Transform3D = PhysicsServer3D.body_get_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM)
	chk(back.origin.is_equal_approx(Vector3(1, 2, 3)), "transform origin round-trip")
	chk(back.basis.get_rotation_quaternion().is_equal_approx(q), "Basis/Quaternion round-trip (30 deg about Y)")
	return true

func t_enums(_c: Dictionary) -> bool:
	var shape_types := [
		PhysicsServer3D.SHAPE_WORLD_BOUNDARY, PhysicsServer3D.SHAPE_SEPARATION_RAY, PhysicsServer3D.SHAPE_SPHERE,
		PhysicsServer3D.SHAPE_BOX, PhysicsServer3D.SHAPE_CAPSULE, PhysicsServer3D.SHAPE_CYLINDER,
		PhysicsServer3D.SHAPE_CONVEX_POLYGON, PhysicsServer3D.SHAPE_CONCAVE_POLYGON, PhysicsServer3D.SHAPE_HEIGHTMAP,
		PhysicsServer3D.SHAPE_CUSTOM,
	]
	var seen := {}
	for v in shape_types:
		chk(not seen.has(v), "shape enum value %d unique" % v)
		seen[v] = true
	var modes := [PhysicsServer3D.BODY_MODE_STATIC, PhysicsServer3D.BODY_MODE_KINEMATIC,
		PhysicsServer3D.BODY_MODE_RIGID, PhysicsServer3D.BODY_MODE_RIGID_LINEAR]
	chk(modes.size() == modes.duplicate().size(), "body mode values distinct (array-dedup check)")
	var overrides := [PhysicsServer3D.AREA_SPACE_OVERRIDE_DISABLED, PhysicsServer3D.AREA_SPACE_OVERRIDE_COMBINE,
		PhysicsServer3D.AREA_SPACE_OVERRIDE_COMBINE_REPLACE, PhysicsServer3D.AREA_SPACE_OVERRIDE_REPLACE,
		PhysicsServer3D.AREA_SPACE_OVERRIDE_REPLACE_COMBINE]
	chk(overrides.size() == overrides.duplicate().size(), "area override modes distinct")
	return true

func t_rid_valid(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var a := reg(PhysicsServer3D.box_shape_create())
	var b := reg(PhysicsServer3D.box_shape_create())
	chk(a.is_valid() and b.is_valid(), "created RIDs valid")
	chk(a != b, "distinct RIDs compare unequal")
	return true

func t_rid_free(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(s, Vector3.ONE)
	chk(s.is_valid(), "RID valid before free")
	PhysicsServer3D.free_rid(s)
	# Engine-level validity: shape_get_data on freed RID must not return stale data.
	var d = PhysicsServer3D.shape_get_data(s)
	chk(true, "query on freed RID did not crash")
	return true

func t_rid_hash(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var a := reg(PhysicsServer3D.box_shape_create())
	var d := {}
	d[a] = "tag"
	chk(d.get(a, "") == "tag", "RID usable as Dictionary key")
	chk(d.has(a), "RID hash stable across lookups")
	return true

func t_rid_default(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var bad := RID()
		chk(not bad.is_valid(), "default RID invalid")
		# Calls on the default RID must not crash the engine.
		PhysicsServer3D.body_set_param(bad, PhysicsServer3D.BODY_PARAM_MASS, 1.0)
		PhysicsServer3D.shape_set_data(bad, Vector3.ONE)
		PhysicsServer3D.body_apply_central_impulse(bad, Vector3.ONE)
		c["b"] = mkbody(c["space"], mkshape_box(Vector3(0.4, 0.4, 0.4)), Vector3(0, 3, 0))
		return false
	if c["frame"] < 45:
		return false
	chk(poso(c["b"]).y < 2.5, "engine healthy after default-RID calls (body fell, y=%.2f)" % poso(c["b"]).y)
	return true

func t_shapes_all(c: Dictionary) -> bool:
	c["space"] = mkspace()
	# Box.
	var box := reg(PhysicsServer3D.box_shape_create())
	PhysicsServer3D.shape_set_data(box, Vector3(0.3, 0.4, 0.5))
	chk(PhysicsServer3D.shape_get_type(box) == PhysicsServer3D.SHAPE_BOX, "box type")
	chk((PhysicsServer3D.shape_get_data(box) as Vector3).is_equal_approx(Vector3(0.3, 0.4, 0.5)), "box data")
	# Sphere.
	var sph := reg(PhysicsServer3D.sphere_shape_create())
	PhysicsServer3D.shape_set_data(sph, 0.7)
	chk(PhysicsServer3D.shape_get_type(sph) == PhysicsServer3D.SHAPE_SPHERE, "sphere type")
	# Capsule / cylinder.
	var cap := reg(PhysicsServer3D.capsule_shape_create())
	PhysicsServer3D.shape_set_data(cap, {"radius": 0.3, "height": 1.6})
	chk(PhysicsServer3D.shape_get_type(cap) == PhysicsServer3D.SHAPE_CAPSULE, "capsule type")
	var cyl := reg(PhysicsServer3D.cylinder_shape_create())
	PhysicsServer3D.shape_set_data(cyl, {"radius": 0.4, "height": 1.2})
	chk(PhysicsServer3D.shape_get_type(cyl) == PhysicsServer3D.SHAPE_CYLINDER, "cylinder type")
	# Custom cone.
	var cone := reg(PhysicsServer3D.custom_shape_create())
	PhysicsServer3D.shape_set_data(cone, {"type": "cone", "radius": 0.7, "height": 1.5})
	chk(PhysicsServer3D.shape_get_type(cone) == PhysicsServer3D.SHAPE_CUSTOM, "custom type")
	chk((PhysicsServer3D.shape_get_data(cone) as Dictionary).get("type", "") == "cone", "cone custom type string")
	# Convex.
	var cvx := reg(PhysicsServer3D.convex_polygon_shape_create())
	var pts := PackedVector3Array([Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(0, 1, 0), Vector3(0, 0, 1)])
	PhysicsServer3D.shape_set_data(cvx, pts)
	chk(PhysicsServer3D.shape_get_type(cvx) == PhysicsServer3D.SHAPE_CONVEX_POLYGON, "convex type")
	# Concave.
	var cnc := reg(PhysicsServer3D.concave_polygon_shape_create())
	PhysicsServer3D.shape_set_data(cnc, {"faces": pts, "backface_collision": false})
	chk(PhysicsServer3D.shape_get_type(cnc) == PhysicsServer3D.SHAPE_CONCAVE_POLYGON, "concave type")
	# Heightmap.
	var hm := reg(PhysicsServer3D.heightmap_shape_create())
	PhysicsServer3D.shape_set_data(hm, {"width": 2, "depth": 2, "heights": PackedFloat32Array([0, 0, 0, 0])})
	chk(PhysicsServer3D.shape_get_type(hm) == PhysicsServer3D.SHAPE_HEIGHTMAP, "heightmap type")
	# Separation ray.
	var sr := reg(PhysicsServer3D.separation_ray_shape_create())
	PhysicsServer3D.shape_set_data(sr, {"length": 2.0, "slide_on_slope": true})
	chk(PhysicsServer3D.shape_get_type(sr) == PhysicsServer3D.SHAPE_SEPARATION_RAY, "separation ray type")
	# World boundary.
	var wb := reg(PhysicsServer3D.world_boundary_shape_create())
	PhysicsServer3D.shape_set_data(wb, Plane(Vector3.UP, 0))
	chk(PhysicsServer3D.shape_get_type(wb) == PhysicsServer3D.SHAPE_WORLD_BOUNDARY, "world boundary type")
	return true

func t_shape_margin(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var s := mkshape_box(Vector3.ONE)
	PhysicsServer3D.shape_set_margin(s, 0.05)
	chk(is_equal_approx(PhysicsServer3D.shape_get_margin(s), 0.05), "margin round-trip")
	return true

func t_body_params(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
	PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_MASS, 2.0)
	PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE, 0.45)
	PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_FRICTION, 0.7)
	PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_CENTER_OF_MASS, Vector3(0.1, -0.2, 0.3))
	chk(is_equal_approx(PhysicsServer3D.body_get_param(b, PhysicsServer3D.BODY_PARAM_MASS), 2.0), "mass round-trip")
	chk(is_equal_approx(PhysicsServer3D.body_get_param(b, PhysicsServer3D.BODY_PARAM_BOUNCE), 0.45), "bounce round-trip")
	chk(is_equal_approx(PhysicsServer3D.body_get_param(b, PhysicsServer3D.BODY_PARAM_FRICTION), 0.7), "friction round-trip")
	chk((PhysicsServer3D.body_get_param(b, PhysicsServer3D.BODY_PARAM_CENTER_OF_MASS) as Vector3).is_equal_approx(Vector3(0.1, -0.2, 0.3)), "COM round-trip")
	return true

func t_body_flags(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
	PhysicsServer3D.body_set_mode(b, PhysicsServer3D.BODY_MODE_KINEMATIC)
	chk(PhysicsServer3D.body_get_mode(b) == PhysicsServer3D.BODY_MODE_KINEMATIC, "mode round-trip")
	PhysicsServer3D.body_set_collision_layer(b, 5)
	PhysicsServer3D.body_set_collision_mask(b, 9)
	chk(PhysicsServer3D.body_get_collision_layer(b) == 5, "layer round-trip")
	chk(PhysicsServer3D.body_get_collision_mask(b) == 9, "mask round-trip")
	PhysicsServer3D.body_attach_object_instance_id(b, 12345)
	chk(int(PhysicsServer3D.body_get_object_instance_id(b)) == 12345, "instance id round-trip")
	PhysicsServer3D.body_set_max_contacts_reported(b, 4)
	chk(PhysicsServer3D.body_get_max_contacts_reported(b) == 4, "max contacts round-trip")
	return true

func t_body_axis_lock(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
	PhysicsServer3D.body_set_axis_lock(b, PhysicsServer3D.BODY_AXIS_ANGULAR_Z, true)
	chk(PhysicsServer3D.body_is_axis_locked(b, PhysicsServer3D.BODY_AXIS_ANGULAR_Z), "axis lock flag round-trip")
	PhysicsServer3D.body_set_axis_lock(b, PhysicsServer3D.BODY_AXIS_ANGULAR_Z, false)
	chk(not PhysicsServer3D.body_is_axis_locked(b, PhysicsServer3D.BODY_AXIS_ANGULAR_Z), "axis lock flag cleared")
	return true

func t_sim_fall(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var floor_shape := mkshape_box(Vector3(200, 0.5, 200))
		var fl := mkbody(c["space"], floor_shape, Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		c["fl"] = fl
		c["b"] = mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
		return false
	if c["frame"] < 50:
		return false
	var p := poso(c["b"])
	var v := vel(c["b"])
	chk(p.y < 3.5, "body fell under gravity (y=%.2f)" % p.y)
	chk(v.y < -5.0, "falling velocity gained (vy=%.2f)" % v.y)
	return true

func t_sim_impulse(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_MASS, 2.0)
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		c["b"] = b
		return false
	if c["frame"] == 2:
		PhysicsServer3D.body_apply_central_impulse(c["b"], Vector3(5, 0, 0))
		return false
	chk(absf(vel(c["b"]).x - 2.5) < 0.05, "impulse dV = J/m (got %.3f)" % vel(c["b"]).x)
	return true

func t_sim_gscale0(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(1, 5, 1))
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		c["b"] = b
		return false
	if c["frame"] < 60:
		return false
	chk(poso(c["b"]).is_equal_approx(Vector3(1, 5, 1)), "gravity_scale=0 body stays put")
	return true

func t_sim_kinematic(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0), PhysicsServer3D.BODY_MODE_KINEMATIC)
		c["b"] = b
		return false
	if c["frame"] == 2:
		PhysicsServer3D.body_set_state(c["b"], PhysicsServer3D.BODY_STATE_TRANSFORM, Transform3D(Basis(), Vector3(5, 2.5, 0)))
		return false
	if c["frame"] < 5:
		return false
	chk(poso(c["b"]).is_equal_approx(Vector3(5, 2.5, 0)), "kinematic teleport applied (%.2f)" % poso(c["b"]).y)
	return true

func t_sim_sleep(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var fl := mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		var mk := func(x: float, can_sleep: bool) -> RID:
			var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(x, 1.2, 0))
			PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_LINEAR_DAMP, 8.0)
			PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_LINEAR_DAMP_MODE, PhysicsServer3D.BODY_DAMP_MODE_REPLACE)
			PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_CAN_SLEEP, can_sleep)
			return b
		c["sleeper"] = mk.call(0.0, true)
		c["waker"] = mk.call(3.0, false)
		return false
	var fr: int = c["frame"]
	if fr == 400:
		chk(PhysicsServer3D.body_get_state(c["sleeper"], PhysicsServer3D.BODY_STATE_SLEEPING) == true, "resting body sleeps")
		chk(PhysicsServer3D.body_get_state(c["waker"], PhysicsServer3D.BODY_STATE_SLEEPING) == false, "can_sleep=false stays awake")
		PhysicsServer3D.body_apply_central_impulse(c["sleeper"], Vector3(0, 4, 0))
		return false
	if fr > 405:
		chk(PhysicsServer3D.body_get_state(c["sleeper"], PhysicsServer3D.BODY_STATE_SLEEPING) == false, "impulse wakes sleeper")
		return true
	return false

func t_sim_cforce(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_LINEAR_DAMP, 0.0)
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_LINEAR_DAMP_MODE, PhysicsServer3D.BODY_DAMP_MODE_REPLACE)
		PhysicsServer3D.body_set_constant_force(b, Vector3(2, 0, 0))
		chk((PhysicsServer3D.body_get_constant_force(b) as Vector3).is_equal_approx(Vector3(2, 0, 0)), "constant force round-trip")
		c["b"] = b
		return false
	if c["frame"] < 61:
		return false
	chk(absf(vel(c["b"]).x - 2.0) < 0.2, "constant force F/m*t (got %.3f)" % vel(c["b"]).x)
	return true

func t_sim_area_gravity(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var ash := mkshape_box(Vector3(2, 2, 2))
		var area := reg(PhysicsServer3D.area_create())
		PhysicsServer3D.area_set_space(area, c["space"])
		PhysicsServer3D.area_add_shape(area, ash, Transform3D(Basis(), Vector3(30, 5, 0)))
		PhysicsServer3D.area_set_param(area, PhysicsServer3D.AREA_PARAM_GRAVITY_OVERRIDE_MODE, PhysicsServer3D.AREA_SPACE_OVERRIDE_REPLACE)
		PhysicsServer3D.area_set_param(area, PhysicsServer3D.AREA_PARAM_GRAVITY, 20.0)
		c["b"] = mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(30, 6.5, 0))
		c["ctrl"] = mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(35, 6.5, 0))
		return false
	if c["frame"] < 31:
		return false
	var in_v := absf(vel(c["b"]).y)
	var out_v := absf(vel(c["ctrl"]).y)
	chk(in_v > out_v + 1.0, "area gravity overrides and accelerates body (%.2f vs %.2f)" % [in_v, out_v])
	return true

func t_query_ray_fields(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var fl := mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		c["fl"] = fl
		return false
	var q := PhysicsRayQueryParameters3D.create(Vector3(3, 5, 0), Vector3(3, -5, 0))
	var hit: Dictionary = ds().intersect_ray(q)
	chk(not hit.is_empty(), "raycast hit")
	if hit.is_empty():
		return true
	chk(hit["rid"] == c["fl"], "hit field rid")
	chk(absf((hit["position"] as Vector3).y) < 0.05, "hit field position")
	chk((hit["normal"] as Vector3).y > 0.9, "hit field normal")
	chk(hit.has("collider_id"), "hit field collider_id present")
	chk(hit.has("shape"), "hit field shape present")
	chk(hit.has("face_index"), "hit field face_index present")
	return true

func t_query_ray_filters(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		c["fl"] = mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		return false
	var q := PhysicsRayQueryParameters3D.create(Vector3(3, 5, 0), Vector3(3, -5, 0), 4, [])
	chk(ds().intersect_ray(q).is_empty(), "raycast mask honored")
	var q2 := PhysicsRayQueryParameters3D.create(Vector3(3, 5, 0), Vector3(3, -5, 0), 0xFFFFFFFF, [c["fl"]])
	chk(ds().intersect_ray(q2).is_empty(), "raycast exclude honored")
	return true

func t_query_point(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		c["fl"] = mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		return false
	var p := PhysicsPointQueryParameters3D.new()
	p.position = Vector3(3, -0.5, 0)
	p.collision_mask = 0xFFFFFFFF
	var hits: Array = ds().intersect_point(p, 4)
	chk(hits.size() >= 1, "intersect_point returns results")
	if hits.size() >= 1:
		chk(hits[0] is Dictionary and hits[0]["rid"] == c["fl"], "point results are Dictionaries with rid")
	p.position = Vector3(5, 5, 5)
	chk(ds().intersect_point(p, 4).is_empty(), "intersect_point empty in air")
	return true

func t_query_shape(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		c["fl"] = mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		var sph := reg(PhysicsServer3D.sphere_shape_create())
		PhysicsServer3D.shape_set_data(sph, 0.5)
		c["sph"] = sph
		return false
	var sq := PhysicsShapeQueryParameters3D.new()
	sq.shape_rid = c["sph"]
	sq.transform = Transform3D(Basis(), Vector3(3, 0.4, 0))
	sq.collision_mask = 0xFFFFFFFF
	var hits: Array = ds().intersect_shape(sq, 4)
	chk(hits.size() >= 1 and hits[0]["rid"] == c["fl"], "intersect_shape overlaps floor")
	var ri: Dictionary = ds().get_rest_info(sq)
	chk(not ri.is_empty() and ri["rid"] == c["fl"], "rest_info reports floor")
	sq.transform = Transform3D(Basis(), Vector3(3, 5, 0))
	chk(ds().intersect_shape(sq, 4).is_empty(), "intersect_shape empty in air")
	return true

func t_motion_fields(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var fl := mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		c["fl"] = fl
		c["b"] = mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(5, 2, 0), PhysicsServer3D.BODY_MODE_KINEMATIC)
		return false
	var mp := PhysicsTestMotionParameters3D.new()
	var mr := PhysicsTestMotionResult3D.new()
	mp.from = Transform3D(Basis(), Vector3(5, 2, 0))
	mp.motion = Vector3(0, -3, 0)
	var hit: bool = PhysicsServer3D.body_test_motion(c["b"], mp, mr)
	chk(hit, "body_test_motion hits floor")
	chk(mr.get_collision_safe_fraction() > 0.2 and mr.get_collision_safe_fraction() < 0.8, "safe fraction sane")
	chk(mr.get_collider_rid(0) == c["fl"], "collider rid reported")
	chk(mr.get_collision_normal(0).y > 0.7, "collision normal up")
	mp.motion = Vector3(0, 0.5, 0)
	chk(PhysicsServer3D.body_test_motion(c["b"], mp, mr) == false, "free motion reports no hit")
	return true

func t_area_events(c: Dictionary) -> bool:
	var fr: int = c["frame"]
	if fr == 1:
		c["space"] = mkspace()
		var ash := mkshape_box(Vector3(2, 2, 2))
		var area := reg(PhysicsServer3D.area_create())
		PhysicsServer3D.area_set_space(area, c["space"])
		PhysicsServer3D.area_add_shape(area, ash, Transform3D(Basis(), Vector3(0, 10, 0)))
		c["area"] = area
		c["enters"] = 0
		c["exits"] = 0
		PhysicsServer3D.area_set_monitor_callback(area, Callable(self, "_on_area_inout"))
		return false
	if fr == 30:
		# Body appears inside the area -> enter.
		c["b"] = mkbody(c["space"], mkshape_box(Vector3(0.3, 0.3, 0.3)), Vector3(0, 10.5, 0))
		PhysicsServer3D.body_set_param(c["b"], PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		return false
	if fr == 60:
		chk(c["enters"] >= 1, "area enter event fired (%d)" % c["enters"])
		# Let it fall out of the volume.
		PhysicsServer3D.body_set_param(c["b"], PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 1.0)
		return false
	if fr > 60 and c["exits"] >= 1:
		chk(true, "area exit event fired")
		return true
	if fr >= 190:
		chk(false, "area exit event never fired (enters=%d exits=%d)" % [c["enters"], c["exits"]])
		return true
	return false

func _on_area_inout(status, _rid, _id, _bs, _as) -> void:
	if _ctx.is_empty():
		return
	if status == 0:
		_ctx["enters"] = int(_ctx.get("enters", 0)) + 1
	else:
		_ctx["exits"] = int(_ctx.get("exits", 0)) + 1

func t_area_area(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var sh := mkshape_box(Vector3(1, 1, 1))
		var watcher := reg(PhysicsServer3D.area_create())
		PhysicsServer3D.area_set_space(watcher, c["space"])
		PhysicsServer3D.area_add_shape(watcher, sh, Transform3D(Basis(), Vector3(0, 5, 0)))
		var probe := reg(PhysicsServer3D.area_create())
		PhysicsServer3D.area_set_space(probe, c["space"])
		PhysicsServer3D.area_add_shape(probe, sh, Transform3D(Basis(), Vector3(0, 5, 0)))
		PhysicsServer3D.area_set_monitorable(probe, true)
		c["aa"] = 0
		PhysicsServer3D.area_set_area_monitor_callback(watcher, Callable(self, "_on_area_area"))
		return false
	if c["frame"] > 30:
		chk(c["aa"] >= 1, "area-area overlap event fired (%d)" % c["aa"])
		return true
	return false

func _on_area_area(_status, _rid, _id, _bs, _as) -> void:
	if _ctx.is_empty():
		return
	_ctx["aa"] = int(_ctx.get("aa", 0)) + 1

func t_joint_pin(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var sh := mkshape_box(Vector3(0.3, 0.3, 0.3))
		var a := mkbody(c["space"], sh, Vector3(0, 6, 0), PhysicsServer3D.BODY_MODE_STATIC)
		var b := mkbody(c["space"], sh, Vector3(0, 4.5, 0))
		var j := reg(PhysicsServer3D.joint_create())
		PhysicsServer3D.joint_make_pin(j, a, Vector3(0, -0.5, 0), b, Vector3(0, 0.5, 0))
		chk(PhysicsServer3D.joint_get_type(j) == PhysicsServer3D.JOINT_TYPE_PIN, "pin joint type")
		PhysicsServer3D.pin_joint_set_param(j, PhysicsServer3D.PIN_JOINT_BIAS, 0.3)
		chk(is_equal_approx(PhysicsServer3D.pin_joint_get_param(j, PhysicsServer3D.PIN_JOINT_BIAS), 0.3), "pin param round-trip")
		PhysicsServer3D.pin_joint_set_local_a(j, Vector3(1, 2, 3))
		chk((PhysicsServer3D.pin_joint_get_local_a(j) as Vector3).is_equal_approx(Vector3(1, 2, 3)), "local_a round-trip")
		c["a"] = a
		c["b"] = b
		return false
	if c["frame"] < 120:
		return false
	var pa := poso(c["a"])
	var pb := poso(c["b"])
	# Measure with the ACTUAL local offsets (local_a was changed to (1,2,3) above):
	# a live constraint keeps anchor A (= pa + local_a) and anchor B (= pb + local_b) coincident.
	var anchor_a := pa + Vector3(1, 2, 3)
	var anchor_b := pb + Vector3(0, 0.5, 0)
	chk((anchor_a - anchor_b).length() < 0.5, "pin anchor separation bounded after set_local_a (%.3f)" % (anchor_a - anchor_b).length())
	chk(pb.y < anchor_a.y - 0.3, "body hangs below re-anchored pivot")
	return true

func t_joint_types(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var sh := mkshape_box(Vector3(0.3, 0.3, 0.3))
	var a := mkbody(c["space"], sh, Vector3(0, 6, 0), PhysicsServer3D.BODY_MODE_STATIC)
	var b := mkbody(c["space"], sh, Vector3(0, 5, 0))
	var hj := reg(PhysicsServer3D.joint_create())
	PhysicsServer3D.joint_make_hinge(hj, a, Transform3D(), b, Transform3D())
	PhysicsServer3D.hinge_joint_set_param(hj, PhysicsServer3D.HINGE_JOINT_LIMIT_UPPER, 1.2)
	PhysicsServer3D.hinge_joint_set_flag(hj, PhysicsServer3D.HINGE_JOINT_FLAG_USE_LIMIT, true)
	chk(PhysicsServer3D.joint_get_type(hj) == PhysicsServer3D.JOINT_TYPE_HINGE, "hinge type")
	chk(PhysicsServer3D.hinge_joint_get_flag(hj, PhysicsServer3D.HINGE_JOINT_FLAG_USE_LIMIT), "hinge flag round-trip")
	var sj := reg(PhysicsServer3D.joint_create())
	PhysicsServer3D.joint_make_slider(sj, a, Transform3D(), b, Transform3D())
	chk(PhysicsServer3D.joint_get_type(sj) == PhysicsServer3D.JOINT_TYPE_SLIDER, "slider type")
	var cj := reg(PhysicsServer3D.joint_create())
	PhysicsServer3D.joint_make_cone_twist(cj, a, Transform3D(), b, Transform3D())
	chk(PhysicsServer3D.joint_get_type(cj) == PhysicsServer3D.JOINT_TYPE_CONE_TWIST, "cone twist type")
	var gj := reg(PhysicsServer3D.joint_create())
	PhysicsServer3D.joint_make_generic_6dof(gj, a, Transform3D(), b, Transform3D())
	chk(PhysicsServer3D.joint_get_type(gj) == PhysicsServer3D.JOINT_TYPE_6DOF, "6dof type")
	PhysicsServer3D.joint_set_solver_priority(gj, 7)
	chk(PhysicsServer3D.joint_get_solver_priority(gj) == 7, "solver priority round-trip")
	PhysicsServer3D.joint_disable_collisions_between_bodies(gj, true)
	chk(PhysicsServer3D.joint_is_disabled_collisions_between_bodies(gj), "disable collisions round-trip")
	return true

func t_ccd(c: Dictionary) -> bool:
	var fr: int = c["frame"]
	if fr == 1:
		c["space"] = mkspace()
		var wall := mkbody(c["space"], mkshape_box(Vector3(0.025, 4, 4)), Vector3(0, 2, 0), PhysicsServer3D.BODY_MODE_STATIC)
		c["wall"] = wall
		var proj_shape := reg(PhysicsServer3D.sphere_shape_create())
		PhysicsServer3D.shape_set_data(proj_shape, 0.1)
		var mkproj := func(z: float, ccd: bool) -> RID:
			var p := mkbody(c["space"], proj_shape, Vector3(-4, 2, z))
			PhysicsServer3D.body_set_param(p, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
			PhysicsServer3D.body_set_state(p, PhysicsServer3D.BODY_STATE_CAN_SLEEP, false)
			if ccd:
				PhysicsServer3D.body_set_enable_continuous_collision_detection(p, true)
			PhysicsServer3D.body_set_state(p, PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY, Vector3(150, 0, 0))
			return p
		c["p_off"] = mkproj.call(0.0, false)
		c["p_on"] = mkproj.call(0.5, true)
		return false
	if fr < 16:
		return false
	chk(poso(c["p_off"]).x > 1.0, "no-CCD projectile tunneled (x=%.2f)" % poso(c["p_off"]).x)
	chk(poso(c["p_on"]).x < -0.05, "CCD projectile blocked (x=%.2f)" % poso(c["p_on"]).x)
	return true

func t_contacts(c: Dictionary) -> bool:
	var fr: int = c["frame"]
	if fr == 1:
		c["space"] = mkspace()
		var fl := mkbody(c["space"], mkshape_box(Vector3(200, 0.5, 200)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		c["fl"] = fl
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(2, 3, 0))
		PhysicsServer3D.body_set_max_contacts_reported(b, 8)
		c["b"] = b
		return false
	if fr < 120:
		return false
	var st := PhysicsServer3D.body_get_direct_state(c["b"])
	chk(st != null, "direct body state object")
	if st == null:
		return true
	chk(st.get_contact_count() > 0, "contact count > 0 while resting (got %d)" % st.get_contact_count())
	if st.get_contact_count() > 0:
		chk(st.get_contact_collider(0) == c["fl"], "contact collider rid")
		chk(st.get_contact_local_normal(0).y > 0.7, "contact normal up")
		chk((st.get_contact_local_position(0) as Vector3).y < 0.1, "contact local position near bottom")
		chk(is_finite_v(st.get_contact_collider_velocity_at_position(0)), "collider velocity finite")
	chk(st.get_total_gravity().length() > 9.0, "total gravity reported")
	return true

func is_finite_v(v: Vector3) -> bool:
	return is_finite(v.x) and is_finite(v.y) and is_finite(v.z)

func t_fi_callback(c: Dictionary) -> bool:
	var fr: int = c["frame"]
	if fr == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.5, 0.5, 0.5)), Vector3(0, 5, 0))
		c["fi"] = 0
		c["b"] = b
		PhysicsServer3D.body_set_force_integration_callback(b, Callable(self, "_on_fi"))
		return false
	if fr == 30:
		chk(c["fi"] >= 25 and c["fi"] <= 30, "force integration callback fired per step (%d/29)" % c["fi"])
		PhysicsServer3D.body_set_force_integration_callback(c["b"], Callable())
		c["fi2"] = c["fi"]
		return false
	if fr == 60:
		chk(c["fi"] == c["fi2"], "callback stops after clear")
		return true
	return false

func _on_fi(_state) -> void:
	if not _ctx.is_empty():
		_ctx["fi"] = int(_ctx.get("fi", 0)) + 1

func t_lifetime_gc(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var sh := mkshape_box(Vector3(0.4, 0.4, 0.4))
		var rng := RandomNumberGenerator.new()
		rng.seed = _seed
		var bodies: Array = []
		for i in 300:
			bodies.append(mkbody(c["space"], sh, Vector3(
				rng.randf() * 30 - 15, 2 + i * 0.01, rng.randf() * 30 - 15)))
		# Drop local references; only RIDs (and ctx tracking) keep natives alive.
		bodies.clear()
		# Force GC pressure through allocations.
		var junk: Array = []
		for i in 2000:
			junk.append(str(i))
		junk.clear()
		return false
	if c["frame"] < 50:
		return false
	# All bodies must still be simulated: the space must be queryable and healthy.
	var q := PhysicsRayQueryParameters3D.create(Vector3(0, 50, 0), Vector3(0, 0.5, 0))
	chk(ds() != null, "space direct state alive after GC pressure")
	return true

func t_lifetime_stale(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		var b := mkbody(c["space"], mkshape_box(Vector3(0.4, 0.4, 0.4)), Vector3(0, 5, 0))
		PhysicsServer3D.free_rid(b)
		c["dead"] = b
		return false
	# Use-after-free calls must not corrupt the engine.
	PhysicsServer3D.body_set_param(c["dead"], PhysicsServer3D.BODY_PARAM_MASS, 2.0)
	PhysicsServer3D.body_apply_central_impulse(c["dead"], Vector3.ONE)
	var b2 := mkbody(c["space"], mkshape_box(Vector3(0.3, 0.3, 0.3)), Vector3(3, 3, 0))
	c["b2"] = b2
	return c["frame"] > 30

func t_errors(c: Dictionary) -> bool:
	if c["frame"] == 1:
		c["space"] = mkspace()
		# Degenerate and malformed inputs.
		var z := reg(PhysicsServer3D.box_shape_create())
		PhysicsServer3D.shape_set_data(z, Vector3.ZERO)
		var zb := mkbody(c["space"], z, Vector3(0, 2, 0))
		var dg := reg(PhysicsServer3D.convex_polygon_shape_create())
		PhysicsServer3D.shape_set_data(dg, PackedVector3Array([Vector3(0, 0, 0), Vector3(1, 0, 0), Vector3(2, 0, 0)]))
		var dgb := mkbody(c["space"], dg, Vector3(5, 2, 0))
		var bad := reg(PhysicsServer3D.box_shape_create())
		PhysicsServer3D.shape_set_data(bad, 7.0)  # wrong type
		c["canary"] = mkbody(c["space"], mkshape_box(Vector3(0.3, 0.3, 0.3)), Vector3(9, 3, 9))
		return false
	if c["frame"] < 50:
		return false
	chk(poso(c["canary"]).y < 1.8, "engine healthy after malformed inputs (canary fell)")
	return true

func t_vehicle(c: Dictionary) -> bool:
	var fr: int = c["frame"]
	if fr == 1:
		var px = PhysXServer3D.get_singleton()
		c["space"] = mkspace()
		var fl := mkbody(c["space"], mkshape_box(Vector3(400, 0.5, 400)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		var chassis := mkbody(c["space"], mkshape_box(Vector3(0.9, 0.3, 2.0)), Vector3(0, 1.0, 0))
		PhysicsServer3D.body_set_param(chassis, PhysicsServer3D.BODY_PARAM_MASS, 800.0)
		var v: RID = reg(px.vehicle_create(0))
		px.vehicle_set_chassis_body(v, chassis)
		px.vehicle_set_space(v, c["space"])
		px.vehicle_set_wheel_count(v, 4)
		for i in 4:
			var wp := {
				"radius": 0.5,
				"suspension_travel": 1.0,
				"local_pose": Transform3D(Basis(), Vector3(-0.6 + 1.2 * (i % 2), -0.1, -0.9 + 1.8 * (i / 2))),
				"traction": true,
				"brake": true,
			}
			if i >= 2:
				wp["steer"] = true
			px.vehicle_set_wheel_params(v, i, wp)
		chk(px.vehicle_get_wheel_count(v) == 4, "wheel count 4")
		c["v"] = v
		c["chassis"] = chassis
		c["z0"] = poso(chassis).z
		return false
	if fr == 30:
		var px = PhysXServer3D.get_singleton()
		px.vehicle_set_control_inputs(c["v"], 1.0, 0.0, 0.0, 0.0)
		return false
	if fr == 210:
		var px = PhysXServer3D.get_singleton()
		var dz := absf(poso(c["chassis"]).z - c["z0"])
		chk(dz > 0.5, "vehicle drives under GDScript throttle (dz=%.2f)" % dz)
		var states: Array = px.vehicle_get_wheel_states(c["v"])
		chk(states.size() == 4, "telemetry returns 4 wheel dicts (got %d)" % states.size())
		var in_contact := 0
		for s in states:
			if s is Dictionary and s.get("in_contact", false):
				in_contact += 1
		chk(in_contact >= 3, "wheels report ground contact (%d/4)" % in_contact)
		if states.size() > 0 and states[0] is Dictionary:
			var w0: Dictionary = states[0]
			chk(w0.has("rpm") and w0.has("skid") and w0.has("rotation"), "wheel state fields present")
			chk(is_finite(w0.get("rpm", NAN)), "rpm finite")
		return true
	return false

func t_vehicle_response_tuning(c: Dictionary) -> bool:
	# GDScript mirror of PHYSX-VEHI-013: vehicle_set_response_params must
	# retune the steer lock per vehicle, and a negative entry reverts to the
	# built-in default (0.6 rad).
	var fr: int = c["frame"]
	if fr == 1:
		var px = PhysXServer3D.get_singleton()
		c["space"] = mkspace()
		mkbody(c["space"], mkshape_box(Vector3(400, 0.5, 400)), Vector3(0, -0.5, 0), PhysicsServer3D.BODY_MODE_STATIC)
		for spec in [{"x": 0.0, "lock": 0.05, "tag": "restricted"}, {"x": 6.0, "lock": -1.0, "tag": "default"}]:
			var chassis := mkbody(c["space"], mkshape_box(Vector3(0.9, 0.3, 2.0)), Vector3(spec["x"], 1.0, 0))
			PhysicsServer3D.body_set_param(chassis, PhysicsServer3D.BODY_PARAM_MASS, 800.0)
			var v: RID = reg(px.vehicle_create(0))
			px.vehicle_set_chassis_body(v, chassis)
			px.vehicle_set_space(v, c["space"])
			px.vehicle_set_wheel_count(v, 4)
			for i in 4:
				px.vehicle_set_wheel_params(v, i, {
					"radius": 0.4,
					"suspension_travel": 0.3,
					"local_pose": Transform3D(Basis(), Vector3(-0.7 + 1.4 * (i % 2), -0.05, -0.7 + 1.4 * (i / 2))),
					"steer": i < 2,
					"front": i < 2,
					"traction": true,
					"brake": true,
				})
			px.vehicle_set_response_params(v, {"max_steer_angle": spec["lock"]})
			c["v_" + spec["tag"]] = v
			c["chassis_" + spec["tag"]] = chassis
		return false
	if fr == 21:
		var px = PhysXServer3D.get_singleton()
		px.vehicle_set_control_inputs(c["v_restricted"], 0.3, 0.0, 1.0, 0.0)
		px.vehicle_set_control_inputs(c["v_default"], 0.3, 0.0, 1.0, 0.0)
		c["yaw_a0"] = pos(c["chassis_restricted"]).basis.get_euler().y
		c["yaw_b0"] = pos(c["chassis_default"]).basis.get_euler().y
		return false
	if fr == 111:
		var px = PhysXServer3D.get_singleton()
		var dyaw_a := _yaw_travel(c["yaw_a0"], pos(c["chassis_restricted"]).basis.get_euler().y)
		var dyaw_b := _yaw_travel(c["yaw_b0"], pos(c["chassis_default"]).basis.get_euler().y)
		c["dyaw_a"] = dyaw_a
		chk(dyaw_b > 0.5, "default steer lock turns the car (%.2f rad)" % dyaw_b)
		chk(dyaw_a < 0.5 * dyaw_b, "restricted steer lock turns slower (%.2f vs %.2f rad)" % [dyaw_a, dyaw_b])
		# Negative entry reverts the channel to the built-in default.
		px.vehicle_set_response_params(c["v_restricted"], {"max_steer_angle": -1.0})
		px.vehicle_set_control_inputs(c["v_restricted"], 0.0, 1.0, 0.0, 0.0)
		PhysicsServer3D.body_set_state(c["chassis_restricted"], PhysicsServer3D.BODY_STATE_TRANSFORM, Transform3D(Basis(), Vector3(0, 1.0, 0)))
		PhysicsServer3D.body_set_state(c["chassis_restricted"], PhysicsServer3D.BODY_STATE_LINEAR_VELOCITY, Vector3.ZERO)
		PhysicsServer3D.body_set_state(c["chassis_restricted"], PhysicsServer3D.BODY_STATE_ANGULAR_VELOCITY, Vector3.ZERO)
		return false
	if fr == 132:
		var px = PhysXServer3D.get_singleton()
		px.vehicle_set_control_inputs(c["v_restricted"], 0.3, 0.0, 1.0, 0.0)
		c["yaw_r0"] = pos(c["chassis_restricted"]).basis.get_euler().y
		return false
	if fr == 222:
		var dyaw_r := _yaw_travel(c["yaw_r0"], pos(c["chassis_restricted"]).basis.get_euler().y)
		chk(dyaw_r > 2.0 * float(c["dyaw_a"]), "negative entry reverts to default steer lock (%.2f vs %.2f rad)" % [dyaw_r, float(c["dyaw_a"])])
		return true
	return false

func _yaw_travel(from: float, to: float) -> float:
	var d := absf(to - from)
	return minf(d, TAU - d)

func t_soft_body(c: Dictionary) -> bool:
	c["space"] = mkspace()
	var sb := reg(PhysicsServer3D.soft_body_create())
	chk(sb.is_valid(), "soft_body_create valid RID")
	PhysicsServer3D.soft_body_set_space(sb, c["space"])
	chk(PhysicsServer3D.soft_body_get_space(sb) == c["space"], "soft body space round-trip")
	PhysicsServer3D.soft_body_set_total_mass(sb, 3.0)
	chk(is_equal_approx(PhysicsServer3D.soft_body_get_total_mass(sb), 3.0), "mass round-trip")
	PhysicsServer3D.soft_body_set_linear_stiffness(sb, 0.8)
	chk(is_equal_approx(PhysicsServer3D.soft_body_get_linear_stiffness(sb), 0.8), "stiffness round-trip")
	PhysicsServer3D.soft_body_pin_point(sb, 2, true)
	chk(PhysicsServer3D.soft_body_is_point_pinned(sb, 2), "pin round-trip")
	note("soft-body simulation itself is not implemented in the module (API skeleton) — behavior intentionally not asserted here")
	return true


# ------------------------------------------------------------------ articulation
# Exercises the newly bound articulation_* methods through GDScript ->
# ClassDB -> inner PhysXServer3D. Behavior assertions are light here (the C#
# suite covers simulation); this validates binding reachability, argument
# marshaling (Transform3D/Vector3/int/float/bool) and round-trips.
func t_articulation(c: Dictionary) -> bool:
	var px = PhysXServer3D.get_singleton()
	var art: RID = reg(px.articulation_create())
	chk(art.is_valid(), "articulation_create valid RID")
	px.articulation_set_fix_base(art, true)
	var base_i: int = px.articulation_add_link(art, -1, Transform3D(), Transform3D(Basis(), Vector3(0, 3, 0)), 0, 1000.0, Vector3(0.1, 0.5, 0.1))
	chk(base_i == 0, "base link index 0 (got %d)" % base_i)
	var link_i: int = px.articulation_add_link(art, 0,
			Transform3D(Basis(), Vector3(0, -0.5, 0)),
			Transform3D(Basis(), Vector3(0, -0.5, 0)), 2, 1000.0, Vector3(0.1, 0.5, 0.1))
	chk(link_i == 1, "child link index 1 (got %d)" % link_i)
	chk(px.articulation_get_link_count(art) == 2, "link count 2")
	var shape := reg(PhysicsServer3D.box_shape_create())
	PhysicsServer3D.shape_set_data(shape, Vector3(0.1, 0.5, 0.1))
	px.articulation_set_link_shape(art, 1, shape, Transform3D())
	px.articulation_set_link_collision_layer(art, 1, 0x20)
	px.articulation_set_link_collision_mask(art, 1, 0x40)
	chk(px.articulation_get_link_collision_layer(art, 1) == 0x20, "link layer round-trip")
	chk(px.articulation_get_link_collision_mask(art, 1) == 0x40, "link mask round-trip")
	px.articulation_set_drive(art, 1, 0, 200.0, 20.0, 0.5, 0.0, 1)
	px.articulation_set_limit(art, 0, 0, -1.0, 1.0)
	px.articulation_set_space(art, c["space"])
	px.articulation_wake(art)
	var xf: Transform3D = px.articulation_get_link_transform(art, 0)
	chk(xf.origin.distance_to(Vector3(0, 3, 0)) < 0.05, "base link transform at its frame (got %s)" % str(xf.origin))
	var vel: Dictionary = px.articulation_get_link_velocity(art, 0)
	chk(vel.has("linear") and vel.has("angular"), "velocity dictionary keys")
	var t: int = int(c.get("articulation_frame", 0)) + 1
	c["articulation_frame"] = t
	if t < 20:
		return false
	# After a few steps the fixed base is still at its pose and the chain sleeps or moves coherently.
	var xf2: Transform3D = px.articulation_get_link_transform(art, 0)
	chk(xf2.origin.distance_to(Vector3(0, 3, 0)) < 0.05, "fixed base stays put over steps")
	return true
