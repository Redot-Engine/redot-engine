struct UntypedVec:
	var x: int
	var y: int = 5

func test():
	# A struct flowing through a Variant-typed value must still support field
	# get/set at runtime (via OPCODE_GET_NAMED / OPCODE_SET_NAMED).
	var v: Variant = UntypedVec.new(1, 2)
	print(v.x)
	print(v.y)
	v.x = 10
	print(v.x)
