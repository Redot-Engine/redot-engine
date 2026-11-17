# A runtime error in a method of a trait from another file is reported at the
# line of the trait's file, not at that line number in this one (#1557).
uses RuntimeErrorLineTrait

func test():
	cause_a_problem()
