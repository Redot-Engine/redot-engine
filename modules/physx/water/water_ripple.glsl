#[compute]
#version 450

#include "water_inc.glsl"

// iWave-style local ripple layer (Tessendorf's damped 2D wave equation),
// reflecting (clamped/Neumann) walls at the grid edge, plus sphere/impulse
// body-disturbance coupling -- port of caustic-volume's `ripple` shader
// (sandbox/src/10_sim.js), simplified for Phase 1. This alone IS the water
// height for now (WATER_LEVEL + this layer's h); Phase 2 composes it with
// the FFT ocean spectrum layer instead of standing in for the whole surface.
//
// Written flat -- no helper functions beyond simple, non-recursive, single-
// call-site ones -- deliberately, per this session's own hard-won lesson
// about a heavily-inlined branchy helper hanging the Blackwell driver's PSO
// compiler in a completely different shader (mpm_bs_march.glsl). This
// shader's real cost is O(GRID_N^2) with a small constant-time body per
// cell (one 5-point Laplacian, a short loop over active spheres/impulses),
// not a deep inlined call chain -- a different risk shape, but keep it that
// way as MAX_SPHERES/MAX_IMPULSES grow.

layout(local_size_x = 8, local_size_y = 8) in;

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	if (c.x >= GRID_N || c.y >= GRID_N) {
		return;
	}
	int idx = grid_index(c);
	if (!is_wet(c)) {
		// Dry (seabed above the water, or outside the surface mesh's
		// footprint): never simulated. Its
		// rendered height continues the adjacent water (mean of its wet 3x3
		// neighbors, last step's values) instead of pinning to WATER_LEVEL:
		// the surface mesh's rim lies between the last wet and first dry cell
		// centers, and a flat dry texel there turns every wave crest at the
		// staircase shoreline into a one-cell sawtooth. Matches the zero-
		// gradient wall the wet cells see.
		float sum = 0.0;
		float count = 0.0;
		for (int dy = -1; dy <= 1; dy++) {
			for (int dx = -1; dx <= 1; dx++) {
				ivec2 nc = clamp(c + ivec2(dx, dy), ivec2(0), ivec2(GRID_N - 1));
				if (is_wet(nc)) {
					sum += state_in[grid_index(nc)].x;
					count += 1.0;
				}
			}
		}
		state_out[idx] = vec4(0.0);
		height_out[idx] = WATER_LEVEL + (count > 0.0 ? sum / count : 0.0);
		return;
	}
	vec4 st = state_in[idx];
	float h = st.x, hp = st.y;

	int xl = max(c.x - 1, 0), xr = min(c.x + 1, GRID_N - 1);
	int yl = max(c.y - 1, 0), yr = min(c.y + 1, GRID_N - 1);
	// Variable-depth wave equation in flux form: each face between two wet
	// cells carries the mean of their coefficients, so it stays symmetric
	// (energy-conserving) as the depth changes -- waves slow and bunch up
	// over shallows. A dry neighbor (or the clamped grid edge, which
	// contributes h - h = 0) has no face: a reflecting wall.
	float k_c = wave_coefficient(c);
	ivec2 nbr[4] = ivec2[4](ivec2(xl, c.y), ivec2(xr, c.y), ivec2(c.x, yl), ivec2(c.x, yr));
	float flux = 0.0;
	for (int i = 0; i < 4; i++) {
		if (is_wet(nbr[i])) {
			flux += 0.5 * (k_c + wave_coefficient(nbr[i])) * (state_in[grid_index(nbr[i])].x - h);
		}
	}
	// Explicit damped wave equation, backward-difference damping (h_t ~=
	// (h-hp)/dt): hn = h*(2-a) - hp*(1-a) + flux, a = ALPHA*DT. At a local
	// bump the flux is negative and pulls it back down -- its sign is what
	// keeps this stable (getting it backwards once turned a small impulse
	// into 1e8+ within ~150 steps).
	float a = ALPHA * DT;
	float hn = h * (2.0 - a) - hp * (1.0 - a) + flux;

	// Body-disturbance coupling: pull the surface toward a soft target set by
	// nearby spheres/impulses, rather than snapping to it instantly -- a
	// single-pole low-pass keeps this from injecting energy discontinuously.
	//
	// `weight` gates how much this blend actually applies at this cell --
	// real, load-bearing, not a nicety. target defaults to 0 with no body
	// nearby, and the naive `hn += (target-hn)*rate` (no weight) that used to
	// be here applied that unconditionally EVERYWHERE, every step: with
	// target=0 almost everywhere, it collapsed to hn *= 0.9 -- an ambient,
	// unintended ~10%/tick pull toward zero across the WHOLE field, every
	// tick, dwarfing the wave equation's own ALPHA damping (which is what a
	// real user correctly read as "feels like jello/syrup, ripples don't
	// travel" -- confirmed empirically: lowering ALPHA alone did nothing,
	// because this term, not ALPHA, was the dominant damping mechanism the
	// whole time). Gating by proximity weight makes the blend vanish far
	// from any body, letting the wave equation's own dynamics actually
	// govern how ripples propagate and persist.
	vec2 xz = (vec2(c) + 0.5) / float(GRID_N) * DOMAIN * 2.0 - DOMAIN;
	float target = 0.0;
	float weight = 0.0;
	for (int i = 0; i < NUM_SPHERES; i++) {
		vec4 s = spheres[i];
		float t = length(xz - s.xy) / max(s.z, 1e-4);
		if (t < 1.6) {
			float w = exp(-pow(t * 1.1, 6.0)) * s.w;
			target -= s.z * w;
			weight = max(weight, w);
		}
	}
	for (int i = 0; i < NUM_IMPULSES; i++) {
		vec4 imp = impulses[i];
		float t = length(xz - imp.xy) / max(imp.z, 1e-4);
		if (t < 1.0) {
			float w = (1.0 - t) * imp.w;
			target += imp.z * w;
			weight = max(weight, w);
		}
	}
	hn += (target * RIPPLE_AMPLITUDE - hn) * weight * clamp(6.0 * DT, 0.0, 1.0);

	state_out[idx] = vec4(hn, h, st.z, 0.0);
	height_out[idx] = WATER_LEVEL + hn;
}
