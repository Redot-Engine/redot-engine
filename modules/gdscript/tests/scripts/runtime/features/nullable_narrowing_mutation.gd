func test():
	var x: int? = 5
	if x != null:
		x = null
		x = 7
		print(x + 1)

	var y: int? = 3
	if y != null and y > 0 and y < 10:
		print("y in range")
