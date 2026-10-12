struct Payload:
	var number: int = 7

var member: Payload? = Payload.new()

func note():
	print("Returning branch.")

func true_branch_returns() -> int:
	if member == null:
		note()
		return -1
	return self.member.number

func false_branch_returns() -> int:
	if member != null:
		pass
	else:
		note()
		return -1
	return self.member.number

func else_guard() -> int:
	if member == null:
		note()
	else:
		return self.member.number
	return -1

func deferred_returning_branch() -> int:
	if member == null:
		var _callback = func():
			note()
		return -1
	return self.member.number

func test():
	print(true_branch_returns())
	print(false_branch_returns())
	print(else_guard())
	print(deferred_returning_branch())
	member = null
	print(true_branch_returns())
	print(false_branch_returns())
	print(else_guard())
	print(deferred_returning_branch())
