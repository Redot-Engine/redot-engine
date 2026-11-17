# A struct field read through a member of another class, in a script that names
# neither the struct nor its file itself (#1558).

func read_health(holder: StructOfMemberHolder) -> float:
	return holder.stats.health

func read_first_health(holder: StructOfMemberHolder) -> float:
	return holder.history[0].health

func test():
	print(read_health != null)
	print(read_first_health != null)
	print("ok")
