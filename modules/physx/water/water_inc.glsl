// Shared definitions for the water ripple + FFT ocean solver (water_*.glsl).
//
// Phase 1 scope only: the iWave-style local ripple layer (2D wave equation +
// sphere/impulse body-disturbance splat) and its height readback. FFT ocean
// spectrum fields get added here in Phase 2 once that layer's own uniform
// needs are designed -- see the "Scoping: FFT ocean + ripple water solver"
// section of the project plan for the full architecture.

#define MAX_SPHERES 48
#define MAX_IMPULSES 16

layout(set = 0, binding = 0, std140) uniform Params {
	vec4 domain_dt; // xy domain half-size (world), z depth, w dt
	vec4 ripple; // x alpha (damping), y gravity, z cell size, w water_level (mean surface y)
	ivec4 res_count; // x grid resolution N (ripple grid, cells per side), y num_spheres, z num_impulses, w unused
	vec4 ripple_control; // x body disturbance amplitude, yzw unused
};

// Ripple state: x=h (height above water_level), y=h_prev, z=foam, w unused.
// Ping-ponged externally (two buffers, swapped by the caller each step) the
// same way mpm_fluid's grid buffers are -- not double-buffered in here.
layout(set = 0, binding = 1, std430) restrict buffer RippleStateIn {
	vec4 state_in[];
};
layout(set = 0, binding = 2, std430) restrict buffer RippleStateOut {
	vec4 state_out[];
};

// Composed final height (world Y), full ripple-grid resolution. Sampled
// directly by rendering; a separate, coarser readback buffer (added once the
// CPU-sampling path is built) is what buoyancy actually reads back to the CPU.
layout(set = 0, binding = 3, std430) restrict buffer HeightOut {
	float height_out[];
};

// Body-disturbance sources, submitted by the node once per physics tick from
// buoyant_body.gd (or similar) via PhysXWaterSurface3D::submit_sphere()/
// submit_impulse() -- see the plan's "push, not discovery" rationale. The
// ripple field is 2D (height over XZ), so these pack world XZ, not a full
// 3D point: x=world_x, y=world_z, z=radius, w=strength (0..1).
layout(set = 0, binding = 4, std430) restrict buffer Spheres {
	vec4 spheres[];
};
layout(set = 0, binding = 5, std430) restrict buffer Impulses {
	vec4 impulses[];
};

// Still-water depth per ripple cell (R16F, meters); <= 0 is dry land. From
// the seabed (PhysXWaterSurface3D::seabed_from_floor) and/or the
// surface_mesh footprint, or the constant depth everywhere. Uploaded once
// per configure(). Read with texelFetch only.
layout(set = 0, binding = 6) uniform sampler2D cell_depth_tex;

#define DOMAIN (domain_dt.xy)
#define DEPTH (domain_dt.z)
#define DT (domain_dt.w)
#define ALPHA (ripple.x)
#define GRAV (ripple.y)
#define CELL (ripple.z)
#define WATER_LEVEL (ripple.w)
#define RIPPLE_AMPLITUDE (ripple_control.x)
#define GRID_N (res_count.x)
#define NUM_SPHERES (res_count.y)
#define NUM_IMPULSES (res_count.z)

int grid_index(ivec2 c) {
	return c.y * GRID_N + c.x;
}

float cell_depth(ivec2 c) {
	return texelFetch(cell_depth_tex, c, 0).r;
}

bool is_wet(ivec2 c) {
	return cell_depth(c) > 0.0;
}

// Wave-equation coefficient c^2 dt^2 / dx^2 for a cell, from its own depth
// (shallow-water speed sqrt(g * depth)), capped at the explicit scheme's
// stability limit.
float wave_coefficient(ivec2 c) {
	return min(GRAV * max(cell_depth(c), 0.1) * DT * DT / (CELL * CELL), 0.5);
}
