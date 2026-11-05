/**************************************************************************/
/*  physx_cloth_paint_plugin.cpp                                          */
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

#include "physx_cloth_paint_plugin.h"

#include "../cloth/physx_skinned_cloth_3d.h"

#include "core/input/input_event.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/separator.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/3d/skin.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "servers/rendering/rendering_server.h"

namespace {
enum BrushMode {
	BRUSH_PAINT,
	BRUSH_ERASE,
	BRUSH_SMOOTH,
};

Color heat(float p_t) {
	// 0 (pinned) blue -> green -> yellow -> red (free).
	const Color stops[4] = { Color(0.15, 0.35, 1.0), Color(0.1, 0.85, 0.35), Color(1.0, 0.9, 0.1), Color(1.0, 0.15, 0.1) };
	const float t = CLAMP(p_t, 0.0f, 1.0f) * 3.0f;
	const int i = MIN((int)t, 2);
	return stops[i].lerp(stops[i + 1], t - i);
}
} // namespace

PhysXClothPaintPlugin::PhysXClothPaintPlugin() {
	toolbar = memnew(HBoxContainer);
	toolbar->hide();

	toolbar->add_child(memnew(VSeparator));
	paint_button = memnew(Button);
	paint_button->set_text(TTRC("Paint Cloth"));
	paint_button->set_toggle_mode(true);
	paint_button->set_theme_type_variation("FlatButton");
	paint_button->set_tooltip_text(TTRC("Paint how far each vertex may stray from its animated position.\nLMB: paint toward Value. Shift+LMB: toward 0 (follows the animation). Ctrl+LMB: smooth."));
	paint_button->connect(SceneStringName(toggled), callable_mp(this, &PhysXClothPaintPlugin::_paint_toggled));
	toolbar->add_child(paint_button);

	auto add_spin = [&](const String &p_label, double p_min, double p_max, double p_step, double p_value, const String &p_suffix) {
		Label *l = memnew(Label);
		l->set_text(p_label);
		toolbar->add_child(l);
		SpinBox *s = memnew(SpinBox);
		s->set_min(p_min);
		s->set_max(p_max);
		s->set_step(p_step);
		s->set_value(p_value);
		s->set_suffix(p_suffix);
		s->set_select_all_on_focus(true);
		toolbar->add_child(s);
		return s;
	};
	value_spin = add_spin(TTR("Value"), 0.0, 2.0, 0.005, 0.25, "m");
	radius_spin = add_spin(TTR("Radius"), 0.01, 2.0, 0.01, 0.1, "m");
	strength_spin = add_spin(TTR("Strength"), 0.0, 1.0, 0.01, 0.5, "");

	Button *fill = memnew(Button);
	fill->set_text(TTRC("Fill"));
	fill->set_tooltip_text(TTRC("Set every vertex to Value."));
	fill->set_theme_type_variation("FlatButton");
	fill->connect(SceneStringName(pressed), callable_mp(this, &PhysXClothPaintPlugin::_fill));
	toolbar->add_child(fill);
	Button *ramp = memnew(Button);
	ramp->set_text(TTRC("Ramp"));
	ramp->set_tooltip_text(TTRC("Start from the height ramp: pinned above Pin Height, rising to Max Distance at the lowest point."));
	ramp->set_theme_type_variation("FlatButton");
	ramp->connect(SceneStringName(pressed), callable_mp(this, &PhysXClothPaintPlugin::_ramp));
	toolbar->add_child(ramp);

	Node3DEditor::get_singleton()->add_control_to_menu_panel(toolbar);

	overlay_material.instantiate();
	overlay_material->set_shading_mode(BaseMaterial3D::SHADING_MODE_UNSHADED);
	overlay_material->set_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
	overlay_material->set_cull_mode(BaseMaterial3D::CULL_DISABLED);
}

void PhysXClothPaintPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_refresh_overlay"), &PhysXClothPaintPlugin::_refresh_overlay);
}

void PhysXClothPaintPlugin::_refresh_overlay() {
	// After undo/redo: reload the painted values from the node.
	if (paint_button->is_pressed() && _prepare()) {
		_update_overlay();
	}
}

bool PhysXClothPaintPlugin::handles(Object *p_object) const {
	return Object::cast_to<PhysXSkinnedCloth3D>(p_object) != nullptr;
}

void PhysXClothPaintPlugin::edit(Object *p_object) {
	PhysXSkinnedCloth3D *next = Object::cast_to<PhysXSkinnedCloth3D>(p_object);
	if (next != cloth) {
		paint_button->set_pressed(false);
		_clear_overlay();
	}
	cloth = next;
}

void PhysXClothPaintPlugin::make_visible(bool p_visible) {
	toolbar->set_visible(p_visible);
	if (!p_visible) {
		paint_button->set_pressed(false);
		_clear_overlay();
	}
}

void PhysXClothPaintPlugin::_paint_toggled(bool p_on) {
	if (p_on) {
		if (!_prepare()) {
			paint_button->set_pressed_no_signal(false);
			return;
		}
		_update_overlay();
	} else {
		_clear_overlay();
	}
}

bool PhysXClothPaintPlugin::_prepare() {
	// The source surface skinned to the skeleton's current (editor) pose --
	// the same transform the renderer applies, so the overlay sits on the
	// visible mesh.
	if (cloth == nullptr || !cloth->is_inside_tree()) {
		return false;
	}
	MeshInstance3D *source = cloth->get_source_mesh_instance();
	ERR_FAIL_NULL_V_MSG(source, false, "PhysXSkinnedCloth3D: set mesh_instance_path to the skinned mesh first.");
	Ref<Mesh> mesh = source->get_mesh();
	ERR_FAIL_COND_V(mesh.is_null() || cloth->get_surface() >= mesh->get_surface_count(), false);
	const Array arrays = mesh->surface_get_arrays(cloth->get_surface());
	const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
	const PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
	const PackedInt32Array bones = arrays[Mesh::ARRAY_BONES];
	const PackedFloat32Array weights = arrays[Mesh::ARRAY_WEIGHTS];
	indices = arrays[Mesh::ARRAY_INDEX];
	const int vcount = verts.size();
	if (indices.is_empty()) {
		indices.resize(vcount);
		for (int i = 0; i < vcount; i++) {
			indices.set(i, i);
		}
	}
	world_verts.resize(vcount);
	world_normals.resize(vcount);
	Skeleton3D *skeleton = Object::cast_to<Skeleton3D>(source->get_node_or_null(source->get_skeleton_path()));
	Ref<Skin> skin = source->get_skin();
	const bool skinned = skeleton != nullptr && skin.is_valid() && !bones.is_empty() && bones.size() == weights.size();
	Vector<Transform3D> bind_world;
	if (skinned) {
		const Transform3D skel_xform = skeleton->get_global_transform();
		for (int j = 0; j < skin->get_bind_count(); j++) {
			int bone = skin->get_bind_bone(j);
			if (bone < 0) {
				bone = skeleton->find_bone(skin->get_bind_name(j));
			}
			const Transform3D pose = bone >= 0 ? skeleton->get_bone_global_pose(bone) : Transform3D();
			bind_world.push_back(skel_xform * pose * skin->get_bind_pose(j));
		}
	}
	const int per = skinned ? bones.size() / vcount : 0;
	const Transform3D mi_xform = source->get_global_transform();
	for (int i = 0; i < vcount; i++) {
		Vector3 p = verts[i];
		Vector3 n = normals.size() == vcount ? normals[i] : Vector3(0, 1, 0);
		if (skinned) {
			Vector3 sp, sn;
			for (int k = 0; k < per; k++) {
				const float w = weights[i * per + k];
				const int b = bones[i * per + k];
				if (w <= 0.0f || b < 0 || b >= bind_world.size()) {
					continue;
				}
				sp += bind_world[b].xform(p) * w;
				sn += bind_world[b].basis.xform(n) * w;
			}
			p = sp;
			n = sn.normalized();
		} else {
			p = mi_xform.xform(p);
			n = mi_xform.basis.xform(n).normalized();
		}
		world_verts.set(i, p);
		world_normals.set(i, n);
	}
	values = cloth->get_effective_max_distances();
	return values.size() == vcount;
}

void PhysXClothPaintPlugin::_clear_overlay() {
	if (overlay != nullptr) {
		overlay->queue_free();
		overlay = nullptr;
	}
	MeshInstance3D *source = ObjectDB::get_instance<MeshInstance3D>(hidden_source);
	if (source != nullptr && source->get_instance().is_valid()) {
		RS::get_singleton()->instance_set_visible(source->get_instance(), source->is_visible_in_tree());
	}
	hidden_source = ObjectID();
	stroking = false;
}

float PhysXClothPaintPlugin::_scale_max() const {
	// Colour scale: the larger of the brush value and the largest painted value.
	float m = MAX((float)value_spin->get_value(), 0.01f);
	for (float v : values) {
		m = MAX(m, v);
	}
	return m;
}

void PhysXClothPaintPlugin::_update_overlay() {
	if (cloth == nullptr || values.size() != world_verts.size()) {
		return;
	}
	const int vcount = world_verts.size();
	PackedVector3Array pos;
	pos.resize(vcount);
	PackedColorArray col;
	col.resize(vcount);
	const float scale = _scale_max();
	for (int i = 0; i < vcount; i++) {
		pos.set(i, world_verts[i]);
		col.set(i, values[i] <= 0.0f ? Color(0.15, 0.35, 1.0) : heat(values[i] / scale));
	}
	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = pos;
	arrays[Mesh::ARRAY_COLOR] = col;
	arrays[Mesh::ARRAY_INDEX] = indices;
	Ref<ArrayMesh> mesh;
	mesh.instantiate();
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
	if (overlay == nullptr) {
		overlay = memnew(MeshInstance3D);
		overlay->set_cast_shadows_setting(GeometryInstance3D::SHADOW_CASTING_SETTING_OFF);
		cloth->add_child(overlay, false, Node::INTERNAL_MODE_BACK); // no owner: never saved
		overlay->set_as_top_level(true);
		overlay->set_global_transform(Transform3D());
	}
	overlay->set_mesh(mesh);
	overlay->set_surface_override_material(0, overlay_material);
	// The overlay replaces the source mesh while painting rather than sitting
	// on top of it (no depth fighting). Only the render instance is hidden, so
	// nothing in the scene changes.
	MeshInstance3D *source = cloth->get_source_mesh_instance();
	if (source != nullptr && source->get_instance().is_valid()) {
		RS::get_singleton()->instance_set_visible(source->get_instance(), false);
		hidden_source = source->get_instance_id();
	}
}

void PhysXClothPaintPlugin::_commit(const String &p_action, const PackedFloat32Array &p_before) {
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(p_action);
	ur->add_do_method(cloth, "set_max_distances", values);
	ur->add_undo_method(cloth, "set_max_distances", p_before);
	ur->add_do_method(this, "_refresh_overlay");
	ur->add_undo_method(this, "_refresh_overlay");
	ur->commit_action(false);
	cloth->set_max_distances(values);
}

void PhysXClothPaintPlugin::_fill() {
	if (cloth == nullptr || !_prepare()) {
		return;
	}
	const PackedFloat32Array before = cloth->get_max_distances();
	const float v = (float)value_spin->get_value();
	for (int i = 0; i < values.size(); i++) {
		values.set(i, v);
	}
	_commit(TTR("Fill Cloth Max Distance"), before);
	if (paint_button->is_pressed()) {
		_update_overlay();
	}
}

void PhysXClothPaintPlugin::_ramp() {
	if (cloth == nullptr || !_prepare()) {
		return;
	}
	const PackedFloat32Array before = cloth->get_max_distances();
	values = cloth->compute_height_ramp();
	_commit(TTR("Ramp Cloth Max Distance"), before);
	if (paint_button->is_pressed()) {
		_update_overlay();
	}
}

bool PhysXClothPaintPlugin::_raycast(Camera3D *p_camera, const Vector2 &p_screen, Vector3 &r_hit) const {
	const Vector3 from = p_camera->project_ray_origin(p_screen);
	const Vector3 dir = p_camera->project_ray_normal(p_screen);
	float best = 1e30f;
	bool found = false;
	for (int t = 0; t + 2 < indices.size(); t += 3) {
		const Vector3 &a = world_verts[indices[t]];
		const Vector3 &b = world_verts[indices[t + 1]];
		const Vector3 &c = world_verts[indices[t + 2]];
		// Möller-Trumbore, both faces.
		const Vector3 e1 = b - a;
		const Vector3 e2 = c - a;
		const Vector3 pv = dir.cross(e2);
		const float det = e1.dot(pv);
		if (Math::abs(det) < 1e-12f) {
			continue;
		}
		const float inv = 1.0f / det;
		const Vector3 tv = from - a;
		const float u = tv.dot(pv) * inv;
		if (u < 0.0f || u > 1.0f) {
			continue;
		}
		const Vector3 qv = tv.cross(e1);
		const float v = dir.dot(qv) * inv;
		if (v < 0.0f || u + v > 1.0f) {
			continue;
		}
		const float dist = e2.dot(qv) * inv;
		if (dist > 0.0f && dist < best) {
			best = dist;
			found = true;
		}
	}
	if (found) {
		r_hit = from + dir * best;
	}
	return found;
}

void PhysXClothPaintPlugin::_apply_brush(const Vector3 &p_center, int p_mode) {
	const float radius = (float)radius_spin->get_value();
	const float strength = (float)strength_spin->get_value();
	const float target = p_mode == BRUSH_ERASE ? 0.0f : (float)value_spin->get_value();
	const float r2 = radius * radius;
	float avg = 0.0f;
	if (p_mode == BRUSH_SMOOTH) {
		float wsum = 0.0f;
		for (int i = 0; i < world_verts.size(); i++) {
			const float d2 = world_verts[i].distance_squared_to(p_center);
			if (d2 < r2) {
				avg += values[i];
				wsum += 1.0f;
			}
		}
		if (wsum <= 0.0f) {
			return;
		}
		avg /= wsum;
	}
	for (int i = 0; i < world_verts.size(); i++) {
		const float d2 = world_verts[i].distance_squared_to(p_center);
		if (d2 >= r2) {
			continue;
		}
		// Smooth falloff toward the brush edge.
		const float f = 1.0f - Math::sqrt(d2) / radius;
		const float w = strength * f * f * (3.0f - 2.0f * f);
		const float goal = p_mode == BRUSH_SMOOTH ? avg : target;
		values.set(i, Math::lerp(values[i], goal, w));
	}
}

EditorPlugin::AfterGUIInput PhysXClothPaintPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (cloth == nullptr || !paint_button->is_pressed() || values.size() != world_verts.size()) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}
	Ref<InputEventMouseButton> mb = p_event;
	if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT) {
		if (mb->is_pressed()) {
			stroking = true;
			stroke_before = cloth->get_max_distances();
			Vector3 hit;
			if (_raycast(p_camera, mb->get_position(), hit)) {
				_apply_brush(hit, mb->is_ctrl_pressed() ? BRUSH_SMOOTH : (mb->is_shift_pressed() ? BRUSH_ERASE : BRUSH_PAINT));
				_update_overlay();
			}
		} else if (stroking) {
			stroking = false;
			_commit(TTR("Paint Cloth Max Distance"), stroke_before);
		}
		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	}
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid() && stroking) {
		Vector3 hit;
		if (_raycast(p_camera, mm->get_position(), hit)) {
			_apply_brush(hit, mm->is_ctrl_pressed() ? BRUSH_SMOOTH : (mm->is_shift_pressed() ? BRUSH_ERASE : BRUSH_PAINT));
			_update_overlay();
		}
		return EditorPlugin::AFTER_GUI_INPUT_STOP;
	}
	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}
