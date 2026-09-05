func bad(v: int?) -> int:
	return v

func test():
	# Valid state: a non-null value flows through the nullable source fine.
	print(bad(5))
	# Invalid state: the declared non-null return type must reject null at runtime.
	print(bad(null))
