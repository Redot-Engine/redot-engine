extends SceneTree

# Depenetration-default parity pin (Phase 4, PHYSX-DEPEN-001).
#
# The parity measurement (godot_physics vs PhysX@4.0 vs PhysX@off, three
# scratch projects, half- and deeply embedded boxes) showed:
#   godot_physics  max per-step ejection 1.184 / 0.672 m, clear in 2 / 3 ticks
#   PhysX cap 4.0                  0.132 / 0.067 m, clear in 5 / 14 ticks
#   PhysX off                      1.189 / 0.680 m, clear in 2 / 3 ticks
# so the default is 0 (off) for godot_physics parity; a cap > 0 remains
# available as a comfort option (measured in the scratch projects; it cannot
# be flipped at runtime because PhysXProjectSettings::read_settings runs at
# server init).
#
# This test pins the decision:
#   PHYSX-DEPEN-001  the project setting defaults to 0 and a deeply embedded
#                   body ejects uncapped (recovery rate and clear time in the
#                   measured parity band).

const CLEAR_TICK_LIMIT := 6
const MIN_EJECTION_STEP := 0.5

var _space: RID
var _body: RID
var _tick := 0
var _last_y := 0.0
var _max_dy := 0.0
var _clear_tick := -1
var _json_path := ""

func _initialize() -> void:
	_json_path = _json_path_from_args()
	_space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(_space, true)
	var floor_shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(floor_shape, Vector3(4, 0.5, 4))
	var floor := PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(floor, PhysicsServer3D.BODY_MODE_STATIC)
	PhysicsServer3D.body_set_space(floor, _space)
	PhysicsServer3D.body_add_shape(floor, floor_shape)
	PhysicsServer3D.body_set_state(floor, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, -0.5, 0)))
	var shape := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(shape, Vector3(0.5, 0.5, 0.5))
	_body = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_space(_body, _space)
	PhysicsServer3D.body_add_shape(_body, shape)
	PhysicsServer3D.body_set_state(_body, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, -0.35, 0)))
	_last_y = -0.35

func _json_path_from_args() -> String:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--json="):
			return arg.substr("--json=".length())
	return ""

func _physics_process(_d: float) -> bool:
	_tick += 1
	var pos: Vector3 = PhysicsServer3D.body_get_state(_body, PhysicsServer3D.BODY_STATE_TRANSFORM).origin
	var dy: float = absf(pos.y - _last_y)
	if dy > _max_dy:
		_max_dy = dy
	_last_y = pos.y
	if _clear_tick < 0 and pos.y >= 0.45:
		_clear_tick = _tick
	if _tick >= 120:
		_finish(pos.y)
		return true
	return false

func _finish(rest_y: float) -> void:
	var messages: Array = []
	var ok := true

	var default_value: float = ProjectSettings.get_setting(
			"physics/physx_3d/simulation/max_depenetration_velocity")
	if not is_equal_approx(default_value, 0.0):
		ok = false
		messages.append("setting default is %s, decision is 0 (off, godot_physics parity)" % default_value)

	if _max_dy < MIN_EJECTION_STEP:
		ok = false
		messages.append("ejection capped or too slow: max per-step dy=%.4f (uncapped parity band ~0.68)" % _max_dy)
	if _clear_tick < 0 or _clear_tick > CLEAR_TICK_LIMIT:
		ok = false
		messages.append("deep embed cleared in %s ticks (parity band 2-3, limit %d)" % [_clear_tick, CLEAR_TICK_LIMIT])
	if rest_y < 0.48 or rest_y > 0.53:
		ok = false
		messages.append("body did not recover to rest on the surface: rest_y=%.4f" % rest_y)

	var status := "pass" if ok else "fail"
	var result := {
		"suite": "depenetration",
		"final": true,
		"tests": [{
			"id": "PHYSX-DEPEN-001",
			"category": "depenetration",
			"description": "max_depenetration_velocity defaults to 0 (godot_physics parity); uncapped ejection measured",
			"status": status,
			"assertions": 4,
			"failures": 0 if ok else messages.size(),
			"messages": messages,
		}],
	}
	var f := FileAccess.open(_json_path, FileAccess.WRITE) if not _json_path.is_empty() else null
	if f:
		f.store_string(JSON.stringify(result, "  "))
		f.close()
	elif not _json_path.is_empty():
		push_error("[depen] could not write JSON report to %s" % _json_path)
	print("[depen] PHYSX-DEPEN-001: %s (max_dy=%.3f clear_tick=%d rest_y=%.3f default=%s)%s" % [
			status.to_upper(), _max_dy, _clear_tick, rest_y, default_value,
			("" if messages.is_empty() else " -- " + str(messages))])
	print("[depen] %s" % ("PASS" if ok else "FAIL"))
