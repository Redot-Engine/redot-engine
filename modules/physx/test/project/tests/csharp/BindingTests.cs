// Bindings: the module's scene-node classes must be present in ClassDB and
// usable from C#. Compilation of this file is itself part of the test — the
// strongly-typed `new Godot.PhysX...()` calls below only compile when the
// source-generated C# API was regenerated after the classes were registered.

using System.Collections;

namespace PhysxTestProject.Tests;

internal static class BindingTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-BIND-001", "PhysXChunkEmitter3D bound in ClassDB + instantiable from C#", ChunkEmitterBound);
		s.Add("PHYSX-BIND-002", "PhysXCloth3D bound in ClassDB + instantiable from C#", ClothBound);
		s.Add("PHYSX-BIND-003", "PhysXParticleFluid3D bound in ClassDB + instantiable from C#", FluidBound);
		s.Add("PHYSX-BIND-004", "PhysXServer3D singleton reachable from C#", ServerSingletonReachable);
		s.Add("PHYSX-FLUID-SS-001", "FLUID-SS-001: Screen Space render mode hidden from the editor, round-trips, names the substitution (F-38 — guard)", FluidScreenSpaceModeHidden);
	}

	static IEnumerator ChunkEmitterBound()
	{
		Assert.Expect(Godot.ClassDB.ClassExists("PhysXChunkEmitter3D"), "ClassDB knows PhysXChunkEmitter3D");
		Assert.Expect(Godot.ClassDB.CanInstantiate("PhysXChunkEmitter3D"), "PhysXChunkEmitter3D is instantiable");
		var emitter = new Godot.PhysXChunkEmitter3D();
		Assert.Expect(emitter != null, "new PhysXChunkEmitter3D() returns an instance");
		Assert.Expect(emitter.ChunkCount == 14, $"chunk_count default is 14 (got {emitter.ChunkCount})");
		Assert.Expect(emitter.GetActiveChunkCount() == 0, "no chunks are active before a burst");
		emitter.Free();
		yield return Wait.Frame();
	}

	static IEnumerator ClothBound()
	{
		Assert.Expect(Godot.ClassDB.ClassExists("PhysXCloth3D"), "ClassDB knows PhysXCloth3D");
		Assert.Expect(Godot.ClassDB.CanInstantiate("PhysXCloth3D"), "PhysXCloth3D is instantiable");
		var cloth = new Godot.PhysXCloth3D();
		Assert.Expect(cloth != null, "new PhysXCloth3D() returns an instance");
		// Outside the tree the cloth is not built and has not taken the GPU path.
		Assert.Expect(cloth.GetVertexCount() == 0, "unbuilt cloth reports 0 vertices");
		Assert.Expect(!cloth.IsGpuAccelerated(), "unbuilt cloth is not GPU-accelerated");
		Assert.Expect(cloth.SimulationMode == Godot.PhysXCloth3D.SimulationModeEnum.Auto,
				"simulation_mode defaults to Auto");
		cloth.Free();
		yield return Wait.Frame();
	}

	static IEnumerator FluidBound()
	{
		Assert.Expect(Godot.ClassDB.ClassExists("PhysXParticleFluid3D"), "ClassDB knows PhysXParticleFluid3D");
		Assert.Expect(Godot.ClassDB.CanInstantiate("PhysXParticleFluid3D"), "PhysXParticleFluid3D is instantiable");
		var fluid = new Godot.PhysXParticleFluid3D();
		Assert.Expect(fluid != null, "new PhysXParticleFluid3D() returns an instance");
		Assert.Expect(fluid.ParticleCount == 4096, $"particle_count default is 4096 (got {fluid.ParticleCount})");
		Assert.Expect(fluid.GetLiveParticleCount() == 0, "no live particles before spawn");
		fluid.Free();
		yield return Wait.Frame();
	}

	static IEnumerator FluidScreenSpaceModeHidden()
	{
		// F-38: the screen-space fluid renderer was deliberately not ported
		// (parked on Redot Compositor maturity), but the enum value used to be
		// selectable and silently rendered particles. The editor hint now
		// hides the option; authored Screen Space scenes still round-trip (the
		// value stays loadable), and the node names the substitution in its
		// configuration warnings instead of staying silent.
		var fluid = new Godot.PhysXParticleFluid3D();
		Assert.Expect(fluid != null, "PhysXParticleFluid3D instantiable");

		// The editor hint must not offer Screen Space anymore.
		string hint = "";
		foreach (var prop in fluid.GetPropertyList())
		{
			var dict = prop;
			if (dict["name"].AsString() == "render_mode")
			{
				hint = dict["hint_string"].AsString();
			}
		}
		Assert.Expect(!hint.Contains("Screen Space"), $"the render_mode hint hides Screen Space (hint=\"{hint}\")");

		// Authored values still round-trip, and selecting the mode is not a
		// silent no-op: set_render_mode warns once at runtime (the
		// configuration warnings name the substitution in the editor).
		fluid.Set("render_mode", 2);
		Assert.Expect(fluid.Get("render_mode").AsInt32() == 2, "an authored Screen Space value still round-trips");

		fluid.Set("render_mode", 1);
		Assert.Expect(fluid.Get("render_mode").AsInt32() == 1, "Isosurface still selects");
		fluid.Free();
		yield return Wait.Frame();
	}

	static IEnumerator ServerSingletonReachable()
	{
		var server = Godot.PhysXServer3D.GetSingleton();
		Assert.Expect(server != null, "PhysXServer3D.GetSingleton() returns the live server");
		yield return Wait.Frame();
	}
}
