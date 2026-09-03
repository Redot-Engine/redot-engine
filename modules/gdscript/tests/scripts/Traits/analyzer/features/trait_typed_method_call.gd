extends RefCounted

trait Greeter:
	func greet() -> void:
		print("hello from trait")

class GreeterImplementation:
	uses Greeter

func call_greeting(greeter: Greeter) -> void:
	greeter.greet()

func test() -> void:
	call_greeting(GreeterImplementation.new())
