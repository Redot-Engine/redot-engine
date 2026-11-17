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
	grid_i[b + 0] = 0;
	grid_i[b + 1] = 0;
	grid_i[b + 2] = 0;
	grid_i[b + 3] = 0;
	grid_v[n] = vec4(0.0);
}
