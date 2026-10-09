class Holder:
	var value: float?
	var other: float?

func guarded(h: Holder) -> float:
	if h.value == null:
		return NAN
	return h.value

func guarded_inline(h: Holder) -> float:
	if h.value != null:
		return h.value + 1.0
	return 0.0

func base_reassigned(h: Holder, next: Holder) -> float:
	if h.value != null:
		h = next
		return h.value
	return 0.0

func aliased_write(a: Holder, b: Holder) -> float:
	if a.value != null:
		b.value = null
		return a.value
	return 0.0

func call_invalidates(h: Holder) -> float:
	if h.value != null:
		reset(h)
		return h.value
	return 0.0

func different_instances(a: Holder, b: Holder) -> float:
	if a.value != null:
		return b.value
	return 0.0

func different_member(h: Holder) -> float:
	if h.value != null:
		return h.other
	return 0.0

func reset(h: Holder) -> void:
	h.value = null

func test():
	var h := Holder.new()
	h.value = 2.0
	print(guarded(h))
	print(guarded_inline(h))
	print(base_reassigned(h, h))
	print(aliased_write(h, Holder.new()))
	print(call_invalidates(Holder.new()))
	print(different_instances(h, h))
	h.other = 5.0
	print(different_member(h))
