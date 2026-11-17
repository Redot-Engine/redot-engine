const Point = preload("struct_import_flat_alias.notest.gd").Point

func test():
	var p := Point.new(3)
	print(p.x)
	print(p.y)
	var q: Point = Point.new(7, 8)
	print(q.x)
	print(q.y)
