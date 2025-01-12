extends CharacterBody2D
class_name CharacterBody2DTopDown

const WALK_SPEED:float = 300.0

func _ready() -> void:
	# top down mode all collisions are walls
	motion_mode = CharacterBody2D.MOTION_MODE_FLOATING

func _physics_process(delta: float) -> void:
	# Get the input direction and handle the movement/deceleration.
	# As good practice, you should replace UI actions with custom gameplay actions.
	var direction := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
	if direction:
		velocity = direction * WALK_SPEED
	else:
		# quick stopping
		velocity = Vector2(0.0, 0.0)

	move_and_slide()
