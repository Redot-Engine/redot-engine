extends Node
uses "export_type_reference_owner.notest.gd"

@export var own: int = 2

class Inner extends Node:
	@export var attachment: ExportTypeReferenceTrait
