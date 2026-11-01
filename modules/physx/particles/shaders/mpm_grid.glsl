#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

void main() {
	uint n = gl_GlobalInvocationID.x;
	uint node_count = uint(RES.x * RES.y * RES.z);
	if (n >= node_count) {
		return;
	}

	int b = int(n) * 4;
	float m = float(grid_i[b + 0]) / FIXED;
	if (m <= 1e-9) {
		grid_v[n] = vec4(0.0);
		return;
	}
	vec3 mom = vec3(grid_i[b + 1], grid_i[b + 2], grid_i[b + 3]) / FIXED;
	vec3 vel = mom / m;
	vel += DT * GRAV;

	// separating boundary a couple of cells thick around the domain
	ivec3 c = node_coord(int(n));
	const int BW = 2;
	if (c.x < BW && vel.x < 0.0) {
		vel.x = 0.0;
	}
	if (c.y < BW && vel.y < 0.0) {
		vel.y = 0.0;
	}
	if (c.z < BW && vel.z < 0.0) {
		vel.z = 0.0;
	}
	if (c.x >= RES.x - BW && vel.x > 0.0) {
		vel.x = 0.0;
	}
	if (c.y >= RES.y - BW && vel.y > 0.0) {
		vel.y = 0.0;
	}
	if (c.z >= RES.z - BW && vel.z > 0.0) {
		vel.z = 0.0;
	}

	grid_v[n] = vec4(vel, m);
}
