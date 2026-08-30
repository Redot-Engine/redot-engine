func take_point(_p: Point) -> void:
	pass

func test():
	var callback := take_point
	print(callback != null)
	print("ok")
