#[compute]
#version 450

// Per-step time evolution of the base spectrum h0 into the current complex
// height field, via the standard dispersion relation w = sqrt(g*k*tanh(k*depth))
// and h(k,t) = h0(k)*e^{-iwt} + conj(h0(-k))*e^{iwt}. Direct port of
// caustic-volume's `spectrum` shader (sandbox/src/10_sim.js) -- proven
// working reference math, not derived from scratch. Output feeds
// water_fft.glsl's first butterfly pass. Written flat, same discipline as
// every other pass in this file (see water_fft.glsl's header).
//
// Also produces Tessendorf's horizontal "choppy" displacement
// D(k) = -i (k/|k|) h(k), the surface slopes i k h(k), and the displacement
// derivatives i k D(k) the Jacobian needs -- eight real fields packed two per
// complex transform: every one of them is real in space, so transforming
// F(k) + i G(k) yields F in the real part and G in the imaginary part.
//   spec_out    : h   + i Dx     = h (1 + kx/k)
//   spec_dz_out : Dz  + i dh/dx  = h (-kx - i kz/k)
//   spec_c_out  : dh/dz + i dDx/dx = h (i (kz + kx^2/k))
//   spec_d_out  : dDz/dz + i dDx/dz = h (kz^2/k + i kx kz/k)

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 domain_time; // xy domain size, z depth, w time
	ivec4 res; // x N, yzw unused
};

layout(set = 0, binding = 1, std430) restrict buffer H0 {
	vec4 h0[];
};
layout(set = 0, binding = 2, std430) restrict buffer SpecOut {
	vec2 spec_out[];
};
layout(set = 0, binding = 3, std430) restrict buffer SpecDzOut {
	vec2 spec_dz_out[];
};
layout(set = 0, binding = 4, std430) restrict buffer SpecCOut {
	vec2 spec_c_out[];
};
layout(set = 0, binding = 5, std430) restrict buffer SpecDOut {
	vec2 spec_d_out[];
};

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	int n = res.x;
	if (c.x >= n || c.y >= n) {
		return;
	}
	int mx = c.x < n / 2 ? c.x : c.x - n;
	int mz = c.y < n / 2 ? c.y : c.y - n;
	vec2 domain = domain_time.xy;
	float depth = domain_time.z;
	float time = domain_time.w;

	vec2 k = 6.28318530718 * vec2(float(mx), float(mz)) / domain;
	float kl = max(length(k), 1e-6);
	float w = sqrt(9.81 * kl * tanh(min(kl * depth, 20.0)));
	float cw = cos(w * time), sw = sin(w * time);

	vec4 packed = h0[c.y * n + c.x];
	vec2 a = packed.xy, b = packed.zw;
	// water_fft.glsl sums e^{+ikx}, so a mode travels along +k only with
	// e^{-iwt} on h0(k) -- the reference's e^{+iwt} sent every wave upwind
	// (measured: the peak moved against the wind at its phase speed).
	vec2 h = vec2(a.x * cw + a.y * sw, a.y * cw - a.x * sw) + vec2(b.x * cw - b.y * sw, b.x * sw + b.y * cw);
	vec2 k_hat = k / kl;
	int i = c.y * n + c.x;
	spec_out[i] = h * (1.0 + k_hat.x);
	spec_dz_out[i] = vec2(-k.x * h.x + k_hat.y * h.y, -k.x * h.y - k_hat.y * h.x);
	float ci = k.y + k.x * k_hat.x;
	spec_c_out[i] = vec2(-ci * h.y, ci * h.x);
	float dr = k.y * k_hat.y;
	float di = k.x * k_hat.y;
	spec_d_out[i] = vec2(dr * h.x - di * h.y, dr * h.y + di * h.x);
}
