func guarded(v: int?) -> int:
	if v == null:
		return -1
	else:
		pass
	return v + 1

func test():
	print(guarded(null))
	print(guarded(7))
