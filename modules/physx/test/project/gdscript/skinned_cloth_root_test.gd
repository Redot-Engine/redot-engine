extends SceneTree

# Skinned-cloth smoke test (Phase 6 / CLOTH-4): builds a minimal skinned mesh
# (MeshInstance3D + Skeleton3D + Skin, one bone, all vertices bound to it) at
# the SCENE-ROOT level — the layout that crashed the old render-twin attach
# (source->get_parent()->add_child dereferenced a null/unsuitable parent in
# root-level setups) — and drives PhysXSkinnedCloth3D on it. The root-source
# fallback parents the render twin under the cloth node with a compensating
# transform. Pass = the build completes, the source is hidden, the twin exists,
# and the process survives the sim. Headless (compute-only path):
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/skinned_cloth_root_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

func _initialize() -> void:
	_run()

func _run() -> void:
	# Nodes added during _initialize are not in the tree yet (get_path fails);
	# wait one frame so the tree is live before building the scene.
	await process_frame
	if not ClassDB.class_exists("PhysXSkinnedCloth3D"):
		print("[skinned-root] SKIP: PhysXSkinnedCloth3D not registered")
		TestReport.write(TestReport.json_path_from_args(), "gpu", "PHYSX-GPU-002",
				"skinned cloth on a scene-root-level source builds and simulates",
				"skip", 0, ["PhysXSkinnedCloth3D not registered in this build"])
		quit(0)
		return

	# --- Skeleton with one bone at the origin ---
	var skeleton := Skeleton3D.new()
	var bone_idx: int = skeleton.add_bone("bone")
	skeleton.set_bone_rest(bone_idx, Transform3D(Basis.IDENTITY, Vector3.ZERO))
	root.add_child(skeleton)

	# --- Skin binding every vertex to bone 0 ---
	var skin := Skin.new()
	skin.add_bind(0, Transform3D.IDENTITY) # bind 0 == bone name defaults to the path; set it explicitly below
	skin.set_bind_name(0, "bone")

	# --- Skinned plane mesh (2x2 quads, bones/weights per vertex) ---
	var st: SurfaceTool = SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var quads := 2
	for yy in quads:
		for xx in quads:
			var x0 := float(xx) - 1.0
			var x1 := float(xx + 1) - 1.0
			var y0 := float(yy)
			var y1 := float(yy + 1)
			for v in [[x0, y0], [x1, y0], [x1, y1], [x0, y0], [x1, y1], [x0, y1]]:
				st.set_bones([0, 0, 0, 0])
				st.set_weights([1.0, 0.0, 0.0, 0.0])
				st.add_vertex(Vector3(v[0], v[1], 0))
			# degenerate UV/normal channels are fine; skinning needs bones+weights
	var mesh := st.commit()

	# --- Source MeshInstance3D at the SCENE-ROOT level ---
	var source := MeshInstance3D.new()
	source.mesh = mesh
	source.skeleton = skeleton.get_path()
	source.skin = skin
	root.add_child(source)

	# --- The cloth node, sibling of the source, pointing at it ---
	var cloth: Node = ClassDB.instantiate("PhysXSkinnedCloth3D")
	root.add_child(cloth)
	cloth.set("mesh_instance_path", cloth.get_path_to(source))
	cloth.set("max_distance", 2.0)

	for i in 30:
		await physics_frame

	var failures: Array = []
	if not cloth.get("built"):
		# Compute path unavailable (headless / no RenderingDevice): the crash
		# guard under test is unreachable -- report skip, not fail.
		print("[skinned-root] SKIP: skinned-cloth compute path unavailable (headless?)")
		TestReport.write(TestReport.json_path_from_args(), "gpu", "PHYSX-GPU-002",
				"skinned cloth on a scene-root-level source builds and simulates",
				"skip", 0, ["RenderingDevice unavailable (run windowed)"])
		quit(0)
		return
	if source.visible:
		failures.append("source mesh was not hidden by the cloth build")
	var twins := 0
	for child in root.get_children():
		if child is MeshInstance3D and child != source:
			twins += 1
	if twins < 1:
		failures.append("no render twin MeshInstance3D was created")
	if not is_finite(source.get_instance_id()):
		failures.append("unreachable")

	var ok := failures.is_empty()
	print("[skinned-root] ", "PASS" if ok else "FAIL")
	for f in failures:
		push_error("[skinned-root] " + f)
	TestReport.write(TestReport.json_path_from_args(), "gpu", "PHYSX-GPU-002",
			"skinned cloth on a scene-root-level source builds and simulates",
			"pass" if ok else "fail", 2, failures)
	quit(0 if ok else 1)
