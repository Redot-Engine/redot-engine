struct AnnotPoint:
	var x: int
	var y: int = 5

func take_point(_p: AnnotPoint) -> void:
	pass

func test():
	var callback := take_point
	print(callback != null)
	print("ok")
