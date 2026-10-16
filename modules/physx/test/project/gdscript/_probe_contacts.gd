# REG-0021 diagnostic: is contact generation broken for cone/convex pairs,
# or do the bodies just slide off (dynamics)?
#
# For each pair: place the shapes PENETRATING (dynamic side has gravity_scale=0,
# can_sleep=false) and read the space's debug contacts. Contacts => narrowphase
# works. No contacts after 20 frames => narrowphase genuinely misses.
extends SceneTree

var space: RID
var shapes := {}
var step := 0

func _initialize() -> void:
	space = PhysicsServer3D.space_create()
	PhysicsServer3D.space_set_active(space, true)
	PhysicsServer3D.space_set_debug_contacts(space, 64)

	# Shapes
	var cone := PhysicsServer3D.custom_shape_create()
	PhysicsServer3D.shape_set_data(cone, {"type": "cone", "radius": 0.35, "height": 1.0})
	var cap := PhysicsServer3D.capsule_shape_create()
	PhysicsServer3D.shape_set_data(cap, {"radius": 0.25, "height": 1.2})
	var tetra_pts := PackedVector3Array([
		Vector3(-0.35, -0.245, -0.28), Vector3(0.42, -0.21, -0.14),
		Vector3(-0.07, -0.175, 0.42), Vector3(0.0, 0.42, 0.0)])
	var convex := PhysicsServer3D.convex_polygon_shape_create()
	PhysicsServer3D.shape_set_data(convex, tetra_pts)
	var box := PhysicsServer3D.box_shape_create()
	PhysicsServer3D.shape_set_data(box, Vector3(1.0, 1.0, 1.0))  # half 0.5

	shapes = {"cone": cone, "cap": cap, "convex": convex, "box": box}

	# (name, static shape, static center, dyn shape, dyn center)
	var half := {"cone": 0.5, "cap": 0.6, "convex": 0.45, "box": 0.5}
	var pairs := [
		["cone x box    ", "box", "cone"],
		["cone x capsule", "cap", "cone"],
		["cone x cone   ", "cone", "cone"],
		["convex x cone ", "cone", "convex"],
		["convex x convex", "convex", "convex"],
	]
	for p in pairs:
		var a_s: RID = shapes[p[1]]
		var b_s: RID = shapes[p[2]]
		var ha: float = half[p[1]]
		var hb: float = half[p[2]]
		var a := PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_space(a, space)
		PhysicsServer3D.body_set_mode(a, PhysicsServer3D.BODY_MODE_STATIC)
		PhysicsServer3D.body_add_shape(a, a_s, Transform3D())
		PhysicsServer3D.body_set_state(a, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(0, ha, 0)))  # bottom at y=0, top at 2*ha
		var b := PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_space(b, space)
		PhysicsServer3D.body_add_shape(b, b_s, Transform3D())
		PhysicsServer3D.body_set_param(b, PhysicsServer3D.BODY_PARAM_GRAVITY_SCALE, 0.0)
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_CAN_SLEEP, false)
		# bottom of B overlaps top of A by 0.1
		PhysicsServer3D.body_set_state(b, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(0, 2.0 * ha + hb - 0.1, 0)))

func _physics_process(_d: float) -> bool:
	step += 1
	if step == 20:
		var count: int = PhysicsServer3D.space_get_contact_count(space)
		var contacts: PackedVector3Array = PhysicsServer3D.space_get_contacts(space)
		print("PENETRATION TEST: contact_count=%d" % count)
		for i in range(0, contacts.size(), 3):
			print("   contact point: %s" % str(contacts[i]))
		# also verify the bodies did not move (gravity off on dynamic side)
		quit(0)
		return true
	return false
