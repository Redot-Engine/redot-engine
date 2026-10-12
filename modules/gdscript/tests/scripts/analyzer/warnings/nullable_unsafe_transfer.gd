func takes_int(v: int) -> int:
	return v

func passthrough(v: int?) -> int:
	return v

func nullable_int() -> int?:
	return 5

func test():
	var a: int? = nullable_int()
	var b: int = a
	print(b)
	print(takes_int(a))
	print(passthrough(3))
