func f(v: int?) -> int:
	var last := -1
	while v != null:
		last = v
		v = null
	return last
func test():
	print(f(7))
	print(f(null))
