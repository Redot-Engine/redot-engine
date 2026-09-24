extends RefCounted

trait Selectable:
	func tag() -> String:
		return "selectable"

class Sel:
	uses Selectable

class Plain:
	pass

func test() -> void:
	var mixed: Array = [Sel.new(), Plain.new(), Sel.new()]

	# Regression: a typed loop variable over an untyped expression forces a
	# conversion-assign into a trait-typed target. This used to print
	# "Compiler bug: unresolved assign" and silently drop the conversion.
	for item: Selectable in mixed.filter(func(o): return o is Selectable):
		print(item.tag())

	# Conversion-assign into a trait-typed local.
	var one: Selectable = Sel.new()
	print(one.tag())

	# Already-typed container: no conversion needed, must still work.
	var typed: Array[Selectable] = [Sel.new()]
	for item: Selectable in typed:
		print(item.tag())
