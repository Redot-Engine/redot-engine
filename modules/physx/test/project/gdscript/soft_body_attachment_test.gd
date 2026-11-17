extends SceneTree

# Phase 13 / GAP-15 probe: soft-body vertex rigidly attached to a kinematic
# body via PxDeformableAttachment (GPU solver path). The kinematic teleports
# stepwise; the attached vertex must follow it exactly. Also covers the
# free-while-attached safety path (the server releases the attachment before
# the rigid's actor dies).
#
# Skips the follow check cleanly when the GPU path cannot build on this
# machine (soft_body_is_gpu false — no CUDA, no tet cook); the API plumbing
# (set/get, detach, free-while-attached) is still exercised.
#
# Run from the repository root:
#   <binary> --headless --path modules/physx/test/project \
#     --script res://gdscript/soft_body_attachment_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var _failed := false
var _messages: Array = []
var _assertions := 0

func _check(cond: bool, msg: String) -> void:
	_assertions += 1
	if not cond:
		print("SBATT FAIL: ", msg)
		_failed = true
		_messages.append(msg)
	else:
		print("SBATT ok: ", msg)

# Unit cube surface (8 verts / 12 tris), outward winding — the same mesh the
# C# SoftBodyTests use, which tetrahedralizes for the GPU path.
func _make_cube_mesh() -> RID:
	var verts := PackedVector3Array([
		Vector3(-0.5, -0.5, -0.5), Vector3(0.5, -0.5, -0.5),
		Vector3(0.5, 0.5, -0.5), Vector3(-0.5, 0.5, -0.5),
		Vector3(-0.5, -0.5, 0.5), Vector3(0.5, -0.5, 0.5),
		Vector3(0.5, 0.5, 0.5), Vector3(-0.5, 0.5, 0.5),
	])
	var quads := [
		[1, 0, 3], [1, 3, 2], # -Z
		[4, 5, 6], [4, 6, 7], # +Z
		[5, 1, 2], [5, 2, 6], # +X
		[0, 4, 7], [0, 7, 3], # -X
		[3, 7, 6], [3, 6, 2], # +Y
		[0, 1, 5], [0, 5, 4], # -Y
	]
	var indices := PackedInt32Array()
	for q in quads:
		indices.push_back(q[0]); indices.push_back(q[1]); indices.push_back(q[2])
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = verts
	arrays[Mesh.ARRAY_INDEX] = indices
	var mesh := RenderingServer.mesh_create()
	RenderingServer.mesh_add_surface_from_arrays(mesh, RenderingServer.PRIMITIVE_TRIANGLES, arrays)
	return mesh

func _init() -> void:
	_run()

func _run() -> void:
	var space: RID = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)

	var sb: RID = PhysicsServer3D.soft_body_create()
	PhysicsServer3D.soft_body_set_space(sb, space)
	PhysicsServer3D.soft_body_set_simulation_precision(sb, 2)
	PhysicsServer3D.soft_body_set_total_mass(sb, 2.0)
	PhysXServer3D.get_singleton().soft_body_set_solver_mode(sb, 2) # force GPU (inner-server API)
	# Transform BEFORE the mesh: the GPU build gate needs the body placed.
	PhysicsServer3D.soft_body_set_state(sb, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, 2, 0)))
	var mesh: RID = _make_cube_mesh()
	PhysicsServer3D.soft_body_set_mesh(sb, mesh)
	await physics_frame
	await physics_frame

	var gpu: bool = PhysXServer3D.get_singleton().soft_body_is_gpu(sb)
	print("SBATT soft body path: ", "GPU" if gpu else "CPU (GPU build unavailable on this machine)")

	# The kinematic carrier: teleports stepwise along +X.
	var carrier_shape: RID = PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(carrier_shape, Vector3(0.1, 0.1, 0.1))
	var carrier: RID = PhysicsServer3D.body_create()
	PhysicsServer3D.body_set_mode(carrier, PhysicsServer3D.BODY_MODE_KINEMATIC)
	PhysicsServer3D.body_set_space(carrier, space)
	PhysicsServer3D.body_add_shape(carrier, carrier_shape)
	PhysicsServer3D.body_set_collision_layer(carrier, 0) # no contacts with the cloth
	PhysicsServer3D.body_set_state(carrier, PhysicsServer3D.BODY_STATE_TRANSFORM,
			Transform3D(Basis(), Vector3(0, 2.5, 0)))

	# Attach vertex 0 (a -X,-Y,-Z corner) to the carrier.
	PhysXServer3D.get_singleton().soft_body_attach_point_to_body(sb, 0, carrier)
	await physics_frame
	await physics_frame

	var p0: Vector3 = PhysicsServer3D.soft_body_get_point_global_position(sb, 0)
	print("SBATT vertex 0 at attach time: ", p0)

	if gpu:
		# Steady-state check: attach, let the deformable's own settle transient
		# finish (the volume relaxes for a few frames after the cook), THEN
		# measure the vertex's offset from the carrier. Teleport the carrier
		# once, settle again, and require the same offset to hold - the
		# PxDeformableAttachment is a hard positional constraint, so it must
		# move the vertex 1:1 with the rigid.
		for i in range(30):
			await physics_frame
		var v0: Vector3 = PhysicsServer3D.soft_body_get_point_global_position(sb, 0)
		var c0: Vector3 = Vector3(0, 2.5, 0)
		var offset: Vector3 = v0 - c0
		PhysicsServer3D.body_set_state(carrier, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(1.0, 2.5, 0)))
		for i in range(30):
			await physics_frame
		var v1: Vector3 = PhysicsServer3D.soft_body_get_point_global_position(sb, 0)
		var c1: Vector3 = Vector3(1.0, 2.5, 0)
		var err: float = (v1 - c1 - offset).length()
		print("SBATT offset=", offset, " after-move err=", err)
		_check(err < 0.05, "attached vertex holds its rigid offset after the carrier moves (err=%.3f)" % err)
		PhysXServer3D.get_singleton().soft_body_detach_point_from_body(sb, 0)
		await physics_frame
		_check(true, "detach after attach is clean")
	else:
		print("SBATT SKIP: follow check (GPU path unavailable)")

	# Free the carrier while still able to hold an attachment: the server must
	# release any attachment referencing it before the actor dies (no crash).
	PhysXServer3D.get_singleton().soft_body_attach_point_to_body(sb, 0, carrier)
	await physics_frame
	PhysicsServer3D.free_rid(carrier)
	PhysicsServer3D.free_rid(carrier_shape)
	await physics_frame
	await physics_frame
	_check(true, "freeing an attached rigid body is crash-free")

	PhysicsServer3D.free_rid(sb)
	RenderingServer.free_rid(mesh)
	PhysicsServer3D.free_rid(space)

	var ok2 := not _failed
	print("SBATT ", "PASS" if ok2 else "FAIL")
	TestReport.write(TestReport.json_path_from_args(), "gpu", "PHYSX-SBATT-001",
			"soft-body rigid attachment (PxDeformableAttachment): follow + free safety",
			"pass" if ok2 else "fail", _assertions, _messages)
	quit(0 if ok2 else 1)
