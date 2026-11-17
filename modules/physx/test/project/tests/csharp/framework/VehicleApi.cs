// Access to the module-specific vehicle2 API (PhysXServer3D inner singleton).
//
// The vehicle methods exist only on the inner PhysXServer3D class (they are not
// part of PhysicsServer3D). On a C#-enabled build of this engine the generated
// GodotSharp contains Godot.PhysXServer3D; we bind it via reflection so this
// test project also compiles against stock Godot.NET.Sdk assemblies. If the
// strongly-typed class is missing we fall back to a GDScript bridge, which
// still exercises the C# -> Variant marshaling -> PhysicsServer3D path.
//
// IMPORTANT: vehicle calls must target the INNER server, never
// PhysicsServer3D.GetSingleton() (that is the WrapMT wrapper without vehicle
// methods).

using System;
using System.Reflection;
using System.Linq;

namespace PhysxTestProject.Tests;

public static class VehicleApi
{
	static object _inner;
	static Type _innerType;
	static Godot.GodotObject _bridge;
	static bool _resolved;

	public static string AccessPath { get; private set; } = "unresolved";

	static void Resolve()
	{
		if (_resolved) return;
		_resolved = true;
		// Strongly-typed path: Godot.PhysXServer3D in the generated bindings
		// assembly. Redot builds produce "RedotSharp"; stock Godot "GodotSharp".
		// The type lives in the already-loaded binding assembly at runtime, so
		// probing the loaded assemblies avoids a hard name dependency.
		foreach (var asm in AppDomain.CurrentDomain.GetAssemblies())
		{
			var t = asm.GetType("Godot.PhysXServer3D");
			if (t != null) { _innerType = t; break; }
		}
		if (_innerType == null)
		{
			try
			{
				_innerType = Assembly.Load("RedotSharp")?.GetType("Godot.PhysXServer3D")
							?? Assembly.Load("GodotSharp")?.GetType("Godot.PhysXServer3D");
			}
			catch { /* fall through to bridge */ }
		}
		if (_innerType != null)
		{
			var getSingleton = _innerType.GetMethod("GetSingleton", BindingFlags.Public | BindingFlags.Static);
			_inner = getSingleton?.Invoke(null, null);
			if (_inner != null) { AccessPath = "csharp-strong-typed"; return; }
		}
		// Fallback: GDScript bridge (works whenever GDScript can see ClassDB classes).
		var script = GD.Load<Godot.GDScript>("res://gdscript/physx_vehicle_bridge.gd");
		if (script != null)
		{
			_bridge = (Godot.GodotObject)script.New();
			AccessPath = "gdscript-bridge";
		}
	}

	/// <summary>Invokes an inner-server vehicle method; returns a Variant.
	/// Falls back to the GDScript bridge when the generated C# bindings are
	/// stale (method not present on the inner-server type).</summary>
	public static Variant Call(string method, params Variant[] args)
	{
		Resolve();
		if (_inner != null)
		{
			var mi = _innerType.GetMethods(BindingFlags.Public | BindingFlags.Instance | BindingFlags.Static)
					.Where(m => m.Name == ToPascal(method))
					.OrderByDescending(m => m.GetParameters().Length) // span overloads lose to array overloads
					.FirstOrDefault();
			if (mi != null)
			{
				var parameters = mi.GetParameters();
				object[] cast = new object[parameters.Length];
				for (int i = 0; i < parameters.Length && i < args.Length; i++) cast[i] = ConvertArg(args[i], parameters[i].ParameterType);
				var result = mi.Invoke(_inner, cast);
				return FromObject(result);
			}
		}
		if (EnsureBridge() != null) return _bridge.Call(method, args);
		throw new InvalidOperationException("No route to PhysXServer3D vehicle API (no C# bindings and no GDScript bridge)");
	}

	static Godot.GodotObject EnsureBridge()
	{
		if (_bridge != null) return _bridge;
		var script = GD.Load<Godot.GDScript>("res://gdscript/physx_vehicle_bridge.gd");
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
		if (target == typeof(uint)) return v.AsUInt32();
		if (target == typeof(long)) return v.AsInt64();
		if (target == typeof(float)) return v.AsSingle();
		if (target == typeof(bool)) return v.AsBool();
		if (target == typeof(Godot.Collections.Dictionary)) return v.AsGodotDictionary();
		if (target == typeof(Transform3D)) return v.AsTransform3D();
		if (target == typeof(Vector3[])) return v.AsVector3Array();
		if (target == typeof(int[])) return v.AsInt32Array();
		if (target == typeof(Variant)) return v;
		return v.AsGodotObject() ?? (object)v;
	}

	static Variant FromObject(object o) => o switch
	{
		null => default,
		Variant v => v,
		Rid r => Variant.From(r),
		int i => Variant.From(i),
		float f => Variant.From(f),
		bool b => Variant.From(b),
		Godot.Collections.Dictionary d => Variant.From(d),
		Godot.Collections.Array a => Variant.From(a),
		Transform3D t => Variant.From(t),
		float[] arr => Variant.From(arr),
		Vector3[] arr => Variant.From(arr),
		int[] arr => Variant.From(arr),
		_ => Variant.From(o.ToString()),
	};

	// ------------------------------------------------------------------ typed helpers
	public static Rid CreateVehicle(int archetype) => Call("vehicle_create", archetype).AsRid();
	public static void SetChassisBody(Rid vehicle, Rid body) => Call("vehicle_set_chassis_body", vehicle, body);
	public static void SetVehicleSpace(Rid vehicle, Rid space) => Call("vehicle_set_space", vehicle, space);
	public static int GetWheelCount(Rid vehicle) => Call("vehicle_get_wheel_count", vehicle).AsInt32();
	public static void SetWheelCount(Rid vehicle, int count) => Call("vehicle_set_wheel_count", vehicle, count);
	public static int AddWheel(Rid vehicle) => Call("vehicle_add_wheel", vehicle).AsInt32();
	public static void SetWheelParams(Rid vehicle, int idx, Godot.Collections.Dictionary p) => Call("vehicle_set_wheel_params", vehicle, idx, p);
	public static void SetWheelDriveTorque(Rid vehicle, int idx, float torque) => Call("vehicle_set_wheel_drive_torque", vehicle, idx, torque);
	public static void SetControlInputs(Rid v, float throttle, float brake, float steer, float handbrake)
		=> Call("vehicle_set_control_inputs", v, throttle, brake, steer, handbrake);
	public static void SetGearCommand(Rid v, int gear) => Call("vehicle_set_gear_command", v, gear);
	public static void SetResponseParams(Rid v, Godot.Collections.Dictionary p) => Call("vehicle_set_response_params", v, p);
	public static Godot.Collections.Array GetWheelStates(Rid v) => Call("vehicle_get_wheel_states", v).AsGodotArray();
	public static Godot.Collections.Dictionary GetEngineState(Rid v) => Call("vehicle_get_engine_state", v).AsGodotDictionary();
	public static void SetAckermannParams(Rid v, Godot.Collections.Dictionary p) => Call("vehicle_set_ackermann_params", v, p);
	public static float[] GetWheelSteerAngles(Rid v) => Call("vehicle_get_wheel_steer_angles", v).AsFloat32Array();
	public static void SetBalanceParams(Rid v, Godot.Collections.Dictionary p) => Call("vehicle_set_balance_params", v, p);
	public static Godot.Collections.Dictionary GetBalanceState(Rid v) => Call("vehicle_get_balance_state", v).AsGodotDictionary();
	public static void SetAntiRollParams(Rid v, Godot.Collections.Dictionary p) => Call("vehicle_set_anti_roll_params", v, p);
	public static Godot.Collections.Dictionary GetAntiRollParams(Rid v) => Call("vehicle_get_anti_roll_params", v).AsGodotDictionary();
	public static Godot.Collections.Dictionary GetResponseParams(Rid v) => Call("vehicle_get_response_params", v).AsGodotDictionary();
}
