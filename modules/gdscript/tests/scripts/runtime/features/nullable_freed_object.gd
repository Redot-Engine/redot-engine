func test():
	var node: Node? = Node.new()
	print(node == null)
	print(is_instance_valid(node))
	node.free()
	print(node == null)
	print(is_instance_valid(node))
