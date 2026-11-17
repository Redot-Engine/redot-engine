// Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.

R"(

//INSERT: AUTO_SHADER_UNIFORMS
#define AUTO_SHADER
uniform float auto_slope : hint_range(0, 10) = 1.0;
uniform float auto_height_reduction : hint_range(0, 1) = 0.1;
uniform int auto_base_texture : hint_range(0, 31) = 0;
uniform int auto_overlay_texture : hint_range(0, 31) = 1;

//INSERT: AUTO_SHADER
	{
		// Auto blend calculation
		float auto_blend = clamp(fma(auto_slope * 2.0, (w_normal.y - 1.0), 1.0)
			- auto_height_reduction * 0.01 * v_vertex.y, 0.0, 1.0);
		// Enable Autoshader if outside regions or painted in regions, otherwise manual painted
		uvec4 is_auto = (control & uvec4(0x1u)) | uvec4(uint(region_uv.z < 0.0));
		uint u_auto =
			((uint(auto_base_texture) & 0x1Fu) << 27u) |
			((uint(auto_overlay_texture) & 0x1Fu) << 22u) |
			((uint(fma(auto_blend, 255.0 , 0.5)) & 0xFFu) << 14u);
		control = control * (1u - is_auto) + u_auto * is_auto;
	}

)"
