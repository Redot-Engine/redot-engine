extends SceneTree

# REG-0021 v2: verifies narrowphase contact generation via
# PhysicsDirectSpaceState3D.intersect_shape(). Replaces _probe_contacts.gd's
# debug-contact API (space_set_debug_contacts / space_get_contact_count /
# space_get_contacts), which is not exposed to GDScript in this build.
#
# Each pair is placed with an exactly-computed 0.2-0.3 overlap; the dynamic
# side has gravity off and can_sleep off, and the query runs at step 2, so
# depenetration drift is negligible. A sphere at the overlap band center
# must return BOTH shapes if narrowphase generates contacts.

var space: RID
var step := 0
var cases := []  # [label, sphere_rid, probe_midpoint]

func _initialize() -> void:
	space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)

	var box := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(box, Vector3(0.5, 0.5, 0.5))
	var cap := PhysicsServer3D.capsule_shape_create()
	PhysicsServer3D.shape_set_data(cap, {"radius": 0.25, "height": 1.2}) # full 1.7, half 0.85
	var cone := PhysicsServer3D.custom_shape_create()
	PhysicsServer3D.shape_set_data(cone, {"type": "cone", "radius": 0.35, "height": 1.0})
	var tetra_pts := PackedVector3Array([
		Vector3(-0.35, -0.245, -0.28), Vector3(0.42, -0.21, -0.14),
		Vector3(-0.07, -0.175, 0.42), Vector3(0.0, 0.42, 0.0)])
	var convex := PhysicsServer3D.convex_polygon_shape_create()
	PhysicsServer3D.shape_set_data(convex, tetra_pts) # y in [-0.245, 0.42], half 0.3325

	var sphere := PhysicsServer3D.sphere_shape_create()

	# The query shape must be a Shape3D resource (server RIDs are not accepted
	# by PhysicsShapeQueryParameters3D); it only needs to sit inside the pair's
	# overlap band, so a small sphere resource is used for every case.
	var query_sphere := SphereShape3D.new()
	query_sphere.radius = 0.05

	# (label, A static shape, B dynamic shape, A center y, B half-extent y, overlap depth)
	var setup := [
		["box x box     ", box, box, 0.5, 0.5, 0.3],
		["box x capsule ", box, cap, 0.5, 0.85, 0.3],
		["cone x cone   ", cone, cone, 0.5, 0.5, 0.2],
		["box x convex  ", box, convex, 0.5, 0.3325, 0.2],
		["capsule x cap ", cap, cap, 0.85, 0.85, 0.3],
	]
	for s in setup:
		var ay: float = s[3]
		var bh: float = s[4]
		var depth: float = s[5]
		var a := PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_space(a, space)
		PhysicsServer3D.body_set_mode(a, PhysicsServer3D.BODY_MODE_STATIC)
		PhysicsServer3D.body_add_shape(a, s[1], Transform3D())
		PhysicsServer3D.body_set_state(a, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(0, ay, 0)))
		var b := PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_space(b, space)
		PhysicsServer3D.body_add_shape(b, s[2], Transform3D())
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_CAN_SLEEP, false)
		# B's bottom sits `depth` below A's top: A top = 2*ay, B center = A top + bh - depth
		var by: float = 2.0 * ay + bh - depth
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(0, by, 0)))
		# probe at the center of the overlap band [A_top - depth, A_top]
		cases.append([s[0], query_sphere, Vector3(0, 2.0 * ay - depth * 0.5, 0)])

func _physics_process(_d: float) -> bool:
	step += 1
	if step != 2:
		return false
	var state: PhysicsDirectSpaceState3D = PhysicsServer3D.space_get_direct_state(space)
	var ok_all := true
	for c in cases:
		var params := PhysicsShapeQueryParameters3D.new()
		params.shape = c[1]
		params.transform = Transform3D(Basis(), c[2])
		params.collide_with_bodies = true
		var hits: Array = state.intersect_shape(params, 8)
		var ok: bool = hits.size() >= 2
		ok_all = ok_all and ok
		print("[contacts-v2] %s hits=%d -> %s" % [c[0], hits.size(), "OK" if ok else "MISS"])
	print("[contacts-v2] RESULT: ", "PASS" if ok_all else "FAIL")
	quit(0 if ok_all else 1)
	return true
