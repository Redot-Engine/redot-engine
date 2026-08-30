enum State { IDLE, RUN, STOP }
func test():
	var a: State? = null
	var b: State = a
	print(b)
