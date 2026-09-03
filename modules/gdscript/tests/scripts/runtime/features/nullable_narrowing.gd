func guard(v: int?) -> int:
	if v == null:
		return -1
	return v

func branch(v: int?) -> int:
	if v != null:
		return v + 1
	return 0

func with_and(v: int?) -> bool:
	return v != null and v > 5

func test():
	print(guard(null))
	print(guard(7))
	print(branch(null))
	print(branch(4))
	print(with_and(null))
	print(with_and(10))
	var a: Vector2? = Vector2(3, 4)
	if a != null:
		print(a.length())
