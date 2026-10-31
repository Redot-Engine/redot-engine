extends SceneTree

# Authoring input validation (audit F-30, PHYSX-BLAST-003).
#
#   fracture_mesh refuses bad input with a reason (get_last_error) instead of
#   feeding it to the native SDK: an index past the vertices, a partial
#   triangle, a non-finite vertex, a mesh with no volume. An inside-out mesh
#   is fractured with its triangles flipped (get_last_warnings says so) and
#   every piece faces outward.
#
# Run from the repository root:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/blast_authoring_validation_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var _json_path := ""
var _failed := false
var _messages: Array = []
var _assertions := 0

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		_failed = true
		_messages.append(msg)
		print("[blast-valid] FAIL: ", msg)
	else:
		print("[blast-valid] ok: ", msg)

func _mesh(verts: PackedVector3Array, idx: PackedInt32Array) -> ArrayMesh:
	var a := []
	a.resize(Mesh.ARRAY_MAX)
	a[Mesh.ARRAY_VERTEX] = verts
	a[Mesh.ARRAY_INDEX] = idx
	var m := ArrayMesh.new()
	m.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, a)
	return m

func _cube() -> Array:
	# A 2 m cube as 12 triangles (unwelded, like a Godot BoxMesh).
	var v := PackedVector3Array()
	var i := PackedInt32Array()
	for corner in 8:
		v.append(Vector3((corner & 1) * 2 - 1, ((corner >> 1) & 1) * 2 - 1, ((corner >> 2) & 1) * 2 - 1))
	# 6 faces, outward winding.
	var faces := [
		[0, 2, 3, 1], [4, 5, 7, 6], [0, 1, 5, 4],
		[2, 6, 7, 3], [0, 4, 6, 2], [1, 3, 7, 5],
	]
	for f in faces:
		var q: Array = f
		i.append_array([q[0], q[2], q[1], q[0], q[3], q[2]])
	return [v, i]

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	if not ClassDB.class_exists("PhysXBlastAuthoring"):
		print("[blast-valid] SKIP: PhysXBlastAuthoring not registered")
		TestReport.write(_json_path, "blast", "PHYSX-BLAST-003",
				"authoring input validation + get_last_error/warnings",
				"skip", 0, ["PhysXBlastAuthoring not registered in this build"])
		quit(0)
		return
	var authoring: Object = ClassDB.instantiate("PhysXBlastAuthoring")
	var cube: Array = _cube()
	var good := _mesh(cube[0], cube[1])

	# 1. An index past the vertices.
	var bad_idx: PackedInt32Array = cube[1].duplicate()
	bad_idx[0] = 500
	authoring.call("fracture_mesh", _mesh(cube[0], bad_idx), 9, 1)
	_check(String(authoring.call("get_last_error")).contains("points past"),
			"index past the vertices refused: %s" % authoring.call("get_last_error"))

	# 2. A partial triangle.
	var partial: PackedInt32Array = cube[1].duplicate()
	partial.resize(35)
	authoring.call("fracture_mesh", _mesh(cube[0], partial), 9, 1)
	_check(String(authoring.call("get_last_error")).contains("whole number of triangles"),
			"partial triangle refused: %s" % authoring.call("get_last_error"))

	# 3. A non-finite vertex.
	var nan_verts: PackedVector3Array = cube[0].duplicate()
	nan_verts[3] = Vector3(NAN, 0, 0)
	authoring.call("fracture_mesh", _mesh(nan_verts, cube[1]), 9, 1)
	_check(String(authoring.call("get_last_error")).contains("finite"),
			"NaN vertex refused: %s" % authoring.call("get_last_error"))

	# 4. A mesh with no volume (all vertices at one point).
	var flat := PackedVector3Array()
	for p in 8:
		flat.append(Vector3())
	authoring.call("fracture_mesh", _mesh(flat, cube[1]), 9, 1)
	_check(String(authoring.call("get_last_error")).contains("no volume"),
			"zero-volume mesh refused: %s" % authoring.call("get_last_error"))

	# 5. A good mesh: no error, and pieces spawn.
	authoring.call("fracture_mesh", good, 9, 7)
	_check(String(authoring.call("get_last_error")).is_empty(),
			"a good mesh leaves no error: %s" % authoring.call("get_last_error"))

	# 6. An inside-out cube is flipped with a warning, and still fractures.
	var reversed_idx: PackedInt32Array = cube[1].duplicate()
	for t in reversed_idx.size() / 3:
		var tmp = reversed_idx[t * 3 + 1]
		reversed_idx[t * 3 + 1] = reversed_idx[t * 3 + 2]
		reversed_idx[t * 3 + 2] = tmp
	var flipped_asset = authoring.call("fracture_mesh", _mesh(cube[0], reversed_idx), 9, 7)
	_check(flipped_asset != null and int(flipped_asset.call("get_chunk_count")) >= 2,
			"an inside-out cube still fractures")
	var warns: PackedStringArray = authoring.call("get_last_warnings")
	var flip_warned := false
	for w in warns:
		if String(w).contains("inside out"):
			flip_warned = true
	_check(flip_warned, "the flip is reported: %s" % [warns])

	_finish()

func _finish() -> void:
	var ok := not _failed
	print("[blast-valid] ", "PASS" if ok else "FAIL")
	TestReport.write(_json_path, "blast", "PHYSX-BLAST-003",
			"authoring input validation + get_last_error/warnings",
			"pass" if ok else "fail", _assertions, _messages)
	quit(0 if ok else 1)
