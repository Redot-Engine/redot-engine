#[compute]
#version 450

// Same as water_blit_ripple.glsl but for the FFT ocean layer: unpacks the
// four ocean transforms (see water_spectrum_evolve.glsl) into
//   height_out_tex : h
//   disp_out_tex   : (Dx, Dz, dDx/dz, 0)
//   deriv_out_tex  : (dh/dx, dh/dz, dDx/dx, dDz/dz)
// so materials can build exact per-pixel normals and the fold Jacobian.

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	ivec4 res; // x = n, yzw unused
};
layout(set = 0, binding = 1, std430) restrict readonly buffer AIn {
	vec2 a_in[]; // h, Dx
};
layout(set = 0, binding = 2, r32f) uniform writeonly image2D height_out_tex;
layout(set = 0, binding = 3, std430) restrict readonly buffer BIn {
	vec2 b_in[]; // Dz, dh/dx
};
layout(set = 0, binding = 4, rgba32f) uniform writeonly image2D disp_out_tex;
layout(set = 0, binding = 5, std430) restrict readonly buffer CIn {
	vec2 c_in[]; // dh/dz, dDx/dx
};
layout(set = 0, binding = 6, std430) restrict readonly buffer DIn {
	vec2 d_in[]; // dDz/dz, dDx/dz
};
layout(set = 0, binding = 7, rgba32f) uniform writeonly image2D deriv_out_tex;

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	int n = res.x;
	if (c.x >= n || c.y >= n) {
		return;
	}
	int i = c.y * n + c.x;
	vec2 a = a_in[i];
	vec2 b = b_in[i];
	vec2 cc = c_in[i];
	vec2 d = d_in[i];
	imageStore(height_out_tex, c, vec4(a.x, 0.0, 0.0, 0.0));
	imageStore(disp_out_tex, c, vec4(a.y, b.x, d.y, 0.0));
	imageStore(deriv_out_tex, c, vec4(b.y, cc.x, cc.y, d.x));
}
