struct SelfRef:
	var next: SelfRef

func test():
	print("unreachable")
