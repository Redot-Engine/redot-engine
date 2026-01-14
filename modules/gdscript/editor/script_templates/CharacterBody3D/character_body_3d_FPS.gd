extends CharacterBody3D
class_name CharacterBody3DFps

const WALKING_SPEED:float = 5.0
const JUMP_VELOCITY:float = 4.5

# this is a first person style example please add a Camera3D
# to your CharacterBody3D to activate the mouse look
@onready var camera_3d:Camera3D = $Camera3D

# used to set a point to rotate to
var rotation_target:Vector3 = Vector3(0.0, 0.0, 0.0)
var mouse_sensitivity:float = 700.0
var doulbe_jump:bool = false

# this method will be run once the scene is loaded:
func _ready() -> void:
	# catches the mouse inside the window
	# is used to avoid to click outside of the window
	# and to measure the distance the mouse traveled per delta
	if camera_3d != null:
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

# this method runs every circle in the main loop:
func _physics_process(delta: float) -> void:
	if not is_on_floor():
		# Add the gravity\falling if in air:
		velocity += get_gravity() * delta
	else:
		# when landed reset the double jump
		doulbe_jump = true

	# Handle (double) jump:
	if Input.is_action_just_pressed("ui_accept"):
		# first jump
		if is_on_floor():
			velocity.y = JUMP_VELOCITY
		# second jump
		elif doulbe_jump:
			doulbe_jump = false
			velocity.y = JUMP_VELOCITY
		# can't jump more


	# Get the input direction and handle the movement/deceleration.
	# As good practice, you should replace UI actions with custom gameplay actions.
	# see Project > Project Settings ... > Input Map > Show Build In Actions.
	# The function return a vector normed to length of 1.0 on basis of the four actions given.
	var input_dir := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	# the moevement will be multiplied by the basis to adjust to scaling of the player.
	var direction := (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()

	if direction:
		# move player into the dirextion
		velocity.x = direction.x * WALKING_SPEED
		velocity.z = direction.z * WALKING_SPEED
	else:
		# deceleration stop player slowly
		velocity.x = move_toward(velocity.x, 0, WALKING_SPEED)
		velocity.z = move_toward(velocity.z, 0, WALKING_SPEED)

	# moves player on basis of velocity and let him bump into other objects.
	move_and_slide()

	# if there is a Camera3D node under the CharacterBody3D node
	# it will be used to turn the player and the camera with the mouse
	if camera_3d != null:
		# Rotation of the camera:
		camera_3d.rotation.z = lerp_angle(camera_3d.rotation.z, -rotation_target.x * 30 * delta, delta * 5)
		camera_3d.rotation.x = lerp_angle(camera_3d.rotation.x, rotation_target.x, delta * 30)
		# Rotation of the player:
		rotation.y = lerp_angle(rotation.y, rotation_target.y, delta * 30)


func _input(event: InputEvent) -> void:
	# saves the rotation on the relative movement of the mouse.
	# if there is no camera function will be stopped early
	if camera_3d == null:
		return

	if event is InputEventMouseMotion:
		rotation_target.y -= event.relative.x / mouse_sensitivity
		rotation_target.x -= event.relative.y / mouse_sensitivity
	# limit view in up and down direction:
	rotation_target.x = clamp(rotation_target.x, deg_to_rad(-90), deg_to_rad(90))
