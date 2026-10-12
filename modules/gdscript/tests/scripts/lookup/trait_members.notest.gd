trait_name LookupTrait
extends RefCounted

signal trait_signal

var trait_value: int = 42

const TRAIT_CONSTANT: int = 7

func trait_method() -> void:
	pass

func overridden_method() -> void:
	pass

func implemented_method() -> void

enum NamedEnum {
	NAMED_VALUE,
}

enum {
	UNNAMED_VALUE,
}

struct TraitStruct:
	var value: int
