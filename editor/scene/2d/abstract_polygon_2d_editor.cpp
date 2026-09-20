/**************************************************************************/
/*  abstract_polygon_2d_editor.cpp                                        */
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
 * @file abstract_polygon_2d_editor.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "abstract_polygon_2d_editor.h"

#include "core/math/geometry_2d.h"
#include "core/os/keyboard.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/button.h"
#include "scene/gui/dialogs.h"

bool AbstractPolygon2DEditor::Vertex::operator==(const AbstractPolygon2DEditor::Vertex &p_vertex) const {
	return polygon == p_vertex.polygon && vertex == p_vertex.vertex;
}

bool AbstractPolygon2DEditor::Vertex::operator!=(const AbstractPolygon2DEditor::Vertex &p_vertex) const {
	return !(*this == p_vertex);
}

bool AbstractPolygon2DEditor::Vertex::valid() const {
	return vertex >= 0;
}

bool AbstractPolygon2DEditor::_is_empty() const {
	if (!_get_node()) {
		return true;
	}

	const int n = _get_polygon_count();

	for (int i = 0; i < n; i++) {
		Vector<Vector2> vertices = _get_polygon(i);

		if (vertices.size() != 0) {
			return false;
		}
	}

	return true;
}

bool AbstractPolygon2DEditor::_is_line() const {
	return false;
}

bool AbstractPolygon2DEditor::_has_uv() const {
	return false;
}

int AbstractPolygon2DEditor::_get_polygon_count() const {
	return 1;
}

Variant AbstractPolygon2DEditor::_get_polygon(int p_idx) const {
	return _get_node()->get("polygon");
}

void AbstractPolygon2DEditor::_set_polygon(int p_idx, const Variant &p_polygon) const {
	_get_node()->set("polygon", p_polygon);
}

void AbstractPolygon2DEditor::_action_set_polygon(int p_idx, const Variant &p_previous, const Variant &p_polygon) {
	Node2D *node = _get_node();
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(node, "set_polygon", p_polygon);
	undo_redo->add_undo_method(node, "set_polygon", p_previous);
}

Vector2 AbstractPolygon2DEditor::_get_offset(int p_idx) const {
	return Vector2(0, 0);
}

void AbstractPolygon2DEditor::_commit_action() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->add_do_method(canvas_item_editor, "update_viewport");
	undo_redo->add_undo_method(canvas_item_editor, "update_viewport");
	undo_redo->commit_action();
}

void AbstractPolygon2DEditor::_action_add_polygon(const Variant &p_polygon) {
	_action_set_polygon(0, p_polygon);
}

void AbstractPolygon2DEditor::_action_remove_polygon(int p_idx) {
	_action_set_polygon(p_idx, _get_polygon(p_idx), Vector<Vector2>());
}

void AbstractPolygon2DEditor::_action_set_polygon(int p_idx, const Variant &p_polygon) {
	_action_set_polygon(p_idx, _get_polygon(p_idx), p_polygon);
}

bool AbstractPolygon2DEditor::_has_resource() const {
	return true;
}

void AbstractPolygon2DEditor::_create_resource() {
}

Vector2 AbstractPolygon2DEditor::_get_geometric_center() const {
	int n_polygons = _get_polygon_count();

	double cx = 0.0;
	double cy = 0.0;
	int n_subs = 0;
	for (int i = 0; i < n_polygons; i++) {
		const Vector<Vector2> &vertices = _get_polygon(i);
		// Don't try to decompose a polygon if it's not yet a polygon :)
		if (vertices.size() < 3) {
			continue;
		}
		Vector<Vector<Point2>> decomp = ::Geometry2D::decompose_polygon_in_convex(vertices);
		if (decomp.is_empty()) {
			continue;
		}
		for (const Vector<Vector2> &sub : decomp) {
			int sub_n_points = sub.size();
			double sub_area2x = 0.0;
			double sub_cx = 0.0;
			double sub_cy = 0.0;
			for (int n = 0; n < sub_n_points; n++) {
				int next = (n + 1 < sub_n_points) ? n + 1 : 0;
				sub_area2x += (sub[n].x * sub[next].y) - (sub[next].x * sub[n].y);
				sub_cx += (sub[n].x + sub[next].x) * (sub[n].x * sub[next].y - sub[next].x * sub[n].y);
				sub_cy += (sub[n].y + sub[next].y) * (sub[n].x * sub[next].y - sub[next].x * sub[n].y);
			}
			sub_cx /= (sub_area2x * 3);
			sub_cy /= (sub_area2x * 3);

			cx += sub_cx;
			cy += sub_cy;
		}
		n_subs += decomp.size();
	}

	if (n_subs == 0) {
		return Vector2();
	}

	cx /= n_subs;
	cy /= n_subs;

	return Vector2(cx, cy);
}

void AbstractPolygon2DEditor::_menu_option(int p_option) {
	switch (p_option) {
		case MODE_CREATE: {
			mode = MODE_CREATE;
			button_create->set_pressed(true);
			button_edit->set_pressed(false);
			button_delete->set_pressed(false);
		} break;
		case MODE_EDIT: {
			_wip_close();
			mode = MODE_EDIT;
			button_create->set_pressed(false);
			button_edit->set_pressed(true);
			button_delete->set_pressed(false);
		} break;
		case MODE_DELETE: {
			_wip_close();
			mode = MODE_DELETE;
			button_create->set_pressed(false);
			button_edit->set_pressed(false);
			button_delete->set_pressed(true);
		} break;
		case CENTER_POLY: {
			_wip_close();

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Move Origin to Geometric Center"));

			Vector2 center = _get_geometric_center();

			int n_polygons = _get_polygon_count();
			for (int i = 0; i < n_polygons; i++) {
				const Vector<Vector2> &vertices = _get_polygon(i);
				int n_points = vertices.size();

				Vector<Vector2> new_vertices;
				new_vertices.resize(n_points);
				for (int n = 0; n < n_points; n++) {
					new_vertices.write[n] = vertices[n] - center;
				}
				_action_set_polygon(i, vertices, new_vertices);
			}
			Node2D *node = _get_node();
			Vector2 node_pos = node->get_position();
			undo_redo->add_do_method(node, "set_position", node_pos + node->get_transform().basis_xform(center));
			undo_redo->add_undo_method(node, "set_position", node_pos);

			_commit_action();
		} break;
	}
}

void AbstractPolygon2DEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED: {
			button_create->set_button_icon(get_editor_theme_icon(SNAME("CurveCreate")));
			button_edit->set_button_icon(get_editor_theme_icon(SNAME("CurveEdit")));
			button_delete->set_button_icon(get_editor_theme_icon(SNAME("CurveDelete")));
			button_center->set_button_icon(get_editor_theme_icon(SNAME("CurveCenter")));
		} break;

		case NOTIFICATION_READY: {
			disable_polygon_editing(false, String());

			button_edit->set_pressed(true);

			get_tree()->connect("node_removed", callable_mp(this, &AbstractPolygon2DEditor::_node_removed));
			create_resource->connect(SceneStringName(confirmed), callable_mp(this, &AbstractPolygon2DEditor::_create_resource));
		} break;
	}
}

void AbstractPolygon2DEditor::_node_removed(Node *p_node) {
	if (p_node == _get_node()) {
		edit(nullptr);
		hide();

		canvas_item_editor->update_viewport();
	}
}

void AbstractPolygon2DEditor::_wip_changed() {
	if (wip_active && _is_line()) {
		_set_polygon(0, wip);
	}
}

void AbstractPolygon2DEditor::_wip_cancel() {
	wip.clear();
	wip_active = false;

	edited_point = PosVertex();
	hover_point = Vertex();
	selected_point = Vertex();
	center_drag = false;

	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::_wip_close() {
	if (!wip_active) {
		return;
	}

	if (_is_line()) {
		_set_polygon(0, wip);
	} else if (wip.size() >= (_is_line() ? 2 : 3)) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Create Polygon"));
		_action_add_polygon(wip);
		if (_has_uv()) {
			undo_redo->add_do_method(_get_node(), "set_uv", Vector<Vector2>());
			undo_redo->add_undo_method(_get_node(), "set_uv", _get_node()->get("uv"));
		}
		_commit_action();
	} else {
		return;
	}

	mode = MODE_EDIT;
	button_edit->set_pressed(true);
	button_create->set_pressed(false);
	button_delete->set_pressed(false);

	wip.clear();
	wip_active = false;

	edited_point = PosVertex();
	hover_point = Vertex();
	selected_point = Vertex();
	center_drag = false;
}

void AbstractPolygon2DEditor::disable_polygon_editing(bool p_disable, const String &p_reason) {
	_polygon_editing_enabled = !p_disable;

	button_create->set_disabled(p_disable);
	button_edit->set_disabled(p_disable);
	button_delete->set_disabled(p_disable);
	button_center->set_disabled(p_disable);

	if (p_disable) {
		button_create->set_tooltip_text(p_reason);
		button_edit->set_tooltip_text(p_reason);
		button_delete->set_tooltip_text(p_reason);
		button_center->set_tooltip_text(p_reason);
	} else {
		button_create->set_tooltip_text(TTRC("Create points."));
		button_edit->set_tooltip_text(TTRC("Edit points.\nLMB: Move Point\nRMB: Erase Point"));
		button_delete->set_tooltip_text(TTRC("Erase points."));
		button_center->set_tooltip_text(TTRC("Move center of gravity to geometric center."));
	}
}

bool AbstractPolygon2DEditor::_commit_drag() {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	center_drag = false;
	int n_polygons = _get_polygon_count();
	ERR_FAIL_COND_V(pre_center_move_edit.size() != n_polygons, false);
	undo_redo->create_action(TTR("Move Geometric Center"));
	for (int i = 0; i < n_polygons; i++) {
		_action_set_polygon(i, pre_center_move_edit[i], _get_polygon(i));
	}
	pre_center_move_edit.clear();
	_commit_action();
	return true;
}

bool AbstractPolygon2DEditor::forward_gui_input(const Ref<InputEvent> &p_event) {
	if (!_get_node() || !_polygon_editing_enabled) {
		return false;
	}

	if (!_get_node()->is_visible_in_tree()) {
		return false;
	}

	Viewport *vp = _get_node()->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return false;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	Ref<InputEventMouseButton> mb = p_event;

	if (!_has_resource()) {
		if (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
			create_resource->set_text(String("No polygon resource on this node.\nCreate and assign one?"));
			create_resource->popup_centered();
		}
		return (mb.is_valid() && mb->get_button_index() == MouseButton::LEFT);
	}

	CanvasItemEditor::Tool tool = CanvasItemEditor::get_singleton()->get_current_tool();
	if (tool != CanvasItemEditor::TOOL_SELECT) {
		return false;
	}

	if (mb.is_valid()) {
		Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

		Vector2 gpoint = mb->get_position();
		Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint));
		cpoint = _get_node()->get_screen_transform().affine_inverse().xform(cpoint);

		if (mode == MODE_EDIT || (_is_line() && mode == MODE_CREATE)) {
			if (mb->get_button_index() == MouseButton::LEFT) {
				if (mb->is_pressed()) {
					if (mb->is_meta_pressed() || mb->is_alt_pressed()) {
						return false;
					}

					bool ctrl_or_cmd = mb->is_ctrl_pressed();
					bool shift = mb->is_shift_pressed();

					const PosVertex closest = closest_point(gpoint);
					if (closest.valid()) {
						if (ctrl_or_cmd && selected_points.has(closest)) {
							// Ctrl-click on an already-selected point removes it.
							selected_points.erase(closest);
							canvas_item_editor->update_viewport();
							return true;
						}
						if (!selected_points.has(closest)) {
							_select_point(closest, ctrl_or_cmd, false);
						}

						original_mouse_pos = gpoint;
						pre_move_edit = _get_polygon(closest.polygon);
						edited_point = PosVertex(closest, xform.affine_inverse().xform(closest.pos));
						selected_point = closest;
						edge_point = PosVertex();

						multi_move_start_positions.clear();
						if (selected_points.size() > 1) {
							pre_move_edit = _get_polygon(closest.polygon); // already set above; reuse it as the "before" snapshot
							for (const Vertex &v : selected_points) {
								multi_move_start_positions[v] = pre_move_edit[v.vertex];
							}
						}

						canvas_item_editor->update_viewport();
						return true;
					} else if (!ctrl_or_cmd) {
						selected_point = Vertex();

						const PosVertex insert = closest_edge_point(gpoint);
						if (insert.valid()) {
							Vector<Vector2> vertices = _get_polygon(insert.polygon);
							Vertex new_selection;

							if (vertices.size() < (_is_line() ? 2 : 3)) {
								vertices.push_back(cpoint);
								undo_redo->create_action(TTR("Edit Polygon"));
								new_selection = Vertex(insert.polygon, vertices.size() - 1);
							} else {
								edited_point = PosVertex(insert.polygon, insert.vertex + 1, xform.affine_inverse().xform(insert.pos));
								vertices.insert(edited_point.vertex, edited_point.pos);
								pre_move_edit = vertices;
								new_selection = Vertex(edited_point.polygon, edited_point.vertex);
								edge_point = PosVertex();

								undo_redo->create_action(TTR("Insert Point"));
							}

							_action_set_polygon(insert.polygon, vertices);
							_commit_action();

							selected_point = new_selection;
							selected_points.clear();
							selected_points.insert(selected_point);

							return true;
						}

						// Hit nothing at all (no point, no edge): start box-selection.
						box_selecting = true;
						box_select_from = gpoint;
						box_select_to = gpoint;
						box_select_additive = shift;
						if (!box_select_additive) {
							selected_points.clear();
						}
						canvas_item_editor->update_viewport();
						return true;
					}
				} else {
					if (edited_point.valid()) {
						if (original_mouse_pos != gpoint) {
							Vector<Vector2> vertices = _get_polygon(edited_point.polygon);
							ERR_FAIL_INDEX_V(edited_point.vertex, vertices.size(), false);

							Vector2 final_pos = edited_point.pos - _get_offset(edited_point.polygon);

							if (multi_move_start_positions.size() > 1) {
								Vector2 delta = final_pos - multi_move_start_positions[selected_point];
								for (const KeyValue<Vertex, Point2> &kv : multi_move_start_positions) {
									ERR_FAIL_INDEX_V(kv.key.vertex, vertices.size(), false);
									vertices.write[kv.key.vertex] = kv.value + delta;
								}
							} else {
								vertices.write[edited_point.vertex] = final_pos;
							}

							undo_redo->create_action(TTR("Edit Polygon"));
							_action_set_polygon(edited_point.polygon, pre_move_edit, vertices);
							_commit_action();
						}

						edited_point = PosVertex();
						multi_move_start_positions.clear();
						return true;
					}
					if (box_selecting) {
						box_selecting = false;
						_box_select_confirm(box_select_additive);
						canvas_item_editor->update_viewport();
						return true;
					}
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && !edited_point.valid()) {
				const PosVertex closest = closest_point(gpoint);

				if (closest.valid()) {
					remove_point(closest);
					return true;
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && !edited_point.valid()) {
				const PosVertex closest = closest_point(gpoint);

				if (closest.valid()) {
					remove_point(closest);
					return true;
				}
			}
		} else if (mode == MODE_DELETE) {
			if (mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
				const PosVertex closest = closest_point(gpoint);

				if (closest.valid()) {
					remove_point(closest);
					return true;
				}
			}
		}

		if (mode == MODE_CREATE) {
			if (mb->get_button_index() == MouseButton::LEFT && mb->is_pressed()) {
				if (_is_line()) {
					// for lines, we don't have a wip mode, and we can undo each single add point.
					Vector<Vector2> vertices = _get_polygon(0);
					vertices.push_back(cpoint);
					undo_redo->create_action(TTR("Insert Point"));
					_action_set_polygon(0, vertices);
					_commit_action();
					return true;
				} else if (!wip_active) {
					wip.clear();
					wip.push_back(cpoint);
					wip_active = true;
					_wip_changed();
					edited_point = PosVertex(-1, 1, cpoint);
					canvas_item_editor->update_viewport();
					hover_point = Vertex();
					selected_point = Vertex(0);
					edge_point = PosVertex();
					return true;
				} else {
					const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

					if (!_is_line() && wip.size() > 1 && xform.xform(wip[0]).distance_to(xform.xform(cpoint)) < grab_threshold) {
						//wip closed
						_wip_close();

						return true;
					} else {
						//add wip point
						wip.push_back(cpoint);
						_wip_changed();
						edited_point = PosVertex(-1, wip.size(), cpoint);
						selected_point = Vertex(wip.size() - 1);
						canvas_item_editor->update_viewport();
						return true;
					}
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed() && wip_active) {
				_wip_cancel();
			}
		}

		// Center drag.
		if (edit_origin_and_center) {
			real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

			if (mb->get_button_index() == MouseButton::LEFT) {
				if (mb->is_meta_pressed() || mb->is_ctrl_pressed() || mb->is_shift_pressed() || mb->is_alt_pressed()) {
					return false;
				}
				if (mb->is_pressed() && !center_drag) {
					Vector2 center_point = xform.xform(_get_geometric_center());
					if ((gpoint - center_point).length() < grab_threshold) {
						pre_center_move_edit.clear();
						int n_polygons = _get_polygon_count();
						for (int i = 0; i < n_polygons; i++) {
							pre_center_move_edit.push_back(_get_polygon(i));
						}
						center_drag_origin = cpoint;
						center_drag = true;
						return true;
					}
				} else if (center_drag) {
					return _commit_drag();
				}
			} else if (mb->get_button_index() == MouseButton::RIGHT && center_drag) {
				_commit_drag();
			}
		}
	}

	Ref<InputEventMouseMotion> mm = p_event;

	if (mm.is_valid()) {
		Vector2 gpoint = mm->get_position();

		if (box_selecting) {
			box_select_to = gpoint;
			canvas_item_editor->update_viewport();
			return true;
		}

		if (center_drag) {
			Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint));
			cpoint = _get_node()->get_screen_transform().affine_inverse().xform(cpoint);
			Vector2 delta = center_drag_origin - cpoint;

			int n_polygons = _get_polygon_count();
			for (int i = 0; i < n_polygons; i++) {
				const Vector<Vector2> &vertices = _get_polygon(i);
				int n_points = vertices.size();

				Vector<Vector2> new_vertices;
				new_vertices.resize(n_points);
				for (int n = 0; n < n_points; n++) {
					new_vertices.write[n] = vertices[n] - delta;
				}
				_set_polygon(i, new_vertices);
			}
			center_drag_origin = cpoint;
		} else if (edited_point.valid() && (wip_active || mm->get_button_mask().has_flag(MouseButtonMask::LEFT))) {
			Vector2 cpoint = canvas_item_editor->snap_point(canvas_item_editor->get_canvas_transform().affine_inverse().xform(gpoint));
			cpoint = _get_node()->get_screen_transform().affine_inverse().xform(cpoint);

			//Move the point in a single axis. Should only work when editing a polygon and while holding shift.
			if (mode == MODE_EDIT && mm->is_shift_pressed()) {
				Vector2 old_point = pre_move_edit.get(selected_point.vertex);
				if (Math::abs(cpoint.x - old_point.x) > Math::abs(cpoint.y - old_point.y)) {
					cpoint.y = old_point.y;
				} else {
					cpoint.x = old_point.x;
				}
			}

			edited_point = PosVertex(edited_point, cpoint);

			if (!wip_active) {
				if (multi_move_start_positions.size() > 1) {
					Vector2 delta = (cpoint - _get_offset(edited_point.polygon)) - multi_move_start_positions[selected_point];
					Vector<Vector2> vertices = _get_polygon(edited_point.polygon);
					for (const KeyValue<Vertex, Point2> &kv : multi_move_start_positions) {
						ERR_FAIL_INDEX_V(kv.key.vertex, vertices.size(), false);
						vertices.write[kv.key.vertex] = kv.value + delta;
					}
					_set_polygon(edited_point.polygon, vertices);
				} else {
					Vector<Vector2> vertices = _get_polygon(edited_point.polygon);
					ERR_FAIL_INDEX_V(edited_point.vertex, vertices.size(), false);
					vertices.write[edited_point.vertex] = cpoint - _get_offset(edited_point.polygon);
					_set_polygon(edited_point.polygon, vertices);
				}
			}

			canvas_item_editor->update_viewport();
		} else if (mode == MODE_EDIT || (_is_line() && mode == MODE_CREATE)) {
			const PosVertex new_hover_point = closest_point(gpoint);
			if (hover_point != new_hover_point) {
				hover_point = new_hover_point;
				canvas_item_editor->update_viewport();
			}

			bool edge_hover = false;
			if (!hover_point.valid()) {
				const PosVertex on_edge_vertex = closest_edge_point(gpoint);

				if (on_edge_vertex.valid()) {
					hover_point = Vertex();
					edge_point = on_edge_vertex;
					canvas_item_editor->update_viewport();
					edge_hover = true;
				}
			}

			if (!edge_hover && edge_point.valid()) {
				edge_point = PosVertex();
				canvas_item_editor->update_viewport();
			}
		}
	}

	Ref<InputEventKey> k = p_event;

	// Key handling
	if (k.is_valid() && k->is_pressed()) {
		// Point deletion
		if (k->get_keycode() == Key::KEY_DELETE || k->get_keycode() == Key::BACKSPACE) {
			// Delete multiple points if multiple are selected
			if (selected_points.size() > 1) {
				_delete_selection();
				return true;
			}
			if (wip_active && selected_point.polygon == -1) {
				if (wip.size() > selected_point.vertex) {
					wip.remove_at(selected_point.vertex);
					_wip_changed();
					selected_point = wip.size() - 1;
					canvas_item_editor->update_viewport();
					return true;
				}
			} else {
				const Vertex active_point = get_active_point();

				if (active_point.valid()) {
					remove_point(active_point);
					return true;
				}
			}
		} else if (wip_active && k->get_keycode() == Key::ENTER) {
			_wip_close();
		} else if (wip_active && k->get_keycode() == Key::ESCAPE) {
			_wip_cancel();
		}
	}

	return false;
}

void AbstractPolygon2DEditor::forward_canvas_draw_over_viewport(Control *p_overlay) {
	if (!_get_node()) {
		return;
	}

	if (!_get_node()->is_visible_in_tree()) {
		return;
	}

	Viewport *vp = _get_node()->get_viewport();
	if (vp && !vp->is_visible_subviewport()) {
		return;
	}

	Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();
	// All polygon points are sharp, so use the sharp handle icon
	const Ref<Texture2D> handle = get_editor_theme_icon(SNAME("EditorPathSharpHandle"));
	const Ref<Texture2D> nhandle = get_editor_theme_icon(SNAME("EditorPathNullHandle"));

	Ref<Font> font = get_theme_font(SNAME("bold"), EditorStringName(EditorFonts));
	int font_size = 1.3 * get_theme_font_size(SNAME("bold_size"), EditorStringName(EditorFonts));
	const float outline_size = 4 * EDSCALE;
	Color font_color = get_theme_color(SceneStringName(font_color), EditorStringName(Editor));
	Color outline_color = font_color.inverted();

	const Vertex active_point = get_active_point();
	const int n_polygons = _get_polygon_count();
	const bool is_closed = !_is_line();

	if (edit_origin_and_center) {
		const Vector2 &center = _get_geometric_center();
		if (!center.is_zero_approx()) {
			const Vector2 point = xform.xform(center);
			p_overlay->draw_texture(nhandle, point - nhandle->get_size() * 0.5, Color(1, 1, 0.4));
			Size2 lbl_size = font->get_string_size("c", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
			p_overlay->draw_string_outline(font, point - lbl_size * 0.5, "c", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, outline_size, outline_color);
			p_overlay->draw_string(font, point - lbl_size * 0.5, "c", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
		}
		{
			const Vector2 point = xform.xform(Vector2());
			p_overlay->draw_texture(nhandle, point - nhandle->get_size() * 0.5, center.is_equal_approx(Vector2()) ? Color(1, 1, 0.4) : Color(1, 0.4, 1));
			Size2 lbl_size = font->get_string_size("o", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
			p_overlay->draw_string_outline(font, point - lbl_size * 0.5, "o", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, outline_size, outline_color);
			p_overlay->draw_string(font, point - lbl_size * 0.5, "o", HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
		}
	}

	for (int j = -1; j < n_polygons; j++) {
		if (wip_active && wip_destructive && j != -1) {
			continue;
		}

		Vector<Vector2> points;
		Vector2 offset;

		if (wip_active && j == edited_point.polygon) {
			points = Variant(wip);
			offset = Vector2(0, 0);
		} else {
			if (j == -1) {
				continue;
			}
			points = _get_polygon(j);
			offset = _get_offset(j);
		}

		if (!wip_active && j == edited_point.polygon && EDITOR_GET("editors/polygon_editor/show_previous_outline")) {
			const Color col = Color(0.5, 0.5, 0.5); /// @todo FIXME polygon->get_outline_color();
			const int n = pre_move_edit.size();
			for (int i = 0; i < n - (is_closed ? 0 : 1); i++) {
				Vector2 p, p2;
				p = pre_move_edit[i] + offset;
				p2 = pre_move_edit[(i + 1) % n] + offset;

				Vector2 point = xform.xform(p);
				Vector2 next_point = xform.xform(p2);

				p_overlay->draw_line(point, next_point, col, Math::round(2 * EDSCALE));
			}
		}

		const int n_points = points.size();
		const Color col = Color(1, 0.3, 0.1, 0.8);

		for (int i = 0; i < n_points; i++) {
			const Vertex vertex(j, i);

			const Vector2 p = (vertex == edited_point) ? edited_point.pos : (points[i] + offset);
			const Vector2 point = xform.xform(p);

			if (is_closed || i < n_points - 1) {
				Vector2 p2;
				if (j == edited_point.polygon &&
						((wip_active && i == n_points - 1) || (((i + 1) % n_points) == edited_point.vertex))) {
					p2 = edited_point.pos;
				} else {
					p2 = points[(i + 1) % n_points] + offset;
				}

				const Vector2 next_point = xform.xform(p2);
				p_overlay->draw_line(point, next_point, col, Math::round(2 * EDSCALE));
			}
		}

		for (int i = 0; i < n_points; i++) {
			const Vertex vertex(j, i);

			const Vector2 p = (vertex == edited_point) ? edited_point.pos : (points[i] + offset);
			const Vector2 point = xform.xform(p);

			const Color overlay_modulate = (vertex == active_point || selected_points.has(vertex)) ? Color(0.4, 1, 1) : Color(1, 1, 1);
			p_overlay->draw_texture(handle, point - handle->get_size() * 0.5, overlay_modulate);

			if (vertex == hover_point) {
				String num = String::num_int64(vertex.vertex);
				Size2 num_size = font->get_string_size(num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size);
				p_overlay->draw_string_outline(font, point - num_size * 0.5, num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, outline_size, outline_color);
				p_overlay->draw_string(font, point - num_size * 0.5, num, HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, font_color);
			}
		}
	}

	if (edge_point.valid()) {
		Ref<Texture2D> add_handle = get_editor_theme_icon(SNAME("EditorHandleAdd"));
		p_overlay->draw_texture(add_handle, edge_point.pos - add_handle->get_size() * 0.5);
	}

	if (box_selecting) {
		Point2 bsfrom = box_select_from;
		Point2 bsto = box_select_to;
		if (bsfrom.x > bsto.x) {
			SWAP(bsfrom.x, bsto.x);
		}
		if (bsfrom.y > bsto.y) {
			SWAP(bsfrom.y, bsto.y);
		}
		p_overlay->draw_rect(Rect2(bsfrom, bsto - bsfrom), get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor)));
		p_overlay->draw_rect(Rect2(bsfrom, bsto - bsfrom), get_theme_color(SNAME("box_selection_stroke_color"), EditorStringName(Editor)), false, Math::round(EDSCALE));
	}
}

void AbstractPolygon2DEditor::set_edit_origin_and_center(bool p_enabled) {
	edit_origin_and_center = p_enabled;
	if (button_center) {
		button_center->set_visible(edit_origin_and_center);
	}
}

void AbstractPolygon2DEditor::edit(Node *p_polygon) {
	if (!canvas_item_editor) {
		canvas_item_editor = CanvasItemEditor::get_singleton();
	}

	_clear_selection();

	if (p_polygon) {
		_set_node(p_polygon);

		// Enable the pencil tool if the polygon is empty.
		if (_is_empty()) {
			_menu_option(MODE_CREATE);
		} else {
			_menu_option(MODE_EDIT);
		}

		wip.clear();
		wip_active = false;
		edited_point = PosVertex();
		hover_point = Vertex();
		selected_point = Vertex();
		center_drag = false;
	} else {
		_set_node(nullptr);
	}

	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::remove_point(const Vertex &p_vertex) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	Vector<Vector2> vertices = _get_polygon(p_vertex.polygon);

	bool whole_polygon_removed = false;
	if (vertices.size() > (_is_line() ? 2 : 3)) {
		vertices.remove_at(p_vertex.vertex);

		undo_redo->create_action(TTR("Edit Polygon (Remove Point)"));
		_action_set_polygon(p_vertex.polygon, vertices);
		_commit_action();
	} else {
		whole_polygon_removed = true;
		undo_redo->create_action(TTR("Remove Polygon And Point"));
		_action_remove_polygon(p_vertex.polygon);
		_commit_action();
	}

	if (_is_empty()) {
		_menu_option(MODE_CREATE);
	}

	hover_point = Vertex();
	if (selected_point == p_vertex) {
		selected_point = Vertex();
	}

	// Re-index (or drop) multi-selected vertices affected by this removal,
	// since remove_at() shifts every subsequent vertex's index down by one.
	HashSet<Vertex, VertexHasher> updated_selection;
	for (const Vertex &v : selected_points) {
		if (v.polygon != p_vertex.polygon) {
			updated_selection.insert(v);
		} else if (whole_polygon_removed || v == p_vertex) {
			continue; // Drop: it no longer exists.
		} else if (v.vertex > p_vertex.vertex) {
			updated_selection.insert(Vertex(v.polygon, v.vertex - 1));
		} else {
			updated_selection.insert(v);
		}
	}
	selected_points = updated_selection;
}

AbstractPolygon2DEditor::Vertex AbstractPolygon2DEditor::get_active_point() const {
	return hover_point.valid() ? hover_point : selected_point;
}

AbstractPolygon2DEditor::PosVertex AbstractPolygon2DEditor::closest_point(const Vector2 &p_pos) const {
	const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");

	const int n_polygons = _get_polygon_count();
	const Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

	PosVertex closest;
	real_t closest_dist = 1e10;

	for (int j = 0; j < n_polygons; j++) {
		Vector<Vector2> points = _get_polygon(j);
		const Vector2 offset = _get_offset(j);
		const int n_points = points.size();

		for (int i = 0; i < n_points; i++) {
			Vector2 cp = xform.xform(points[i] + offset);

			real_t d = cp.distance_to(p_pos);
			if (d < closest_dist && d < grab_threshold) {
				closest_dist = d;
				closest = PosVertex(j, i, cp);
			}
		}
	}

	return closest;
}

AbstractPolygon2DEditor::PosVertex AbstractPolygon2DEditor::closest_edge_point(const Vector2 &p_pos) const {
	const real_t grab_threshold = EDITOR_GET("editors/polygon_editor/point_grab_radius");
	const real_t eps = grab_threshold * 2;
	const real_t eps2 = eps * eps;

	const int n_polygons = _get_polygon_count();
	const Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

	PosVertex closest;
	real_t closest_dist = 1e10;

	for (int j = 0; j < n_polygons; j++) {
		Vector<Vector2> points = _get_polygon(j);
		const Vector2 offset = _get_offset(j);
		const int n_points = points.size();
		const int n_segments = n_points - (_is_line() ? 1 : 0);

		for (int i = 0; i < n_segments; i++) {
			const Vector2 segment_a = xform.xform(points[i] + offset);
			const Vector2 segment_b = xform.xform(points[(i + 1) % n_points] + offset);

			Vector2 cp = Geometry2D::get_closest_point_to_segment(p_pos, segment_a, segment_b);

			if (cp.distance_squared_to(segment_a) < eps2 || cp.distance_squared_to(segment_b) < eps2) {
				continue; //not valid to reuse point
			}

			real_t d = cp.distance_to(p_pos);
			if (d < closest_dist && d < grab_threshold) {
				closest_dist = d;
				closest = PosVertex(j, i, cp);
			}
		}
	}

	return closest;
}

void AbstractPolygon2DEditor::_select_point(const Vertex &p_vertex, bool p_add_to_selection, bool p_toggle) {
	if (!p_add_to_selection) {
		selected_points.clear();
	}
	if (p_toggle && selected_points.has(p_vertex)) {
		selected_points.erase(p_vertex);
	} else {
		selected_points.insert(p_vertex);
	}
	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::_box_select_confirm(bool p_additive) {
	Transform2D xform = canvas_item_editor->get_canvas_transform() * _get_node()->get_screen_transform();

	Point2 bsfrom = box_select_from;
	Point2 bsto = box_select_to;
	if (bsfrom.x > bsto.x) {
		SWAP(bsfrom.x, bsto.x);
	}
	if (bsfrom.y > bsto.y) {
		SWAP(bsfrom.y, bsto.y);
	}
	Rect2 rect(bsfrom, bsto - bsfrom);

	if (!p_additive) {
		selected_points.clear();
	}

	int n_polygons = _get_polygon_count();
	for (int j = 0; j < n_polygons; j++) {
		Vector<Vector2> points = _get_polygon(j);
		Vector2 offset = _get_offset(j);
		for (int i = 0; i < points.size(); i++) {
			if (rect.has_point(xform.xform(points[i] + offset))) {
				selected_points.insert(Vertex(j, i));
			}
		}
	}
	canvas_item_editor->update_viewport();
}

void AbstractPolygon2DEditor::_clear_selection() {
	selected_points.clear();
	multi_move_start_positions.clear();
	box_selecting = false;
}

void AbstractPolygon2DEditor::_clear_point_selection() {
	selected_points.clear();
}

void AbstractPolygon2DEditor::_delete_selection() {
	if (selected_points.is_empty()) {
		return;
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();

	int polygon_idx = selected_points.begin()->polygon;

	Vector<int> vertex_indices;
	for (const Vertex &v : selected_points) {
		vertex_indices.push_back(v.vertex);
	}
	vertex_indices.sort();

	Vector<Vector2> before = _get_polygon(polygon_idx);
	Vector<Vector2> after = before;

	for (int i = vertex_indices.size() - 1; i >= 0; i--) {
		after.remove_at(vertex_indices[i]);
	}

	undo_redo->create_action(TTR("Delete Points from Polygon"));

	if (after.size() < (_is_line() ? 2 : 3)) {
		_action_remove_polygon(polygon_idx);
	} else {
		_action_set_polygon(polygon_idx, before, after);
	}

	undo_redo->add_do_method(this, "_clear_point_selection");
	undo_redo->add_undo_method(this, "_clear_point_selection");

	_commit_action();

	hover_point = Vertex();
	selected_point = Vertex();

	if (_is_empty()) {
		_menu_option(MODE_CREATE);
	}
}

AbstractPolygon2DEditor::AbstractPolygon2DEditor(bool p_wip_destructive) {
	edited_point = PosVertex();
	center_drag = false;
	wip_destructive = p_wip_destructive;

	hover_point = Vertex();
	selected_point = Vertex();
	edge_point = PosVertex();

	button_create = memnew(Button);
	button_create->set_theme_type_variation(SceneStringName(FlatButton));
	button_create->set_accessibility_name(TTRC("Create Polygon Points"));
	add_child(button_create);
	button_create->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_CREATE));
	button_create->set_toggle_mode(true);

	button_edit = memnew(Button);
	button_edit->set_theme_type_variation(SceneStringName(FlatButton));
	button_edit->set_accessibility_name(TTRC("Edit Polygon Points"));
	add_child(button_edit);
	button_edit->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_EDIT));
	button_edit->set_toggle_mode(true);

	button_delete = memnew(Button);
	button_delete->set_theme_type_variation(SceneStringName(FlatButton));
	button_delete->set_accessibility_name(TTRC("Delete Polygon Points"));
	add_child(button_delete);
	button_delete->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(MODE_DELETE));
	button_delete->set_toggle_mode(true);

	button_center = memnew(Button);
	button_center->set_theme_type_variation(SceneStringName(FlatButton));
	add_child(button_center);
	button_center->connect(SceneStringName(pressed), callable_mp(this, &AbstractPolygon2DEditor::_menu_option).bind(CENTER_POLY));
	button_center->set_visible(edit_origin_and_center);

	create_resource = memnew(ConfirmationDialog);
	add_child(create_resource);
	create_resource->set_ok_button_text(TTR("Create"));
}

void AbstractPolygon2DEditorPlugin::edit(Object *p_object) {
	Node *polygon_node = Object::cast_to<Node>(p_object);
	polygon_editor->edit(polygon_node);
	make_visible(polygon_node != nullptr);
}

bool AbstractPolygon2DEditorPlugin::handles(Object *p_object) const {
	return p_object->is_class(klass);
}

void AbstractPolygon2DEditorPlugin::make_visible(bool p_visible) {
	if (p_visible) {
		polygon_editor->show();
	} else {
		polygon_editor->hide();
		polygon_editor->edit(nullptr);
	}
}

AbstractPolygon2DEditorPlugin::AbstractPolygon2DEditorPlugin(AbstractPolygon2DEditor *p_polygon_editor, const String &p_class) :
		polygon_editor(p_polygon_editor),
		klass(p_class) {
	CanvasItemEditor::get_singleton()->add_control_to_menu_panel(polygon_editor);
	polygon_editor->hide();
}

void AbstractPolygon2DEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_clear_point_selection"), &AbstractPolygon2DEditor::_clear_point_selection);
}
