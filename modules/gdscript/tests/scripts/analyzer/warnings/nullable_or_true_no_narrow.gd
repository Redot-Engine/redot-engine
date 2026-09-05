func f(a: int?, b: int?) -> int:
	if a != null or b != null:
		return a + 1
	return -1
func test():
	print(f(5, null))
