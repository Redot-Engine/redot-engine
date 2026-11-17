trait Unconstrained:
	func ping() -> void:
		pass

trait RefCountedOnly extends RefCounted:
	func ping() -> void:
		pass

class NodeImplementation extends Node:
	uses Unconstrained

func test() -> void:
	var implementation := NodeImplementation.new()
	var parent_typed: Node = implementation
	parent_typed as RefCountedOnly
	parent_typed is RefCountedOnly

	var trait_typed: Unconstrained = implementation
	var ref_counted: RefCounted = trait_typed
	var node: Node = trait_typed
	print(ref_counted, node)
	implementation.free()
