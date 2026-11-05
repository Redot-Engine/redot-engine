# GDScript bridge to the module-specific articulation API on the inner
# PhysXServer3D singleton.
#
# Same purpose as physx_vehicle_bridge.gd: lets the C# suite call the API on
# builds whose C# bindings have not been regenerated yet (marshaling through
# Variant), and gives the GDScript binding suite a direct ClassDB path.
# The articulation methods exist only on PhysXServer3D (not on
# PhysicsServer3D), so scripts must fetch the inner singleton:
#   PhysXServer3D.get_singleton().articulation_create()
extends Object
# (No class_name: this file is loaded by path and must not depend on the
# project's global class cache being rescanned.)


func articulation_create() -> RID:
	return PhysXServer3D.get_singleton().articulation_create()


func articulation_set_space(articulation: RID, space: RID) -> void:
	PhysXServer3D.get_singleton().articulation_set_space(articulation, space)


func articulation_add_link(articulation: RID, parent_index: int, parent_frame: Transform3D,
		child_frame: Transform3D, joint_type: int, density: float, box_half_extents: Vector3) -> int:
	return PhysXServer3D.get_singleton().articulation_add_link(
			articulation, parent_index, parent_frame, child_frame, joint_type, density, box_half_extents)


func articulation_set_drive(articulation: RID, link_index: int, axis: int, stiffness: float,
		damping: float, drive_target: float, drive_velocity: float, drive_type: int) -> void:
	PhysXServer3D.get_singleton().articulation_set_drive(
			articulation, link_index, axis, stiffness, damping, drive_target, drive_velocity, drive_type)


func articulation_set_limit(articulation: RID, link_index: int, axis: int, low: float, high: float) -> void:
	PhysXServer3D.get_singleton().articulation_set_limit(articulation, link_index, axis, low, high)


func articulation_set_fix_base(articulation: RID, fix: bool) -> void:
	PhysXServer3D.get_singleton().articulation_set_fix_base(articulation, fix)


func articulation_wake(articulation: RID) -> void:
	PhysXServer3D.get_singleton().articulation_wake(articulation)


func articulation_sleep(articulation: RID) -> void:
	PhysXServer3D.get_singleton().articulation_sleep(articulation)


func articulation_get_link_count(articulation: RID) -> int:
	return PhysXServer3D.get_singleton().articulation_get_link_count(articulation)


func articulation_get_link_transform(articulation: RID, link_index: int) -> Transform3D:
	return PhysXServer3D.get_singleton().articulation_get_link_transform(articulation, link_index)


func articulation_is_sleeping(articulation: RID) -> bool:
	return PhysXServer3D.get_singleton().articulation_is_sleeping(articulation)


func articulation_set_link_shape(articulation: RID, link_index: int, shape: RID, transform: Transform3D) -> void:
	PhysXServer3D.get_singleton().articulation_set_link_shape(articulation, link_index, shape, transform)


func articulation_set_link_collision_layer(articulation: RID, link_index: int, layer: int) -> void:
	PhysXServer3D.get_singleton().articulation_set_link_collision_layer(articulation, link_index, layer)


func articulation_set_link_collision_mask(articulation: RID, link_index: int, mask: int) -> void:
	PhysXServer3D.get_singleton().articulation_set_link_collision_mask(articulation, link_index, mask)


func articulation_get_link_collision_layer(articulation: RID, link_index: int) -> int:
	return PhysXServer3D.get_singleton().articulation_get_link_collision_layer(articulation, link_index)


func articulation_get_link_collision_mask(articulation: RID, link_index: int) -> int:
	return PhysXServer3D.get_singleton().articulation_get_link_collision_mask(articulation, link_index)


func articulation_get_link_velocity(articulation: RID, link_index: int) -> Dictionary:
	return PhysXServer3D.get_singleton().articulation_get_link_velocity(articulation, link_index)
