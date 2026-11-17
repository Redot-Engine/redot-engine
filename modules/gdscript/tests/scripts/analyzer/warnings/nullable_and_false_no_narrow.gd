func f(a: int?, b: int?) -> int:
	if a == null and b == null:
		return a + 1
	return -1
func test():
	print(f(5, 5))
