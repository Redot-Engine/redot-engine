#[compute]
#version 450

// Copies the ripple layer's composed height buffer into a real RD texture
// (image2D) for zero-copy sampling by the scene renderer via Texture2DRD --
// see water_solver.h's header for why this exists (the shared-device design
// this module's earlier solvers never needed). Deliberately trivial: one
// buffer read, one imageStore, no branching beyond the bounds guard -- no
// risk of the kind of compiler blowup a heavily-inlined shader caused
// elsewhere in this module (see mpm_bs_march.glsl's header for that story).

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	ivec4 res; // x = n, yzw unused
};
layout(set = 0, binding = 1, std430) restrict readonly buffer HeightIn {
	float height_in[];
};
layout(set = 0, binding = 2, r32f) uniform writeonly image2D height_out_tex;

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	int n = res.x;
	if (c.x >= n || c.y >= n) {
		return;
	}
	imageStore(height_out_tex, c, vec4(height_in[c.y * n + c.x], 0.0, 0.0, 0.0));
}
