#debug-only
var values: Array? = [1]
var vector_property: Vector2 = Vector2.ZERO:
	set(value):
		vector_property = value
		values = null

func bare_write_back():
	values = [1]
	if values != null:
		vector_property.x = 1
		print(self.values[0])

func explicit_write_back():
	values = [1]
	if values != null:
		self.vector_property.y += 1
		print(self.values[0])

func test():
	bare_write_back()
	print("Bare property setter returned to caller.")
	explicit_write_back()
	print("Explicit property setter returned to caller.")
