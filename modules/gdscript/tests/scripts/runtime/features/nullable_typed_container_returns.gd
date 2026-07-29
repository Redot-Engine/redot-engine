func array_null() -> Array[int]?:
	return null
func array_value() -> Array[int]?:
	return [1, 2]
func dict_null() -> Dictionary[String, int]?:
	return null
func dict_value() -> Dictionary[String, int]?:
	return {"a": 1}
func test():
	print(array_null())
	print(array_value())
	print(dict_null())
	print(dict_value())
