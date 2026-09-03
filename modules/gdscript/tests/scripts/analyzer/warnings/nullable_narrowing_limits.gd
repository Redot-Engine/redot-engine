func reassigned(v: int?) -> int:
	if v != null:
		print(v + 1)
		v = get_nullable()
		return v + 1
	return 0

func not_definite_guard(x: int?, cond: bool) -> int:
	if x == null:
		if cond:
			return -1
	return x + 1

func get_nullable() -> int?:
	return 3

func test():
	print(reassigned(5))
	print(not_definite_guard(5, false))
