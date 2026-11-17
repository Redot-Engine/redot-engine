struct Payload:
	var inner: int? = 21

var member: Payload = Payload.new()
static var static_member: Payload = Payload.new()

func guarded_parameter(value: Payload) -> int:
	if value.inner != null:
		return value.inner
	return -1

func assigned_parameter(value: Payload) -> int:
	value.inner = 42
	return value.inner

func guarded_local() -> int:
	var value: Payload = Payload.new()
	if value.inner != null:
		return value.inner
	return -1

func assigned_local() -> int:
	var value: Payload = Payload.new()
	value.inner = 42
	return value.inner

func guarded_member() -> int:
	if member.inner != null:
		return member.inner
	return -1

func assigned_member() -> int:
	member.inner = 42
	return member.inner

func explicit_member() -> int:
	if self.member.inner != null:
		return member.inner
	return -1

func guarded_static_member() -> int:
	if static_member.inner != null:
		return static_member.inner
	return -1

func nullable_receiver(value: Payload?) -> int:
	if value != null and value.inner != null:
		return value.inner
	return -1

func early_return(value: Payload) -> int:
	if value.inner == null:
		return -1
	return value.inner

func short_circuit(value: Payload) -> bool:
	return value.inner != null and value.inner + 1 == 22

func nullable_write_and_renarrow(value: Payload) -> int:
	if value.inner != null:
		value.inner = null
		if value.inner == null:
			value.inner = 22
			return value.inner
	return -1

func while_guard(value: Payload) -> int:
	var last: int = -1
	while value.inner != null:
		last = value.inner
		value.inner = null
	return last

func test():
	var value: Payload = Payload.new()
	print(guarded_parameter(value))
	print(assigned_parameter(value))
	print(guarded_parameter(value))
	print(guarded_local())
	print(assigned_local())
	print(guarded_member())
	print(assigned_member())
	print(explicit_member())
	print(guarded_static_member())
	print(nullable_receiver(value))
	print(nullable_receiver(null))
	print(early_return(value))
	print(early_return(Payload.new(null)))
	print(short_circuit(value))
	print(nullable_write_and_renarrow(value))
	print(while_guard(value))
	print(while_guard(Payload.new(null)))
