class Payload:
	pass

class Unrelated:
	pass

struct Record:
	var value: Payload

func test():
	var record := Record.new(Unrelated.new())
	print(record)
