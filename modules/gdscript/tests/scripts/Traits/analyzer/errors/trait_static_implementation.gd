trait RequiresInstance:
	func required() -> void:
		pass

class Implementation:
	uses RequiresInstance

	static func required() -> void:
		pass

func test() -> void:
	pass
