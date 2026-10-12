struct Payload:
	var inner: int? = 21
	var other: int? = 22

struct Outer:
	var payload: Payload = Payload.new()

var member: Payload = Payload.new()
var getter_member: Payload:
	get:
		return member

func unchecked(value: Payload) -> int:
	return value.inner

func different_instance(a: Payload, b: Payload) -> int:
	if a.inner != null:
		return b.inner
	return -1

func different_field(value: Payload) -> int:
	if value.inner != null:
		return value.other
	return -1

func receiver_reassigned(value: Payload, next: Payload) -> int:
	if value.inner != null:
		value = next
		return value.inner
	return -1

func null_write(value: Payload) -> int:
	if value.inner != null:
		value.inner = null
		return value.inner
	return -1

func reset_member() -> int:
	member.inner = null
	return 0

func after_call() -> int:
	if member.inner != null:
		var _reset: int = reset_member()
		return member.inner
	return -1

func later_operand() -> int:
	if member.inner != null:
		return member.inner + reset_member()
	return -1

func getter_receiver() -> int:
	if getter_member.inner != null:
		return getter_member.inner
	return -1

func nested_receiver(value: Outer) -> int:
	if value.payload.inner != null:
		return value.payload.inner
	return -1

func loop_write(value: Payload) -> int:
	if value.inner != null:
		for _i in [0, 1]:
			var result: int = value.inner
			value.inner = null
			if result == 0:
				return result
	return -1

func test():
	pass

signal completed

func after_await() -> int:
	if member.inner != null:
		await completed
		return member.inner
	return -1

func deferred_read():
	if member.inner != null:
		var _callback = func() -> int:
			return member.inner

func string_key_write(value: Payload) -> int:
	if value.inner != null:
		value["inner"] = null
		return value.inner
	return -1

func string_name_key_write() -> int:
	if member.inner != null:
		self.member[&"inner"] = null
		return member.inner
	return -1

func integer_key_write(value: Payload) -> int:
	if value.inner != null:
		value[0] = null
		return value.inner
	return -1

func dynamic_key_write(value: Payload, key: String) -> int:
	if value.inner != null:
		value[key] = null
		return value.inner
	return -1

func known_field_write(value: Payload) -> int:
	if value.inner != null:
		value.other = null
		return value.inner
	return -1
