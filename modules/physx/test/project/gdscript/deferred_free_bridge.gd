# Tiny bridge for lifecycle tests: frees a server RID from the message-queue
# flush (loaded by path from C#; no class_name so it never depends on the
# global class cache).
extends Object


func free_rid(rid: RID) -> void:
	PhysicsServer3D.free_rid(rid)
