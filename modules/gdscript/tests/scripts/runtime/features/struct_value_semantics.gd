struct Rgb:
	var r: int
	var g: int
	var b: int

func mutate(c: Rgb) -> void:
	c.r = 255

func make() -> Rgb:
	var c := Rgb.new(1, 2, 3)
	return c

func test():
	var explicit: Rgb = Rgb.new(10, 20, 30)
	print(explicit.r, " ", explicit.g, " ", explicit.b)

	mutate(explicit)
	print(explicit.r)

	var built := make()
	print(built.r, " ", built.g, " ", built.b)

	var f: Rgb = Rgb.new()
	f.r = 7
	print(f.r, " ", f.g)
