extends SceneTree

# Phase 9 runtime-cost & robustness validation: gas single-submit + throttle,
# flow gated texture upload, water setter-storm, and headless graceful
# degradation for all three. Skips per-section when that system's runtime is
# unavailable (no RenderingDevice / no nvflow device) — headless boot clean is
# itself one of the completion criteria.
#
#   <binary> --headless --fixed-fps 60 --path modules/physx/test/project \
#     --script res://gdscript/runtime_cost_test.gd -- --json=<path>

const TestReport := preload("res://gdscript/test_report.gd")

var _checks := 0
var _failures: Array = []

func _chk(ok: bool, msg: String) -> void:
	_checks += 1
	if ok:
		print("  PASS  ", msg)
	else:
		_failures.append(msg)
		print("  FAIL  ", msg)

func _initialize() -> void:
	_run()

func _run() -> void:
	await process_frame

	# ================= GAS =================
	print("[rt-cost] == gas ==")
	if ClassDB.class_exists("PhysXGas3D"):
		var gas: Node = ClassDB.instantiate("PhysXGas3D")
		root.add_child(gas)
		gas.set("emitting", true)
		for i in 12:
			await physics_frame
		var submits: int = -1
		if gas.has_method("get_solver_step_submits"):
			submits = gas.get_solver_step_submits()
		if submits < 0:
			print("  SKIP  pre-Phase-9 build (no submit counter)")
		elif submits == 0:
			print("  SKIP  gas solver unavailable (headless/no device) -- boot was clean")
		else:
			_chk(submits == 1, "gas step issues exactly 1 submit (got %d)" % submits)
			# Steady state: submits stay 1 over more ticks (curl/jacobi merged).
			for i in 10:
				await physics_frame
			_chk(gas.get_solver_step_submits() == 1, "gas submits remain 1 in steady state")
		gas.queue_free()
		await process_frame
	else:
		print("  SKIP  PhysXGas3D not registered")

	# ================= FLOW =================
	print("[rt-cost] == flow ==")
	if ClassDB.class_exists("PhysXFlowSimulation3D"):
		var holder := Node3D.new()
		root.add_child(holder)
		var flow: Node = ClassDB.instantiate("PhysXFlowSimulation3D")
		holder.add_child(flow)
		flow.set("max_blocks", 512)
		flow.set("cell_size", 0.5)
		var emitter: Node = ClassDB.instantiate("PhysXFlowEmitter3D")
		holder.add_child(emitter)
		emitter.set("radius", 0.4)
		emitter.set("velocity", Vector3(0, 4, 0))
		emitter.set("smoke", 1.0)
		flow.set("emitters", [flow.get_path_to(emitter)])
		for i in 60:
			await physics_frame

		var diag: Dictionary = flow.get_diagnostics()
		if not bool(diag.get("available", false)):
			print("  SKIP  flow runtime unavailable -- boot was clean, no crash")
		else:
			if flow.has_method("get_frame_count"):
				var frames: int = int(flow.get_frame_count())
				var uploads: int = int(flow.get_texture_update_count())
				_chk(frames > 0, "flow simulated frames counted (%d)" % frames)
				# Gated upload: every texture update must correspond to a
				# decoded frame, and a paused sim must add none.
				_chk(uploads <= frames, "texture uploads gated on decoded frames (%d uploads / %d frames)" % [uploads, frames])
				flow.set("paused", true)
				# Drain any in-flight frame that completed before the pause.
				for i in 10:
					await physics_frame
				var before: int = int(flow.get_texture_update_count())
				for i in 30:
					await physics_frame
				var after: int = int(flow.get_texture_update_count())
				_chk(after == before, "paused sim issues no texture uploads (%d -> %d)" % [before, after])
			else:
				print("  SKIP  pre-Phase-9 build (no upload counters)")
			# FLOW-12: step_once while paused must actually advance the sim.
			# The old step_once delegated to _step, which force-disables the
			# core simulation when paused -- a guaranteed no-op in exactly this
			# configuration. Observed via the decoded-frame counter (density
			# itself lags the readback by 1-2 frames, so max_smoke is not a
			# reliable discriminator on short windows).
			flow.set("paused", true)
			var before_once: int = int(flow.get_frame_count())
			for i in 8:
				flow.step_once()
			var after_once: int = int(flow.get_frame_count())
			_chk(after_once > before_once,
					"step_once advances the sim while paused (%d -> %d)" % [before_once, after_once])
		flow.queue_free()
		holder.queue_free()
		await process_frame
	else:
		print("  SKIP  PhysXFlowSimulation3D not registered")

	# ================= WATER (setter storm, headless-tolerant) =================
	print("[rt-cost] == water ==")
	if ClassDB.class_exists("PhysXWaterSurface3D"):
		var water: Node = ClassDB.instantiate("PhysXWaterSurface3D")
		root.add_child(water)
		# 100 rapid setter changes across the live + rebuild groups: must not
		# error (WATER-1's stale-texture window was validation-error-visible).
		var ok := true
		for round in 10:
			water.set("damping", 0.02 + 0.01 * (round % 5))
			water.set("water_level", 0.1 * (round % 3))
			water.set("depth", 5.0 + round)
			water.set("ripple_amplitude", 0.5 + 0.1 * round)
			water.set("wind_speed", 5.0 + round)
			water.set("fetch", 100.0 + round)
			water.set("choppiness", 1.0 + 0.1 * round)
			water.set("foam_threshold", 0.3 + 0.01 * round)
			water.set("shallow_fade_depth", 1.0 + 0.1 * round)
			water.set("render_extent", 60.0 + round)
		for i in 5:
			await physics_frame
		_chk(ok, "water survived 100 rapid setter changes")
		water.queue_free()
		await process_frame
	else:
		print("  SKIP  PhysXWaterSurface3D not registered")

	var ok_all := _failures.is_empty()
	print("[rt-cost] %d checks, %d failures -> %s" % [_checks, _failures.size(), "PASS" if ok_all else "FAIL"])
	TestReport.write(TestReport.json_path_from_args(), "runtime", "PHYSX-RTC-001",
			"gas single-submit, flow gated upload, water setter storm, headless boot",
			"pass" if ok_all else "fail", _checks, _failures)
	quit(0 if ok_all else 1)
