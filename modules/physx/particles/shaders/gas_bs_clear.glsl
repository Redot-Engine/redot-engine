#[compute]
#version 450

#include "gas_block_inc.glsl"

layout(local_size_x = GROUP) in;

// Zeroes both grid buffers for every cell in the (already-touched) fixed box.
// Run once at configure(), right after gas_bs_touch. Binding 5 = grid A,
// binding 6 = grid B for this pass (direction is irrelevant here).
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
	data5[idx] = vec4(0.0);
	data6[idx] = vec4(0.0);
}
