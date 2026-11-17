/**************************************************************************/
/*  test_simd_benchmark.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

// Throughput and latency of every SIMD path against its original scalar implementation, measured
// in the same binary. Skipped by default; run with:
//   <binary> --test --test-case="*Benchmark*" --no-skip
// Use an optimized build (for example `scons target=editor tests=yes`): dev builds are compiled
// without optimization and their numbers are meaningless.

#include "core/math/geometry_3d.h"
#include "core/os/os.h"
#include "tests/core/math/test_simd.h"

namespace TestSIMDBenchmark {

using namespace SIMDReference;
using TestSIMD::Random;

static volatile float sink_float;
static volatile uint64_t sink_int;

// Nanoseconds per operation: each sample runs long enough to be measurable, the best of 5 is kept.
template <typename F>
double measure_ns(int64_t p_ops_per_call, F p_call) {
	OS *os = OS::get_singleton();
	p_call();
	int64_t reps = 1;
	while (true) {
		const uint64_t t0 = os->get_ticks_usec();
		for (int64_t r = 0; r < reps; r++) {
			p_call();
		}
		if (os->get_ticks_usec() - t0 >= 20000 || reps >= (int64_t(1) << 30)) {
			break;
		}
		reps *= 2;
	}
	double best = 1e30;
	for (int sample = 0; sample < 5; sample++) {
		const uint64_t t0 = os->get_ticks_usec();
		for (int64_t r = 0; r < reps; r++) {
			p_call();
		}
		const double us = double(os->get_ticks_usec() - t0);
		best = MIN(best, us);
	}
	return best * 1000.0 / double(reps * p_ops_per_call);
}

struct Table {
	String section;

	void header(const String &p_section) {
		section = p_section;
		print_line("");
		print_line(vformat("%-44s %12s %12s %9s", p_section, "scalar ns", "simd ns", "speedup"));
	}

	template <typename S, typename V>
	void row(const String &p_name, int64_t p_ops, S p_scalar, V p_simd) {
		const double scalar = measure_ns(p_ops, p_scalar);
		const double simd = measure_ns(p_ops, p_simd);
		print_line(vformat("  %-42s %12.2f %12.2f %8.2fx", p_name, scalar, simd, simd > 0 ? scalar / simd : 0.0));
	}
};

constexpr int N = 4096;

static void run_math(Random &p_rng, Table &p_table) {
	LocalVector<AABB> aabbs;
	LocalVector<Transform3D> xforms;
	LocalVector<Basis> bases;
	LocalVector<Vector4> v4;
	LocalVector<Vector3> v3;
	LocalVector<Vector2> v2;
	LocalVector<Quaternion> quats;
	LocalVector<Plane> planes;
	LocalVector<Projection> projections;
	LocalVector<Rect2> rects;
	LocalVector<Transform2D> xforms2d;
	LocalVector<Color> colors;
	for (int i = 0; i < N; i++) {
		aabbs.push_back(AABB(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()), Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()).abs() + Vector3(0.1, 0.1, 0.1)));
		xforms.push_back(Transform3D(Basis(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()), Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()), Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit())), Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit())));
		bases.push_back(xforms[i].basis);
		v4.push_back(Vector4(p_rng.unit() * 10, p_rng.unit() * 10, p_rng.unit() * 10, p_rng.unit() * 10));
		v3.push_back(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()));
		v2.push_back(Vector2(p_rng.unit(), p_rng.unit()));
		quats.push_back(p_rng.quaternion());
		planes.push_back(Plane(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()).normalized(), p_rng.unit()));
		Projection p;
		for (int k = 0; k < 4; k++) {
			p.columns[k] = Vector4(p_rng.unit(), p_rng.unit(), p_rng.unit(), p_rng.unit());
		}
		projections.push_back(p);
		rects.push_back(Rect2(p_rng.unit(), p_rng.unit(), p_rng.unit() + 1.5f, p_rng.unit() + 1.5f));
		xforms2d.push_back(Transform2D(Vector2(p_rng.unit(), p_rng.unit()), Vector2(p_rng.unit(), p_rng.unit()), Vector2(p_rng.unit(), p_rng.unit())));
		colors.push_back(Color(p_rng.unit(), p_rng.unit(), p_rng.unit(), p_rng.unit() * 0.5f + 0.5f));
	}
	LocalVector<AABB> out_aabb;
	out_aabb.resize(N);
	LocalVector<Vector4> out_v4;
	out_v4.resize(N);
	LocalVector<Vector3> out_v3;
	out_v3.resize(N);
	LocalVector<Vector2> out_v2;
	out_v2.resize(N);
	LocalVector<Rect2> out_rect;
	out_rect.resize(N);
	LocalVector<Color> out_color;
	out_color.resize(N);
	const int M = N - 1;

	p_table.header("Math (throughput, per call)");
	p_table.row("AABB::intersects_convex_shape (6 planes)", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefAABB(aabbs[i]).ref_intersects_convex_shape(&planes[i & 1023], 6, &v3[i & 1023], 8); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += aabbs[i].intersects_convex_shape(&planes[i & 1023], 6, &v3[i & 1023], 8); } sink_int = c; });
	p_table.row("AABB::inside_convex_shape (6 planes)", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefAABB(aabbs[i]).ref_inside_convex_shape(&planes[i & 1023], 6); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += aabbs[i].inside_convex_shape(&planes[i & 1023], 6); } sink_int = c; });
	p_table.row("AABB::intersects_plane", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefAABB(aabbs[i]).ref_intersects_plane(planes[i]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += aabbs[i].intersects_plane(planes[i]); } sink_int = c; });
	p_table.row("AABB::smits_intersect_ray", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefAABB(aabbs[i]).ref_smits_intersect_ray(v3[i], v3[(i + 1) & M], 0, 10); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += aabbs[i].smits_intersect_ray(v3[i], v3[(i + 1) & M], 0, 10); } sink_int = c; });
	p_table.row("Transform3D::xform_inv(AABB)", N, [&]() { for (int i = 0; i < N; i++) { out_aabb[i] = RefTransform3D(xforms[i]).ref_xform_inv_aabb(aabbs[i]); } }, [&]() { for (int i = 0; i < N; i++) { out_aabb[i] = xforms[i].xform_inv(aabbs[i]); } });
	p_table.row("Basis::get_scale_abs", N, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = RefBasis(bases[i]).ref_get_scale_abs(); } }, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = bases[i].get_scale_abs(); } });
	p_table.row("Vector4::sign", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_sign(); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].sign(); } });
	p_table.row("Vector4::floor", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_floor(); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].floor(); } });
	p_table.row("Vector4::round", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_round(); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].round(); } });
	p_table.row("Vector4::clamp", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_clamp(v4[(i + 1) & M], v4[(i + 2) & M]); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].clamp(v4[(i + 1) & M], v4[(i + 2) & M]); } });
	p_table.row("Vector4::is_equal_approx", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefVector4(v4[i]).ref_is_equal_approx(v4[(i + 1) & M]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += v4[i].is_equal_approx(v4[(i + 1) & M]); } sink_int = c; });
	p_table.row("Vector4::cubic_interpolate", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_cubic_interpolate(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].cubic_interpolate(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f); } });
	p_table.row("Vector4::cubic_interpolate_in_time", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefVector4(v4[i]).ref_cubic_interpolate_in_time(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f, 1.0f, -1.0f, 2.0f); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = v4[i].cubic_interpolate_in_time(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f, 1.0f, -1.0f, 2.0f); } });
	p_table.row("Vector3::cubic_interpolate", N, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = RefVector3(v3[i]).ref_cubic_interpolate(v3[(i + 1) & M], v3[(i + 2) & M], v3[(i + 3) & M], 0.3f); } }, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = v3[i].cubic_interpolate(v3[(i + 1) & M], v3[(i + 2) & M], v3[(i + 3) & M], 0.3f); } });
	p_table.row("Vector3::cubic_interpolate_in_time", N, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = RefVector3(v3[i]).ref_cubic_interpolate_in_time(v3[(i + 1) & M], v3[(i + 2) & M], v3[(i + 3) & M], 0.3f, 1.0f, -1.0f, 2.0f); } }, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = v3[i].cubic_interpolate_in_time(v3[(i + 1) & M], v3[(i + 2) & M], v3[(i + 3) & M], 0.3f, 1.0f, -1.0f, 2.0f); } });
	p_table.row("Vector2::cubic_interpolate", N, [&]() { for (int i = 0; i < N; i++) { out_v2[i] = RefVector2(v2[i]).ref_cubic_interpolate(v2[(i + 1) & M], v2[(i + 2) & M], v2[(i + 3) & M], 0.3f); } }, [&]() { for (int i = 0; i < N; i++) { out_v2[i] = v2[i].cubic_interpolate(v2[(i + 1) & M], v2[(i + 2) & M], v2[(i + 3) & M], 0.3f); } });
	p_table.row("Quaternion::xform", N, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = RefQuaternion(quats[i]).ref_xform(v3[i]); } }, [&]() { for (int i = 0; i < N; i++) { out_v3[i] = quats[i].xform(v3[i]); } });
	p_table.row("Plane::intersect_3", N, [&]() { for (int i = 0; i < N; i++) { RefPlane(planes[i]).ref_intersect_3(planes[(i + 1) & M], planes[(i + 2) & M], &out_v3[i]); } }, [&]() { for (int i = 0; i < N; i++) { planes[i].intersect_3(planes[(i + 1) & M], planes[(i + 2) & M], &out_v3[i]); } });
	p_table.row("Projection::xform_inv(Vector4)", N, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = RefProjection(projections[i]).ref_xform_inv(v4[i]); } }, [&]() { for (int i = 0; i < N; i++) { out_v4[i] = projections[i].xform_inv(v4[i]); } });
	p_table.row("Rect2::intersects", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefRect2(rects[i]).ref_intersects(rects[(i + 1) & M]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += rects[i].intersects(rects[(i + 1) & M]); } sink_int = c; });
	p_table.row("Transform2D::xform(Rect2)", N, [&]() { for (int i = 0; i < N; i++) { out_rect[i] = RefTransform2D(xforms2d[i]).ref_xform_rect(rects[i]); } }, [&]() { for (int i = 0; i < N; i++) { out_rect[i] = xforms2d[i].xform(rects[i]); } });
	p_table.row("Color::to_rgba32", N, [&]() { uint64_t c = 0; for (int i = 0; i < N; i++) { c += RefColor(colors[i]).ref_to_rgba32(); } sink_int = c; }, [&]() { uint64_t c = 0; for (int i = 0; i < N; i++) { c += colors[i].to_rgba32(); } sink_int = c; });
	p_table.row("Color::to_rgba64", N, [&]() { uint64_t c = 0; for (int i = 0; i < N; i++) { c += RefColor(colors[i]).ref_to_rgba64(); } sink_int = c; }, [&]() { uint64_t c = 0; for (int i = 0; i < N; i++) { c += colors[i].to_rgba64(); } sink_int = c; });
	p_table.row("Color::operator==", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefColor(colors[i]).ref_equals(colors[(i + 1) & M]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += colors[i] == colors[(i + 1) & M]; } sink_int = c; });
	p_table.row("Color::clamp", N, [&]() { for (int i = 0; i < N; i++) { out_color[i] = RefColor(colors[i]).ref_clamp(); } }, [&]() { for (int i = 0; i < N; i++) { out_color[i] = colors[i].clamp(); } });
	p_table.row("Color::darkened", N, [&]() { for (int i = 0; i < N; i++) { out_color[i] = RefColor(colors[i]).ref_darkened(0.3f); } }, [&]() { for (int i = 0; i < N; i++) { out_color[i] = colors[i].darkened(0.3f); } });
	p_table.row("Color::lightened", N, [&]() { for (int i = 0; i < N; i++) { out_color[i] = RefColor(colors[i]).ref_lightened(0.3f); } }, [&]() { for (int i = 0; i < N; i++) { out_color[i] = colors[i].lightened(0.3f); } });

	p_table.header("Math (latency, dependent chain)");
	p_table.row("Vector4::cubic_interpolate", N, [&]() { Vector4 acc = v4[0]; for (int i = 0; i < N; i++) { acc = RefVector4(acc).ref_cubic_interpolate(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f); } sink_float = acc.x; }, [&]() { Vector4 acc = v4[0]; for (int i = 0; i < N; i++) { acc = acc.cubic_interpolate(v4[(i + 1) & M], v4[(i + 2) & M], v4[(i + 3) & M], 0.3f); } sink_float = acc.x; });

	// Culling and BVH bounds.
	LocalVector<BVH_ABB<AABB, Vector3>> abbs;
	LocalVector<BVH_ABB<Rect2, Vector2>> abbs2d;
	LocalVector<RendererSceneCull::InstanceBounds> bounds;
	for (int i = 0; i < N; i++) {
		BVH_ABB<AABB, Vector3> a;
		a.from(aabbs[i]);
		abbs.push_back(a);
		BVH_ABB<Rect2, Vector2> b;
		b.from(rects[i]);
		abbs2d.push_back(b);
		bounds.push_back(RendererSceneCull::InstanceBounds(AABB(aabbs[i].position * 20.0f, aabbs[i].size)));
	}
	Vector<Plane> frustum_planes = Projection::create_perspective(70, 1.6, 0.05, 100).get_projection_planes(Transform3D());
	const RendererSceneCull::Frustum frustum(frustum_planes);
	p_table.header("Culling and BVH (throughput, per call)");
	p_table.row("InstanceBounds::in_frustum (6 planes)", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefInstanceBounds(bounds[i]).ref_in_frustum(frustum); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += bounds[i].in_frustum(frustum); } sink_int = c; });
	p_table.row("BVH_ABB<AABB>::intersects", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefBVH_ABB<AABB, Vector3>(abbs[i]).ref_intersects(abbs[(i + 1) & M]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += abbs[i].intersects(abbs[(i + 1) & M]); } sink_int = c; });
	p_table.row("BVH_ABB<Rect2>::intersects", N, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += RefBVH_ABB<Rect2, Vector2>(abbs2d[i]).ref_intersects(abbs2d[(i + 1) & M]); } sink_int = c; }, [&]() { int c = 0; for (int i = 0; i < N; i++) { c += abbs2d[i].intersects(abbs2d[(i + 1) & M]); } sink_int = c; });

	// DynamicBVH queries against the original class (identical trees).
	{
		DynamicBVH bvh;
		RefDynamicBVH ref_bvh;
		for (int i = 0; i < 30000; i++) {
			const AABB box(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()) * 150.0f, Vector3(1, 1, 1) + Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()).abs());
			bvh.insert(box, (void *)intptr_t(i));
			ref_bvh.insert(box, (void *)intptr_t(i));
		}
		const Vector<Plane> query_planes = Projection::create_perspective(70, 1.6, 0.05, 100).get_projection_planes(Transform3D());
		const Vector<Vector3> query_points = Geometry3D::compute_convex_mesh_points(query_planes.ptr(), query_planes.size());
		const AABB query_box(Vector3(-20, -20, -20), Vector3(40, 40, 40));
		struct Count {
			int hits = 0;
			bool operator()(void *) {
				hits++;
				return false;
			}
		};
		p_table.header("DynamicBVH, 30000 leaves (per query)");
		p_table.row("convex_query (camera frustum)", 1, [&]() { Count c; ref_bvh.convex_query(query_planes.ptr(), query_planes.size(), query_points.ptr(), query_points.size(), c); sink_int = c.hits; }, [&]() { Count c; bvh.convex_query(query_planes.ptr(), query_planes.size(), query_points.ptr(), query_points.size(), c); sink_int = c.hits; });
		p_table.row("aabb_query", 1, [&]() { Count c; ref_bvh.aabb_query(query_box, c); sink_int = c.hits; }, [&]() { Count c; bvh.aabb_query(query_box, c); sink_int = c.hits; });
	}

	// Physics vertex scans.
	LocalVector<Vector3> mesh;
	for (int i = 0; i < 1024; i++) {
		mesh.push_back(Vector3(p_rng.unit(), p_rng.unit(), p_rng.unit()));
	}
	const Vector3 dir = Vector3(0.3, 0.5, 0.8);
	p_table.header("Physics vertex scans (per vertex)");
	p_table.row("project_range, 1024 vertices", 1024, [&]() { real_t mn = 0; real_t mx = 0; TestSIMD::reference_project_range(mesh.ptr(), 1024, dir, xforms[0], mn, mx); sink_float = mn + mx; }, [&]() { real_t mn = 0; real_t mx = 0; SIMDGeometry::project_range(mesh.ptr(), 1024, dir, xforms[0], mn, mx); sink_float = mn + mx; });
	p_table.row("project_range, 24 vertices", 24, [&]() { real_t mn = 0; real_t mx = 0; TestSIMD::reference_project_range(mesh.ptr(), 24, dir, xforms[0], mn, mx); sink_float = mn + mx; }, [&]() { real_t mn = 0; real_t mx = 0; SIMDGeometry::project_range(mesh.ptr(), 24, dir, xforms[0], mn, mx); sink_float = mn + mx; });
	p_table.row("support_index, 1024 vertices", 1024, [&]() { sink_int = TestSIMD::reference_support_index(mesh.ptr(), 1024, dir); }, [&]() { sink_int = SIMDGeometry::support_index(mesh.ptr(), 1024, dir); });

	Vector<float> fa;
	for (int i = 0; i < 65536; i++) {
		fa.push_back(p_rng.unit());
	}
	Vector<float> fb = fa;
	fb.write[65535] = 2.0f;
	p_table.header("Containers (per element)");
	p_table.row("Vector<float>::operator==, 65536", 65536, [&]() { sink_int = TestSIMD::reference_vector_equals(fa, fb); }, [&]() { sink_int = fa == fb; });
}

static void run_strings(Random &p_rng, Table &p_table) {
	String ascii;
	for (int i = 0; i < 4096; i++) {
		ascii += char32_t('a' + p_rng.range(26));
	}
	const String mixed = TestSIMD::random_string(p_rng, 4096);
	const CharString ascii_utf8 = ascii.utf8();
	const CharString mixed_utf8 = mixed.utf8();
	const RefString ref_ascii(ascii);
	const RefString ref_mixed(mixed);

	p_table.header("Strings (per character)");
	p_table.row("String::hash, ASCII", 4096, [&]() { sink_int = ref_ascii.ref_hash(); }, [&]() { sink_int = ascii.hash(); });
	p_table.row("String::to_upper, ASCII", 4096, [&]() { sink_int = ref_ascii.ref_to_upper().length(); }, [&]() { sink_int = ascii.to_upper().length(); });
	p_table.row("String::to_upper, mixed scripts", 4096, [&]() { sink_int = ref_mixed.ref_to_upper().length(); }, [&]() { sink_int = mixed.to_upper().length(); });
	p_table.row("String::utf8, ASCII", 4096, [&]() { sink_int = ref_ascii.ref_utf8(nullptr).length(); }, [&]() { sink_int = ascii.utf8().length(); });
	p_table.row("String::utf8, mixed scripts", 4096, [&]() { sink_int = ref_mixed.ref_utf8(nullptr).length(); }, [&]() { sink_int = mixed.utf8().length(); });
	p_table.row("String::append_utf8, ASCII", 4096, [&]() { RefString s; s.ref_append_utf8(ascii_utf8.get_data(), ascii_utf8.length(), false); sink_int = s.length(); }, [&]() { String s; s.append_utf8(ascii_utf8.get_data(), ascii_utf8.length()); sink_int = s.length(); });
	p_table.row("String::append_utf8, mixed scripts", mixed_utf8.length(), [&]() { RefString s; s.ref_append_utf8(mixed_utf8.get_data(), mixed_utf8.length(), false); sink_int = s.length(); }, [&]() { String s; s.append_utf8(mixed_utf8.get_data(), mixed_utf8.length()); sink_int = s.length(); });
	p_table.row("String::operator==(const char *)", 4096, [&]() { sink_int = ref_ascii.ref_equals(ascii_utf8.get_data()); }, [&]() { sink_int = ascii == ascii_utf8.get_data(); });
}

static void run_images(Random &p_rng, Table &p_table) {
	const int size = 1024;
	const int64_t pixels = int64_t(size) * size;
	const Ref<Image> rgba = TestSIMD::random_image(p_rng, size, size, Image::FORMAT_RGBA8);
	const Ref<Image> rgb = TestSIMD::random_image(p_rng, size, size, Image::FORMAT_RGB8);
	const Ref<Image> rgbaf = TestSIMD::random_image(p_rng, size, size, Image::FORMAT_RGBAF);
	const Ref<Image> rgbah = TestSIMD::random_image(p_rng, size, size, Image::FORMAT_RGBAH);
	Vector<uint8_t> opaque_data = rgba->get_data();
	for (int64_t i = 0; i < pixels; i++) {
		opaque_data.write[i * 4 + 3] = 255;
	}
	const Ref<Image> opaque = Image::create_from_data(size, size, false, Image::FORMAT_RGBA8, opaque_data);
	Vector<uint8_t> clear_data = rgba->get_data();
	for (int64_t i = 0; i < pixels; i++) {
		clear_data.write[i * 4 + 3] = (i == pixels / 2) ? 255 : 0;
	}
	const Ref<Image> sparse = Image::create_from_data(size, size, false, Image::FORMAT_RGBA8, clear_data);

	// Conversion and resize read shared source data and initialize a new destination, like the
	// public methods. Mipmap generation resizes the shared data before detaching it for writing.
	auto reference_convert = [&](const Ref<Image> &p_image, Image::Format p_to) {
		const Vector<uint8_t> src = p_image->get_data();
		Ref<Image> destination;
		destination.instantiate();
		destination->initialize_data(size, size, false, p_to);
		Vector<uint8_t> dst = destination->get_data();
		destination.unref();
		ref_convert_dispatch(p_image->get_format(), p_to, size, size, src.ptr(), dst.ptrw());
		sink_int = dst[0];
	};
	auto reference_resize = [&](const Ref<Image> &p_image, int p_target) {
		const Vector<uint8_t> src = p_image->get_data();
		Ref<Image> destination;
		destination.instantiate();
		destination->initialize_data(p_target, p_target, false, p_image->get_format());
		Vector<uint8_t> dst = destination->get_data();
		destination.unref();
		TestSIMD::reference_bilinear(p_image->get_format(), src.ptr(), dst.ptrw(), size, size, p_target, p_target);
		sink_int = dst[0];
	};
	auto reference_mipmaps = [&](const Ref<Image> &p_image) {
		const Image::Format format = p_image->get_format();
		Vector<uint8_t> data = p_image->get_data();
		data.resize(Image::get_image_data_size(size, size, format, true));
		uint8_t *w = data.ptrw();
		int64_t prev_ofs = 0;
		int prev_w = size;
		int prev_h = size;
		const int count = Image::get_image_required_mipmaps(size, size, format);
		for (int level = 1; level <= count; level++) {
			int mip_w = 0;
			int mip_h = 0;
			const int64_t ofs = Image::get_image_mipmap_offset_and_dimensions(size, size, format, level, mip_w, mip_h);
			ref_generate_mipmap_from_format(format, w + prev_ofs, w + ofs, prev_w, prev_h, false);
			prev_ofs = ofs;
			prev_w = mip_w;
			prev_h = mip_h;
		}
		sink_int = data[0];
	};

	p_table.header("Images, 1024x1024 (per pixel)");
	p_table.row("convert RGBA8 -> RGB8", pixels, [&]() { reference_convert(rgba, Image::FORMAT_RGB8); }, [&]() { Ref<Image> c = rgba->duplicate(); c->convert(Image::FORMAT_RGB8); });
	p_table.row("convert RGB8 -> RGBA8", pixels, [&]() { reference_convert(rgb, Image::FORMAT_RGBA8); }, [&]() { Ref<Image> c = rgb->duplicate(); c->convert(Image::FORMAT_RGBA8); });
	p_table.row("convert RGBA8 -> L8", pixels, [&]() { reference_convert(rgba, Image::FORMAT_L8); }, [&]() { Ref<Image> c = rgba->duplicate(); c->convert(Image::FORMAT_L8); });
	p_table.row("resize bilinear RGBA8 -> 700x700", 700 * 700, [&]() { reference_resize(rgba, 700); }, [&]() { Ref<Image> c = rgba->duplicate(); c->resize(700, 700, Image::INTERPOLATE_BILINEAR); });
	p_table.row("resize bilinear RGB8 -> 700x700", 700 * 700, [&]() { reference_resize(rgb, 700); }, [&]() { Ref<Image> c = rgb->duplicate(); c->resize(700, 700, Image::INTERPOLATE_BILINEAR); });
	p_table.row("resize bilinear RGBAF -> 700x700", 700 * 700, [&]() { reference_resize(rgbaf, 700); }, [&]() { Ref<Image> c = rgbaf->duplicate(); c->resize(700, 700, Image::INTERPOLATE_BILINEAR); });
	p_table.row("resize bilinear RGBAH -> 700x700", 700 * 700, [&]() { reference_resize(rgbah, 700); }, [&]() { Ref<Image> c = rgbah->duplicate(); c->resize(700, 700, Image::INTERPOLATE_BILINEAR); });
	p_table.row("generate_mipmaps RGBA8", pixels, [&]() { reference_mipmaps(rgba); }, [&]() { Ref<Image> c = rgba->duplicate(); c->generate_mipmaps(); });
	p_table.row("generate_mipmaps RGB8", pixels, [&]() { reference_mipmaps(rgb); }, [&]() { Ref<Image> c = rgb->duplicate(); c->generate_mipmaps(); });
	p_table.row("generate_mipmaps RGBAF", pixels, [&]() { reference_mipmaps(rgbaf); }, [&]() { Ref<Image> c = rgbaf->duplicate(); c->generate_mipmaps(); });
	p_table.row("generate_mipmaps RGBAH", pixels, [&]() { reference_mipmaps(rgbah); }, [&]() { Ref<Image> c = rgbah->duplicate(); c->generate_mipmaps(); });
	p_table.row("premultiply_alpha", pixels, [&]() { sink_int = TestSIMD::reference_premultiply_alpha(rgba).size(); }, [&]() { Ref<Image> c = rgba->duplicate(); c->premultiply_alpha(); });
	p_table.row("flip_x", pixels, [&]() { sink_int = TestSIMD::reference_flip(rgba, true).size(); }, [&]() { Ref<Image> c = rgba->duplicate(); c->flip_x(); });
	p_table.row("flip_y", pixels, [&]() { sink_int = TestSIMD::reference_flip(rgba, false).size(); }, [&]() { Ref<Image> c = rgba->duplicate(); c->flip_y(); });
	p_table.row("detect_alpha (fully opaque)", pixels, [&]() { sink_int = TestSIMD::reference_detect_alpha(opaque); }, [&]() { sink_int = opaque->detect_alpha(); });
	p_table.row("is_invisible (one visible pixel)", pixels, [&]() { sink_int = TestSIMD::reference_is_invisible(sparse); }, [&]() { sink_int = sparse->is_invisible(); });
	p_table.row("get_used_rect (one visible pixel)", pixels, [&]() { sink_int = TestSIMD::reference_get_used_rect(sparse).size.x; }, [&]() { sink_int = sparse->get_used_rect().size.x; });
}

static void run_audio(Random &p_rng, Table &p_table) {
	const int frames = 1024;
	LocalVector<AudioFrame> src;
	LocalVector<AudioFrame> out;
	LocalVector<int32_t> pcm;
	src.resize(frames);
	out.resize(frames);
	pcm.resize(frames * 2);
	for (int i = 0; i < frames; i++) {
		src[i] = AudioFrame(p_rng.unit(), p_rng.unit());
		out[i] = AudioFrame(0, 0);
	}
	AudioFrame peak;

	p_table.header("Audio, 1024-frame buffers (per frame)");
	p_table.row("output conversion to int32", frames, [&]() { TestSIMD::reference_convert_stereo_to_int32(src.ptr(), pcm.ptr(), frames); }, [&]() { AudioMixKernels::convert_stereo_to_int32(src.ptr(), pcm.ptr(), frames); });
	p_table.row("bus volume and peak", frames, [&]() { TestSIMD::reference_apply_volume_and_peak(out.ptr(), frames, 1.0f, peak); }, [&]() { AudioMixKernels::apply_volume_and_peak(out.ptr(), frames, 1.0f, peak); });
	p_table.row("bus send (add)", frames, [&]() { for (int i = 0; i < frames; i++) { out[i] += src[i]; } }, [&]() { AudioMixKernels::add_frames(out.ptr(), src.ptr(), frames); });
	p_table.row("playback volume ramp", frames, [&]() { TestSIMD::reference_mix_ramped(out.ptr(), src.ptr(), AudioFrame(0.5f, 0.5f), AudioFrame(0.6f, 0.4f), frames); }, [&]() { AudioMixKernels::mix_ramped(out.ptr(), src.ptr(), AudioFrame(0.5f, 0.5f), AudioFrame(0.6f, 0.4f), frames); });

	static const EQ::Preset presets[] = { EQ::PRESET_6_BANDS, EQ::PRESET_10_BANDS, EQ::PRESET_21_BANDS, EQ::PRESET_31_BANDS };
	for (EQ::Preset preset : presets) {
		EQ eq;
		eq.set_mix_rate(44100);
		eq.set_preset_band_mode(preset);
		const int band_count = eq.get_band_count();
		LocalVector<EQ::BandProcess> procs_l;
		LocalVector<EQ::BandProcess> procs_r;
		LocalVector<float> gains;
		for (int k = 0; k < band_count; k++) {
			procs_l.push_back(eq.get_band_processor(k));
			procs_r.push_back(eq.get_band_processor(k));
			gains.push_back(1.0f);
		}
		p_table.row(vformat("EQ, %d bands", band_count), frames, [&]() { TestSIMD::reference_eq(src.ptr(), out.ptr(), frames, procs_l.ptr(), procs_r.ptr(), gains.ptr(), band_count); }, [&]() { AudioMixKernels::EQProcess::process(src.ptr(), out.ptr(), frames, procs_l.ptr(), procs_r.ptr(), gains.ptr(), band_count); });
	}

	Ref<AudioEffectStereoEnhance> enhance;
	enhance.instantiate();
	enhance->set_time_pullout(10);
	enhance->set_surround(0.5f);
	Ref<AudioEffectInstance> enhance_instance = enhance->instantiate();
	RefStereoEnhance ref_enhance;
	ref_enhance.params.time_pullout = 10;
	ref_enhance.params.surround = 0.5f;
	LocalVector<float> ring;
	ring.resize(4096);
	for (float &v : ring) {
		v = 0;
	}
	ref_enhance.delay_ringbuff = ring.ptr();
	ref_enhance.ringbuff_mask = 4095;
	p_table.row("stereo enhance effect", frames, [&]() { ref_enhance.process(src.ptr(), out.ptr(), frames); }, [&]() { enhance_instance->process(src.ptr(), out.ptr(), frames); });
}

TEST_CASE("[Audio][SIMD][Benchmark] SIMD paths versus the original scalar code" * doctest::skip()) {
#ifdef DEV_ENABLED
	print_line("WARNING: this is a dev build (no optimization). Rebuild without dev_build for meaningful numbers.");
#endif
	const uint32_t features = CPUFeatures::get();
	print_line(vformat("CPU features: SSE4.2 %s, AVX %s, AVX2 %s, FMA %s, F16C %s",
			(features & CPUFeatures::SSE42) ? "yes" : "no", (features & CPUFeatures::AVX) ? "yes" : "no",
			(features & CPUFeatures::AVX2) ? "yes" : "no", (features & CPUFeatures::FMA) ? "yes" : "no",
			(features & CPUFeatures::F16C) ? "yes" : "no"));
	print_line("Lower is better for ns columns. speedup = scalar / simd.");

	for (const TestSIMD::DispatchMode &mode : TestSIMD::DISPATCH_MODES) {
		if (mode.mask == (CPUFeatures::SSE42 | CPUFeatures::AVX)) {
			continue;
		}
		CPUFeatures::set_override(mode.mask);
		print_line("");
		print_line(vformat("==== Dispatch level: %s ====", mode.name));
		Random rng(99);
		Table table;
		run_math(rng, table);
		run_strings(rng, table);
		run_images(rng, table);
		run_audio(rng, table);
	}
	CPUFeatures::reset_override();
	CHECK(true);
}

} // namespace TestSIMDBenchmark
