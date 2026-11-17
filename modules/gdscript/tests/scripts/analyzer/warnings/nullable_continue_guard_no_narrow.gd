func f(v: int?) -> int:
	var total := 0
	for i in range(3):
		if v == null:
			continue
		total += v + 1
	return total
func test():
	print(f(2))
	print(f(null))
