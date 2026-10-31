extends SceneTree

# Blast debris API (audit R-14, PHYSX-BLAST-002).
#
#   A dynamic destructible falls, shatters mid-flight with shatter_speed=0
#   (no kick): every spawned piece must inherit the pre-impact velocity --
#   recorded at the END of the previous tick, so the impact's own push does
#   not fling far-away chunks along. is_fractured() agrees with the
#   chunks_fractured signal; debris_lifetime retires every piece (and its
#   Blast actor, so a later hit cannot re-spawn it).
#
# Run from the repository root:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/blast_debris_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var authoring: Object
var destructible: Node3D
var frame := 0
var fractured_frame := -1
var fall_speed_before := 0.0
var last_y := 12.0
var fractured_flag := false
var _json_path := ""
var _failed := false
var _messages: Array = []
var _assertions := 0

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		_failed = true
		_messages.append(msg)
		print("BLAST-DEBRIS FAIL: ", msg)
	else:
		print("BLAST-DEBRIS ok: ", msg)

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	for cls in ["PhysXBlastAuthoring", "PhysXDestructible3D", "PhysXBlastAsset"]:
		if not ClassDB.class_exists(cls):
			print("[blast-debris] SKIP: ", cls, " not registered")
			TestReport.write(_json_path, "blast", "PHYSX-BLAST-002",
					"blast debris API (is_fractured, velocity inheritance, lifetime)",
					"skip", 0, [cls + " not registered in this build"])
			quit(0)
			return

	authoring = ClassDB.instantiate("PhysXBlastAuthoring")
	var mesh := BoxMesh.new()
	mesh.size = Vector3(2, 2, 2)
	var asset: Object = authoring.fracture_mesh(mesh, 12, 777)
	if asset == null or asset.call("get_chunk_count") < 2:
		_check(false, "asset authoring produced no chunks")
		_finish()
		return

	destructible = ClassDB.instantiate("PhysXDestructible3D")
	destructible.set("blast_asset", asset)
	destructible.set("dynamic", true)
	destructible.set("shatter_speed", 0.0) # isolate the inherited velocity
	destructible.set("debris_lifetime", 1.0)
	destructible.set("debris_shrink", 0.4)
	destructible.connect("chunks_fractured", _on_fractured)
	root.add_child(destructible)
	# High up: 45 frames of free fall make the pre-impact velocity clearly
	# directional (about -7.4 m/s), far above solver noise.
	destructible.set_position(Vector3(0, 12, 0))
	physics_frame.connect(_on_physics_frame)

func _on_fractured(_world_position: Vector3, _pieces: int, _damage: float) -> void:
	fractured_flag = true

func _on_physics_frame() -> void:
	frame += 1
	var y: float = destructible.get_position().y
	if frame < 45:
		last_y = y

	# The node tracks the intact body; the per-frame transform delta at the
	# frame before the fracture is the pre-impact velocity the pieces are
	# expected to inherit.
	if frame >= 2:
		fall_speed_before = (y - last_y) * 60.0
	if frame == 45:
		var pieces: int = destructible.call("apply_radial_damage",
				destructible.get_position(), 10.0, 0.0, 5.0)
		fractured_frame = frame
		_check(pieces > 0, "radial damage spawned pieces (got %d)" % pieces)
		_check(destructible.call("is_fractured"), "is_fractured agrees with the fracture")
		_check(fractured_flag, "chunks_fractured signal fired for the fracture")
		_check(absf(fall_speed_before) > 3.0, "pre-impact fall speed measurable (%.2f m/s)" % fall_speed_before)
		var count: int = destructible.call("get_piece_count")
		_check(count == pieces, "piece count matches the spawn report (%d vs %d)" % [count, pieces])
		# Every piece leaves with the pre-impact velocity (shatter_speed=0):
		# still falling, close to the fall speed measured a frame earlier.
		var inherited := true
		for i in count:
			var v: Vector3 = destructible.call("get_piece_velocity", i)
			if v.y > fall_speed_before + 1.0 or v.y < fall_speed_before - 4.0:
				inherited = false
				print("[blast-debris] piece %d velocity %s (expected near %s)" % [i, v, fall_speed_before])
		_check(inherited, "all pieces inherit the pre-impact velocity")
		return

	if frame == 75:
		# Half a second later: still falling (nothing under them here).
		var count: int = destructible.call("get_piece_count")
		var still_falling := true
		for i in count:
			var v: Vector3 = destructible.call("get_piece_velocity", i)
			if v.y > -3.0:
				still_falling = false
		_check(count > 0 and still_falling, "pieces keep the momentum mid-flight")
		return

	if frame == 45 + 90:
		# Lifetime (1.0 s) + shrink (0.4 s) have passed: every piece retired.
		var count: int = destructible.call("get_piece_count")
		_check(count == 0, "debris_lifetime retired every piece (left %d)" % count)
		_check(destructible.call("is_fractured"), "is_fractured stays true after cleanup")
		var again: int = destructible.call("apply_radial_damage",
				Vector3(0, 12, 0), 10.0, 0.0, 5.0)
		_check(again == 0, "retired actors cannot be re-fractured (got %d pieces)" % again)
		_finish()

func _finish() -> void:
	var ok := not _failed
	print("[blast-debris] ", "PASS" if ok else "FAIL")
	TestReport.write(_json_path, "blast", "PHYSX-BLAST-002",
			"blast debris API (is_fractured, velocity inheritance, lifetime)",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
