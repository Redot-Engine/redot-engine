// Access to the module-specific articulation API (PhysXServer3D inner
// singleton) — same pattern as VehicleApi: prefer the generated
// Godot.PhysXServer3D C# bindings; fall back to the GDScript bridge
// (physx_articulation_bridge.gd) when the C# glue has not been regenerated
// yet for the newly bound methods.
//
// IMPORTANT: articulation calls must target the INNER server, never
// PhysicsServer3D.GetSingleton() (that is the WrapMT wrapper without the
// module API).

using System;
using System.Reflection;

namespace PhysxTestProject.Tests;

public static class ArticulationApi
{
	static object _inner;
	static Type _innerType;
	static Godot.GodotObject _bridge;
	static bool _resolved;

	static void Resolve()
	{
		if (_resolved) return;
		_resolved = true;
		foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
		{
			var t = asm.GetType("Godot.PhysXServer3D");
			if (t != null) { _innerType = t; break; }
		}
		if (_innerType != null)
		{
			var getSingleton = _innerType.GetMethod("GetSingleton", BindingFlags.Public | BindingFlags.Static);
			_inner = getSingleton?.Invoke(null, null);
		}
		// The bridge is created lazily per call: the singleton usually exists
		// (the type was bound before), but newly added methods may be missing
		// from the stale generated C# glue.
	}

	/// <summary>Invokes an inner-server articulation method; returns a Variant.
	/// Falls back to the GDScript bridge when the strongly-typed path is
	/// unavailable (stale C# bindings).</summary>
	public static Variant Call(string method, params Variant[] args)
	{
		Resolve();
		if (_inner != null)
		{
			var mi = _innerType.GetMethod(ToPascal(method), BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static);
			if (mi != null)
			{
				var parameters = mi.GetParameters();
				object[] cast = new object[parameters.Length];
				for (int i = 0; i < parameters.Length && i < args.Length; i++) cast[i] = ConvertArg(args[i], parameters[i].ParameterType);
				var result = mi.Invoke(_inner, cast);
				return FromObject(result);
			}
		}
		var bridge = EnsureBridge();
		if (bridge != null) return bridge.Call(method, args);
		throw new InvalidOperationException("No route to PhysXServer3D articulation API (no C# bindings and no GDScript bridge)");
	}

	static Godot.GodotObject EnsureBridge()
	{
		if (_bridge != null) return _bridge;
		var script = GD.Load<Godot.GDScript>("res://gdscript/physx_articulation_bridge.gd");
		if (script != null) _bridge = (Godot.GodotObject)script.New();
		return _bridge;
	}

	static string ToPascal(string snake)
	{
		if (string.IsNullOrEmpty(snake)) return snake;
		var parts = snake.Split('_');
		for (int i = 0; i < parts.Length; i++)
			if (parts[i].Length > 0) parts[i] = char.ToUpperInvariant(parts[i][0]) + parts[i].Substring(1);
		return string.Concat(parts);
	}

	static object ConvertArg(Variant v, Type target)
	{
		if (target == typeof(Rid)) return v.AsRid();
		if (target == typeof(int)) return v.AsInt32();
		if (target == typeof(uint)) return (uint)v.AsInt64();
		if (target == typeof(float)) return v.AsSingle();
		if (target == typeof(bool)) return v.AsBool();
		if (target == typeof(Godot.Collections.Dictionary)) return v.AsGodotDictionary();
		if (target == typeof(Transform3D)) return v.AsTransform3D();
		if (target == typeof(Vector3)) return v.AsVector3();
		if (target == typeof(Variant)) return v;
		return v.AsGodotObject() ?? (object)v;
	}

	static Variant FromObject(object o) => o switch
	{
		null => default,
		Variant v => v,
		Rid r => Variant.From(r),
		int i => Variant.From(i),
		uint u => Variant.From(u),
		float f => Variant.From(f),
		bool b => Variant.From(b),
		Godot.Collections.Dictionary d => Variant.From(d),
		Transform3D t => Variant.From(t),
		_ => Variant.From(o.ToString()),
	};

	// ------------------------------------------------------------------ typed helpers
	public static Rid Create() => Call("articulation_create").AsRid();
	public static void SetSpace(Rid art, Rid space) => Call("articulation_set_space", art, space);
	public static int AddLink(Rid art, int parentIndex, Transform3D parentFrame, Transform3D childFrame,
		int jointType, float density, Vector3 boxHalfExtents)
		=> Call("articulation_add_link", art, parentIndex, parentFrame, childFrame, jointType, density, boxHalfExtents).AsInt32();
	public static void SetDrive(Rid art, int linkIndex, int axis, float stiffness, float damping,
		float driveTarget, float driveVelocity, int driveType)
		=> Call("articulation_set_drive", art, linkIndex, axis, stiffness, damping, driveTarget, driveVelocity, driveType);
	public static void SetLimit(Rid art, int linkIndex, int axis, float low, float high)
		=> Call("articulation_set_limit", art, linkIndex, axis, low, high);
	public static void SetFixBase(Rid art, bool fix) => Call("articulation_set_fix_base", art, fix);
	public static void Wake(Rid art) => Call("articulation_wake", art);
	public static void Sleep(Rid art) => Call("articulation_sleep", art);
	public static int GetLinkCount(Rid art) => Call("articulation_get_link_count", art).AsInt32();
	public static Transform3D GetLinkTransform(Rid art, int linkIndex) => Call("articulation_get_link_transform", art, linkIndex).AsTransform3D();
	public static bool IsSleeping(Rid art) => Call("articulation_is_sleeping", art).AsBool();
	public static void SetLinkShape(Rid art, int linkIndex, Rid shape, Transform3D transform)
		=> Call("articulation_set_link_shape", art, linkIndex, shape, transform);
	public static void SetLinkCollisionLayer(Rid art, int linkIndex, uint layer)
		=> Call("articulation_set_link_collision_layer", art, linkIndex, layer);
	public static void SetLinkCollisionMask(Rid art, int linkIndex, uint mask)
		=> Call("articulation_set_link_collision_mask", art, linkIndex, mask);
	public static uint GetLinkCollisionLayer(Rid art, int linkIndex) => Call("articulation_get_link_collision_layer", art, linkIndex).AsUInt32();
	public static uint GetLinkCollisionMask(Rid art, int linkIndex) => Call("articulation_get_link_collision_mask", art, linkIndex).AsUInt32();
	public static Godot.Collections.Dictionary GetLinkVelocity(Rid art, int linkIndex)
		=> Call("articulation_get_link_velocity", art, linkIndex).AsGodotDictionary();
}
