#[compute]
#version 450

// Stockham auto-sort radix-2 FFT butterfly pass, ported directly from
// caustic-volume's `fft` shader (sandbox/src/10_sim.js) -- a proven, working
// reference implementation, not an FFT derived from scratch here. Dispatched
// 2*log2(N) times per step (log2(N) horizontal passes, then log2(N)
// vertical), each time with `ns` doubling from 2 to N, ping-ponging between
// two buffers.
//
// Written deliberately flat -- no nested helper functions beyond the one
// trivial complex-multiply, no dynamic branching in the main body at all
// (just index arithmetic) -- per this session's own hard-won lesson about a
// heavily-inlined branchy helper (block_lookup(), called ~3000+ times per
// invocation) hanging the driver's PSO compiler outright in a completely
// different shader (mpm_bs_march.glsl; see its own header for the full
// story). This shader's call graph is as flat as it gets: one function call
// per invocation, no loops, no recursion. Keep it that way.

layout(local_size_x = 8, local_size_y = 8) in;

layout(set = 0, binding = 0, std140) uniform Params {
	ivec4 p; // x = N, y = ns, z = horiz (1 or 0), w unused
};

layout(set = 0, binding = 1, std430) restrict buffer FFTIn {
	vec2 fft_in[];
};
layout(set = 0, binding = 2, std430) restrict buffer FFTOut {
	vec2 fft_out[];
};

vec2 cmul(vec2 a, vec2 b) {
	return vec2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

void main() {
	ivec2 c = ivec2(gl_GlobalInvocationID.xy);
	int n = p.x, ns = p.y, horiz = p.z;
	if (c.x >= n || c.y >= n) {
		return;
	}
	int j = horiz == 1 ? c.x : c.y;
	int hs = ns / 2;
	int e = (j / ns) * hs + (j % hs);
	int od = e + n / 2;
	ivec2 ce = horiz == 1 ? ivec2(e, c.y) : ivec2(c.x, e);
	ivec2 co = horiz == 1 ? ivec2(od, c.y) : ivec2(c.x, od);
	vec2 E = fft_in[ce.y * n + ce.x];
	vec2 O = fft_in[co.y * n + co.x];
	float ang = 6.28318530718 * float(j % ns) / float(ns);
	vec2 w = vec2(cos(ang), sin(ang));
	fft_out[c.y * n + c.x] = E + cmul(w, O);
}
