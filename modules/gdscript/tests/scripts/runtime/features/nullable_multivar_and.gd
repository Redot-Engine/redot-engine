func sum(a: int?, b: int?) -> int:
	if a != null and b != null:
		return a + b
	return -1
func test():
	print(sum(2, 3))
	print(sum(null, 3))
	print(sum(2, null))
