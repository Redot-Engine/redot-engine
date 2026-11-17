struct Payload:
	var number: int = 0

var member: Payload? = Payload.new()
var vector_property: Vector2 = Vector2.ZERO:
	set(value):
		vector_property = value
		member = null

func reset() -> int:
	member = null
	return 0

func value_call():
	var local: Array = [0]
	if member != null:
		local[0] = reset()
		print(member.number)

func index_call():
	var local: Array = [0]
	if member != null:
		local[reset()] = 1
		print(member.number)

func property_write_back():
	if member != null:
		vector_property.x = 1
		print(member.number)

func explicit_property_write_back():
	if member != null:
		self.vector_property.y += 1
		print(self.member.number)

func test():
	pass
