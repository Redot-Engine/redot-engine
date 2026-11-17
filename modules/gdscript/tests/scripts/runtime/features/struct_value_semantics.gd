struct Rgb:
	var r: int
	var g: int
	var b: int

trait Marker:
	var marker: int = 7

trait NodeMarker extends Node:
	var marker: int = 9

class Payload:
	uses Marker

class MarkedNode extends Node:
	uses Marker

class ConstrainedNode extends Node:
	uses NodeMarker

struct References:
	var object: Payload
	var trait_object: Marker
	var trait_node: Marker
	var constrained_node: NodeMarker
	var count: int

func mutate(c: Rgb) -> void:
	c.r = 255

func make() -> Rgb:
	var c := Rgb.new(1, 2, 3)
	return c

func has_reference(weak_reference: WeakRef) -> bool:
	return weak_reference.get_ref() != null

func test():
	var explicit: Rgb = Rgb.new(10, 20, 30)
	print(explicit.r, " ", explicit.g, " ", explicit.b)

	mutate(explicit)
	print(explicit.r)

	var built := make()
	print(built.r, " ", built.g, " ", built.b)

	var f: Rgb = Rgb.new()
	f.r = 7
	print(f.r, " ", f.g)

	var payload := Payload.new()
	var observer: WeakRef = weakref(payload)
	var node := MarkedNode.new()
	var constrained := ConstrainedNode.new()
	var references := References.new(payload, payload, node, constrained, 1)
	var copied := references
	copied.count = 2
	copied.trait_object.marker = 11
	print(references.count, " ", copied.count, " ", references.object.marker)
	print(copied.trait_node.marker, " ", copied.constrained_node.marker)
	payload = null
	references.object = null
	references.trait_object = null
	print(has_reference(observer))
	copied.object = null
	copied.trait_object = null
	print(not has_reference(observer))
	node.free()
	constrained.free()
	print(is_instance_valid(references.trait_node))
	print(is_instance_valid(copied.constrained_node))
