#[compute]
#version 450

#include "gas_block_inc.glsl"

// Binding 5 = read (this frame's "current" grid), binding 6 = write (the
// other one), binding 7 = curl field already computed this frame by
// gas_bs_curl from the same binding-5 buffer.

// Boundless-within-the-box lookup used for the backtrace sample and the
// 6-neighbour diffusion taps -- out-of-box or unallocated cells read as zero
// (open boundary, matches a plume that hasn't reached the box edge yet).
vec4 sample_field(ivec3 c) {
	if (any(lessThan(c, ivec3(0))) || any(greaterThanEqual(c, BOXC))) {
		return vec4(0.0);
	}
	int slot = block_lookup(c >> 2);
	if (slot < 0) {
		return vec4(0.0);
	}
	return data5[slot * BCELLS + lin3(c & 3)];
}

// Trilinear resample at a fractional cell-space position (same -0.5
// cell-centre convention as the MPM path's grid_local). Nearest-cell sampling
// left visible per-cell banding in the prototype's first pass.
vec4 sample_trilinear(vec3 gp) {
	vec3 gp0 = gp - 0.5;
	ivec3 base = ivec3(floor(gp0));
	vec3 f = gp0 - vec3(base);
	vec4 c000 = sample_field(base + ivec3(0, 0, 0));
	vec4 c100 = sample_field(base + ivec3(1, 0, 0));
	vec4 c010 = sample_field(base + ivec3(0, 1, 0));
	vec4 c110 = sample_field(base + ivec3(1, 1, 0));
	vec4 c001 = sample_field(base + ivec3(0, 0, 1));
	vec4 c101 = sample_field(base + ivec3(1, 0, 1));
	vec4 c011 = sample_field(base + ivec3(0, 1, 1));
	vec4 c111 = sample_field(base + ivec3(1, 1, 1));
	vec4 x00 = mix(c000, c100, f.x), x10 = mix(c010, c110, f.x);
	vec4 x01 = mix(c001, c101, f.x), x11 = mix(c011, c111, f.x);
	vec4 y0 = mix(x00, x10, f.y), y1 = mix(x01, x11, f.y);
	return mix(y0, y1, f.z);
}

float curl_mag_at(ivec3 c) {
	if (any(lessThan(c, ivec3(0))) || any(greaterThanEqual(c, BOXC))) {
		return 0.0;
	}
	int slot = block_lookup(c >> 2);
	if (slot < 0) {
		return 0.0;
	}
	return curl_data[slot * BCELLS + lin3(c & 3)].w;
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

	vec3 wpos = ORIGIN + (vec3(c) + 0.5) * CDX;
	vec4 self_v = data5[idx];

	// Pure pull-based (semi-Lagrangian) advection alone can't spread momentum
	// to a cell whose own velocity is still zero -- it just samples itself and
	// stays zero forever, so a plume can never climb past the cells the
	// source directly touches. Pressure projection (gas_bs_divergence/
	// gas_bs_jacobi/gas_bs_project, run after this pass -- see
	// GasSolver::step) is now what actually gives this solver its spread and
	// its shape-holding coherence; this neighbour-average blend is kept ONLY
	// as a tiny numerical-diffusion floor so a fully-isolated zero-velocity
	// cell still gets touched at all. Cut from 0.12 to 0.02: at the higher
	// value this term was actively erasing the small-scale vorticity a real
	// shear-layer roll-up (the actual mechanism a mushroom-cloud cap curls
	// under from) needs to survive more than a step or two -- a pure blur
	// operator fighting the thing projection is supposed to preserve.
	// Curl-noise turbulence: an analytically divergence-free perturbation
	// velocity (see turbulence_velocity()'s header comment) added on top of
	// the real solved velocity for the backtrace ONLY -- this fakes the
	// fine "billowing cauliflower" detail a coarse real-time grid can't
	// resolve, without disturbing new_vel itself (which stays the real,
	// physically-driven field vorticity confinement/projection work with).
	vec3 turb = turbulence_velocity(wpos);
	vec3 back_gp = (wpos - (self_v.xyz + turb) * DT - ORIGIN) / CDX;
	vec4 advected = sample_trilinear(back_gp);

	vec4 nsum = sample_field(c + ivec3(1, 0, 0)) + sample_field(c - ivec3(1, 0, 0)) +
			sample_field(c + ivec3(0, 1, 0)) + sample_field(c - ivec3(0, 1, 0)) +
			sample_field(c + ivec3(0, 0, 1)) + sample_field(c - ivec3(0, 0, 1));
	vec4 nbr_avg = nsum * (1.0 / 6.0);

	const float DIFFUSE = 0.02;
	vec4 blended = mix(advected, nbr_avg, DIFFUSE);

	vec3 new_vel = blended.xyz;
	float new_dens = blended.w;

	new_vel.y += BUOY * new_dens * DT;

	// Real vorticity confinement (Fedkiw et al.): eta = normalize(grad|curl|),
	// force = VORT * dx * cross(eta, curl) -- pumps energy back into small
	// eddies a coarse grid's numerical diffusion would otherwise smear away.
	vec4 self_curl = curl_data[idx];
	vec3 grad;
	grad.x = curl_mag_at(c + ivec3(1, 0, 0)) - curl_mag_at(c - ivec3(1, 0, 0));
	grad.y = curl_mag_at(c + ivec3(0, 1, 0)) - curl_mag_at(c - ivec3(0, 1, 0));
	grad.z = curl_mag_at(c + ivec3(0, 0, 1)) - curl_mag_at(c - ivec3(0, 0, 1));
	float glen = length(grad);
	if (glen > 1e-6) {
		vec3 eta = grad / glen;
		new_vel += VORT * CDX * cross(eta, self_curl.xyz) * DT;
	}

	new_dens *= DISS;
	new_vel *= 0.998;

	// Up to MAX_GAS_EMITTERS independent sphere/box injection points -- lets a
	// scene place several differently-shaped sources to sculpt a composite
	// plume. Per-emitter jitter is decorrelated by folding the emitter index
	// into the hash input (else simultaneously-active emitters would jitter
	// in lockstep).
	for (int ei = 0; ei < MAX_GAS_EMITTERS; ei++) {
		int eshape = int(emitter_pos_shape[ei].w);
		if (eshape < 0) {
			continue; // disabled slot
		}
		vec3 epos = emitter_pos_shape[ei].xyz;
		vec3 esize = emitter_size_density[ei].xyz;
		bool inside;
		if (eshape == EMITTER_BOX) {
			// Axis-aligned only (v1) -- no inverse-rotation transform yet, see
			// the header for the scope note.
			vec3 d = abs(wpos - epos) - esize;
			inside = all(lessThan(d, vec3(0.0)));
		} else {
			inside = distance(wpos, epos) < esize.x;
		}
		if (!inside) {
			continue;
		}
		new_dens = max(new_dens, emitter_size_density[ei].w);
		vec3 evel = emitter_velocity[ei].xyz;

		// Divergence: outward-radial speed from the emitter center, on top of
		// the directional jet -- an explosion/burst wants every cell pushed
		// AWAY FROM CENTER, not all pushed the same direction (Flow's
		// NvFlowEmitterSphereParams.divergence). Swirl: tangential speed
		// around world +Y through the center, for directly authoring rotation
		// (a mushroom-cloud cap curling over needs more coherent rotation than
		// vorticity confinement alone reliably amplifies from incidental
		// jitter).
		float ediv = emitter_extra[ei].x;
		float eswirl = emitter_extra[ei].y;
		vec3 rel = wpos - epos;
		if (ediv != 0.0 || eswirl != 0.0) {
			float rlen = length(rel);
			if (rlen > 1e-5) {
				vec3 radial = rel / rlen;
				vec3 tangent = normalize(cross(vec3(0.0, 1.0, 0.0), radial) + vec3(1e-6));
				evel += radial * ediv + tangent * eswirl;
			}
		}

		vec3 jitter = vec3(
				hash13(vec3(c) + vec3(TIME + float(ei) * 17.0, 0.0, 0.0)) - 0.5,
				0.0,
				hash13(vec3(c) + vec3(0.0, 0.0, TIME + float(ei) * 17.0)) - 0.5);
		new_vel += (evel + jitter * length(evel) * 0.6) * DT;
	}

	// Solid obstacles: voxelized sphere/box/plane, forced empty+still every
	// step. A cell just outside one still pulls its backtrace/diffusion
	// samples FROM inside (always zero), so the field naturally thins and
	// parts near the surface without any extra boundary-condition code --
	// the same "missing/zero neighbor" mechanism that makes the box edges
	// an open boundary.
	for (int ci = 0; ci < MAX_GAS_COLLIDERS; ci++) {
		if (collider_sdf(ci, wpos) < 0.0) {
			new_vel = vec3(0.0);
			new_dens = 0.0;
			break;
		}
	}

	data6[idx] = vec4(new_vel, new_dens);
}
