func test():
	var values: Dictionary[String, int]? = {"a": 1}
	var raw: Dictionary = {1: "bad"}
	values = raw
	print(values)
