struct TypedS:
	var a: int

func test():
	var x := TypedS.new("hello")
	print(x)
