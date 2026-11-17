trait Feature:
	var value: int

class Unrelated:
	pass

struct Record:
	var value: Feature

func test():
	var record := Record.new(Unrelated.new())
	print(record)
