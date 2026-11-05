# GDScript bridge to the module-specific vehicle2 API on the inner
# PhysXServer3D singleton.
#
# The vehicle methods exist only on PhysXServer3D (not on PhysicsServer3D),
# so scripts must fetch the inner singleton:
#   PhysXServer3D.get_singleton().vehicle_create(0)
#
# This bridge exists so the C# suite can reach the same API on builds whose
# C# bindings have not been regenerated yet (marshaling through Variant),
# and for the GDScript binding suite below.
extends Object
class_name PhysXVehicleBridge

static func create(archetype: int) -> RID:
	return PhysXServer3D.get_singleton().vehicle_create(archetype)

func vehicle_create(archetype: int) -> RID:
	return PhysXServer3D.get_singleton().vehicle_create(archetype)

func vehicle_set_chassis_body(vehicle: RID, body: RID) -> void:
	PhysXServer3D.get_singleton().vehicle_set_chassis_body(vehicle, body)

func vehicle_set_space(vehicle: RID, space: RID) -> void:
	PhysXServer3D.get_singleton().vehicle_set_space(vehicle, space)

func vehicle_get_wheel_count(vehicle: RID) -> int:
	return PhysXServer3D.get_singleton().vehicle_get_wheel_count(vehicle)

func vehicle_set_wheel_count(vehicle: RID, count: int) -> void:
	PhysXServer3D.get_singleton().vehicle_set_wheel_count(vehicle, count)

func vehicle_add_wheel(vehicle: RID) -> int:
	return PhysXServer3D.get_singleton().vehicle_add_wheel(vehicle)

func vehicle_set_wheel_params(vehicle: RID, idx: int, params: Dictionary) -> void:
	PhysXServer3D.get_singleton().vehicle_set_wheel_params(vehicle, idx, params)

func vehicle_set_control_inputs(vehicle: RID, throttle: float, brake: float, steer: float, handbrake: float) -> void:
	PhysXServer3D.get_singleton().vehicle_set_control_inputs(vehicle, throttle, brake, steer, handbrake)

func vehicle_set_response_params(vehicle: RID, params: Dictionary) -> void:
	PhysXServer3D.get_singleton().vehicle_set_response_params(vehicle, params)

func vehicle_set_anti_roll_params(vehicle: RID, params: Dictionary) -> void:
	PhysXServer3D.get_singleton().vehicle_set_anti_roll_params(vehicle, params)

func vehicle_get_anti_roll_params(vehicle: RID) -> Dictionary:
	return PhysXServer3D.get_singleton().vehicle_get_anti_roll_params(vehicle)

func vehicle_get_response_params(vehicle: RID) -> Dictionary:
	return PhysXServer3D.get_singleton().vehicle_get_response_params(vehicle)

func vehicle_get_wheel_states(vehicle: RID) -> Array:
	return PhysXServer3D.get_singleton().vehicle_get_wheel_states(vehicle)

func vehicle_get_engine_state(vehicle: RID) -> Dictionary:
	return PhysXServer3D.get_singleton().vehicle_get_engine_state(vehicle)

func vehicle_set_ackermann_params(vehicle: RID, params: Dictionary) -> void:
	PhysXServer3D.get_singleton().vehicle_set_ackermann_params(vehicle, params)

func vehicle_get_wheel_steer_angles(vehicle: RID) -> PackedFloat32Array:
	return PhysXServer3D.get_singleton().vehicle_get_wheel_steer_angles(vehicle)

func vehicle_set_balance_params(vehicle: RID, params: Dictionary) -> void:
	PhysXServer3D.get_singleton().vehicle_set_balance_params(vehicle, params)

func vehicle_get_balance_state(vehicle: RID) -> Dictionary:
	return PhysXServer3D.get_singleton().vehicle_get_balance_state(vehicle)
