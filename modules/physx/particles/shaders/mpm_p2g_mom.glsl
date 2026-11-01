#[compute]
#version 450

#include "mpm_fluid_inc.glsl"

layout(local_size_x = GROUP) in;

// P2G pass 2: build the particle's stress and scatter APIC momentum + the
// MLS-MPM stress contribution to the grid.
//   fluid    -- sample the local density off the grid mass and run a Tait EOS.
//   granular -- evolve the deformation gradient, SVD + Drucker-Prager return
//               map (so the material yields in shear and carries no tension),
//               St-Venant-Kirchhoff-Hencky stress from the elastic part.
void main() {
	uint id = gl_GlobalInvocationID.x;
	if (id >= uint(PCOUNT)) {
		return;
	}

	Particle p = particles[id];
	vec3 x = p.x_d.xyz;
	vec3 v = p.v.xyz;
	mat3 C = mat3(p.c0.xyz, p.c1.xyz, p.c2.xyz);

	ivec3 base;
	vec3 fx;
	grid_local(x, base, fx);
	vec3 wx = bspline(fx.x), wy = bspline(fx.y), wz = bspline(fx.z);

	mat3 affine;

	if (GRANULAR) {
		// Trial elastic deformation gradient, then plastic return mapping.
		mat3 F = mat3(p.f0.xyz, p.f1.xyz, p.f2.xyz);
		F = (mat3(1.0) + DT * C) * F;

		mat3 U, V;
		vec3 sig;
		svd3(F, U, sig, V);
		vec3 sig_e = dp_project(sig, GMU, GLAMBDA, DP_ALPHA, DP_COH);

		// store the corrected (elastic) F back
		mat3 Fe = U * mat3(sig_e.x, 0, 0, 0, sig_e.y, 0, 0, 0, sig_e.z) * transpose(V);
		particles[id].f0.xyz = Fe[0];
		particles[id].f1.xyz = Fe[1];
		particles[id].f2.xyz = Fe[2];

		vec3 eps = log(max(sig_e, vec3(1e-4)));
		float tr = eps.x + eps.y + eps.z;
		float J = sig_e.x * sig_e.y * sig_e.z;
		vec3 tau_p = 2.0 * GMU * eps + vec3(GLAMBDA * tr); // principal Kirchhoff stress
		mat3 cauchy = (1.0 / max(J, 1e-4)) * U * mat3(tau_p.x, 0, 0, 0, tau_p.y, 0, 0, 0, tau_p.z) * transpose(U);

		float vol = PMASS * J / RHO0;
		affine = PMASS * C - (DT * vol * 4.0 / (DX * DX)) * cauchy;

		particles[id].x_d.w = RHO0 / max(J, 1e-4);
	} else {
		// gather grid mass back to a local density estimate
		float mass_here = 0.0;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 3; j++) {
				for (int k = 0; k < 3; k++) {
					ivec3 node = base + ivec3(i, j, k);
					if (any(lessThan(node, ivec3(0))) || any(greaterThanEqual(node, RES))) {
						continue;
					}
					float w = wx[i] * wy[j] * wz[k];
					mass_here += w * float(grid_i[node_index(node) * 4 + 0]) / FIXED;
				}
			}
		}
		float density = mass_here / (DX * DX * DX);
		float ratio = clamp(density / RHO0, 0.6, 2.0);

		// Tait EOS -- stiff in compression (ratio > 1), gentle pull in expansion.
		float pressure = STIFF * (pow(ratio, GAMMA) - 1.0);
		pressure = max(pressure, -0.01 * STIFF);

		mat3 stress = mat3(-pressure);
		stress += VISC * (C + transpose(C)); // simple velocity-gradient viscosity

		float vol = PMASS / max(density, 1.0);
		affine = PMASS * C - (DT * vol * 4.0 / (DX * DX)) * stress;

		particles[id].x_d.w = density; // readback / debug
	}

	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 3; j++) {
			for (int k = 0; k < 3; k++) {
				ivec3 node = base + ivec3(i, j, k);
				if (any(lessThan(node, ivec3(0))) || any(greaterThanEqual(node, RES))) {
					continue;
				}
				float w = wx[i] * wy[j] * wz[k];
				vec3 dpos = (ORIGIN + vec3(node) * DX) - x;
				vec3 contrib = w * (PMASS * v + affine * dpos);
				int b = node_index(node) * 4;
				atomicAdd(grid_i[b + 1], int(contrib.x * FIXED));
				atomicAdd(grid_i[b + 2], int(contrib.y * FIXED));
				atomicAdd(grid_i[b + 3], int(contrib.z * FIXED));
			}
		}
	}
}
