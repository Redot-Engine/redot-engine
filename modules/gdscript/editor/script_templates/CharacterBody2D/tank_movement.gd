extends CharacterBody2D
class_name CharacterBody2DTopDownTank

const DRIVE_SPEED:float = 120.0
const TURN_SPEED:float = 2.0

func _ready() -> void:
	# top down mode all collisions are walls
	motion_mode = CharacterBody2D.MOTION_MODE_FLOATING

func _physics_process(delta: float) -> void:
	# Get the input direction and handle the movement/deceleration.
	# As good practice, you should replace UI actions with custom gameplay actions.
	# see Project > Project Settings ... > Input Map > Show Build In Actions.
	# arrow keys up and down move the "tank" and left and right turns it.
	var movement := Input.get_axis("ui_up", "ui_down")
	var turning := Input.get_axis("ui_left", "ui_right")
	# if the movement keys are pressed apply the drive speed
	# into the facting direction "transform.y" if your tank looks up by default
	# change it to transform.x id the default facing is left.
	# you can invert all of this by multiplying with -1.0 if you face in the opposide direction
	if movement:
		velocity = transform.y * movement * DRIVE_SPEED
	else:
		velocity.x = move_toward(velocity.x, 0, 10.0)
		velocity.y = move_toward(velocity.y, 0, 10.0)

	if turning:
		rotation += turning * TURN_SPEED * delta

	move_and_slide()
