// Queries: ray / point / shape / rest-info / cast-motion coverage with all
// returned fields verified, filter flags, degenerate origins, and rotated
// geometry. Documented module quirks (hit_from_inside synthetic hit with zero
// normal) are encoded as such; cast_motion follows godot_physics per-object
// overlap disregard (godot_space_3d _cast_motion).

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class QueryTests
{
	public static void Register(SuiteBuilder s)
	{
		s.Add("PHYSX-QUERY-001", "ray hit returns all documented fields", RayFields);
		s.Add("PHYSX-QUERY-002", "ray miss returns empty dictionary", RayMiss);
		s.Add("PHYSX-QUERY-003", "nearest hit wins among stacked objects", RayNearest);
		s.Add("PHYSX-QUERY-004", "ray respects collision mask", RayMask);
		s.Add("PHYSX-QUERY-005", "ray respects exclude list", RayExclude);
		s.Add("PHYSX-QUERY-006", "ray hits bodies only / areas only toggles", RayBodiesAreas);
		s.Add("PHYSX-QUERY-007", "hit_from_inside=false drops inside-origin hits", RayInsideOff);
		s.Add("PHYSX-QUERY-008", "hit_from_inside=true synthesizes origin hit (zero normal)", RayInsideOn);
		s.Add("PHYSX-QUERY-009", "zero-length ray behaves sanely", RayZeroLength);
		s.Add("PHYSX-QUERY-010", "very long ray still accurate", RayVeryLong);
		s.Add("PHYSX-QUERY-011", "diagonal ray against box reports correct face", RayDiagonal);
		s.Add("PHYSX-QUERY-012", "ray against rotated geometry reports rotated normal", RayRotated);
		s.Add("PHYSX-QUERY-013", "ray from boundary surface is stable", RayBoundaryOrigin);
		s.Add("PHYSX-QUERY-014", "point query inside/outside objects", PointInOut);
		s.Add("PHYSX-QUERY-015", "point query returns multiple objects and honors max results", PointMultiple);
		s.Add("PHYSX-QUERY-016", "point query respects mask", PointMask);
		s.Add("PHYSX-QUERY-017", "shape query overlap / non-overlap / translated / rotated", ShapeOverlap);
		s.Add("PHYSX-QUERY-018", "shape query multiple results and max results", ShapeMultiple);
		s.Add("PHYSX-QUERY-019", "shape query respects mask and areas", ShapeMaskAreas);
		s.Add("PHYSX-QUERY-020", "rest_info reports deepest collider, normal, point", RestInfoFields);
		s.Add("PHYSX-QUERY-021", "rest_info empty when no overlap", RestInfoEmpty);
		s.Add("PHYSX-QUERY-022", "cast_motion blocked fraction is sane", CastMotionBlocked);
		s.Add("PHYSX-QUERY-023", "cast_motion free path returns (1,1)", CastMotionFree);
		s.Add("PHYSX-QUERY-024", "cast_motion with initial overlap returns unobstructed (documented)", CastMotionInitialOverlap);
		s.Add("PHYSX-QUERY-025", "queries against areas with collide_with_areas", AreaQueries);
		s.Add("PHYSX-QUERY-026", "query on second space does not leak first space", CrossSpaceLeak);
		s.Add("PHYSX-QUERY-027", "ray_pickable=false body is invisible to raycasts until re-enabled", RayPickableToggle);
		s.Add("PHYSX-QUERY-028", "ray_pickable across bodies and areas; enforcement is ray-only", RayPickableMixed);
		s.Add("PHYSX-QUERY-029", "cast_motion overlapped start hits the wall behind the overlap", CastMotionOverlapDisregard);
		s.Add("PHYSX-QUERY-030", "rest_info picks the deepest of several overlaps", RestInfoDeepest);
		s.Add("PHYSX-QUERY-031", "one-sided wall never reports its back face, even to hit_back_faces rays", RayBackfaceInterior);
		s.Add("PHYSX-QUERY-032", "query margin inflates primitive query shapes (godot_physics contract)", QueryMarginInflation);
	}

	// Phase 13 margin-contract item: godot_physics grows the query shape by
	// ShapeParameters.margin for intersect_shape/collide_shape/cast_motion/
	// rest_info. A sphere query 0.15 m from a wall must miss at margin 0 and
	// hit at margin 0.3 — and cast_motion's swept box must be stopped by a
	// wall it only marginally overlaps.
	static IEnumerator QueryMarginInflation()
	{
		using var w = new PhysxWorld(false);
		// Thin wall: +Z face at z = 0.1.
		w.MakeStatic(w.Box(2f, 2f, 0.1f), new Vector3(0, 0, 0));
		yield return Wait.Frames(2);

		// Sphere query whose surface is 0.15 away from the wall face.
		var sphere = w.Sphere(0.25f);
		var at = new Transform3D(Basis.Identity, new Vector3(0, 0, 0.5f));

		var q0 = new PhysicsShapeQueryParameters3D
		{
			ShapeRid = sphere,
			Transform = at,
			CollisionMask = 0xFFFFFFFF,
			CollideWithBodies = true,
			Margin = 0f,
		};
		Assert.Expect(w.Dss().IntersectShape(q0).Count == 0, "margin 0: no hit across a 0.15 gap");

		var q1 = new PhysicsShapeQueryParameters3D
		{
			ShapeRid = sphere,
			Transform = at,
			CollisionMask = 0xFFFFFFFF,
			CollideWithBodies = true,
			Margin = 0.3f,
		};
		var hits = w.Dss().IntersectShape(q1);
		Assert.Expect(hits.Count > 0, "margin 0.3: inflated sphere reaches across the 0.15 gap");

		// cast_motion with margin: the inflated sphere swept toward -Z starts
		// overlapping, so the query reports the blocked/initial-overlap state
		// (safe fraction 0), while margin 0 sweeps the full motion.
		// cast_motion with margin: sweep from farther out so neither start pose
		// overlaps the wall. The margin-inflated sphere must be stopped
		// strictly earlier than the uninflated one — margin makes the sweep
		// more conservative.
		var far = new Transform3D(Basis.Identity, new Vector3(0, 0, 0.9f));
		var q2 = new PhysicsShapeQueryParameters3D
		{
			ShapeRid = sphere,
			Transform = far,
			CollisionMask = 0xFFFFFFFF,
			CollideWithBodies = true,
			Margin = 0f,
			Motion = new Vector3(0, 0, -1f),
		};
		var f0 = w.Dss().CastMotion(q2);
		q2.Margin = 0.3f;
		var f1 = w.Dss().CastMotion(q2);
		Assert.Expect(f0 != null && f0.Length == 2 && f0[0] < 1f, "margin 0: sweep stops at the wall");
		Assert.Expect(f1 != null && f1.Length == 2 && f1[0] < f0[0],
			$"margin 0.3: inflated sweep stops earlier ({Fmt(f1[0])} < {Fmt(f0[0])})");
	}

	private static string Fmt(float v)
	{
		return v.ToString("0.000");
	}

	static IEnumerator RayFields()
	{
		using var w = new PhysxWorld();
		var hit = w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0));
		Assert.Require(hit.Count > 0, "ray hits floor");
		Assert.Expect(hit["rid"].AsRid() == w.FloorRid, "field rid");
		Assert.Expect(hit["position"].AsVector3().Y is > -0.05f and < 0.05f, $"field position on surface (y={hit["position"].AsVector3().Y:F3})");
		Assert.Expect(hit["normal"].AsVector3().Y > 0.9f, "field normal up");
		Assert.Expect(hit["collider_id"].AsInt64() == 0, "collider_id of raw server body is 0");
		Assert.Expect(hit["shape"].AsInt32() == 0, "field shape index");
		Assert.Expect(hit.ContainsKey("face_index"), "face_index field present");
		yield return Wait.Frame();
	}
	static IEnumerator RayMiss()
	{
		using var w = new PhysxWorld(false);
		var miss = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
		Assert.Expect(miss.Count == 0, "ray in empty space returns empty dict");
		yield return Wait.Frame();
	}
	static IEnumerator RayNearest()
	{
		using var w = new PhysxWorld(false);
		var low = w.MakeStatic(w.Box(4, 0.5f, 4), new Vector3(0, 0, 0));
		var high = w.MakeStatic(w.Box(4, 0.5f, 4), new Vector3(0, 3, 0));
		yield return Wait.Frames(3);
		var fromAbove = w.Ray(new Vector3(0, 8, 0), new Vector3(0, -8, 0));
		Assert.Expect(fromAbove["rid"].AsRid() == high, "nearest hit from above is the high box");
		var fromBelow = w.Ray(new Vector3(0, -8, 0), new Vector3(0, 8, 0));
		Assert.Expect(fromBelow.Count > 0 && fromBelow["rid"].AsRid() == low, "nearest hit from below is the low box");
	}
	static IEnumerator RayMask()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(0, 1, 0), layer: 1);
		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0), mask: 0b01).Count > 0, "matching mask hits");
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0), mask: 0b10).Count == 0, "non-matching mask misses");
	}
	static IEnumerator RayExclude()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(4, 0.5f, 4), new Vector3(0, 0, 0));
		var b = w.MakeStatic(w.Box(4, 0.5f, 4), new Vector3(0, 3, 0));
		yield return Wait.Frames(3);
		var excl = new Godot.Collections.Array<Rid> { b };
		var hit = w.Ray(new Vector3(0, 8, 0), new Vector3(0, -8, 0), exclude: excl);
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == a, "excluding the near object reveals the far one");
		var exclBoth = new Godot.Collections.Array<Rid> { a, b };
		Assert.Expect(w.Ray(new Vector3(0, 8, 0), new Vector3(0, -8, 0), exclude: exclBoth).Count == 0, "excluding both misses");
	}
	static IEnumerator RayBodiesAreas()
	{
		using var w = new PhysxWorld(false);
		var body = w.MakeStatic(w.Box(2, 0.5f, 2), new Vector3(-2, 1, 0));
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(2, 1, 0));
		yield return Wait.Frames(3);
		var onlyBodies = w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0));
		Assert.Expect(onlyBodies.Count > 0, "bodies on by default");
		var areaDefault = w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0));
		Assert.Expect(areaDefault.Count == 0, "areas off by default");
		var areaOn = w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0), areas: true);
		Assert.Expect(areaOn.Count > 0 && areaOn["rid"].AsRid() == area, "area hit with collide_with_areas");
		var bodiesOff = w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0), bodies: false, areas: true);
		Assert.Expect(bodiesOff.Count == 0, "bodies ignored with collide_with_bodies=false");
	}
	static IEnumerator RayInsideOff()
	{
		using var w = new PhysxWorld();
		var hit = w.Ray(new Vector3(2, -0.4f, 0), new Vector3(2, -8, 0)); // origin inside the floor
		Assert.Expect(hit.Count == 0, "inside-origin ray misses with hit_from_inside=false");
		yield return Wait.Frame();
	}
	static IEnumerator RayInsideOn()
	{
		using var w = new PhysxWorld();
		var origin = new Vector3(2, -0.4f, 0);
		var hit = w.Ray(origin, new Vector3(2, -8, 0), fromInside: true);
		Assert.Expect(hit.Count > 0, "inside-origin ray hits with hit_from_inside=true");
		if (hit.Count > 0)
		{
			Assert.ExpectVecNear(hit["position"].AsVector3(), origin, 1e-3f, "synthetic hit at ray origin");
			// Documented module behavior: synthetic inside hit has a zero normal.
			Assert.ExpectVecNear(hit["normal"].AsVector3(), Vector3.Zero, 1e-6f, "synthetic inside hit normal is zero (documented)");
		}
		yield return Wait.Frame();
	}
	static IEnumerator RayZeroLength()
	{
		using var w = new PhysxWorld();
		var onSurface = w.Ray(new Vector3(2, 0.0f, 0), new Vector3(2, 0.0f, 0));
		Assert.Expect(onSurface.Count == 0 || onSurface.ContainsKey("rid"), "zero-length ray returns sane result");
		var inAir = w.Ray(new Vector3(2, 5, 0), new Vector3(2, 5, 0));
		Assert.Expect(inAir.Count == 0, "zero-length ray in air misses");
		yield return Wait.Frame();
	}
	static IEnumerator RayVeryLong()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(1, 1, 1), new Vector3(0, 5000, 0));
		yield return Wait.Frames(3);
		var hit = w.Ray(new Vector3(0, 9000, 0), new Vector3(0, -9000, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == b, "long ray finds distant body");
		Assert.ExpectNear(hit["position"].AsVector3().Y, 5001f, 0.5f, "distant hit position accurate");
	}
	static IEnumerator RayDiagonal()
	{
		using var w = new PhysxWorld(false);
		w.MakeStatic(w.Box(1, 1, 1), new Vector3(0, 1, 0));
		yield return Wait.Frames(3);
		var hit = w.Ray(new Vector3(-6, 8, 0), new Vector3(6, -4, 0));
		Assert.Expect(hit.Count > 0, "diagonal ray hits box");
		if (hit.Count > 0)
		{
			var n = hit["normal"].AsVector3();
			Assert.Expect(n.Y > 0.9f, $"diagonal hit on top face (normal {n})");
		}
	}
	static IEnumerator RayRotated()
	{
		using var w = new PhysxWorld(false);
		var rot = Basis.FromEuler(new Vector3(0, Mathf.DegToRad(90), 0));
		w.MakeStatic(w.Box(1, 0.25f, 0.25f), new Vector3(0, 1, 0), shapeXf: new Transform3D(rot, Vector3.Zero));
		yield return Wait.Frames(3);
		var hit = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
		Assert.Expect(hit.Count > 0, "ray hits rotated box");
		if (hit.Count > 0)
			Assert.Expect(Mathf.Abs(hit["position"].AsVector3().Y - 1.25f) < 0.15f,
				$"hit on rotated top face (y={hit["position"].AsVector3().Y:F2}, expect ~1.25)");
	}
	static IEnumerator RayBoundaryOrigin()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(1, 0.5f, 1), new Vector3(0, 0.5f, 0));
		yield return Wait.Frames(3);
		var at = w.Ray(new Vector3(0, 1.0f, 0), new Vector3(0, -5, 0));
		Assert.Expect(at.Count == 0 || at["rid"].AsRid() == b, "boundary-origin ray is stable and only reports the surface body");
		var slightlyAbove = w.Ray(new Vector3(0, 1.001f, 0), new Vector3(0, -5, 0));
		Assert.Expect(slightlyAbove.Count > 0, "ray just above surface hits");
		yield return Wait.Frame();
	}
	static IEnumerator PointInOut()
	{
		using var w = new PhysxWorld();
		var inside = w.Point(new Vector3(2, -0.4f, 0));
		Assert.Expect(inside.Count >= 1 && inside[0]["rid"].AsRid() == w.FloorRid, "point inside floor found");
		var outside = w.Point(new Vector3(2, 40, 0));
		Assert.Expect(outside.Count == 0, "point in air finds nothing");
		yield return Wait.Frame();
	}
	static IEnumerator PointMultiple()
	{
		using var w = new PhysxWorld(false);
		for (int i = 0; i < 5; i++) w.MakeStatic(w.Box(1.5f, 1.5f, 1.5f), new Vector3(0, i, 0));
		yield return Wait.Frames(3);
		var all = w.Point(new Vector3(0, 2, 0), max: 32);
		Assert.Expect(all.Count >= 3, $"point overlapping several boxes returns several (got {all.Count})");
		var capped = w.Point(new Vector3(0, 2, 0), max: 2);
		Assert.Expect(capped.Count <= 2, $"max results respected (got {capped.Count})");
	}
	static IEnumerator PointMask()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(2, 2, 2), new Vector3(0, 1, 0), layer: 1);
		yield return Wait.Frames(3);
		Assert.Expect(w.Point(new Vector3(0, 1, 0), mask: 0b01).Count > 0, "point with matching mask");
		Assert.Expect(w.Point(new Vector3(0, 1, 0), mask: 0b10).Count == 0, "point with wrong mask");
	}
	static IEnumerator ShapeOverlap()
	{
		using var w = new PhysxWorld();
		var probe = w.Sphere(0.5f);
		var hit = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(2, 0.3f, 0)));
		Assert.Expect(hit.Count >= 1 && hit[0]["rid"].AsRid() == w.FloorRid, "sphere overlaps floor");
		var miss = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(2, 3, 0)));
		Assert.Expect(miss.Count == 0, "translated probe misses");
		var rotated = w.Overlap(w.Box(0.4f, 2f, 0.4f), new Transform3D(Basis.FromEuler(new Vector3(0, 0, Mathf.Pi / 4f)), new Vector3(2, 1.0f, 0)));
		Assert.Expect(rotated.Count > 0, "rotated tall box reaches floor when tipped");
		yield return Wait.Frame();
	}
	static IEnumerator ShapeMultiple()
	{
		using var w = new PhysxWorld(false);
		var a = w.MakeStatic(w.Box(1, 1, 1), new Vector3(-1.5f, 1, 0));
		var b = w.MakeStatic(w.Box(1, 1, 1), new Vector3(1.5f, 1, 0));
		var c = w.MakeStatic(w.Box(1, 1, 1), new Vector3(0, 1, 3));
		yield return Wait.Frames(3);
		var probe = w.Box(2.5f, 2.5f, 2.5f);
		var hits = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(0, 1, 0)), max: 32);
		Assert.Expect(hits.Count >= 2, $"wide probe finds multiple boxes (got {hits.Count})");
		var capped = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(0, 1, 0)), max: 1);
		Assert.Expect(capped.Count == 1, "max results = 1 respected");
	}
	static IEnumerator ShapeMaskAreas()
	{
		using var w = new PhysxWorld(false);
		var body = w.MakeStatic(w.Box(1, 1, 1), new Vector3(-2, 1, 0), layer: 1);
		var area = w.MakeArea(w.Box(1, 1, 1), new Vector3(2, 1, 0), layer: 1);
		yield return Wait.Frames(3);
		Assert.Expect(w.Overlap(w.Sphere(1), new Transform3D(Basis.Identity, new Vector3(-2, 1, 0)), mask: 0b01).Count > 0,
			"shape query with matching mask");
		Assert.Expect(w.Overlap(w.Sphere(1), new Transform3D(Basis.Identity, new Vector3(-2, 1, 0)), mask: 0b10).Count == 0,
			"shape query with wrong mask");
		// Shape queries default to bodies only; areas are not returned.
		Assert.Expect(w.Overlap(w.Sphere(1), new Transform3D(Basis.Identity, new Vector3(2, 1, 0))).Count == 0,
			"shape query ignores areas by default");
	}
	static IEnumerator RestInfoFields()
	{
		using var w = new PhysxWorld();
		var ri = w.RestInfo(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(2, 0.2f, 0)));
		Assert.Require(ri.Count > 0, "rest_info on overlapping sphere");
		Assert.Expect(ri["rid"].AsRid() == w.FloorRid, "rest_info rid");
		Assert.Expect(ri["normal"].AsVector3().Y < -0.7f || ri["normal"].AsVector3().Y > 0.7f, "rest_info normal vertical");
		Assert.Expect(ri["point"].AsVector3().Y < 0.1f, "rest_info point near floor surface");
		Assert.Expect(ri.ContainsKey("linear_velocity"), "rest_info has linear_velocity");
		yield return Wait.Frame();
	}
	static IEnumerator RestInfoEmpty()
	{
		using var w = new PhysxWorld();
		var ri = w.RestInfo(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(2, 5, 0)));
		Assert.Expect(ri.Count == 0, "rest_info empty without overlap");
		yield return Wait.Frame();
	}
	static IEnumerator CastMotionBlocked()
	{
		using var w = new PhysxWorld();
		var (motion, shape) = (new Vector3(0, -3, 0), w.Sphere(0.3f));
		var frac = w.CastMotion(shape, new Transform3D(Basis.Identity, new Vector3(2, 1.0f, 0)), motion);
		// Sphere center starts 1.0 above floor, radius 0.3 -> contact at 0.3 => fraction 0.7/3 ~ 0.233.
		Assert.Expect(frac.X > 0.05f && frac.X < 0.5f, $"blocked cast safe fraction sane (got {frac.X:F3})");
		Assert.Expect(frac.Y >= frac.X, "unsafe fraction >= safe fraction");
		yield return Wait.Frame();
	}
	static IEnumerator CastMotionFree()
	{
		using var w = new PhysxWorld(false);
		var frac = w.CastMotion(w.Sphere(0.3f), new Transform3D(Basis.Identity, new Vector3(0, 5, 0)), new Vector3(0, -1, 0));
		Assert.ExpectNear(frac.X, 1f, 1e-4f, "free cast safe = 1");
		Assert.ExpectNear(frac.Y, 1f, 1e-4f, "free cast unsafe = 1");
		yield return Wait.Frame();
	}
	static IEnumerator CastMotionInitialOverlap()
	{
		using var w = new PhysxWorld();
		// Probe already inside the floor, motion driving deeper: godot_physics
		// disregards overlapped objects entirely, so with no other object
		// ahead the motion reports unobstructed (safe = unsafe = 1).
		var frac = w.CastMotion(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(2, -0.2f, 0)), new Vector3(0, -1, 0));
		Assert.ExpectNear(frac.X, 1f, 1e-4f, "initially-overlapping cast disregards the object (safe=1)");
		Assert.ExpectNear(frac.Y, 1f, 1e-4f, "initially-overlapping cast disregards the object (unsafe=1)");
		yield return Wait.Frame();
	}

	// godot_physics contract (godot_space_3d _cast_motion): objects the query
	// shape already overlaps are disregarded per-object; the closest FORWARD
	// blocker behind them still bounds the motion.
	static IEnumerator CastMotionOverlapDisregard()
	{
		using var w = new PhysxWorld(false);
		// Blocker the probe overlaps: sphere r=0.5 at the origin, box spans 0.4..1.4.
		w.MakeStatic(w.Box(0.5f, 1f, 1f), new Vector3(0.9f, 0, 0));
		// Wall face at x = 5.
		w.MakeStatic(w.Box(0.5f, 2f, 2f), new Vector3(5.5f, 0, 0));
		var frac = w.CastMotion(w.Sphere(0.5f), new Transform3D(Basis.Identity, Vector3.Zero), new Vector3(10, 0, 0));
		// Wall contact at 5 - 0.5 = 4.5 of 10 -> fraction 0.45.
		Assert.ExpectNear(frac.X, 0.45f, 0.02f, $"forward blocker found past the overlap (safe={frac.X:F3})");
		Assert.ExpectNear(frac.Y, 0.45f, 0.02f, "unsafe fraction at the wall");
		yield return Wait.Frame();
	}

	// godot_physics keeps the DEEPEST contact as the rest result
	// (godot_space_3d _rest_cbk_result: is_best_result = len > best.len).
	static IEnumerator RestInfoDeepest()
	{
		using var w = new PhysxWorld(false);
		// Deep slab: top at y = -0.1 (sphere at y = 0.2, r = 0.5 -> 0.2 deep).
		var deep = w.MakeStatic(w.Box(5f, 1f, 5f), new Vector3(0, -1.1f, 0));
		// Shallow block: top at y = -0.2 (0.1 deep) under the same sphere.
		w.MakeStatic(w.Box(0.5f, 0.25f, 0.5f), new Vector3(0.3f, -0.45f, 0));
		var rest = w.RestInfo(w.Sphere(0.5f), new Transform3D(Basis.Identity, new Vector3(0, 0.2f, 0)));
		Assert.Require(rest.Count > 0, "rest_info finds an overlap");
		Assert.Expect(rest["rid"].AsRid() == deep, "deepest collider wins (slab over shallow block)");
		Assert.Expect(rest["normal"].AsVector3().Y > 0.9f, "slab normal up");
		yield return Wait.Frame();
	}

	// godot_physics contract: with hit_back_faces the ray reports interior
	// (back) faces at their real position with the raw face normal -- a ray
	// inside a room hits the far wall. Only distance <= 0 (origin inside the
	// shape) is "from inside". The wall here is cooked SINGLE-sided
	// (backface_collision=false): the ray approaches its back, so the query
	// flag alone decides whether the hit exists.
	static IEnumerator RayBackfaceInterior()
	{
		using var w = new PhysxWorld(false);
		var wall = PhysicsServer3D.ConcavePolygonShapeCreate();
		PhysicsServer3D.ShapeSetData(wall, new Godot.Collections.Dictionary
		{
			["faces"] = new Vector3[] {
				new(2, 0, -2), new(2, 0, 2), new(2, 4, -2),
				new(2, 4, -2), new(2, 0, 2), new(2, 4, 2),
			},
			["backface_collision"] = false,
		});
		w.AdoptShape(wall);
		w.MakeStatic(wall, Vector3.Zero);

		// One-sided means one-sided for rays too (matching the built-in and
		// Jolt): the wall's face front points away from the room, so a ray
		// from inside sees only its back — and a back face on a
		// backface_collision=false shape is never reported, not even with
		// hit_back_faces=true (the query flag opts INTO back faces; the SHAPE
		// still decides whether it has any).
		var hit = w.Ray(new Vector3(0, 1, 0), new Vector3(4, 1, 0), backFaces: true);
		Assert.Expect(hit.Count == 0, "backface-enabled ray does not hit a one-sided wall's back face");

		var miss = w.Ray(new Vector3(0, 1, 0), new Vector3(4, 1, 0));
		Assert.Expect(miss.Count == 0, "with hit_back_faces=false the backface hit is not reported");
		yield return Wait.Frame();
	}
	static IEnumerator AreaQueries()
	{
		using var w = new PhysxWorld(false);
		var area = w.MakeArea(w.Box(2, 2, 2), new Vector3(0, 1, 0));
		var p = new PhysicsPointQueryParameters3D { Position = new Vector3(0, 1, 0), CollisionMask = 0xFFFFFFFF, CollideWithAreas = true };
		var pts = w.Dss().IntersectPoint(p, 8);
		Assert.Expect(pts.Count >= 1 && pts[0]["rid"].AsRid() == area, "point query hits area with collide_with_areas");
		yield return Wait.Frame();
	}
	static IEnumerator CrossSpaceLeak()
	{
		using var a = new PhysxWorld();
		using var b = new PhysxWorld(false);
		var bodyInA = a.MakeStatic(a.Box(1, 1, 1), new Vector3(0, 1, 0));
		yield return Wait.Frames(3);
		Assert.Expect(a.Point(new Vector3(0, 1, 0)).Count > 0, "body found in its own space");
		var p = new PhysicsPointQueryParameters3D { Position = new Vector3(0, 1, 0), CollisionMask = 0xFFFFFFFF };
		Assert.Expect(b.Dss().IntersectPoint(p, 8).Count == 0, "body invisible from other space");
	}
	static IEnumerator RayPickableToggle()
	{
		using var w = new PhysxWorld(false);
		var b = w.MakeStatic(w.Box(1, 1, 1), new Vector3(0, 1, 0));
		PhysicsServer3D.BodySetRayPickable(b, false);
		yield return Wait.Frames(3);
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0,
			"ray misses body with ray_pickable=false (reference semantics)");
		// Pickability is a raycast-only flag: point queries are unaffected.
		Assert.Expect(w.Point(new Vector3(0, 1, 0)).Count > 0, "point query still finds non-pickable body");
		PhysicsServer3D.BodySetRayPickable(b, true);
		yield return Wait.Frames(3);
		var hit = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
		Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == b, "ray hits body again after re-enable");
	}
	static IEnumerator RayPickableMixed()
	{
		using var w = new PhysxWorld(false);
		var pickable = w.MakeStatic(w.Box(1, 1, 1), new Vector3(-3, 1, 0));
		var hidden = w.MakeStatic(w.Box(1, 1, 1), new Vector3(3, 1, 0));
		PhysicsServer3D.BodySetRayPickable(hidden, false);
		var area = w.MakeArea(w.Box(1, 1, 1), new Vector3(0, 1, 0));
		PhysicsServer3D.AreaSetRayPickable(area, false);
		yield return Wait.Frames(3);

		// The hidden body never resolves its RID in a raycast...
		Assert.Expect(w.Ray(new Vector3(3, 5, 0), new Vector3(3, -5, 0)).Count == 0,
			"non-pickable body: ray misses");
		// ...while the pickable one is reported.
		var hitPickable = w.Ray(new Vector3(-3, 5, 0), new Vector3(-3, -5, 0));
		Assert.Expect(hitPickable.Count > 0 && hitPickable["rid"].AsRid() == pickable,
			"pickable body: ray hits");

		// Area rays: non-pickable area skipped until re-enabled.
		Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0), areas: true, bodies: false).Count == 0,
			"non-pickable area: ray misses");
		PhysicsServer3D.AreaSetRayPickable(area, true);
		yield return Wait.Frames(3);
		var areaHit = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0), areas: true, bodies: false);
		Assert.Expect(areaHit.Count > 0 && areaHit["rid"].AsRid() == area, "area hit after re-enable");

		// Shape/point queries ignore pickability entirely: the hidden body is found.
		Assert.Expect(w.Point(new Vector3(3, 1, 0)).Count > 0, "point query ignores pickability");
		var overlap = w.Overlap(w.Box(0.5f, 0.5f, 0.5f), new Transform3D(Basis.Identity, new Vector3(3, 1, 0)));
		Assert.Expect(overlap.Count > 0 && overlap[0]["rid"].AsRid() == hidden, "shape query ignores pickability");
	}
}
