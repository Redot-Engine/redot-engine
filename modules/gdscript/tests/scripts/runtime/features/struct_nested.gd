struct NestInner:
	var v: int

struct NestOuter:
	var inner: NestInner
	var n: int

func test():
	var o := NestOuter.new()
	o.inner = NestInner.new(5)
	print(o.inner.v)
	o.inner.v = 42
	print(o.inner.v)
	print(o.n)
