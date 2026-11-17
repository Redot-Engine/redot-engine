trait RequiredRanges:
	func with_default(_value: int = 0) -> void
	func with_rest(_value: int, ..._args: Array) -> void

class MissingDefault:
	uses RequiredRanges

	func with_default(_value: int) -> void:
		pass

	func with_rest(_value: int, ..._args: Array) -> void:
		pass

class MissingRest:
	uses RequiredRanges

	func with_default(_value: int = 0) -> void:
		pass

	func with_rest(_value: int) -> void:
		pass

func test() -> void:
	pass
