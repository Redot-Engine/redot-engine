/**************************************************************************/
/*  xpbd_cloth_solver.cpp                                                 */
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

#include "xpbd_cloth_solver.h"

#include "core/templates/hash_map.h"

void XPBDClothSolver::clear() {
	rest_positions.clear();
	positions.clear();
	prev.clear();
	velocities.clear();
	inv_mass.clear();
	base_inv_mass.clear();
	pinned.clear();
	pin_target.clear();
	constraints.clear();
	indices.clear();
	cols = 0;
	rows = 0;
	rest_volume = 0.0f;
	applied_rest_scale = 1.0f;
}

void XPBDClothSolver::_add_constraint(int p_a, int p_b, float p_compliance) {
	DistanceConstraint c;
	c.a = p_a;
	c.b = p_b;
	c.rest = (rest_positions[p_a] - rest_positions[p_b]).length();
	c.compliance = p_compliance;
	constraints.push_back(c);
}

void XPBDClothSolver::build_grid(int p_cols, int p_rows, const Vector2 &p_size) {
	clear();
	cols = MAX(p_cols, 2);
	rows = MAX(p_rows, 2);
	const int n = cols * rows;
	rest_positions.resize(n);

	const float dx = p_size.x / (float)(cols - 1);
	const float dy = p_size.y / (float)(rows - 1);
	thickness = 0.5f * MIN(dx, dy);
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			rest_positions[y * cols + x] = Vector3(-0.5f * p_size.x + x * dx, 0.5f * p_size.y - y * dy, 0.0f);
		}
	}

	// Triangle mesh (two per quad).
	for (int y = 0; y < rows - 1; y++) {
		for (int x = 0; x < cols - 1; x++) {
			const int a = y * cols + x;
			const int b = a + 1;
			const int c = a + cols;
			const int d = c + 1;
			indices.push_back(a);
			indices.push_back(c);
			indices.push_back(b);
			indices.push_back(b);
			indices.push_back(c);
			indices.push_back(d);
		}
	}

	const float stretch = settings.stretch_compliance;
	const float shear = settings.shear_compliance;
	const float bend = settings.bend_compliance;
	for (int y = 0; y < rows; y++) {
		for (int x = 0; x < cols; x++) {
			const int i = y * cols + x;
			if (x + 1 < cols) {
				_add_constraint(i, i + 1, stretch); // structural, horizontal
			}
			if (y + 1 < rows) {
				_add_constraint(i, i + cols, stretch); // structural, vertical
			}
			if (x + 1 < cols && y + 1 < rows) {
				_add_constraint(i, i + cols + 1, shear); // diagonal
				_add_constraint(i + 1, i + cols, shear); // anti-diagonal
			}
			if (x + 2 < cols) {
				_add_constraint(i, i + 2, bend); // bending, horizontal
			}
			if (y + 2 < rows) {
				_add_constraint(i, i + 2 * cols, bend); // bending, vertical
			}
		}
	}

	_finalize();
}

void XPBDClothSolver::build_mesh(const PackedVector3Array &p_positions, const PackedInt32Array &p_indices) {
	clear();
	const int n = p_positions.size();
	if (n < 3 || p_indices.size() < 3) {
		return;
	}
	rest_positions.resize(n);
	for (int i = 0; i < n; i++) {
		rest_positions[i] = p_positions[i];
	}
	indices.resize(p_indices.size());
	float min_edge = 1.0e20f;
	for (int i = 0; i < (int)p_indices.size(); i++) {
		indices[i] = p_indices[i];
	}

	// Unique edges -> structural constraints; edge -> the two opposite vertices
	// of its adjacent triangles -> bending constraints.
	HashMap<uint64_t, int> edge_other; // packed (min,max) -> opposite vertex of the first tri seen
	auto key = [](int a, int b) -> uint64_t {
		const uint32_t lo = MIN(a, b);
		const uint32_t hi = MAX(a, b);
		return ((uint64_t)hi << 32) | lo;
	};
	const float stretch = settings.stretch_compliance;
	const float bend = settings.bend_compliance;
	HashMap<uint64_t, bool> have_structural;
	for (int t = 0; t + 2 < (int)indices.size(); t += 3) {
		const int v[3] = { indices[t], indices[t + 1], indices[t + 2] };
		for (int e = 0; e < 3; e++) {
			const int a = v[e];
			const int b = v[(e + 1) % 3];
			const int opp = v[(e + 2) % 3];
			const uint64_t k = key(a, b);
			min_edge = MIN(min_edge, (float)(rest_positions[a] - rest_positions[b]).length());
			if (!have_structural.has(k)) {
				have_structural[k] = true;
				_add_constraint(a, b, stretch);
			}
			HashMap<uint64_t, int>::Iterator it = edge_other.find(k);
			if (it == edge_other.end()) {
				edge_other[k] = opp;
			} else if (it->value != opp) {
				_add_constraint(it->value, opp, bend);
			}
		}
	}
	thickness = 0.5f * (min_edge < 1.0e19f ? min_edge : 0.02f);
	_finalize();
}

void XPBDClothSolver::_finalize() {
	const int n = rest_positions.size();
	positions.resize(n);
	prev.resize(n);
	velocities.resize(n);
	inv_mass.resize(n);
	base_inv_mass.resize(n);
	pinned.resize(n);
	pin_target.resize(n);

	// Vertex mass from the surrounding triangle area * areal density.
	LocalVector<float> mass;
	mass.resize(n);
	for (int i = 0; i < n; i++) {
		mass[i] = 0.0f;
		velocities[i] = Vector3();
		pinned[i] = 0;
		pin_target[i] = Vector3(NAN, NAN, NAN);
	}
	for (int t = 0; t + 2 < (int)indices.size(); t += 3) {
		const int a = indices[t];
		const int b = indices[t + 1];
		const int c = indices[t + 2];
		const float area = 0.5f * (rest_positions[b] - rest_positions[a]).cross(rest_positions[c] - rest_positions[a]).length();
		const float m = area * settings.density / 3.0f;
		mass[a] += m;
		mass[b] += m;
		mass[c] += m;
	}
	for (int i = 0; i < n; i++) {
		const float m = MAX(mass[i], 1.0e-6f);
		base_inv_mass[i] = 1.0f / m;
		inv_mass[i] = base_inv_mass[i];
	}

	rest_volume = _mesh_volume(rest_positions);
}

void XPBDClothSolver::reset(const Transform3D &p_xform) {
	const int n = rest_positions.size();
	positions.resize(n);
	prev.resize(n);
	velocities.resize(n);
	for (int i = 0; i < n; i++) {
		positions[i] = p_xform.xform(rest_positions[i]);
		prev[i] = positions[i];
		velocities[i] = Vector3();
	}
}

void XPBDClothSolver::set_pinned(int p_vertex, bool p_pinned) {
	if (p_vertex < 0 || p_vertex >= (int)pinned.size()) {
		return;
	}
	pinned[p_vertex] = p_pinned ? 1 : 0;
	inv_mass[p_vertex] = p_pinned ? 0.0f : base_inv_mass[p_vertex];
	if (!p_pinned) {
		pin_target[p_vertex] = Vector3(NAN, NAN, NAN);
	}
}

bool XPBDClothSolver::is_pinned(int p_vertex) const {
	return p_vertex >= 0 && p_vertex < (int)pinned.size() && pinned[p_vertex] != 0;
}

void XPBDClothSolver::set_pin_target(int p_vertex, const Vector3 &p_world_pos) {
	if (p_vertex < 0 || p_vertex >= (int)pin_target.size()) {
		return;
	}
	pin_target[p_vertex] = p_world_pos;
	pinned[p_vertex] = 1;
	inv_mass[p_vertex] = 0.0f;
}

void XPBDClothSolver::clear_pin_target(int p_vertex) {
	if (p_vertex < 0 || p_vertex >= (int)pin_target.size()) {
		return;
	}
	pin_target[p_vertex] = Vector3(NAN, NAN, NAN);
}

float XPBDClothSolver::_mesh_volume(const LocalVector<Vector3> &p_pos) const {
	if (p_pos.is_empty() || indices.size() < 3) {
		return 0.0f;
	}
	Vector3 ref;
	for (uint32_t i = 0; i < p_pos.size(); i++) {
		ref += p_pos[i];
	}
	ref /= (float)p_pos.size();
	double v = 0.0;
	for (uint32_t t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 a = p_pos[indices[t]] - ref;
		const Vector3 b = p_pos[indices[t + 1]] - ref;
		const Vector3 c = p_pos[indices[t + 2]] - ref;
		v += (double)a.dot(b.cross(c));
	}
	return (float)(v / 6.0);
}

float XPBDClothSolver::rest_surface_area() const {
	double area = 0.0;
	for (uint32_t t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 &a = rest_positions[indices[t]];
		const Vector3 &b = rest_positions[indices[t + 1]];
		const Vector3 &c = rest_positions[indices[t + 2]];
		area += 0.5 * (double)(b - a).cross(c - a).length();
	}
	return (float)area;
}

void XPBDClothSolver::set_rest_length_scale(float p_scale) {
	if (p_scale <= 0.0f || Math::is_equal_approx(p_scale, applied_rest_scale)) {
		return;
	}
	const float rel = p_scale / applied_rest_scale;
	for (uint32_t i = 0; i < constraints.size(); i++) {
		constraints[i].rest *= rel;
	}
	applied_rest_scale = p_scale;

	// The volume constraint's target must shrink with the edges, or the
	// pressure term fights the shrink: _mesh_volume() measures the ORIGINAL
	// rest positions (unchanged by the scale), so scale the cached value by
	// p_scale^3 instead of recomputing it.
	rest_volume *= p_scale * p_scale * p_scale;
}

void XPBDClothSolver::_solve_volume(float p_sdt) {
	if (settings.pressure <= 0.0f || Math::is_zero_approx(rest_volume) || indices.size() < 3) {
		return;
	}
	const int n = positions.size();

	Vector3 ref;
	for (int i = 0; i < n; i++) {
		ref += positions[i];
	}
	ref /= (float)MAX(n, 1);

	LocalVector<Vector3> grad;
	grad.resize(n);
	for (int i = 0; i < n; i++) {
		grad[i] = Vector3();
	}
	double vol = 0.0;
	for (uint32_t t = 0; t + 2 < indices.size(); t += 3) {
		const int ia = indices[t];
		const int ib = indices[t + 1];
		const int ic = indices[t + 2];
		const Vector3 a = positions[ia] - ref;
		const Vector3 b = positions[ib] - ref;
		const Vector3 c = positions[ic] - ref;
		vol += (double)a.dot(b.cross(c));
		grad[ia] += b.cross(c) * (1.0f / 6.0f);
		grad[ib] += c.cross(a) * (1.0f / 6.0f);
		grad[ic] += a.cross(b) * (1.0f / 6.0f);
	}
	const float constraint = (float)(vol / 6.0) - settings.pressure * rest_volume;

	float denom = 0.0f;
	for (int i = 0; i < n; i++) {
		denom += inv_mass[i] * grad[i].length_squared();
	}
	if (denom < 1.0e-12f) {
		return;
	}
	// pressure_stiffness in (0,1] -> XPBD compliance: 1 is near-rigid, small
	// values are soft. Scaled by rest_volume^2 so the response is shape-size
	// independent.
	const float k = CLAMP(settings.pressure_stiffness, 0.01f, 1.0f);
	const float compliance = (1.0f - k) * 1.0e-3f * MAX(rest_volume * rest_volume, 1.0e-6f);
	const float alpha = compliance / (p_sdt * p_sdt);
	const float dlambda = -constraint / (denom + alpha);
	for (int i = 0; i < n; i++) {
		if (inv_mass[i] == 0.0f) {
			continue;
		}
		positions[i] += grad[i] * (dlambda * inv_mass[i]);
	}
}

void XPBDClothSolver::_apply_aero(float p_dt, const Vector3 &p_wind) {
	if (settings.drag <= 0.0f && settings.lift <= 0.0f) {
		return;
	}
	for (int t = 0; t + 2 < (int)indices.size(); t += 3) {
		const int a = indices[t];
		const int b = indices[t + 1];
		const int c = indices[t + 2];
		const Vector3 e1 = positions[b] - positions[a];
		const Vector3 e2 = positions[c] - positions[a];
		Vector3 cr = e1.cross(e2);
		const float twice_area = cr.length();
		if (twice_area < 1.0e-9f) {
			continue;
		}
		const Vector3 n = cr / twice_area;
		const float area = 0.5f * twice_area;
		const Vector3 v_tri = (velocities[a] + velocities[b] + velocities[c]) / 3.0f;
		const Vector3 rel = p_wind - v_tri;
		const float vn = n.dot(rel);
		const Vector3 tangential = rel - n * vn;
		// Drag pushes along the normal in proportion to how square-on the flow is;
		// lift nudges along the in-plane flow so slack cloth ripples.
		const Vector3 force = n * (settings.drag * area * vn * Math::abs(vn)) + tangential * (settings.lift * area * Math::abs(vn));
		const Vector3 impulse = force * (p_dt / 3.0f);
		velocities[a] += impulse * inv_mass[a];
		velocities[b] += impulse * inv_mass[b];
		velocities[c] += impulse * inv_mass[c];
	}
}

void XPBDClothSolver::step(float p_dt, const Vector3 &p_gravity, const Vector3 &p_wind) {
	const int n = positions.size();
	if (n == 0 || p_dt <= 0.0f) {
		return;
	}
	const int substeps = MAX(settings.substeps, 1);
	const float sdt = p_dt / (float)substeps;
	// Per-SUBSTEP retain: the velocity is re-derived from positions each
	// substep, so a frame-scaled factor here would compound (removing
	// substeps x too much). Damping is documented as a fraction of velocity
	// removed per second; distributing the frame's removal evenly across the
	// substeps gives exactly that.
	const float sub_retain = MAX(1.0f - settings.damping * p_dt / substeps, 0.0f);

	_apply_aero(p_dt, p_wind);

	for (int s = 0; s < substeps; s++) {
		// Integrate.
		for (int i = 0; i < n; i++) {
			prev[i] = positions[i];
			if (inv_mass[i] == 0.0f) {
				const Vector3 &target = pin_target[i];
				if (!Math::is_nan(target.x)) {
					positions[i] = target; // snap the pin to its anchor
				}
				continue;
			}
			velocities[i] += p_gravity * sdt;
			positions[i] += velocities[i] * sdt;
		}

		// One Gauss-Seidel pass over the distance constraints (small-steps XPBD:
		// lambda resets each substep, so its accumulation term drops out).
		for (uint32_t ci = 0; ci < constraints.size(); ci++) {
			const DistanceConstraint &c = constraints[ci];
			const float wa = inv_mass[c.a];
			const float wb = inv_mass[c.b];
			const float w = wa + wb;
			if (w == 0.0f) {
				continue;
			}
			Vector3 d = positions[c.a] - positions[c.b];
			const float len = d.length();
			if (len < 1.0e-9f) {
				continue;
			}
			d /= len;
			const float constraint = len - c.rest;
			const float alpha = c.compliance / (sdt * sdt);
			const float dlambda = -constraint / (w + alpha);
			positions[c.a] += d * (dlambda * wa);
			positions[c.b] -= d * (dlambda * wb);
		}

		// Optional closed-mesh volume/pressure constraint (soft bodies).
		_solve_volume(sdt);

		// Update velocities from the constrained motion.
		const float inv_sdt = 1.0f / sdt;
		const float max_speed = settings.max_speed;
		for (int i = 0; i < n; i++) {
			if (inv_mass[i] == 0.0f) {
				velocities[i] = Vector3();
				continue;
			}
			Vector3 v = (positions[i] - prev[i]) * inv_sdt * sub_retain;
			if (max_speed > 0.0f) {
				const float sp = v.length();
				if (sp > max_speed) {
					v *= max_speed / sp;
				}
			}
			velocities[i] = v;
		}
	}
}
