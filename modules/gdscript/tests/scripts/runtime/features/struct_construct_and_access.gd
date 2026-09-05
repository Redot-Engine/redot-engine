struct Vec2:
	var x: int
	var y: int = 5

func test():
	var p := Vec2.new()
	print(p.x)
	print(p.y)

	p.x = 10
	p.y = 20
	print(p.x)
	print(p.y)

	var q := Vec2.new(1, 2)
	print(q.x)
	print(q.y)

	var r := p
	r.x = 99
	print(p.x)
	print(r.x)
