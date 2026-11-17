const TraitStruct := FooTrait.Bar
const ClassStruct := FooClass.Bar

class FooClass:
	struct Bar:
		var value: String

trait FooTrait:
	struct Bar:
		var value: String

class FooTraitImpl:
	uses FooTrait

func test() -> void:
	var direct_trait := FooTrait.Bar.new("direct trait")
	var direct_class := FooClass.Bar.new("direct class")
	var aliased_trait: TraitStruct = TraitStruct.new("aliased trait")
	var aliased_class: ClassStruct = ClassStruct.new("aliased class")
	var implemented_trait := FooTraitImpl.Bar.new("implemented trait")

	print(direct_trait.value)
	print(direct_class.value)
	print(aliased_trait.value)
	print(aliased_class.value)
	print(implemented_trait.value)
