// Shapes: per-type data plumbing, dynamic collision for every supported
// shape, the pairwise collision matrix (highest-value area), transform
// fidelity (rotation/translation conversion bugs), and live shape mutation.
//
// Rest-height windows are deliberately generous (±0.2…0.45) — they must catch
// "no collision / fell through / exploded", not quantize solver softness. The
// analytic windows come from each shape's half-extent along Y.

using System;
using System.Collections;

namespace PhysxTestProject.Tests;

internal static class ShapeTests {
    // (name, half-height along Y, factory). Half-heights are the expected
    // resting offset of the body origin above a flat surface.
    static readonly (string name, float half, Func<PhysxWorld, Rid> mk)[] Dyn = {
        ("BOX", 0.40f, w => w.Box(0.4f, 0.4f, 0.4f)),
        ("SPHERE", 0.40f, w => w.Sphere(0.4f)),
        ("CAPSULE", 0.60f, w => w.Capsule(0.25f, 1.2f)),
        ("CYLINDER", 0.50f, w => w.Cylinder(0.3f, 1.0f)),
        ("CONE", 0.50f, w => w.Cone(0.35f, 1.0f)),
        ("CONVEX", 0.45f, w => w.Tetra(0.7f)),
    };

    public static void Register(SuiteBuilder s) {
        s.Add("PHYSX-SHAPE-001", "box data round-trip", BoxRoundtrip);
        s.Add("PHYSX-SHAPE-002", "sphere data round-trip", SphereRoundtrip);
        s.Add("PHYSX-SHAPE-003", "capsule data round-trip", CapsuleRoundtrip);
        s.Add("PHYSX-SHAPE-004", "cylinder data round-trip", CylinderRoundtrip);
        s.Add("PHYSX-SHAPE-005", "custom cone data round-trip", ConeRoundtrip);
        s.Add("PHYSX-SHAPE-006", "convex polygon data round-trip", ConvexRoundtrip);
        s.Add("PHYSX-SHAPE-007", "concave polygon data round-trip", ConcaveRoundtrip);
        s.Add("PHYSX-SHAPE-008", "heightmap data round-trip", HeightmapRoundtrip);
        s.Add("PHYSX-SHAPE-009", "separation ray data round-trip", SepRayRoundtrip);
        s.Add("PHYSX-SHAPE-010", "world boundary data round-trip", PlaneRoundtrip);
        s.Add("PHYSX-SHAPE-011", "margin and solver-bias round-trip", MarginBiasRoundtrip);
        s.Add("PHYSX-SHAPE-012", "unknown custom shape type rejected safely", UnknownCustomType);
        s.Add("PHYSX-SHAPE-013", "raycast hits every primitive shape with sane normals", RayPrimitives);
        s.Add("PHYSX-SHAPE-014", "raycast hits mesh shapes (convex/concave/heightmap)", RayMeshes);
        s.Add("PHYSX-SHAPE-015", "shape overlap query against each shape family", OverlapPerFamily);
        s.Add("PHYSX-SHAPE-016", "heightmap slope directs sliding downhill", HeightmapSlope);
        s.Add("PHYSX-SHAPE-017", "heightmap valley captures sphere", HeightmapValley);
        s.Add("PHYSX-SHAPE-018", "body rests on world boundary plane", PlaneRest);
        s.Add("PHYSX-SHAPE-019", "world boundary is one-sided for rays", PlaneOneSided);
        s.Add("PHYSX-SHAPE-020", "separation ray invisible to raycasts; host body still collides", SepRayBehavior);
        s.Add("PHYSX-SHAPE-021", "multi-triangle concave roof deflects sphere and rays", ConcaveRoof);
        s.Add("PHYSX-SHAPE-022", "thin single-triangle floor holds a resting body", ThinTriangleFloor);

        // Pairwise matrix: dynamic-vs-dynamic pairs (i,j with j<=i covers same-type too).
        int n = 0;
        for (int i = 0; i < Dyn.Length; i++) {
            for (int j = 0; j <= i; j++) {
                int a = i, b = j;
                n++;
                s.Add($"PHYSX-SHAPE-P-{n:00}", $"{Dyn[a].name} x {Dyn[b].name}: dynamic pair collides, fall arrested at contact",
                    () => PairRest(Dyn[a], Dyn[b]));
            }
        }
        // Dynamic pairs vs static mesh-like shapes.
        var statics = new (string name, Func<PhysxWorld, Rid> mk, float top)[] {
            ("PLANE", w => w.WorldBoundary(new Plane(Vector3.Up, 0f)), 0f),
            ("HEIGHTMAP", w => w.FlatHeightmap(8, 0f), 0f),
            ("CONCAVE", w => w.Concave(TriFloor()), 0f),
        };
        for (int i = 0; i < Dyn.Length; i++) {
            for (int j = 0; j < statics.Length; j++) {
                int a = i, b = j;
                n++;
                s.Add($"PHYSX-SHAPE-P-{n:00}", $"{Dyn[a].name} x {statics[b].name}: dynamic-vs-static collides, fall arrested at contact",
                    () => PairRestStatic(Dyn[a], statics[b].mk, statics[b].top));
            }
        }

        s.Add("PHYSX-SHAPE-XF-001", "identity rotation: box rests at half-extent", XfIdentity);
        s.Add("PHYSX-SHAPE-XF-002", "90-degree rotated box rests at rotated half-extent", XfRot90);
        s.Add("PHYSX-SHAPE-XF-003", "rotated ramp: sphere slides in rotated direction", XfRampDirection);
        s.Add("PHYSX-SHAPE-XF-004", "shape local offset changes resting height", XfShapeOffset);
        s.Add("PHYSX-SHAPE-XF-005", "ray normal on rotated box matches rotated normal", XfRayNormal);
        s.Add("PHYSX-SHAPE-XF-006", "diagonal offset collision deflects sphere sideways", XfDiagonalDeflect);
        s.Add("PHYSX-SHAPE-MUT-001", "shared shape resize updates every attached body", MutSharedResize);
        s.Add("PHYSX-SHAPE-MUT-002", "removing one of two shapes leaves the other collidable", MutRemoveOne);
        s.Add("PHYSX-SHAPE-MUT-003", "clear_shapes removes all collision", MutClear);
        s.Add("PHYSX-SHAPE-MUT-004", "body_set_shape replaces geometry", MutReplace);
        s.Add("PHYSX-SHAPE-MUT-005", "shape disabled flag disables collision but keeps slot", MutDisabled);

        // Mirrored (negative-scale) convex shapes: Godot preserves mirroring
        // for ConvexPolygonShape3D; the module bakes it via signed PxMeshScale.
        s.Add("PHYSX-SHAPE-MIR-001", "mirrored convex cube drops and rests like unmirrored", MirroredCubeRest);
        s.Add("PHYSX-SHAPE-MIR-002", "mirrored convex wedge: ray height + normal follow the mirror", MirroredWedgeRay);
        s.Add("PHYSX-SHAPE-MIR-003", "rest_info on a mirrored query shape matches floor surface", MirroredRestInfo);
        s.Add("PHYSX-SHAPE-SCA-001", "heightmap at scale.y=2 collides at doubled sample height", HeightmapScaledUp);
        s.Add("PHYSX-SHAPE-SCA-002", "heightmap at scale.y=0.5 collides at halved sample height", HeightmapScaledDown);
        s.Add("PHYSX-SHAPE-SCA-003", "convex hull from >255 points cooks and collides", ConvexManyPoints);
        s.Add("PHYSX-SHAPE-SCA-004", "backface ray hit reports the author's face index", BackfaceFaceIndex);
        s.Add("PHYSX-SHAPE-SCA-005", "separation-ray-only body does not collide (query-only)", RayShapeNoSimulation);
    }

    // ------------------------------------------------------- Phase 5: scaling
    // 2x2 heightmap with a single raised sample at the (-0.5, -0.5) corner.
    static Rid RaisedHeightmap(PhysxWorld w) {
        var s = PhysicsServer3D.HeightmapShapeCreate();
        w.AdoptShape(s);
        PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary {
            ["width"] = 2, ["depth"] = 2, ["cell_size"] = 1f,
            ["heights"] = new float[] { 1f, 0f, 0f, 0f },
        });
        return s;
    }

    // A static body carrying a scaled heightmap: the raised sample (h=1) sits
    // at world y = 1*scale.y — the surface point a query must find.
    static Rid ScaledHeightmapBody(PhysxWorld w, float scaleY) {
        var shape = RaisedHeightmap(w);
        var b = PhysicsServer3D.BodyCreate();
        w.TrackBody(b);
        PhysicsServer3D.BodySetMode(b, PhysicsServer3D.BodyMode.Static);
        PhysicsServer3D.BodySetSpace(b, w.Space);
        PhysicsServer3D.BodyAddShape(b, shape);
        PhysicsServer3D.BodySetCollisionLayer(b, 1);
        PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Transform,
            new Transform3D(Basis.FromScale(new Vector3(1, scaleY, 1)), Vector3.Zero));
        return b;
    }

    static IEnumerator HeightmapScaledUp() {
        using var w = new PhysxWorld(false);
        ScaledHeightmapBody(w, 2f);
        yield return Wait.Frames(2);
        // The h=1 sample sits at world (-0.5, 2, -0.5) — a downward ray must
        // hit there, not at the unscaled height y=1 (raycasts probe heightfields;
        // rest_info's computePenetration rejects them).
        var hit = w.Ray(new Vector3(-0.5f, 5, -0.5f), new Vector3(-0.5f, -5, -0.5f));
        Assert.Require(hit.Count > 0, "ray hits the scaled heightmap");
        Assert.ExpectNear(hit["position"].AsVector3().Y, 2f, 0.05f,
            $"surface at the doubled sample height (got {hit["position"].AsVector3().Y:F3})");
    }

    static IEnumerator HeightmapScaledDown() {
        using var w = new PhysxWorld(false);
        ScaledHeightmapBody(w, 0.5f);
        yield return Wait.Frames(2);
        var hit = w.Ray(new Vector3(-0.5f, 5, -0.5f), new Vector3(-0.5f, -5, -0.5f));
        Assert.Require(hit.Count > 0, "ray hits the scaled heightmap");
        Assert.ExpectNear(hit["position"].AsVector3().Y, 0.5f, 0.05f,
            $"surface at the halved sample height (got {hit["position"].AsVector3().Y:F3})");
    }

    // Decomposed-mesh hulls routinely exceed PhysX's 255 cooked-polygon cap;
    // the cook must quantize and retry with plane shifting instead of failing.
    static IEnumerator ConvexManyPoints() {
        using var w = new PhysxWorld();
        var pts = new System.Collections.Generic.List<Vector3>();
        for (int i = 0; i < 300; i++) {
            float phi = i * 2.399963f; // golden-angle spiral on a sphere
            float theta = i * 1.117f;
            pts.Add(new Vector3(Mathf.Cos(phi) * Mathf.Cos(theta), Mathf.Sin(theta), Mathf.Sin(phi) * Mathf.Cos(theta)));
        }
        var hull = w.Convex(pts.ToArray());
        w.MakeStatic(hull, new Vector3(0, 3, 0));
        var ball = w.MakeBody(w.Sphere(0.3f), new Vector3(0, 5, 0));
        PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.Mass, 1f);
        yield return Wait.UntilOrFail(() => w.Pos(ball).Origin.Y < 4.0f, 240, "ball lands on the 300-point hull");
        Assert.Expect(w.Pos(ball).Origin.Y > 3.3f, $"ball rests ON the hull (y={w.Pos(ball).Origin.Y:F2})");
    }

    // Backface cooking duplicates every triangle; a ray hitting a reversed
    // duplicate must report the AUTHOR's triangle index (raw/2), not the raw
    // doubled-mesh index.
    static IEnumerator BackfaceFaceIndex() {
        using var w = new PhysxWorld(false);
        // Quad of 2 triangles at y=0, front faces UP (Godot CW convention).
        var concave = PhysicsServer3D.ConcavePolygonShapeCreate();
        w.AdoptShape(concave);
        PhysicsServer3D.ShapeSetData(concave, new Godot.Collections.Dictionary {
            ["faces"] = new Vector3[] {
                new(-1, 0, -1), new(1, 0, -1), new(-1, 0, 1),   // triangle 0
                new(-1, 0, 1), new(1, 0, -1), new(1, 0, 1),     // triangle 1
            },
            ["backface_collision"] = true,
        });
        w.MakeStatic(concave, Vector3.Zero);
        yield return Wait.Frames(2);
        // Ray from BELOW hits the reversed duplicate of triangle 0 (raw PhysX
        // index 2*0+1 = 1... in the module's cook the forward copy is the
        // swizzled (0,2,1) and the duplicate the original winding); the author
        // index must land back in [0, 1] and identify triangle 0's area.
        var hit = w.Ray(new Vector3(0, -2, 0), new Vector3(0, 2, 0), backFaces: true);
        Assert.Require(hit.Count > 0, "backface-enabled ray hits the quad from below");
        int fi = hit["face_index"].AsInt32();
        Assert.Expect(fi >= 0 && fi < 2, $"face index maps to the author's triangle (got {fi})");

        // From above, the forward copy of triangle 1's area also maps into range.
        var hit2 = w.Ray(new Vector3(0.5f, 2, 0.5f), new Vector3(0.5f, -2, 0.5f));
        Assert.Require(hit2.Count > 0, "front ray hits triangle 1's area");
        int fi2 = hit2["face_index"].AsInt32();
        Assert.Expect(fi2 >= 0 && fi2 < 2, $"front face index in author range (got {fi2})");
    }

    // Godot separation-ray shapes never generate simulation contacts — the
    // module's emulated thin box must be query-only, so a body resting where
    // only the ray volume is falls straight through.
    static IEnumerator RayShapeNoSimulation() {
        using var w = new PhysxWorld();
        var ray = w.SeparationRay(2f);
        w.MakeStatic(ray, new Vector3(6, 2, 0)); // ray-only static body
        var b = w.MakeBody(w.Sphere(0.3f), new Vector3(6, 2.4f, 0));
        PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.Mass, 1f);
        yield return Wait.Frames(90);
        Assert.Expect(w.Pos(b).Origin.Y < 1.5f,
            $"sphere fell through the ray-only body (y={w.Pos(b).Origin.Y:F2})");
    }

    // Godot front faces are CW seen from outside; the module (like Jolt)
    // swizzles triangle winding to PhysX CCW at cook time, so authored floors
    // here face UP in the Godot CW convention.
    static Vector3[] TriFloor() => new[] {
        new Vector3(-8, 0, -8), new Vector3(8, 0, -8), new Vector3(-8, 0, 8),
        new Vector3(-8, 0, 8), new Vector3(8, 0, -8), new Vector3(8, 0, 8),
    };

    // -------------------------------------------------------------- round-trips
    static IEnumerator BoxRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = PhysicsServer3D.BoxShapeCreate();
        w.AdoptShape(s);
        PhysicsServer3D.ShapeSetData(s, new Vector3(0.3f, 0.4f, 0.5f));
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.Box, "type is Box");
        Assert.ExpectVecNear(PhysicsServer3D.ShapeGetData(s).AsVector3(), new Vector3(0.3f, 0.4f, 0.5f), 1e-5f, "box data round-trip");
        yield return Wait.Frame();
    }
    static IEnumerator SphereRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Sphere(0.7f);
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.Sphere, "type is Sphere");
        Assert.ExpectNear(PhysicsServer3D.ShapeGetData(s).AsSingle(), 0.7f, 1e-5f, "sphere radius round-trip");
        yield return Wait.Frame();
    }
    static IEnumerator CapsuleRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Capsule(0.3f, 1.6f);
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.Capsule, "type is Capsule");
        Assert.ExpectNear(d["radius"].AsSingle(), 0.3f, 1e-4f, "capsule radius");
        Assert.ExpectNear(d["height"].AsSingle(), 1.6f, 1e-4f, "capsule height");
        yield return Wait.Frame();
    }
    static IEnumerator CylinderRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Cylinder(0.4f, 1.2f);
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.Cylinder, "type is Cylinder");
        Assert.ExpectNear(d["radius"].AsSingle(), 0.4f, 1e-4f, "cylinder radius");
        Assert.ExpectNear(d["height"].AsSingle(), 1.2f, 1e-4f, "cylinder height");
        yield return Wait.Frame();
    }
    static IEnumerator ConeRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Cone(0.7f, 1.5f);
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.Custom, "type is Custom");
        Assert.Expect(d["type"].AsString() == "cone", "custom type string is 'cone'");
        Assert.ExpectNear(d["radius"].AsSingle(), 0.7f, 1e-4f, "cone radius");
        Assert.ExpectNear(d["height"].AsSingle(), 1.5f, 1e-4f, "cone height");
        yield return Wait.Frame();
    }
    static IEnumerator ConvexRoundtrip() {
        using var w = new PhysxWorld(false);
        var pts = new Vector3[] { new(0, 0, 0), new(1, 0, 0), new(0, 1, 0), new(0, 0, 1) };
        var s = PhysicsServer3D.ConvexPolygonShapeCreate();
        w.AdoptShape(s);
        PhysicsServer3D.ShapeSetData(s, pts);
        var back = PhysicsServer3D.ShapeGetData(s).AsVector3Array();
        Assert.Expect(back.Length == 4, "convex point count round-trip");
        Assert.ExpectVecNear(back[0], pts[0], 1e-5f, "convex first point round-trip");
        yield return Wait.Frame();
    }
    static IEnumerator ConcaveRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Concave(TriFloor());
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(d["faces"].AsVector3Array().Length == 6, "concave face count");
        Assert.Expect(d["backface_collision"].AsBool() == false, "backface flag");
        yield return Wait.Frame();
    }
    static IEnumerator HeightmapRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.FlatHeightmap(4, 0f);
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(d["width"].AsInt32() == 4, "heightmap width");
        Assert.Expect(d["heights"].AsFloat32Array().Length == 16, "heightmap sample count");
        yield return Wait.Frame();
    }
    static IEnumerator SepRayRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.SeparationRay(2f, true);
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.ExpectNear(d["length"].AsSingle(), 2f, 1e-4f, "sep ray length");
        Assert.Expect(d["slide_on_slope"].AsBool(), "sep ray slide flag");
        yield return Wait.Frame();
    }
    static IEnumerator PlaneRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.WorldBoundary(new Plane(Vector3.Up, 0f));
        var p = PhysicsServer3D.ShapeGetData(s).AsPlane();
        Assert.Expect(PhysicsServer3D.ShapeGetType(s) == PhysicsServer3D.ShapeType.WorldBoundary, "type is WorldBoundary");
        Assert.ExpectVecNear(p.Normal, Vector3.Up, 1e-4f, "plane normal round-trip");
        yield return Wait.Frame();
    }
    static IEnumerator MarginBiasRoundtrip() {
        using var w = new PhysxWorld(false);
        var s = w.Box(0.5f);
        PhysicsServer3D.ShapeSetMargin(s, 0.05f);
        Assert.ExpectNear(PhysicsServer3D.ShapeGetMargin(s), 0.05f, 1e-5f, "margin round-trip");
        yield return Wait.Frame();
    }
    static IEnumerator UnknownCustomType() {
        using var w = new PhysxWorld();
        var s = PhysicsServer3D.CustomShapeCreate();
        w.AdoptShape(s);
        PhysicsServer3D.ShapeSetData(s, new Godot.Collections.Dictionary { ["type"] = "banana", ["radius"] = 1f });
        var d = PhysicsServer3D.ShapeGetData(s).AsGodotDictionary();
        Assert.Expect(d["type"].AsString() == "", "unknown custom type reports empty type string");
        var b = w.MakeBody(w.Box(0.4f), new Vector3(0, 2, 0));
        yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.6f, 200, "simulation healthy after invalid custom shape");
    }

    // -------------------------------------------------------------- queries per shape
    static IEnumerator RayPrimitives() {
        using var w = new PhysxWorld(false);
        float x = 0;
        foreach (var mk in new Func<Rid>[] {
            () => w.Box(0.8f, 0.4f, 0.8f), () => w.Sphere(0.5f), () => w.Capsule(0.3f, 1.4f),
            () => w.Cylinder(0.35f, 1.0f), () => w.Cone(0.4f, 1.0f),
        }) {
            var s = mk();
            w.MakeStatic(s, new Vector3(x, 2, 0));
            var hit = w.Ray(new Vector3(x, 8, 0), new Vector3(x, -8, 0));
            Assert.Expect(hit.Count > 0, $"ray hits shape at x={x}");
            if (hit.Count > 0) {
                Assert.Expect(hit["normal"].AsVector3().Y > 0.5f, $"top normal is up for shape at x={x} (got {hit["normal"].AsVector3()})");
                Assert.Expect(hit["position"].AsVector3().Y < 3.2f, $"hit position on upper surface at x={x}");
            }
            x += 4;
        }
        yield return Wait.Frame();
    }
    static IEnumerator RayMeshes() {
        using var w = new PhysxWorld(false);
        var convex = w.Tetra(1.4f);
        w.MakeStatic(convex, new Vector3(0, 1, 0));
        var concave = w.Concave(TriFloor());
        w.MakeStatic(concave, new Vector3(20, 2, 0));
        var hm = w.FlatHeightmap(8, 1.5f);
        w.MakeStatic(hm, new Vector3(40, 2, 0));

        var h1 = w.Ray(new Vector3(0, 8, 0), new Vector3(0, -8, 0));
        Assert.Expect(h1.Count > 0, "ray hits convex hull");
        var h2 = w.Ray(new Vector3(20, 8, 0), new Vector3(20, -8, 0));
        Assert.Expect(h2.Count > 0, "ray hits concave mesh");
        if (h2.Count > 0)
            Assert.Expect(h2["position"].AsVector3().Y < 2.6f, "concave hit on triangle surface");
        var h3 = w.Ray(new Vector3(40, 8, 0), new Vector3(40, -8, 0));
        Assert.Expect(h3.Count > 0, "ray hits heightmap");
        Assert.Expect(h3["position"].AsVector3().Y < 4f, "heightmap hit on terrain surface");
        yield return Wait.Frame();
    }
    static IEnumerator OverlapPerFamily() {
        using var w = new PhysxWorld(false);
        float x = 0;
        foreach (var mk in new Func<Rid>[] {
            () => w.Box(0.8f), () => w.Sphere(0.6f), () => w.Capsule(0.3f, 1.2f),
            () => w.Cylinder(0.4f, 1.0f), () => w.Cone(0.4f, 1.0f), () => w.Tetra(1.2f),
        }) {
            var s = mk();
            var b = w.MakeStatic(s, new Vector3(x, 3, 0));
            var probe = w.Sphere(0.3f);
            var hits = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(x, 3, 0)), max: 32);
            var found = false;
            foreach (var h in hits) if (h["rid"].AsRid() == b) found = true;
            Assert.Expect(found, $"overlap query finds {PhysicsServer3D.ShapeGetType(s)} body");
            var away = w.Overlap(probe, new Transform3D(Basis.Identity, new Vector3(x, 8, 0)));
            Assert.Expect(away.Count == 0, $"overlap query misses distant {PhysicsServer3D.ShapeGetType(s)} body");
            x += 8;
        }
        yield return Wait.Frame();
    }

    // -------------------------------------------------------------- pairwise matrix
    // Drop protocol: the dynamic body falls from rest height + 1.2 m and must
    // (a) reach impact speed >= 3 m/s and (b) have its fall arrested (|vy| <
    // 2.5) at some frame while still above the static's contact zone. This
    // validates contact generation and impulse response for every pair,
    // including asymmetric (tetra) and pointy (cone) shapes where eternal
    // balance is metastable in every engine: a tunneling pair crosses the
    // contact zone at full impact speed and never satisfies the arrest gate.
    // Diagnosed 2026-08-30 via trajectory probe: contacts ARE generated for
    // all previously failing pairs; they slide off tip-first and land below.
    static IEnumerator PairRest((string name, float half, Func<PhysxWorld, Rid> mk) a,
                                 (string name, float half, Func<PhysxWorld, Rid> mk) b) {
        using var w = new PhysxWorld(false);
        w.AddFloor(-20f); // safety floor far below
        var sa = a.half >= b.half ? a : b; // larger stays static on the ground
        var sd = a.half >= b.half ? b : a;
        var staticShape = sa.mk(w);
        w.MakeStatic(staticShape, new Vector3(0, 0, 0));
        var dynShape = sd.mk(w);
        var body = w.MakeBody(dynShape, new Vector3(0f, sa.half + sd.half + 1.2f, 0));
        PhysicsServer3D.BodySetParam(body, PhysicsServer3D.BodyParameter.LinearDamp, 2f);
        PhysicsServer3D.BodySetParam(body, PhysicsServer3D.BodyParameter.AngularDamp, 6f);
        float top = 2f * sa.half; // static's top surface
        float minVy = 0f;
        bool impact = false, arrested = false;
        for (int f = 0; f < 300; f++) {
            yield return Wait.Frame();
            float vy = w.Vel(body).Y;
            if (vy < minVy) minVy = vy;
            if (minVy <= -3f) impact = true;
            if (impact && !arrested && Math.Abs(vy) < 2.5f && w.Pos(body).Origin.Y > top - 0.6f) arrested = true;
        }
        Assert.Expect(impact, $"{a.name} x {b.name}: impact reached (min vy {minVy:F2})");
        Assert.Expect(arrested, $"{a.name} x {b.name}: fall arrested at the static's contact zone");
    }

    static IEnumerator PairRestStatic((string name, float half, Func<PhysxWorld, Rid> mk) a,
                                      Func<PhysxWorld, Rid> mkStatic, float top) {
        using var w = new PhysxWorld(false);
        w.AddFloor(-20f);
        var st = mkStatic(w);
        w.MakeStatic(st, new Vector3(0, top, 0));
        var body = w.MakeBody(a.mk(w), new Vector3(0f, top + a.half + 1.2f, 0));
        PhysicsServer3D.BodySetParam(body, PhysicsServer3D.BodyParameter.LinearDamp, 2f);
        PhysicsServer3D.BodySetParam(body, PhysicsServer3D.BodyParameter.AngularDamp, 6f);
        float minVy = 0f;
        bool impact = false, arrested = false;
        for (int f = 0; f < 300; f++) {
            yield return Wait.Frame();
            float vy = w.Vel(body).Y;
            if (vy < minVy) minVy = vy;
            if (minVy <= -3f) impact = true;
            if (impact && !arrested && Math.Abs(vy) < 2.5f && w.Pos(body).Origin.Y > top - 0.6f) arrested = true;
        }
        Assert.Expect(impact, $"{a.name}: impact reached on static surface (min vy {minVy:F2})");
        Assert.Expect(arrested, $"{a.name}: fall arrested on the static's contact zone");
    }

    // -------------------------------------------------------------- heightmap / plane / sepray
    static IEnumerator HeightmapSlope() {
        using var w = new PhysxWorld(false);
        const int n = 16;
        var hs = new float[n * n];
        for (int z = 0; z < n; z++)
            for (int x = 0; x < n; x++)
                hs[z * n + x] = x * 0.8f; // steep ramp (38+ deg) so friction cannot hold the ball
        var hm = w.Heightmap(n, n, hs);
        w.MakeStatic(hm, new Vector3(0, 2, 0));
        var ball = w.MakeBody(w.Sphere(0.3f), new Vector3(4, 15f, 0));
        yield return Wait.Frames(300);
        Assert.Expect(w.Pos(ball).Origin.X < 2.5f,
            $"sphere slides down steep heightmap slope (x={w.Pos(ball).Origin.X:F2})");
    }
    static IEnumerator HeightmapValley() {
        using var w = new PhysxWorld(false);
        const int n = 16;
        var hs = new float[n * n];
        for (int z = 0; z < n; z++)
            for (int x = 0; x < n; x++)
                hs[z * n + x] = 0.15f * (x - n / 2) * (x - n / 2); // steep valley (max ~1.35)
        var hm = w.Heightmap(n, n, hs);
        w.MakeStatic(hm, new Vector3(0, 2, 0));
        var ball = w.MakeBody(w.Sphere(0.3f), new Vector3(3f, 5.2f, 0));
        PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.LinearDamp, 1.5f);
        // The settle gate (|vx| < 0.1) is trivially true at spawn (vx starts
        // 0) — only trust it once the ball has landed and rolled down into
        // the valley (y below the spawn column's slope surface).
        yield return Wait.UntilOrFail(
            () => w.Pos(ball).Origin.Y < 3f && Math.Abs(w.Vel(ball).X) < 0.1f, 600, "sphere settles in valley");
        Assert.Expect(Math.Abs(w.Pos(ball).Origin.X) < 1.5f,
            $"sphere captured by heightmap valley (x={w.Pos(ball).Origin.X:F2})");
    }
    static IEnumerator PlaneRest() {
        using var w = new PhysxWorld(false);
        var plane = w.WorldBoundary(new Plane(Vector3.Up, 0f));
        w.MakeStatic(plane, Vector3.Zero);
        var probe = w.Ray(new Vector3(3, 8, 0), new Vector3(3, -8, 0));
        float planeY = probe.Count > 0 ? probe["position"].AsVector3().Y : float.NaN;
        var b = w.MakeBody(w.Box(0.5f), new Vector3(0, 3, 0));
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(b).Y) < 0.05f, 400, "body settles on plane");
        Assert.ExpectNear(planeY, 0f, 0.05f, "ray sees boundary plane at y=0");
        Assert.ExpectNear(w.Pos(b).Origin.Y, planeY + 0.5f, 0.15f,
            $"body rests on plane surface at half-extent (plane@{planeY:F2}, body@{w.Pos(b).Origin.Y:F2})");
    }
    static IEnumerator PlaneOneSided() {
        using var w = new PhysxWorld(false);
        var plane = w.WorldBoundary(new Plane(Vector3.Up, 0f));
        w.MakeStatic(plane, Vector3.Zero);
        var up = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
        Assert.Expect(up.Count > 0, "ray from above hits plane");
        var below = w.Ray(new Vector3(0, -5, 0), new Vector3(0, 5, 0));
        Assert.Expect(below.Count == 0, "ray from below misses plane (one-sided)");
        yield return Wait.Frame();
    }
    static IEnumerator SepRayBehavior() {
        using var w = new PhysxWorld();
        var sr = w.SeparationRay(2f);
        var host = w.MakeStatic(w.Box(0.5f), new Vector3(10, 2, 0));
        var srOnly = w.MakeStatic(sr, new Vector3(0, 2, 0)); // sep-ray-only static body
        yield return Wait.Frames(5);
        var hit = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
        // The world floor also intersects this ray, so the correct strong claim
        // is: the nearest hit is the floor — never the separation-ray body.
        Assert.Expect(hit.Count > 0 && hit["rid"].AsRid() == w.FloorRid,
            "raycast sees only the floor, never the separation-ray-only body");
        var b = w.MakeBody(w.Box(0.4f), new Vector3(10, 3, 0));
        yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 3.1f, 200, "box+sepray host body still collides normally");
        Assert.ExpectNear(w.Pos(b).Origin.Y, 2.9f, 0.2f, "body resting on sep-ray host box");
    }
    static IEnumerator ConcaveRoof() {
        using var w = new PhysxWorld(false);
        w.AddFloor(-15f);
        // Two slanted quads forming a roof ridge along z at y=2 (Godot CW
        // winding, front faces up — see TriFloor note).
        var left = new Vector3[] { new(-4, 0.5f, 0), new(0, 2, 0), new(-4, 0.5f, 2), new(-4, 0.5f, 2), new(0, 2, 0), new(0, 2, 2) };
        var right = new Vector3[] { new(4, 0.5f, 0), new(4, 0.5f, 2), new(0, 2, 0), new(4, 0.5f, 2), new(0, 2, 2), new(0, 2, 0) };
        var roof = w.Concave(left);
        w.AdoptShape(roof);
        var roof2 = w.Concave(right);
        w.MakeStatic(roof, Vector3.Zero);
        w.MakeStatic(roof2, Vector3.Zero);
        // Raycast the clean roof BEFORE the ball exists — the ball settles at
        // this exact column and its top-sphere normal (0,1,0) would shadow the
        // roof slant.
        yield return Wait.Frames(3);
        var hit = w.Ray(new Vector3(-1.5f, 8, 1), new Vector3(-1.5f, -8, 1));
        Assert.Expect(hit.Count > 0, "ray hits roof triangle");
        if (hit.Count > 0) {
            var n = hit["normal"].AsVector3();
            Assert.Expect(n.Y > 0.2f && n.Y < 0.95f, $"roof normal is slanted (got {n})");
        }
        var ball = w.MakeBody(w.Sphere(0.2f), new Vector3(-1.5f, 4, 1));
        PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.LinearDamp, 1f);
        yield return Wait.UntilOrFail(() => w.Pos(ball).Origin.Y < 2.8f, 300, "sphere reaches roof");
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(ball).Y) < 0.2f, 600, "sphere settles after rolling off roof");
        Assert.Expect(w.Pos(ball).Origin.Y < 2.5f, "sphere rolled/landed below ridge");
    }
    static IEnumerator ThinTriangleFloor() {
        using var w = new PhysxWorld(false);
        // Single triangle wound face-UP in Godot's CW convention (see TriFloor
        // note): with backface_collision=false the cooked triangle is correctly
        // one-sided and a down-wound sphere-facing triangle would tunnel.
        var tri = w.Concave(new Vector3(-6, 0, -6), new Vector3(6, 0, -6), new Vector3(0, 0, 6));
        w.MakeStatic(tri, new Vector3(0, 2, 0));
        var b = w.MakeBody(w.Sphere(0.3f), new Vector3(0.5f, 4, 0.5f));
        yield return Wait.UntilOrFail(
            () => Math.Abs(w.Pos(b).Origin.Y - 2.3f) < 0.15f, 240, "sphere rests on single thin triangle");
        Assert.ExpectNear(w.Pos(b).Origin.Y, 2.3f, 0.2f, "sphere rests on single thin triangle");
    }

    // -------------------------------------------------------------- transforms
    static IEnumerator XfIdentity() {
        using var w = new PhysxWorld(false);
        var plane = w.WorldBoundary();
        w.MakeStatic(plane, Vector3.Zero);
        var b = w.MakeBody(w.Box(0.4f, 0.3f, 0.4f), new Vector3(0, 2, 0));
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(b).Y) < 0.05f, 300, "box settles");
        Assert.ExpectNear(w.Pos(b).Origin.Y, 0.3f, 0.12f, "identity rotation: rests at y-half-extent 0.3");
    }
    static IEnumerator XfRot90() {
        using var w = new PhysxWorld(false);
        var plane = w.WorldBoundary();
        w.MakeStatic(plane, Vector3.Zero);
        // Box 0.8x0.2x0.6 rotated 90 degrees about X: Y half-extent becomes 0.3.
        var b = w.MakeBody(w.Box(0.4f, 0.1f, 0.3f), new Vector3(0, 2, 0), shapeXf: new Transform3D(Basis.FromEuler(new Vector3(Mathf.Pi / 2f, 0, 0)), Vector3.Zero));
        PhysicsServer3D.BodySetParam(b, PhysicsServer3D.BodyParameter.AngularDamp, 4f);
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(b).Y) < 0.05f && Math.Abs(w.AngVel(b).Y) < 0.05f, 500, "rotated box settles");
        float y = w.Pos(b).Origin.Y;
        Assert.Expect(y > 0.18f && y < 0.45f,
            $"90-degree-rotated box rests near rotated half-extent 0.3 (got {y:F2})");
    }
    static IEnumerator XfRampDirection() {
        using var w = new PhysxWorld(false);
        // Ramp rotated 20 degrees about Z: high side at -x, so ball slides +x.
        var rot = Basis.FromEuler(new Vector3(0, 0, Mathf.DegToRad(20)));
        var ramp = w.MakeStatic(w.Box(10, 0.4f, 4), new Vector3(0, 1, 0), shapeXf: new Transform3D(rot, Vector3.Zero));
        var ball = w.MakeBody(w.Sphere(0.25f), new Vector3(3, 2.8f, 0));
        PhysicsServer3D.BodySetParam(ball, PhysicsServer3D.BodyParameter.LinearDamp, 0.5f);
        yield return Wait.Frames(240);
        Assert.Expect(w.Pos(ball).Origin.X < 1.5f,
            $"sphere slides downhill (-x) on Z-rotated ramp (x={w.Pos(ball).Origin.X:F2}) — rotation conversion correct");
    }
    static IEnumerator XfShapeOffset() {
        using var w = new PhysxWorld();
        var b = w.MakeBody(w.Box(0.3f), new Vector3(0, 2, 0), shapeXf: new Transform3D(Basis.Identity, new Vector3(0.5f, 0.5f, 0)));
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(b).Y) < 0.05f, 300, "offset-shape body settles");
        float y = w.Pos(b).Origin.Y;
        // Shape center rests (offset 0.5) + half 0.3 above the floor, so the
        // BODY origin settles at 0.3 - 0.5 = -0.2: the offset is honored.
        Assert.Expect(y > -0.35f && y < -0.05f, $"shape offset honored: body origin rests at -0.2 (got {y:F2})");
    }
    static IEnumerator XfRayNormal() {
        using var w = new PhysxWorld(false);
        var rot = Basis.FromEuler(new Vector3(0, 0, Mathf.DegToRad(45)));
        w.MakeStatic(w.Box(1, 0.2f, 1), new Vector3(0, 2, 0), shapeXf: new Transform3D(rot, Vector3.Zero));
        yield return Wait.Frames(5);
        var hit = w.Ray(new Vector3(0.3f, 8, 0), new Vector3(0.3f, -8, 0));
        Assert.Expect(hit.Count > 0, "ray hits rotated box");
        if (hit.Count > 0) {
            var n = hit["normal"].AsVector3();
            // Top face normal rotated 45 deg about Z: (+/-0.707, +0.707, 0) —
            // either of the two upward faces can win the hit near the ridge.
            Assert.Expect(Mathf.Abs(Mathf.Abs(n.X) - 0.707f) < 0.12f && Mathf.Abs(n.Y - 0.707f) < 0.12f,
                $"normal matches 45-degree rotation (got {n})");
        }
    }
    static IEnumerator XfDiagonalDeflect() {
        using var w = new PhysxWorld(false);
        w.AddFloor(-10f);
        var pedestal = w.MakeStatic(w.Sphere(0.8f), new Vector3(0, 0.8f, 0));
        var ball = w.MakeBody(w.Sphere(0.25f), new Vector3(0.55f, 4f, 0));
        yield return Wait.Frames(240);
        Assert.Expect(w.Pos(ball).Origin.X > 0.8f,
            $"diagonally-offset sphere deflects to +x off pedestal (x={w.Pos(ball).Origin.X:F2})");
    }

    // -------------------------------------------------------------- mutation
    static IEnumerator MutSharedResize() {
        using var w = new PhysxWorld();
        var shared = w.Box(0.4f);
        var a = w.MakeBody(shared, new Vector3(-1, 0.4f, 0));
        var b = w.MakeBody(shared, new Vector3(1, 0.4f, 0));
        yield return Wait.UntilOrFail(() => Math.Abs(w.Vel(a).Y) < 0.05f, 300, "bodies settle on shared shape");
        // Grow the shared shape: both bodies must rise to the new half-extent.
        // SHAPE_BOX data is half-extents (REG-0013): half 0.8 = data 0.8.
        PhysicsServer3D.ShapeSetData(shared, new Vector3(0.8f, 0.8f, 0.8f));
        PhysicsServer3D.BodySetState(a, PhysicsServer3D.BodyState.Sleeping, false);
        PhysicsServer3D.BodySetState(b, PhysicsServer3D.BodyState.Sleeping, false);
        // Wait on the POSITION, not the velocity: with the project's
        // max_depenetration_velocity cap the bodies creep up at a bounded
        // rate, and their velocity can dip below the settle threshold while
        // they are still well below the new rest height.
        yield return Wait.UntilOrFail(() => w.Pos(a).Origin.Y > 0.7f && w.Pos(b).Origin.Y > 0.7f, 300, "bodies rise to the new half-extent");
        Assert.Expect(w.Pos(a).Origin.Y > 0.6f && w.Pos(a).Origin.Y < 1.0f, $"body A follows shared resize (y={w.Pos(a).Origin.Y:F2})");
        Assert.Expect(w.Pos(b).Origin.Y > 0.6f && w.Pos(b).Origin.Y < 1.0f, $"body B follows shared resize (y={w.Pos(b).Origin.Y:F2})");
    }
    static IEnumerator MutRemoveOne() {
        using var w = new PhysxWorld(false);
        var s1 = w.Box(0.5f);
        var s2 = w.Box(0.5f);
        var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 1, 0));
        PhysicsServer3D.BodyAddShape(b, s1, new Transform3D(Basis.Identity, new Vector3(2, 1, 0)));
        PhysicsServer3D.BodyAddShape(b, s2, new Transform3D(Basis.Identity, new Vector3(-2, 1, 0)));
        yield return Wait.Frames(3);
        Assert.Expect(w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0)).Count > 0, "first shape hit before removal");
        Assert.Expect(w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0)).Count > 0, "second shape hit before removal");
        PhysicsServer3D.BodyRemoveShape(b, 2); // remove the -x shape (index 0 is the base shape)
        yield return Wait.Frames(3);
        Assert.Expect(PhysicsServer3D.BodyGetShapeCount(b) == 2, "shape count after removal");
        Assert.Expect(w.Ray(new Vector3(2, 5, 0), new Vector3(2, -5, 0)).Count > 0, "kept shape still hit");
        Assert.Expect(w.Ray(new Vector3(-2, 5, 0), new Vector3(-2, -5, 0)).Count == 0, "removed shape no longer hit");
    }
    static IEnumerator MutClear() {
        using var w = new PhysxWorld(false);
        var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 1, 0));
        PhysicsServer3D.BodyAddShape(b, w.Box(0.5f), Transform3D.Identity);
        yield return Wait.Frames(3);
        Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count > 0, "body hit before clear");
        PhysicsServer3D.BodyClearShapes(b);
        yield return Wait.Frames(3);
        Assert.Expect(PhysicsServer3D.BodyGetShapeCount(b) == 0, "shape count zero after clear");
        Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0, "body gone from queries after clear");
    }
    static IEnumerator MutReplace() {
        using var w = new PhysxWorld(false);
        var box = w.Box(0.5f);
        var b = w.MakeStatic(box, new Vector3(0, 1, 0));
        yield return Wait.Frames(3);
        var sphere = w.Sphere(0.7f);
        PhysicsServer3D.BodySetShape(b, 0, sphere);
        yield return Wait.Frames(3);
        Assert.Expect(PhysicsServer3D.BodyGetShape(b, 0) == sphere, "body_get_shape returns replacement RID");
        var hit = w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0));
        Assert.Expect(hit.Count > 0 && hit["position"].AsVector3().Y > 1.4f, "replacement geometry used (sphere top higher than box top)");
    }
    static IEnumerator MutDisabled() {
        using var w = new PhysxWorld(false);
        var b = w.MakeStatic(w.Box(0.5f), new Vector3(0, 1, 0));
        yield return Wait.Frames(3);
        PhysicsServer3D.BodySetShapeDisabled(b, 0, true);
        yield return Wait.Frames(3);
        Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count == 0, "disabled shape not hit");
        PhysicsServer3D.BodySetShapeDisabled(b, 0, false);
        yield return Wait.Frames(3);
        Assert.Expect(w.Ray(new Vector3(0, 5, 0), new Vector3(0, -5, 0)).Count > 0, "re-enabled shape hit again");
    }

    // ------------------------------------------------------------- mirrored
    // Triangular prism, origin at bottom center: bottom rect x in [-1,1],
    // top edge at x=+1,y=+1. Slope plane y=(x+1)/2, normal ~(-0.447,0.894,0).
    static Rid Wedge(PhysxWorld w) => w.Convex(
        new Vector3(-1, 0, -1), new Vector3(1, 0, -1), new Vector3(1, 0, 1),
        new Vector3(-1, 0, 1), new Vector3(1, 1, -1), new Vector3(1, 1, 1));
    static Rid ConvexCube(PhysxWorld w, float half) => w.Convex(
        new Vector3(-half, -half, -half), new Vector3(half, -half, -half),
        new Vector3(half, -half, half), new Vector3(-half, -half, half),
        new Vector3(-half, half, -half), new Vector3(half, half, -half),
        new Vector3(half, half, half), new Vector3(-half, half, half));

    // A symmetric hull is mirror-invariant; this only proves the mirrored
    // attachment still collides (no crash/NaN from the negative PxMeshScale).
    static IEnumerator MirroredCubeRest() {
        using var w = new PhysxWorld();
        var hull = ConvexCube(w, 0.4f);
        var b = w.MakeBody(hull, new Vector3(0, 5, 0),
            shapeXf: new Transform3D(Basis.FromScale(new Vector3(-1, 1, 1)), Vector3.Zero));
        yield return Wait.UntilOrFail(() => w.Pos(b).Origin.Y < 0.6f, 240, "mirrored convex lands");
        yield return Wait.Frames(90);
        Assert.ExpectNear(w.Pos(b).Origin.Y, 0.4f, 0.2f, "mirrored convex cube rests at half-height");
        Assert.Expect(PhysxWorld.Finite(w.Pos(b)) && PhysxWorld.Finite(w.Vel(b)), "state finite after mirrored rest");
    }

    // The decisive geometry check: the wedge is asymmetric, so an X-mirror
    // moves the slope. With the mirror applied, a ray at local x=-0.5 hits the
    // slope 0.75 above the base with the mirrored normal; without it (old abs
    // behavior) the same ray would hit the low side at 0.25 with a -X normal.
    static IEnumerator MirroredWedgeRay() {
        using var w = new PhysxWorld(false);
        w.MakeStatic(Wedge(w), new Vector3(-10, 0, 0));
        w.MakeStatic(Wedge(w), new Vector3(10, 0, 0),
            shapeXf: new Transform3D(Basis.FromScale(new Vector3(-1, 1, 1)), Vector3.Zero));
        yield return Wait.Frames(3);

        var h1 = w.Ray(new Vector3(-10 + 0.5f, 5, 0), new Vector3(-10 + 0.5f, -5, 0));
        Assert.Require(h1.Count > 0, "ray hits unmirrored wedge slope");
        Assert.ExpectNear(h1["position"].AsVector3().Y, 0.75f, 0.05f, "unmirrored slope height at +0.5");
        Assert.Expect(h1["normal"].AsVector3().X < -0.3f, "unmirrored slope normal tilts toward -X");

        var h2 = w.Ray(new Vector3(10 - 0.5f, 5, 0), new Vector3(10 - 0.5f, -5, 0));
        Assert.Require(h2.Count > 0, "ray hits mirrored wedge slope");
        Assert.ExpectNear(h2["position"].AsVector3().Y, 0.75f, 0.05f, "mirrored slope height at -0.5 (mirror applied)");
        Assert.Expect(h2["normal"].AsVector3().X > 0.3f, "mirrored slope normal tilts toward +X");
        yield return Wait.Frame();
    }

    // Query-vs-simulation agreement: the mirrored query shape must resolve
    // against the floor the same way the mirrored attachment collides.
    static IEnumerator MirroredRestInfo() {
        using var w = new PhysxWorld(); // floor top at y=0
        var hull = ConvexCube(w, 0.4f);
        var rm = w.RestInfo(hull, new Transform3D(Basis.FromScale(new Vector3(-1, 1, 1)), new Vector3(0, 0.3f, 0)));
        var rp = w.RestInfo(hull, new Transform3D(Basis.FromScale(Vector3.One), new Vector3(5, 0.3f, 0)));
        Assert.Require(rm.Count > 0, "mirrored overlapping query reports rest info");
        Assert.Expect(rm["normal"].AsVector3().Y > 0.9f, "mirrored rest normal points up out of the floor");
        Assert.ExpectNear(rm["point"].AsVector3().Y, 0f, 0.05f, "mirrored rest point on floor surface");
        Assert.Expect(rp.Count > 0 && rp["normal"].AsVector3().Y > 0.9f, "unmirrored control agrees");
        yield return Wait.Frame();
    }
}
