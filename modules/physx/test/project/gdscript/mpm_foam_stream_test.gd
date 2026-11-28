extends SceneTree

# MPM-foam variant of godot-physx-example's test/gpu/particle_foam_test.gd:
# the same faucet stream (12k particles/s at -7 m/s into a floor) with the
# solver switched to the Vulkan MPM backend, so the diffuse layer is our
# Vulkan foam instead of NVIDIA's (guarded) CUDA diffuse particles.
# Same pass criteria: peak > 500, bounded by capacity, ebbing after the
# faucet stops. Windowed run (MPM needs a RenderingDevice).

const CAP := 40000
const FOAM_CAP := 20000

var _fluid: PhysXParticleFluid3D
var _tick := 0
var _peak_foam := 0

func _initialize() -> void:
	print("[foam-mpm] engine = ", ProjectSettings.get_setting("physics/3d/physics_engine", "?"))
	var root := Node3D.new()
	get_root().add_child(root)

	var fb := StaticBody3D.new()
	var fc := CollisionShape3D.new()
	var fs := BoxShape3D.new()
	fs.size = Vector3(8, 1, 8)
	fc.shape = fs
	fb.add_child(fc)
	fb.position = Vector3(0, -0.5, 0)
	root.add_child(fb)

	_fluid = PhysXParticleFluid3D.new()
	# No solver set: exercises the Auto rule (foam enabled -> MPM backend)
	# exactly as godot-physx-example's gpu/physx_fluid.gd configures it.
	_fluid.spawn_on_ready = false
	_fluid.particle_count = CAP
	_fluid.particle_size = 0.06
	_fluid.emitting = true
	_fluid.emission_rate = 12000.0
	_fluid.emission_radius = 0.1
	_fluid.emission_velocity = Vector3(0, -7, 0)
	_fluid.position = Vector3(0, 2.2, 0) # domain center; domain spans the stream + floor
	_fluid.mpm_domain_size = Vector3(5, 6, 5)
	_fluid.foam_enabled = true
	_fluid.foam_particle_count = FOAM_CAP
	_fluid.foam_lifetime = 1.0
	# The example's PBD-scale threshold (200) maps to 2.0 MPM units, which is
	# below the kinetic-energy term of a 7 m/s stream itself -- the whole
	# column foams and the ring saturates. 500 (-> 5.0) gates spawning to the
	# impact zone, where the divergence term spikes.
	_fluid.foam_threshold = 500.0
	root.add_child(_fluid)
	_fluid.mpm_colliders = [fb.get_path()]
	_fluid.spawn()

func _physics_process(_d: float) -> bool:
	_tick += 1
	if _fluid == null:
		return true
	var live := _fluid.get_live_particle_count()
	var foam := _fluid.get_live_foam_count()
	_peak_foam = maxi(_peak_foam, foam)
	if _tick == 10 and live == 0:
		print("[foam-mpm] 0 particles after emitting (no RenderingDevice?) -> SKIP")
		quit(0)
		return true
	if _tick % 60 == 0:
		print("[foam-mpm] tick %3d  fluid=%d  foam=%d" % [_tick, live, foam])
	if _tick == 240:
		# Stop the faucet; the pool keeps churning but foam output should ebb.
		_fluid.emitting = false
	if _tick == 1140:
		# 15 s after the faucet stops. A contained tank keeps sloshing (and
		# sloshing water legitimately keeps spawning foam), so assert a clear
		# ebb rather than a full drain.
		var foam_now := _fluid.get_live_foam_count()
		var spawned: bool = _peak_foam > 500
		var bounded: bool = _peak_foam <= FOAM_CAP
		var ebbed: bool = foam_now < _peak_foam * 0.9
		var ok: bool = spawned and bounded and ebbed
		print("[foam-mpm] peak=%d  after_settle=%d -> %s" %
				[_peak_foam, foam_now, "PASS" if ok else "FAIL"])
		quit(0 if ok else 1)
	return false
