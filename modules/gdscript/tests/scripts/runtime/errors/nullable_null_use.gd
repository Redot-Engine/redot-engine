func get_nullable() -> int?:
	return null

func test():
	var value: int? = get_nullable()
	print(value + 1)
