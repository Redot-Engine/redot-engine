#[compute]
#version 450

// Shore foam: the milky, foamy sheet where waves wash in over the shallows.
// Lives on the ripple grid (fixed to the ground, unlike the ocean foam that
// rides with the water). Fed where the still water is shallower than the
// band depth -- strongest at the waterline -- whenever an arriving crest
// raises the surface there, so it pulses with each wave, then fades.
//
// It also moves with the water: along the seabed slope at the shallow-water
// particle velocity u = eta * sqrt(g / d), eta capped by depth-limited
// breaking -- shoreward under a crest,
// seaward under a through -- minus a steady undertow, so each wave pushes the
// sheet up the beach and the backwash drags it back out into the water.
// Semi-Lagrangian: each cell pulls its foam from upstream in src and writes
// dst; a second dispatch in copy mode (res.z == 1) copies dst back to src.
//
// Driven by the ocean height *before* the shallow-water fade: the rendered
// chop dies out at the waterline, but the waves still arrive there.
//
// Past the still-water line it lives on the swash (water_swash.glsl, last
// step's run-up): sand the swash covers counts as water as deep as the film
// over it, so the sheet rides up with the uprush instead of stopping at the
// still-water line.

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 foam; // x decay factor this step, y band depth (m), z crest rise for full foam (m, injection starts at 30% of it), w water_level
	vec4 domains; // xy ripple domain size, zw ocean domain size
	vec4 flow; // x dt, y ripple cell size (m), z undertow (m/s, seaward), w gravity
	ivec4 res; // x = ripple grid n, y = 1 if shore foam is enabled, z = 1 for copy mode, w unused
};
layout(set = 0, binding = 1) uniform sampler2D cell_depth_tex;
layout(set = 0, binding = 2) uniform sampler2D ripple_height_tex;
layout(set = 0, binding = 3) uniform sampler2D ocean_height_tex;
layout(set = 0, binding = 4, r32f) uniform restrict readonly image2D src_tex;
layout(set = 0, binding = 5, r32f) uniform restrict writeonly image2D dst_tex;
layout(set = 0, binding = 6) uniform sampler2D swash_tex;

float src_bilinear(vec2 p, int n) {
	p = clamp(p, vec2(0.0), vec2(float(n - 1)));
	ivec2 i0 = ivec2(floor(p));
	ivec2 i1 = min(i0 + 1, ivec2(n - 1));
	vec2 f = p - vec2(i0);
	float a = imageLoad(src_tex, i0).r;
	float b = imageLoad(src_tex, ivec2(i1.x, i0.y)).r;
	float c = imageLoad(src_tex, ivec2(i0.x, i1.y)).r;
	float d = imageLoad(src_tex, i1).r;
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Depth for the slope estimate (negative on dry land, which keeps the slope
// going up the beach); a wall neighbor counts as this cell's own depth so
// the sentinel can't blow the gradient up.
float slope_depth(ivec2 p, float fallback, int n) {
	float d = texelFetch(cell_depth_tex, clamp(p, ivec2(0), ivec2(n - 1)), 0).r;
	return d < -5000.0 ? fallback : d;
}

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	int n = res.x;
	if (c.x >= n || c.y >= n) {
		return;
	}
	if (res.z == 1) {
		imageStore(dst_tex, c, imageLoad(src_tex, c));
		return;
	}
	float still_depth = texelFetch(cell_depth_tex, c, 0).r;
	// Water here: the still water, or the swash film over dry sand.
	float depth = still_depth > 0.0 ? still_depth : still_depth + texelFetch(swash_tex, c, 0).r;
	if (res.y == 0 || still_depth < -5000.0 || depth <= 0.0) {
		imageStore(dst_tex, c, vec4(0.0));
		return;
	}

	// Surface rise here: ripples plus the (unfaded) ocean waves arriving.
	vec2 rel = ((vec2(c) + 0.5) / float(n) - 0.5) * domains.xy;
	vec2 uv_o = clamp(rel / domains.zw + 0.5, vec2(0.0), vec2(1.0));
	float rise = texelFetch(ripple_height_tex, c, 0).r - foam.w + texture(ocean_height_tex, uv_o).r;

	// Shoreward direction: down the depth gradient.
	vec2 grad = vec2(slope_depth(c + ivec2(1, 0), still_depth, n) - slope_depth(c - ivec2(1, 0), still_depth, n),
			slope_depth(c + ivec2(0, 1), still_depth, n) - slope_depth(c - ivec2(0, 1), still_depth, n));
	vec2 shoreward = length(grad) > 1e-4 ? -normalize(grad) : vec2(0.0);
	// Surf is depth-limited: a wave can't stand much taller than ~0.78 x the
	// water depth before it breaks, so cap the local amplitude at 0.4 d.
	// Without it the open-sea chop drove u to the clamp every frame and the
	// undertow never mattered.
	float eta = clamp(rise, -0.4 * depth, 0.4 * depth);
	float u = clamp(eta * sqrt(flow.w / max(depth, 0.05)) - flow.z, -1.5, 1.5);
	vec2 upstream = vec2(c) - shoreward * u * flow.x / max(flow.y, 1e-3);
	float carried = src_bilinear(upstream, n) * foam.x;
	if (still_depth <= 0.0) {
		// Foam pops faster on the sand.
		carried *= exp(-flow.x / 1.5);
	}

	float inject = 0.0;
	if (depth < foam.y) {
		// Full at the still-water line, thinning seaward over the band and
		// landward up the swash, so the sheet carries on onto the sand and
		// fades out up the uprush instead of stopping at a line.
		float band = still_depth > 0.0 ? 1.0 - smoothstep(0.0, foam.y, still_depth) : 1.0 - smoothstep(0.0, 0.3, -still_depth);
		// Only real crests feed it, so the sheet pulses with each wave instead
		// of sitting there as a constant stripe.
		inject = band * smoothstep(0.3 * foam.z, max(foam.z, 1e-3), rise);
	}
	imageStore(dst_tex, c, vec4(max(carried, inject), 0.0, 0.0, 0.0));
}
