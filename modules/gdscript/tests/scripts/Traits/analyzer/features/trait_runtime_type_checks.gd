extends Node

trait Damageable extends Node:
	func take_damage() -> void:
		pass

class Enemy extends Node:
	uses Damageable

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
	var empty: Damageable = null
	print(empty == null)
	print(null_damageable() == null)
	other.free()
	value.free()
