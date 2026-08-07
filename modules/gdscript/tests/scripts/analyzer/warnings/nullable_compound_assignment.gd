func f(v: int?) -> int:
	if v != null:
		v += 1
		return v
	return 0
func test():
	print(f(5))
	print(f(null))
