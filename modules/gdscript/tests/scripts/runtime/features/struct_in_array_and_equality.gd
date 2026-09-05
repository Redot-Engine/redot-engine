struct Coord:
	var x: int
	var y: int

func test():
	var arr: Array = [Coord.new(1, 2), Coord.new(3, 4)]
	print(arr[0].x, " ", arr[1].y)

	var a := Coord.new(5, 6)
	var b := Coord.new(5, 6)
	var c := Coord.new(5, 7)
	print(a == b)
	print(a == c)

	var s := var_to_str(a)
	var back = str_to_var(s)
	print(back == a)
	print(back.x, " ", back.y)
