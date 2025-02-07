/**************************************************************************/
/*  triangulate.cpp                                                       */
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

#include "triangulate.h"

real_t Triangulate::get_area(const Vector<Vector2> &contour) {
	const int n = contour.size();
	const Vector2 *c = contour.ptr();

	real_t A = 0.0f;

	for (int p = n - 1, q = 0; q < n; p = q++) {
		A += c[p].cross(c[q]);
	}
	return A * 0.5f;
}

bool Triangulate::is_inside_triangle(real_t Ax, real_t Ay,
									 real_t Bx, real_t By,
									 real_t Cx, real_t Cy,
									 real_t Px, real_t Py,
									 bool include_edges) {
	const real_t ax = Cx - Bx;
	const real_t ay = Cy - By;
	const real_t bx = Ax - Cx;
	const real_t by = Ay - Cy;
	const real_t cx = Bx - Ax;
	const real_t cy = By - Ay;

	const real_t apx = Px - Ax;
	const real_t apy = Py - Ay;
	const real_t bpx = Px - Bx;
	const real_t bpy = Py - By;
	const real_t cpx = Px - Cx;
	const real_t cpy = Py - Cy;

	const real_t aCROSSbp = ax * bpy - ay * bpx;
	const real_t cCROSSap = cx * apy - cy * apx;
	const real_t bCROSScp = bx * cpy - by * cpx;

	return include_edges ?
	(aCROSSbp > 0.0f && bCROSScp > 0.0f && cCROSSap > 0.0f) :
	(aCROSSbp >= 0.0f && bCROSScp >= 0.0f && cCROSSap >= 0.0f);
									 }

									 bool Triangulate::snip(const Vector<Vector2> &p_contour, int u, int v, int w, int n, const Vector<int> &V, bool relaxed) {
										 const Vector2 *contour = p_contour.ptr();

										 const real_t Ax = contour[V[u]].x;
										 const real_t Ay = contour[V[u]].y;

										 const real_t Bx = contour[V[v]].x;
										 const real_t By = contour[V[v]].y;

										 const real_t Cx = contour[V[w]].x;
										 const real_t Cy = contour[V[w]].y;

										 const float threshold = relaxed ? -CMP_EPSILON : CMP_EPSILON;

										 if (threshold > ((Bx - Ax) * (Cy - Ay) - (By - Ay) * (Cx - Ax))) {
											 return false;
										 }

										 for (int p = 0; p < n; ++p) {
											 if ((p == u) || (p == v) || (p == w)) {
												 continue;
											 }
											 const real_t Px = contour[V[p]].x;
											 const real_t Py = contour[V[p]].y;
											 if (is_inside_triangle(Ax, Ay, Bx, By, Cx, Cy, Px, Py, relaxed)) {
												 return false;
											 }
										 }

										 return true;
									 }

									 bool Triangulate::triangulate(const Vector<Vector2> &contour, Vector<int> &result) {
										 const int n = contour.size();
										 if (n < 3) {
											 return false;
										 }

										 Vector<int> V;
										 V.resize(n);

										 if (0.0f < get_area(contour)) {
											 for (int v = 0; v < n; ++v) {
												 V.write[v] = v;
											 }
										 } else {
											 for (int v = 0; v < n; ++v) {
												 V.write[v] = (n - 1) - v;
											 }
										 }

										 bool relaxed = false;
										 int nv = n;

										 int count = 2 * nv;

										 for (int v = nv - 1; nv > 2;) {
											 if (0 >= (count--)) {
												 if (relaxed) {
													 return false;
												 } else {
													 count = 2 * nv;
													 relaxed = true;
												 }
											 }

											 int u = v;
											 if (nv <= u) {
												 u = 0;
											 }
											 v = u + 1;
											 if (nv <= v) {
												 v = 0;
											 }
											 int w = v + 1;
											 if (nv <= w) {
												 w = 0;
											 }

											 if (snip(contour, u, v, w, nv, V, relaxed)) {
												 const int a = V[u];
												 const int b = V[v];
												 const int c = V[w];

												 result.push_back(a);
												 result.push_back(b);
												 result.push_back(c);

												 int s = v;
												 for (int t = v + 1; t < nv; ++s, ++t) {
													 V.write[s] = V[t];
												 }

												 nv--;
												 count = 2 * nv;
											 }
										 }

										 return true;
									 }
