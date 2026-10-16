extends SceneTree

# Headless smoke test for the Blast (destructible mesh) extension:
#   authoring (fracture) -> PhysXBlastAsset -> PhysXDestructible3D lifecycle
#   (load, spawn intact, radial damage -> split into rigid pieces).
#
# Classes are resolved through ClassDB so the suite also runs on builds
# compiled with blast=no: it then reports SKIP instead of failing to parse
# (bare identifiers of unregistered classes are compile-time errors).
# Run from the repository root:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/blast_smoke_test.gd -- --json=<path>
# Exit code 0 = pass/skip, 1 = fail.

const TestReport := preload("res://gdscript/test_report.gd")

var _failed := false
var _messages: Array = []
var _assertions := 0

func _fail(msg: String) -> void:
	print("BLAST-TEST FAIL: ", msg)
	_failed = true
	_messages.append(msg)

func _init() -> void:
	# GODOT_PHYSX_BLAST is a build flag (blast=yes, Windows-only): without it
	# the classes do not exist and the suite is a SKIP, not a failure.
	if not (ClassDB.class_exists("PhysXBlastAuthoring")
			and ClassDB.class_exists("PhysXBlastAsset")
			and ClassDB.class_exists("PhysXDestructible3D")):
		print("BLAST-TEST SKIP: Blast classes not registered (blast=no build?)")
		TestReport.write(TestReport.json_path_from_args(), "blast", "PHYSX-BLAST-001",
				"authoring + .tres round-trip + destructible radial damage",
				"skip", 0, ["Blast classes not registered in this build"])
		quit(0)
		return
	_run()

func _run() -> void:
	var authoring: Object = ClassDB.instantiate("PhysXBlastAuthoring")
	if authoring == null:
		_fail("PhysXBlastAuthoring could not be instantiated")
		_finish()
		return

	var mesh := BoxMesh.new()
	mesh.size = Vector3(2, 2, 2)

	# Voronoi: the default pattern, deterministic for a fixed seed.
	var asset: Object = authoring.fracture_mesh(mesh, 12, 12345)
	_assertions += 1
	if asset == null or asset.call("get_chunk_count") < 2:
		_fail("voronoi fracture produced no chunks")
	else:
		print("BLAST-TEST voronoi chunks: ", asset.call("get_chunk_count"))

	# Slicing: cube-root split across the axes.
	var slicing_pattern: int = ClassDB.class_get_integer_constant("PhysXBlastAuthoring", "PATTERN_SLICING")
	var slicing_asset: Object = authoring.fracture_mesh(mesh, 8, 7, slicing_pattern)
	_assertions += 1
	if slicing_asset == null or slicing_asset.call("get_chunk_count") < 2:
		_fail("slicing fracture produced no chunks")
	else:
		print("BLAST-TEST slicing chunks: ", slicing_asset.call("get_chunk_count"))

	# Round-trip through Resource serialization (the .tres save path the
	# editor fracture dialog uses).
	if asset != null:
		asset.set("resource_path", "res://blast_smoke_asset.tres")
		var err := ResourceSaver.save(asset, asset.get("resource_path"))
		if err != OK:
			_fail("ResourceSaver.save failed with error %d" % err)
		else:
			var loaded := ResourceLoader.load("res://blast_smoke_asset.tres")
			if loaded == null or loaded.get_class() != "PhysXBlastAsset" \
					or loaded.call("get_chunk_count") != asset.call("get_chunk_count"):
				_fail("asset save/load round-trip lost chunks")
			else:
				print("BLAST-TEST resource round-trip ok (", loaded.call("get_chunk_count"), " chunks)")
			DirAccess.remove_absolute(ProjectSettings.globalize_path("res://blast_smoke_asset.tres"))

	# Destructible node: load, spawn the intact body, fracture it, count pieces.
	if asset != null:
		var destructible: Node = ClassDB.instantiate("PhysXDestructible3D")
		destructible.set("blast_asset", asset)
		destructible.set("dynamic", false)
		root.add_child(destructible)
		# Give ENTER_WORLD a chance to load the asset and spawn the intact
		# body before damaging it.
		await process_frame
		await physics_frame

		var pieces: int = destructible.call("apply_radial_damage", Vector3(0, 2, 0), 10.0, 0.0, 5.0)
		_assertions += 1
		if pieces < 1:
			_fail("apply_radial_damage spawned no pieces")
		else:
			print("BLAST-TEST radial damage spawned pieces: ", pieces)

		# Let a few physics ticks run so the spawned rigid bodies simulate
		# (sync transform pass + kill-floor logic must not crash headless).
		await physics_frame
		await physics_frame
		await physics_frame

		destructible.free()

	_finish()

func _finish() -> void:
	var ok := not _failed
	print("BLAST-TEST ", "PASS" if ok else "FAIL")
	TestReport.write(TestReport.json_path_from_args(), "blast", "PHYSX-BLAST-001",
			"authoring + .tres round-trip + destructible radial damage",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
