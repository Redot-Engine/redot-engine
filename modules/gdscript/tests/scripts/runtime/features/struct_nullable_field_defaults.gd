struct NullDefBox:
	var i: int?
	var vec2: Vector2?
	var vec3: Vector3?
	var typed: int
	var initialized: int? = 5

func test():
	var box := NullDefBox.new()
	print(box.i)
	print(box.vec2)
	print(box.vec3)
	print(box.typed)
	print(box.initialized)
