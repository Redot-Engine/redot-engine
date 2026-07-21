# Structs declared with `struct_name` are globally visible by their bare name,
# usable from other files without an enclosing-class prefix.

func test():
	# Construct a global struct from another file by its bare name.
	var p := TestPoint.new()
	p.x = 3
	p.y = 4
	print("p: ", p.x, " ", p.y)

	# Value semantics: assignment copies, so mutating the copy leaves the original intact.
	var q := p
	q.x = 99
	print("after copy mutate -> p: ", p.x, " q: ", q.x)

	# A global struct also works as a declared type.
	var typed: TestPoint = TestPoint.new()
	typed.y = 7
	print("typed: ", typed.x, " ", typed.y)
