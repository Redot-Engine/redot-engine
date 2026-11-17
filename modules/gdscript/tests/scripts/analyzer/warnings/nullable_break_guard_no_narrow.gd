func f(v: int?) -> int:
	while true:
		if v == null:
			break
		return v + 1
	return -1
func test():
	print(f(5))
	print(f(null))
