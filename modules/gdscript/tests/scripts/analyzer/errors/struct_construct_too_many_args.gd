struct CtorS:
	var a: int

func test():
	var x := CtorS.new(1, 2)
	print(x)
