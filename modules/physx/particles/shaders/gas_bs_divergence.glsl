#[compute]
#version 450

#include "gas_block_inc.glsl"

// Binding 5 = the post-step, pre-projection velocity field (this frame's
// gas_bs_step output, NOT yet "current" -- same buffer gas_bs_project will
// correct). Computes div(velocity) per cell into the Divergence buffer and
// zero-initializes the pressure ping-pong buffers as the Jacobi solve's
// starting guess.
vec3 vel_at(ivec3 c) {
	if (any(lessThan(c, ivec3(0))) || any(greaterThanEqual(c, BOXC))) {
		return vec3(0.0);
	}
	int slot = block_lookup(c >> 2);
	if (slot < 0) {
		return vec3(0.0);
	}
	return data5[slot * BCELLS + lin3(c & 3)].xyz;
}

layout(local_size_x = GROUP) in;

void main() {
	uint i = gl_GlobalInvocationID.x;
	if (int(i) >= BOXC.x * BOXC.y * BOXC.z) {
		return;
	}
	int cx = BOXC.x, cy = BOXC.y;
	ivec3 c = ivec3(int(i) % cx, (int(i) / cx) % cy, int(i) / (cx * cy));
	int slot = block_lookup(c >> 2);
	if (slot < 0) {
		return;
	}
	int idx = slot * BCELLS + lin3(c & 3);

	vec3 vxp = vel_at(c + ivec3(1, 0, 0)), vxn = vel_at(c - ivec3(1, 0, 0));
	vec3 vyp = vel_at(c + ivec3(0, 1, 0)), vyn = vel_at(c - ivec3(0, 1, 0));
	vec3 vzp = vel_at(c + ivec3(0, 0, 1)), vzn = vel_at(c - ivec3(0, 0, 1));
	float div = ((vxp.x - vxn.x) + (vyp.y - vyn.y) + (vzp.z - vzn.z)) * (0.5 / CDX);
	divergence_data[idx] = div;
	pressure5[idx] = 0.0;
	pressure6[idx] = 0.0;
}
