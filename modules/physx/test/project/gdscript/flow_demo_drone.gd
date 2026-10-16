extends Node3D
# Demo companion for flow_demo.tscn: orbits a small box FlowCollider3D around
# the fire plume, stirring the smoke -- exercises moving-collider velocity
# coupling (estimated from transform differencing) without any physics body.

var _angle := 0.0

@onready var _collider := PhysXFlowCollider3D.new()

func _ready() -> void:
	_collider.shape = PhysXFlowCollider3D.SHAPE_BOX
	_collider.size = Vector3(0.8, 0.5, 0.8)
	add_child(_collider)
	var flow: Node3D = get_parent().get_node("Flow")
	var paths: Array[NodePath] = [NodePath("../Ground"), NodePath("../Ball"), get_path_to(_collider)]
	flow.set("colliders", paths)

func _process(delta: float) -> void:
	_angle += delta * 1.2
	position = Vector3(cos(_angle) * 2.5, 1.0 + 0.4 * sin(_angle * 2.3), sin(_angle) * 2.5)
	rotate_y(delta * 2.0)
