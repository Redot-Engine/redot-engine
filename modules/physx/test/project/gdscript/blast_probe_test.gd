extends SceneTree

# Phase 12 headless probe for the Blast pipeline and the reference-parity
# chunk-emitter additions (run_suite "blast" tier, wired alongside
# blast_smoke_test.gd). Deeper than the smoke test:
#
#   blast part (gated on the classes being registered):
#     - valid authored asset loads (is_loaded, the new probe accessor)
#     - corrupt asset bytes fail CLEANLY (BLAST-4): no crash, is_loaded=false,
#       node stays freeable; retried 5x (BLAST-2: a failed load leaves no
#       partial native state, so retries cannot accumulate)
#     - damage -> fracture -> pieces; a second damage no-ops (depth-1 asset,
#       nothing left to fracture) instead of vanishing or spawning ghosts
#   chunk-emitter part (always runs): RD-9 parity setters round-trip and a
#     spawn burst creates live chunks.
#
# Run from the repository root:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/blast_probe_test.gd -- --json=<path>
# Exit code 0 = pass/skip, 1 = fail.

const TestReport := preload("res://gdscript/test_report.gd")

var _failed := false
var _messages: Array = []
var _assertions := 0

func _fail(msg: String) -> void:
	print("BLAST-PROBE FAIL: ", msg)
	_failed = true
	_messages.append(msg)

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		_fail(msg)
	else:
		print("BLAST-PROBE ok: ", msg)

func _init() -> void:
	_run()

func _run() -> void:
	# ---------------- chunk emitter parity (RD-9), backend-independent ----
	var emitter: Node = ClassDB.instantiate("PhysXChunkEmitter3D")
	if emitter == null:
		_fail("PhysXChunkEmitter3D not registered")
	else:
		emitter.set("friction", 1.25)
		emitter.set("bounce", 0.35)
		emitter.set("linear_damp", 0.4)
		emitter.set("angular_damp", 0.6)
		emitter.set("shrink_time", 1.0)
		_check(absf(float(emitter.get("friction")) - 1.25) < 0.001, "chunk friction setter round-trips")
		_check(absf(float(emitter.get("bounce")) - 0.35) < 0.001, "chunk bounce setter round-trips")
		_check(absf(float(emitter.get("linear_damp")) - 0.4) < 0.001, "chunk linear_damp setter round-trips")
		_check(absf(float(emitter.get("angular_damp")) - 0.6) < 0.001, "chunk angular_damp setter round-trips")
		_check(absf(float(emitter.get("shrink_time")) - 1.0) < 0.001, "chunk shrink_time setter round-trips")
		root.add_child(emitter)
		await physics_frame
		emitter.call("spawn_at", Vector3(0, 3, 0), Vector3(0, 1, 0), 6)
		_check(int(emitter.call("get_active_chunk_count")) == 6, "chunk burst spawns chunks")
		await physics_frame
		# CORE-3: the pair count must come from the per-step cache and be live
		# right after a spawn (the freshly spawned chunks overlap each other).
		var pairs: int = PhysicsServer3D.get_process_info(PhysicsServer3D.INFO_COLLISION_PAIRS)
		_check(pairs > 0, "INFO_COLLISION_PAIRS reports contacts from the step cache (%d)" % pairs)
		# CORE-5: the PVD debug bridge defaults off (it used to dial
		# localhost:5425 unconditionally on every debug build).
		_check(ProjectSettings.get_setting("physics/physx_3d/debug/pvd", true) == false,
				"physics/physx_3d/debug/pvd is registered and defaults off (CORE-5)")
		await physics_frame
		emitter.free()

	# ---------------- blast pipeline ---------------------------------------
	if not (ClassDB.class_exists("PhysXBlastAuthoring")
			and ClassDB.class_exists("PhysXBlastAsset")
			and ClassDB.class_exists("PhysXDestructible3D")):
		print("BLAST-PROBE SKIP: Blast classes not registered (blast=no build?) — chunk-emitter part ran")
		_finish()
		return

	var authoring: Object = ClassDB.instantiate("PhysXBlastAuthoring")
	var mesh := BoxMesh.new()
	mesh.size = Vector3(2, 2, 2)
	var asset: Object = authoring.call("fracture_mesh", mesh, 12, 12345)
	if asset == null or asset.call("get_chunk_count") < 2:
		_fail("probe asset authoring failed")
		_finish()
		return

	# Valid asset loads end to end.
	var good: Node = ClassDB.instantiate("PhysXDestructible3D")
	good.set("blast_asset", asset)
	good.set("dynamic", false)
	root.add_child(good)
	await process_frame
	await physics_frame
	_check(bool(good.call("is_loaded")), "valid authored asset loads (is_loaded)")
	var pieces: int = good.call("apply_radial_damage", Vector3(0, 2, 0), 10.0, 0.0, 5.0)
	_check(pieces >= 1, "radial damage fractures (%d pieces)" % pieces)
	await physics_frame
	var second: int = good.call("apply_radial_damage", Vector3(0, 2, 0), 40.0, 0.0, 5.0)
	_check(second == 0, "second damage on a fully-fractured depth-1 asset no-ops (%d)" % second)
	await physics_frame
	await physics_frame
	good.free()

	# BLAST-3: auto_mass must survive a scene save/load round-trip. The mass
	# property used to serialize the auto-computed value, and deserialization
	# called set_mass() with it — silently flipping auto_mass off on every
	# load. PackedScene pack/instantiate is the same property-serialization
	# path the editor's save uses.
	var keep: Node = ClassDB.instantiate("PhysXDestructible3D")
	keep.set("blast_asset", asset)
	root.add_child(keep) # ENTER_WORLD loads the asset and auto-computes mass
	await process_frame
	await physics_frame
	var loaded_mass: float = float(keep.get("mass"))
	_check(loaded_mass != 1.0, "auto-mass computed a non-default value (%.1f)" % loaded_mass)
	root.remove_child(keep)
	var packed := PackedScene.new()
	var perr := packed.pack(keep)
	_check(perr == OK, "destructible packs into a scene")
	var revived: Node = packed.instantiate()
	_check(bool(revived.get("auto_mass")), "auto_mass survives the scene round-trip (BLAST-3)")
	keep.free()
	revived.free()

	# Corrupt bytes: fail cleanly, no crash, retryable (BLAST-2/BLAST-4).
	# Case 1: a truncated REAL asset — the realistic corruption (interrupted
	# write / bad download): the block declares more bytes than it carries.
	var good_bytes: PackedByteArray = asset.call("get_asset_bytes")
	var truncated := PackedByteArray()
	truncated.resize(int(good_bytes.size() * 0.6))
	for i in range(truncated.size()):
		truncated[i] = good_bytes[i]
	# Case 2: same length, arbitrary bytes.
	var garbage := PackedByteArray()
	garbage.resize(good_bytes.size())
	for i in range(garbage.size()):
		garbage[i] = (i * 37 + 11) % 251

	var corrupt_paths: Array = []
	for c in [[truncated, "res://blast_truncated.bin"], [garbage, "res://blast_garbage.bin"]]:
		var fp := FileAccess.open(c[1], FileAccess.WRITE)
		fp.store_buffer(c[0])
		fp.close()
		corrupt_paths.append(ProjectSettings.globalize_path(c[1]))

	for attempt in range(5):
		for ci in range(corrupt_paths.size()):
			var bad: Node = ClassDB.instantiate("PhysXDestructible3D")
			bad.set("asset_path", corrupt_paths[ci])
			bad.set("dynamic", false)
			root.add_child(bad)
			await process_frame
			await physics_frame
			_check(bool(bad.call("is_loaded")) == false,
					"corrupt asset load %d/%d rejected cleanly (is_loaded=false)" % [attempt + 1, ci + 1])
			bad.free()
	for p in corrupt_paths:
		var gp := p as String
		DirAccess.remove_absolute(gp)

	_finish()

func _finish() -> void:
	var ok := not _failed
	print("BLAST-PROBE ", "PASS" if ok else "FAIL")
	TestReport.write(TestReport.json_path_from_args(), "blast", "PHYSX-BLAST-002",
			"pipeline probe: load/corrupt-reject/retry + chunk-emitter parity",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
