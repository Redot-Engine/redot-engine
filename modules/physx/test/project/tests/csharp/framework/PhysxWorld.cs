// Per-test physics world harness.
//
// Each test constructs a PhysxWorld, which creates a fresh, active space (full
// test isolation) and tracks every RID it created. Dispose() frees everything
// in dependency-safe order (joints/vehicles/areas -> bodies -> shapes ->
// space), so a test that aborts mid-way cannot leak resources into the next
// test. All common geometry builders and state accessors live here so the
// test files stay about *behavior*, not plumbing.

using System;
using System.Collections.Generic;

namespace PhysxTestProject.Tests;

public sealed class PhysxWorld : IDisposable
{
	public readonly Rid Space;
	public const float G = 9.81f; // project gravity (project.godot)
	public static float Dt => 1f / Engine.GetPhysicsTicksPerSecond();

	readonly List<Rid> _joints = new();
	readonly List<Rid> _areas = new();
	readonly List<Rid> _bodies = new();
	readonly List<(Rid owner, Rid shape)> _externalShapeUses = new();
	readonly List<Rid> _shapes = new();
	readonly List<Rid> _vehicles = new();
	readonly List<Rid> _extraSpaces = new();
	public readonly List<Rid> TrackedSpaces = new();
	/// <summary>RID of the floor body when the world was created with one.</summary>
	public Rid FloorRid { get; private set; }
	bool _disposed;

	public PhysxWorld(bool withFloor = true, float floorTop = 0f)
	{
		Space = TrackSpace(CreateSpace());
		if (withFloor) FloorRid = AddFloor(floorTop);
	}

	// ------------------------------------------------------------------ spaces
	public static Rid CreateSpace()
	{
		var s = PhysicsServer3D.SpaceCreate();
		PhysicsServer3D.SpaceSetActive(s, true);
		return s;
	}

	public Rid TrackSpace(Rid s) { TrackedSpaces.Add(s); return s; }

	/// <summary>Large static floor box (half-height 1) whose top surface sits at p_top.</summary>
	public Rid AddFloor(float top = 0f, float size = 400f)
	{
		return MakeStatic(Box(size * 0.5f, 1f, size * 0.5f), new Vector3(0, top - 1f, 0));
	}

	public PhysicsDirectSpaceState3D Dss() => PhysicsServer3D.SpaceGetDirectState(Space);

	// ------------------------------------------------------------------ shapes
	/// <summary>
	/// Box with half-extents (ex, ey, ez). SHAPE_BOX server data is
	/// half-extents: core BoxShape3D passes size / 2, and godot_physics /
	/// Jolt read the data as half-extents too.
	/// </summary>
	public Rid Box(float ex, float ey, float ez)
	{
		var s = PhysicsServer3D.BoxShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Vector3(ex, ey, ez));
		return TrackShape(s);
	}
	public Rid Box(float e) => Box(e, e, e);

	public Rid Sphere(float r)
	{
		var s = PhysicsServer3D.SphereShapeCreate();
		PhysicsServer3D.ShapeSetData(s, r);
		return TrackShape(s);
	}

	public Rid Capsule(float radius, float height)
	{
		var s = PhysicsServer3D.CapsuleShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["radius"] = radius, ["height"] = height });
		return TrackShape(s);
	}

	public Rid Cylinder(float radius, float height)
	{
		var s = PhysicsServer3D.CylinderShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["radius"] = radius, ["height"] = height });
		return TrackShape(s);
	}

	/// <summary>Cone via the module's custom shape factory ({"type":"cone",...}).</summary>
	public Rid Cone(float radius, float height)
	{
		var s = PhysicsServer3D.CustomShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["type"] = "cone", ["radius"] = radius, ["height"] = height });
		return TrackShape(s);
	}

	public Rid Convex(params Vector3[] points)
	{
		var s = PhysicsServer3D.ConvexPolygonShapeCreate();
		PhysicsServer3D.ShapeSetData(s, points);
		return TrackShape(s);
	}

	/// <summary>Unit-ish tetrahedron hull centered near origin.</summary>
	public Rid Tetra(float scale = 1f) => Convex(
		new Vector3(-0.5f, -0.35f, -0.4f) * scale, new Vector3(0.6f, -0.3f, -0.2f) * scale,
		new Vector3(-0.1f, -0.25f, 0.6f) * scale, new Vector3(0.0f, 0.6f, 0.0f) * scale);

	public Rid Concave(params Vector3[] tris)
	{
		var s = PhysicsServer3D.ConcavePolygonShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["faces"] = tris, ["backface_collision"] = false });
		return TrackShape(s);
	}

	public Rid Heightmap(int width, int depth, float[] heights, float cellSize = 1f)
	{
		var s = PhysicsServer3D.HeightmapShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary
		{
			["width"] = width,
			["depth"] = depth,
			["cell_size"] = cellSize,
			["heights"] = heights,
		});
		return TrackShape(s);
	}

	public Rid FlatHeightmap(int n, float height = 0f)
	{
		var hs = new float[n * n];
		for (int i = 0; i < hs.Length; i++) hs[i] = height;
		return Heightmap(n, n, hs);
	}

	public Rid SeparationRay(float length, bool slideOnSlope = true)
	{
		var s = PhysicsServer3D.SeparationRayShapeCreate();
		PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["length"] = length, ["slide_on_slope"] = slideOnSlope });
		return TrackShape(s);
	}

	public Rid WorldBoundary(Plane? plane = null)
	{
		var s = PhysicsServer3D.WorldBoundaryShapeCreate();
		PhysicsServer3D.ShapeSetData(s, plane ?? new Plane(Vector3.Up, 0f));
		return TrackShape(s);
	}

	Rid TrackShape(Rid s) { _shapes.Add(s); return s; }

	/// <summary>Registers a shape created outside this world so cleanup still frees it.</summary>
	public Rid AdoptShape(Rid s) { _shapes.Add(s); return s; }

	// ------------------------------------------------------------------ bodies
	public Rid MakeBody(Rid shape, Vector3 pos, float mass = 1f, uint layer = 1, uint mask = 0xFFFFFFFF,
						PhysicsServer3D.BodyMode mode = PhysicsServer3D.BodyMode.Rigid, Transform3D? shapeXf = null)
	{
		var b = PhysicsServer3D.BodyCreate();
		_bodies.Add(b);
		PhysicsServer3D.BodySetSpace(b, Space);
		// Mode must be set BEFORE attaching shapes: the module forwards shape
		// data straight to PhysX, and triangle-mesh/heightfield/plane geometry
		// attached while the body is still default-rigid puts PhysX into an
		// error state (see REG-0014). Static-then-attach is the canonical order.
		PhysicsServer3D.BodySetMode(b, mode);
		if (shape.IsValid)
		{
			PhysicsServer3D.BodyAddShape(b, shape, shapeXf ?? Transform3D.Identity);
			if (!_shapes.Contains(shape)) _externalShapeUses.Add((b, shape));
		}
		PhysicsServer3D.BodySetCollisionLayer(b, layer);
		PhysicsServer3D.BodySetCollisionMask(b, mask);
		PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Mass, mass);
		PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, new Transform3D(Basis.Identity, pos));
		return b;
	}

	public Rid MakeStatic(Rid shape, Vector3 pos, uint layer = 1, uint mask = 0xFFFFFFFF, Transform3D? shapeXf = null)
		=> MakeBody(shape, pos, 0f, layer, mask, PhysicsServer3D.BodyMode.Static, shapeXf);

	public Rid MakeKinematic(Rid shape, Vector3 pos, uint layer = 1, uint mask = 0xFFFFFFFF)
		=> MakeBody(shape, pos, 1f, layer, mask, PhysicsServer3D.BodyMode.Kinematic);

	public Rid TrackBody(Rid b) { _bodies.Add(b); return b; }

	// ------------------------------------------------------------------ areas
	public Rid MakeArea(Rid shape, Vector3 pos, uint layer = 1, uint mask = 0xFFFFFFFF)
	{
		var a = PhysicsServer3D.AreaCreate();
		_areas.Add(a);
		PhysicsServer3D.AreaSetSpace(a, Space);
		PhysicsServer3D.AreaAddShape(a, shape, new Transform3D(Basis.Identity, pos));
		PhysicsServer3D.AreaSetCollisionLayer(a, layer);
		PhysicsServer3D.AreaSetCollisionMask(a, mask);
		return a;
	}

	public Rid TrackArea(Rid a) { _areas.Add(a); return a; }

	// ------------------------------------------------------------------ joints
	public Rid TrackJoint(Rid j) { _joints.Add(j); return j; }

	// ------------------------------------------------------------------ vehicles
	public Rid TrackVehicle(Rid v) { _vehicles.Add(v); return v; }

	/// <summary>
	/// Removes a tracked RID from cleanup tracking. For tests that free a
	/// tracked resource mid-test (stale-RID robustness, op storms): without
	/// this, <see cref="FreeAll"/> frees it a second time and the server
	/// prints an "attempted to free unknown RID" error per dead RID, drowning
	/// real diagnostics. A no-op for RIDs that were never tracked.
	/// </summary>
	public void Forget(Rid r)
	{
		_joints.Remove(r);
		_areas.Remove(r);
		_bodies.Remove(r);
		_shapes.Remove(r);
		_vehicles.Remove(r);
		TrackedSpaces.Remove(r);
	}

	/// <summary>Frees a tracked RID and forgets it — the one correct way for
	/// a test to free a tracked resource mid-test.</summary>
	public void Free(Rid r)
	{
		Forget(r);
		PhysicsServer3D.FreeRid(r);
	}

	// ------------------------------------------------------------------ state
	public Transform3D Pos(Rid b) => PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.Transform).AsTransform3D();
	public Vector3 Vel(Rid b) => PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.LinearVelocity).AsVector3();
	public Vector3 AngVel(Rid b) => PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.AngularVelocity).AsVector3();
	public bool Sleeping(Rid b) => PhysicsServer3D.BodyGetState(b, PhysicsServer3D.BodyState.Sleeping).AsBool();
	public Vector3 GravityOf(Rid b) => PhysicsServer3D.BodyGetDirectState(b)?.TotalGravity ?? Vector3.Zero;

	public PhysicsDirectBodyState3D Direct(Rid b) => PhysicsServer3D.BodyGetDirectState(b);

	public void SetVel(Rid b, Vector3 v) => PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.LinearVelocity, v);
	public void SetAngVel(Rid b, Vector3 v) => PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.AngularVelocity, v);
	public void Teleport(Rid b, Vector3 pos, Basis? basis = null)
		=> PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform, new Transform3D(basis ?? Basis.Identity, pos));

	/// <summary>True while all of the body's state stays finite (NaN/INF watchdog).</summary>
	public static bool Finite(Transform3D t) =>
		float.IsFinite(t.Origin.X) && float.IsFinite(t.Origin.Y) && float.IsFinite(t.Origin.Z) && Finite(t.Basis);
	public static bool Finite(Vector3 v) => float.IsFinite(v.X) && float.IsFinite(v.Y) && float.IsFinite(v.Z);
	static bool Finite(Basis b)
	{
		for (int i = 0; i < 3; i++) if (!Finite(b[i])) return false;
		return true;
	}

	// ------------------------------------------------------------------ queries
	public Godot.Collections.Dictionary Ray(Vector3 from, Vector3 to, uint mask = 0xFFFFFFFF,
		Godot.Collections.Array<Rid> exclude = null, bool areas = false, bool bodies = true,
		bool fromInside = false, bool backFaces = false)
	{
		var p = PhysicsRayQueryParameters3D.Create(from, to, mask, exclude ?? new Godot.Collections.Array<Rid>());
		p.CollideWithAreas = areas;
		p.CollideWithBodies = bodies;
		p.HitFromInside = fromInside;
		p.HitBackFaces = backFaces;
		return Dss().IntersectRay(p);
	}

	public Godot.Collections.Array<Godot.Collections.Dictionary> Point(Vector3 at, uint mask = 0xFFFFFFFF, int max = 32)
	{
		var p = new PhysicsPointQueryParameters3D
		{
			Position = at,
			CollisionMask = mask,
			CollideWithBodies = true,
			CollideWithAreas = false,
		};
		return Dss().IntersectPoint(p, max);
	}

	public Godot.Collections.Array<Godot.Collections.Dictionary> Overlap(Rid shape, Transform3D xf, uint mask = 0xFFFFFFFF, int max = 32)
	{
		var p = new PhysicsShapeQueryParameters3D { ShapeRid = shape, Transform = xf, CollisionMask = mask, CollideWithBodies = true };
		return Dss().IntersectShape(p, max);
	}

	public Godot.Collections.Dictionary RestInfo(Rid shape, Transform3D xf, uint mask = 0xFFFFFFFF)
	{
		var p = new PhysicsShapeQueryParameters3D { ShapeRid = shape, Transform = xf, CollisionMask = mask, CollideWithBodies = true };
		return Dss().GetRestInfo(p);
	}

	public Vector2 CastMotion(Rid shape, Transform3D xf, Vector3 motion, uint mask = 0xFFFFFFFF)
	{
		var p = new PhysicsShapeQueryParameters3D
		{
			ShapeRid = shape,
			Transform = xf,
			Motion = motion,
			CollisionMask = mask,
			CollideWithBodies = true,
		};
		float[] r = Dss().CastMotion(p);
		return (r != null && r.Length >= 2) ? new Vector2(r[0], r[1]) : Vector2.One;
	}

	public (bool hit, PhysicsTestMotionResult3D res) TestMotion(Rid body, Transform3D from, Vector3 motion, bool recoveryAsCollision = false)
	{
		var p = new PhysicsTestMotionParameters3D { From = from, Motion = motion, RecoveryAsCollision = recoveryAsCollision };
		var r = new PhysicsTestMotionResult3D();
		bool hit = PhysicsServer3D.BodyTestMotion(body, p, r);
		return (hit, r);
	}

	// ------------------------------------------------------------------ cleanup
	public void Dispose()
	{
		if (_disposed) return;
		_disposed = true;
		FreeAll();
	}

	void FreeAll()
	{
		void TryFree(Rid r) { if (r.IsValid) PhysicsServer3D.FreeRid(r); }
		foreach (var v in _vehicles) TryFree(v);
		foreach (var j in _joints) TryFree(j);
		foreach (var a in _areas) TryFree(a);
		foreach (var b in _bodies) TryFree(b);
		foreach (var s in _shapes) TryFree(s);
		foreach (var s in TrackedSpaces) TryFree(s);
		_vehicles.Clear(); _joints.Clear(); _areas.Clear(); _bodies.Clear(); _shapes.Clear(); TrackedSpaces.Clear();
	}
}
