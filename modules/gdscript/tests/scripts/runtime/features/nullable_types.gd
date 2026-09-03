var member: int?

func returns_null() -> int?:
	return null

func returns_value() -> int?:
	return 5

func takes(v: int?) -> int:
	if v == null:
		return -1
	return v

func test():
	var x: int?
	print(x)
	x = 5
	print(x)
	x = null
	print(x)

	print(returns_null())
	print(returns_value())
	print(takes(null))
	print(takes(10))

	print(member)
	member = 42
	print(member)
