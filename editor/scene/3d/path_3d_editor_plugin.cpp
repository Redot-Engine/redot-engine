/**************************************************************************/
/*  path_3d_editor_plugin.cpp                                             */
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

/**
 * @file path_3d_editor_plugin.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "path_3d_editor_plugin.h"

#include "core/math/geometry_2d.h"
#include "core/math/geometry_3d.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/separator.h"
#include "scene/resources/curve.h"

String Path3DGizmo::get_handle_name(int p_id, bool p_secondary) const {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return "";
	}

	// Primary handles: position.
	if (!p_secondary) {
		return TTR("Curve Point #") + itos(p_id);
	}

	// Secondary handles: in, out, tilt.
	const HandleInfo info = _secondary_handles_info[p_id];
	switch (info.type) {
		case HandleType::HANDLE_TYPE_IN:
			return TTR("Handle In #") + itos(info.point_idx);
		case HandleType::HANDLE_TYPE_OUT:
			return TTR("Handle Out #") + itos(info.point_idx);
		case HandleType::HANDLE_TYPE_TILT:
			return TTR("Handle Tilt #") + itos(info.point_idx);
	}

	return "";
}

Variant Path3DGizmo::get_handle_value(int p_id, bool p_secondary) const {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return Variant();
	}

	// Primary handles: position.
	if (!p_secondary) {
		original = c->get_point_position(p_id);
		return original;
	}

	// Secondary handles: in, out, tilt.
	const HandleInfo info = _secondary_handles_info[p_id];
	Vector3 ofs;
	switch (info.type) {
		case HandleType::HANDLE_TYPE_TILT:
			return c->get_point_tilt(info.point_idx);
		case HandleType::HANDLE_TYPE_IN:
			ofs = c->get_point_in(info.point_idx);
			break;
		case HandleType::HANDLE_TYPE_OUT:
			ofs = c->get_point_out(info.point_idx);
			break;
	}

	original = ofs + c->get_point_position(info.point_idx);
	return ofs;
}

void Path3DGizmo::set_snapped_point(int p_idx) {
	if (snapped_point_idx != p_idx) {
		snapped_point_idx = p_idx;
		// Only request a redraw on transitions; the point position itself may
		// not change when the cursor moves onto/off a collider.
		path->update_gizmos();
	}
}

void Path3DGizmo::set_handle(int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	const Transform3D gt = path->get_global_transform();
	const Transform3D gi = gt.affine_inverse();
	const Vector3 ray_from = p_camera->project_ray_origin(p_point);
	const Vector3 ray_dir = p_camera->project_ray_normal(p_point);
	const Plane p = Plane(p_camera->get_transform().basis.get_column(2), gt.xform(original));

	// Primary handles: position.
	if (!p_secondary) {
		Vector3 inters;
		// Special case for primary handle, the handle id equals control point id.
		const int idx = p_id;
		if (Path3DEditorPlugin::singleton->snap_to_collider && (!Path3DEditorPlugin::singleton->_edit.waiting_handle_physics || !Path3DEditorPlugin::singleton->_edit.in_physics_frame)) {
			Path3DEditorPlugin::singleton->_edit.waiting_handle_physics = true;
			Path3DEditorPlugin::singleton->_edit.gizmo_handle = p_id;
			Path3DEditorPlugin::singleton->_edit.gizmo_handle_secondary = p_secondary;
			Path3DEditorPlugin::singleton->_edit.gizmo_camera = p_camera;
			Path3DEditorPlugin::singleton->_edit.mouse_pos = p_point;
			return;
			// Only continue if inside physics frame and waiting for physics.
		}
		if (Path3DEditorPlugin::singleton->snap_to_collider) {
			PhysicsDirectSpaceState3D *ss = p_camera->get_world_3d()->get_direct_space_state();
			PhysicsDirectSpaceState3D::RayParameters ray_params;
			ray_params.from = ray_from;
			ray_params.to = ray_from + ray_dir * p_camera->get_far();
			PhysicsDirectSpaceState3D::RayResult result;
			if (ss->intersect_ray(ray_params, result)) {
				Vector3 snapped = result.position;

				// Constrain the collider hit the same way as the plane path.
				Path3DEditorPlugin::singleton->_apply_axis_locks(gt.xform(original), snapped);

				// If honoring the lock pulled the point off the surface, treat as a miss
				// and fall through to the plane intersect below.
				const float snap_tolerance = Node3DEditor::get_singleton()->is_snap_enabled()
						? Node3DEditor::get_singleton()->get_translate_snap()
						: 0.25f; // or a fixed tolerance of your choice

				if (snapped.distance_to(result.position) <= snap_tolerance) {
					Vector3 local = gi.xform(snapped);
					c->set_point_position(idx, local);
					set_snapped_point(idx);
					return;
				}
			}
			set_snapped_point(-1); // Missed, or lock pulled us off the surface.
		} else {
			set_snapped_point(-1);
		}
		if (p.intersects_ray(ray_from, ray_dir, &inters)) {
			if (Node3DEditor::get_singleton()->is_snap_enabled()) {
				float snap = Node3DEditor::get_singleton()->get_translate_snap();
				inters.snapf(snap);
			}

			Path3DEditorPlugin::singleton->_apply_axis_locks(gt.xform(original), inters);
			Vector3 local = gi.xform(inters);
			c->set_point_position(idx, local);
		}

		return;
	}

	// Secondary handles: in, out, tilt.
	const HandleInfo info = _secondary_handles_info[p_id];
	switch (info.type) {
		case HandleType::HANDLE_TYPE_OUT:
		case HandleType::HANDLE_TYPE_IN: {
			const int idx = info.point_idx;
			const Vector3 base = c->get_point_position(idx);

			Vector3 inters;
			if (p.intersects_ray(ray_from, ray_dir, &inters)) {
				if (!Path3DEditorPlugin::singleton->is_handle_clicked()) {
					orig_in_length = c->get_point_in(idx).length();
					orig_out_length = c->get_point_out(idx).length();
					Path3DEditorPlugin::singleton->set_handle_clicked(true);
				}

				Vector3 local = gi.xform(inters) - base;
				if (Node3DEditor::get_singleton()->is_snap_enabled()) {
					float snap = Node3DEditor::get_singleton()->get_translate_snap();
					local.snapf(snap);
				}

				if (snapped_point_idx < 0) {
					// Determine if control points should be swapped based on delta movement.
					// Only run on the next update after an overlap is detected, to get proper delta movement.
					if (control_points_overlapped) {
						control_points_overlapped = false;
						Vector3 delta = local - (info.type == HANDLE_TYPE_IN ? c->get_point_in(idx) : c->get_point_out(idx));
						Vector3 p0 = c->get_point_position(idx - 1) - base;
						Vector3 p1 = c->get_point_position(idx + 1) - base;
						HandleType new_type = Math::abs(delta.angle_to(p0)) < Math::abs(delta.angle_to(p1)) ? HANDLE_TYPE_IN : HANDLE_TYPE_OUT;
						if (info.type != new_type) {
							swapped_control_points_idx = idx;
						}
					}

					// Detect control points overlap.
					bool control_points_equal = c->get_point_in(idx).is_equal_approx(c->get_point_out(idx));
					if (idx > 0 && idx < (c->get_point_count() - 1) && control_points_equal) {
						control_points_overlapped = true;
					}
				}
				HandleType control_type = info.type;
				if (snapped_point_idx < 0 && swapped_control_points_idx == idx) {
					control_type = info.type == HANDLE_TYPE_IN ? HANDLE_TYPE_OUT : HANDLE_TYPE_IN;
				}

				if (control_type == HandleType::HANDLE_TYPE_IN) {
					c->set_point_in(idx, local);
					if (Path3DEditorPlugin::singleton->mirror_angle_enabled()) {
						c->set_point_out(idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -local : (-local.normalized() * orig_out_length));
					}
				} else {
					c->set_point_out(idx, local);
					if (Path3DEditorPlugin::singleton->mirror_angle_enabled()) {
						c->set_point_in(idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -local : (-local.normalized() * orig_in_length));
					}
				}
			}
			break;
		}
		case HandleType::HANDLE_TYPE_TILT: {
			const int idx = info.point_idx;
			const Vector3 position = c->get_point_position(idx);
			const Basis posture = c->get_point_baked_posture(idx);
			const Vector3 tangent = -posture.get_column(2);
			const Vector3 up = posture.get_column(1);
			const Plane tilt_plane_global = gt.xform(Plane(tangent, position));

			Vector3 intersection;

			if (tilt_plane_global.intersects_ray(ray_from, ray_dir, &intersection)) {
				Vector3 direction = gi.xform(intersection) - position;
				real_t tilt_angle = up.signed_angle_to(direction, tangent);

				if (Node3DEditor::get_singleton()->is_snap_enabled()) {
					real_t snap_degrees = Node3DEditor::get_singleton()->get_rotate_snap();
					tilt_angle = Math::deg_to_rad(Math::snapped(Math::rad_to_deg(tilt_angle), snap_degrees));
				}

				c->set_point_tilt(idx, tilt_angle);
			}
			break;
		}
	}
}

void Path3DGizmo::commit_handle(int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	swapped_control_points_idx = -1;
	control_points_overlapped = false;
	set_snapped_point(-1);

	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();

	// Primary handles: position.
	if (!p_secondary && !Path3DEditorPlugin::singleton->curve_edit->is_pressed()) {
		// Special case for primary handle, the handle id equals control point id.
		const int idx = p_id;
		if (p_cancel) {
			c->set_point_position(idx, p_restore);
			return;
		}
		ur->create_action(TTR("Set Curve Point Position"));
		ur->add_do_method(c.ptr(), "set_point_position", idx, c->get_point_position(idx));
		ur->add_undo_method(c.ptr(), "set_point_position", idx, p_restore);
		ur->commit_action();

		return;
	}

	// Secondary handles: in, out, tilt.
	const HandleInfo info = _secondary_handles_info[p_id];
	const int idx = info.point_idx;
	switch (info.type) {
		case HandleType::HANDLE_TYPE_OUT: {
			if (p_cancel) {
				c->set_point_out(idx, p_restore);

				return;
			}

			ur->create_action(TTR("Set Curve Out Position"));
			ur->add_do_method(c.ptr(), "set_point_out", idx, c->get_point_out(idx));
			ur->add_undo_method(c.ptr(), "set_point_out", idx, p_restore);

			if (Path3DEditorPlugin::singleton->mirror_angle_enabled()) {
				ur->add_do_method(c.ptr(), "set_point_in", idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -c->get_point_out(idx) : (-c->get_point_out(idx).normalized() * orig_in_length));
				ur->add_undo_method(c.ptr(), "set_point_in", idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -static_cast<Vector3>(p_restore) : (-static_cast<Vector3>(p_restore).normalized() * orig_in_length));
			}

			ur->commit_action();
			break;
		}
		case HandleType::HANDLE_TYPE_IN: {
			if (p_cancel) {
				c->set_point_in(idx, p_restore);
				return;
			}

			ur->create_action(TTR("Set Curve In Position"));
			ur->add_do_method(c.ptr(), "set_point_in", idx, c->get_point_in(idx));
			ur->add_undo_method(c.ptr(), "set_point_in", idx, p_restore);

			if (Path3DEditorPlugin::singleton->mirror_angle_enabled()) {
				ur->add_do_method(c.ptr(), "set_point_out", idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -c->get_point_in(idx) : (-c->get_point_in(idx).normalized() * orig_out_length));
				ur->add_undo_method(c.ptr(), "set_point_out", idx, Path3DEditorPlugin::singleton->mirror_length_enabled() ? -static_cast<Vector3>(p_restore) : (-static_cast<Vector3>(p_restore).normalized() * orig_out_length));
			}

			ur->commit_action();
			break;
		}
		case HandleType::HANDLE_TYPE_TILT: {
			if (p_cancel) {
				c->set_point_tilt(idx, p_restore);
				return;
			}
			ur->create_action(TTR("Set Curve Point Tilt"));
			ur->add_do_method(c.ptr(), "set_point_tilt", idx, c->get_point_tilt(idx));
			ur->add_undo_method(c.ptr(), "set_point_tilt", idx, p_restore);

			ur->commit_action();
			break;
		}
	}
}

void Path3DGizmo::redraw() {
	clear();

	Ref<StandardMaterial3D> path_thin_material = gizmo_plugin->get_material("path_thin_material", this);
	Ref<StandardMaterial3D> path_tilt_material = gizmo_plugin->get_material("path_tilt_material", this);
	Ref<StandardMaterial3D> path_tilt_muted_material = gizmo_plugin->get_material("path_tilt_muted_material", this);
	Ref<StandardMaterial3D> handles_material = gizmo_plugin->get_material("handles");
	Ref<StandardMaterial3D> first_pt_handle_material = gizmo_plugin->get_material("first_pt_handle");
	Ref<StandardMaterial3D> last_pt_handle_material = gizmo_plugin->get_material("last_pt_handle");
	Ref<StandardMaterial3D> closed_pt_handle_material = gizmo_plugin->get_material("closed_pt_handle");
	Ref<StandardMaterial3D> selected_handle_material = gizmo_plugin->get_material("selected_handle", this);
	Ref<StandardMaterial3D> sec_handles_material = gizmo_plugin->get_material("sec_handles");
	Ref<StandardMaterial3D> snapped_handle_material = gizmo_plugin->get_material("snapped_handle", this);

	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}

	debug_material = gizmo_plugin->get_material("path_material", this);

	Color path_color = path->get_debug_custom_color();
	if (path_color != Color(0.0, 0.0, 0.0)) {
		debug_material.instantiate();
		debug_material->set_albedo(path_color);
		debug_material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		debug_material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		debug_material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);
		debug_material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		debug_material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
	}

	real_t interval = 0.1;
	const real_t length = c->get_baked_length();

	// 1. Draw curve and bones if it is visible (alpha > 0.0).
	if (length > CMP_EPSILON && path_color.a > 0.0) {
		const int sample_count = int(length / interval) + 2;
		interval = length / (sample_count - 1); // Recalculate real interval length.

		Vector<Transform3D> frames;
		frames.resize(sample_count);

		{
			Transform3D *w = frames.ptrw();

			for (int i = 0; i < sample_count; i++) {
				w[i] = c->sample_baked_with_rotation(i * interval, true, true);
			}
		}

		const Transform3D *r = frames.ptr();

		Vector<Vector3> _collision_segments;
		_collision_segments.resize((sample_count - 1) * 2);
		Vector3 *_collisions_ptr = _collision_segments.ptrw();

		Vector<Vector3> bones;
		bones.resize(sample_count * 4);
		Vector3 *bones_ptr = bones.ptrw();

		Vector<Vector3> ribbon;
		ribbon.resize(sample_count);
		Vector3 *ribbon_ptr = ribbon.ptrw();

		for (int i = 0; i < sample_count; i++) {
			const Vector3 p1 = r[i].origin;
			const Vector3 side = r[i].basis.get_column(0);
			const Vector3 up = r[i].basis.get_column(1);
			const Vector3 forward = r[i].basis.get_column(2);

			// Collision segments.
			if (i != sample_count - 1) {
				const Vector3 p2 = r[i + 1].origin;
				_collisions_ptr[(i * 2)] = p1;
				_collisions_ptr[(i * 2) + 1] = p2;
			}

			// Path3D as a ribbon.
			ribbon_ptr[i] = p1;

			if (i % 4 == 0) {
				// Draw fish bone every 4 points to reduce visual noise and performance impact
				// (compared to drawing it for every point).
				const Vector3 p_left = p1 + (side + forward - up * 0.3) * 0.06;
				const Vector3 p_right = p1 + (-side + forward - up * 0.3) * 0.06;

				const int bone_idx = i * 4;

				bones_ptr[bone_idx] = p1;
				bones_ptr[bone_idx + 1] = p_left;
				bones_ptr[bone_idx + 2] = p1;
				bones_ptr[bone_idx + 3] = p_right;
			}
		}

		add_collision_segments(_collision_segments);
		add_lines(bones, debug_material);
		add_vertices(ribbon, debug_material, Mesh::PRIMITIVE_LINE_STRIP);
	}

	// 2. Draw handles when selected.
	if (Path3DEditorPlugin::singleton->get_edited_path() == path) {
		PackedVector3Array handle_lines;
		PackedVector3Array tilt_handle_lines;
		PackedVector3Array primary_handle_points;
		PackedVector3Array secondary_handle_points;
		PackedInt32Array collected_secondary_handle_ids; // Avoid shadowing member on Node3DEditorGizmo.

		_secondary_handles_info.resize(c->get_point_count() * 3);

		const float disk_size = EDITOR_GET("editors/3d_gizmos/gizmo_settings/path3d_tilt_disk_size");

		for (int idx = 0; idx < c->get_point_count(); idx++) {
			// Collect primary-handles.
			const Vector3 pos = c->get_point_position(idx);
			primary_handle_points.append(pos);

			HandleInfo info;
			info.point_idx = idx;

			// Collect in-handles except for the first point.
			if (idx > (c->is_closed() ? -1 : 0) && Path3DEditorPlugin::singleton->curve_edit_curve->is_pressed()) {
				const Vector3 in = c->get_point_in(idx);

				info.type = HandleType::HANDLE_TYPE_IN;
				const int handle_idx = idx * 3 + 0;
				collected_secondary_handle_ids.append(handle_idx);
				_secondary_handles_info.write[handle_idx] = info;

				secondary_handle_points.append(pos + in);
				handle_lines.append(pos);
				handle_lines.append(pos + in);
			}

			// Collect out-handles except for the last point.
			if (idx < (c->is_closed() ? c->get_point_count() : c->get_point_count() - 1) && Path3DEditorPlugin::singleton->curve_edit_curve->is_pressed()) {
				const Vector3 out = c->get_point_out(idx);

				info.type = HandleType::HANDLE_TYPE_OUT;
				const int handle_idx = idx * 3 + 1;
				collected_secondary_handle_ids.append(handle_idx);
				_secondary_handles_info.write[handle_idx] = info;

				secondary_handle_points.append(pos + out);
				handle_lines.append(pos);
				handle_lines.append(pos + out);
			}

			// Collect tilt-handles.
			if (Path3DEditorPlugin::singleton->curve_edit_tilt->is_pressed()) {
				// Tilt handle.
				{
					info.type = HandleType::HANDLE_TYPE_TILT;
					const int handle_idx = idx * 3 + 2;
					collected_secondary_handle_ids.append(handle_idx);
					_secondary_handles_info.write[handle_idx] = info;

					const Basis posture = c->get_point_baked_posture(idx, true);
					const Vector3 up = posture.get_column(1);
					secondary_handle_points.append(pos + up * disk_size);
					tilt_handle_lines.append(pos);
					tilt_handle_lines.append(pos + up * disk_size);
				}

				// Tilt disk.
				{
					const Basis posture = c->get_point_baked_posture(idx, false);
					const Vector3 up = posture.get_column(1);
					const Vector3 side = posture.get_column(0);

					PackedVector3Array disk;
					disk.append(pos);

					const int n = 36;
					for (int i = 0; i <= n; i++) {
						const float a = Math::TAU * i / n;
						const Vector3 edge = std::sin(a) * side + std::cos(a) * up;
						disk.append(pos + edge * disk_size);
					}
					add_vertices(disk, debug_material, Mesh::PRIMITIVE_LINE_STRIP);
				}
			}
		}

		if (handle_lines.size() > 1) {
			add_lines(handle_lines, path_thin_material);
		}

		if (tilt_handle_lines.size() > 1) {
			add_lines(tilt_handle_lines, path_tilt_material);
		}

		if (!Path3DEditorPlugin::singleton->curve_edit->is_pressed() && primary_handle_points.size()) {
			// Need to define indices separately.
			// Build ids from the ORIGINAL point count so they stay equal to curve point indices.
			const int orig_pc = primary_handle_points.size();
			Vector<int> idx;
			idx.resize(orig_pc);
			int *idx_ptr = idx.ptrw();
			for (int j = 0; j < orig_pc; j++) {
				idx_ptr[j] = j;
			}

			// Extract the snapped point (if any) BEFORE computing pc / removing
			// first and last, so it can be drawn with its own material.
			PackedVector3Array snapped_handle_point;
			Vector<int> snapped_id;
			if (snapped_point_idx >= 0 && snapped_point_idx < orig_pc) {
				snapped_handle_point.append(primary_handle_points[snapped_point_idx]);
				snapped_id.append(snapped_point_idx);
				primary_handle_points.remove_at(snapped_point_idx);
				idx.remove_at(snapped_point_idx);
			}

			// Extract selected points the same way — they get the blue material.
			// Iterate backwards so remove_at doesn't shift unvisited indices.
			PackedVector3Array sel_handle_point;
			Vector<int> sel_id;
			for (int j = idx.size() - 1; j >= 0; j--) {
				if (is_subgizmo_selected(idx[j])) {
					sel_handle_point.append(primary_handle_points[j]);
					sel_id.append(idx[j]);
					primary_handle_points.remove_at(j);
					idx.remove_at(j);
				}
			}

			// Point count — computed AFTER the snapped point was removed.
			const int pc = primary_handle_points.size();

			if (pc > 0) {
				// Initialize arrays for first point.
				PackedVector3Array first_pt_handle_point;
				Vector<int> first_pt_id;
				first_pt_handle_point.append(primary_handle_points[0]);
				first_pt_id.append(idx[0]);

				// Initialize arrays and add handle for last point if needed.
				if (pc > 1) {
					PackedVector3Array last_pt_handle_point;
					Vector<int> last_pt_id;
					last_pt_handle_point.append(primary_handle_points[pc - 1]);
					last_pt_id.append(idx[pc - 1]);
					primary_handle_points.remove_at(pc - 1);
					idx.remove_at(pc - 1);
					add_handles(last_pt_handle_point, c->is_closed() ? handles_material : last_pt_handle_material, last_pt_id);
				}

				// Add handle for first point.
				primary_handle_points.remove_at(0);
				idx.remove_at(0);
				add_handles(first_pt_handle_point, c->is_closed() ? closed_pt_handle_material : first_pt_handle_material, first_pt_id);

				// Add handles for remaining intermediate points.
				if (!primary_handle_points.is_empty()) {
					add_handles(primary_handle_points, handles_material, idx);
				}
			}

			// Add selected and snapped points with their own materials.
			// These are drawn regardless of pc, so they stay visible even when
			// every regular point was extracted above (e.g. all points selected,
			// or the single point of a one-point curve is selected).
			if (!sel_handle_point.is_empty()) {
				add_handles(sel_handle_point, selected_handle_material, sel_id);
			}
			if (!snapped_handle_point.is_empty()) {
				add_handles(snapped_handle_point, snapped_handle_material, snapped_id);
			}
		}

		if (secondary_handle_points.size()) {
			add_handles(secondary_handle_points, sec_handles_material, collected_secondary_handle_ids, false, true);
		}
		// Draw the gizmo plugin manually, because handles are registered. In which case, the caller code skips drawing the gizmo plugin.
		gizmo_plugin->redraw(this);
	}
}

void Path3DGizmo::_update_transform_gizmo() {
	Node3DEditor::get_singleton()->update_transform_gizmo();
}

Path3DGizmo::Path3DGizmo(Path3D *p_path) {
	path = p_path;
	set_node_3d(p_path);
	orig_in_length = 0;
	orig_out_length = 0;

	// Connecting to a signal once, rather than plaguing the implementation with calls to `Node3DEditor::update_transform_gizmo`.
	path->connect("curve_changed", callable_mp(this, &Path3DGizmo::_update_transform_gizmo));
	path->connect("debug_color_changed", callable_mp(this, &Path3DGizmo::redraw));

	Path3DEditorPlugin::singleton->curve_edit->connect(SceneStringName(pressed), callable_mp(this, &Path3DGizmo::redraw));
	Path3DEditorPlugin::singleton->curve_edit_curve->connect(SceneStringName(pressed), callable_mp(this, &Path3DGizmo::redraw));
	Path3DEditorPlugin::singleton->curve_create->connect(SceneStringName(pressed), callable_mp(this, &Path3DGizmo::redraw));
	Path3DEditorPlugin::singleton->curve_del->connect(SceneStringName(pressed), callable_mp(this, &Path3DGizmo::redraw));
	Path3DEditorPlugin::singleton->curve_closed->connect(SceneStringName(pressed), callable_mp(this, &Path3DGizmo::redraw));
}

EditorPlugin::AfterGUIInput Path3DEditorPlugin::forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) {
	if (!path) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return EditorPlugin::AFTER_GUI_INPUT_PASS;
	}
	Transform3D gt = path->get_global_transform();
	Transform3D it = gt.affine_inverse();

	static const int click_dist = 10; //should make global

	Ref<InputEventKey> k = p_event;
	if (k.is_valid() && k->is_pressed() && curve_edit->is_pressed() &&
			(k->get_keycode() == Key::BACKSPACE || k->get_keycode() == Key::KEY_DELETE)) {
		// Only consume if we actually have points selected.
		Ref<EditorNode3DGizmo> gz;
		for (Ref<Node3DGizmo> g : path->get_gizmos()) {
			gz = g;
			if (gz.is_valid()) {
				break;
			}
		}
		if (gz.is_valid() && !gz->get_subgizmo_selection().is_empty()) {
			_delete_selected_points();
			p_camera->get_viewport()->set_input_as_handled();
			return EditorPlugin::AFTER_GUI_INPUT_STOP;
		}
	}

	Ref<InputEventMouseButton> mb = p_event;

	if (mb.is_valid()) {
		Point2 mbpos(mb->get_position().x, mb->get_position().y);

		Node3DEditorViewport *viewport = nullptr;
		for (uint32_t i = 0; i < Node3DEditor::VIEWPORTS_COUNT; i++) {
			Node3DEditorViewport *vp = Node3DEditor::get_singleton()->get_editor_viewport(i);
			if (vp->get_camera_3d() == p_camera) {
				viewport = vp;
				break;
			}
		}

		ERR_FAIL_NULL_V(viewport, EditorPlugin::AFTER_GUI_INPUT_PASS);

		if (!mb->is_pressed()) {
			set_handle_clicked(false);
		}

		// Add point branch
		if (mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT &&
				(curve_create->is_pressed() ||
						((curve_edit->is_pressed()) && mb->is_command_or_control_pressed()))) {
			//click into curve, break it down
			Vector<Vector3> v3a = c->tessellate();
			int rc = v3a.size();
			int closest_seg = -1;
			Vector3 closest_seg_point;

			if (rc >= 2) {
				int idx = 0;
				const Vector3 *r = v3a.ptr();
				float closest_d = 1e20;

				if (viewport->point_to_screen(gt.xform(c->get_point_position(0))).distance_to(mbpos) < click_dist) {
					return EditorPlugin::AFTER_GUI_INPUT_PASS; //nope, existing
				}

				for (int i = 0; i < c->get_point_count() - 1; i++) {
					//find the offset and point index of the place to break up
					int j = idx;
					if (viewport->point_to_screen(gt.xform(c->get_point_position(i + 1))).distance_to(mbpos) < click_dist) {
						return EditorPlugin::AFTER_GUI_INPUT_PASS; //nope, existing
					}

					while (j < rc && c->get_point_position(i + 1) != r[j]) {
						Vector3 from = r[j];
						Vector3 to = r[j + 1];
						real_t cdist = from.distance_to(to);
						from = gt.xform(from);
						to = gt.xform(to);
						if (cdist > 0) {
							const Vector2 segment_a = viewport->point_to_screen(from);
							const Vector2 segment_b = viewport->point_to_screen(to);
							Vector2 inters = Geometry2D::get_closest_point_to_segment(mbpos, segment_a, segment_b);
							float d = inters.distance_to(mbpos);

							if (d < 10 && d < closest_d) {
								closest_d = d;
								closest_seg = i;
								Vector3 ray_from = viewport->get_ray_pos(mbpos);
								Vector3 ray_dir = viewport->get_ray(mbpos);

								Vector3 ra, rb;
								Geometry3D::get_closest_points_between_segments(ray_from, ray_from + ray_dir * 4096, from, to, ra, rb);

								closest_seg_point = it.xform(rb);
							}
						}
						j++;
					}
					if (idx == j) {
						idx++; //force next
					} else {
						idx = j; //swap
					}

					if (j == rc) {
						break;
					}
				}
			}

			EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
			if (closest_seg != -1) {
				//subdivide

				ur->create_action(TTR("Split Path"));
				ur->add_do_method(c.ptr(), "add_point", closest_seg_point, Vector3(), Vector3(), closest_seg + 1);
				ur->add_undo_method(c.ptr(), "remove_point", closest_seg + 1);
				ur->commit_action();
				return EditorPlugin::AFTER_GUI_INPUT_STOP;

			} else {
				Vector3 origin;
				if (c->get_point_count() == 0) {
					origin = path->get_transform().get_origin();
				} else {
					origin = gt.xform(c->get_point_position(c->get_point_count() - 1));
				}

				Vector3 ray_from = viewport->get_ray_pos(mbpos);
				Vector3 ray_dir = viewport->get_ray(mbpos);

				if (snap_to_collider) {
					_edit.click_ray_pos = ray_from;
					_edit.click_ray_dir = ray_dir * p_camera->get_far();
					_edit.gizmo_camera = p_camera;
					_edit.origin = origin;
					_edit.waiting_point_physics = true;
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				}

				Plane p(p_camera->get_transform().basis.get_column(2), origin);

				Vector3 inters;
				if (p.intersects_ray(ray_from, ray_dir, &inters)) {
					if (c->get_point_count() > 0) {
						const Vector3 last_global = gt.xform(c->get_point_position(c->get_point_count() - 1));
						_apply_axis_locks(last_global, inters);
					}
					ur->create_action(TTR("Add Point to Curve"));
					ur->add_do_method(c.ptr(), "add_point", it.xform(inters), Vector3(), Vector3(), -1);
					ur->add_undo_method(c.ptr(), "remove_point", c->get_point_count());
					ur->commit_action();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				}

				//add new at pos
			}

		} else if (mb->is_pressed() && ((mb->get_button_index() == MouseButton::LEFT && curve_del->is_pressed()) || (mb->get_button_index() == MouseButton::RIGHT && curve_edit->is_pressed()))) {
			const float disk_size = EDITOR_GET("editors/3d_gizmos/gizmo_settings/path3d_tilt_disk_size");
			for (int i = 0; i < c->get_point_count(); i++) {
				real_t dist_to_p = viewport->point_to_screen(gt.xform(c->get_point_position(i))).distance_to(mbpos);
				real_t dist_to_p_out = viewport->point_to_screen(gt.xform(c->get_point_position(i) + c->get_point_out(i))).distance_to(mbpos);
				real_t dist_to_p_in = viewport->point_to_screen(gt.xform(c->get_point_position(i) + c->get_point_in(i))).distance_to(mbpos);
				real_t dist_to_p_up = viewport->point_to_screen(gt.xform(c->get_point_position(i) + c->get_point_baked_posture(i, true).get_column(1) * disk_size)).distance_to(mbpos);

				// Find the offset and point index of the place to break up.
				// Also check for the control points.
				if (dist_to_p < click_dist) {
					EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
					ur->create_action(TTR("Remove Path Point"));
					ur->add_do_method(c.ptr(), "remove_point", i);
					ur->add_undo_method(c.ptr(), "add_point", c->get_point_position(i), c->get_point_in(i), c->get_point_out(i), i);
					ur->commit_action();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				} else if (dist_to_p_out < click_dist) {
					EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
					ur->create_action(TTR("Reset Out-Control Point"));
					ur->add_do_method(c.ptr(), "set_point_out", i, Vector3());
					ur->add_undo_method(c.ptr(), "set_point_out", i, c->get_point_out(i));
					ur->commit_action();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				} else if (dist_to_p_in < click_dist) {
					EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
					ur->create_action(TTR("Reset In-Control Point"));
					ur->add_do_method(c.ptr(), "set_point_in", i, Vector3());
					ur->add_undo_method(c.ptr(), "set_point_in", i, c->get_point_in(i));
					ur->commit_action();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				} else if (dist_to_p_up < click_dist) {
					EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
					ur->create_action(TTR("Reset Point Tilt"));
					ur->add_do_method(c.ptr(), "set_point_tilt", i, 0.0f);
					ur->add_undo_method(c.ptr(), "set_point_tilt", i, c->get_point_tilt(i));
					ur->commit_action();
					return EditorPlugin::AFTER_GUI_INPUT_STOP;
				}
			}
		}
	}

	return EditorPlugin::AFTER_GUI_INPUT_PASS;
}

void Path3DEditorPlugin::_delete_selected_points() {
	ERR_FAIL_NULL(path);
	Ref<Curve3D> c = path->get_curve();
	ERR_FAIL_COND(c.is_null());

	// Grab the gizmo to read the subgizmo (point) selection.
	Ref<EditorNode3DGizmo> gizmo;
	for (Ref<Node3DGizmo> g : path->get_gizmos()) {
		gizmo = g;
		if (gizmo.is_valid()) {
			break;
		}
	}
	ERR_FAIL_COND(gizmo.is_null());

	Vector<int> selection = gizmo->get_subgizmo_selection();
	if (selection.is_empty()) {
		return;
	}
	selection.sort();

	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Remove Path Points"), UndoRedo::MERGE_DISABLE, c.ptr());
	ur->add_do_method(Node3DEditor::get_singleton(), "_clear_subgizmo_selection", path);

	// Do: remove highest index first so lower indices stay valid.
	for (int i = selection.size() - 1; i >= 0; i--) { // descending for removal
		ur->add_do_method(c.ptr(), "remove_point", selection[i]);
	}
	// Undo: re-add lowest index first.
	for (int i = 0; i < selection.size(); i++) { // ascending for restore
		const int idx = selection[i];
		ur->add_undo_method(c.ptr(), "add_point", c->get_point_position(idx), c->get_point_in(idx), c->get_point_out(idx), idx);
		ur->add_undo_method(c.ptr(), "set_point_tilt", idx, c->get_point_tilt(idx));
	}
	ur->commit_action();
}

void Path3DEditorPlugin::edit(Object *p_object) {
	if (p_object) {
		path = Object::cast_to<Path3D>(p_object);
		if (path) {
			if (path->get_curve().is_valid()) {
				path->get_curve()->emit_signal(CoreStringName(changed));
			}
			_update_toolbar();
		}
	} else {
		Path3D *pre = path;
		path = nullptr;
		if (pre && pre->get_curve().is_valid()) {
			pre->get_curve()->emit_signal(CoreStringName(changed));
		}
	}

	update_overlays();
	// collision_polygon_editor->edit(Object::cast_to<Node>(p_object));
}

bool Path3DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class("Path3D");
}

void Path3DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		topmenu_bar->show();
	} else {
		topmenu_bar->hide();

		{
			Path3D *pre = path;
			path = nullptr;
			if (pre && pre->get_curve().is_valid()) {
				pre->get_curve()->emit_signal(CoreStringName(changed));
			}
		}
	}

	set_physics_process(p_visible);
}

void Path3DEditorPlugin::_mode_changed(int p_mode) {
	curve_create->set_pressed_no_signal(p_mode == MODE_CREATE);
	curve_edit_curve->set_pressed_no_signal(p_mode == MODE_EDIT_CURVE);
	curve_edit_tilt->set_pressed_no_signal(p_mode == MODE_EDIT_TILT);
	curve_edit->set_pressed_no_signal(p_mode == MODE_EDIT);
	curve_del->set_pressed_no_signal(p_mode == MODE_DELETE);

	Node3DEditor::get_singleton()->clear_subgizmo_selection();
}

void Path3DEditorPlugin::_toggle_closed_curve() {
	Ref<Curve3D> c = path->get_curve();
	if (c.is_null()) {
		return;
	}
	if (c->get_point_count() < 2) {
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Toggle Open/Closed Curve"));
	ur->add_do_method(c.ptr(), "set_closed", !c.ptr()->is_closed());
	ur->add_undo_method(c.ptr(), "set_closed", c.ptr()->is_closed());
	ur->commit_action();
}

void Path3DEditorPlugin::_apply_axis_locks(const Vector3 &p_reference_global, Vector3 &r_global_point) const {
	if (axis_lock_x->is_pressed()) {
		r_global_point.x = p_reference_global.x;
	}
	if (axis_lock_y->is_pressed()) {
		r_global_point.y = p_reference_global.y;
	}
	if (axis_lock_z->is_pressed()) {
		r_global_point.z = p_reference_global.z;
	}
}

void Path3DEditorPlugin::_update_axis_lock_icons() {
	const StringName axis_names[3] = { SNAME("X_Letter"), SNAME("Y_Letter"), SNAME("Z_Letter") };
	const StringName lock_names[3] = { SNAME("X_Letter_Locked"), SNAME("Y_Letter_Locked"), SNAME("Z_Letter_Locked") };
	Button *buttons[3] = { axis_lock_x, axis_lock_y, axis_lock_z };

	// These icons actually use a separate SVG for their toggled state, so we don't want the tint to occur on pressed.
	// This overrides that.
	axis_lock_x->add_theme_color_override(SNAME("icon_pressed_color"), Color(1, 1, 1));
	axis_lock_y->add_theme_color_override(SNAME("icon_pressed_color"), Color(1, 1, 1));
	axis_lock_z->add_theme_color_override(SNAME("icon_pressed_color"), Color(1, 1, 1));
	for (int i = 0; i < 3; i++) {
		buttons[i]->set_button_icon(topmenu_bar->get_editor_theme_icon(buttons[i]->is_pressed() ? lock_names[i] : axis_names[i]));
	}
}

void Path3DEditorPlugin::_handle_option_pressed(int p_option) {
	PopupMenu *pm;
	pm = handle_menu->get_popup();

	switch (p_option) {
		case HANDLE_OPTION_ANGLE: {
			bool is_checked = pm->is_item_checked(HANDLE_OPTION_ANGLE);
			mirror_handle_angle = !is_checked;
			pm->set_item_checked(HANDLE_OPTION_ANGLE, mirror_handle_angle);
			pm->set_item_disabled(HANDLE_OPTION_LENGTH, !mirror_handle_angle);
		} break;
		case HANDLE_OPTION_LENGTH: {
			bool is_checked = pm->is_item_checked(HANDLE_OPTION_LENGTH);
			mirror_handle_length = !is_checked;
			pm->set_item_checked(HANDLE_OPTION_LENGTH, mirror_handle_length);
		} break;
		case HANDLE_OPTION_SNAP_COLLIDER: {
			bool is_checked = pm->is_item_checked(HANDLE_OPTION_SNAP_COLLIDER);
			snap_to_collider = !is_checked;
			pm->set_item_checked(HANDLE_OPTION_SNAP_COLLIDER, snap_to_collider);
		} break;
	}
}

void Path3DEditorPlugin::_create_curve() {
	ERR_FAIL_NULL(path);

	Ref<Curve3D> new_curve;
	new_curve.instantiate();

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Create Curve in Path3D"));
	undo_redo->add_do_property(path, "curve", new_curve);
	undo_redo->add_undo_property(path, "curve", Ref<Curve3D>());
	undo_redo->add_do_method(this, "_update_toolbar");
	undo_redo->add_undo_method(this, "_update_toolbar");
	undo_redo->commit_action();
}

void Path3DEditorPlugin::_confirm_clear_points() {
	if (!path || path->get_curve().is_null() || path->get_curve()->get_point_count() == 0) {
		return;
	}
	clear_points_dialog->reset_size();
	clear_points_dialog->popup_centered();
}

void Path3DEditorPlugin::_clear_points() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	PackedVector3Array points = path->get_curve()->get_points().duplicate();

	undo_redo->create_action(TTR("Clear Curve Points"));
	undo_redo->add_do_method(this, "_clear_curve_points");
	undo_redo->add_undo_method(this, "_restore_curve_points", points);
	undo_redo->commit_action();
}

void Path3DEditorPlugin::_clear_curve_points() {
	if (!path || path->get_curve().is_null() || path->get_curve()->get_point_count() == 0) {
		return;
	}
	Ref<Curve3D> curve = path->get_curve();
	curve->set_closed(false);
	curve->clear_points();
}

void Path3DEditorPlugin::_restore_curve_points(const PackedVector3Array &p_points) {
	if (!path || path->get_curve().is_null()) {
		return;
	}
	Ref<Curve3D> curve = path->get_curve();

	if (curve->get_point_count() > 0) {
		curve->clear_points();
	}

	for (int i = 0; i < p_points.size(); i += 3) {
		curve->add_point(p_points[i + 2], p_points[i], p_points[i + 1]);
	}
}

void Path3DEditorPlugin::_smooth_all_points() {
	ERR_FAIL_NULL(path);
	Ref<Curve3D> c = path->get_curve();
	ERR_FAIL_COND(c.is_null());
	if (c->get_point_count() < 3) {
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Smooth All Points"));
	ur->add_do_method(c.ptr(), "smooth_all_points");
	ur->add_undo_method(c.ptr(), "_set_data", c->call("_get_data"));
	ur->commit_action();
}

void Path3DEditorPlugin::_reset_all_points_handles() {
	ERR_FAIL_NULL(path);
	Ref<Curve3D> c = path->get_curve();
	ERR_FAIL_COND(c.is_null());
	if (c->get_point_count() == 0) {
		return;
	}
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	ur->create_action(TTR("Reset All Handles"));
	ur->add_do_method(c.ptr(), "reset_all_points_handles");
	ur->add_undo_method(c.ptr(), "_set_data", c->call("_get_data"));
	ur->commit_action();
}

void Path3DEditorPlugin::_update_theme() {
	curve_edit->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveEdit")));
	curve_edit_curve->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveCurve")));
	curve_edit_tilt->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveTilt")));
	curve_create->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveCreate")));
	curve_del->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveDelete")));
	curve_closed->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveClose")));
	curve_clear_points->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("Clear")));
	curve_smooth->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveInOut")));
	curve_reset_handles->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("CurveLinear")));
	create_curve_button->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("Curve3D")));

	axis_lock_x->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("X_Letter")));
	axis_lock_y->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("Y_Letter")));
	axis_lock_z->set_button_icon(topmenu_bar->get_editor_theme_icon(SNAME("Z_Letter")));

	separator_line_style->set_color(axis_separator_left->get_theme_color("accent_color", "Editor"));

	_update_axis_lock_icons(); // Buttons have a different icon for locked/not locked
}

void Path3DEditorPlugin::_update_toolbar() {
	if (!path) {
		return;
	}
	bool has_curve = path->get_curve().is_valid();
	toolbar->set_visible(has_curve);
	create_curve_button->set_visible(!has_curve);
}

void Path3DEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_toolbar"), &Path3DEditorPlugin::_update_toolbar);
	ClassDB::bind_method(D_METHOD("_clear_curve_points"), &Path3DEditorPlugin::_clear_curve_points);
	ClassDB::bind_method(D_METHOD("_restore_curve_points"), &Path3DEditorPlugin::_restore_curve_points);
}

void Path3DEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (!path) {
				return;
			}

			if (!EditorSettings::get_singleton()->check_changed_settings_in_group("editors/3d_gizmos/gizmo_settings")) {
				return;
			}

			path->update_gizmos();
		} break;
		case NOTIFICATION_PHYSICS_PROCESS: {
			if (_edit.waiting_point_physics) {
				_edit.waiting_point_physics = false;
				const Transform3D gt = path->get_global_transform();
				const Transform3D it = gt.affine_inverse();
				Ref<Curve3D> c = path->get_curve();
				EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
				PhysicsDirectSpaceState3D *ss = get_tree()->get_root()->get_world_3d()->get_direct_space_state();
				if (ss) {
					PhysicsDirectSpaceState3D::RayParameters ray_params;
					PhysicsDirectSpaceState3D::RayResult result;
					ray_params.from = _edit.click_ray_pos;
					ray_params.to = ray_params.from + _edit.click_ray_dir;
					bool hit_something = false;
					Vector3 inters;
					if (ss->intersect_ray(ray_params, result)) {
						// print_line("ray HIT at ", result.position, " collider=", result.collider_id);
						inters = result.position;
						hit_something = true;
					} else {
						// print_line("ray MISSED, plane fallback");
						Plane p(_edit.gizmo_camera->get_transform().basis.get_column(2), _edit.origin);
						if (p.intersects_ray(ray_params.from, _edit.click_ray_dir, &inters)) {
							hit_something = true;
						}
					}
					if (hit_something) {
						if (c->get_point_count() > 0) {
							const Vector3 last_global = gt.xform(c->get_point_position(c->get_point_count() - 1));
							_apply_axis_locks(last_global, inters);
						}
						ur->create_action(TTR("Add Point to Curve"));
						ur->add_do_method(c.ptr(), "add_point", it.xform(inters), Vector3(), Vector3(), -1);
						ur->add_undo_method(c.ptr(), "remove_point", c->get_point_count());
						ur->commit_action();
					}
				}
			}
			if (_edit.waiting_handle_physics) {
				_edit.in_physics_frame = true;

				// Find gizmo reference.
				Vector<Ref<Node3DGizmo>> gizmos = path->get_gizmos();
				for (Ref<EditorNode3DGizmo> seg : gizmos) {
					if (seg.is_valid()) {
						_edit.gizmo = seg;
						break;
					}
				}

				_edit.gizmo->set_handle(_edit.gizmo_handle, _edit.gizmo_handle_secondary, _edit.gizmo_camera, _edit.mouse_pos);
				_edit.in_physics_frame = false;
				_edit.waiting_handle_physics = false;
			}
		}
	}
}

Path3DEditorPlugin::Path3DEditorPlugin() {
	singleton = this;
	mirror_handle_angle = true;
	mirror_handle_length = true;

	Ref<Path3DGizmoPlugin> gizmo_plugin;
	gizmo_plugin.instantiate();
	Node3DEditor::get_singleton()->add_gizmo_plugin(gizmo_plugin);
	path_3d_gizmo_plugin = gizmo_plugin;

	topmenu_bar = memnew(HBoxContainer);
	topmenu_bar->hide();

	toolbar = memnew(HBoxContainer);
	topmenu_bar->add_child(toolbar);

	curve_edit = memnew(Button);
	curve_edit->set_theme_type_variation(SceneStringName(FlatButton));
	curve_edit->set_toggle_mode(true);
	curve_edit->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_edit->set_tooltip_text(TTR("Select Points") + "\n" + TTR("Shift+Click: Select multiple Points") + "\n" + keycode_get_string((Key)KeyModifierMask::CMD_OR_CTRL) + TTR("Click: Add Point") + "\n" + TTR("Right Click: Delete Point"));
	curve_edit->set_accessibility_name(TTRC("Select Points"));
	toolbar->add_child(curve_edit);
	curve_edit->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_mode_changed).bind(MODE_EDIT));

	curve_edit_curve = memnew(Button);
	curve_edit_curve->set_theme_type_variation(SceneStringName(FlatButton));
	curve_edit_curve->set_toggle_mode(true);
	curve_edit_curve->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_edit_curve->set_tooltip_text(TTR("Select Control Points") + "\n" + TTR("Shift+Click: Drag out Control Points"));
	curve_edit_curve->set_accessibility_name(TTRC("Select Control Points"));
	toolbar->add_child(curve_edit_curve);
	curve_edit_curve->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_mode_changed).bind(MODE_EDIT_CURVE));

	curve_edit_tilt = memnew(Button);
	curve_edit_tilt->set_theme_type_variation(SceneStringName(FlatButton));
	curve_edit_tilt->set_toggle_mode(true);
	curve_edit_tilt->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_edit_tilt->set_tooltip_text(TTR("Select Tilt Handles"));
	toolbar->add_child(curve_edit_tilt);
	curve_edit_tilt->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_mode_changed).bind(MODE_EDIT_TILT));

	curve_create = memnew(Button);
	curve_create->set_theme_type_variation(SceneStringName(FlatButton));
	curve_create->set_toggle_mode(true);
	curve_create->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_create->set_tooltip_text(TTR("Add Point (in empty space)") + "\n" + TTR("Split Segment (in curve)"));
	curve_create->set_accessibility_name(TTRC("Add Point (in empty space)"));
	toolbar->add_child(curve_create);
	curve_create->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_mode_changed).bind(MODE_CREATE));

	curve_del = memnew(Button);
	curve_del->set_theme_type_variation(SceneStringName(FlatButton));
	curve_del->set_toggle_mode(true);
	curve_del->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_del->set_tooltip_text(TTR("Delete Point"));
	toolbar->add_child(curve_del);
	curve_del->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_mode_changed).bind(MODE_DELETE));

	curve_closed = memnew(Button);
	curve_closed->set_theme_type_variation(SceneStringName(FlatButton));
	curve_closed->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_closed->set_tooltip_text(TTR("Close Curve"));
	toolbar->add_child(curve_closed);
	curve_closed->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_toggle_closed_curve));

	separator_line_style = memnew(StyleBoxLine);
	separator_line_style->set_thickness(2);
	separator_line_style->set_vertical(true);

	axis_separator_left = memnew(VSeparator);
	axis_separator_left->add_theme_style_override("separator", separator_line_style);
	toolbar->add_child(axis_separator_left);

	axis_lock_x = memnew(Button);
	axis_lock_x->set_theme_type_variation(SceneStringName(FlatButton));
	axis_lock_x->set_toggle_mode(true);
	axis_lock_x->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	axis_lock_x->set_tooltip_text(TTR("Lock X Axis") + "\n" + TTR("New points keep the last point's X coordinate"));
	axis_lock_x->set_accessibility_name(TTRC("Lock X Axis"));
	toolbar->add_child(axis_lock_x);

	axis_lock_y = memnew(Button);
	axis_lock_y->set_theme_type_variation(SceneStringName(FlatButton));
	axis_lock_y->set_toggle_mode(true);
	axis_lock_y->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	axis_lock_y->set_tooltip_text(TTR("Lock Y Axis") + "\n" + TTR("New points keep the last point's Y coordinate"));
	axis_lock_y->set_accessibility_name(TTRC("Lock Y Axis"));
	toolbar->add_child(axis_lock_y);

	axis_lock_z = memnew(Button);
	axis_lock_z->set_theme_type_variation(SceneStringName(FlatButton));
	axis_lock_z->set_toggle_mode(true);
	axis_lock_z->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	axis_lock_z->set_tooltip_text(TTR("Lock Z Axis") + "\n" + TTR("New points keep the last point's Z coordinate"));
	axis_lock_z->set_accessibility_name(TTRC("Lock Z Axis"));
	toolbar->add_child(axis_lock_z);

	axis_separator_right = memnew(VSeparator);
	axis_separator_right->add_theme_style_override("separator", separator_line_style);
	toolbar->add_child(axis_separator_right);

	curve_smooth = memnew(Button);
	curve_smooth->set_theme_type_variation(SceneStringName(FlatButton));
	curve_smooth->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_smooth->set_tooltip_text(TTR("Smooth All Points"));
	curve_smooth->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_smooth_all_points));
	toolbar->add_child(curve_smooth);

	curve_reset_handles = memnew(Button);
	curve_reset_handles->set_theme_type_variation(SceneStringName(FlatButton));
	curve_reset_handles->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_reset_handles->set_tooltip_text(TTR("Reset All Handles"));
	curve_reset_handles->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_reset_all_points_handles));
	toolbar->add_child(curve_reset_handles);

	separator_clear_points = memnew(VSeparator);
	separator_clear_points->add_theme_style_override("separator", separator_line_style);
	toolbar->add_child(separator_clear_points);

	curve_clear_points = memnew(Button);
	curve_clear_points->set_theme_type_variation(SceneStringName(FlatButton));
	curve_clear_points->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	curve_clear_points->set_tooltip_text(TTR("Clear Points"));
	curve_clear_points->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_confirm_clear_points));
	toolbar->add_child(curve_clear_points);

	separator_options_menu = memnew(VSeparator);
	separator_options_menu->add_theme_style_override("separator", separator_line_style);
	toolbar->add_child(separator_options_menu);

	clear_points_dialog = memnew(ConfirmationDialog);
	clear_points_dialog->set_title(TTR("Please Confirm..."));
	clear_points_dialog->set_text(TTR("Remove all curve points?"));
	clear_points_dialog->connect(SceneStringName(confirmed), callable_mp(this, &Path3DEditorPlugin::_clear_points));
	toolbar->add_child(clear_points_dialog);

	handle_menu = memnew(MenuButton);
	handle_menu->set_flat(false);
	handle_menu->set_theme_type_variation("FlatMenuButton");
	handle_menu->set_text(TTR("Options"));
	toolbar->add_child(handle_menu);

	create_curve_button = memnew(Button);
	create_curve_button->set_text(TTR("Create Curve"));
	create_curve_button->hide();
	topmenu_bar->add_child(create_curve_button);
	create_curve_button->connect(SceneStringName(pressed), callable_mp(this, &Path3DEditorPlugin::_create_curve));

	axis_lock_x->connect(SceneStringName(toggled), callable_mp(this, &Path3DEditorPlugin::_update_axis_lock_icons).unbind(1));
	axis_lock_y->connect(SceneStringName(toggled), callable_mp(this, &Path3DEditorPlugin::_update_axis_lock_icons).unbind(1));
	axis_lock_z->connect(SceneStringName(toggled), callable_mp(this, &Path3DEditorPlugin::_update_axis_lock_icons).unbind(1));

	PopupMenu *menu = handle_menu->get_popup();
	menu->add_check_item(TTR("Mirror Handle Angles"));
	menu->set_item_checked(HANDLE_OPTION_ANGLE, mirror_handle_angle);
	menu->add_check_item(TTR("Mirror Handle Lengths"));
	menu->set_item_checked(HANDLE_OPTION_LENGTH, mirror_handle_length);
	menu->add_check_item(TTR("Snap to Colliders"));
	menu->set_item_checked(HANDLE_OPTION_SNAP_COLLIDER, snap_to_collider);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &Path3DEditorPlugin::_handle_option_pressed));

	curve_edit->set_pressed_no_signal(true);

	topmenu_bar->connect(SceneStringName(theme_changed), callable_mp(this, &Path3DEditorPlugin::_update_theme));
	Node3DEditor::get_singleton()->add_control_to_menu_panel(topmenu_bar);
}

Ref<EditorNode3DGizmo> Path3DGizmoPlugin::create_gizmo(Node3D *p_spatial) {
	Ref<Path3DGizmo> ref;

	Path3D *path = Object::cast_to<Path3D>(p_spatial);
	if (path) {
		ref.instantiate(path);
	}

	return ref;
}

bool Path3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<Path3D>(p_spatial) != nullptr;
}

String Path3DGizmoPlugin::get_gizmo_name() const {
	return "Path3D";
}

void Path3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL(path);

	Ref<Curve3D> curve = path->get_curve();

	Ref<StandardMaterial3D> handle_material = get_material("handles", p_gizmo);
	Ref<StandardMaterial3D> snapped_handle_material = get_material("snapped_handle", p_gizmo);
	Ref<StandardMaterial3D> selected_handle_material = get_material("selected_handle", p_gizmo);
	Ref<StandardMaterial3D> first_pt_handle_material = get_material("first_pt_handle", p_gizmo);
	Ref<StandardMaterial3D> last_pt_handle_material = get_material("last_pt_handle", p_gizmo);
	Ref<StandardMaterial3D> closed_pt_handle_material = get_material("closed_pt_handle", p_gizmo);

	PackedVector3Array first_pt;
	PackedVector3Array last_pt;
	PackedVector3Array mid_pts;
	PackedVector3Array sel_pts;

	if (Path3DEditorPlugin::singleton->curve_edit->is_pressed()) {
		const int pc = curve->get_point_count();
		for (int idx = 0; idx < pc; ++idx) {
			const Vector3 pos = curve->get_point_position(idx);
			if (p_gizmo->is_subgizmo_selected(idx)) {
				sel_pts.append(pos);
			} else if (idx == 0) {
				first_pt.append(pos);
			} else if (idx == pc - 1) {
				last_pt.append(pos);
			} else {
				mid_pts.append(pos);
			}
		}
	}

	if (!last_pt.is_empty()) {
		p_gizmo->add_vertices(last_pt, curve->is_closed() ? handle_material : last_pt_handle_material, Mesh::PRIMITIVE_POINTS);
	}
	if (!first_pt.is_empty()) {
		p_gizmo->add_vertices(first_pt, curve->is_closed() ? closed_pt_handle_material : first_pt_handle_material, Mesh::PRIMITIVE_POINTS);
	}
	if (!mid_pts.is_empty()) {
		p_gizmo->add_vertices(mid_pts, handle_material, Mesh::PRIMITIVE_POINTS);
	}
	if (!sel_pts.is_empty()) {
		p_gizmo->add_vertices(sel_pts, selected_handle_material, Mesh::PRIMITIVE_POINTS);
	}
}

int Path3DGizmoPlugin::subgizmos_intersect_ray(const EditorNode3DGizmo *p_gizmo, Camera3D *p_camera, const Vector2 &p_point) const {
	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL_V(path, -1);
	Ref<Curve3D> curve = path->get_curve();
	ERR_FAIL_COND_V(curve.is_null(), -1);

	if (Path3DEditorPlugin::singleton->curve_edit->is_pressed()) {
		for (int idx = 0; idx < curve->get_point_count(); ++idx) {
			Vector3 pos = path->get_global_transform().xform(curve->get_point_position(idx));
			if (p_camera->unproject_position(pos).distance_to(p_point) < 20) {
				return idx;
			}
		}
	}
	return -1;
}

Vector<int> Path3DGizmoPlugin::subgizmos_intersect_frustum(const EditorNode3DGizmo *p_gizmo, const Camera3D *p_camera, const Vector<Plane> &p_frustum) const {
	Vector<int> contained_points;

	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL_V(path, contained_points);
	Ref<Curve3D> curve = path->get_curve();
	ERR_FAIL_COND_V(curve.is_null(), contained_points);

	if (Path3DEditorPlugin::singleton->curve_edit->is_pressed()) {
		for (int idx = 0; idx < curve->get_point_count(); ++idx) {
			Vector3 pos = path->get_global_transform().xform(curve->get_point_position(idx));
			bool is_contained_in_frustum = true;
			for (int i = 0; i < p_frustum.size(); ++i) {
				if (p_frustum[i].distance_to(pos) > 0) {
					is_contained_in_frustum = false;
					break;
				}
			}

			if (is_contained_in_frustum) {
				contained_points.push_back(idx);
			}
		}
	}

	return contained_points;
}

Transform3D Path3DGizmoPlugin::get_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id) const {
	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL_V(path, Transform3D());
	Ref<Curve3D> curve = path->get_curve();
	ERR_FAIL_COND_V(curve.is_null(), Transform3D());
	ERR_FAIL_INDEX_V(p_id, curve->get_point_count(), Transform3D());

	Basis basis = transformation_locked_basis.has(p_id) ? transformation_locked_basis[p_id] : curve->get_point_baked_posture(p_id, true);
	Vector3 pos = curve->get_point_position(p_id);

	Transform3D t = Transform3D(basis, pos);
	return t;
}

void Path3DGizmoPlugin::set_subgizmo_transform(const EditorNode3DGizmo *p_gizmo, int p_id, Transform3D p_transform) {
	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL(path);
	Ref<Curve3D> curve = path->get_curve();
	ERR_FAIL_COND(curve.is_null());
	ERR_FAIL_INDEX(p_id, curve->get_point_count());

	const Transform3D gt = path->get_global_transform();
	const Transform3D gi = gt.affine_inverse();

	if (!transformation_locked_basis.has(p_id)) {
		transformation_locked_basis[p_id] = Basis(curve->get_point_baked_posture(p_id, true));
	}
	if (!transformation_locked_origins.has(p_id)) {
		// The first set_subgizmo_transform() call happens before the point has moved,
		// so the current curve position is the drag origin.
		transformation_locked_origins[p_id] = gt.xform(curve->get_point_position(p_id));
	}

	Path3DEditorPlugin *editor = Path3DEditorPlugin::singleton;
	if (editor) {
		// Apply the axis locks in global space so the locked axis is the global one,
		// regardless of the Path3D's own rotation. Mirrors set_handle() and
		// forward_3d_gui_input(), which call _apply_axis_locks() on global-space points.
		Vector3 locked_global = gt.xform(p_transform.origin);
		editor->_apply_axis_locks(transformation_locked_origins[p_id], locked_global);
		curve->set_point_position(p_id, gi.xform(locked_global));
		return;
	}

	curve->set_point_position(p_id, p_transform.origin);
}

void Path3DGizmoPlugin::commit_subgizmos(const EditorNode3DGizmo *p_gizmo, const Vector<int> &p_ids, const Vector<Transform3D> &p_restore, bool p_cancel) {
	Path3D *path = Object::cast_to<Path3D>(p_gizmo->get_node_3d());
	ERR_FAIL_NULL(path);
	Ref<Curve3D> curve = path->get_curve();
	ERR_FAIL_COND(curve.is_null());

	transformation_locked_basis.clear();
	transformation_locked_origins.clear();

	if (p_cancel) {
		for (int i = 0; i < p_ids.size(); ++i) {
			curve->set_point_position(p_ids[i], p_restore[i].origin);
		}
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	undo_redo->create_action(TTR("Set Curve Point Position"));

	for (int i = 0; i < p_ids.size(); ++i) {
		const int idx = p_ids[i];
		undo_redo->add_do_method(curve.ptr(), "set_point_position", idx, curve->get_point_position(idx));
		undo_redo->add_undo_method(curve.ptr(), "set_point_position", idx, p_restore[i].origin);
	}
	undo_redo->commit_action();
}

int Path3DGizmoPlugin::get_priority() const {
	return -1;
}

Path3DGizmoPlugin::Path3DGizmoPlugin() {
	Color path_color = SceneTree::get_singleton()->get_debug_paths_color();
	Color path_tilt_color = EDITOR_GET("editors/3d_gizmos/gizmo_colors/path_tilt");

	create_material("path_material", path_color);
	create_material("path_thin_material", Color(0.6, 0.6, 0.6));
	create_material("path_tilt_material", path_tilt_color);
	create_material("path_tilt_muted_material", path_tilt_color * 0.7);
	create_handle_material("handles", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));
	create_handle_material("first_pt_handle", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));
	create_handle_material("last_pt_handle", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));
	create_handle_material("closed_pt_handle", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));
	create_handle_material("sec_handles", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorCurveHandle"), EditorStringName(EditorIcons)));
	create_handle_material("selected_handle", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));
	create_handle_material("snapped_handle", false, EditorNode::get_singleton()->get_editor_theme()->get_icon(SNAME("EditorPathSmoothHandle"), EditorStringName(EditorIcons)));

	// Static handle colors — set once here instead of every redraw().
	get_material("first_pt_handle")->set_albedo(Color(0.2, 1.0, 0.0));
	get_material("last_pt_handle")->set_albedo(Color(1.0, 0.2, 0.0));
	get_material("closed_pt_handle")->set_albedo(Color(1.0, 0.8, 0.0));
	get_material("sec_handles")->set_albedo(Color(1.0, 0.75, 0.0));
	get_material("selected_handle")->set_albedo(Color(0.1, 0.6, 1.0));
	get_material("snapped_handle")->set_albedo(Color(1.0, 0.2, 1.0));
}
