func sum(a: int?, b: int?) -> int:
	if a == null or b == null:
		return -1
	return a + b
func test():
	print(sum(2, 3))
	print(sum(null, 3))
