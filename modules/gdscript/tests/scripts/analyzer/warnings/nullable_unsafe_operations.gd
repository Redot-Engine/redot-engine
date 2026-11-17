func unsafe_operations(x: int?, v: Vector2?, a: Array?):
	print(x + 1)
	print(-x)
	print(v.x)
	for i in a:
		print(i)

func test():
	unsafe_operations(5, Vector2(1, 2), [10])
