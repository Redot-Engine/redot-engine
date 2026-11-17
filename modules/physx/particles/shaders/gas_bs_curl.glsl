#[compute]
#version 450

#include "gas_block_inc.glsl"

// Binding 5 = the pre-step velocity field being read this frame (whichever of
// grid A/B is "current"); binding 7 = curl output. Has to be its own pass,
// separate from gas_bs_step: vorticity confinement needs the GRADIENT of
// |curl|, which needs every cell's curl already written before any cell reads
// its neighbours', so it can't be folded into the single read-old/write-new
// step pass.
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
	float inv2dx = 0.5 / CDX;
	vec3 curl;
	curl.x = (vyp.z - vyn.z) * inv2dx - (vzp.y - vzn.y) * inv2dx;
	curl.y = (vzp.x - vzn.x) * inv2dx - (vxp.z - vxn.z) * inv2dx;
	curl.z = (vxp.y - vxn.y) * inv2dx - (vyp.x - vyn.x) * inv2dx;
	curl_data[idx] = vec4(curl, length(curl));
}
