#debug-only
var values: Array? = [1]
var number: int? = 4
var side_effect: bool:
	get:
		values = null
		return true

class NativeAccessor extends PhysicsDirectBodyState2DExtension:
	var values: Array? = [1]

	func _get_center_of_mass() -> Vector2:
		values = null
		return Vector2.ZERO

	func native_property():
		if values != null:
			var _center: Vector2 = center_of_mass
			print(self.values[0])

func clear_values() -> int:
	values = null
	return 0

func clear_number() -> int:
	number = null
	return 1

func consume(_value: int, _after: int):
	pass

func array_index():
	values = [1]
	if values != null:
		print(self.values[clear_values()])

func arithmetic():
	number = 4
	if number != null:
		print(self.number + clear_number())

func utility_argument():
	number = 4
	if number != null:
		print(maxi(self.number, clear_number()))

func bare_argument():
	number = 4
	if number != null:
		consume(number, clear_number())

func dynamic_property():
	values = [1]
	var alias: Variant = self
	if values != null:
		var _ignored: Variant = alias.side_effect
		print(self.values[0])

func dynamic_index():
	values = [1]
	var alias: Variant = self
	if values != null:
		var _ignored: Variant = alias["side_effect"]
		print(self.values[0])

func test():
	array_index()
	print("Array null access returned to caller.")
	arithmetic()
	print("Arithmetic null access returned to caller.")
	utility_argument()
	print("Utility null argument returned to caller.")
	bare_argument()
	print("Bare member null argument returned to caller.")
	dynamic_property()
	print("Dynamic property null access returned to caller.")
	dynamic_index()
	print("Dynamic index null access returned to caller.")
	var native := NativeAccessor.new()
	native.native_property()
	native.free()
	print("Native property null access returned to caller.")
