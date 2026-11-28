#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

void main() {
	uint id = gl_GlobalInvocationID.x;
	if (id >= uint(PCOUNT)) {
		return;
	}

	Particle p = particles[id];
	vec3 x = p.x_d.xyz;

	ivec3 base;
	vec3 fx;
	grid_local(x, base, fx);
	vec3 wx = bspline(fx.x), wy = bspline(fx.y), wz = bspline(fx.z);

	vec3 new_v = vec3(0.0);
	mat3 new_C = mat3(0.0);
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				ivec3 node = base + ivec3(i, j, k);
				if (any(lessThan(node, ivec3(0))) || any(greaterThanEqual(node, RES))) {
					continue;
				}
				float w = wx[i] * wy[j] * wz[k];
				vec3 gv = grid_v[node_index(node)].xyz;
				vec3 dpos = (ORIGIN + vec3(node) * DX) - x;
				new_v += w * gv;
				new_C += (4.0 / (DX * DX)) * w * outerProduct(gv, dpos);
			}
		}
	}

	// Gentle global damping. Granular material creeps at an unconfined free
	// surface (continuum MPM sand always does); a touch more damping settles it.
	new_v *= GRANULAR ? 0.992 : 0.996;

	// safety clamp: nothing in a plausible fluid moves faster than this, and it
	// stops a bad grid velocity (coupling spike, coarse-grid aliasing) from
	// launching a particle
	float sp = length(new_v);
	if (sp > 25.0) {
		new_v *= 25.0 / sp;
	}

	x += DT * new_v;

	// positional depenetration: push a particle that has entered a collider back
	// to its surface and kill the into-surface velocity (no energy injected)
	for (int ci = 0; ci < NCOL; ci++) {
		float sd = collider_sdf(ci, x);
		if (sd < 0.0) {
			vec3 nh = collider_normal(ci, x);
			x += nh * (-sd + 1e-4);
			float vn = dot(new_v - colliders[ci].c2.xyz, nh);
			if (vn < 0.0) {
				new_v -= vn * nh;
			}
		}
	}

	// keep particles inside the domain (walls take a little energy)
	vec3 lo = bmin.xyz + DX * 1.5;
	vec3 hi = bmax.xyz - DX * 1.5;
	if (x.x < lo.x) {
		x.x = lo.x;
		new_v.x *= -0.2;
	}
	if (x.y < lo.y) {
		x.y = lo.y;
		new_v.y *= -0.2;
	}
	if (x.z < lo.z) {
		x.z = lo.z;
		new_v.z *= -0.2;
	}
	if (x.x > hi.x) {
		x.x = hi.x;
		new_v.x *= -0.2;
	}
	if (x.y > hi.y) {
		x.y = hi.y;
		new_v.y *= -0.2;
	}
	if (x.z > hi.z) {
		x.z = hi.z;
		new_v.z *= -0.2;
	}

	particles[id].x_d = vec4(x, p.x_d.w);
	particles[id].v = vec4(new_v, 0.0);
	particles[id].c0 = vec4(new_C[0], 0.0);
	particles[id].c1 = vec4(new_C[1], 0.0);
	particles[id].c2 = vec4(new_C[2], 0.0);
}
