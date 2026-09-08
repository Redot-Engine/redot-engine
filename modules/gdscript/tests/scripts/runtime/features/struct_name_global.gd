func test():
	# GlobalPoint is declared with `struct_name` in another file, so it is usable
	# by its bare name here without a preload.
	var p := GlobalPoint.new(3)
	print(p.x)
	print(p.y)

	var q: GlobalPoint = GlobalPoint.new(10, 20)
	print(q.x)
	print(q.y)
