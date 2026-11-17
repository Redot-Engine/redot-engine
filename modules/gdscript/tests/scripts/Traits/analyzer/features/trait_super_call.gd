extends Node

trait Greeter:
	func greet(recipient: String = "world") -> String:
		return "hello, " + recipient

class DefaultGreeter:
	uses Greeter

class LoudGreeter:
	uses Greeter

	func greet(recipient: String = "world") -> String:
		return super(recipient).to_upper()

trait Incrementer:
	static func increment(value: int) -> int:
		return value + 1

class DoubleIncrementer:
	uses Incrementer

	static func increment(value: int) -> int:
		return super(value) * 2

trait ReadyTrait extends Node:
	func _ready() -> void:
		print("trait ready")

class ReadyOverride extends Node:
	uses ReadyTrait

	func _ready() -> void:
		print("before ready")
		super()
		print("after ready")

func test() -> void:
	print(DefaultGreeter.new().greet())
	print(LoudGreeter.new().greet("traits"))
	print(DoubleIncrementer.increment(3))

	var ready_override := ReadyOverride.new()
	ready_override._ready()
	ready_override.free()
