#[compute]
#version 450

#include "gas_block_inc.glsl"

// Zeroes grid cells for a contiguous SLOT range [growth_range.x, growth_range.y)
// directly -- no hash lookup, no block coordinate iteration. Used only when
// growing the domain (see GasSolver::grow_if_needed): newly block_touch()'d
// slots always land at the END of the pool (bcounts[0] only ever increases,
// each new block claims the next sequential slot), so "everything inserted
// since the last growth pass" is exactly this range. Critically this must
// NOT touch any slot below growth_range.x -- that's already-populated smoke,
// clearing it would erase real simulation state, not just newly-allocated
// empty cells (unlike gas_bs_clear, which is safe to hit every cell because
// it only ever runs once, before anything has simulated).
layout(local_size_x = GROUP) in;

void main() {
	uint i = gl_GlobalInvocationID.x;
	int start_slot = int(growth_range.x);
	int end_slot = int(growth_range.y);
	int slot = start_slot + int(i) / BCELLS;
	if (slot >= end_slot) {
		return;
	}
	int idx = slot * BCELLS + (int(i) % BCELLS);
	data5[idx] = vec4(0.0);
	data6[idx] = vec4(0.0);
}
