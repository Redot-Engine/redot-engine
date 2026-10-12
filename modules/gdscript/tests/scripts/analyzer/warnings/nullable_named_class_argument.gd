# GH-1489

class_name TestClass extends RefCounted

var nullable_variable: float?

func test_method(test_class: TestClass) -> float:
	if test_class.nullable_variable == null:
		return NAN

	return test_class.nullable_variable

func test():
	var test_class := TestClass.new()
	print(test_method(test_class))
	test_class.nullable_variable = 1.5
	print(test_method(test_class))
