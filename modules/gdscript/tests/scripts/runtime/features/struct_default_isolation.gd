struct Bag:
	var items: Array = []

func test():
	var a := Bag.new()
	var b := Bag.new()
	# In-place mutation of one instance's container default must not leak into
	# other instances or into later constructions.
	a.items.append("x")
	print(a.items.size())
	print(b.items.size())
	var c := Bag.new()
	print(c.items.size())
