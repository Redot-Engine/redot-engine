/**************************************************************************/
/*  animation_bezier_editor.cpp                                           */
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

#include "animation_bezier_editor.h"

#include "editor/animation/animation_player_editor_plugin.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/gui/editor_spin_slider.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/option_button.h"
#include "scene/gui/view_panner.h"
#include "scene/resources/text_line.h"

#include <climits>

float AnimationBezierTrackEdit::_bezier_h_to_pixel(float p_h) {
	float h = p_h;
	h = (h - timeline_v_scroll) / timeline_v_zoom;
	h = (get_size().height / 2.0) - h;
	return h;
}

void AnimationBezierTrackEdit::_draw_track(int p_track, const Color &p_color) {
	float scale = timeline->get_zoom_scale();

	int limit = timeline->get_name_limit();
	int right_limit = get_size().width;

	// Selection may have altered the order of keys.
	RBMap<real_t, int> key_order;

	for (int i = 0; i < animation->track_get_key_count(p_track); i++) {
		real_t ofs = animation->track_get_key_time(p_track, i);
		if (selection.has(IntPair(p_track, i))) {
			if (moving_selection) {
				ofs += moving_selection_offset.x;
			} else if (scaling_selection) {
				ofs += -scaling_selection_offset.x + (ofs - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);
			}
		}

		key_order[ofs] = i;
	}

	for (RBMap<real_t, int>::Element *E = key_order.front(); E; E = E->next()) {
		int i = E->get();

		if (!E->next()) {
			break;
		}

		int i_n = E->next()->get();

		float offset = animation->track_get_key_time(p_track, i);
		float height = animation->bezier_track_get_key_value(p_track, i);
		Vector2 out_handle = animation->bezier_track_get_key_out_handle(p_track, i);
		if (p_track == moving_handle_track && (moving_handle == -1 || moving_handle == 1) && moving_handle_key == i) {
			out_handle = moving_handle_right;
		}

		if (selection.has(IntPair(p_track, i))) {
			if (moving_selection) {
				offset += moving_selection_offset.x;
				height += moving_selection_offset.y;
			} else if (scaling_selection) {
				offset += -scaling_selection_offset.x + (offset - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);
				height += -scaling_selection_offset.y + (height - scaling_selection_pivot.y) * (scaling_selection_scale.y - 1);
			}
		}

		float offset_n = animation->track_get_key_time(p_track, i_n);
		float height_n = animation->bezier_track_get_key_value(p_track, i_n);
		Vector2 in_handle = animation->bezier_track_get_key_in_handle(p_track, i_n);
		if (p_track == moving_handle_track && (moving_handle == -1 || moving_handle == 1) && moving_handle_key == i_n) {
			in_handle = moving_handle_left;
		}

		if (selection.has(IntPair(p_track, i_n))) {
			if (moving_selection) {
				offset_n += moving_selection_offset.x;
				height_n += moving_selection_offset.y;
			} else if (scaling_selection) {
				offset_n += -scaling_selection_offset.x + (offset_n - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);
				height_n += -scaling_selection_offset.y + (height_n - scaling_selection_pivot.y) * (scaling_selection_scale.y - 1);
			}
		}

		if (moving_inserted_key && moving_selection_from_track == p_track) {
			if (moving_selection_from_key == i) {
				Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(p_track, i);
				if (handle_mode != Animation::HANDLE_MODE_FREE) {
					float offset_p = offset;
					float height_p = height;
					if (E->prev()) {
						int i_p = E->prev()->get();
						offset_p = animation->track_get_key_time(p_track, i_p);
						height_p = animation->bezier_track_get_key_value(p_track, i_p);
					}

					animation->bezier_track_calculate_handles(offset, offset_p, height_p, offset_n, height_n, handle_mode, Animation::HANDLE_SET_MODE_AUTO, nullptr, &out_handle);
				}
			} else if (moving_selection_from_key == i_n) {
				Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(p_track, i_n);
				if (handle_mode != Animation::HANDLE_MODE_FREE) {
					float offset_nn = offset_n;
					float height_nn = height_n;
					if (E->next()->next()) {
						int i_nn = E->next()->next()->get();
						offset_nn = animation->track_get_key_time(p_track, i_nn);
						height_nn = animation->bezier_track_get_key_value(p_track, i_nn);
					}

					animation->bezier_track_calculate_handles(offset_n, offset, height, offset_nn, height_nn, handle_mode, Animation::HANDLE_SET_MODE_AUTO, &in_handle, nullptr);
				}
			}
		}

		out_handle += Vector2(offset, height);
		in_handle += Vector2(offset_n, height_n);

		Vector2 start(offset, height);
		Vector2 end(offset_n, height_n);

		int from_x = (offset - timeline->get_value()) * scale + limit;
		int point_start = from_x;
		int to_x = (offset_n - timeline->get_value()) * scale + limit;
		int point_end = to_x;

		if (from_x > right_limit) { // Not visible.
			continue;
		}

		if (to_x < limit) { // Not visible.
			continue;
		}

		from_x = MAX(from_x, limit);
		to_x = MIN(to_x, right_limit);

		Vector<Vector2> lines;

		Vector2 prev_pos;

		for (int j = from_x; j <= to_x; j++) {
			float t = (j - limit) / scale + timeline->get_value();

			float h;

			if (j == point_end) {
				h = end.y; // Make sure it always connects.
			} else if (j == point_start) {
				h = start.y; // Make sure it always connects.
			} else { // Custom interpolation, used because it needs to show paths affected by moving the selection or handles.
				int iterations = 10;
				float low = 0;
				float high = 1;

				// Narrow high and low as much as possible.
				for (int k = 0; k < iterations; k++) {
					float middle = (low + high) / 2.0;

					Vector2 interp = start.bezier_interpolate(out_handle, in_handle, end, middle);

					if (interp.x < t) {
						low = middle;
					} else {
						high = middle;
					}
				}

				// Interpolate the result.
				Vector2 low_pos = start.bezier_interpolate(out_handle, in_handle, end, low);
				Vector2 high_pos = start.bezier_interpolate(out_handle, in_handle, end, high);

				float c = (t - low_pos.x) / (high_pos.x - low_pos.x);

				h = low_pos.lerp(high_pos, c).y;
			}

			h = _bezier_h_to_pixel(h);

			Vector2 pos(j, h);

			if (j > from_x) {
				lines.push_back(prev_pos);
				lines.push_back(pos);
			}
			prev_pos = pos;
		}

		if (lines.size() >= 2) {
			draw_multiline(lines, p_color, Math::round(EDSCALE), true);
		}
	}
}

void AnimationBezierTrackEdit::_draw_line_clipped(const Vector2 &p_from, const Vector2 &p_to, const Color &p_color, int p_clip_left, int p_clip_right) {
	Vector2 from = p_from;
	Vector2 to = p_to;

	if (from.x == to.x && from.y == to.y) {
		return;
	}
	if (to.x < from.x) {
		SWAP(to, from);
	}

	if (to.x < p_clip_left) {
		return;
	}

	if (from.x > p_clip_right) {
		return;
	}

	if (to.x > p_clip_right) {
		float c = (p_clip_right - from.x) / (to.x - from.x);
		to = from.lerp(to, c);
	}

	if (from.x < p_clip_left) {
		float c = (p_clip_left - from.x) / (to.x - from.x);
		from = from.lerp(to, c);
	}

	draw_line(from, to, p_color, Math::round(EDSCALE), true);
}

void AnimationBezierTrackEdit::_notification(int p_what) {
	switch (p_what) {
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("editors/panning")) {
				panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/animation_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
				panner->setup_warped_panning(get_viewport(), EDITOR_GET("editors/panning/warped_mouse_panning"));
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			panner->setup((ViewPanner::ControlScheme)EDITOR_GET("editors/panning/animation_editors_panning_scheme").operator int(), ED_GET_SHORTCUT("canvas_item_editor/pan_view"), bool(EDITOR_GET("editors/panning/simple_panning")));
			panner->setup_warped_panning(get_viewport(), EDITOR_GET("editors/panning/warped_mouse_panning"));
		} break;
		case NOTIFICATION_THEME_CHANGED: {
			bezier_icon = get_editor_theme_icon(SNAME("KeyBezierPoint"));
			bezier_handle_icon = get_editor_theme_icon(SNAME("KeyBezierHandle"));
			selected_icon = get_editor_theme_icon(SNAME("KeyBezierSelected"));
		} break;

		case NOTIFICATION_ACCESSIBILITY_UPDATE: {
			RID ae = get_accessibility_element();
			ERR_FAIL_COND(ae.is_null());

			//TODO
			DisplayServer::get_singleton()->accessibility_update_set_role(ae, DisplayServer::AccessibilityRole::ROLE_STATIC_TEXT);
			DisplayServer::get_singleton()->accessibility_update_set_value(ae, TTR(vformat("The %s is not accessible at this time.", "Animation bezier track editor")));
		} break;

		case NOTIFICATION_DRAW: {
			if (animation.is_null()) {
				return;
			}

			int limit = timeline->get_name_limit();

			const Ref<Font> font = get_theme_font(SceneStringName(font), SNAME("Label"));
			const int font_size = get_theme_font_size(SceneStringName(font_size), SNAME("Label"));
			const Color color = get_theme_color(SceneStringName(font_color), SNAME("Label"));

			const Color h_line_color = get_theme_color(SNAME("h_line_color"), SNAME("AnimationBezierTrackEdit"));
			const Color v_line_color = get_theme_color(SNAME("v_line_color"), SNAME("AnimationBezierTrackEdit"));
			const Color focus_color = get_theme_color(SNAME("focus_color"), SNAME("AnimationBezierTrackEdit"));
			const Color track_focus_color = get_theme_color(SNAME("track_focus_color"), SNAME("AnimationBezierTrackEdit"));

			const int h_separation = get_theme_constant(SNAME("h_separation"), SNAME("AnimationBezierTrackEdit"));
			const int v_separation = get_theme_constant(SNAME("h_separation"), SNAME("AnimationBezierTrackEdit"));

			if (has_focus()) {
				draw_rect(Rect2(Point2(), get_size()), focus_color, false, Math::round(EDSCALE));
			}

			draw_line(Point2(limit, 0), Point2(limit, get_size().height), v_line_color, Math::round(EDSCALE));

			int right_limit = get_size().width;

			track_v_scroll_max = v_separation;

			int vofs = v_separation + track_v_scroll;
			int margin = 0;

			RBMap<int, Color> subtrack_colors;
			Color selected_track_color;
			subtracks.clear();
			subtrack_icons.clear();

			RBMap<String, Vector<int>> track_indices;
			int track_count = animation->get_track_count();
			for (int i = 0; i < track_count; ++i) {
				if (!_is_track_displayed(i)) {
					continue;
				}

				String base_path = String(animation->track_get_path(i));
				int end = base_path.find_char(':');
				if (end != -1) {
					base_path = base_path.substr(0, end + 1);
				}
				Vector<int> indices = track_indices.has(base_path) ? track_indices[base_path] : Vector<int>();
				indices.push_back(i);
				track_indices[base_path] = indices;
			}

			for (const KeyValue<String, Vector<int>> &E : track_indices) {
				String base_path = E.key;

				Vector<int> tracks = E.value;

				// Names and icon.
				{
					NodePath path = animation->track_get_path(tracks[0]);

					Node *node = nullptr;

					if (root && root->has_node(path)) {
						node = root->get_node(path);
					}

					String text;

					if (node) {
						int ofs = 0;

						Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(node, "Node");

						text = node->get_name();
						ofs += h_separation;

						TextLine text_buf = TextLine(text, font, font_size);
						text_buf.set_width(limit - ofs - icon->get_width() - h_separation);

						int h = MAX(text_buf.get_size().y, icon->get_height());

						draw_texture(icon, Point2(ofs, vofs + int(h - icon->get_height()) / 2.0));
						ofs += icon->get_width() + h_separation;

						margin = icon->get_width();

						Vector2 string_pos = Point2(ofs, vofs);
						string_pos = string_pos.floor();
						text_buf.draw(get_canvas_item(), string_pos, color);

						vofs += h + v_separation;
						track_v_scroll_max += h + v_separation;
					}
				}

				const Color dc = get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

				Ref<Texture2D> remove = get_editor_theme_icon(SNAME("Remove"));
				float remove_hpos = limit - h_separation - remove->get_width();

				Ref<Texture2D> lock = get_editor_theme_icon(SNAME("Lock"));
				Ref<Texture2D> unlock = get_editor_theme_icon(SNAME("Unlock"));
				float lock_hpos = remove_hpos - h_separation - lock->get_width();

				Ref<Texture2D> visibility_visible = get_editor_theme_icon(SNAME("GuiVisibilityVisible"));
				Ref<Texture2D> visibility_hidden = get_editor_theme_icon(SNAME("GuiVisibilityHidden"));
				float visibility_hpos = lock_hpos - h_separation - visibility_visible->get_width();

				Ref<Texture2D> solo = get_editor_theme_icon(SNAME("AudioBusSolo"));
				float solo_hpos = visibility_hpos - h_separation - solo->get_width();

				float buttons_width = remove->get_width() + lock->get_width() + visibility_visible->get_width() + solo->get_width() + h_separation * 3;

				for (int i = 0; i < tracks.size(); ++i) {
					// Related track titles.

					int current_track = tracks[i];

					String path = String(animation->track_get_path(current_track));
					path = path.replace_first(base_path, "");

					Color cc = color;
					TextLine text_buf = TextLine(path, font, font_size);
					text_buf.set_width(limit - margin - buttons_width - h_separation * 2);

					Rect2 rect = Rect2(margin, vofs, solo_hpos - h_separation - solo->get_width(), text_buf.get_size().y + v_separation);

					cc.a *= 0.7;
					float h;
					if (path.ends_with(":x")) {
						h = 0;
					} else if (path.ends_with(":y")) {
						h = 0.33f;
					} else if (path.ends_with(":z")) {
						h = 0.66f;
					} else {
						uint32_t hash = path.hash();
						hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
						hash = ((hash >> 16) ^ hash) * 0x45d9f3b;
						hash = (hash >> 16) ^ hash;
						h = (hash % 65535) / 65536.0;
					}

					if (current_track != selected_track) {
						Color track_color;
						if (locked_tracks.has(current_track)) {
							track_color.set_hsv(h, 0, 0.4);
						} else {
							track_color.set_hsv(h, 0.2, 0.8);
						}
						track_color.a = 0.5;
						draw_rect(Rect2(0, vofs, margin - h_separation, text_buf.get_size().y * 0.8), track_color);
						subtrack_colors[current_track] = track_color;

						subtracks[current_track] = rect;
					} else {
						draw_rect(rect, track_focus_color);
						if (locked_tracks.has(selected_track)) {
							selected_track_color.set_hsv(h, 0.0, 0.4);
						} else {
							selected_track_color.set_hsv(h, 0.8, 0.8);
						}
					}

					Vector2 string_pos = Point2(margin + h_separation, vofs);
					text_buf.draw(get_canvas_item(), string_pos, cc);

					float icon_start_height = vofs + rect.size.y / 2.0;
					Rect2 remove_rect = Rect2(remove_hpos, icon_start_height - remove->get_height() / 2.0, remove->get_width(), remove->get_height());
					if (read_only) {
						draw_texture(remove, remove_rect.position, dc);
					} else {
						draw_texture(remove, remove_rect.position);
					}

					Rect2 lock_rect = Rect2(lock_hpos, icon_start_height - lock->get_height() / 2.0, lock->get_width(), lock->get_height());
					if (locked_tracks.has(current_track)) {
						draw_texture(lock, lock_rect.position);
					} else {
						draw_texture(unlock, lock_rect.position);
					}

					Rect2 visible_rect = Rect2(visibility_hpos, icon_start_height - visibility_visible->get_height() / 2.0, visibility_visible->get_width(), visibility_visible->get_height());
					if (hidden_tracks.has(current_track)) {
						draw_texture(visibility_hidden, visible_rect.position);
					} else {
						draw_texture(visibility_visible, visible_rect.position);
					}

					Rect2 solo_rect = Rect2(solo_hpos, icon_start_height - solo->get_height() / 2.0, solo->get_width(), solo->get_height());
					draw_texture(solo, solo_rect.position);

					RBMap<int, Rect2> track_icons;
					track_icons[REMOVE_ICON] = remove_rect;
					track_icons[LOCK_ICON] = lock_rect;
					track_icons[VISIBILITY_ICON] = visible_rect;
					track_icons[SOLO_ICON] = solo_rect;

					subtrack_icons[current_track] = track_icons;

					vofs += text_buf.get_size().y + v_separation;
					track_v_scroll_max += text_buf.get_size().y + v_separation;
				}
			}

			const Color accent = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));

			// Guides.
			{
				float min_left_scale = font->get_height(font_size) + v_separation;

				float scale = (min_left_scale * 2) * timeline_v_zoom;
				float step = Math::pow(10.0, Math::round(Math::log(scale / 5.0) / Math::log(10.0))) * 5.0;
				scale = Math::snapped(scale, step);

				while (scale / timeline_v_zoom < min_left_scale * 2) {
					scale += step;
				}

				bool first = true;
				int prev_iv = 0;
				for (int i = font->get_height(font_size); i < get_size().height; i++) {
					float ofs = get_size().height / 2.0 - i;
					ofs *= timeline_v_zoom;
					ofs += timeline_v_scroll;

					int iv = int(ofs / scale);
					if (ofs < 0) {
						iv -= 1;
					}
					if (!first && iv != prev_iv) {
						Color lc = h_line_color;
						lc.a *= 0.5;
						draw_line(Point2(limit, i), Point2(right_limit, i), lc, Math::round(EDSCALE));
						Color c = color;
						c.a *= 0.5;
						draw_string(font, Point2(limit + 8, i - 2), TS->format_number(rtos(Math::snapped((iv + 1) * scale, step))), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, c);
					}

					first = false;
					prev_iv = iv;
				}
			}

			// Draw other curves.
			{
				float scale = timeline->get_zoom_scale();
				Ref<Texture2D> point = get_editor_theme_icon(SNAME("KeyValue"));
				for (const KeyValue<int, Color> &E : subtrack_colors) {
					if (hidden_tracks.has(E.key)) {
						continue;
					}
					_draw_track(E.key, E.value);

					for (int i = 0; i < animation->track_get_key_count(E.key); i++) {
						float offset = animation->track_get_key_time(E.key, i);
						float value = animation->bezier_track_get_key_value(E.key, i);

						Vector2 pos((offset - timeline->get_value()) * scale + limit, _bezier_h_to_pixel(value));

						if (pos.x >= limit && pos.x <= right_limit) {
							draw_texture(point, pos - point->get_size() / 2.0, E.value);
						}
					}
				}

				if (track_count > 0 && !hidden_tracks.has(selected_track)) {
					// Draw edited curve.
					_draw_track(selected_track, selected_track_color);
				}
			}

			const bool draw_selection_handles = selection.size() > 1;
			LocalVector<Point2> selected_pos;

			// Draw editor handles.
			{
				edit_points.clear();
				float scale = timeline->get_zoom_scale();

				for (int i = 0; i < track_count; ++i) {
					bool draw_track = _is_track_curves_displayed(i) && !locked_tracks.has(i);
					if (!draw_selection_handles && !draw_track) {
						continue;
					}

					int key_count = animation->track_get_key_count(i);
					for (int j = 0; j < key_count; ++j) {
						float offset = animation->track_get_key_time(i, j);
						float value = animation->bezier_track_get_key_value(i, j);
						bool is_selected = selection.has(IntPair(i, j));

						if (is_selected) {
							if (moving_selection) {
								offset += moving_selection_offset.x;
								value += moving_selection_offset.y;
							} else if (scaling_selection) {
								offset += -scaling_selection_offset.x + (offset - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);
								value += -scaling_selection_offset.y + (value - scaling_selection_pivot.y) * (scaling_selection_scale.y - 1);
							}
						}

						Vector2 pos((offset - timeline->get_value()) * scale + limit, _bezier_h_to_pixel(value));

						if (draw_selection_handles && is_selected) {
							selected_pos.push_back(pos);
						}

						if (!draw_track) {
							continue;
						}

						Vector2 in_vec = animation->bezier_track_get_key_in_handle(i, j);
						Vector2 out_vec = animation->bezier_track_get_key_out_handle(i, j);

						if ((moving_handle == 1 || moving_handle == -1) && moving_handle_track == i && moving_handle_key == j) {
							in_vec = moving_handle_left;
						}

						if ((moving_handle == 1 || moving_handle == -1) && moving_handle_track == i && moving_handle_key == j) {
							out_vec = moving_handle_right;
						}

						if (moving_inserted_key && moving_selection_from_key == j) {
							Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(i, j);
							if (handle_mode != Animation::HANDLE_MODE_FREE) {
								int key_prev = 0;
								int key_next = moving_selection_from_key;
								for (int k = 0; k < key_count; k++) {
									if (k == moving_selection_from_key) {
										continue;
									}

									if (animation->track_get_key_time(i, k) < offset) {
										key_prev = k;
									} else {
										key_next = k;
										break;
									}
								}

								float prev_time = offset;
								float prev_value = value;
								if (key_prev != moving_selection_from_key) {
									prev_time = animation->track_get_key_time(i, key_prev);
									prev_value = animation->bezier_track_get_key_value(i, key_prev);
								}

								float next_time = offset;
								float next_value = value;
								if (key_next != moving_selection_from_key) {
									next_time = animation->track_get_key_time(i, key_next);
									next_value = animation->bezier_track_get_key_value(i, key_next);
								}

								animation->bezier_track_calculate_handles(offset, prev_time, prev_value, next_time, next_value, handle_mode, Animation::HANDLE_SET_MODE_AUTO, &in_vec, &out_vec);
							}
						}

						Vector2 pos_in(((offset + in_vec.x) - timeline->get_value()) * scale + limit, _bezier_h_to_pixel(value + in_vec.y));
						Vector2 pos_out(((offset + out_vec.x) - timeline->get_value()) * scale + limit, _bezier_h_to_pixel(value + out_vec.y));

						if (i == selected_track || is_selected) {
							_draw_line_clipped(pos, pos_in, accent, limit, right_limit);
							_draw_line_clipped(pos, pos_out, accent, limit, right_limit);
						}

						EditPoint ep;
						ep.track = i;
						ep.key = j;
						if (pos.x >= limit && pos.x <= right_limit) {
							ep.point_rect.position = (pos - bezier_icon->get_size() / 2.0).floor();
							ep.point_rect.size = bezier_icon->get_size();
							if (is_selected) {
								draw_texture(selected_icon, ep.point_rect.position);
								draw_string(font, ep.point_rect.position + Vector2(8, -font->get_height(font_size) - 8), TTR("Time:") + " " + TS->format_number(rtos(Math::snapped(offset, 0.0001))), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, accent);
								draw_string(font, ep.point_rect.position + Vector2(8, -8), TTR("Value:") + " " + TS->format_number(rtos(Math::snapped(value, 0.001))), HORIZONTAL_ALIGNMENT_LEFT, -1, font_size, accent);
							} else {
								Color track_color = Color(1, 1, 1, 1);
								if (i != selected_track) {
									track_color = subtrack_colors[i];
								}
								draw_texture(bezier_icon, ep.point_rect.position, track_color);
							}
							ep.point_rect = ep.point_rect.grow(ep.point_rect.size.width * 0.5);
						}
						ep.point_rect = ep.point_rect.grow(ep.point_rect.size.width * 0.5);

						if (i == selected_track || is_selected) {
							if (animation->bezier_track_get_key_handle_mode(i, j) != Animation::HANDLE_MODE_LINEAR) {
								if (pos_in.x >= limit && pos_in.x <= right_limit) {
									ep.in_rect.position = (pos_in - bezier_handle_icon->get_size() / 2.0).floor();
									ep.in_rect.size = bezier_handle_icon->get_size();
									draw_texture(bezier_handle_icon, ep.in_rect.position);
									ep.in_rect = ep.in_rect.grow(ep.in_rect.size.width * 0.5);
								}
								if (pos_out.x >= limit && pos_out.x <= right_limit) {
									ep.out_rect.position = (pos_out - bezier_handle_icon->get_size() / 2.0).floor();
									ep.out_rect.size = bezier_handle_icon->get_size();
									draw_texture(bezier_handle_icon, ep.out_rect.position);
									ep.out_rect = ep.out_rect.grow(ep.out_rect.size.width * 0.5);
								}
							}
						}
						if (!locked_tracks.has(i)) {
							edit_points.push_back(ep);
						}
					}
				}

				for (int i = 0; i < edit_points.size(); ++i) {
					if (edit_points[i].track == selected_track) {
						EditPoint ep = edit_points[i];
						edit_points.remove_at(i);
						edit_points.insert(0, ep);
					}
				}
			}

			selection_rect = Rect2();
			selection_handles_rect = Rect2();
			// Draw scale handles.
			if (draw_selection_handles) {
				selection_rect.position = selected_pos[0];
				selected_pos.remove_at(0);
				for (const Point2 &pos : selected_pos) {
					selection_rect = selection_rect.expand(pos);
				}

				const int outer_ofs = Math::round(12 * EDSCALE);
				const int inner_ofs = Math::round(outer_ofs / 2.0);

				// Draw horizontal handles.
				if (selection_rect.size.height > CMP_EPSILON) {
					_draw_line_clipped(selection_rect.position - Vector2(inner_ofs, inner_ofs), selection_rect.position + Vector2(selection_rect.size.width + inner_ofs, -inner_ofs), accent, limit, right_limit);
					_draw_line_clipped(selection_rect.position + Vector2(-inner_ofs, selection_rect.size.height + inner_ofs), selection_rect.position + selection_rect.size + Vector2(inner_ofs, inner_ofs), accent, limit, right_limit);
				}
				// Draw vertical handles.
				if (selection_rect.size.width > CMP_EPSILON) {
					_draw_line_clipped(selection_rect.position - Vector2(inner_ofs, inner_ofs), selection_rect.position + Vector2(-inner_ofs, selection_rect.size.height + inner_ofs), accent, limit, right_limit);
					_draw_line_clipped(selection_rect.position + Vector2(selection_rect.size.width + inner_ofs, -inner_ofs), selection_rect.position + selection_rect.size + Vector2(inner_ofs, inner_ofs), accent, limit, right_limit);
				}

				selection_handles_rect.position = selection_rect.position - Vector2(outer_ofs, outer_ofs);
				selection_handles_rect.size = selection_rect.size + Vector2(outer_ofs, outer_ofs) * 2;
			}

			if (box_selecting) {
				Vector2 bs_from = box_selection_from;
				Vector2 bs_to = box_selection_to;
				if (bs_from.x > bs_to.x) {
					SWAP(bs_from.x, bs_to.x);
				}
				if (bs_from.y > bs_to.y) {
					SWAP(bs_from.y, bs_to.y);
				}
				draw_rect(
						Rect2(bs_from, bs_to - bs_from),
						get_theme_color(SNAME("box_selection_fill_color"), EditorStringName(Editor)));
				draw_rect(
						Rect2(bs_from, bs_to - bs_from),
						get_theme_color(SNAME("box_selection_stroke_color"), EditorStringName(Editor)),
						false,
						Math::round(EDSCALE));
			}
		} break;
	}
}

// Check if a track is displayed in the bezier editor (track type = bezier and track not filtered).
bool AnimationBezierTrackEdit::_is_track_displayed(int p_track_index) {
	if (animation->track_get_type(p_track_index) != Animation::TrackType::TYPE_BEZIER) {
		return false;
	}

	if (is_filtered) {
		String path = String(animation->track_get_path(p_track_index));
		if (root && root->has_node(path)) {
			Node *node = root->get_node(path);
			if (!node) {
				return false; // No node, no filter.
			}
			if (!EditorNode::get_singleton()->get_editor_selection()->is_selected(node)) {
				return false; // Skip track due to not selected.
			}
		}
	}

	return true;
}

// Check if the curves for a track are displayed in the editor (not hidden). Includes the check on the track visibility.
bool AnimationBezierTrackEdit::_is_track_curves_displayed(int p_track_index) {
	// Is the track is visible in the editor?
	if (!_is_track_displayed(p_track_index)) {
		return false;
	}

	// And curves visible?
	if (hidden_tracks.has(p_track_index)) {
		return false;
	}

	return true;
}

Ref<Animation> AnimationBezierTrackEdit::get_animation() const {
	return animation;
}

void AnimationBezierTrackEdit::set_animation_and_track(const Ref<Animation> &p_animation, int p_track, bool p_read_only) {
	animation = p_animation;
	read_only = p_read_only;
	selected_track = p_track;
	queue_redraw();
}

Size2 AnimationBezierTrackEdit::get_minimum_size() const {
	return Vector2(1, 1);
}

Control::CursorShape AnimationBezierTrackEdit::get_cursor_shape(const Point2 &p_pos) const {
	// Box selecting or moving a handle
	if (box_selecting || Math::abs(moving_handle) == 1) {
		return get_default_cursor_shape();
	}
	// Hovering a handle
	if (!read_only) {
		for (const EditPoint &edit_point : edit_points) {
			if (edit_point.in_rect.has_point(p_pos) || edit_point.out_rect.has_point(p_pos)) {
				return get_default_cursor_shape();
			}
		}
	}
	// Currently box scaling
	if (scaling_selection) {
		if (scaling_selection_handles == Vector2i(1, 1) || scaling_selection_handles == Vector2i(-1, -1)) {
			return CURSOR_FDIAGSIZE;
		} else if (scaling_selection_handles == Vector2i(1, -1) || scaling_selection_handles == Vector2i(-1, 1)) {
			return CURSOR_BDIAGSIZE;
		} else if (abs(scaling_selection_handles.x) == 1) {
			return CURSOR_HSIZE;
		} else if (abs(scaling_selection_handles.y) == 1) {
			return CURSOR_VSIZE;
		}
	}
	// Hovering the scaling box
	const Vector2i rel_pos = p_pos - selection_rect.position;
	if (selection_handles_rect.has_point(p_pos)) {
		if ((rel_pos.x < 0 && rel_pos.y < 0) || (rel_pos.x > selection_rect.size.width && rel_pos.y > selection_rect.size.height)) {
			return CURSOR_FDIAGSIZE;
		} else if ((rel_pos.x < 0 && rel_pos.y > selection_rect.size.height) || (rel_pos.x > selection_rect.size.width && rel_pos.y < 0)) {
			return CURSOR_BDIAGSIZE;
		} else if (rel_pos.x < 0 || rel_pos.x > selection_rect.size.width) {
			return CURSOR_HSIZE;
		} else if (rel_pos.y < 0 || rel_pos.y > selection_rect.size.height) {
			return CURSOR_VSIZE;
		}
		return CURSOR_MOVE;
	}
	return get_default_cursor_shape();
}

void AnimationBezierTrackEdit::set_timeline(AnimationTimelineEdit *p_timeline) {
	timeline = p_timeline;
	timeline->connect("zoom_changed", callable_mp(this, &AnimationBezierTrackEdit::_zoom_changed));
	timeline->connect("name_limit_changed", callable_mp(this, &AnimationBezierTrackEdit::_zoom_changed));
}

void AnimationBezierTrackEdit::set_editor(AnimationTrackEditor *p_editor) {
	editor = p_editor;
	connect("clear_selection", callable_mp(editor, &AnimationTrackEditor::_clear_selection).bind(false));
	connect("select_key", callable_mp(editor, &AnimationTrackEditor::_key_selected), CONNECT_DEFERRED);
	connect("deselect_key", callable_mp(editor, &AnimationTrackEditor::_key_deselected), CONNECT_DEFERRED);
}

void AnimationBezierTrackEdit::_play_position_draw() {
	if (animation.is_null() || play_position_pos < 0) {
		return;
	}

	float scale = timeline->get_zoom_scale();
	int h = get_size().height;

	int limit = timeline->get_name_limit();

	int px = (-timeline->get_value() + play_position_pos) * scale + limit;

	if (px >= limit && px < (get_size().width)) {
		const Color color = get_theme_color(SNAME("accent_color"), EditorStringName(Editor));
		play_position->draw_line(Point2(px, 0), Point2(px, h), color, Math::round(2 * EDSCALE));
	}
}

void AnimationBezierTrackEdit::set_play_position(real_t p_pos) {
	play_position_pos = p_pos;
	play_position->queue_redraw();
}

void AnimationBezierTrackEdit::update_play_position() {
	play_position->queue_redraw();
}

void AnimationBezierTrackEdit::set_root(Node *p_root) {
	root = p_root;
}

void AnimationBezierTrackEdit::set_filtered(bool p_filtered) {
	is_filtered = p_filtered;
	if (animation.is_null()) {
		return;
	}
	String base_path = String(animation->track_get_path(selected_track));
	if (is_filtered) {
		if (root && root->has_node(base_path)) {
			Node *node = root->get_node(base_path);
			if (!node || !EditorNode::get_singleton()->get_editor_selection()->is_selected(node)) {
				for (int i = 0; i < animation->get_track_count(); ++i) {
					if (animation->track_get_type(i) != Animation::TrackType::TYPE_BEZIER) {
						continue;
					}

					base_path = String(animation->track_get_path(i));
					if (root && root->has_node(base_path)) {
						node = root->get_node(base_path);
						if (!node) {
							continue; // No node, no filter.
						}
						if (!EditorNode::get_singleton()->get_editor_selection()->is_selected(node)) {
							continue; // Skip track due to not selected.
						}

						set_animation_and_track(animation, i, read_only);
						break;
					}
				}
			}
		}
	}
	queue_redraw();
}

void AnimationBezierTrackEdit::auto_fit_vertically() {
	int track_count = animation->get_track_count();
	real_t minimum_value = Math::INF;
	real_t maximum_value = -Math::INF;

	int nb_track_visible = 0;
	for (int i = 0; i < track_count; ++i) {
		if (!_is_track_curves_displayed(i) || locked_tracks.has(i)) {
			continue;
		}

		int key_count = animation->track_get_key_count(i);

		for (int j = 0; j < key_count; ++j) {
			real_t value = animation->bezier_track_get_key_value(i, j);

			minimum_value = MIN(value, minimum_value);
			maximum_value = MAX(value, maximum_value);

			// We also want to includes the handles...
			Vector2 in_vec = animation->bezier_track_get_key_in_handle(i, j);
			Vector2 out_vec = animation->bezier_track_get_key_out_handle(i, j);

			minimum_value = MIN(value + in_vec.y, minimum_value);
			maximum_value = MAX(value + in_vec.y, maximum_value);
			minimum_value = MIN(value + out_vec.y, minimum_value);
			maximum_value = MAX(value + out_vec.y, maximum_value);
		}

		nb_track_visible++;
	}

	if (nb_track_visible == 0) {
		// No visible track... we will not adjust the vertical zoom
		return;
	}

	if (Math::is_finite(minimum_value) && Math::is_finite(maximum_value)) {
		_zoom_vertically(minimum_value, maximum_value);
		queue_redraw();
	}
}

void AnimationBezierTrackEdit::_zoom_vertically(real_t p_minimum_value, real_t p_maximum_value) {
	real_t target_height = p_maximum_value - p_minimum_value;
	if (target_height <= CMP_EPSILON) {
		timeline_v_scroll = p_maximum_value;
		return;
	}

	timeline_v_scroll = (p_maximum_value + p_minimum_value) / 2.0;
	timeline_v_zoom = target_height / ((get_size().height - timeline->get_size().height) * 0.9);
}

void AnimationBezierTrackEdit::_zoom_changed() {
	queue_redraw();
	play_position->queue_redraw();
}

void AnimationBezierTrackEdit::_update_locked_tracks_after(int p_track) {
	if (locked_tracks.has(p_track)) {
		locked_tracks.erase(p_track);
	}

	Vector<int> updated_locked_tracks;
	for (const int &E : locked_tracks) {
		updated_locked_tracks.push_back(E);
	}
	locked_tracks.clear();
	for (int i = 0; i < updated_locked_tracks.size(); ++i) {
		if (updated_locked_tracks[i] > p_track) {
			locked_tracks.insert(updated_locked_tracks[i] - 1);
		} else {
			locked_tracks.insert(updated_locked_tracks[i]);
		}
	}
}

void AnimationBezierTrackEdit::_update_hidden_tracks_after(int p_track) {
	if (hidden_tracks.has(p_track)) {
		hidden_tracks.erase(p_track);
	}

	Vector<int> updated_hidden_tracks;
	for (const int &E : hidden_tracks) {
		updated_hidden_tracks.push_back(E);
	}
	hidden_tracks.clear();
	for (int i = 0; i < updated_hidden_tracks.size(); ++i) {
		if (updated_hidden_tracks[i] > p_track) {
			hidden_tracks.insert(updated_hidden_tracks[i] - 1);
		} else {
			hidden_tracks.insert(updated_hidden_tracks[i]);
		}
	}
}

String AnimationBezierTrackEdit::get_tooltip(const Point2 &p_pos) const {
	return Control::get_tooltip(p_pos);
}

void AnimationBezierTrackEdit::_clear_selection() {
	selection.clear();
	emit_signal(SNAME("clear_selection"));
	queue_redraw();
}

void AnimationBezierTrackEdit::_change_selected_keys_handle_mode(Animation::HandleMode p_mode, bool p_auto) {
	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Update Selected Key Handles"), UndoRedo::MERGE_DISABLE, animation.ptr());
	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		const IntPair track_key_pair = E->get();
		undo_redo->add_undo_method(editor, "_bezier_track_set_key_handle_mode", animation.ptr(), track_key_pair.first, track_key_pair.second, animation->bezier_track_get_key_handle_mode(track_key_pair.first, track_key_pair.second), Animation::HANDLE_SET_MODE_NONE);
		undo_redo->add_undo_method(animation.ptr(), "bezier_track_set_key_in_handle", track_key_pair.first, track_key_pair.second, animation->bezier_track_get_key_in_handle(track_key_pair.first, track_key_pair.second));
		undo_redo->add_undo_method(animation.ptr(), "bezier_track_set_key_out_handle", track_key_pair.first, track_key_pair.second, animation->bezier_track_get_key_out_handle(track_key_pair.first, track_key_pair.second));
		undo_redo->add_do_method(editor, "_bezier_track_set_key_handle_mode", animation.ptr(), track_key_pair.first, track_key_pair.second, p_mode, p_auto ? Animation::HANDLE_SET_MODE_AUTO : Animation::HANDLE_SET_MODE_RESET);
	}
	AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
	if (ape) {
		undo_redo->add_do_method(ape, "_animation_update_key_frame");
		undo_redo->add_undo_method(ape, "_animation_update_key_frame");
	}
	undo_redo->commit_action();
}

void AnimationBezierTrackEdit::_clear_selection_for_anim(const Ref<Animation> &p_anim) {
	if (!(animation == p_anim) || !is_visible()) {
		return;
	}
	_clear_selection();
}

void AnimationBezierTrackEdit::_select_at_anim(const Ref<Animation> &p_anim, int p_track, real_t p_pos, bool p_single) {
	if (!(animation == p_anim) || !is_visible()) {
		return;
	}

	int idx = animation->track_find_key(p_track, p_pos, Animation::FIND_MODE_APPROX);
	ERR_FAIL_COND(idx < 0);

	selection.insert(IntPair(p_track, idx));
	emit_signal(SNAME("select_key"), idx, p_single, p_track);
	queue_redraw();
}

void AnimationBezierTrackEdit::gui_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	if (panner->gui_input(p_event)) {
		accept_event();
		return;
	}

	if (p_event->is_pressed()) {
		if (ED_IS_SHORTCUT("animation_editor/duplicate_selected_keys", p_event)) {
			if (!read_only) {
				duplicate_selected_keys(-1.0, false);
			}
			accept_event();
		}
		if (ED_IS_SHORTCUT("animation_editor/cut_selected_keys", p_event)) {
			if (!read_only) {
				copy_selected_keys(true);
			}
			accept_event();
		}
		if (ED_IS_SHORTCUT("animation_editor/copy_selected_keys", p_event)) {
			if (!read_only) {
				copy_selected_keys(false);
			}
			accept_event();
		}
		if (ED_IS_SHORTCUT("animation_editor/paste_keys", p_event)) {
			if (!read_only) {
				paste_keys(-1.0, false);
			}
			accept_event();
		}
		if (ED_IS_SHORTCUT("animation_editor/delete_selection", p_event)) {
			if (!read_only) {
				delete_selection();
			}
			accept_event();
		}
	}

	Ref<InputEventKey> key_press = p_event;

	if (key_press.is_valid() && key_press->is_pressed()) {
		if (ED_IS_SHORTCUT("animation_bezier_editor/focus", p_event)) {
			SelectionSet focused_keys;
			if (selection.is_empty()) {
				for (int i = 0; i < edit_points.size(); ++i) {
					IntPair key_pair = IntPair(edit_points[i].track, edit_points[i].key);
					focused_keys.insert(key_pair);
				}
			} else {
				for (const IntPair &E : selection) {
					focused_keys.insert(E);
					if (E.second > 0) {
						IntPair previous_key = IntPair(E.first, E.second - 1);
						focused_keys.insert(previous_key);
					}
					if (E.second < animation->track_get_key_count(E.first) - 1) {
						IntPair next_key = IntPair(E.first, E.second + 1);
						focused_keys.insert(next_key);
					}
				}
			}
			if (focused_keys.is_empty()) {
				accept_event();
				return;
			}

			real_t minimum_time = Math::INF;
			real_t maximum_time = -Math::INF;
			real_t minimum_value = Math::INF;
			real_t maximum_value = -Math::INF;

			for (const IntPair &E : focused_keys) {
				IntPair key_pair = E;

				real_t time = animation->track_get_key_time(key_pair.first, key_pair.second);
				real_t value = animation->bezier_track_get_key_value(key_pair.first, key_pair.second);

				minimum_time = MIN(time, minimum_time);
				maximum_time = MAX(time, maximum_time);
				minimum_value = MIN(value, minimum_value);
				maximum_value = MAX(value, maximum_value);
			}

			float width = get_size().width - timeline->get_name_limit() - timeline->get_buttons_width();
			float padding = width * 0.1;
			float desired_scale = (width - padding / 2.0) / (maximum_time - minimum_time);
			minimum_time = MAX(0, minimum_time - (padding / 2.0) / desired_scale);

			float zv = Math::pow(100 / desired_scale, 0.125f);
			if (zv < 1) {
				zv = Math::pow(desired_scale / 100, 0.125f) - 1;
				zv = 1 - zv;
			}
			float zoom_value = timeline->get_zoom()->get_max() - zv;

			if (Math::is_finite(minimum_time) && Math::is_finite(maximum_time) && maximum_time - minimum_time > CMP_EPSILON) {
				timeline->get_zoom()->set_value(zoom_value);
				callable_mp((Range *)timeline, &Range::set_value).call_deferred(minimum_time);
			}

			if (Math::is_finite(minimum_value) && Math::is_finite(maximum_value)) {
				_zoom_vertically(minimum_value, maximum_value);
			}

			queue_redraw();
			accept_event();
			return;
		} else if (ED_IS_SHORTCUT("animation_bezier_editor/select_all_keys", p_event)) {
			for (int i = 0; i < edit_points.size(); ++i) {
				_select_at_anim(animation, edit_points[i].track, animation->track_get_key_time(edit_points[i].track, edit_points[i].key), i == 0);
			}

			queue_redraw();
			accept_event();
			return;
		} else if (ED_IS_SHORTCUT("animation_bezier_editor/deselect_all_keys", p_event)) {
			selection.clear();
			emit_signal(SNAME("clear_selection"));

			queue_redraw();
			accept_event();
			return;
		}
	}

	Ref<InputEventMouseButton> mb = p_event;
	int limit = timeline->get_name_limit();
	if (mb.is_valid() && mb->get_button_index() == MouseButton::RIGHT && mb->is_pressed()) {
		menu_insert_key = mb->get_position();
		if (menu_insert_key.x >= limit && menu_insert_key.x <= get_size().width) {
			if (!read_only) {
				Vector2 popup_pos = get_screen_position() + mb->get_position();

				bool selected = _try_select_at_ui_pos(mb->get_position(), mb->is_shift_pressed(), false);

				menu->clear();
				menu->add_icon_item(bezier_icon, TTR("Insert Key Here"), MENU_KEY_INSERT);
				if (selected || selection.size()) {
					menu->add_separator();
					menu->add_icon_item(get_editor_theme_icon(SNAME("Duplicate")), TTR("Duplicate Selected Key(s)"), MENU_KEY_DUPLICATE);
					menu->add_icon_item(get_editor_theme_icon(SNAME("ActionCut")), TTR("Cut Selected Key(s)"), MENU_KEY_CUT);
					menu->add_icon_item(get_editor_theme_icon(SNAME("ActionCopy")), TTR("Copy Selected Key(s)"), MENU_KEY_COPY);
				}

				if (editor->is_key_clipboard_active()) {
					menu->add_icon_item(get_editor_theme_icon(SNAME("ActionPaste")), TTR("Paste Key(s)"), MENU_KEY_PASTE);
				}

				if (selected || selection.size()) {
					menu->add_separator();
					menu->add_icon_item(get_editor_theme_icon(SNAME("Remove")), TTR("Delete Selected Key(s)"), MENU_KEY_DELETE);
					menu->add_separator();
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesFree")), TTR("Make Handles Free"), MENU_KEY_SET_HANDLE_FREE);
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesLinear")), TTR("Make Handles Linear"), MENU_KEY_SET_HANDLE_LINEAR);
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesBalanced")), TTR("Make Handles Balanced"), MENU_KEY_SET_HANDLE_BALANCED);
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesMirror")), TTR("Make Handles Mirrored"), MENU_KEY_SET_HANDLE_MIRRORED);
					menu->add_separator();
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesBalanced")), TTR("Make Handles Balanced (Auto Tangent)"), MENU_KEY_SET_HANDLE_AUTO_BALANCED);
					menu->add_icon_item(get_editor_theme_icon(SNAME("BezierHandlesMirror")), TTR("Make Handles Mirrored (Auto Tangent)"), MENU_KEY_SET_HANDLE_AUTO_MIRRORED);
				}

				if (menu->get_item_count()) {
					menu->reset_size();
					menu->set_position(popup_pos);
					menu->popup();
				}
			}
		}
	}

	if (mb.is_valid() && mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		Point2 pos = mb->get_position();
		bool no_mod_key_pressed = !mb->is_alt_pressed() && !mb->is_shift_pressed() && !mb->is_command_or_control_pressed();
		if (mb->is_double_click() && !moving_selection && no_mod_key_pressed) {
			int x = pos.x - timeline->get_name_limit();
			float ofs = x / timeline->get_zoom_scale() + timeline->get_value();
			emit_signal(SNAME("timeline_changed"), ofs, false);
		}
		for (const KeyValue<int, Rect2> &E : subtracks) {
			if (E.value.has_point(mb->get_position())) {
				if (!locked_tracks.has(E.key) && !hidden_tracks.has(E.key)) {
					set_animation_and_track(animation, E.key, read_only);
					_clear_selection();
				}
				return;
			}
		}

		for (const KeyValue<int, RBMap<int, Rect2>> &E : subtrack_icons) {
			int track = E.key;
			RBMap<int, Rect2> track_icons = E.value;
			for (const KeyValue<int, Rect2> &I : track_icons) {
				if (I.value.has_point(mb->get_position())) {
					if (I.key == REMOVE_ICON) {
						if (!read_only) {
							EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
							undo_redo->create_action("Remove Bezier Track", UndoRedo::MERGE_DISABLE, animation.ptr());

							undo_redo->add_do_method(this, "_update_locked_tracks_after", track);
							undo_redo->add_do_method(this, "_update_hidden_tracks_after", track);

							undo_redo->add_do_method(animation.ptr(), "remove_track", track);

							undo_redo->add_undo_method(animation.ptr(), "add_track", Animation::TrackType::TYPE_BEZIER, track);
							undo_redo->add_undo_method(animation.ptr(), "track_set_path", track, animation->track_get_path(track));

							for (int i = 0; i < animation->track_get_key_count(track); ++i) {
								undo_redo->add_undo_method(
										this,
										"_bezier_track_insert_key_at_anim",
										animation,
										track,
										animation->track_get_key_time(track, i),
										animation->bezier_track_get_key_value(track, i),
										animation->bezier_track_get_key_in_handle(track, i),
										animation->bezier_track_get_key_out_handle(track, i),
										animation->bezier_track_get_key_handle_mode(track, i));
							}

							undo_redo->commit_action();

							selected_track = CLAMP(selected_track, 0, animation->get_track_count() - 1);
						}
						return;
					} else if (I.key == LOCK_ICON) {
						if (locked_tracks.has(track)) {
							locked_tracks.erase(track);
						} else {
							locked_tracks.insert(track);
							if (selected_track == track) {
								for (int i = 0; i < animation->get_track_count(); ++i) {
									if (!locked_tracks.has(i) && animation->track_get_type(i) == Animation::TrackType::TYPE_BEZIER) {
										set_animation_and_track(animation, i, read_only);
										break;
									}
								}
							}
						}
						queue_redraw();
						return;
					} else if (I.key == VISIBILITY_ICON) {
						if (hidden_tracks.has(track)) {
							hidden_tracks.erase(track);
						} else {
							hidden_tracks.insert(track);
							if (selected_track == track) {
								for (int i = 0; i < animation->get_track_count(); ++i) {
									if (!hidden_tracks.has(i) && animation->track_get_type(i) == Animation::TrackType::TYPE_BEZIER) {
										set_animation_and_track(animation, i, read_only);
										break;
									}
								}
							}
						}

						Vector<int> visible_tracks;
						for (int i = 0; i < animation->get_track_count(); ++i) {
							if (!hidden_tracks.has(i) && animation->track_get_type(i) == Animation::TrackType::TYPE_BEZIER) {
								visible_tracks.push_back(i);
							}
						}

						if (visible_tracks.size() == 1) {
							solo_track = visible_tracks[0];
						} else {
							solo_track = -1;
						}

						queue_redraw();
						return;
					} else if (I.key == SOLO_ICON) {
						if (solo_track == track) {
							solo_track = -1;

							hidden_tracks.clear();
						} else {
							if (hidden_tracks.has(track)) {
								hidden_tracks.erase(track);
							}
							for (int i = 0; i < animation->get_track_count(); ++i) {
								if (animation->track_get_type(i) == Animation::TrackType::TYPE_BEZIER) {
									if (i != track && !hidden_tracks.has(i)) {
										hidden_tracks.insert(i);
									}
								}
							}

							set_animation_and_track(animation, track, read_only);
							solo_track = track;
						}
						queue_redraw();
						return;
					}
					return;
				}
			}
		}

		// Check this first, to allow manipulating key handles while ignoring keyframes before scaling/moving.
		bool inside_selection_handles_rect = !read_only && selection_handles_rect.has_point(mb->get_position());

		// First, check keyframe.
		// Command/Control makes it ignore the keyframe, so control point editors can be force-edited.
		if (!inside_selection_handles_rect && !mb->is_command_or_control_pressed()) {
			if (_try_select_at_ui_pos(mb->get_position(), mb->is_shift_pressed(), true)) {
				return;
			}
		}
		// Second, check key handles.
		for (int i = 0; i < edit_points.size(); i++) {
			if (!read_only) {
				if (edit_points[i].in_rect.has_point(mb->get_position())) {
					moving_handle = -1;
					moving_handle_key = edit_points[i].key;
					moving_handle_track = edit_points[i].track;
					moving_handle_left = animation->bezier_track_get_key_in_handle(edit_points[i].track, edit_points[i].key);
					moving_handle_right = animation->bezier_track_get_key_out_handle(edit_points[i].track, edit_points[i].key);
					queue_redraw();
					return;
				}

				if (edit_points[i].out_rect.has_point(mb->get_position())) {
					moving_handle = 1;
					moving_handle_key = edit_points[i].key;
					moving_handle_track = edit_points[i].track;
					moving_handle_left = animation->bezier_track_get_key_in_handle(edit_points[i].track, edit_points[i].key);
					moving_handle_right = animation->bezier_track_get_key_out_handle(edit_points[i].track, edit_points[i].key);
					queue_redraw();
					return;
				}
			}
		}

		// Box scaling/movement.
		if (inside_selection_handles_rect) {
			const Vector2i rel_pos = mb->get_position() - selection_rect.position;
			scaling_selection_handles = Vector2i();

			// Check which scaling handles are available.
			if (selection_rect.size.width > CMP_EPSILON) {
				if (rel_pos.x <= 0) {
					scaling_selection_handles.x = -1;
				} else if (rel_pos.x >= selection_rect.size.width) {
					scaling_selection_handles.x = 1;
				}
			}
			if (selection_rect.size.height > CMP_EPSILON) {
				if (rel_pos.y <= 0) {
					scaling_selection_handles.y = -1;
				} else if (rel_pos.y >= selection_rect.size.height) {
					scaling_selection_handles.y = 1;
				}
			}

			if (scaling_selection_handles != Vector2i()) {
				scaling_selection = true;

				const float time = ((selection_rect.position.x - limit) / timeline->get_zoom_scale()) + timeline->get_value();
				const float h = (get_size().height / 2.0 - selection_rect.position.y) * timeline_v_zoom + timeline_v_scroll;
				scaling_selection_pivot = Point2(time, h);

				return;
			}

			// If not scaling, that means we're moving.
			moving_selection_attempt = true;
			moving_selection = false;
			moving_selection_mouse_begin = mb->get_position();
			// The pivot will be from the mouse click location, not a specific key.
			moving_selection_from_key = -1;
			moving_selection_from_track = selected_track;
			moving_selection_offset = Vector2();
			select_single_attempt = IntPair(-1, -1);

			return;
		}

		// Insert new point.
		if (mb->get_position().x >= limit && mb->get_position().x < get_size().width && mb->is_command_or_control_pressed()) {
			float h = (get_size().height / 2.0 - mb->get_position().y) * timeline_v_zoom + timeline_v_scroll;
			Array new_point = animation->make_default_bezier_key(h);

			real_t time = ((mb->get_position().x - limit) / timeline->get_zoom_scale()) + timeline->get_value();
			while (animation->track_find_key(selected_track, time, Animation::FIND_MODE_APPROX) != -1) {
				time += 0.0001;
			}

			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Add Bezier Point"));
			undo_redo->add_do_method(animation.ptr(), "bezier_track_insert_key", selected_track, time, new_point[0], Vector2(new_point[1], new_point[2]), Vector2(new_point[3], new_point[4]));
			undo_redo->add_do_method(editor, "_bezier_track_set_key_handle_mode_at_time", animation.ptr(), selected_track, time, (Animation::HandleMode)editor->bezier_key_mode->get_selected_id(), Animation::HANDLE_SET_MODE_AUTO);
			undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", selected_track, time);
			undo_redo->commit_action();

			// Then attempt to move.
			int index = animation->track_find_key(selected_track, time, Animation::FIND_MODE_APPROX);
			ERR_FAIL_COND(index == -1);
			_clear_selection();
			_select_at_anim(animation, selected_track, animation->track_get_key_time(selected_track, index), true);

			moving_selection_attempt = true;
			moving_inserted_key = true;
			moving_selection = false;
			moving_selection_mouse_begin = mb->get_position();
			moving_selection_from_key = index;
			moving_selection_from_track = selected_track;
			moving_selection_offset = Vector2();
			select_single_attempt = IntPair(-1, -1);
			queue_redraw();

			return;
		}

		// Box select.
		if (mb->get_position().x >= limit && mb->get_position().x < get_size().width) {
			box_selecting_attempt = true;
			box_selecting = false;
			box_selecting_add = false;
			box_selection_from = mb->get_position();
			return;
		}
	}

	if (box_selecting_attempt && mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (box_selecting) {
			// Do actual select.
			if (!box_selecting_add) {
				_clear_selection();
			}

			Vector2 bs_from = box_selection_from;
			Vector2 bs_to = box_selection_to;
			if (bs_from.x > bs_to.x) {
				SWAP(bs_from.x, bs_to.x);
			}
			if (bs_from.y > bs_to.y) {
				SWAP(bs_from.y, bs_to.y);
			}
			Rect2 rect(bs_from, bs_to - bs_from);

			bool track_set = false;
			int j = 0;
			for (int i = 0; i < edit_points.size(); i++) {
				if (edit_points[i].point_rect.intersects(rect)) {
					_select_at_anim(animation, edit_points[i].track, animation->track_get_key_time(edit_points[i].track, edit_points[i].key), j == 0 && !box_selecting_add);
					if (!track_set) {
						track_set = true;
						set_animation_and_track(animation, edit_points[i].track, read_only);
					}
					j++;
				}
			}
		} else {
			_clear_selection(); // Clicked and nothing happened, so clear the selection.

			// Select by clicking on curve.
			int track_count = animation->get_track_count();

			real_t animation_length = animation->get_length();
			animation->set_length(real_t(INT_MAX)); // bezier_track_interpolate doesn't find keys if they exist beyond anim length.

			real_t time = ((mb->get_position().x - limit) / timeline->get_zoom_scale()) + timeline->get_value();

			for (int i = 0; i < track_count; ++i) {
				if (animation->track_get_type(i) != Animation::TrackType::TYPE_BEZIER || hidden_tracks.has(i) || locked_tracks.has(i)) {
					continue;
				}

				float track_h = animation->bezier_track_interpolate(i, time);
				float track_height = _bezier_h_to_pixel(track_h);

				if (std::abs(mb->get_position().y - track_height) < 10) {
					set_animation_and_track(animation, i, read_only);
					break;
				}
			}

			animation->set_length(animation_length);
		}

		box_selecting_attempt = false;
		box_selecting = false;
		queue_redraw();
	}

	if (moving_selection_attempt && mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (!read_only) {
			if (moving_selection && (std::abs(moving_selection_offset.x) > CMP_EPSILON || std::abs(moving_selection_offset.y) > CMP_EPSILON)) {
				// Commit it.
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Move Bezier Points"));

				List<AnimMoveRestore> to_restore;
				List<Animation::HandleMode> to_restore_handle_modes;
				// 1 - Remove the keys.
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					undo_redo->add_do_method(animation.ptr(), "track_remove_key", E->get().first, E->get().second);
				}
				// 2 - Remove overlapped keys.
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					real_t newtime = animation->track_get_key_time(E->get().first, E->get().second) + moving_selection_offset.x;

					int idx = animation->track_find_key(E->get().first, newtime, Animation::FIND_MODE_APPROX);
					if (idx == -1) {
						continue;
					}

					if (selection.has(IntPair(E->get().first, idx))) {
						continue; // Already in selection, don't save.
					}

					undo_redo->add_do_method(animation.ptr(), "track_remove_key_at_time", E->get().first, newtime);
					AnimMoveRestore amr;

					amr.key = animation->track_get_key_value(E->get().first, idx);
					amr.track = E->get().first;
					amr.time = newtime;

					to_restore.push_back(amr);
					to_restore_handle_modes.push_back(animation->bezier_track_get_key_handle_mode(E->get().first, idx));
				}

				// 3 - Move the keys (re-insert them).
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					real_t newpos = animation->track_get_key_time(E->get().first, E->get().second) + moving_selection_offset.x;
					Array key = animation->track_get_key_value(E->get().first, E->get().second);
					real_t h = key[0];
					h += moving_selection_offset.y;
					key[0] = h;

					Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(E->get().first, E->get().second);
					Animation::HandleSetMode handle_set_mode = Animation::HANDLE_SET_MODE_NONE;
					if (moving_inserted_key) {
						handle_mode = (Animation::HandleMode)editor->bezier_key_mode->get_selected_id();
						handle_set_mode = Animation::HANDLE_SET_MODE_AUTO;
					}

					undo_redo->add_do_method(
							this,
							"_bezier_track_insert_key_at_anim",
							animation,
							E->get().first,
							newpos,
							key[0],
							Vector2(key[1], key[2]),
							Vector2(key[3], key[4]),
							handle_mode,
							handle_set_mode);
				}

				// 4 - (undo) Remove inserted keys.
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					real_t newpos = animation->track_get_key_time(E->get().first, E->get().second) + moving_selection_offset.x;
					undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", E->get().first, newpos);
				}

				// 5 - (undo) Reinsert keys.
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					real_t oldpos = animation->track_get_key_time(E->get().first, E->get().second);
					Array key = animation->track_get_key_value(E->get().first, E->get().second);
					undo_redo->add_undo_method(
							this,
							"_bezier_track_insert_key_at_anim",
							animation,
							E->get().first,
							oldpos,
							key[0],
							Vector2(key[1], key[2]),
							Vector2(key[3], key[4]),
							animation->bezier_track_get_key_handle_mode(E->get().first, E->get().second));
				}

				// 6 - (undo) Reinsert overlapped keys.
				List<AnimMoveRestore>::ConstIterator restore_itr = to_restore.begin();
				List<Animation::HandleMode>::ConstIterator handle_itr = to_restore_handle_modes.begin();
				for (; restore_itr != to_restore.end() && handle_itr != to_restore_handle_modes.end(); ++restore_itr, ++handle_itr) {
					const AnimMoveRestore &amr = *restore_itr;
					Array key = amr.key;
					undo_redo->add_undo_method(animation.ptr(), "track_insert_key", amr.track, amr.time, amr.key, 1);
					undo_redo->add_undo_method(
							this,
							"_bezier_track_insert_key_at_anim",
							animation,
							amr.track,
							amr.time,
							key[0],
							Vector2(key[1], key[2]),
							Vector2(key[3], key[4]),
							*handle_itr);
				}

				undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
				undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);

				// 7 - Reselect.
				int i = 0;
				for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
					real_t oldpos = animation->track_get_key_time(E->get().first, E->get().second);
					real_t newpos = oldpos + moving_selection_offset.x;

					undo_redo->add_do_method(this, "_select_at_anim", animation, E->get().first, newpos, i == 0);
					undo_redo->add_undo_method(this, "_select_at_anim", animation, E->get().first, oldpos, i == 0);
					i++;
				}

				AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
				if (ape) {
					undo_redo->add_do_method(ape, "_animation_update_key_frame");
					undo_redo->add_undo_method(ape, "_animation_update_key_frame");
				}
				undo_redo->commit_action();

			} else if (select_single_attempt != IntPair(-1, -1)) {
				selection.clear();
				set_animation_and_track(animation, select_single_attempt.first, read_only);
				_select_at_anim(animation, select_single_attempt.first, animation->track_get_key_time(select_single_attempt.first, select_single_attempt.second), true);
			}

			moving_selection = false;
			moving_selection_attempt = false;
			moving_inserted_key = false;
			moving_selection_mouse_begin = Point2();
			queue_redraw();
		}
	}

	if (scaling_selection && mb.is_valid() && !read_only && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (std::abs(scaling_selection_scale.x - 1) > CMP_EPSILON || std::abs(scaling_selection_scale.y - 1) > CMP_EPSILON) {
			// Scale it.
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Scale Bezier Points"));

			List<AnimMoveRestore> to_restore;
			List<Animation::HandleMode> to_restore_handle_modes;
			// 1 - Remove the keys.
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				undo_redo->add_do_method(animation.ptr(), "track_remove_key", E->get().first, E->get().second);
			}
			// 2 - Remove overlapped keys.
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				real_t newtime = animation->track_get_key_time(E->get().first, E->get().second);
				newtime += -scaling_selection_offset.x + (newtime - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);

				int idx = animation->track_find_key(E->get().first, newtime, Animation::FIND_MODE_APPROX);
				if (idx == -1) {
					continue;
				}

				if (selection.has(IntPair(E->get().first, idx))) {
					continue; // Already in selection, don't save.
				}

				undo_redo->add_do_method(animation.ptr(), "track_remove_key_at_time", E->get().first, newtime);
				AnimMoveRestore amr;

				amr.key = animation->track_get_key_value(E->get().first, idx);
				amr.track = E->get().first;
				amr.time = newtime;

				to_restore.push_back(amr);
				to_restore_handle_modes.push_back(animation->bezier_track_get_key_handle_mode(E->get().first, idx));
			}

			// 3 - Scale the keys (re-insert them).
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				real_t newpos = animation->track_get_key_time(E->get().first, E->get().second);
				newpos += -scaling_selection_offset.x + (newpos - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);

				Array key = animation->track_get_key_value(E->get().first, E->get().second);
				real_t h = key[0];
				h += -scaling_selection_offset.y + (h - scaling_selection_pivot.y) * (scaling_selection_scale.y - 1);
				key[0] = h;

				undo_redo->add_do_method(
						this,
						"_bezier_track_insert_key_at_anim",
						animation,
						E->get().first,
						newpos,
						key[0],
						Vector2(key[1], key[2]),
						Vector2(key[3], key[4]),
						animation->bezier_track_get_key_handle_mode(E->get().first, E->get().second));
			}

			// 4 - (undo) Remove inserted keys.
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				real_t newpos = animation->track_get_key_time(E->get().first, E->get().second);
				newpos += -scaling_selection_offset.x + (newpos - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);
				undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", E->get().first, newpos);
			}

			// 5 - (undo) Reinsert keys.
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				real_t oldpos = animation->track_get_key_time(E->get().first, E->get().second);
				Array key = animation->track_get_key_value(E->get().first, E->get().second);
				undo_redo->add_undo_method(
						this,
						"_bezier_track_insert_key_at_anim",
						animation,
						E->get().first,
						oldpos,
						key[0],
						Vector2(key[1], key[2]),
						Vector2(key[3], key[4]),
						animation->bezier_track_get_key_handle_mode(E->get().first, E->get().second));
			}

			// 6 - (undo) Reinsert overlapped keys.
			List<AnimMoveRestore>::ConstIterator restore_itr = to_restore.begin();
			List<Animation::HandleMode>::ConstIterator handle_itr = to_restore_handle_modes.begin();
			for (; restore_itr != to_restore.end() && handle_itr != to_restore_handle_modes.end(); ++restore_itr, ++handle_itr) {
				const AnimMoveRestore &amr = *restore_itr;
				Array key = amr.key;
				undo_redo->add_undo_method(animation.ptr(), "track_insert_key", amr.track, amr.time, amr.key, 1);
				undo_redo->add_undo_method(
						this,
						"_bezier_track_insert_key_at_anim",
						animation,
						amr.track,
						amr.time,
						key[0],
						Vector2(key[1], key[2]),
						Vector2(key[3], key[4]),
						*handle_itr);
			}

			undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
			undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);

			// 7 - Reselect.
			int i = 0;
			for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
				real_t oldpos = animation->track_get_key_time(E->get().first, E->get().second);
				real_t newpos = animation->track_get_key_time(E->get().first, E->get().second);
				newpos += -scaling_selection_offset.x + (newpos - scaling_selection_pivot.x) * (scaling_selection_scale.x - 1);

				undo_redo->add_do_method(this, "_select_at_anim", animation, E->get().first, newpos, i == 0);
				undo_redo->add_undo_method(this, "_select_at_anim", animation, E->get().first, oldpos, i == 0);
				i++;
			}

			AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
			if (ape) {
				undo_redo->add_do_method(ape, "_animation_update_key_frame");
				undo_redo->add_undo_method(ape, "_animation_update_key_frame");
			}
			undo_redo->commit_action();
		}

		scaling_selection = false;
		scaling_selection_scale = Vector2(1, 1);
		scaling_selection_offset = Vector2();
		queue_redraw();
	}

	Ref<InputEventMouseMotion> mm = p_event;
	if (moving_selection_attempt && mm.is_valid()) {
		Point2 new_pos = mm->get_position();
		if (mm->is_alt_pressed()) { // Axis snap key move when alt is pressed
			if (Math::abs(new_pos.x - moving_selection_mouse_begin.x) > Math::abs(new_pos.y - moving_selection_mouse_begin.y)) {
				new_pos.y = moving_selection_mouse_begin.y;
			} else {
				new_pos.x = moving_selection_mouse_begin.x;
			}
		}

		if (!moving_selection) {
			moving_selection = true;
			select_single_attempt = IntPair(-1, -1);
		}

		if (!read_only) {
			float y = (get_size().height / 2.0 - new_pos.y) * timeline_v_zoom + timeline_v_scroll;
			float moving_selection_begin_time = ((moving_selection_mouse_begin.x - limit) / timeline->get_zoom_scale()) + timeline->get_value();
			float new_time = ((new_pos.x - limit) / timeline->get_zoom_scale()) + timeline->get_value();
			float moving_selection_pivot = moving_selection_from_key != -1 ? animation->track_get_key_time(moving_selection_from_track, moving_selection_from_key) : 0;
			float time_delta = new_time - moving_selection_begin_time;

			float snapped_time = editor->snap_time(moving_selection_pivot + time_delta);
			float time_offset = 0.0;
			if (std::abs(moving_selection_offset.x) > CMP_EPSILON || (snapped_time > moving_selection_pivot && time_delta > CMP_EPSILON) || (snapped_time < moving_selection_pivot && time_delta < -CMP_EPSILON)) {
				time_offset = snapped_time - moving_selection_pivot;
			}

			float moving_selection_begin_value;
			if (moving_selection_from_key == -1) {
				moving_selection_begin_value = (get_size().height / 2.0 - moving_selection_mouse_begin.y) * timeline_v_zoom + timeline_v_scroll;
			} else {
				moving_selection_begin_value = animation->bezier_track_get_key_value(moving_selection_from_track, moving_selection_from_key);
			}

			float y_offset = y - moving_selection_begin_value;
			moving_selection_offset = Vector2(time_offset, y_offset);
		}

		additional_moving_handle_lefts.clear();
		additional_moving_handle_rights.clear();

		queue_redraw();
	}

	if (box_selecting_attempt && mm.is_valid()) {
		if (!box_selecting) {
			box_selecting = true;
			box_selecting_add = mm->is_shift_pressed();
		}

		box_selection_to = mm->get_position();
		queue_redraw();
	}

	if (scaling_selection && mm.is_valid() && !read_only) {
		Point2 mp = mm->get_position();
		const int handle_length = Math::round((selection_handles_rect.size.width - selection_rect.size.width) / 4.0);
		Point2 rel_pos;

		// Calculate the scale according with the distance between the mouse's position (adjusted so that the cursor appears inside the handles)
		// and the opposite end of the `selection_rect`.

		if (scaling_selection_handles.x != 0) {
			if (scaling_selection_handles.x == 1) { // Right Handle
				const int handle_adjust = Math::round(mp.x - (scaling_selection_scale.x >= 0 ? selection_rect.position.x : (selection_rect.position.x + selection_rect.size.width)));
				mp.x -= MIN(Math::abs(handle_adjust), handle_length) * scaling_selection_handles.x * SIGN(handle_adjust);

				if (editor->is_snap_keys_enabled()) {
					mp.x = editor->snap_time((mp.x - limit) / timeline->get_zoom_scale(), true) + timeline->get_value();
					mp.x = (mp.x - timeline->get_value()) * timeline->get_zoom_scale() + limit;
				}

				rel_pos.x = scaling_selection_scale.x >= 0 ? (mp.x - selection_rect.position.x) : selection_rect.position.x + selection_rect.size.width - mp.x;
			} else { // Left Handle
				const int handle_adjust = Math::round((scaling_selection_scale.x >= 0 ? (selection_rect.position.x + selection_rect.size.width) : selection_rect.position.x) - mp.x);
				mp.x -= MIN(Math::abs(handle_adjust), handle_length) * scaling_selection_handles.x * SIGN(handle_adjust);

				const float x = editor->snap_time((mp.x - limit) / timeline->get_zoom_scale(), true) + timeline->get_value();
				if (editor->is_snap_keys_enabled()) {
					mp.x = (x - timeline->get_value()) * timeline->get_zoom_scale() + limit;
				}

				rel_pos.x = scaling_selection_scale.x >= 0 ? (selection_rect.position.x + selection_rect.size.width - mp.x) : (mp.x - selection_rect.position.x);
				scaling_selection_offset.x = scaling_selection_pivot.x - x;
			}

			scaling_selection_scale.x *= rel_pos.x / selection_rect.size.width;
			if (scaling_selection_scale.x == 0) {
				scaling_selection_scale.x = CMP_EPSILON;
			}
		}

		if (scaling_selection_handles.y != 0) {
			if (scaling_selection_handles.y == 1) { // Bottom Handle
				const int handle_adjust = Math::round(mp.y - (scaling_selection_scale.y >= 0 ? selection_rect.position.y : (selection_rect.position.y + selection_rect.size.height)));
				mp.y -= MIN(Math::abs(handle_adjust), handle_length) * scaling_selection_handles.y * SIGN(handle_adjust);

				if (scaling_selection_scale.y >= 0) {
					rel_pos.y = mp.y - selection_rect.position.y;
				} else {
					rel_pos.y = selection_rect.position.y + selection_rect.size.height - mp.y;
				}
			} else { // Top Handle
				const int handle_adjust = Math::round((scaling_selection_scale.y >= 0 ? (selection_rect.position.y + selection_rect.size.height) : selection_rect.position.y) - mp.y);
				mp.y -= MIN(Math::abs(handle_adjust), handle_length) * scaling_selection_handles.y * SIGN(handle_adjust);

				if (scaling_selection_scale.y >= 0) {
					rel_pos.y = selection_rect.position.y + selection_rect.size.height - mp.y;
				} else {
					rel_pos.y = mp.y - selection_rect.position.y;
				}

				const float h = (get_size().height / 2.0 - mp.y) * timeline_v_zoom + timeline_v_scroll;
				scaling_selection_offset.y = scaling_selection_pivot.y - h;
			}

			scaling_selection_scale.y *= rel_pos.y / selection_rect.size.height;
			if (scaling_selection_scale.y == 0) {
				scaling_selection_scale.y = CMP_EPSILON;
			}
		}

		queue_redraw();
	}

	if ((moving_handle == 1 || moving_handle == -1) && mm.is_valid()) {
		float y = (get_size().height / 2.0 - mm->get_position().y) * timeline_v_zoom + timeline_v_scroll;
		float x = editor->snap_time((mm->get_position().x - limit) / timeline->get_zoom_scale()) + timeline->get_value();

		Vector2 key_pos = Vector2(animation->track_get_key_time(moving_handle_track, moving_handle_key), animation->bezier_track_get_key_value(moving_handle_track, moving_handle_key));

		Vector2 moving_handle_value = Vector2(x, y) - key_pos;

		moving_handle_left = animation->bezier_track_get_key_in_handle(moving_handle_track, moving_handle_key);
		moving_handle_right = animation->bezier_track_get_key_out_handle(moving_handle_track, moving_handle_key);

		if (moving_handle == -1) {
			moving_handle_left = moving_handle_value;

			Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(moving_handle_track, moving_handle_key);

			if (handle_mode == Animation::HANDLE_MODE_BALANCED) {
				real_t ratio = timeline->get_zoom_scale() * timeline_v_zoom;
				Transform2D xform;
				xform.set_scale(Vector2(1.0, 1.0 / ratio));

				Vector2 vec_out = xform.xform(moving_handle_right);
				Vector2 vec_in = xform.xform(moving_handle_left);

				moving_handle_right = xform.affine_inverse().xform(-vec_in.normalized() * vec_out.length());
			} else if (handle_mode == Animation::HANDLE_MODE_MIRRORED) {
				moving_handle_right = -moving_handle_left;
			}
		} else if (moving_handle == 1) {
			moving_handle_right = moving_handle_value;

			Animation::HandleMode handle_mode = animation->bezier_track_get_key_handle_mode(moving_handle_track, moving_handle_key);

			if (handle_mode == Animation::HANDLE_MODE_BALANCED) {
				real_t ratio = timeline->get_zoom_scale() * timeline_v_zoom;
				Transform2D xform;
				xform.set_scale(Vector2(1.0, 1.0 / ratio));

				Vector2 vec_in = xform.xform(moving_handle_left);
				Vector2 vec_out = xform.xform(moving_handle_right);

				moving_handle_left = xform.affine_inverse().xform(-vec_out.normalized() * vec_in.length());
			} else if (handle_mode == Animation::HANDLE_MODE_MIRRORED) {
				moving_handle_left = -moving_handle_right;
			}
		}
		queue_redraw();
	}

	if ((moving_handle == -1 || moving_handle == 1) && mb.is_valid() && !mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
		if (!read_only) {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			undo_redo->create_action(TTR("Move Bezier Points"));
			if (moving_handle == -1) {
				real_t ratio = timeline->get_zoom_scale() * timeline_v_zoom;
				undo_redo->add_do_method(animation.ptr(), "bezier_track_set_key_in_handle", moving_handle_track, moving_handle_key, moving_handle_left, ratio);
				undo_redo->add_undo_method(animation.ptr(), "bezier_track_set_key_in_handle", moving_handle_track, moving_handle_key, animation->bezier_track_get_key_in_handle(moving_handle_track, moving_handle_key), ratio);
			} else if (moving_handle == 1) {
				real_t ratio = timeline->get_zoom_scale() * timeline_v_zoom;
				undo_redo->add_do_method(animation.ptr(), "bezier_track_set_key_out_handle", moving_handle_track, moving_handle_key, moving_handle_right, ratio);
				undo_redo->add_undo_method(animation.ptr(), "bezier_track_set_key_out_handle", moving_handle_track, moving_handle_key, animation->bezier_track_get_key_out_handle(moving_handle_track, moving_handle_key), ratio);
			}
			AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
			if (ape) {
				undo_redo->add_do_method(ape, "_animation_update_key_frame");
				undo_redo->add_undo_method(ape, "_animation_update_key_frame");
			}
			undo_redo->commit_action();
			moving_handle = 0;
			queue_redraw();
		}
	}
}

bool AnimationBezierTrackEdit::_try_select_at_ui_pos(const Point2 &p_pos, bool p_aggregate, bool p_deselectable) {
	for (int i = 0; i < edit_points.size(); i++) {
		// Path 2D editing in the 3D and 2D editors works the same way. (?)
		if (edit_points[i].point_rect.has_point(p_pos)) {
			IntPair pair = IntPair(edit_points[i].track, edit_points[i].key);
			if (p_aggregate) {
				// Add to selection.
				if (selection.has(pair)) {
					if (p_deselectable) {
						selection.erase(pair);
						emit_signal(SNAME("deselect_key"), edit_points[i].key, edit_points[i].track);
					}
				} else {
					_select_at_anim(animation, edit_points[i].track, animation->track_get_key_time(edit_points[i].track, edit_points[i].key), false);
				}
				queue_redraw();
				select_single_attempt = IntPair(-1, -1);
			} else {
				if (p_deselectable) {
					moving_selection_attempt = true;
					moving_selection_from_key = pair.second;
					moving_selection_from_track = pair.first;
					moving_selection_mouse_begin = p_pos;
					moving_selection_offset = Vector2();
					moving_handle_track = pair.first;
					moving_handle_left = animation->bezier_track_get_key_in_handle(pair.first, pair.second);
					moving_handle_right = animation->bezier_track_get_key_out_handle(pair.first, pair.second);

					if (selection.has(pair)) {
						moving_selection = false;
					} else {
						moving_selection = true;
					}
					select_single_attempt = pair;
				}

				set_animation_and_track(animation, pair.first, read_only);
				if (!selection.has(pair)) {
					selection.clear();
					_select_at_anim(animation, edit_points[i].track, animation->track_get_key_time(edit_points[i].track, edit_points[i].key), true);
				}
			}
			return true;
		}
	}
	return false;
}

void AnimationBezierTrackEdit::_pan_callback(Vector2 p_scroll_vec, Ref<InputEvent> p_event) {
	Ref<InputEventMouseMotion> mm = p_event;
	if (mm.is_valid()) {
		if (mm->get_position().x > timeline->get_name_limit()) {
			timeline_v_scroll += p_scroll_vec.y * timeline_v_zoom;
			timeline_v_scroll = CLAMP(timeline_v_scroll, -100000, 100000);
			timeline->set_value(timeline->get_value() - p_scroll_vec.x / timeline->get_zoom_scale());
		} else {
			track_v_scroll += p_scroll_vec.y;
			if (track_v_scroll < -track_v_scroll_max) {
				track_v_scroll = -track_v_scroll_max;
			} else if (track_v_scroll > 0) {
				track_v_scroll = 0;
			}
		}
		queue_redraw();
	}
}

void AnimationBezierTrackEdit::_zoom_callback(float p_zoom_factor, Vector2 p_origin, Ref<InputEvent> p_event) {
	const float v_zoom_orig = timeline_v_zoom;
	Ref<InputEventWithModifiers> iewm = p_event;
	if (iewm.is_valid() && iewm->is_alt_pressed()) {
		// Alternate zoom (doesn't affect timeline).
		timeline_v_zoom = CLAMP(timeline_v_zoom / p_zoom_factor, 0.000001, 100000);
	} else {
		float zoom_factor = p_zoom_factor > 1.0 ? AnimationTimelineEdit::SCROLL_ZOOM_FACTOR_IN : AnimationTimelineEdit::SCROLL_ZOOM_FACTOR_OUT;
		timeline->_zoom_callback(zoom_factor, p_origin, p_event);
	}
	timeline_v_scroll = timeline_v_scroll + (p_origin.y - get_size().y / 2.0) * (timeline_v_zoom - v_zoom_orig);
	queue_redraw();
}

float AnimationBezierTrackEdit::get_bezier_key_value(Array p_bezier_key_array) {
	return p_bezier_key_array[0];
}

void AnimationBezierTrackEdit::_menu_selected(int p_index) {
	int limit = timeline->get_name_limit();

	real_t time = ((menu_insert_key.x - limit) / timeline->get_zoom_scale()) + timeline->get_value();

	switch (p_index) {
		case MENU_KEY_INSERT: {
			if (animation->get_track_count() > 0) {
				if (editor->snap_keys->is_pressed() && editor->step->get_value() != 0) {
					time = editor->snap_time(time);
				}
				while (animation->track_find_key(selected_track, time, Animation::FIND_MODE_APPROX) != -1) {
					time += 0.001;
				}
				float h = (get_size().height / 2.0 - menu_insert_key.y) * timeline_v_zoom + timeline_v_scroll;
				Array new_point = animation->make_default_bezier_key(h);
				Animation::HandleMode handle_mode = (Animation::HandleMode)editor->bezier_key_mode->get_selected_id();
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				undo_redo->create_action(TTR("Add Bezier Point"));
				undo_redo->add_do_method(animation.ptr(), "track_insert_key", selected_track, time, new_point);
				undo_redo->add_do_method(editor, "_bezier_track_set_key_handle_mode_at_time", animation.ptr(), selected_track, time, handle_mode, Animation::HANDLE_SET_MODE_AUTO);
				undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);
				undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", selected_track, time);
				AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
				if (ape) {
					undo_redo->add_do_method(ape, "_animation_update_key_frame");
					undo_redo->add_undo_method(ape, "_animation_update_key_frame");
				}
				undo_redo->commit_action();
				queue_redraw();
			}
		} break;
		case MENU_KEY_DUPLICATE: {
			duplicate_selected_keys(time, true);
		} break;
		case MENU_KEY_DELETE: {
			delete_selection();
		} break;
		case MENU_KEY_CUT: {
			copy_selected_keys(true);
		} break;
		case MENU_KEY_COPY: {
			copy_selected_keys(false);
		} break;
		case MENU_KEY_PASTE: {
			paste_keys(time, true);
		} break;
		case MENU_KEY_SET_HANDLE_FREE: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_FREE);
		} break;
		case MENU_KEY_SET_HANDLE_LINEAR: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_LINEAR);
		} break;
		case MENU_KEY_SET_HANDLE_BALANCED: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_BALANCED);
		} break;
		case MENU_KEY_SET_HANDLE_MIRRORED: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_MIRRORED);
		} break;
		case MENU_KEY_SET_HANDLE_AUTO_BALANCED: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_BALANCED, true);
		} break;
		case MENU_KEY_SET_HANDLE_AUTO_MIRRORED: {
			_change_selected_keys_handle_mode(Animation::HANDLE_MODE_MIRRORED, true);
		} break;
	}
}

void AnimationBezierTrackEdit::duplicate_selected_keys(real_t p_ofs, bool p_ofs_valid) {
	if (selection.is_empty()) {
		return;
	}

	real_t top_time = 1e10;
	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		real_t t = animation->track_get_key_time(E->get().first, E->get().second);
		if (t < top_time) {
			top_time = t;
		}
	}

	EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
	undo_redo->create_action(TTR("Animation Duplicate Keys"));

	List<Pair<int, real_t>> new_selection_values;

	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		real_t t = animation->track_get_key_time(E->get().first, E->get().second);
		real_t insert_pos = p_ofs_valid ? p_ofs : timeline->get_play_position();

		if (p_ofs_valid) {
			if (editor->snap_keys->is_pressed() && editor->step->get_value() != 0) {
				insert_pos = editor->snap_time(insert_pos);
			}
		}

		real_t dst_time = t + (insert_pos - top_time);
		int existing_idx = animation->track_find_key(E->get().first, dst_time, Animation::FIND_MODE_APPROX);

		undo_redo->add_do_method(animation.ptr(), "track_insert_key", E->get().first, dst_time, animation->track_get_key_value(E->get().first, E->get().second), animation->track_get_key_transition(E->get().first, E->get().second));
		undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", E->get().first, dst_time);

		Pair<int, real_t> p;
		p.first = E->get().first;
		p.second = dst_time;
		new_selection_values.push_back(p);

		if (existing_idx != -1) {
			undo_redo->add_undo_method(animation.ptr(), "track_insert_key", E->get().first, dst_time, animation->track_get_key_value(E->get().first, existing_idx), animation->track_get_key_transition(E->get().first, existing_idx));
		}
	}

	undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
	undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);

	// Reselect duplicated.
	int i = 0;
	for (const Pair<int, real_t> &E : new_selection_values) {
		undo_redo->add_do_method(this, "_select_at_anim", animation, E.first, E.second, i == 0);
		i++;
	}
	i = 0;
	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		real_t time = animation->track_get_key_time(E->get().first, E->get().second);
		undo_redo->add_undo_method(this, "_select_at_anim", animation, E->get().first, time, i == 0);
		i++;
	}

	AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
	if (ape) {
		undo_redo->add_do_method(ape, "_animation_update_key_frame");
		undo_redo->add_undo_method(ape, "_animation_update_key_frame");
	}
	undo_redo->add_do_method(this, "queue_redraw");
	undo_redo->add_undo_method(this, "queue_redraw");
	undo_redo->commit_action();
}

void AnimationBezierTrackEdit::copy_selected_keys(bool p_cut) {
	if (selection.is_empty()) {
		return;
	}

	float top_time = 1e10;
	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		float t = animation->track_get_key_time(E->get().first, E->get().second);
		if (t < top_time) {
			top_time = t;
		}
	}

	RBMap<AnimationTrackEditor::SelectedKey, AnimationTrackEditor::KeyInfo> keys;
	for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
		AnimationTrackEditor::SelectedKey sk;
		AnimationTrackEditor::KeyInfo ki;
		sk.track = E->get().first;
		sk.key = E->get().second;
		ki.pos = animation->track_get_key_time(E->get().first, E->get().second);
		keys.insert(sk, ki);
	}
	editor->_set_key_clipboard(selected_track, top_time, keys);

	if (p_cut) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Animation Cut Keys"), UndoRedo::MERGE_DISABLE, animation.ptr());
		undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
		undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);
		int i = 0;
		for (RBMap<AnimationTrackEditor::SelectedKey, AnimationTrackEditor::KeyInfo>::Element *E = keys.back(); E; E = E->prev()) {
			int track_idx = E->key().track;
			int key_idx = E->key().key;
			float time = E->value().pos;
			undo_redo->add_do_method(animation.ptr(), "track_remove_key_at_time", track_idx, time);
			undo_redo->add_undo_method(animation.ptr(), "track_insert_key", track_idx, time, animation->track_get_key_value(track_idx, key_idx), animation->track_get_key_transition(track_idx, key_idx));
			undo_redo->add_undo_method(this, "_select_at_anim", animation, track_idx, time, i == 0);
			i++;
		}
		i = 0;
		for (RBMap<AnimationTrackEditor::SelectedKey, AnimationTrackEditor::KeyInfo>::Element *E = keys.back(); E; E = E->prev()) {
			undo_redo->add_undo_method(this, "_select_at_anim", animation, E->key().track, E->value().pos, i == 0);
			i++;
		}

		AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
		if (ape) {
			undo_redo->add_do_method(ape, "_animation_update_key_frame");
			undo_redo->add_undo_method(ape, "_animation_update_key_frame");
		}
		undo_redo->add_do_method(this, "queue_redraw");
		undo_redo->add_undo_method(this, "queue_redraw");

		undo_redo->commit_action();
	}
}

void AnimationBezierTrackEdit::paste_keys(real_t p_ofs, bool p_ofs_valid) {
	if (editor->is_key_clipboard_active() && animation.is_valid() && (selected_track >= 0 && selected_track < animation->get_track_count())) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Animation Paste Keys"));

		bool same_track = true;
		bool all_compatible = true;

		for (int i = 0; i < editor->key_clipboard.keys.size(); i++) {
			const AnimationTrackEditor::KeyClipboard::Key key = editor->key_clipboard.keys[i];

			if (key.track != 0) {
				same_track = false;
				break;
			}

			if (!editor->_is_track_compatible(selected_track, key.value.get_type(), key.track_type)) {
				all_compatible = false;
				break;
			}
		}

		ERR_FAIL_COND_MSG(!all_compatible, "Paste failed: Not all animation keys were compatible with their target tracks");
		if (!same_track) {
			WARN_PRINT("Pasted animation keys from multiple tracks into single Bezier track");
		}

		List<Pair<int, float>> new_selection_values;
		for (int i = 0; i < editor->key_clipboard.keys.size(); i++) {
			const AnimationTrackEditor::KeyClipboard::Key key = editor->key_clipboard.keys[i];

			float insert_pos = p_ofs_valid ? p_ofs : timeline->get_play_position();
			if (p_ofs_valid) {
				if (editor->snap_keys->is_pressed() && editor->step->get_value() != 0) {
					insert_pos = editor->snap_time(insert_pos);
				}
			}
			float dst_time = key.time + insert_pos;

			int existing_idx = animation->track_find_key(selected_track, dst_time, Animation::FIND_MODE_APPROX);

			Variant value = key.value;
			if (key.track_type != Animation::TYPE_BEZIER) {
				value = animation->make_default_bezier_key(key.value);
			}

			undo_redo->add_do_method(animation.ptr(), "track_insert_key", selected_track, dst_time, value, key.transition);
			undo_redo->add_undo_method(animation.ptr(), "track_remove_key_at_time", selected_track, dst_time);

			Pair<int, float> p;
			p.first = selected_track;
			p.second = dst_time;
			new_selection_values.push_back(p);

			if (existing_idx != -1) {
				undo_redo->add_undo_method(animation.ptr(), "track_insert_key", selected_track, dst_time, animation->track_get_key_value(selected_track, existing_idx), animation->track_get_key_transition(selected_track, existing_idx));
			}
		}

		undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
		undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);

		// Reselect pasted.
		int i = 0;
		for (const Pair<int, float> &E : new_selection_values) {
			undo_redo->add_do_method(this, "_select_at_anim", animation, E.first, E.second, i == 0);
			i++;
		}
		i = 0;
		for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
			undo_redo->add_undo_method(this, "_select_at_anim", animation, E->get().first, animation->track_get_key_time(E->get().first, E->get().second), i == 0);
			i++;
		}

		AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
		if (ape) {
			undo_redo->add_do_method(ape, "_animation_update_key_frame");
			undo_redo->add_undo_method(ape, "_animation_update_key_frame");
		}
		undo_redo->add_do_method(this, "queue_redraw");
		undo_redo->add_undo_method(this, "queue_redraw");

		undo_redo->commit_action();
	}
}

void AnimationBezierTrackEdit::delete_selection() {
	if (selection.size()) {
		EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
		undo_redo->create_action(TTR("Animation Delete Keys"));

		for (SelectionSet::Element *E = selection.back(); E; E = E->prev()) {
			undo_redo->add_do_method(animation.ptr(), "track_remove_key", E->get().first, E->get().second);
			undo_redo->add_undo_method(animation.ptr(), "track_insert_key", E->get().first, animation->track_get_key_time(E->get().first, E->get().second), animation->track_get_key_value(E->get().first, E->get().second), 1);
		}
		undo_redo->add_do_method(this, "_clear_selection_for_anim", animation);
		undo_redo->add_undo_method(this, "_clear_selection_for_anim", animation);
		AnimationPlayerEditor *ape = AnimationPlayerEditor::get_singleton();
		if (ape) {
			undo_redo->add_do_method(ape, "_animation_update_key_frame");
			undo_redo->add_undo_method(ape, "_animation_update_key_frame");
		}
		undo_redo->commit_action();

		//selection.clear();
	}
}

void AnimationBezierTrackEdit::_bezier_track_insert_key_at_anim(const Ref<Animation> &p_anim, int p_track, double p_time, real_t p_value, const Vector2 &p_in_handle, const Vector2 &p_out_handle, const Animation::HandleMode p_handle_mode, Animation::HandleSetMode p_handle_set_mode) {
	int idx = p_anim->bezier_track_insert_key(p_track, p_time, p_value, p_in_handle, p_out_handle);
	p_anim->bezier_track_set_key_handle_mode(p_track, idx, p_handle_mode, p_handle_set_mode);
}

void AnimationBezierTrackEdit::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_clear_selection"), &AnimationBezierTrackEdit::_clear_selection);
	ClassDB::bind_method(D_METHOD("_clear_selection_for_anim"), &AnimationBezierTrackEdit::_clear_selection_for_anim);
	ClassDB::bind_method(D_METHOD("_select_at_anim"), &AnimationBezierTrackEdit::_select_at_anim);
	ClassDB::bind_method(D_METHOD("_update_hidden_tracks_after"), &AnimationBezierTrackEdit::_update_hidden_tracks_after);
	ClassDB::bind_method(D_METHOD("_update_locked_tracks_after"), &AnimationBezierTrackEdit::_update_locked_tracks_after);
	ClassDB::bind_method(D_METHOD("_bezier_track_insert_key_at_anim"), &AnimationBezierTrackEdit::_bezier_track_insert_key_at_anim, DEFVAL(Animation::HANDLE_SET_MODE_NONE));

	ADD_SIGNAL(MethodInfo("select_key", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::BOOL, "single"), PropertyInfo(Variant::INT, "track")));
	ADD_SIGNAL(MethodInfo("deselect_key", PropertyInfo(Variant::INT, "index"), PropertyInfo(Variant::INT, "track")));
	ADD_SIGNAL(MethodInfo("clear_selection"));
	ADD_SIGNAL(MethodInfo("timeline_changed", PropertyInfo(Variant::FLOAT, "position"), PropertyInfo(Variant::BOOL, "timeline_only")));
}

AnimationBezierTrackEdit::AnimationBezierTrackEdit() {
	panner.instantiate();
	panner->set_callbacks(callable_mp(this, &AnimationBezierTrackEdit::_pan_callback), callable_mp(this, &AnimationBezierTrackEdit::_zoom_callback));

	play_position = memnew(Control);
	play_position->set_mouse_filter(MOUSE_FILTER_PASS);
	add_child(play_position);
	play_position->set_anchors_and_offsets_preset(PRESET_FULL_RECT);
	play_position->connect(SceneStringName(draw), callable_mp(this, &AnimationBezierTrackEdit::_play_position_draw));
	set_focus_mode(FOCUS_CLICK);

	set_clip_contents(true);

	ED_SHORTCUT("animation_bezier_editor/focus", TTRC("Focus"), Key::F);
	ED_SHORTCUT("animation_bezier_editor/select_all_keys", TTRC("Select All Keys"), KeyModifierMask::CMD_OR_CTRL | Key::A);
	ED_SHORTCUT("animation_bezier_editor/deselect_all_keys", TTRC("Deselect All Keys"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::A);

	menu = memnew(PopupMenu);
	add_child(menu);
	menu->connect(SceneStringName(id_pressed), callable_mp(this, &AnimationBezierTrackEdit::_menu_selected));
}
