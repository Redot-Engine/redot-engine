enum State { IDLE, RUN, STOP }
func test():
	var a: State? = State.RUN
	var b: State = a
	print(b)
