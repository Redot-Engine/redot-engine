func test():
	var raw: Dictionary = {1: "text"}
	var typed: Dictionary[String, int]? = raw
	print(typed)
