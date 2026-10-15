extends SceneTree

# Temporary diagnostic: minimal RID-level area + falling body.

var _space: RID
var _area: RID
var _area_shape: RID
var _body: RID
var _body_shape: RID
var _tick := 0
var _enters := 0
var _exits := 0

func _initialize() -> void:
	print("[probe] init (no setup here)")
	

func _on_monitor(status: int, _body_rid: RID, _id: int, _bs: int, _as: int) -> void:
	if status == PhysicsServer3D.AREA_BODY_ADDED:
		_enters += 1
	else:
		_exits += 1

func _physics_process(_delta: float) -> bool:
	_tick += 1
	if _tick == 1:
		print("[probe] setup from _physics_process tick 1")
		_space = PhysicsServer3D.space_create()
		PhysicsServer3D.space_set_active(_space, true)
		_area_shape = PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(_area_shape, Vector3(0.75, 0.75, 0.75))
		_area = PhysicsServer3D.area_create()
		PhysicsServer3D.area_set_space(_area, _space)
		PhysicsServer3D.area_add_shape(_area, _area_shape)
		PhysicsServer3D.area_set_monitor_callback(_area, Callable(self, "_on_monitor"))
		_body_shape = PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(_body_shape, Vector3(0.25, 0.25, 0.25))
		_body = PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(_body, PhysicsServer3D.BODY_MODE_RIGID)
		PhysicsServer3D.body_set_space(_body, _space)
		PhysicsServer3D.body_add_shape(_body, _body_shape)
		PhysicsServer3D.body_set_state(_body, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3.ZERO))
	if _tick % 10 == 0:
		var xf: Transform3D = PhysicsServer3D.body_get_state(_body, PhysicsServer3D.BODY_STATE_TRANSFORM)
		print("[probe] tick=%d pos_y=%.3f enters=%d exits=%d" % [_tick, xf.origin.y, _enters, _exits])
	if _tick >= 100:
		print("[probe] done")
		return true
	return false
