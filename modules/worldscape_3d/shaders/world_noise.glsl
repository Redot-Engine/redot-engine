// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

R"(

//INSERT: WORLD_NOISE1
// World Noise
uniform bool world_noise_fragment_normals = false;
uniform float world_noise_region_blend : hint_range(0.05, 0.95, 0.01) = 0.33;
uniform int world_noise_max_octaves : hint_range(0, 15) = 4;
uniform int world_noise_min_octaves : hint_range(0, 15) = 2;
uniform float world_noise_lod_distance : hint_range(0, 40000, 1) = 7500.;
uniform float world_noise_scale : hint_range(0.25, 20, 0.01) = 5.0;
uniform float world_noise_height : hint_range(0, 1000, 0.1) = 64.0;
uniform vec3 world_noise_offset = vec3(0.0);
varying vec2 world_noise_ddxy;

// Takes in UV2 region space coordinates, returns 1.0 or 0.0 if a region is present or not.
float check_region(const vec2 uv2) {
	ivec2 pos = ivec2(floor(uv2)) + (_region_map_size / 2);
	int layer_index = 0;
	if (uint(pos.x | pos.y) < uint(_region_map_size)) {
		layer_index = clamp(_region_map[ pos.y * _region_map_size + pos.x ] - 1, -1, 0) + 1;
	}
	return float(layer_index);
}

// Takes in UV2 region space coordinates, returns a blend value (0 - 1 range) between empty, and valid regions
float region_blend(vec2 uv2) {
	uv2 -= 0.5;
	const vec2 offset = vec2(0.0,1.0);
	float a = check_region(uv2 + offset.xy);
	float b = check_region(uv2 + offset.yy);
	float c = check_region(uv2 + offset.yx);
	float d = check_region(uv2 + offset.xx);
	vec2 w = smoothstep(vec2(0.0), vec2(1.0), fract(uv2));
	float blend = mix(mix(d, c, w.x), mix(a, b, w.x), w.y);
    return 1.0 - blend;
}

float hashf(float f) {
	return fract(sin(f) * 1e4);
}

float hashv2(vec2 v) {
	return fract(1e4 * sin(fma(17.0, v.x, v.y * 0.1)) * (0.1 + abs(sin(fma(v.y, 13.0, v.x)))));
}

// https://iquilezles.org/articles/morenoise/
vec3 noise2D(vec2 x) {
    vec2 f = fract(x);
    // Quintic Hermine Curve.  Similar to SmoothStep()
	vec2 f2 = f * f, f3 = f2 * f, s = f - 1.0, s2 = s * s;
	vec2 u = f3 * fma(vec2(6.0), f2, fma(vec2(-15.0), f, vec2(10.0)));
	vec2 du = 30.0 * f2 * s2;

    vec2 p = floor(x);

	// Four corners in 2D of a tile
	float a = hashv2( p+vec2(0,0) );
    float b = hashv2( p+vec2(1,0) );
    float c = hashv2( p+vec2(0,1) );
    float d = hashv2( p+vec2(1,1) );

    // Mix 4 corner percentages
    float k0 =   a;
    float k1 =   b - a;
    float k2 =   c - a;
    float k3 =   d - (b + k2);
	return vec3(fma(k2, u.y, fma(u.x, fma(k3, u.y, k1), k0)),
		 du * fma(vec2(k3), u.yx, vec2(k1, k2)));
}

float world_noise(vec2 p) {
    float a = 0.0;
    float b = 1.0;
    vec2  d = vec2(0.0);

    int octaves = int( clamp(
		float(world_noise_max_octaves) - floor(v_vertex_xz_dist/(world_noise_lod_distance)),
		float(world_noise_min_octaves), float(world_noise_max_octaves))
	);

    for( int i=0; i < octaves; i++ ) {
        vec3 n = noise2D(p);
        d += n.yz;
        a += b * n.x / (1.0 + dot(d,d));
        b *= 0.5;
        p = mat2( vec2(0.8, -0.6), vec2(0.6, 0.8) ) * p * 2.0;
    }
    return a;
}

float get_noise_height(const vec2 uv) {
	float weight = region_blend(uv);
	// only calculate world noise when it could be visible.
	if (weight <= 1.0 - world_noise_region_blend) {
		return 0.0;
	}
	//TODO: Offset/scale UVs are semi-dependent upon region size 1024. Base on v_vertex.xz instead
	float noise = world_noise((uv + world_noise_offset.xz * 1024. / _region_size) * world_noise_scale * _region_size / 1024. * .1) *
            world_noise_height * 10. + world_noise_offset.y * 100.;
	weight = smoothstep(1.0 - world_noise_region_blend, 1.0, weight);
	return mix(0.0, noise, weight);
}

// World Noise end

//INSERT: WORLD_NOISE2
		// World Noise
		if (_background_mode == 2u) {
			vec2 nuv_a = fma(start_pos, vec2(_region_texel_size), vec2(0.5 * _region_texel_size));
			vec2 nuv_b = fma(end_pos, vec2(_region_texel_size), vec2(0.5 * _region_texel_size));
			float nh = mix(get_noise_height(nuv_a),get_noise_height(nuv_b),vertex_lerp);
			float nu = mix(get_noise_height(nuv_a + vec2(_region_texel_size, 0.0)),
				get_noise_height(nuv_b + vec2(_region_texel_size, 0.0)),vertex_lerp);
			float nv = mix(get_noise_height(nuv_a + vec2(0.0, _region_texel_size)),
				get_noise_height(nuv_b + vec2(0.0, _region_texel_size)),vertex_lerp);
			world_noise_ddxy = vec2(nh - nu, nh - nv);
			h += nh;
		}

//INSERT: WORLD_NOISE3
	// World Noise
	if (_background_mode == 2u && world_noise_fragment_normals) {
		float noise_height = get_noise_height(uv2);
		u += noise_height - get_noise_height(uv2 + vec2(_region_texel_size, 0.0));
		v += noise_height - get_noise_height(uv2 + vec2(0.0, _region_texel_size));
	}
	if (_background_mode == 2u && !world_noise_fragment_normals) {
		u += world_noise_ddxy.x;
		v += world_noise_ddxy.y;
	}
)"
