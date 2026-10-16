// Foam spawn pass -- Ihmsen et al. 2012 ("Unified spray, foam and air bubbles
// for particle-based fluids") evaluated per fluid particle off this frame's
// final MPM state. Four potentials per particle:
//
//   trapped air  -- deviatoric strain rate + compression off the affine matrix
//                   C (converging/opposing streams), the continuum form of
//                   Ihmsen Eq. 2's neighbor sum;
//   wave crest   -- outward motion (v . n >= 0.6 of the mass-field gradient,
//                   Ihmsen Eq. 7's gate) weighted by surface steepness;
//   vorticity    -- |curl v| straight off C (Bender et al. 2019's channel);
//   kinetic energy -- 0.5 |v|^2.
//
// Every raw potential is normalized against a per-channel decaying running max
// (foam_norm, SPlisHSPlasH's auto-tuning mode) so the spawn scale holds across
// scenes and dt. The spawn count is energy-weighted and probabilistic --
// n = scale * I_ke * (k_ta*I_ta + k_wc*I_wc + k_vo*I_vo) * dt, floor + random
// fraction (RikkaBunny's scheme) -- landing in Ihmsen's velocity-aligned
// cylinder with a tangential jitter. One thread per fluid particle; spawns
// land in a fixed-capacity ring (foam_meta.x counts, oldest slots recycle).
//
// Runs once per frame after the substep loop (visual layer, 60 Hz is plenty);
// never touches the MPM grid or particle state.
#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

// Per-frame decay of the running maxima: ~6%/frame halves a stale peak in
// ~11 frames (~0.19 s at 60 Hz). The half-life must sit well under a typical
// agitation cycle so the maxima track the RECENT splash envelope -- pinned at
// rare all-time peaks they would starve spawns between peaks (measured: the
// pool test's foam population collapsed to zero mid-churn at 0.985).
const float NORM_DECAY = 0.94;
// Hard cap on spawns from one fluid particle in one frame (the ring recycles
// past the budget anyway; this bounds worst-case atomic pressure).
const uint MAX_SPAWNS_PER_PARTICLE = 8u;

void main() {
	uint id = gl_GlobalInvocationID.x;

	// Thread 0 decays the running maxima before this frame's samples max in.
	// The write races other threads' atomicMax -- a lost decay only delays
	// normalization by a frame, which a decaying maximum tolerates by design.
	if (id == 0u) {
		for (int c = 0; c < 4; c++) {
			float m = uintBitsToFloat(foam_norm[c]);
			if (m > 1e-6f) {
				foam_norm[c] = floatBitsToUint(m * NORM_DECAY);
			}
		}
	}

	if (id >= uint(PCOUNT)) {
		return;
	}
	vec3 x = particles[id].x_d.xyz;
	vec3 v = particles[id].v.xyz;

	mat3 C = mat3(particles[id].c0.xyz, particles[id].c1.xyz, particles[id].c2.xyz);
	float vlen = length(v);

	// --- raw potentials (all non-negative, so the uint bit trick sorts) -----
	float vlen_safe = max(vlen, 1e-4);

	// Trapped air: shear rate + compression (converging flow), 1/s units.
	float div = C[0][0] + C[1][1] + C[2][2];
	float ta_raw = foam_strain_dev(C) + max(-div, 0.0);

	// Wave crest: outward motion through the surface band. The mass-field
	// gradient is nonzero only near the surface, so steepness doubles as the
	// surface-proximity factor Ihmsen's curvature sum provides.
	vec4 mg = sample_grid_mass_grad(x);
	float steep = mg.w * DX / max(mg.w * DX + sample_grid_mass(x), 1e-4);
	vec3 n_out = mg.w > 1e-6 ? -mg.xyz / mg.w : vec3(0.0, 1.0, 0.0);
	float outward = dot(v / vlen_safe, n_out);
	float wc_raw = outward > 0.6 ? outward * vlen * steep : 0.0;

	// Vorticity: 1/s units off C.
	float vo_raw = foam_curl_len(C);

	// Kinetic energy (unit mass): m^2/s^2.
	float ke_raw = 0.5 * dot(v, v);

	// --- normalize against the running maxima -------------------------------
	// Snapshot the maxima BEFORE this frame's samples max in: every thread then
	// normalizes against the same pre-frame scale (no first-frame spike, no
	// per-thread disagreement), and the new peaks raise the maxima for frames
	// to come. The NaN ternaries keep a poisoned potential from pinning a max.
	uvec4 prev = foam_norm;
	// Absolute per-channel floors, in each potential's own physical units -- the
	// minimum credible agitation (SPlisHSPlasH's --limits minimums play exactly
	// this role). Without a floor, a settling pool's maxima decay toward zero
	// and its own residual noise renormalizes to ~1.0: the layer foams at rest
	// in a runaway feedback loop (measured: 5k particles spawned into a still
	// pool). Roughly: |v| = 0.45 m/s -> ke floor; ~1/s shear for ta/vo; a 0.1
	// m/s outward-crest product for wc.
	const float FLOOR_TA = 1.0;
	const float FLOOR_WC = 0.1;
	const float FLOOR_VO = 1.0;
	const float FLOOR_KE = 0.1;
	atomicMax(foam_norm.x, ta_raw == ta_raw ? floatBitsToUint(ta_raw) : 0u);
	atomicMax(foam_norm.y, wc_raw == wc_raw ? floatBitsToUint(wc_raw) : 0u);
	atomicMax(foam_norm.z, vo_raw == vo_raw ? floatBitsToUint(vo_raw) : 0u);
	atomicMax(foam_norm.w, ke_raw == ke_raw ? floatBitsToUint(ke_raw) : 0u);

	float i_ta = clamp(ta_raw / max(uintBitsToFloat(prev.x), FLOOR_TA), 0.0, 1.0);
	float i_wc = clamp(wc_raw / max(uintBitsToFloat(prev.y), FLOOR_WC), 0.0, 1.0);
	float i_vo = clamp(vo_raw / max(uintBitsToFloat(prev.z), FLOOR_VO), 0.0, 1.0);
	float i_ke = clamp(ke_raw / max(uintBitsToFloat(prev.w), FLOOR_KE), 0.0, 1.0);

	// --- energy-weighted probabilistic spawn count --------------------------
	float f = FOAM_THRESH * FOAM_K_KE * i_ke *
			(FOAM_K_TA * i_ta + FOAM_K_WC * i_wc + FOAM_K_VO * i_vo) * FRAME_DT;
	uint count = uint(floor(f));
	uint rng = foam_hash(id + foam_meta.x * 2654435761u + floatBitsToUint(x.x) + floatBitsToUint(x.y) * 3u);
	if (foam_rand(rng) < f - float(count)) {
		count++;
	}
	if (count == 0u) {
		return;
	}
	count = min(count, MAX_SPAWNS_PER_PARTICLE);

	// --- place the spawns in Ihmsen's velocity-aligned cylinder --------------
	vec3 vn = v / vlen_safe;
	vec3 helper = abs(vn.y) < 0.9 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
	vec3 e1 = normalize(cross(vn, helper));
	vec3 e2 = cross(vn, e1);
	const float radius = DX * 0.5; // one fluid-particle radius

	for (uint i = 0u; i < count; i++) {
		float r1 = foam_rand(rng);
		float r2 = foam_rand(rng);
		float r3 = foam_rand(rng);
		float r4 = foam_rand(rng);
		float r = radius * sqrt(r1);
		float theta = r2 * 6.2831853;
		// Cylinder height spans the distance the fluid particle travels this
		// frame, so fast jets lay down a dense trail and still pools none.
		float h = (r3 - 0.5) * FRAME_DT * vlen;
		vec3 offset = (r * cos(theta)) * e1 + (r * sin(theta)) * e2;
		vec3 pos = x + offset + h * vn;
		// Mild tangential velocity jitter (the references' small kick) so a
		// clump of spawns doesn't share one identical trajectory.
		vec3 vel = v + offset * 2.0;

		uint slot = atomicAdd(foam_meta.x, 1u) % uint(max(FOAM_CAP, 1.0));
		foam[slot].pos_life = vec4(pos, FOAM_LIFE * (0.75 + 0.5 * i_ke * r4));
		foam[slot].vel_kind = vec4(vel, 1.0); // 1 = foam; the advect pass re-classifies
	}
}
