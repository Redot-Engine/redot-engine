// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

R"(

//INSERT: DUAL_SCALING_UNIFORMS
uniform int dual_scale_texture : hint_range(0,31) = 0;
uniform float dual_scale_reduction : hint_range(0.001,1) = 0.3;
uniform float tri_scale_reduction : hint_range(0.001,1) = 0.3;
uniform float dual_scale_far : hint_range(0,1000) = 170.0;
uniform float dual_scale_near : hint_range(0,1000) = 100.0;

//INSERT: DUAL_SCALING
		// tri scaling
		if (index.z < 0) {
			i_dd *= tri_scale_reduction;
			i_uv *= tri_scale_reduction;
			i_pos *= tri_scale_reduction;
		}

	// dual scaling
	float far_factor = clamp(smoothstep(dual_scale_near, dual_scale_far, length(v_vertex - _camera_pos)), 0.0, 1.0);
	vec4 far_alb = vec4(0.);
	vec4 far_nrm = vec4(0.);
	if (far_factor > 0. && any(equal(texture_id, ivec2(dual_scale_texture)))) {
		float far_scale = _texture_uv_scale_array[dual_scale_texture] * dual_scale_reduction;
		vec4 far_dd = i_dd * far_scale;

		// Detiling and Control map rotation
		vec2 uv_center = floor(fma(i_pos, vec2(far_scale), vec2(0.5)));
		vec2 far_detile = fma(random(uv_center), 2.0, -1.0) * _texture_detile_array[dual_scale_texture] * TAU;
		vec2 far_cs_angle = vec2(cos(far_detile.x), sin(far_detile.x));
		// Apply UV rotation and shift around pivot.
		vec2 far_uv = rotate_vec2(fma(i_uv, vec2(far_scale), -uv_center), far_cs_angle) + uv_center + far_detile.y - 0.5;
		// Manual transpose to rotate derivatives and normals counter to uv rotation whilst also
		// including control map rotation. avoids extra matrix op, and sin/cos calls.
		far_cs_angle = vec2(
			fma(far_cs_angle.x, c_cs_angle.x, -far_cs_angle.y * c_cs_angle.y),
			fma(far_cs_angle.y, c_cs_angle.x, far_cs_angle.x * c_cs_angle.y));
		// Align derivatives for correct anisotropic filtering
		far_dd.xy = rotate_vec2(far_dd.xy, far_cs_angle);
		far_dd.zw = rotate_vec2(far_dd.zw, far_cs_angle);

		far_alb = textureGrad(_texture_array_albedo, vec3(far_uv, float(dual_scale_texture)), far_dd.xy, far_dd.zw);
		far_nrm = textureGrad(_texture_array_normal, vec3(far_uv, float(dual_scale_texture)), far_dd.xy, far_dd.zw);
		far_alb.rgb *= _texture_color_array[dual_scale_texture].rgb;
		far_nrm.a = clamp(far_nrm.a + _texture_roughness_mod_array[dual_scale_texture], 0., 1.);
		// Unpack and rotate normal map.
		far_nrm.xyz = fma(far_nrm.xzy, vec3(2.0), vec3(-1.0));
		far_nrm.xz = rotate_vec2(far_nrm.xz, far_cs_angle) * p_align;

		// apply weighting when far_factor == 1.0 as the later lookup will be skipped.
		if (far_factor == 1.0) {
			float id_w = texture_id[0] == dual_scale_texture ? texture_weight[0] : texture_weight[1];
			float id_weight = exp2(sharpness * log2(weight + id_w + far_alb.a)) * weight;
			world_normal = FAST_WORLD_NORMAL(far_nrm).y;
			mat.albedo_height = fma(far_alb, vec4(id_weight), mat.albedo_height);
			mat.normal_rough = fma(far_nrm, vec4(id_weight), mat.normal_rough);
			mat.normal_map_depth = fma(_texture_normal_depth_array[dual_scale_texture], id_weight, mat.normal_map_depth);
			mat.ao_strength = fma(_texture_ao_strength_array[dual_scale_texture], id_weight, mat.ao_strength);
			mat.total_weight += id_weight;
		}
	}

//INSERT: DUAL_SCALING_CONDITION_0
		&& !(far_factor == 1.0 && texture_id[0] == dual_scale_texture)
//INSERT: DUAL_SCALING_CONDITION_1
		&& !(far_factor == 1.0 && texture_id[1] == dual_scale_texture)
//INSERT: DUAL_SCALING_MIX
		// If dual scaling, apply to overlay texture
		if (id == dual_scale_texture && far_factor > 0.) {
			alb = mix(alb, far_alb, far_factor);
			nrm = mix(nrm, far_nrm, far_factor);
		}
)"
