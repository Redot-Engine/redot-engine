struct Inner:
	var number: int = 0

struct Payload:
	var number: int = 0
	var direction: Vector2 = Vector2.ZERO
	var inner: Inner = Inner.new()

var member: Payload? = Payload.new()
static var static_member: Payload? = Payload.new()
var values: Array? = [1]
var mapping: Dictionary? = {"number": 2}
var vector: Vector2? = Vector2.ZERO
var other: int? = 3

func early_return_guard():
	if member == null:
		print(member)
		print("member was null")
		return
	member.number += 10
	print(member.number)

func explicit_self():
	if self.member != null:
		self.member.number += 2
		print(member.number)

func static_write():
	if static_member != null:
		static_member.number = 7
		print(static_member.number)

func nested_write():
	if member != null:
		member.direction.x = 5
		self.member.inner.number = 4
		print(member.direction, ":", member.inner.number)

func collection_write():
	if values != null:
		values[0] += 2
		print(values[0])
	if mapping != null:
		mapping["number"] += 3
		mapping.extra = 4
		print(mapping["number"], ":", mapping.extra)

func vector_write():
	if vector != null:
		vector.x = 6
		self.vector.y += 2
		print(vector)

func other_member_guard():
	if member != null and other != null:
		member.number += 1
		print(other + 1)

func local_collection_write():
	var local: Array = [1]
	if member != null:
		local[0] = 2
		print(member.number)

func test():
	early_return_guard()
	explicit_self()
	static_write()
	nested_write()
	collection_write()
	vector_write()
	other_member_guard()
	local_collection_write()
