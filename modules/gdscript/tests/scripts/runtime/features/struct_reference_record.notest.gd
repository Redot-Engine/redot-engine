const Defs = preload("struct_external_construct.notest.gd")

struct_name StructReferenceRecord:
	var value: StructReferencePayload
	var trait_value: StructReferenceTrait
	var inner: StructReferencePayload.Inner
	var values: Array[StructReferencePayload]
	var by_name: Dictionary[String, StructReferencePayload]
	var nested: Defs.PayloadHolder
