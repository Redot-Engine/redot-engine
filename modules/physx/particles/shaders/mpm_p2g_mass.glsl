#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

// P2G pass 1: scatter particle mass to the grid so pass 2 can read back a local
// density for the equation of state.
void main() {
	uint id = gl_GlobalInvocationID.x;
	if (id >= uint(PCOUNT)) {
		return;
	}
	vec3 x = particles[id].x_d.xyz;
	ivec3 base;
	vec3 fx;
	grid_local(x, base, fx);
	vec3 wx = bspline(fx.x), wy = bspline(fx.y), wz = bspline(fx.z);

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				ivec3 node = base + ivec3(i, j, k);
				if (any(lessThan(node, ivec3(0))) || any(greaterThanEqual(node, RES))) {
					continue;
				}
				float w = wx[i] * wy[j] * wz[k];
				atomicAdd(grid_i[node_index(node) * 4 + 0], int(w * PMASS * FIXED));
			}
		}
	}
}
