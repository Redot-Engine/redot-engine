class PathUser:
	uses "path_trait_a.notest.gd", "path_trait_b.notest.gd"

class ConstantUser:
	uses PATH_TRAIT_B, TraitHolder.PATH_TRAIT_A

	const PATH_TRAIT_B = preload("path_trait_b.notest.gd")

	class TraitHolder:
		const PATH_TRAIT_A = preload("path_trait_a.notest.gd")

func test() -> void:
	print(PathUser.new() != null)
	print(ConstantUser.new() != null)
