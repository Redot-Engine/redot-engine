extends Node
uses GroupTrait, CategoryTrait, SubgroupTrait, RepeatedGroupTrait

# GH-1395
@export var trait_group_name: int

trait GroupTrait:
	@export_group("trait_group_name")
	@export var group_value: int

trait CategoryTrait:
	@export_category("queue_free")
	@export var category_value: int

trait SubgroupTrait:
	@export_subgroup("First-position Subgroup")
	@export var subgroup_value: int

trait RepeatedGroupTrait:
	@export_group("trait_group_name")
	@export var repeated_group_value: int

trait InheritedGroupTrait:
	@export_group("inherited_group_name")
	@export var inherited_group_value: int

class ScriptBase:
	var inherited_group_name: int

class ScriptChild extends ScriptBase:
	uses InheritedGroupTrait

trait SharedTrait:
	@export_group("Shared Trait")
	@export var shared_value: int

trait LeftTrait:
	uses SharedTrait
	@export var left_value: int

trait RightTrait:
	uses SharedTrait
	@export var right_value: int

class DiamondClass:
	uses LeftTrait, RightTrait

func get_export_signatures(object: Object, names: Array[String]) -> Array[String]:
	var signatures: Array[String]
	for property in object.get_property_list():
		if property.name in names:
			signatures.append(Utils.get_property_signature(property))
	return signatures

func test():
	var export_signatures := get_export_signatures(self, [
		"trait_group_name",
		"group_value",
		"queue_free",
		"category_value",
		"First-position Subgroup",
		"subgroup_value",
		"repeated_group_value",
	])
	for signature in export_signatures:
		print(signature)

	var inherited := ScriptChild.new()
	var inherited_signatures := get_export_signatures(inherited, ["inherited_group_name", "inherited_group_value"])
	for signature in inherited_signatures:
		print(signature)

	var diamond := DiamondClass.new()
	var diamond_signatures := get_export_signatures(diamond, ["Shared Trait", "shared_value", "left_value", "right_value"])
	for signature in diamond_signatures:
		print(signature)
