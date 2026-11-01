// Foam advection pass -- one thread per diffuse slot. Each particle is
// re-classified per frame by how much of its containing grid cell is fluid
// (jeantimex's hybrid-FLIP scheme, the grid-based twin of Ihmsen's
// neighbor-count classes), then follows that kind's dynamics:
//
//   spray  (mostly-air cell)  -- ballistic under gravity with quadratic air
//                                drag; fast lifetime decay;
//   foam   (mixed cell)       -- no independent velocity: the position is
//                                advected BY the fluid velocity field (Ihmsen:
//                                foam rides the surface flow);
//   bubble (fully-fluid cell) -- drag toward the fluid velocity plus a capped
//                                buoyant rise; slow lifetime decay.
//
// All kinds get collider push-out (the analytic SDFs the fluid solve already
// uses) so froth banks off hulls instead of flying through them, and the same
// domain kill. Lifetime decays in real seconds; spent slots park at life <= 0
// until the spawn pass recycles them. The alive count feeds the readback so
// the node shows only live foam.
//
// Runs once per frame (frame dt from Params.foam_b.y), after the spawn pass.
#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

#define KIND_SPRAY 0
#define KIND_FOAM 1
#define KIND_BUBBLE 2

// Fluid-node test shared with the old in-fluid drag: a node carries mass from
// any particle whose B-spline footprint touches it.
bool node_is_fluid(ivec3 c) {
	return grid_v[node_index(clamp(c, ivec3(0), RES - 1))].w > 0.25 * PMASS;
}

void main() {
	uint id = gl_GlobalInvocationID.x;
	if (id >= uint(max(FOAM_CAP, 1.0))) {
		return;
	}
	float life = foam[id].pos_life.w;
	if (life <= 0.0) {
		return;
	}
	vec3 p = foam[id].pos_life.xyz;
	vec3 v = foam[id].vel_kind.xyz;

	// Kill foam that escaped the sim domain entirely (it would hang in the air
	// sampling a zero grid forever).
	if (any(lessThan(p, bmin.xyz)) || any(greaterThan(p, bmax.xyz))) {
		foam[id].pos_life.w = 0.0;
		return;
	}

	float dt = max(FRAME_DT, 1e-4);

	// --- classify by fluid coverage of the containing cell ------------------
	// Count the cell's 8 corner nodes carrying fluid (0..8): 0 = in air,
	// 8 = fully enclosed. The two ratios (foam_b.z / foam_b.w) place the
	// spray/foam and foam/bubble cut-offs as fractions of 8, so a designer can
	// widen the foam band without re-tuning mass scales.
	ivec3 base = ivec3(floor((p - ORIGIN) / DX));
	int fluid_corners = 0;
	for (int i = 0; i < 8; i++) {
		ivec3 o = ivec3(i & 1, (i >> 1) & 1, (i >> 2) & 1);
		fluid_corners += node_is_fluid(base + o) ? 1 : 0;
	}
	const float corners = 8.0;
	int kind;
	if (float(fluid_corners) <= FOAM_SPRAY_RATIO * corners) {
		kind = KIND_SPRAY;
	} else if (float(fluid_corners) >= FOAM_BUBBLE_RATIO * corners) {
		kind = KIND_BUBBLE;
	} else {
		kind = KIND_FOAM;
	}

	// --- per-kind dynamics ---------------------------------------------------
	if (kind == KIND_SPRAY) {
		// Ballistic under gravity; quadratic air drag bleeds the launch speed
		// (droplets decelerate hard -- RikkaBunny uses the same |v|^2 form).
		v += GRAV * dt;
		float sp = length(v);
		if (sp > 1e-4) {
			v -= (v / sp) * sp * sp * 0.08 * dt;
		}
		p += v * dt;
		life -= 2.0 * dt;
	} else if (kind == KIND_FOAM) {
		// Foam rides the flow: position-advected by the fluid velocity field,
		// its stored velocity kept in sync for the readback.
		vec3 vf = sample_grid_v(p);
		p += vf * dt;
		v = vf;
		life -= dt;
	} else {
		// Bubbles: drag toward the fluid velocity plus a capped buoyant rise
		// (an uncapped fraction-of-gravity target turned every submerged
		// bubble into a bottle rocket -- the rise speed stays capped).
		vec3 vf = sample_grid_v(p);
		vec3 drift = vf + vec3(0.0, 1.0, 0.0) * min(FOAM_BUOY * 1.5, 1.2);
		v += (drift - v) * (1.0 - exp(-max(FOAM_DRAG, 0.01) * 4.0 * dt));
		p += v * dt;
		life -= 0.5 * dt;
	}

	// --- collider push-out (all kinds) ---------------------------------------
	for (int ci = 0; ci < NCOL; ci++) {
		float sd = collider_sdf(ci, p);
		if (sd < 0.0) {
			vec3 nh = collider_normal(ci, p);
			p += nh * (-sd + 1e-4);
			float vn = dot(v - colliders[ci].c2.xyz, nh);
			if (vn < 0.0) {
				v -= vn * nh;
			}
		}
	}

	if (life <= 0.0) {
		foam[id].pos_life.w = 0.0; // park; the spawn pass recycles the slot
		return;
	}
	foam[id].pos_life = vec4(p, life);
	foam[id].vel_kind = vec4(v, float(kind));
	atomicAdd(foam_meta.y, 1u);
}
