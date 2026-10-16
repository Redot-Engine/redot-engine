# GPU-simulation smoke test for the PhysX module (headless).
#
# Run:
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project --script res://gdscript/gpu_smoke_test.gd
#
# Validates the merged GPU feature family programmatically:
#   * PhysXServer3D singleton is live (the startup log separately shows
#     "PhysX: CUDA context ready on device '...'" when a device is present);
#   * PhysXCloth3D builds and steps on the GPU deformable-surface path
#     (is_gpu_accelerated() reports which), and falls back to the built-in
#     XPBD CPU solver with simulation_mode = SIM_CPU;
#   * PhysXParticleFluid3D spawns particles, emits, enables foam, and
#     survives the surface-mesh (GPU isosurface) rebuild.
#
# Result model: PASS/FAIL lines + summary, exit code 1 on failure.
extends SceneTree

const TestReport := preload("res://gdscript/test_report.gd")

var _failures: Array = []
var _checks := 0
var _json_path := ""

func _initialize() -> void:
	_json_path = TestReport.json_path_from_args()
	_run()

func _chk(ok: bool, msg: String) -> void:
	_checks += 1
	if ok:
		print("  PASS  %s" % msg)
	else:
		_failures.append(msg)
		print("  FAIL  %s" % msg)

func _run() -> void:
	print("=== PhysX GPU smoke test ===")

	# Stage gating for bisecting: --stages=cloth,fluid,foamearly,foam,surface (all by default).
	var stages := {"cloth": true, "fluid": true, "foamearly": true, "foam": true, "surface": true}
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--stages="):
			for s in {"cloth": 1, "fluid": 1, "foamearly": 1, "foam": 1, "surface": 1}:
				stages[s] = false
			for s in arg.substr(9).split(","):
				stages[s] = true
	print("  INFO  stages = %s" % str(stages))

	# --- 1. Backend live -----------------------------------------------------
	var server: Object = PhysXServer3D.get_singleton()
	_chk(server != null, "PhysXServer3D singleton is available")

	# --- 2. Cloth: GPU path + CPU fallback -----------------------------------
	if stages.cloth:
		var cloth: PhysXCloth3D = PhysXCloth3D.new()
		root.add_child(cloth)
		for i in 12:
			await physics_frame
		_chk(cloth.get_vertex_count() > 0, "cloth built (vertex_count=%d)" % cloth.get_vertex_count())
		print("  INFO  cloth is_gpu_accelerated() = %s" % cloth.is_gpu_accelerated())
		_chk(cloth.is_simulating(), "cloth is simulating")
		for i in 30:
			await physics_frame

		# CPU fallback: force the built-in XPBD solver.
		cloth.simulation_mode = 2 # SIM_CPU
		for i in 12:
			await physics_frame
		_chk(not cloth.is_gpu_accelerated(), "cloth CPU fallback active (simulation_mode=SIM_CPU)")
		_chk(cloth.get_vertex_count() > 0, "cloth still built on the CPU path")
		cloth.simulation_mode = 0 # back to SIM_AUTO
		cloth.queue_free()
		await process_frame

	# --- 3. Fluid: spawn / emitting / foam / surface mesh --------------------
	if stages.fluid:
		var fluid: PhysXParticleFluid3D = PhysXParticleFluid3D.new()
		# This stage deliberately validates the CUDA/PBD pipeline (spawn, emit,
		# the guarded foam path, the GPU isosurface rebuild) -- pin the backend:
		# with Auto, a foam-enabled fluid now resolves to the MPM/Vulkan backend
		# (whose diffuse layer works where the CUDA one is guarded), and MPM
		# cannot run under --headless.
		fluid.solver = PhysXParticleFluid3D.SOLVER_PBD
		fluid.position = Vector3(0, 2, 0) # fall onto the floor below for agitation
		root.add_child(fluid)
		await process_frame

		# Static floor so the falling stream splashes (foam spawns on agitation).
		var floor_body: RID = PhysicsServer3D.body_create()
		PhysicsServer3D.body_set_mode(floor_body, PhysicsServer3D.BODY_MODE_STATIC)
		var floor_shape: RID = PhysicsServer3D.box_shape_create()
		PhysicsServer3D.shape_set_data(floor_shape, Vector3(2, 0.05, 2))
		PhysicsServer3D.body_add_shape(floor_body, floor_shape)
		PhysicsServer3D.body_set_state(floor_body, PhysicsServer3D.BODY_STATE_TRANSFORM,
				Transform3D(Basis(), Vector3(0, 0.95, 0)))
		PhysicsServer3D.body_set_space(floor_body, root.world_3d.get_space())

		if stages.foamearly:
			# Enable foam BEFORE the first buffer exists: the first (and only)
			# buffer is then created with diffuse capacity, no destroy/recreate.
			fluid.set_foam_enabled(true)
		fluid.spawn()
		for i in 10:
			await physics_frame
		var n0: int = fluid.get_live_particle_count()
		_chk(n0 > 0, "fluid spawned particles (%d)" % n0)
		_chk(fluid.get_particle_positions().size() > 0, "particle positions readable")

		fluid.emission_velocity = Vector3(0, -8, 0) # hard impact -> foam
		fluid.set_emitting(true)
		for i in 60:
			await physics_frame
		var n1: int = fluid.get_live_particle_count()
		_chk(n1 >= n0, "emitting keeps/raises particle count (%d -> %d)" % [n0, n1])

		if stages.foam:
			fluid.set_foam_enabled(true)
			for i in 120:
				await physics_frame
			var foam_n: int = fluid.get_live_foam_count()
			print("  INFO  live foam count = %d (0 expected: see maxActiveDiffuseParticles note in physx_gpu_particle_fluid_3d.cpp)" % foam_n)
			# The foam enablement path must run WITHOUT corrupting GPU state.
			# Foam spawning itself is disabled on this PhysX build (SDK defect:
			# any active-diffuse allocation breaks the buffer's device copies),
			# so 0 foam particles is the expected, documented outcome.
			_chk(fluid.get_live_particle_count() > 0, "fluid unaffected by foam enablement")

		if stages.surface:
			# Surface mesh: rebuilds the fluid with the GPU isosurface extractor.
			fluid.set_surface_mesh(true)
			for i in 60:
				await physics_frame
			_chk(fluid.get_live_particle_count() > 0, "fluid alive after surface-mesh rebuild")
			print("  INFO  surface mesh enabled = %s" % fluid.is_surface_mesh())
		fluid.queue_free()
		PhysicsServer3D.free_rid(floor_body)
		PhysicsServer3D.free_rid(floor_shape)
		await process_frame

	print("=== %d checks, %d failures ===" % [_checks, _failures.size()])
	var ok := _failures.is_empty()
	TestReport.write(_json_path, "gpu", "PHYSX-GPU-001",
			"GPU family: cloth GPU path + CPU fallback, PBD fluid spawn/emit/surface",
			"pass" if ok else "fail", _checks, _failures)
	quit(0 if ok else 1)
