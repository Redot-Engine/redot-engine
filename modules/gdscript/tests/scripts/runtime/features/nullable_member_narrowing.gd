struct Payload:
	var number: int = 7

var member: Payload? = Payload.new()
static var static_member: Payload? = Payload.new()

func direct_guards():
	if member != null:
		print(member.number)
	if self.member != null:
		print(member.number)
	if member != null:
		print(self.member.number)
	if static_member != null:
		print(static_member.number)

func truthy_guards():
	if member:
		print(member.number)
	if self.member:
		print(member.number)
	if not member:
		print("null")
	else:
		print(self.member.number)
	if not static_member:
		print("null")
	else:
		print(static_member.number)

func guarded_member() -> int:
	if self.member == null:
		return -1
	return member.number

func guarded_truthiness() -> int:
	if not member:
		return -1
	return self.member.number

func unchanged() -> bool:
	return true

func guard_after_call() -> int:
	if unchanged() and member != null:
		return member.number
	return -1

func deferred_body():
	if member != null:
		var _callback = func():
			var _result = unchanged()
		print(member.number)

func local_capture(value: Payload?):
	if value != null:
		var callback = func():
			return value.number
		print(callback.call())

func local_values():
	var local: Payload? = Payload.new()
	print(local.number)
	var value: int? = 5
	print(value + 1)
	value = null
	value = 8
	print(value + 1)
	var zero: int? = 0
	if zero:
		print(zero + 1)
	else:
		print("zero")

func loop_guard():
	while member != null:
		print(member.number)
		member = null

func test():
	direct_guards()
	truthy_guards()
	print(guarded_member())
	print(guarded_truthiness())
	print(guard_after_call())
	print(member != null and self.member.number == 7)
	print(self.member == null or member.number == 7)
	deferred_body()
	local_capture(member)
	local_values()
	loop_guard()
	print(guarded_member())
	print(guarded_truthiness())
