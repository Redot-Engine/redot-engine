func takes_int(v: int) -> int:
	return v

func passthrough(v: int?) -> int:
	return v

func test():
	var a: int? = 5
	var b: int = a
	print(b)
	print(takes_int(a))
	print(passthrough(3))
