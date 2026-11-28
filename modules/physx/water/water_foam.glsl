#[compute]
#version 450

// Persistent whitecap foam on the ocean grid. Each step: where the choppy
// displacement folds the surface (area Jacobian below the threshold) foam is
// injected, and everywhere it fades by exp(-dt / persistence). Stored in the
// water's rest coordinates, like the FFT fields, so it rides with the water
// itself rather than the wave shape: a crest passes through and leaves its
// whitecap behind as a fading streak.

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 foam_params; // x decay factor this step, y choppiness, z threshold, w injection softness
	ivec4 res; // x = n, y = 1 if foam is enabled, zw unused
};
layout(set = 0, binding = 1, rgba32f) uniform restrict readonly image2D deriv_tex; // (dh/dx, dh/dz, dDx/dx, dDz/dz)
layout(set = 0, binding = 2, rgba32f) uniform restrict readonly image2D disp_tex; // (Dx, Dz, dDx/dz, 0)
layout(set = 0, binding = 3, r32f) uniform restrict image2D foam_tex;

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	if (c.x >= res.x || c.y >= res.x) {
		return;
	}
	if (res.y == 0) {
		// Foam off: keep the texture clear.
		imageStore(foam_tex, c, vec4(0.0));
		return;
	}
	// Same displacement sign and Jacobian as the materials (see
	// PhysXWaterSurface3D's built-in shader).
	float s = -foam_params.y;
	vec4 der = imageLoad(deriv_tex, c);
	float dxz = imageLoad(disp_tex, c).b;
	float jacobian = (1.0 + s * der.z) * (1.0 + s * der.w) - s * s * dxz * dxz;
	float inject = clamp((foam_params.z - jacobian) / max(foam_params.w, 1e-3), 0.0, 1.0);
	float foam = imageLoad(foam_tex, c).r * foam_params.x;
	imageStore(foam_tex, c, vec4(max(foam, inject), 0.0, 0.0, 0.0));
}
