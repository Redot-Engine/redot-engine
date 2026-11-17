trait Variadic:
	func required(...args)

class Implementation:
	uses Variadic

	func required(..._args):
		pass

func test() -> void:
	pass
