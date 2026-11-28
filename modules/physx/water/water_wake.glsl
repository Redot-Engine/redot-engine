#[compute]
#version 450

// Moving wake grid (PhysXWaterWake3D): a small ripple simulation that
// travels with a boat. The grid is fixed to the water, not the boat: when
// the boat has moved whole cells, each step reads its state from `shift`
// cells over (the window slides; the waves stay where they were made), with
// still water entering at the leading edge. Same damped wave equation as the
// shore ripple grid (water_ripple.glsl), at one wave speed, with an
// absorbing band around the border so waves fade out instead of reflecting
// back at the boat. Sources pull the surface down toward a hull-shaped dip.
//
// Foam rides along in the same state: churned in by the sources (a bow
// wave, a propeller) and by steep wake waves, fading over its persistence.
// It stays where it was made on the water, like the waves.

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, std140) uniform Params {
	ivec4 grid; // x cells per side, y source count, zw shift (cells, this step)
	vec4 wave; // x wave coefficient c^2 dt^2 / dx^2, y damping * dt, z border damping * dt, w border width (cells)
	vec4 cell; // x cell size (m), y source pull rate (0..1 per step), zw origin (world xz of cell 0's corner)
	vec4 foam; // x foam kept per step, y dt (s), z wave slope that starts foaming, w foam per second per unit of slope past it
	vec4 track; // world xz of the hull's bow (xy) and stern (zw): foam is pushed out from this line
	vec4 spread; // x outward push speed (m/s), y distance it fades out over (m), z hull half-beam (m), w hull foam per second at the bow
};

layout(set = 0, binding = 1, std430) restrict readonly buffer StateIn {
	vec4 state_in[]; // h, h_prev, hull foam (pushed out sideways), propeller foam (stays put)
};
layout(set = 0, binding = 2, std430) restrict writeonly buffer StateOut {
	vec4 state_out[];
};
// Two vec4 per source: (world x, world z, radius, depth m), (foam per second, drifts (1) or stays put (0), unused...).
layout(set = 0, binding = 3, std430) restrict readonly buffer Sources {
	vec4 sources[];
};
layout(set = 0, binding = 4, rgba16f) uniform restrict writeonly image2D height_tex; // r height (m), g hull foam, b propeller foam (0..1)

int n() {
	return grid.x;
}

vec4 read_state(ivec2 c) {
	// Shifted read: this step's cell c was cell c + shift last step.
	ivec2 s = c + grid.zw;
	if (s.x < 0 || s.y < 0 || s.x >= n() || s.y >= n()) {
		return vec4(0.0); // still water entering the window
	}
	return state_in[s.y * n() + s.x];
}

// Foam at a fractional cell position (last step's state, shifted), bilinear.
float read_foam(vec2 p) {
	vec2 f = p - 0.5;
	ivec2 i = ivec2(floor(f));
	vec2 t = f - vec2(i);
	float a = mix(read_state(i).z, read_state(i + ivec2(1, 0)).z, t.x);
	float b = mix(read_state(i + ivec2(0, 1)).z, read_state(i + ivec2(1, 1)).z, t.x);
	return mix(a, b, t.y);
}

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	if (c.x >= n() || c.y >= n()) {
		return;
	}
	vec4 st = read_state(c);
	float h = st.x;
	float hp = st.y;
	// Neighbors; the grid edge reads still water (the absorbing band has
	// already taken the waves out by then).
	float hr = read_state(c + ivec2(1, 0)).x;
	float hl = read_state(c - ivec2(1, 0)).x;
	float hu = read_state(c + ivec2(0, 1)).x;
	float hd = read_state(c - ivec2(0, 1)).x;
	float flux = wave.x * (hr + hl + hu + hd - 4.0 * h);

	// Absorbing border: damping ramps up toward the edge.
	int edge = min(min(c.x, c.y), min(n() - 1 - c.x, n() - 1 - c.y));
	float b = clamp(1.0 - float(edge) / max(wave.w, 1.0), 0.0, 1.0);
	float a = min(wave.y + wave.z * b * b, 0.9);
	float hn = h * (2.0 - a) - hp * (1.0 - a) + flux;

	// Sources: a soft hull-shaped dip each source pulls the surface toward,
	// weighted so the pull vanishes away from them (see water_ripple.glsl
	// for why the weight matters), and the foam they churn in.
	vec2 xz = cell.zw + (vec2(c) + 0.5) * cell.x;
	float target = 0.0;
	float weight = 0.0;
	// Where this cell is along the hull's line (0 bow, 1 stern, past 1 behind
	// it) and its sideways offset from that line, extended behind the boat.
	vec2 ab = track.zw - track.xy;
	float tl = dot(xz - track.xy, ab) / max(dot(ab, ab), 1e-6);
	vec2 away = xz - (track.xy + ab * tl);
	float dist = length(away);
	// The hull pushes the water it displaces aside and churns foam into it
	// along its sides: a band just outside the hull from bow to stern,
	// heaviest at the bow. None ahead of the bow, behind the stern or under
	// the hull (foam left there would stay on the water once the hull had
	// passed), which leaves the water between the two side bands clear; the
	// propeller's band runs down the middle on its own.
	float beside = smoothstep(-0.1, 0.05, tl) * (1.0 - smoothstep(0.85, 1.0, tl));
	float band = exp(-pow((dist - 1.15 * spread.z) / max(0.3 * spread.z, 0.05), 2.0));
	float hull_foam = spread.w * mix(1.0, 0.3, clamp(tl, 0.0, 1.0)) * beside * band * smoothstep(0.8 * spread.z, spread.z, dist);
	// Foam is pushed out sideways from the hull's line, strongest near it:
	// read it from where it was a moment ago, nearer the line. Sideways
	// only, so it peels off in two bands instead of fanning out behind.
	float f = st.z;
	if (spread.x > 0.0) {
		if (dist > 1e-3) {
			float push = spread.x * foam.y * exp(-dist / max(spread.y, 0.1));
			vec2 from_xz = xz - away / dist * push;
			f = read_foam((from_xz - cell.zw) / cell.x);
		}
	}
	f = f * foam.x + hull_foam * foam.y;
	float pf = st.w * foam.x; // propeller foam: not pushed
	float dip = 0.0; // the hull's deepest dip this step (sets how high a crest may stand)
	for (int i = 0; i < grid.y; i++) {
		vec4 s = sources[i * 2];
		dip = max(dip, s.w);
		float t = length(xz - s.xy) / max(s.z, 1e-4);
		if (t < 2.2) {
			float w = exp(-pow(t * 1.1, 6.0));
			if (s.w > 0.0) {
				target = min(target, -s.w * w);
				weight = max(weight, w);
			} else {
				// Foam-only (a propeller, landed spray): churned in across a
				// disk, no dip -- either drifting out with the hull's foam or
				// staying put.
				vec4 sf = sources[i * 2 + 1];
				if (sf.y > 0.5) {
					f += sf.x * w * foam.y;
				} else {
					pf += sf.x * w * foam.y;
				}
			}
		}
	}
	hn += (target - hn) * weight * cell.y;

	// Breaking: a wave can't stand more than a few hull dips high -- past that it
	// breaks, its height going into foam. The sim is otherwise linear, and a hull
	// circling at speed focuses its wake onto the circle's center lap after lap
	// (a jetski piled a +2 m spike there).
	float crest_limit = max(dip * 4.0, 0.3);
	float over = abs(hn) - crest_limit;
	if (over > 0.0) {
		f += over;
		hn = sign(hn) * (crest_limit + over * 0.25);
	}

	// Steep wake waves break into foam.
	float slope = length(vec2(hr - hl, hu - hd)) / (2.0 * cell.x);
	f += max(slope - foam.z, 0.0) * foam.w * foam.y;
	f = clamp(f, 0.0, 1.0) * (1.0 - b); // none in the absorbing band
	pf = clamp(pf, 0.0, 1.0) * (1.0 - b);

	state_out[c.y * n() + c.x] = vec4(hn, h, f, pf);
	imageStore(height_tex, c, vec4(hn, f, pf, 0.0));
}
