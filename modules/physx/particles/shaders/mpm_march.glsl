#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

// GPU marching tetrahedra over the smoothed density grid (surf[]). One thread per
// grid cell; each cell is split into six tetrahedra sharing the body diagonal.
// Triangles are appended non-indexed to the mesh buffers via an atomic counter;
// per-vertex normals come from the density gradient. This replaces a CPU march
// over the whole grid -- the only readback is the emitted geometry.

layout(set = 1, binding = 0, std430) restrict buffer MeshVerts {
	vec4 mverts[];
};
layout(set = 1, binding = 1, std430) restrict buffer MeshNorms {
	vec4 mnorms[];
};
layout(set = 1, binding = 2, std430) restrict buffer MeshCount {
	uint tri_count;
	uint tri_budget;
};

layout(local_size_x = GROUP) in;

#define ISO (extra.z)

const ivec3 CORNER[8] = ivec3[8](
		ivec3(0, 0, 0), ivec3(1, 0, 0), ivec3(1, 1, 0), ivec3(0, 1, 0),
		ivec3(0, 0, 1), ivec3(1, 0, 1), ivec3(1, 1, 1), ivec3(0, 1, 1));

// six tets, all sharing corner 0 and corner 6 (body diagonal)
const int TET[24] = int[24](0, 1, 2, 6, 0, 2, 3, 6, 0, 3, 7, 6, 0, 7, 4, 6, 0, 4, 5, 6, 0, 5, 1, 6);
// tet-local edges
const int EA[6] = int[6](0, 0, 0, 1, 1, 2);
const int EB[6] = int[6](1, 2, 3, 2, 3, 3);
// the three edges incident to each tet-local corner
const int INC[12] = int[12](0, 1, 2, 0, 3, 4, 1, 3, 5, 2, 4, 5);

float dsample(ivec3 c) {
	c = clamp(c, ivec3(0), RES - 1);
	return float(surf_i[node_index(c)]) / SURF_FIXED;
}

float dsample_w(vec3 wp) {
	vec3 g = (wp - ORIGIN) / DX;
	ivec3 b = ivec3(floor(g));
	vec3 f = g - vec3(b);
	float c00 = mix(dsample(b + ivec3(0, 0, 0)), dsample(b + ivec3(1, 0, 0)), f.x);
	float c10 = mix(dsample(b + ivec3(0, 1, 0)), dsample(b + ivec3(1, 1, 0)), f.x);
	float c01 = mix(dsample(b + ivec3(0, 0, 1)), dsample(b + ivec3(1, 0, 1)), f.x);
	float c11 = mix(dsample(b + ivec3(0, 1, 1)), dsample(b + ivec3(1, 1, 1)), f.x);
	return mix(mix(c00, c10, f.y), mix(c01, c11, f.y), f.z);
}

vec3 dnormal(vec3 wp) {
	float e = DX * 0.5;
	vec3 n = vec3(
			dsample_w(wp + vec3(e, 0, 0)) - dsample_w(wp - vec3(e, 0, 0)),
			dsample_w(wp + vec3(0, e, 0)) - dsample_w(wp - vec3(0, e, 0)),
			dsample_w(wp + vec3(0, 0, e)) - dsample_w(wp - vec3(0, 0, e)));
	float len = length(n);
	return len > 1e-6 ? -n / len : vec3(0.0, 1.0, 0.0); // outward = down-gradient
}

void write_tri(vec3 p0, vec3 p1, vec3 p2) {
	// drop triangles stretched well past a cell (a stray particle smear)
	float mx = DX * 3.0;
	if (distance(p0, p1) > mx || distance(p1, p2) > mx || distance(p2, p0) > mx) {
		return;
	}
	vec3 fn = cross(p1 - p0, p2 - p0);
	vec3 outn = dnormal((p0 + p1 + p2) * (1.0 / 3.0));
	if (dot(fn, outn) < 0.0) {
		vec3 t = p1;
		p1 = p2;
		p2 = t;
	}
	uint slot = atomicAdd(tri_count, 1u);
	if (slot >= tri_budget) {
		return;
	}
	uint o = slot * 3u;
	mverts[o + 0] = vec4(p0, 0.0);
	mverts[o + 1] = vec4(p1, 0.0);
	mverts[o + 2] = vec4(p2, 0.0);
	mnorms[o + 0] = vec4(dnormal(p0), 0.0);
	mnorms[o + 1] = vec4(dnormal(p1), 0.0);
	mnorms[o + 2] = vec4(dnormal(p2), 0.0);
}

void main() {
	uint id = gl_GlobalInvocationID.x;
	ivec3 d1 = RES - 1;
	uint cells = uint(d1.x * d1.y * d1.z);
	if (id >= cells) {
		return;
	}
	ivec3 cell = ivec3(int(id) % d1.x, (int(id) / d1.x) % d1.y, int(id) / (d1.x * d1.y));

	float cv[8];
	vec3 cp[8];
	for (int k = 0; k < 8; k++) {
		ivec3 cc = cell + CORNER[k];
		cv[k] = dsample(cc);
		cp[k] = ORIGIN + vec3(cc) * DX;
	}

	for (int t = 0; t < 6; t++) {
		int lc[4] = int[4](TET[t * 4 + 0], TET[t * 4 + 1], TET[t * 4 + 2], TET[t * 4 + 3]);
		int mask = 0;
		for (int k = 0; k < 4; k++) {
			if (cv[lc[k]] >= ISO) {
				mask |= 1 << k;
			}
		}
		if (mask == 0 || mask == 15) {
			continue;
		}

		vec3 ev[6];
		for (int e = 0; e < 6; e++) {
			int i = EA[e], j = EB[e];
			if (((mask >> i) & 1) != ((mask >> j) & 1)) {
				float vi = cv[lc[i]], vj = cv[lc[j]];
				float tt = clamp((ISO - vi) / (vj - vi + 1e-8), 0.0, 1.0);
				ev[e] = mix(cp[lc[i]], cp[lc[j]], tt);
			}
		}

		int inside = bitCount(mask);
		if (inside == 1 || inside == 3) {
			int lone = 0;
			for (int k = 0; k < 4; k++) {
				bool bit = ((mask >> k) & 1) != 0;
				if ((inside == 1) == bit) {
					lone = k;
				}
			}
			write_tri(ev[INC[lone * 3 + 0]], ev[INC[lone * 3 + 1]], ev[INC[lone * 3 + 2]]);
		} else {
			int in0 = -1, in1 = -1, out0 = -1, out1 = -1;
			for (int k = 0; k < 4; k++) {
				if (((mask >> k) & 1) != 0) {
					if (in0 < 0) {
						in0 = k;
					} else {
						in1 = k;
					}
				} else {
					if (out0 < 0) {
						out0 = k;
					} else {
						out1 = k;
					}
				}
			}
			// edge index between two tet-local corners
			int e_ac = -1, e_ad = -1, e_bc = -1, e_bd = -1;
			for (int e = 0; e < 6; e++) {
				int a = EA[e], b = EB[e];
				if ((a == in0 && b == out0) || (a == out0 && b == in0)) {
					e_ac = e;
				}
				if ((a == in0 && b == out1) || (a == out1 && b == in0)) {
					e_ad = e;
				}
				if ((a == in1 && b == out0) || (a == out0 && b == in1)) {
					e_bc = e;
				}
				if ((a == in1 && b == out1) || (a == out1 && b == in1)) {
					e_bd = e;
				}
			}
			write_tri(ev[e_ac], ev[e_ad], ev[e_bd]);
			write_tri(ev[e_ac], ev[e_bd], ev[e_bc]);
		}
	}
}
