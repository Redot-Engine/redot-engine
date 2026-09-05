extends Node

trait Damageable extends Node:
	func take_damage() -> void:
		pass

class Enemy extends Node:
	uses Damageable

trait UnconstrainedDamageable:
	func take_damage() -> void:
		print("damaged")

class UnconstrainedEnemy extends Node2D:
	uses UnconstrainedDamageable

func can_use_unconstrained_trait_from_node_2d(parent_typed: Node2D) -> bool:
	return parent_typed is UnconstrainedDamageable and (parent_typed as UnconstrainedDamageable) != null

func can_downcast_unconstrained_trait_to_node_2d(trait_typed: UnconstrainedDamageable) -> bool:
	return trait_typed is Node2D and (trait_typed as Node2D) != null

func accept_damageable(_value: Damageable) -> void:
	print("accepted")

func null_damageable() -> Damageable:
	return null

func test() -> void:
	var value: Node = Enemy.new()
	var damageable := value as Damageable
	print(damageable != null)
	print(value is Damageable)
	var other := Enemy.new()
	accept_damageable(other)
	accept_damageable(damageable)
	accept_damageable(null)
	var base_node: Node = damageable
	print(base_node == value)
	var empty: Damageable = null
	print(empty == null)
	print(null_damageable() == null)
	other.free()
	value.free()

	var implementation := UnconstrainedEnemy.new()
	var parent_typed: Node2D = implementation
	print(can_use_unconstrained_trait_from_node_2d(implementation))
	var unconstrained := parent_typed as UnconstrainedDamageable
	print(unconstrained != null)
	unconstrained.take_damage()
	print(parent_typed is UnconstrainedDamageable)

	var plain_node := Node2D.new()
	print((plain_node as UnconstrainedDamageable) == null)
	print(plain_node is UnconstrainedDamageable)

	var trait_typed: UnconstrainedDamageable = implementation
	print(can_downcast_unconstrained_trait_to_node_2d(trait_typed))
	var downcast := trait_typed as Node2D
	print(downcast == implementation)
	print(trait_typed is Node2D)
	implementation.free()
	plain_node.free()
