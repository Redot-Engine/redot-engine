struct MutualA:
	var b: MutualB

struct MutualB:
	var a: MutualA

func test():
	print("unreachable")
