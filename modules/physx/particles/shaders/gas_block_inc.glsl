// Shared definitions for the sparse-grid gas/smoke solver (gas_bs_*.glsl).
//
// Reuses the exact block-hash technique the MPM block-sparse fluid grid uses
// (see mpm_block_inc.glsl) -- hash the 4^3 block a cell belongs to, not the
// cell itself, so a stencil/neighbourhood touches a handful of hash lookups
// instead of one per cell. The two solvers differ in what lives in a cell:
// MPM's grid is a disposable per-substep P2G/G2P scratch buffer rebuilt from
// particles every step; this grid is a PERSISTENT Eulerian velocity+density
// field that only advects/diffuses in place. Validated in a standalone
// RenderingDevice prototype before porting here -- see the backlog memory
// entry for the milestone history.
//
// Scope cuts (v1): the box's lateral (X/Z) footprint and grid_anchor (its
// bottom corner) are frozen at configure() and never move or reflow -- only
// its height grows automatically as a plume rises past it (GasSolver::
// _grow_if_needed periodically block_touch()'s more blocks at the top, safe
// because it never touches an already-populated block); up to
// MAX_GAS_COLLIDERS sphere colliders (voxelized solid: forced empty+still
// every step, not a real no-slip/free-slip boundary condition -- pressure
// solve below does NOT special-case them, so a collider surface isn't a true
// Neumann boundary, just an open one like the box edge).
//
// Pressure projection (added after the diffusion-only v1 turned out unable to
// hold a sharp density silhouette -- e.g. a mushroom-cloud cap always melted
// back into a blob within a few hundred steps, since diffusion is a pure
// blur with no mechanism to conserve volume/shape): each step, after
// advection+forces+injection (gas_bs_step), a divergence pass
// (gas_bs_divergence) computes div(velocity) per cell, a fixed number of
// Jacobi iterations (gas_bs_jacobi) solve the discrete Poisson equation
// Laplacian(p) = div, and a final pass (gas_bs_project) subtracts grad(p)
// from velocity so the field is approximately divergence-free -- the actual
// mechanism real smoke/mushroom-cloud caps get their sharp rolled-under edge
// from (incompressible flow forced to go somewhere coherent, not blur away).
// A FIXED iteration count (not a convergence check) trades solve accuracy for
// a predictable, bounded per-step GPU cost -- see GasSolver::
// PRESSURE_JACOBI_ITERS for the current count and the perf tradeoff note.

#define HASH_EMPTY 0xffffffffu
#define PROBE_MAX 48u
#define BLK 4
#define BCELLS 64
#define GROUP 64
#define MAX_GAS_COLLIDERS 4
// 8, not 4: a full mushroom-cloud/nuke-plume needs three simultaneous groups
// of emitters (a ground-hugging base surge ring, a rising shaft, a capping
// head burst) active at once, not time-shared -- 4 slots was only enough for
// one ring OR a shaft, never both plus a distinct head. See the backlog
// memory for the authoring recipe this unlocks.
#define MAX_GAS_EMITTERS 8
#define EMITTER_SPHERE 0
#define EMITTER_BOX 1

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 dt_buoy_vort_diss; // x dt, y buoyancy, z vorticity confinement strength, w dissipation
	vec4 origin_dx_pad; // xyz grid anchor (world, frozen at configure), w cell size
	ivec4 box_blocks_cells; // xyz box size in BLOCKS, w unused
	vec4 hash_maxb_time_pad; // x hash slot count (pow2), y block-pool capacity, z time (seconds, for emitter jitter)
	// x = turbulence strength (extra curl-noise velocity, m/s), y = turbulence
	// scale (spatial frequency, 1/meters -- higher = finer wrinkles), zw
	// unused. See curl_noise3()'s header comment for why this exists.
	vec4 turb_strength_scale_pad;
	// Analytic collider catalog (mirrors MPMFluidSolver's, minus rotation --
	// axis-aligned box only, v1): c0 = xyz world center/point, w = shape
	// (0=sphere, 1=box, 2=plane; -1 = unused slot). c1 = shape params: sphere
	// x=radius, box xyz=half-extents, plane xyz=unit outward normal.
	vec4 colliders_c0[MAX_GAS_COLLIDERS];
	vec4 colliders_c1[MAX_GAS_COLLIDERS];
	ivec4 growth_range; // x = first newly-touched pool slot, y = one past the last, zw unused -- gas_bs_clear_range only
	// Unified emitters -- sphere or (axis-aligned, v1: no rotation) box, each
	// with its own independent shape/size/velocity/density, up to
	// MAX_GAS_EMITTERS simultaneous. A world position alone can't signal
	// "unused", so shape doubles as the enable flag: -1 = disabled slot.
	vec4 emitter_pos_shape[MAX_GAS_EMITTERS]; // xyz world position, w shape (EMITTER_SPHERE/EMITTER_BOX, or -1 = disabled)
	vec4 emitter_size_density[MAX_GAS_EMITTERS]; // sphere: x = radius (yz unused); box: xyz = half-extents; w = injected density
	vec4 emitter_velocity[MAX_GAS_EMITTERS]; // xyz world-space injected velocity, w unused
	// x = divergence (m/s outward-radial speed added on top of velocity --
	// Flow's NvFlowEmitterSphereParams.divergence; this is what a burst/
	// explosion emitter wants instead of one fixed jet direction), y = swirl
	// (m/s tangential speed around world +Y through the emitter center, for
	// directly authoring rotation instead of relying only on vorticity
	// confinement amplifying incidental curl), zw unused.
	vec4 emitter_extra[MAX_GAS_EMITTERS];
};

// Signed distance from wp to collider ci's surface (negative = inside/solid).
// A disabled slot (colliders_c0[ci].w < 0) always returns a large positive
// distance so callers can use a plain `< 0.0` solid test uniformly, no
// separate enabled check needed.
float collider_sdf(int ci, vec3 wp) {
	float shape = colliders_c0[ci].w;
	if (shape < -0.5) {
		return 1e9;
	}
	vec3 ctr = colliders_c0[ci].xyz;
	if (shape > 1.5) { // plane: c1.xyz = unit outward normal, ctr = a point on it
		return dot(wp - ctr, colliders_c1[ci].xyz);
	}
	if (shape > 0.5) { // axis-aligned box: c1.xyz = half-extents
		vec3 d = abs(wp - ctr) - colliders_c1[ci].xyz;
		return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
	}
	return distance(wp, ctr) - colliders_c1[ci].x; // sphere: c1.x = radius
}

layout(set = 0, binding = 1, std430) restrict buffer BHashKey {
	uint bhash[];
};
layout(set = 0, binding = 2, std430) restrict buffer BHashVal {
	uint bhash_val[];
};
layout(set = 0, binding = 3, std430) restrict buffer BKey {
	uint bkey[];
};
layout(set = 0, binding = 4, std430) restrict buffer BCounts {
	uint bcounts[];
};
// Binding 5/6 swap roles (read/write, or read-for-curl/write-curl-out)
// between passes -- see each gas_bs_*.glsl for which is which this pass. The
// uniform set alternates which physical buffer (grid A or B) occupies which
// slot each step (ping-pong), matching the prototype's us_step_atob /
// us_step_btoa. gas_bs_project reads NEW_STATE and writes to a dedicated
// scratch buffer (never the same physical buffer as binding 5) -- an earlier
// version aliased data5/data6 to the SAME buffer for an in-place correction,
// which turned out to be a real hazard (reproduced: density silently went to
// zero and stayed there after the solver had configure()'d more than once,
// root-caused to this aliasing) -- so both bindings are safely "restrict"
// again, and GasSolver::step() copies the scratch buffer back into NEW_STATE
// itself (a plain buffer copy, not a compute dispatch) afterward.
layout(set = 0, binding = 5, std430) restrict buffer Grid5 {
	vec4 data5[];
};
layout(set = 0, binding = 6, std430) restrict buffer Grid6 {
	vec4 data6[];
};
layout(set = 0, binding = 7, std430) restrict buffer Curl {
	vec4 curl_data[];
};
// Pressure-projection scratch (gas_bs_divergence/gas_bs_jacobi/gas_bs_project
// only -- every other pass still has to bind these, unused, since they share
// this one descriptor-set layout). Divergence is computed once/step from the
// post-step, pre-projection velocity field; pressure5/pressure6 ping-pong
// across a fixed number of Jacobi iterations solving Laplacian(p) = div,
// exactly like Grid5/Grid6 but scalar. See gas_bs_jacobi.glsl for the solve.
layout(set = 0, binding = 8, std430) restrict buffer Divergence {
	float divergence_data[];
};
layout(set = 0, binding = 9, std430) restrict buffer Pressure5 {
	float pressure5[];
};
layout(set = 0, binding = 10, std430) restrict buffer Pressure6 {
	float pressure6[];
};

#define DT (dt_buoy_vort_diss.x)
#define BUOY (dt_buoy_vort_diss.y)
#define VORT (dt_buoy_vort_diss.z)
#define DISS (dt_buoy_vort_diss.w)
#define ORIGIN (origin_dx_pad.xyz)
#define CDX (origin_dx_pad.w)
#define BOXB (box_blocks_cells.xyz)
#define BOXC (box_blocks_cells.xyz * BLK)
#define HMASK (uint(hash_maxb_time_pad.x) - 1u)
#define MAXB (uint(hash_maxb_time_pad.y))
#define TIME (hash_maxb_time_pad.z)
#define TURB_STRENGTH (turb_strength_scale_pad.x)
#define TURB_SCALE (turb_strength_scale_pad.y)

uint pack_block(ivec3 bc) {
	uvec3 u = uvec3(bc + 512);
	return u.x | (u.y << 10) | (u.z << 20);
}

ivec3 unpack_block(uint k) {
	return ivec3(int(k & 1023u), int((k >> 10) & 1023u), int((k >> 20) & 1023u)) - 512;
}

uint hash_u32(uint k) {
	k ^= k >> 16;
	k *= 0x7feb352du;
	k ^= k >> 15;
	k *= 0x846ca68bu;
	k ^= k >> 16;
	return k;
}

// Register a block, claiming a pool slot on first sight. Idempotent.
void block_touch(ivec3 bc) {
	if (bcounts[0] >= MAXB) {
		return;
	}
	uint key = pack_block(bc);
	uint h = hash_u32(key) & HMASK;
	for (uint t = 0u; t < PROBE_MAX; t++) {
		uint prev = atomicCompSwap(bhash[h], HASH_EMPTY, key);
		if (prev == HASH_EMPTY) {
			uint slot = atomicAdd(bcounts[0], 1u);
			if (slot >= MAXB) {
				slot = MAXB - 1u;
			}
			bkey[slot] = key;
			bhash_val[h] = slot;
			return;
		}
		if (prev == key) {
			return;
		}
		h = (h + 1u) & HMASK;
	}
}

// Look up a block's pool slot, or -1.
int block_lookup(ivec3 bc) {
	uint key = pack_block(bc);
	uint h = hash_u32(key) & HMASK;
	for (uint t = 0u; t < PROBE_MAX; t++) {
		uint k = bhash[h];
		if (k == key) {
			return int(bhash_val[h]);
		}
		if (k == HASH_EMPTY) {
			return -1;
		}
		h = (h + 1u) & HMASK;
	}
	return -1;
}

int lin3(ivec3 c) {
	return (c.z * 4 + c.y) * 4 + c.x;
}

// Cheap per-cell, per-frame pseudo-random jitter -- seeds a little asymmetry
// into the source injection. A perfectly symmetric source has no curl for
// real vorticity confinement to amplify, so a laminar stable-fluids sim needs
// *something* non-symmetric to kick off instabilities, the way real smoke
// seeds off nozzle turbulence.
float hash13(vec3 p) {
	p = fract(p * 0.3183099 + vec3(0.1, 0.2, 0.3));
	p *= 17.0;
	return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Trilinearly-interpolated value noise -- hash13 alone is white noise (no
// continuity between lattice points), unusable for a smooth field whose
// GRADIENT we're about to take; this smooths it into something with
// coherent structure at the ~1-cell scale.
float vnoise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f); // smoothstep, avoids visible lattice creases
	float n000 = hash13(i + vec3(0.0, 0.0, 0.0));
	float n100 = hash13(i + vec3(1.0, 0.0, 0.0));
	float n010 = hash13(i + vec3(0.0, 1.0, 0.0));
	float n110 = hash13(i + vec3(1.0, 1.0, 0.0));
	float n001 = hash13(i + vec3(0.0, 0.0, 1.0));
	float n101 = hash13(i + vec3(1.0, 0.0, 1.0));
	float n011 = hash13(i + vec3(0.0, 1.0, 1.0));
	float n111 = hash13(i + vec3(1.0, 1.0, 1.0));
	float nx00 = mix(n000, n100, f.x), nx10 = mix(n010, n110, f.x);
	float nx01 = mix(n001, n101, f.x), nx11 = mix(n011, n111, f.x);
	return mix(mix(nx00, nx10, f.y), mix(nx01, nx11, f.y), f.z);
}

// Curl of a 3-component noise potential (Bridson's "curl noise" -- take the
// curl of a smooth vector field built from independent noise channels; the
// result is analytically divergence-free, so adding it as an extra velocity
// perturbs the flow's SHAPE without injecting or destroying mass, unlike a
// naive noise-displaced sample would). This is what fakes the small-scale
// "billowing cauliflower" turbulent detail a coarse real-time grid can never
// resolve on its own -- the same trick production VFX (and NVIDIA Flow's own
// example content) lean on rather than brute-forcing grid resolution up.
// Central-difference epsilon is in the SAME units as p (already pre-scaled
// by TURB_SCALE at the call site), so a fixed 0.15 is scale-independent.
vec3 curl_noise3(vec3 p) {
	const float e = 0.15;
	const vec3 off_x = vec3(37.2, 91.1, 13.7);
	const vec3 off_y = vec3(5.3, 62.4, 71.8);
	const vec3 off_z = vec3(83.9, 24.6, 47.1);
	float dz_dy = (vnoise3(p + off_z + vec3(0.0, e, 0.0)) - vnoise3(p + off_z - vec3(0.0, e, 0.0))) / (2.0 * e);
	float dy_dz = (vnoise3(p + off_y + vec3(0.0, 0.0, e)) - vnoise3(p + off_y - vec3(0.0, 0.0, e))) / (2.0 * e);
	float dx_dz = (vnoise3(p + off_x + vec3(0.0, 0.0, e)) - vnoise3(p + off_x - vec3(0.0, 0.0, e))) / (2.0 * e);
	float dz_dx = (vnoise3(p + off_z + vec3(e, 0.0, 0.0)) - vnoise3(p + off_z - vec3(e, 0.0, 0.0))) / (2.0 * e);
	float dy_dx = (vnoise3(p + off_y + vec3(e, 0.0, 0.0)) - vnoise3(p + off_y - vec3(e, 0.0, 0.0))) / (2.0 * e);
	float dx_dy = (vnoise3(p + off_x + vec3(0.0, e, 0.0)) - vnoise3(p + off_x - vec3(0.0, e, 0.0))) / (2.0 * e);
	return vec3(dz_dy - dy_dz, dx_dz - dz_dx, dy_dx - dx_dy);
}

// Two octaves (coarse rolling structure + a finer wrinkle on top, standard
// fractal-noise practice) summed at half amplitude/double frequency each
// step up. Animated by drifting the sample point with TIME so the wrinkles
// roil instead of sitting frozen in world space.
vec3 turbulence_velocity(vec3 wpos) {
	if (TURB_STRENGTH <= 0.0) {
		return vec3(0.0);
	}
	vec3 p0 = wpos * TURB_SCALE + vec3(TIME * 0.6, TIME * -0.4, TIME * 0.5);
	vec3 p1 = wpos * (TURB_SCALE * 2.3) + vec3(TIME * -0.9, TIME * 0.7, TIME * -0.3) + 19.0;
	return (curl_noise3(p0) + curl_noise3(p1) * 0.5) * TURB_STRENGTH;
}
