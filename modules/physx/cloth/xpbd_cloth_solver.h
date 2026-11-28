/**************************************************************************/
/*  xpbd_cloth_solver.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2024-present Redot Engine contributors                   */
/*                                          (see REDOT_AUTHORS.md)        */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "core/math/transform_3d.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/templates/local_vector.h"
#include "core/variant/variant.h"

// A small extended position-based-dynamics (XPBD) solver -- no PhysX and no
// scene dependency, so it runs on any platform. It is the CPU fallback for the
// PhysX GPU deformable-surface path (cloth) and also backs the module's
// SoftBody3D implementation: a closed triangle mesh whose edge constraints hold
// its shape and whose optional volume constraint (pressure > 0) stops it
// collapsing. (Named ...ClothSolver for historical reasons.)
//
// It uses the "small steps" formulation (Macklin et al. 2019): each frame is
// split into a number of substeps, and every substep does a single Gauss-Seidel
// pass over the constraints. Stiffness is expressed as XPBD compliance, so it
// stays consistent regardless of the substep count or frame rate.
class XPBDClothSolver {
public:
	struct Settings {
		int substeps = 12;
		// Compliance in m/N. 0 == rigid; larger == softer. Structural (stretch)
		// wants to stay near 0; shear and bending are softer by default.
		float stretch_compliance = 0.0f;
		float shear_compliance = 2.0e-4f;
		float bend_compliance = 1.0e-3f;
		float damping = 0.03f; // fraction of relative velocity removed per second
		float drag = 1.0f; // air drag normal to each triangle
		float lift = 0.2f; // sideways push from air flowing across a triangle
		float density = 0.2f; // kg/m^2, sets per-vertex mass
		// Closed-mesh volume preservation. `pressure` is the target enclosed
		// volume as a fraction of the rest volume: 0 disables the constraint
		// (plain cloth), 1 holds the rest shape, >1 inflates. `pressure_stiffness`
		// (0..1] is how hard it pulls back -- 1 is near-incompressible, small
		// values let it squish. Only meaningful for a watertight mesh.
		float pressure = 0.0f;
		float pressure_stiffness = 0.5f;
		float max_speed = 0.0f; // 0 = uncapped; else clamps per-vertex speed
	};

	Settings settings;

	void clear();

	// Build an (p_cols x p_rows) vertex grid spanning a local-space rectangle of
	// p_size meters, centered on the origin in the XY plane (facing +Z).
	void build_grid(int p_cols, int p_rows, const Vector2 &p_size);
	// Build from an arbitrary triangle mesh. Structural constraints follow the
	// unique edges; bending constraints span the shared edge of adjacent tris.
	void build_mesh(const PackedVector3Array &p_positions, const PackedInt32Array &p_indices);

	// Place the rest shape into world space and seed the simulation there.
	void reset(const Transform3D &p_xform);

	bool is_built() const { return !rest_positions.is_empty(); }
	int vertex_count() const { return positions.size(); }
	int grid_cols() const { return cols; }
	int grid_rows() const { return rows; }

	// inv_mass 0 pins a vertex. pin_target, when finite, drags a pinned vertex to
	// a world position each step (for attachment to a moving anchor).
	void set_pinned(int p_vertex, bool p_pinned);
	bool is_pinned(int p_vertex) const;
	void set_pin_target(int p_vertex, const Vector3 &p_world_pos);
	void clear_pin_target(int p_vertex);

	// Advance the simulation. p_wind is a world-space air velocity.
	void step(float p_dt, const Vector3 &p_gravity, const Vector3 &p_wind);

	// Mutable access for the node's post-step world-collision pass.
	LocalVector<Vector3> &positions_mut() { return positions; }
	LocalVector<Vector3> &velocities_mut() { return velocities; }
	const LocalVector<Vector3> &get_positions() const { return positions; }
	const LocalVector<Vector3> &get_velocities() const { return velocities; }
	const LocalVector<int> &get_indices() const { return indices; }
	float vertex_radius() const { return thickness; }

	// Total rest surface area, for turning a target total mass into an areal density.
	float rest_surface_area() const;
	// Scale every constraint's rest length (SoftBody3D shrinking_factor).
	void set_rest_length_scale(float p_scale);

private:
	struct DistanceConstraint {
		int a = 0;
		int b = 0;
		float rest = 0.0f;
		float compliance = 0.0f;
	};

	LocalVector<Vector3> rest_positions; // local space
	LocalVector<Vector3> positions; // world space, current
	LocalVector<Vector3> prev; // world space, start of substep
	LocalVector<Vector3> velocities;
	LocalVector<float> inv_mass;
	LocalVector<float> base_inv_mass; // inv_mass with pinning ignored
	LocalVector<int> pinned; // 1 == pinned
	LocalVector<Vector3> pin_target; // NaN component == no target
	LocalVector<DistanceConstraint> constraints;
	LocalVector<int> indices;

	int cols = 0;
	int rows = 0;
	float thickness = 0.01f; // half-spacing, used as the collision radius
	float rest_volume = 0.0f; // signed enclosed volume of the rest mesh
	float applied_rest_scale = 1.0f; // last shrinking_factor applied to constraint rest lengths

	void _add_constraint(int p_a, int p_b, float p_compliance);
	void _finalize();
	void _apply_aero(float p_dt, const Vector3 &p_wind);
	void _solve_volume(float p_sdt);
	float _mesh_volume(const LocalVector<Vector3> &p_pos) const;
};
