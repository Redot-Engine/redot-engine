func bad_array(v: Variant) -> Array[int]?:
	return v
func test():
	print(bad_array(["x"]))
