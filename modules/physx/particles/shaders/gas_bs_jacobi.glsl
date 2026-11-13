#[compute]
#version 450

#include "gas_block_inc.glsl"

// One Jacobi relaxation step solving Laplacian(p) = div for the discrete
// pressure field: p(c) = (sum of 6-neighbour p - dx^2 * div(c)) / 6. Reads
// pressure5 (this iteration's "current" guess), writes pressure6 (the
// updated guess) -- GasSolver::step alternates which physical buffer is
// bound to which slot across a fixed number of iterations (ping-pong, same
// pattern as Grid5/Grid6). Out-of-box or unallocated neighbors read as
// p = 0 -- an open (Dirichlet) boundary, matching the existing "zero outside
// the box" convention everywhere else in this solver; collider cells are NOT
// special-cased (see gas_block_inc.glsl's header note).
//
// A red-black Gauss-Seidel + SOR variant was tried here (converges faster
// per unit of work in theory), but a live test showed smoke leaking straight
// THROUGH a collider across its whole footprint instead of spreading past
// its edges -- reproduced at both omega=1.8 and omega=1.0 (plain GS), so the
// irregular boundary handling in this solver (sparse block-hash grid,
// colliders not specially handled in the pressure solve itself) doesn't
// tolerate that scheme here. Reverted to plain Jacobi.
float press_at(ivec3 c) {
	if (any(lessThan(c, ivec3(0))) || any(greaterThanEqual(c, BOXC))) {
		return 0.0;
	}
	int slot = block_lookup(c >> 2);
	if (slot < 0) {
		return 0.0;
	}
	return pressure5[slot * BCELLS + lin3(c & 3)];
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

	float psum = press_at(c + ivec3(1, 0, 0)) + press_at(c - ivec3(1, 0, 0)) +
			press_at(c + ivec3(0, 1, 0)) + press_at(c - ivec3(0, 1, 0)) +
			press_at(c + ivec3(0, 0, 1)) + press_at(c - ivec3(0, 0, 1));
	pressure6[idx] = (psum - CDX * CDX * divergence_data[idx]) / 6.0;
}
