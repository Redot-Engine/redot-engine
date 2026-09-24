const Defs = preload("struct_external_point.notest.gd")

func take_point(_p: Defs.Point) -> void:
	pass

func test():
	var callback := take_point
	print(callback != null)
	print("ok")
