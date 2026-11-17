struct Payload:
	var number: int = 7

signal completed

var member: Payload? = Payload.new()
static var static_member: Payload? = Payload.new()
var getter_member: Payload?:
	get:
		return member
var setter_member: Payload? = Payload.new():
	set(value):
		setter_member = value
var side_effect: bool:
	get:
		member = null
		return true
var setter_effect: bool:
	set(value):
		setter_effect = value
		member = null

func reset() -> bool:
	member = null
	return true

func unguarded():
	print(member.number)

func getter_guard():
	if getter_member != null:
		print(getter_member.number)

func setter_guard():
	if setter_member != null:
		print(setter_member.number)

func direct_assignment():
	if member != null:
		self.member = null
		print(member.number)

func alias_assignment():
	if member != null:
		var alias = self
		alias.member = null
		print(self.member.number)

func after_call():
	if member != null:
		var _result = reset()
		print(member.number)

func static_after_call():
	if static_member != null:
		var _result = reset()
		print(static_member.number)

func after_getter():
	if member != null:
		var _read: bool = side_effect
		print(member.number)

func after_setter():
	if member != null:
		setter_effect = true
		print(self.member.number)

func stale_compound_guard():
	if member != null and reset():
		print(member.number)

func stale_compound_else():
	if member == null or reset():
		pass
	else:
		print(member.number)

func stale_return_guard():
	if member == null or not reset():
		return
	print(member.number)

func deferred_member():
	if member != null:
		var _callback = func():
			return member.number

func member_loop():
	if member != null:
		for _i in [0, 1]:
			print(member.number)
			member = null

func local_loop():
	var local: Payload? = Payload.new()
	for _i in range(2):
		print(local.number)
		local = null

func after_await():
	if member != null:
		await completed
		print(member.number)

func null_initializer():
	var local: Payload? = null
	print(local.number)

func test():
	pass
