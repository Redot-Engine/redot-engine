#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

#define SURF_H (extra.w) // isosurface SPH kernel radius, world meters
#define SURF_BOOST (bmin.w) // per-particle mass multiplier (inflates the surface)

// Scatter each particle's mass into surf_i with a poly6 SPH kernel of radius
// SURF_H (driven by particle_size). This is a separate reconstruction from the
// P2G grid mass -- its reach scales with particle_size, so a bigger particle
// inflates the isosurface the way the PBD extractor's particle radius does,
// rather than just shifting it. surf_i is cleared each frame by the caller.
void main() {
	uint id = gl_GlobalInvocationID.x;
	if (id >= uint(PCOUNT)) {
		return;
	}
	vec3 x = particles[id].x_d.xyz;

	float h = max(SURF_H, DX);
	float h2 = h * h;
	float norm = (315.0 / (64.0 * 3.14159265 * pow(h, 9.0))) * PMASS * SURF_BOOST; // kg/m^3 per (h^2-d^2)^3

	int R = min(int(ceil(h / DX)) + 1, 6);
	ivec3 base = ivec3(floor((x - ORIGIN) / DX));
	for (int dz = -R; dz <= R; dz++) {
		for (int dy = -R; dy <= R; dy++) {
			for (int dx = -R; dx <= R; dx++) {
				ivec3 nc = base + ivec3(dx, dy, dz);
				if (any(lessThan(nc, ivec3(0))) || any(greaterThanEqual(nc, RES))) {
					continue;
				}
				vec3 np = ORIGIN + vec3(nc) * DX;
				float d2 = dot(np - x, np - x);
				if (d2 >= h2) {
					continue;
				}
				float t = h2 - d2;
				float dens = norm * t * t * t;
				atomicAdd(surf_i[node_index(nc)], int(dens * SURF_FIXED));
			}
		}
	}
}
