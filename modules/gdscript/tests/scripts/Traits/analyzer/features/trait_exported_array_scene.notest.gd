extends RefCounted

trait Damageable extends Node:
	func take_damage() -> void:
		pass

class Enemy extends Node:
	uses Damageable

class Owner extends Node:
	@export var targets: Array[Damageable] = []
