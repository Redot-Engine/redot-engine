struct Leaf:
	var v: int = 7

struct Holder:
	# An explicit `T.new()` names a concrete value, so a nullable struct field
	# honors it as the schema default instead of staying null.
	var present: Leaf? = Leaf.new()
	# A nullable struct field with no initializer stays null.
	var absent: Leaf?

func test():
	var h := Holder.new()
	print(h.present.v)
	print(h.absent)
