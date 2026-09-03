struct Body:
	var mass: float = 1.0
	var tags: Array = []

func test():
	var a := Body.new()
	var b := Body.new()
	a.mass = 3
	a.tags = ["x"]
	print(a.mass)
	print(typeof(a.mass) == TYPE_FLOAT)
	print(a.tags.size())
	print(b.tags.size())
	print(b.mass)
