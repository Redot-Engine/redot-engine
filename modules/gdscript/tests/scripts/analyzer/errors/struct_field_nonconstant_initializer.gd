struct Inner:
	var value: int

struct Outer:
	var inner: Inner = Inner.new(42)

func test():
	print("unreachable")
