extends CharacterBody2D
class_name CharacterBody2DJumpAndRun

const WALKING_SPEED:float = 300.0
const JUMP_VELOCITY:float = -400.0

var doulbe_jump:bool = false

func _ready() -> void:
	# ground and walls create different collision types see is_on_floor().
	motion_mode = CharacterBody2D.MOTION_MODE_GROUNDED

func _physics_process(delta: float) -> void:
	if not is_on_floor():
		# Add the gravity\falling if in air:
		velocity += get_gravity() * delta
	else:
		# when landed reset the double jump
		doulbe_jump = true

	# Handle (double) jump:
	if Input.is_action_just_pressed("ui_accept")\
			|| Input.is_action_just_pressed("ui_up"):
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
	var direction := Input.get_axis("ui_left", "ui_right")
	if direction:
		velocity.x = direction * WALKING_SPEED
	else:
		velocity.x = move_toward(velocity.x, 0, WALKING_SPEED)

	move_and_slide()
