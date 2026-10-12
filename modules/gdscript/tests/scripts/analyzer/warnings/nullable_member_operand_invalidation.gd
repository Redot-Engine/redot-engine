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

	func stale_native_property():
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

func stale_array_index():
	if values != null:
		print(self.values[clear_values()])

func stale_arithmetic():
	if number != null:
		print(self.number + clear_number())

func stale_utility_argument():
	if number != null:
		print(maxi(self.number, clear_number()))

func stale_bare_argument():
	if number != null:
		consume(number, clear_number())

func stale_dynamic_property():
	var alias: Variant = self
	if values != null:
		var _ignored: Variant = alias.side_effect
		print(self.values[0])

func stale_dynamic_index():
	var alias: Variant = self
	if values != null:
		var _ignored: Variant = alias["side_effect"]
		print(self.values[0])

func test():
	pass
