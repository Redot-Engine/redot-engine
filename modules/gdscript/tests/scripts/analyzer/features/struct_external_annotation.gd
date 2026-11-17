const Defs = preload("struct_external_point.notest.gd")

func take_point(_p: Defs.Point) -> void:
	pass

func read_record(record: StructReferenceRecord) -> int:
	return record.value.custom_field + record.trait_value.get_marker() + record.inner.number \
		+ record.values[0].custom_field + record.by_name["value"].custom_field \
		+ record.nested.value.custom_field

func test():
	var callback := take_point
	print(callback != null)
	print(read_record != null)
	print("ok")
