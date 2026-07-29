class Base:
	extends RefCounted
	var value: int
	func _init(v: int = 0): value = v
	func doubled() -> int: return value * 2
enum State { IDLE, RUN, STOP }
func hard_int() -> int: return 7
func from_int(v: int?) -> int:
	if v != null:
		v = hard_int()
		return v + 1
	return 0
func from_string(v: String?) -> int:
	if v != null:
		v = "abcd"
		return v.length()
	return -1
func from_vector(v: Vector2?) -> float:
	if v != null:
		v = Vector2(3, 4)
		return v.length()
	return -1.0
func from_object(v: Base?) -> int:
	if v != null:
		v = Base.new(4)
		return v.doubled()
	return -1
func from_enum(v: State?) -> bool:
	if v != null:
		v = State.STOP
		return v == State.STOP
	return false
func test():
	print(from_int(1))
	print(from_string("x"))
	print(from_vector(Vector2.ONE))
	print(from_object(Base.new(1)))
	print(from_enum(State.IDLE))
