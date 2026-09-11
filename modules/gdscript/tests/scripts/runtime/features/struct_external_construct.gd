const Defs = preload("struct_external_construct.notest.gd")

func test():
	var p := Defs.Point.new(3)
	print(p.x)
	print(p.y)
	p.y = 9
	print(p.y)
