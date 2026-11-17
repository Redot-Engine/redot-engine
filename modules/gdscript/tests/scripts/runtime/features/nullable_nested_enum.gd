func test():
	var a: Vector2.Axis? = null
	print(a)
	a = Vector2.AXIS_Y
	print(a)

	var b: Variant.Type? = null
	print(b)
	b = TYPE_INT
	print(b)

	# non-nullable nested enum is unaffected
	var c: Vector2.Axis = Vector2.AXIS_X
	print(c)
