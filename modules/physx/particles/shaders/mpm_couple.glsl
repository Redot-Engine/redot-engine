#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

// Rigid coupling: each analytic collider is a moving velocity boundary condition
// on the grid. Grid nodes near a collider surface that are moving into it have
// the inward normal velocity removed and tangential slip damped toward the
// collider velocity; the equal/opposite impulse is summed per-collider (coarse
// fixed-point atomics) for the CPU to push the rigid body back. Positional
// depenetration of stragglers is handled in G2P. (collider_sdf / collider_normal
// come from mpm_fluid_inc.glsl.)

void main() {
	uint n = gl_GlobalInvocationID.x;
	uint node_count = uint(RES.x * RES.y * RES.z);
	if (n >= node_count) {
		return;
	}
	vec4 gv = grid_v[n];
	if (gv.w <= 1e-9) {
		return;
	}

	ivec3 c = node_coord(int(n));
	vec3 wp = ORIGIN + vec3(c) * DX;
	vec3 vel = gv.xyz;
	float m = gv.w;

	const float skin = 0.5 * DX;
	const float buried = 1.0 * DX; // deeper than this: node is inside solid, fluid shouldn't be here

	for (int ci = 0; ci < NCOL; ci++) {
		float sd = collider_sdf(ci, wp);
		if (sd >= skin) {
			continue;
		}
		vec3 cv = colliders[ci].c2.xyz;

		// Buried well inside the collider: don't try to redirect along a normal
		// that flips near the medial axis (that is what launches thin-wall fluid).
		// Just take the collider's velocity.
		if (sd < -buried) {
			vec3 dvv = cv - vel;
			atomicAdd(cimp[ci * 4 + 0], int(-m * dvv.x * IMP_FIXED));
			atomicAdd(cimp[ci * 4 + 1], int(-m * dvv.y * IMP_FIXED));
			atomicAdd(cimp[ci * 4 + 2], int(-m * dvv.z * IMP_FIXED));
			vel = cv;
			continue;
		}

		vec3 nh = collider_normal(ci, wp);
		vec3 rel = vel - cv;
		float vn = dot(rel, nh);
		bool penetrating = sd < 0.0;
		if (vn >= 0.0 && !penetrating) {
			continue; // separating and outside -- leave it
		}

		// Hard boundary: remove the into-surface velocity, damp the tangential
		// slip toward the collider. No depenetration velocity is injected -- it
		// jets a fluid up walls and catapults an object off a granular bed;
		// straggler push-out is positional, in G2P.
		vec3 rel_t = rel - min(vn, 0.0) * nh;
		vec3 nvel = cv + rel_t * clamp(1.0 - CFRIC, 0.0, 1.0);
		vec3 dv = nvel - vel;

		vec3 imp = -m * dv;
		atomicAdd(cimp[ci * 4 + 0], int(imp.x * IMP_FIXED));
		atomicAdd(cimp[ci * 4 + 1], int(imp.y * IMP_FIXED));
		atomicAdd(cimp[ci * 4 + 2], int(imp.z * IMP_FIXED));

		vel = nvel;
	}

	grid_v[n].xyz = vel;
}
