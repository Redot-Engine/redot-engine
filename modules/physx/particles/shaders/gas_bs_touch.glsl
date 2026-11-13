#[compute]
#version 450

#include "gas_block_inc.glsl"

layout(local_size_x = GROUP) in;

// Registers every block in the fixed box as active. Run once at configure()
// (and again if the domain is resized) -- unlike the MPM path, this solver's
// active set does not get rebuilt every step, since there are no particles to
// re-scatter it from.
void main() {
	uint i = gl_GlobalInvocationID.x;
	if (int(i) >= BOXB.x * BOXB.y * BOXB.z) {
		return;
	}
	int bx = BOXB.x, by = BOXB.y;
	int ix = int(i) % bx;
	int iy = (int(i) / bx) % by;
	int iz = int(i) / (bx * by);
	block_touch(ivec3(ix, iy, iz));
}
