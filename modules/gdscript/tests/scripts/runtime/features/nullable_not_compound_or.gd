func sum(a: int?, b: int?) -> int:
	if not (a == null or b == null):
		return a + b
	return -1
func test():
	print(sum(4, 5))
	print(sum(null, 5))
