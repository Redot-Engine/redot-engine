#[compute]
#version 450

#include "gas_block_inc.glsl"

// Subtracts grad(pressure) from velocity so the field is approximately
// divergence-free -- the final step of the projection, run once/step after
// gas_bs_jacobi's fixed iteration count. Binding 5 is the post-step,
// pre-projection grid (read); binding 6 is GasSolver's dedicated scratch
// buffer (write) -- NEVER the same physical buffer as binding 5 (an earlier
// version aliased them for an in-place correction, which turned out to be a
// real hazard, not just a GLSL semantics technicality -- see
// gas_block_inc.glsl's header). GasSolver::step() copies this pass's output
// back into the grid_a/grid_b ping-pong afterward via a plain buffer copy.
// Pressure5 is fixed to whichever physical buffer PRESSURE_JACOBI_ITERS'
// parity leaves the final solve in (see GasSolver::step) -- this pass only
// reads it, never writes.
//
// Re-zeroes velocity inside colliders same as gas_bs_step's own collider
// loop: projection can otherwise inject a nonzero velocity into a collider
// cell via grad(p) even though gas_bs_step already forced it to zero.
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

	vec4 v = data5[idx];
	float pxp = press_at(c + ivec3(1, 0, 0)), pxn = press_at(c - ivec3(1, 0, 0));
	float pyp = press_at(c + ivec3(0, 1, 0)), pyn = press_at(c - ivec3(0, 1, 0));
	float pzp = press_at(c + ivec3(0, 0, 1)), pzn = press_at(c - ivec3(0, 0, 1));
	vec3 grad = vec3(pxp - pxn, pyp - pyn, pzp - pzn) * (0.5 / CDX);
	vec3 new_vel = v.xyz - grad;

	vec3 wpos = ORIGIN + (vec3(c) + 0.5) * CDX;
	for (int ci = 0; ci < MAX_GAS_COLLIDERS; ci++) {
		if (collider_sdf(ci, wpos) < 0.0) {
			new_vel = vec3(0.0);
			break;
		}
	}

	data6[idx] = vec4(new_vel, v.w);
}
