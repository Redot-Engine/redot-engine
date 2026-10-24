/**************************************************************************/
/*  physx_skinned_cloth_3d.cpp                                            */
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

#include "physx_skinned_cloth_3d.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/templates/hash_map.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/physical_bone_3d.h"
#include "scene/3d/physics/physical_bone_simulator_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/skin.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture_rd.h"

namespace {

const char *CLOTH_RENDER_SHADER = R"(
shader_type spatial;
render_mode cull_disabled;

// Drawn from PhysXSkinnedCloth3D's simulated positions: CUSTOM0.x is the
// particle each vertex belongs to, looked up in the solver's output images.
uniform sampler2D cloth_positions : filter_nearest, repeat_disable;
uniform sampler2D cloth_normals : filter_nearest, repeat_disable;
uniform int texture_width = 1;
uniform float normal_sign = 1.0;
uniform sampler2D albedo_texture : source_color, filter_linear_mipmap, repeat_enable, hint_default_white;
uniform vec4 albedo_color : source_color = vec4(1.0);
uniform float roughness : hint_range(0.0, 1.0) = 0.9;

void vertex() {
	int i = int(CUSTOM0.x + 0.5);
	ivec2 px = ivec2(i % texture_width, i / texture_width);
	VERTEX = texelFetch(cloth_positions, px, 0).xyz;
	NORMAL = normalize(texelFetch(cloth_normals, px, 0).xyz * normal_sign);
}

)";

// Body capsules between consecutive bones, by humanoid-profile name or the
// common Unreal-style name (radii at each end in meters).
struct CapsuleSpec {
	const char *a[2];
	const char *b[2];
	float ra;
	float rb;
};
const CapsuleSpec CAPSULE_SPECS[] = {
	{ { "Hips", "pelvis" }, { "Spine", "spine_01" }, 0.15f, 0.14f },
	{ { "Spine", "spine_01" }, { "Chest", "spine_02" }, 0.14f, 0.15f },
	{ { "Chest", "spine_02" }, { "UpperChest", "spine_03" }, 0.15f, 0.15f },
	{ { "UpperChest", "spine_03" }, { "Neck", "neck_01" }, 0.14f, 0.08f },
	{ { "LeftUpperLeg", "thigh_l" }, { "LeftLowerLeg", "calf_l" }, 0.085f, 0.06f },
	{ { "LeftLowerLeg", "calf_l" }, { "LeftFoot", "foot_l" }, 0.058f, 0.045f },
	{ { "RightUpperLeg", "thigh_r" }, { "RightLowerLeg", "calf_r" }, 0.085f, 0.06f },
	{ { "RightLowerLeg", "calf_r" }, { "RightFoot", "foot_r" }, 0.058f, 0.045f },
	{ { "LeftUpperArm", "upperarm_l" }, { "LeftLowerArm", "lowerarm_l" }, 0.048f, 0.04f },
	{ { "LeftLowerArm", "lowerarm_l" }, { "LeftHand", "hand_l" }, 0.04f, 0.032f },
	{ { "RightUpperArm", "upperarm_r" }, { "RightLowerArm", "lowerarm_r" }, 0.048f, 0.04f },
	{ { "RightLowerArm", "lowerarm_r" }, { "RightHand", "hand_r" }, 0.04f, 0.032f },
};

int find_bone_any(Skeleton3D *p_skeleton, const char *const p_names[2]) {
	for (int i = 0; i < 2; i++) {
		int b = p_skeleton->find_bone(p_names[i]);
		if (b >= 0) {
			return b;
		}
	}
	return -1;
}

float bits_as_float(uint32_t p_u) {
	float f;
	memcpy(&f, &p_u, 4);
	return f;
}

void put_mat4(float *r_dst, const Transform3D &p_t) {
	for (int c = 0; c < 3; c++) {
		const Vector3 col = p_t.basis.get_column(c);
		r_dst[c * 4 + 0] = col.x;
		r_dst[c * 4 + 1] = col.y;
		r_dst[c * 4 + 2] = col.z;
		r_dst[c * 4 + 3] = 0.0f;
	}
	r_dst[12] = p_t.origin.x;
	r_dst[13] = p_t.origin.y;
	r_dst[14] = p_t.origin.z;
	r_dst[15] = 1.0f;
}

} // namespace

void PhysXSkinnedCloth3D::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_mesh_instance_path", "path"), &PhysXSkinnedCloth3D::set_mesh_instance_path);
	ClassDB::bind_method(D_METHOD("get_mesh_instance_path"), &PhysXSkinnedCloth3D::get_mesh_instance_path);
	ClassDB::bind_method(D_METHOD("set_body_mesh_path", "path"), &PhysXSkinnedCloth3D::set_body_mesh_path);
	ClassDB::bind_method(D_METHOD("get_body_mesh_path"), &PhysXSkinnedCloth3D::get_body_mesh_path);
	ClassDB::bind_method(D_METHOD("set_surface", "surface"), &PhysXSkinnedCloth3D::set_surface);
	ClassDB::bind_method(D_METHOD("get_surface"), &PhysXSkinnedCloth3D::get_surface);
	ClassDB::bind_method(D_METHOD("set_max_distances", "distances"), &PhysXSkinnedCloth3D::set_max_distances);
	ClassDB::bind_method(D_METHOD("get_max_distances"), &PhysXSkinnedCloth3D::get_max_distances);
	ClassDB::bind_method(D_METHOD("set_pin_height", "height"), &PhysXSkinnedCloth3D::set_pin_height);
	ClassDB::bind_method(D_METHOD("get_pin_height"), &PhysXSkinnedCloth3D::get_pin_height);
	ClassDB::bind_method(D_METHOD("set_max_distance", "distance"), &PhysXSkinnedCloth3D::set_max_distance);
	ClassDB::bind_method(D_METHOD("get_max_distance"), &PhysXSkinnedCloth3D::get_max_distance);
	ClassDB::bind_method(D_METHOD("set_use_vertex_color", "enabled"), &PhysXSkinnedCloth3D::set_use_vertex_color);
	ClassDB::bind_method(D_METHOD("get_use_vertex_color"), &PhysXSkinnedCloth3D::get_use_vertex_color);
	ClassDB::bind_method(D_METHOD("get_effective_max_distances"), &PhysXSkinnedCloth3D::get_effective_max_distances);
	ClassDB::bind_method(D_METHOD("compute_height_ramp"), &PhysXSkinnedCloth3D::compute_height_ramp);
	ClassDB::bind_method(D_METHOD("set_substeps", "substeps"), &PhysXSkinnedCloth3D::set_substeps);
	ClassDB::bind_method(D_METHOD("get_substeps"), &PhysXSkinnedCloth3D::get_substeps);
	ClassDB::bind_method(D_METHOD("set_stiffness", "stiffness"), &PhysXSkinnedCloth3D::set_stiffness);
	ClassDB::bind_method(D_METHOD("get_stiffness"), &PhysXSkinnedCloth3D::get_stiffness);
	ClassDB::bind_method(D_METHOD("set_bend_stiffness", "stiffness"), &PhysXSkinnedCloth3D::set_bend_stiffness);
	ClassDB::bind_method(D_METHOD("get_bend_stiffness"), &PhysXSkinnedCloth3D::get_bend_stiffness);
	ClassDB::bind_method(D_METHOD("set_damping", "damping"), &PhysXSkinnedCloth3D::set_damping);
	ClassDB::bind_method(D_METHOD("get_damping"), &PhysXSkinnedCloth3D::get_damping);
	ClassDB::bind_method(D_METHOD("set_thickness", "thickness"), &PhysXSkinnedCloth3D::set_thickness);
	ClassDB::bind_method(D_METHOD("get_thickness"), &PhysXSkinnedCloth3D::get_thickness);
	ClassDB::bind_method(D_METHOD("set_backstop", "distance"), &PhysXSkinnedCloth3D::set_backstop);
	ClassDB::bind_method(D_METHOD("get_backstop"), &PhysXSkinnedCloth3D::get_backstop);
	ClassDB::bind_method(D_METHOD("set_self_collision", "enabled"), &PhysXSkinnedCloth3D::set_self_collision);
	ClassDB::bind_method(D_METHOD("get_self_collision"), &PhysXSkinnedCloth3D::get_self_collision);
	ClassDB::bind_method(D_METHOD("set_self_collision_thickness", "thickness"), &PhysXSkinnedCloth3D::set_self_collision_thickness);
	ClassDB::bind_method(D_METHOD("get_self_collision_thickness"), &PhysXSkinnedCloth3D::get_self_collision_thickness);
	ClassDB::bind_method(D_METHOD("set_animation_drive", "drive"), &PhysXSkinnedCloth3D::set_animation_drive);
	ClassDB::bind_method(D_METHOD("get_animation_drive"), &PhysXSkinnedCloth3D::get_animation_drive);
	ClassDB::bind_method(D_METHOD("set_collide_with_body", "enabled"), &PhysXSkinnedCloth3D::set_collide_with_body);
	ClassDB::bind_method(D_METHOD("get_collide_with_body"), &PhysXSkinnedCloth3D::get_collide_with_body);
	ClassDB::bind_method(D_METHOD("set_collision_radius_scale", "scale"), &PhysXSkinnedCloth3D::set_collision_radius_scale);
	ClassDB::bind_method(D_METHOD("get_collision_radius_scale"), &PhysXSkinnedCloth3D::get_collision_radius_scale);
	ClassDB::bind_method(D_METHOD("set_simulating", "simulating"), &PhysXSkinnedCloth3D::set_simulating);
	ClassDB::bind_method(D_METHOD("is_simulating"), &PhysXSkinnedCloth3D::is_simulating);
	ClassDB::bind_method(D_METHOD("rebuild"), &PhysXSkinnedCloth3D::rebuild);
	ClassDB::bind_method(D_METHOD("reset"), &PhysXSkinnedCloth3D::reset);
	ClassDB::bind_method(D_METHOD("get_particle_count"), &PhysXSkinnedCloth3D::get_particle_count);
	ClassDB::bind_method(D_METHOD("get_body_capsules"), &PhysXSkinnedCloth3D::get_body_capsules);

	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "mesh_instance_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MeshInstance3D"), "set_mesh_instance_path", "get_mesh_instance_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "surface", PROPERTY_HINT_RANGE, "0,16,1"), "set_surface", "get_surface");
	ADD_GROUP("Painting", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_FLOAT32_ARRAY, "max_distances"), "set_max_distances", "get_max_distances");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pin_height", PROPERTY_HINT_RANGE, "-5,5,0.01,or_greater,or_less,suffix:m"), "set_pin_height", "get_pin_height");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_distance", PROPERTY_HINT_RANGE, "0,2,0.01,or_greater,suffix:m"), "set_max_distance", "get_max_distance");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "use_vertex_color"), "set_use_vertex_color", "get_use_vertex_color");
	ADD_GROUP("Simulation", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "substeps", PROPERTY_HINT_RANGE, "1,32,1"), "set_substeps", "get_substeps");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "stiffness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_stiffness", "get_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "bend_stiffness", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_bend_stiffness", "get_bend_stiffness");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "damping", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_damping", "get_damping");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "thickness", PROPERTY_HINT_RANGE, "0,0.1,0.001,suffix:m"), "set_thickness", "get_thickness");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "simulating"), "set_simulating", "is_simulating");
	ADD_GROUP("Collision", "");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "collide_with_body"), "set_collide_with_body", "get_collide_with_body");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "body_mesh_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "MeshInstance3D"), "set_body_mesh_path", "get_body_mesh_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collision_radius_scale", PROPERTY_HINT_RANGE, "0.1,3,0.01"), "set_collision_radius_scale", "get_collision_radius_scale");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "backstop", PROPERTY_HINT_RANGE, "-0.01,0.2,0.001,suffix:m"), "set_backstop", "get_backstop");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "animation_drive", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_animation_drive", "get_animation_drive");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "self_collision"), "set_self_collision", "get_self_collision");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "self_collision_thickness", PROPERTY_HINT_RANGE, "0.001,0.1,0.001,suffix:m"), "set_self_collision_thickness", "get_self_collision_thickness");
}

void PhysXSkinnedCloth3D::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			// Built on the next process tick, once siblings (the skeleton, the
			// source mesh) are in the tree too. Re-adding the node rebuilds it.
			if (!Engine::get_singleton()->is_editor_hint()) {
				set_process_internal(true);
				build_pending = true;
			}
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
			if (build_pending) {
				_try_build();
			}
			if (built && simulating) {
				_step(get_process_delta_time());
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			_clear();
		} break;
	}
}

void PhysXSkinnedCloth3D::_clear() {
	MeshInstance3D *ri = ObjectDB::get_instance<MeshInstance3D>(render_instance_id);
	if (ri != nullptr) {
		ri->queue_free();
	}
	render_instance = nullptr;
	render_instance_id = ObjectID();
	MeshInstance3D *source = ObjectDB::get_instance<MeshInstance3D>(source_id);
	if (source != nullptr) {
		source->set_visible(true);
	}
	// Let go of the solver's textures before it frees them, or the material
	// still binds them on its next update.
	if (render_material.is_valid()) {
		render_material->set_shader_parameter("cloth_positions", Variant());
		render_material->set_shader_parameter("cloth_normals", Variant());
		render_material.unref();
	}
	for (Ref<Texture2DRD> *tex : { &pos_texture, &nrm_texture }) {
		if (tex->is_valid()) {
			(*tex)->set_texture_rd_rid(RID());
			tex->unref();
		}
	}
	if (solver != nullptr) {
		memdelete(solver);
		solver = nullptr;
	}
	built = false;
	particle_count = 0;
}

bool PhysXSkinnedCloth3D::_find_physical_bone_shapes(Skeleton3D *p_skeleton) {
	// The skeleton's physical bones (Skeleton3D > Create Physical Skeleton in
	// the editor): every enabled capsule or sphere CollisionShape3D on them,
	// fixed to its bone the same way the PhysicalBone3D follows it.
	for (int i = 0; i < p_skeleton->get_child_count(true); i++) {
		PhysicalBoneSimulator3D *sim = Object::cast_to<PhysicalBoneSimulator3D>(p_skeleton->get_child(i, true));
		if (sim == nullptr) {
			continue;
		}
		for (int j = 0; j < sim->get_child_count(); j++) {
			PhysicalBone3D *pb = Object::cast_to<PhysicalBone3D>(sim->get_child(j));
			if (pb == nullptr) {
				continue;
			}
			int bone = pb->get_bone_id();
			if (bone < 0) {
				bone = p_skeleton->find_bone(pb->get_bone_name());
			}
			if (bone < 0) {
				continue;
			}
			for (int k = 0; k < pb->get_child_count(); k++) {
				CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(pb->get_child(k));
				if (cs == nullptr || cs->is_disabled() || cs->get_shape().is_null()) {
					continue;
				}
				Capsule c;
				c.from_shape = true;
				c.bone_a = bone;
				c.bone_b = bone;
				c.shape_local = pb->get_body_offset() * cs->get_transform();
				const float sx = c.shape_local.basis.get_column(0).length();
				const float sy = c.shape_local.basis.get_column(1).length();
				Ref<CapsuleShape3D> capsule = cs->get_shape();
				Ref<SphereShape3D> sphere = cs->get_shape();
				float radius;
				if (capsule.is_valid()) {
					radius = capsule->get_radius();
					c.half_length = MAX(capsule->get_height() * 0.5f - radius, 0.0f) * sy;
				} else if (sphere.is_valid()) {
					radius = sphere->get_radius();
				} else {
					continue;
				}
				c.radius_a = radius * sx;
				c.radius_b = radius * sx;
				c.shape_local.orthonormalize();
				if (capsules.size() >= SkinnedClothSolverGPU::MAX_CAPSULES) {
					WARN_PRINT_ONCE(vformat("PhysXSkinnedCloth3D: more than %d collision shapes on the physical bones; the rest are ignored.", SkinnedClothSolverGPU::MAX_CAPSULES));
					return true;
				}
				capsules.push_back(c);
			}
		}
	}
	return !capsules.is_empty();
}

void PhysXSkinnedCloth3D::_find_capsules(Skeleton3D *p_skeleton) {
	capsules.clear();
	using_physical_bones = _find_physical_bone_shapes(p_skeleton);
	if (using_physical_bones) {
		return;
	}
	for (const CapsuleSpec &s : CAPSULE_SPECS) {
		Capsule c;
		c.bone_a = find_bone_any(p_skeleton, s.a);
		c.bone_b = find_bone_any(p_skeleton, s.b);
		if (c.bone_a < 0 || c.bone_b < 0) {
			continue;
		}
		c.radius_a = s.ra;
		c.radius_b = s.rb;
		capsules.push_back(c);
	}
}

void PhysXSkinnedCloth3D::_fit_capsules(Skeleton3D *p_skeleton, MeshInstance3D *p_body) {
	// Radii from the body itself: for each capsule, the body vertices skinned
	// mostly to its first bone, measured from the bone segment in the body's
	// bind space. A high percentile at each end, so the capsule wraps the
	// limb instead of its average.
	if (p_body == nullptr || p_body->get_mesh().is_null() || p_body->get_skin().is_null()) {
		return;
	}
	Ref<Skin> skin = p_body->get_skin();
	HashMap<int, int> bone_to_bind;
	for (int j = 0; j < skin->get_bind_count(); j++) {
		int bone = skin->get_bind_bone(j);
		if (bone < 0) {
			bone = p_skeleton->find_bone(skin->get_bind_name(j));
		}
		bone_to_bind[bone] = j;
	}
	Ref<Mesh> mesh = p_body->get_mesh();
	for (Capsule &c : capsules) {
		if (!bone_to_bind.has(c.bone_a) || !bone_to_bind.has(c.bone_b)) {
			continue;
		}
		const int bind_a = bone_to_bind[c.bone_a];
		// Bone heads in the body's bind (mesh) space.
		const Vector3 a = skin->get_bind_pose(bind_a).affine_inverse().origin;
		const Vector3 b = skin->get_bind_pose(bone_to_bind[c.bone_b]).affine_inverse().origin;
		const Vector3 ab = b - a;
		const float len2 = MAX(ab.length_squared(), 1e-8f);
		LocalVector<float> near_a, near_b;
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			const Array arr = mesh->surface_get_arrays(s);
			const PackedVector3Array v = arr[Mesh::ARRAY_VERTEX];
			const PackedInt32Array bi = arr[Mesh::ARRAY_BONES];
			const PackedFloat32Array bw = arr[Mesh::ARRAY_WEIGHTS];
			if (v.is_empty() || bi.size() != bw.size() || bi.is_empty()) {
				continue;
			}
			const int per = bi.size() / v.size();
			for (int i = 0; i < v.size(); i++) {
				int best = -1;
				float best_w = 0.0f;
				for (int k = 0; k < per; k++) {
					if (bw[i * per + k] > best_w) {
						best_w = bw[i * per + k];
						best = bi[i * per + k];
					}
				}
				if (best != bind_a || best_w < 0.5f) {
					continue;
				}
				const float t = CLAMP((v[i] - a).dot(ab) / len2, 0.0f, 1.0f);
				const float d = v[i].distance_to(a + ab * t);
				(t < 0.5f ? near_a : near_b).push_back(d);
			}
		}
		auto percentile = [](LocalVector<float> &p_vals, float p_fallback) {
			if (p_vals.size() < 8) {
				return p_fallback;
			}
			p_vals.sort();
			return p_vals[(uint32_t)(p_vals.size() * 0.9f)];
		};
		c.radius_a = percentile(near_a, c.radius_a);
		c.radius_b = percentile(near_b, c.radius_b);
	}
}

MeshInstance3D *PhysXSkinnedCloth3D::get_source_mesh_instance() const {
	return Object::cast_to<MeshInstance3D>(get_node_or_null(mesh_instance_path));
}

PackedFloat32Array PhysXSkinnedCloth3D::compute_height_ramp() const {
	PackedFloat32Array out;
	MeshInstance3D *source = get_source_mesh_instance();
	if (source == nullptr || source->get_mesh().is_null() || surface >= source->get_mesh()->get_surface_count()) {
		return out;
	}
	const PackedVector3Array verts = source->get_mesh()->surface_get_arrays(surface)[Mesh::ARRAY_VERTEX];
	float min_y = 1e30f;
	for (const Vector3 &v : verts) {
		min_y = MIN(min_y, v.y);
	}
	const float span = MAX(pin_height - min_y, 1e-3f);
	out.resize(verts.size());
	for (int i = 0; i < verts.size(); i++) {
		out.set(i, CLAMP((pin_height - verts[i].y) / span, 0.0f, 1.0f) * max_distance);
	}
	return out;
}

PackedFloat32Array PhysXSkinnedCloth3D::get_effective_max_distances() const {
	MeshInstance3D *source = get_source_mesh_instance();
	if (source == nullptr || source->get_mesh().is_null() || surface >= source->get_mesh()->get_surface_count()) {
		return PackedFloat32Array();
	}
	const Array arrays = source->get_mesh()->surface_get_arrays(surface);
	const int vcount = PackedVector3Array(arrays[Mesh::ARRAY_VERTEX]).size();
	if (max_distances.size() == vcount) {
		PackedFloat32Array out = max_distances;
		for (int i = 0; i < vcount; i++) {
			out.set(i, MAX(out[i], 0.0f));
		}
		return out;
	}
	const PackedColorArray colors = arrays[Mesh::ARRAY_COLOR];
	if (use_vertex_color && colors.size() == vcount) {
		PackedFloat32Array out;
		out.resize(vcount);
		for (int i = 0; i < vcount; i++) {
			out.set(i, CLAMP(colors[i].r, 0.0f, 1.0f) * max_distance);
		}
		return out;
	}
	return compute_height_ramp();
}

void PhysXSkinnedCloth3D::_try_build() {
	build_pending = false;
	_clear();
	MeshInstance3D *source = Object::cast_to<MeshInstance3D>(get_node_or_null(mesh_instance_path));
	ERR_FAIL_NULL_MSG(source, "PhysXSkinnedCloth3D: mesh_instance_path doesn't point to a MeshInstance3D.");
	Ref<ArrayMesh> mesh = source->get_mesh();
	ERR_FAIL_COND_MSG(mesh.is_null(), "PhysXSkinnedCloth3D: the MeshInstance3D has no ArrayMesh.");
	ERR_FAIL_INDEX(surface, mesh->get_surface_count());
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(source->get_node_or_null(source->get_skeleton_path()));
	Ref<Skin> skin = source->get_skin();
	ERR_FAIL_COND_MSG(skeleton == nullptr || skin.is_null(), "PhysXSkinnedCloth3D: the mesh must be skinned to a Skeleton3D (with a Skin).");

	bind_bones.clear();
	bind_poses.clear();
	for (int j = 0; j < skin->get_bind_count(); j++) {
		int bone = skin->get_bind_bone(j);
		if (bone < 0) {
			bone = skeleton->find_bone(skin->get_bind_name(j));
		}
		bind_bones.push_back(bone);
		bind_poses.push_back(skin->get_bind_pose(j));
	}

	const Array arrays = mesh->surface_get_arrays(surface);
	const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
	const PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	const PackedInt32Array bones = arrays[Mesh::ARRAY_BONES];
	const PackedFloat32Array weights = arrays[Mesh::ARRAY_WEIGHTS];
	PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];
	const int vcount = verts.size();
	ERR_FAIL_COND_MSG(vcount == 0 || bones.is_empty() || weights.size() != bones.size(), "PhysXSkinnedCloth3D: the surface has no skinning data.");
	const int per_vertex = bones.size() / vcount;
	if (indices.is_empty()) {
		indices.resize(vcount);
		for (int i = 0; i < vcount; i++) {
			indices.set(i, i);
		}
	}

	// Max distance per render vertex: painted, vertex colour, or height ramp.
	const PackedFloat32Array vertex_max = get_effective_max_distances();
	ERR_FAIL_COND_MSG(vertex_max.size() != vcount, "PhysXSkinnedCloth3D: couldn't work out the max distances.");

	// Weld split vertices (UV/normal seams) into particles.
	HashMap<Vector3i, int> weld;
	Vector<int> vertex_particle;
	vertex_particle.resize(vcount);
	Vector<int> particle_vertex; // first render vertex of each particle
	Vector<float> particle_max;
	for (int i = 0; i < vcount; i++) {
		const Vector3 &v = verts[i];
		const Vector3i key((int)Math::round(v.x * 1e5f), (int)Math::round(v.y * 1e5f), (int)Math::round(v.z * 1e5f));
		HashMap<Vector3i, int>::Iterator it = weld.find(key);
		int p;
		if (it) {
			p = it->value;
			particle_max.set(p, MIN(particle_max[p], vertex_max[i]));
		} else {
			p = particle_vertex.size();
			weld.insert(key, p);
			particle_vertex.push_back(i);
			particle_max.push_back(vertex_max[i]);
		}
		vertex_particle.set(i, p);
	}
	const int n = particle_vertex.size();

	PackedFloat32Array rest;
	rest.resize(n * 4);
	// Rest normal per particle: the welded vertices' normals averaged (seams
	// carry the same normal on both sides, hard edges get the average).
	PackedFloat32Array rest_normals;
	rest_normals.resize(n * 4);
	{
		Vector<Vector3> acc;
		acc.resize(n);
		Vector3 *aw = acc.ptrw();
		for (int i = 0; i < n; i++) {
			aw[i] = Vector3();
		}
		if (normals.size() == vcount) {
			for (int i = 0; i < vcount; i++) {
				aw[vertex_particle[i]] += normals[i];
			}
		}
		for (int p = 0; p < n; p++) {
			const Vector3 nn = acc[p].length_squared() > 1e-12f ? acc[p].normalized() : Vector3(0, 1, 0);
			rest_normals.set(p * 4 + 0, nn.x);
			rest_normals.set(p * 4 + 1, nn.y);
			rest_normals.set(p * 4 + 2, nn.z);
			rest_normals.set(p * 4 + 3, 0.0f);
		}
	}
	PackedInt32Array skin_idx;
	skin_idx.resize(n * 4);
	PackedFloat32Array skin_w;
	skin_w.resize(n * 4);
	for (int p = 0; p < n; p++) {
		const int v = particle_vertex[p];
		rest.set(p * 4 + 0, verts[v].x);
		rest.set(p * 4 + 1, verts[v].y);
		rest.set(p * 4 + 2, verts[v].z);
		rest.set(p * 4 + 3, particle_max[p]);
		// Strongest four influences, renormalised.
		int bi[4] = { 0, 0, 0, 0 };
		float bw[4] = { 0, 0, 0, 0 };
		for (int k = 0; k < per_vertex; k++) {
			const float w = weights[v * per_vertex + k];
			const int b = bones[v * per_vertex + k];
			for (int s = 0; s < 4; s++) {
				if (w > bw[s]) {
					for (int m = 3; m > s; m--) {
						bw[m] = bw[m - 1];
						bi[m] = bi[m - 1];
					}
					bw[s] = w;
					bi[s] = b;
					break;
				}
			}
		}
		const float sum = MAX(bw[0] + bw[1] + bw[2] + bw[3], 1e-6f);
		for (int s = 0; s < 4; s++) {
			skin_idx.set(p * 4 + s, CLAMP(bi[s], 0, MAX(bind_bones.size() - 1, 0)));
			skin_w.set(p * 4 + s, bw[s] / sum);
		}
	}

	// Triangles in particle indices; edges with their opposite corners.
	LocalVector<Vector3i> tris;
	for (int t = 0; t + 2 < indices.size(); t += 3) {
		const int a = vertex_particle[indices[t]];
		const int b = vertex_particle[indices[t + 1]];
		const int c = vertex_particle[indices[t + 2]];
		if (a == b || b == c || a == c) {
			continue;
		}
		tris.push_back(Vector3i(a, b, c));
	}
	struct EdgeInfo {
		int opp0 = -1;
		int opp1 = -1;
	};
	HashMap<uint64_t, EdgeInfo> edges;
	auto edge_key = [](int a, int b) -> uint64_t {
		return ((uint64_t)MIN(a, b) << 32) | (uint64_t)MAX(a, b);
	};
	for (const Vector3i &t : tris) {
		const int c[3] = { t.x, t.y, t.z };
		for (int e = 0; e < 3; e++) {
			const uint64_t k = edge_key(c[e], c[(e + 1) % 3]);
			EdgeInfo *info = edges.getptr(k);
			if (info == nullptr) {
				EdgeInfo ni;
				ni.opp0 = c[(e + 2) % 3];
				edges.insert(k, ni);
			} else if (info->opp1 < 0) {
				info->opp1 = c[(e + 2) % 3];
			}
		}
	}
	struct Con {
		int a, b;
		float rest, k;
	};
	LocalVector<Con> cons;
	auto particle_pos = [&](int p) {
		return Vector3(rest[p * 4], rest[p * 4 + 1], rest[p * 4 + 2]);
	};
	auto add_con = [&](int a, int b, float k) {
		if (particle_max[a] <= 0.0f && particle_max[b] <= 0.0f) {
			return; // both follow the animation
		}
		cons.push_back({ a, b, particle_pos(a).distance_to(particle_pos(b)), k });
	};
	for (const KeyValue<uint64_t, EdgeInfo> &kv : edges) {
		add_con((int)(kv.key >> 32), (int)(kv.key & 0xffffffffu), 1.0f);
		if (kv.value.opp0 >= 0 && kv.value.opp1 >= 0 && kv.value.opp0 != kv.value.opp1) {
			add_con(kv.value.opp0, kv.value.opp1, bend_stiffness);
		}
	}

	// Colour the constraints so no two in a batch share a particle.
	LocalVector<uint64_t> used;
	used.resize(n);
	for (uint64_t &u : used) {
		u = 0;
	}
	LocalVector<int> colour;
	colour.resize(cons.size());
	int colors = 0;
	for (uint32_t i = 0; i < cons.size(); i++) {
		const uint64_t taken = used[cons[i].a] | used[cons[i].b];
		int c = 0;
		while (c < 63 && (taken & (1ull << c))) {
			c++;
		}
		colour[i] = c;
		used[cons[i].a] |= 1ull << c;
		used[cons[i].b] |= 1ull << c;
		colors = MAX(colors, c + 1);
	}
	PackedInt32Array batch_offsets;
	batch_offsets.resize(colors + 1);
	PackedFloat32Array con_data;
	con_data.resize(cons.size() * 4);
	{
		int write = 0;
		for (int c = 0; c < colors; c++) {
			batch_offsets.set(c, write);
			for (uint32_t i = 0; i < cons.size(); i++) {
				if (colour[i] != c) {
					continue;
				}
				con_data.set(write * 4 + 0, bits_as_float((uint32_t)cons[i].a));
				con_data.set(write * 4 + 1, bits_as_float((uint32_t)cons[i].b));
				con_data.set(write * 4 + 2, cons[i].rest);
				con_data.set(write * 4 + 3, cons[i].k);
				write++;
			}
		}
		batch_offsets.set(colors, write);
	}

	// Tethers: each free particle to its nearest pinned particle.
	LocalVector<int> pinned;
	for (int p = 0; p < n; p++) {
		if (particle_max[p] <= 0.0f) {
			pinned.push_back(p);
		}
	}
	PackedFloat32Array tethers;
	tethers.resize(n * 2);
	for (int p = 0; p < n; p++) {
		uint32_t anchor = 0xffffffffu;
		float best = 1e30f;
		if (particle_max[p] > 0.0f) {
			const Vector3 pp = particle_pos(p);
			for (int q : pinned) {
				const float d = pp.distance_squared_to(particle_pos(q));
				if (d < best) {
					best = d;
					anchor = (uint32_t)q;
				}
			}
		}
		tethers.set(p * 2 + 0, bits_as_float(anchor));
		tethers.set(p * 2 + 1, anchor == 0xffffffffu ? 0.0f : Math::sqrt(best));
	}

	// Triangle adjacency for normals: each particle's incident triangles as
	// the other two corners, in winding order.
	PackedInt32Array adj_offsets;
	adj_offsets.resize(n + 1);
	{
		LocalVector<int> counts;
		counts.resize(n);
		for (int &c : counts) {
			c = 0;
		}
		for (const Vector3i &t : tris) {
			counts[t.x]++;
			counts[t.y]++;
			counts[t.z]++;
		}
		int acc = 0;
		for (int p = 0; p < n; p++) {
			adj_offsets.set(p, acc);
			acc += counts[p];
		}
		adj_offsets.set(n, acc);
	}
	PackedInt32Array adj_pairs;
	adj_pairs.resize(adj_offsets[n] * 2);
	{
		LocalVector<int> fill;
		fill.resize(n);
		for (int p = 0; p < n; p++) {
			fill[p] = adj_offsets[p];
		}
		auto put = [&](int p, int o1, int o2) {
			adj_pairs.set(fill[p] * 2 + 0, o1);
			adj_pairs.set(fill[p] * 2 + 1, o2);
			fill[p]++;
		};
		for (const Vector3i &t : tris) {
			put(t.x, t.y, t.z);
			put(t.y, t.z, t.x);
			put(t.z, t.x, t.y);
		}
	}

	solver = memnew(SkinnedClothSolver);
	if (!solver->is_available()) {
		memdelete(solver);
		solver = nullptr;
		ERR_FAIL_MSG("PhysXSkinnedCloth3D: no RenderingDevice -- compute cloth needs a GPU renderer.");
	}
	solver->build(rest, rest_normals, skin_idx, skin_w, con_data, batch_offsets, tethers, adj_offsets, adj_pairs, bind_bones.size());
	particle_count = n;

	// Render copy: same surface, no skinning, CUSTOM0.x = particle index.
	Array render_arrays;
	render_arrays.resize(Mesh::ARRAY_MAX);
	render_arrays[Mesh::ARRAY_VERTEX] = verts;
	render_arrays[Mesh::ARRAY_NORMAL] = normals;
	render_arrays[Mesh::ARRAY_TEX_UV] = arrays[Mesh::ARRAY_TEX_UV];
	render_arrays[Mesh::ARRAY_INDEX] = indices;
	PackedFloat32Array custom0;
	custom0.resize(vcount);
	for (int i = 0; i < vcount; i++) {
		custom0.set(i, (float)vertex_particle[i]);
	}
	render_arrays[Mesh::ARRAY_CUSTOM0] = custom0;
	Ref<ArrayMesh> render_mesh;
	render_mesh.instantiate();
	render_mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, render_arrays, Array(), Dictionary(),
			Mesh::ARRAY_CUSTOM_R_FLOAT << Mesh::ARRAY_FORMAT_CUSTOM0_SHIFT);

	// Our normals come from the triangle winding; match the source's sense.
	float normal_sign = 1.0f;
	if (!tris.is_empty() && !normals.is_empty()) {
		const int i0 = indices[0], i1 = indices[1], i2 = indices[2];
		const Vector3 fn = (verts[i1] - verts[i0]).cross(verts[i2] - verts[i0]);
		normal_sign = fn.dot(normals[i0] + normals[i1] + normals[i2]) >= 0.0f ? 1.0f : -1.0f;
	}

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(CLOTH_RENDER_SHADER);
	render_material.instantiate();
	render_material->set_shader(shader);
	pos_texture.instantiate();
	pos_texture->set_texture_rd_rid(solver->get_position_texture_rd_rid());
	nrm_texture.instantiate();
	nrm_texture->set_texture_rd_rid(solver->get_normal_texture_rd_rid());
	render_material->set_shader_parameter("cloth_positions", pos_texture);
	render_material->set_shader_parameter("cloth_normals", nrm_texture);
	render_material->set_shader_parameter("texture_width", solver->get_texture_width());
	render_material->set_shader_parameter("normal_sign", normal_sign);
	Ref<BaseMaterial3D> src_mat = source->get_active_material(surface);
	if (src_mat.is_valid()) {
		render_material->set_shader_parameter("albedo_texture", src_mat->get_texture(BaseMaterial3D::TEXTURE_ALBEDO));
		render_material->set_shader_parameter("albedo_color", src_mat->get_albedo());
		render_material->set_shader_parameter("roughness", src_mat->get_roughness());
	}

	// Drawn as an internal sibling of the source mesh with the same local
	// transform, in the source's space: it then moves exactly like the mesh it
	// replaces, physics interpolation included (a top-level node in world
	// space would lead the interpolated character by up to a tick).
	render_instance = memnew(MeshInstance3D);
	render_instance->set_mesh(render_mesh);
	render_instance->set_surface_override_material(0, render_material);
	render_instance->set_cast_shadows_setting(source->get_cast_shadows_setting());
	Node *source_parent = source->get_parent();
	if (source_parent) {
		// Internal sibling of the source with the same local transform: it then
		// moves exactly like the mesh it replaces, physics interpolation
		// included (a top-level node in world space would lead the interpolated
		// character by up to a tick).
		source_parent->add_child(render_instance, false, INTERNAL_MODE_BACK);
		render_instance->set_transform(source->get_transform());
	} else {
		// Root-level source (no parent to sibling into): parent the render twin
		// here with a compensating transform, so a one-node test scene works.
		add_child(render_instance, false, INTERNAL_MODE_BACK);
		render_instance->set_transform(get_global_transform().affine_inverse() * source->get_global_transform());
	}
	float largest = 0.0f;
	for (int i = 0; i < vertex_max.size(); i++) {
		largest = MAX(largest, vertex_max[i]);
	}
	render_instance->set_custom_aabb(source->get_aabb().grow(largest + 0.25f));
	render_instance_id = render_instance->get_instance_id();
	source->set_visible(false);

	source_id = source->get_instance_id();
	skeleton_id = skeleton->get_instance_id();
	prev_capsules.clear();
	_find_capsules(skeleton);
	if (!using_physical_bones) {
		_fit_capsules(skeleton, Object::cast_to<MeshInstance3D>(get_node_or_null(body_mesh_path)));
	}
	built = true;
}

void PhysXSkinnedCloth3D::_step(double p_delta) {
	Skeleton3D *skeleton = ObjectDB::get_instance<Skeleton3D>(skeleton_id);
	MeshInstance3D *source = ObjectDB::get_instance<MeshInstance3D>(source_id);
	if (skeleton == nullptr || source == nullptr || solver == nullptr) {
		return;
	}
	const Transform3D skel_xform = skeleton->get_global_transform();
	PackedFloat32Array bone_data;
	bone_data.resize(MAX(bind_bones.size(), 1) * 16);
	float *bw = bone_data.ptrw();
	for (int j = 0; j < bind_bones.size(); j++) {
		const int bone = bind_bones[j];
		const Transform3D bone_pose = bone >= 0 ? skeleton->get_bone_global_pose(bone) : Transform3D();
		put_mat4(bw + j * 16, skel_xform * bone_pose * bind_poses[j]);
	}

	PackedFloat32Array capsule_data;
	if (collide_with_body) {
		// This frame's capsule ends, after last frame's: the solver blends
		// between them across the substeps, so a fast leg sweeps through the
		// substeps instead of jumping through the cloth once per frame.
		PackedFloat32Array current;
		current.resize(capsules.size() * 8);
		float *cw = current.ptrw();
		for (int c = 0; c < capsules.size(); c++) {
			Vector3 a;
			Vector3 b;
			float scale = collision_radius_scale;
			if (capsules[c].from_shape) {
				// Where the PhysicalBone3D puts its shape: bone pose * body offset.
				const Transform3D x = skel_xform * skeleton->get_bone_global_pose(capsules[c].bone_a) * capsules[c].shape_local;
				a = x.xform(Vector3(0, -capsules[c].half_length, 0));
				b = x.xform(Vector3(0, capsules[c].half_length, 0));
				scale = 1.0f; // authored shapes are used as they are
			} else {
				a = skel_xform.xform(skeleton->get_bone_global_pose(capsules[c].bone_a).origin);
				b = skel_xform.xform(skeleton->get_bone_global_pose(capsules[c].bone_b).origin);
			}
			cw[c * 8 + 0] = a.x;
			cw[c * 8 + 1] = a.y;
			cw[c * 8 + 2] = a.z;
			cw[c * 8 + 3] = capsules[c].radius_a * scale;
			cw[c * 8 + 4] = b.x;
			cw[c * 8 + 5] = b.y;
			cw[c * 8 + 6] = b.z;
			cw[c * 8 + 7] = capsules[c].radius_b * scale;
		}
		if (prev_capsules.size() != current.size()) {
			prev_capsules = current;
		}
		capsule_data.resize(capsules.size() * 16);
		float *dw = capsule_data.ptrw();
		for (int c = 0; c < capsules.size(); c++) {
			memcpy(dw + c * 16, prev_capsules.ptr() + c * 8, 8 * sizeof(float));
			memcpy(dw + c * 16 + 8, current.ptr() + c * 8, 8 * sizeof(float));
		}
		prev_capsules = current;
	}

	SkinnedClothSolver::Settings s;
	s.stiffness = stiffness;
	s.damping = damping;
	s.thickness = thickness;
	s.backstop = backstop;
	s.animation_drive = animation_drive;
	s.self_collision_thickness = self_collision ? self_collision_thickness : 0.0f;
	s.output_xform = source->get_global_transform().affine_inverse();
	solver->step(bone_data, capsule_data, p_delta, substeps, s);

	MeshInstance3D *ri = ObjectDB::get_instance<MeshInstance3D>(render_instance_id);
	if (ri != nullptr && ri->get_transform() != source->get_transform()) {
		ri->set_transform(source->get_transform());
	}
}

Array PhysXSkinnedCloth3D::get_body_capsules() const {
	Array out;
	Skeleton3D *skeleton = ObjectDB::get_instance<Skeleton3D>(skeleton_id);
	for (const Capsule &c : capsules) {
		Dictionary d;
		d["bone_a"] = skeleton != nullptr ? skeleton->get_bone_name(c.bone_a) : String();
		d["bone_b"] = skeleton != nullptr ? skeleton->get_bone_name(c.bone_b) : String();
		const float scale = c.from_shape ? 1.0f : collision_radius_scale;
		d["radius_a"] = c.radius_a * scale;
		d["radius_b"] = c.radius_b * scale;
		d["from_physical_bone"] = c.from_shape;
		out.push_back(d);
	}
	return out;
}

void PhysXSkinnedCloth3D::rebuild() {
	if (is_inside_tree() && !Engine::get_singleton()->is_editor_hint()) {
		build_pending = true;
	}
}

void PhysXSkinnedCloth3D::reset() {
	if (solver != nullptr) {
		solver->reset();
	}
}

void PhysXSkinnedCloth3D::set_mesh_instance_path(const NodePath &p_path) {
	mesh_instance_path = p_path;
	rebuild();
}

void PhysXSkinnedCloth3D::set_body_mesh_path(const NodePath &p_path) {
	body_mesh_path = p_path;
	rebuild();
}

void PhysXSkinnedCloth3D::set_surface(int p_surface) {
	surface = MAX(p_surface, 0);
	rebuild();
}

void PhysXSkinnedCloth3D::set_max_distances(const PackedFloat32Array &p_distances) {
	max_distances = p_distances;
	rebuild();
}

void PhysXSkinnedCloth3D::set_pin_height(float p_height) {
	pin_height = p_height;
	rebuild();
}

void PhysXSkinnedCloth3D::set_max_distance(float p_distance) {
	max_distance = MAX(p_distance, 0.0f);
	rebuild();
}

void PhysXSkinnedCloth3D::set_use_vertex_color(bool p_enabled) {
	use_vertex_color = p_enabled;
	rebuild();
}

void PhysXSkinnedCloth3D::set_substeps(int p_substeps) {
	substeps = CLAMP(p_substeps, 1, 64);
}

void PhysXSkinnedCloth3D::set_stiffness(float p_stiffness) {
	stiffness = CLAMP(p_stiffness, 0.0f, 1.0f);
}

void PhysXSkinnedCloth3D::set_bend_stiffness(float p_stiffness) {
	bend_stiffness = CLAMP(p_stiffness, 0.0f, 1.0f);
	rebuild();
}

void PhysXSkinnedCloth3D::set_damping(float p_damping) {
	damping = CLAMP(p_damping, 0.0f, 1.0f);
}

void PhysXSkinnedCloth3D::set_thickness(float p_thickness) {
	thickness = MAX(p_thickness, 0.0f);
}

void PhysXSkinnedCloth3D::set_self_collision(bool p_enabled) {
	self_collision = p_enabled;
}

void PhysXSkinnedCloth3D::set_self_collision_thickness(float p_thickness) {
	self_collision_thickness = MAX(p_thickness, 0.001f);
}

void PhysXSkinnedCloth3D::set_animation_drive(float p_drive) {
	animation_drive = CLAMP(p_drive, 0.0f, 1.0f);
}

void PhysXSkinnedCloth3D::set_backstop(float p_distance) {
	backstop = p_distance;
}

void PhysXSkinnedCloth3D::set_collide_with_body(bool p_enabled) {
	collide_with_body = p_enabled;
}

void PhysXSkinnedCloth3D::set_collision_radius_scale(float p_scale) {
	collision_radius_scale = MAX(p_scale, 0.01f);
}

void PhysXSkinnedCloth3D::set_simulating(bool p_simulating) {
	simulating = p_simulating;
}

PhysXSkinnedCloth3D::PhysXSkinnedCloth3D() {
	set_process_priority(100); // after AnimationPlayer has posed the skeleton
}

PhysXSkinnedCloth3D::~PhysXSkinnedCloth3D() {
	if (solver != nullptr) {
		memdelete(solver);
		solver = nullptr;
	}
}
