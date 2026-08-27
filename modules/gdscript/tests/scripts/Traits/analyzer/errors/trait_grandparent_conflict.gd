trait AddsGreeting:
	func greet() -> void:
		pass

class Grandparent:
	func greet() -> void:
		pass

class Parent extends Grandparent:
	pass

class Child extends Parent:
	uses AddsGreeting

func test() -> void:
	pass
