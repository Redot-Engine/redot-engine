#[compute]
#version 450

// Character cloth on any GPU (RenderingDevice compute): position-based cloth
// on a region of a skinned mesh. Every particle has an animated target -- its
// linear-blend-skinned rest position -- and a max distance it may stray from
// it (0 = follows the animation exactly). Per frame:
//   MODE_SKIN       targets from this frame's bone matrices (previous kept for
//                   interpolating across substeps).
// Per substep:
//   MODE_INTEGRATE  pinned particles snap to their (interpolated) target, the
//                   rest take a Verlet step under gravity with damping, then
//                   are pulled part of the way back to the target (animation
//                   drive -- keeps the garment's authored shape).
//   MODE_DISTANCE   one colour batch of distance constraints (structural +
//                   bending). Within a batch no two constraints share a
//                   particle, so each thread can write its pair directly.
//   MODE_HASH_CLEAR / MODE_HASH_INSERT / MODE_SELF / MODE_SELF_APPLY
//                   (self collision, opt-in) particles into a spatial hash of
//                   thickness-sized cells, then every pair closer than the
//                   thickness that isn't that close at rest is pushed apart
//                   (Jacobi: each particle computes its own share, then all
//                   apply at once).
//   MODE_LIMITS     tethers (stay within the rest distance of the nearest
//                   pinned particle -- keeps long robes from stretching with
//                   few iterations), the backstop (no further inward, along
//                   the animated normal, than a small distance -- keeps
//                   clothing outside the body), the max-distance sphere
//                   around the animated target, then body capsules
//                   (interpolated across the substeps, so fast limbs don't
//                   jump through the cloth; last, so they win over the
//                   max distance).
// Once per frame after the substeps:
//   MODE_RESET      (first frame / teleport) particles placed on the animated pose.
//   MODE_OUTPUT     per-particle normal from the incident triangles, and
//                   position + normal written to RGBA32F images the render
//                   material reads (no CPU readback).

layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

#define MODE_SKIN 0
#define MODE_INTEGRATE 1
#define MODE_DISTANCE 2
#define MODE_LIMITS 3
#define MODE_OUTPUT 4
#define MODE_RESET 5
#define MODE_HASH_CLEAR 6
#define MODE_HASH_INSERT 7
#define MODE_SELF 8
#define MODE_SELF_APPLY 9

layout(set = 0, binding = 0, std140) uniform Params {
	ivec4 counts; // x particles, y self-collision hash size (power of two), z substeps, w unused
	ivec4 counts2; // x capsules, y texture width, z bones, w unused
	vec4 step; // x dt (substep), y damping (velocity kept per substep), z animation drive (fraction pulled to the target per substep), w stiffness
	vec4 gravity; // xyz gravity (m/s^2), w thickness (m)
	vec4 limits; // x tether slack (allowed stretch, e.g. 1.03), y tether stiffness, z capsule friction, w backstop distance (m, < 0 = off)
	// World -> source mesh local space (rows of a 3x4 affine transform). The
	// solver works in world space; the output images are in the source mesh's
	// space so the cloth renders through the same (possibly interpolated)
	// transform as the mesh it replaces.
	vec4 out_x;
	vec4 out_y;
	vec4 out_z;
	vec4 self_params; // x self-collision thickness (m), 0 = off
};

// Per dispatch (push constants -- the uniform buffer can't change between
// dispatches inside one compute list).
layout(push_constant, std430) uniform Pass {
	int mode;
	int batch_start;
	int batch_count;
	float frac; // substep fraction, for interpolating the animated targets
}
pass;

layout(set = 0, binding = 1, std430) restrict readonly buffer Rest {
	vec4 rest[]; // xyz bind-space position, w max distance (m)
};
layout(set = 0, binding = 2, std430) restrict readonly buffer SkinIdx {
	uvec4 skin_idx[];
};
layout(set = 0, binding = 3, std430) restrict readonly buffer SkinW {
	vec4 skin_w[];
};
layout(set = 0, binding = 4, std430) restrict readonly buffer Bones {
	mat4 bones[]; // world-space skinning matrices (world * bone global * bind)
};
layout(set = 0, binding = 5, std430) restrict buffer Target {
	vec4 target[];
};
layout(set = 0, binding = 6, std430) restrict buffer TargetPrev {
	vec4 target_prev[];
};
layout(set = 0, binding = 7, std430) restrict buffer Pos {
	vec4 pos[]; // xyz world position, w inverse mass (0 = pinned)
};
layout(set = 0, binding = 8, std430) restrict buffer Prev {
	vec4 prev[];
};
layout(set = 0, binding = 9, std430) restrict readonly buffer Constraints {
	vec4 constraints[]; // x,y particle indices (as float bits), z rest length, w stiffness 0..1
};
layout(set = 0, binding = 10, std430) restrict readonly buffer Tethers {
	vec2 tethers[]; // x anchor particle (float bits; ~0 = none), y rest distance
};
layout(set = 0, binding = 11, std430) restrict readonly buffer Capsules {
	vec4 capsules[]; // per capsule: previous frame (a.xyz, ra), (b.xyz, rb), then this frame's
};
layout(set = 0, binding = 12, std430) restrict readonly buffer AdjOffsets {
	uint adj_offsets[]; // particle -> start in adj_pairs, count = next - this
};
layout(set = 0, binding = 13, std430) restrict readonly buffer AdjPairs {
	uvec2 adj_pairs[]; // the other two corners of each incident triangle, winding order
};
layout(set = 0, binding = 14, rgba32f) uniform restrict writeonly image2D pos_image;
layout(set = 0, binding = 15, rgba32f) uniform restrict writeonly image2D nrm_image;
layout(set = 0, binding = 16, std430) restrict readonly buffer RestNormal {
	vec4 rest_normal[]; // bind-space normal
};
layout(set = 0, binding = 17, std430) restrict buffer TargetNormal {
	vec4 target_normal[]; // skinned normal, this frame
};
// Self collision: spatial hash as per-cell linked lists of particles.
layout(set = 0, binding = 18, std430) restrict buffer CellHead {
	uint cell_head[];
};
layout(set = 0, binding = 19, std430) restrict buffer CellNext {
	uint cell_next[];
};
layout(set = 0, binding = 20, std430) restrict buffer SelfDelta {
	vec4 self_delta[];
};

uint cell_hash(ivec3 c) {
	return (uint(c.x * 73856093) ^ uint(c.y * 19349663) ^ uint(c.z * 83492791)) & uint(counts.y - 1);
}

mat4 skin_matrix(uint i) {
	uvec4 b = skin_idx[i];
	vec4 w = skin_w[i];
	return bones[b.x] * w.x + bones[b.y] * w.y + bones[b.z] * w.z + bones[b.w] * w.w;
}

vec3 skin(uint i) {
	return (skin_matrix(i) * vec4(rest[i].xyz, 1.0)).xyz;
}

vec3 skin_normal(uint i) {
	vec3 n = mat3(skin_matrix(i)) * rest_normal[i].xyz;
	float l = length(n);
	return l > 1e-8 ? n / l : vec3(0.0, 1.0, 0.0);
}

// Closest points between segments p1-q1 and p2-q2 (Ericson, Real-Time
// Collision Detection 5.1.9), as the parameters along each.
vec2 closest_segments(vec3 p1, vec3 q1, vec3 p2, vec3 q2) {
	vec3 d1 = q1 - p1;
	vec3 d2 = q2 - p2;
	vec3 r = p1 - p2;
	float a = dot(d1, d1);
	float e = dot(d2, d2);
	float f = dot(d2, r);
	float s;
	float t;
	if (a <= 1e-12) {
		s = 0.0;
		t = e > 1e-12 ? clamp(f / e, 0.0, 1.0) : 0.0;
		return vec2(s, t);
	}
	float c = dot(d1, r);
	if (e <= 1e-12) {
		return vec2(clamp(-c / a, 0.0, 1.0), 0.0);
	}
	float b = dot(d1, d2);
	float denom = a * e - b * b;
	s = denom > 1e-12 ? clamp((b * f - c * e) / denom, 0.0, 1.0) : 0.0;
	t = (b * s + f) / e;
	if (t < 0.0) {
		t = 0.0;
		s = clamp(-c / a, 0.0, 1.0);
	} else if (t > 1.0) {
		t = 1.0;
		s = clamp((b - c) / a, 0.0, 1.0);
	}
	return vec2(s, t);
}

// Keeps p outside the body capsules. Swept from prev_p (where the particle
// was at the start of the substep): a particle whose path this substep
// crossed a capsule -- carried past a limb by the max-distance limit or by
// the limb itself moving -- is stopped at the surface on the side it came
// from, instead of ending up through the limb.
void project_capsules(inout vec3 p, vec3 prev_p, float frac) {
	float thickness = gravity.w;
	for (int c = 0; c < counts2.x; c++) {
		vec4 a = mix(capsules[c * 4], capsules[c * 4 + 2], frac);
		vec4 b = mix(capsules[c * 4 + 1], capsules[c * 4 + 3], frac);
		vec3 ab = b.xyz - a.xyz;
		float len2 = max(dot(ab, ab), 1e-8);
		// Where prev_p sits relative to this capsule.
		float tp = clamp(dot(prev_p - a.xyz, ab) / len2, 0.0, 1.0);
		vec3 qp = a.xyz + ab * tp;
		vec3 dp = prev_p - qp;
		float dpl = length(dp);
		float rp = mix(a.w, b.w, tp) + thickness;
		vec2 st = closest_segments(prev_p, p, a.xyz, b.xyz);
		vec3 on_path = mix(prev_p, p, st.x);
		vec3 on_axis = a.xyz + ab * st.y;
		float r_path = mix(a.w, b.w, st.y) + thickness;
		vec3 n;
		vec3 q;
		float r;
		if (dpl >= rp * 0.98 && distance(on_path, on_axis) < r_path) {
			// Came from outside and the path crosses the capsule: stop on the
			// side it came from.
			float tq = clamp(dot(p - a.xyz, ab) / len2, 0.0, 1.0);
			q = a.xyz + ab * tq;
			r = mix(a.w, b.w, tq) + thickness;
			vec3 side = dp - ab * (dot(dp, ab) / len2);
			float sl = length(side);
			n = sl > 1e-6 ? side / sl : dp / max(dpl, 1e-6);
			vec3 d = p - q;
			// Keep the tangential part of the motion, drop what went inward.
			vec3 tangential = d - n * dot(d, n);
			p = q + tangential + n * r;
			vec3 dd = p - q;
			float ddl = length(dd);
			if (ddl < r) {
				p = q + (ddl > 1e-6 ? dd / ddl : n) * r;
			}
		} else {
			float t = clamp(dot(p - a.xyz, ab) / len2, 0.0, 1.0);
			q = a.xyz + ab * t;
			r = mix(a.w, b.w, t) + thickness;
			vec3 d = p - q;
			float dist = length(d);
			if (dist >= r) {
				continue;
			}
			n = dist > 1e-6 ? d / dist : vec3(0.0, 1.0, 0.0);
			p = q + n * r;
		}
		// Friction: bleed off tangential motion against the capsule.
		vec3 v = p - prev_p;
		vec3 vt = v - n * dot(v, n);
		p -= vt * limits.z;
	}
}

void main() {
	uint i = gl_GlobalInvocationID.x;
	int mode = pass.mode;

	if (mode == MODE_DISTANCE) {
		if (i >= uint(pass.batch_count)) {
			return;
		}
		vec4 c = constraints[uint(pass.batch_start) + i];
		uint a = floatBitsToUint(c.x);
		uint b = floatBitsToUint(c.y);
		vec4 pa = pos[a];
		vec4 pb = pos[b];
		float wsum = pa.w + pb.w;
		if (wsum <= 0.0) {
			return;
		}
		vec3 d = pb.xyz - pa.xyz;
		float len = length(d);
		if (len < 1e-7) {
			return;
		}
		vec3 corr = d * ((len - c.z) / (len * wsum)) * c.w * step.w;
		vec3 qa = pa.xyz + corr * pa.w;
		vec3 qb = pb.xyz - corr * pb.w;
		// Edge collision (structural edges): keep the fabric between two
		// particles out of the body capsules too, not just the particles --
		// otherwise a round limb rolls neighboring particles aside and
		// passes between them.
		if (c.w >= 0.999) {
			for (int k = 0; k < counts2.x; k++) {
				vec4 ca = mix(capsules[k * 4], capsules[k * 4 + 2], pass.frac);
				vec4 cb = mix(capsules[k * 4 + 1], capsules[k * 4 + 3], pass.frac);
				vec2 st = closest_segments(qa, qb, ca.xyz, cb.xyz);
				vec3 on_edge = mix(qa, qb, st.x);
				vec3 on_axis = mix(ca.xyz, cb.xyz, st.y);
				float r = mix(ca.w, cb.w, st.y) + gravity.w;
				vec3 dn = on_edge - on_axis;
				float dl = length(dn);
				if (dl >= r || dl < 1e-6) {
					continue;
				}
				vec3 n = dn / dl;
				// Push the edge point out, shared by the ends by barycentric
				// weight and inverse mass.
				float wa = (1.0 - st.x) * pa.w;
				float wb = st.x * pb.w;
				float denom = (1.0 - st.x) * wa + st.x * wb;
				if (denom < 1e-6) {
					continue;
				}
				float lambda = (r - dl) / denom;
				qa += n * (lambda * wa);
				qb += n * (lambda * wb);
			}
		}
		pos[a].xyz = qa;
		pos[b].xyz = qb;
		return;
	}

	if (mode == MODE_HASH_CLEAR) {
		if (i < uint(counts.y)) {
			cell_head[i] = 0xffffffffu;
		}
		return;
	}

	if (i >= uint(counts.x)) {
		return;
	}

	if (mode == MODE_HASH_INSERT) {
		ivec3 c = ivec3(floor(pos[i].xyz / self_params.x));
		cell_next[i] = atomicExchange(cell_head[cell_hash(c)], i);
		return;
	}

	if (mode == MODE_SELF) {
		vec4 pi = pos[i];
		vec3 acc = vec3(0.0);
		if (pi.w > 0.0) {
			float h = self_params.x;
			vec3 ri = rest[i].xyz;
			ivec3 c = ivec3(floor(pi.xyz / h));
			for (int dz = -1; dz <= 1; dz++) {
				for (int dy = -1; dy <= 1; dy++) {
					for (int dx = -1; dx <= 1; dx++) {
						uint j = cell_head[cell_hash(c + ivec3(dx, dy, dz))];
						for (int guard = 0; guard < 48 && j != 0xffffffffu; guard++) {
							if (j != i) {
								vec4 pj = pos[j];
								vec3 d = pi.xyz - pj.xyz;
								float l = length(d);
								// Skip pairs that are this close in the garment
								// itself (neighbors along the surface).
								if (l < h && l > 1e-7 && distance(ri, rest[j].xyz) > h * 1.5) {
									acc += d / l * ((h - l) * pi.w / (pi.w + pj.w));
								}
							}
							j = cell_next[j];
						}
					}
				}
			}
		}
		self_delta[i] = vec4(acc * 0.5, 0.0); // relaxed: many pairs push at once
		return;
	}

	if (mode == MODE_SELF_APPLY) {
		if (pos[i].w > 0.0) {
			pos[i].xyz += self_delta[i].xyz;
		}
		return;
	}

	if (mode == MODE_SKIN) {
		target_prev[i] = target[i];
		target[i] = vec4(skin(i), 0.0);
		target_normal[i] = vec4(skin_normal(i), 0.0);
		return;
	}

	if (mode == MODE_RESET) {
		// First frame (or a teleport): start at rest on the animated pose.
		vec4 tt = vec4(skin(i), 0.0);
		target[i] = tt;
		target_normal[i] = vec4(skin_normal(i), 0.0);
		target_prev[i] = tt;
		pos[i] = vec4(tt.xyz, rest[i].w > 0.0 ? 1.0 : 0.0);
		prev[i] = tt;
		return;
	}

	vec3 t = mix(target_prev[i].xyz, target[i].xyz, pass.frac);
	float max_dist = rest[i].w;

	if (mode == MODE_INTEGRATE) {
		vec4 p = pos[i];
		if (max_dist <= 0.0) {
			prev[i] = vec4(p.xyz, 0.0);
			pos[i] = vec4(t, 0.0);
			return;
		}
		// Damping acts on the motion relative to the animated garment, not
		// the world: running doesn't blow the cloth back like a headwind,
		// swings and jiggle still settle.
		vec3 t_last = mix(target_prev[i].xyz, target[i].xyz, max(pass.frac - 1.0 / float(max(counts.z, 1)), 0.0));
		vec3 v_anim = t - t_last;
		vec3 v = v_anim + ((p.xyz - prev[i].xyz) - v_anim) * step.y;
		prev[i] = vec4(p.xyz, 0.0);
		vec3 np = p.xyz + v + gravity.xyz * step.x * step.x;
		pos[i] = vec4(mix(np, t, step.z), p.w);
		return;
	}

	if (mode == MODE_LIMITS) {
		vec4 p = pos[i];
		if (p.w <= 0.0) {
			return;
		}
		vec3 q = p.xyz;
		vec2 te = tethers[i];
		uint anchor = floatBitsToUint(te.x);
		if (anchor != 0xffffffffu) {
			vec3 ap = pos[anchor].xyz;
			vec3 d = q - ap;
			float len = length(d);
			float allowed = te.y * limits.x;
			if (len > allowed) {
				q = mix(q, ap + d * (allowed / len), limits.y);
			}
		}
		if (limits.w >= 0.0) {
			vec3 n = target_normal[i].xyz;
			float inward = dot(q - t, n);
			if (inward < -limits.w) {
				q -= n * (inward + limits.w);
			}
		}
		vec3 off = q - t;
		float ol = length(off);
		if (ol > max_dist) {
			q = t + off * (max_dist / ol);
		}
		// Body collision last: where a limb moves further than the painted
		// max distance allows (a leg swinging through a skirt), the cloth goes
		// past the limit rather than letting the limb show through.
		project_capsules(q, prev[i].xyz, pass.frac);
		pos[i].xyz = q;
		return;
	}

	if (mode == MODE_OUTPUT) {
		vec3 p = pos[i].xyz;
		vec3 n = vec3(0.0);
		for (uint k = adj_offsets[i]; k < adj_offsets[i + 1]; k++) {
			uvec2 o = adj_pairs[k];
			n += cross(pos[o.x].xyz - p, pos[o.y].xyz - p);
		}
		float nl = length(n);
		n = nl > 1e-12 ? n / nl : vec3(0.0, 1.0, 0.0);
		vec3 lp = vec3(dot(out_x.xyz, p) + out_x.w, dot(out_y.xyz, p) + out_y.w, dot(out_z.xyz, p) + out_z.w);
		vec3 ln = vec3(dot(out_x.xyz, n), dot(out_y.xyz, n), dot(out_z.xyz, n));
		float lnl = length(ln);
		ln = lnl > 1e-12 ? ln / lnl : vec3(0.0, 1.0, 0.0);
		ivec2 px = ivec2(int(i) % counts2.y, int(i) / counts2.y);
		imageStore(pos_image, px, vec4(lp, 1.0));
		imageStore(nrm_image, px, vec4(ln, 0.0));
		return;
	}
}
