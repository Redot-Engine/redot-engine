/**************************************************************************/
/*  physx_skinned_cloth_3d.h                                              */
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

#include "skinned_cloth_solver.h"

#include "scene/3d/node_3d.h"

class MeshInstance3D;
class Skeleton3D;
class ShaderMaterial;
class Texture2DRD;

// Cloth on a region of a character's skinned mesh -- a robe, cape or skirt
// that's part of the character, not a separate pinned sheet. Each vertex has
// a max distance it may stray from where the animation puts it: 0 follows the
// animation exactly (shoulders, waistband), larger values swing freely (hem,
// cape tail). The mesh is simulated on the GPU with compute shaders (any GPU)
// and collides with capsules on the character's bones. The source mesh is
// hidden and drawn instead from the simulated positions.
class PhysXSkinnedCloth3D : public Node3D {
	GDCLASS(PhysXSkinnedCloth3D, Node3D);

	NodePath mesh_instance_path;
	NodePath body_mesh_path; // optional: fit the body capsules to this skinned mesh
	int surface = 0;
	PackedFloat32Array max_distances; // per vertex of the surface; empty = auto from height
	float pin_height = 1.3f; // auto paint: at/above this bind-space height the cloth follows the animation
	float max_distance = 0.4f; // auto paint: max distance at the lowest point, ramping up from pin_height
	bool use_vertex_color = false; // unpainted: the mesh's vertex colour red channel x max_distance
	int substeps = 8;
	float stiffness = 0.9f;
	float bend_stiffness = 0.3f;
	float damping = 0.05f;
	float thickness = 0.012f;
	float backstop = 0.02f;
	float animation_drive = 0.0f;
	bool self_collision = false;
	float self_collision_thickness = 0.02f;
	float collision_radius_scale = 1.0f;
	bool collide_with_body = true;
	bool simulating = true;

	SkinnedClothSolver *solver = nullptr;
	MeshInstance3D *render_instance = nullptr;
	ObjectID render_instance_id;
	Ref<ShaderMaterial> render_material;
	Ref<Texture2DRD> pos_texture;
	Ref<Texture2DRD> nrm_texture;
	ObjectID source_id;
	ObjectID skeleton_id;
	Vector<int> bind_bones; // skeleton bone index per skin bind
	Vector<Transform3D> bind_poses;
	struct Capsule {
		// Automatic: between two bones' heads. From a physical bone: a
		// capsule (or sphere) shape fixed to one bone.
		int bone_a = -1;
		int bone_b = -1;
		float radius_a = 0.05f;
		float radius_b = 0.05f;
		bool from_shape = false;
		Transform3D shape_local; // bone space: body_offset * shape transform
		float half_length = 0.0f; // along the shape's local Y
	};
	bool using_physical_bones = false;
	Vector<Capsule> capsules;
	PackedFloat32Array prev_capsules; // last frame's capsule ends, for blending across substeps
	int particle_count = 0;
	bool built = false;
	bool build_pending = false;

	void _try_build();
	void _clear();
	void _find_capsules(Skeleton3D *p_skeleton);
	bool _find_physical_bone_shapes(Skeleton3D *p_skeleton);
	void _fit_capsules(Skeleton3D *p_skeleton, MeshInstance3D *p_body);
	void _step(double p_delta);

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	void set_mesh_instance_path(const NodePath &p_path);
	NodePath get_mesh_instance_path() const { return mesh_instance_path; }
	void set_body_mesh_path(const NodePath &p_path);
	NodePath get_body_mesh_path() const { return body_mesh_path; }
	void set_surface(int p_surface);
	int get_surface() const { return surface; }
	void set_max_distances(const PackedFloat32Array &p_distances);
	PackedFloat32Array get_max_distances() const { return max_distances; }
	void set_pin_height(float p_height);
	float get_pin_height() const { return pin_height; }
	void set_max_distance(float p_distance);
	float get_max_distance() const { return max_distance; }
	void set_use_vertex_color(bool p_enabled);
	bool get_use_vertex_color() const { return use_vertex_color; }

	// The source MeshInstance3D / surface arrays, and the max distance each of
	// its vertices gets: painted (max_distances), else the vertex colour red
	// channel (use_vertex_color), else the height ramp. Also used by the
	// editor's paint tool.
	MeshInstance3D *get_source_mesh_instance() const;
	PackedFloat32Array get_effective_max_distances() const;
	PackedFloat32Array compute_height_ramp() const;
	void set_substeps(int p_substeps);
	int get_substeps() const { return substeps; }
	void set_stiffness(float p_stiffness);
	float get_stiffness() const { return stiffness; }
	void set_bend_stiffness(float p_stiffness);
	float get_bend_stiffness() const { return bend_stiffness; }
	void set_damping(float p_damping);
	float get_damping() const { return damping; }
	void set_thickness(float p_thickness);
	float get_thickness() const { return thickness; }
	void set_backstop(float p_distance);
	float get_backstop() const { return backstop; }
	void set_self_collision(bool p_enabled);
	bool get_self_collision() const { return self_collision; }
	void set_self_collision_thickness(float p_thickness);
	float get_self_collision_thickness() const { return self_collision_thickness; }
	void set_animation_drive(float p_drive);
	float get_animation_drive() const { return animation_drive; }
	void set_collide_with_body(bool p_enabled);
	bool get_collide_with_body() const { return collide_with_body; }
	void set_collision_radius_scale(float p_scale);
	float get_collision_radius_scale() const { return collision_radius_scale; }
	void set_simulating(bool p_simulating);
	bool is_simulating() const { return simulating; }

	void rebuild();
	void reset();
	int get_particle_count() const { return particle_count; }
	Array get_body_capsules() const;

	PhysXSkinnedCloth3D();
	~PhysXSkinnedCloth3D();
};
