/**************************************************************************/
/*  style_box_flat.cpp                                                    */
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

#include "style_box_flat.h"

#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/rendering_server.h"

float StyleBoxFlat::get_style_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);
	return border_width[p_side];
}

void StyleBoxFlat::_validate_property(PropertyInfo &p_property) const {
	if (!Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (!anti_aliased && p_property.name == "anti_aliasing_size") {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

void StyleBoxFlat::set_bg_color(const Color &p_color) {
	bg_color = p_color;
	emit_changed();
}

Color StyleBoxFlat::get_bg_color() const {
	return bg_color;
}

void StyleBoxFlat::set_border_color(const Color &p_color) {
	border_color = p_color;
	emit_changed();
}

Color StyleBoxFlat::get_border_color() const {
	return border_color;
}

void StyleBoxFlat::set_border_width_all(int p_size) {
	border_width[0] = p_size;
	border_width[1] = p_size;
	border_width[2] = p_size;
	border_width[3] = p_size;
	emit_changed();
}

int StyleBoxFlat::get_border_width_min() const {
	return MIN(MIN(border_width[0], border_width[1]), MIN(border_width[2], border_width[3]));
}

void StyleBoxFlat::set_border_width(Side p_side, int p_width) {
	ERR_FAIL_INDEX((int)p_side, 4);
	border_width[p_side] = p_width;
	emit_changed();
}

int StyleBoxFlat::get_border_width(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0);
	return border_width[p_side];
}

void StyleBoxFlat::set_border_blend(bool p_blend) {
	blend_border = p_blend;
	emit_changed();
}

bool StyleBoxFlat::get_border_blend() const {
	return blend_border;
}

void StyleBoxFlat::set_corner_radius(const Corner p_corner, const int radius) {
	ERR_FAIL_INDEX((int)p_corner, 4);
	corner_radius[p_corner] = radius;
	emit_changed();
}

void StyleBoxFlat::set_corner_radius_all(int radius) {
	for (int i = 0; i < 4; i++) {
		corner_radius[i] = radius;
	}

	emit_changed();
}

void StyleBoxFlat::set_corner_radius_individual(const int radius_top_left, const int radius_top_right, const int radius_bottom_right, const int radius_bottom_left) {
	corner_radius[0] = radius_top_left;
	corner_radius[1] = radius_top_right;
	corner_radius[2] = radius_bottom_right;
	corner_radius[3] = radius_bottom_left;

	emit_changed();
}

int StyleBoxFlat::get_corner_radius(const Corner p_corner) const {
	ERR_FAIL_INDEX_V((int)p_corner, 4, 0);
	return corner_radius[p_corner];
}

void StyleBoxFlat::set_corner_detail(const int &p_corner_detail) {
	corner_detail = CLAMP(p_corner_detail, 1, 20);
	emit_changed();
}

int StyleBoxFlat::get_corner_detail() const {
	return corner_detail;
}

void StyleBoxFlat::set_expand_margin(Side p_side, float p_size) {
	ERR_FAIL_INDEX((int)p_side, 4);
	expand_margin[p_side] = p_size;
	emit_changed();
}

void StyleBoxFlat::set_expand_margin_all(float p_expand_margin_size) {
	for (int i = 0; i < 4; i++) {
		expand_margin[i] = p_expand_margin_size;
	}
	emit_changed();
}

void StyleBoxFlat::set_expand_margin_individual(float p_left, float p_top, float p_right, float p_bottom) {
	expand_margin[SIDE_LEFT] = p_left;
	expand_margin[SIDE_TOP] = p_top;
	expand_margin[SIDE_RIGHT] = p_right;
	expand_margin[SIDE_BOTTOM] = p_bottom;
	emit_changed();
}

float StyleBoxFlat::get_expand_margin(Side p_side) const {
	ERR_FAIL_INDEX_V((int)p_side, 4, 0.0);
	return expand_margin[p_side];
}

void StyleBoxFlat::set_draw_center(bool p_enabled) {
	draw_center = p_enabled;
	emit_changed();
}

bool StyleBoxFlat::is_draw_center_enabled() const {
	return draw_center;
}

void StyleBoxFlat::set_skew(Vector2 p_skew) {
	skew = p_skew;
	emit_changed();
}

Vector2 StyleBoxFlat::get_skew() const {
	return skew;
}

void StyleBoxFlat::set_shadow_color(const Color &p_color) {
	shadow_color = p_color;
	emit_changed();
}

Color StyleBoxFlat::get_shadow_color() const {
	return shadow_color;
}

void StyleBoxFlat::set_shadow_size(const int &p_size) {
	shadow_size = p_size;
	emit_changed();
}

int StyleBoxFlat::get_shadow_size() const {
	return shadow_size;
}

void StyleBoxFlat::set_shadow_offset(const Point2 &p_offset) {
	shadow_offset = p_offset;
	emit_changed();
}

Point2 StyleBoxFlat::get_shadow_offset() const {
	return shadow_offset;
}

void StyleBoxFlat::set_anti_aliased(const bool &p_anti_aliased) {
	anti_aliased = p_anti_aliased;
	emit_changed();
	notify_property_list_changed();
}

bool StyleBoxFlat::is_anti_aliased() const {
	return anti_aliased;
}

void StyleBoxFlat::set_aa_size(const real_t p_aa_size) {
	aa_size = CLAMP(p_aa_size, 0.01, 10);
	emit_changed();
}

real_t StyleBoxFlat::get_aa_size() const {
	return aa_size;
}

inline void set_inner_corner_radius(const Rect2 style_rect, const Rect2 inner_rect, const real_t corner_radius[4], real_t *inner_corner_radius) {
	real_t border_left = inner_rect.position.x - style_rect.position.x;
	real_t border_top = inner_rect.position.y - style_rect.position.y;
	real_t border_right = style_rect.size.width - inner_rect.size.width - border_left;
	real_t border_bottom = style_rect.size.height - inner_rect.size.height - border_top;

	inner_corner_radius[0] = MAX(corner_radius[0] - MIN(border_top, border_left), 0); // Top left.
	inner_corner_radius[1] = MAX(corner_radius[1] - MIN(border_top, border_right), 0); // Top right.
	inner_corner_radius[2] = MAX(corner_radius[2] - MIN(border_bottom, border_right), 0); // Bottom right.
	inner_corner_radius[3] = MAX(corner_radius[3] - MIN(border_bottom, border_left), 0); // Bottom left.
}

inline void set_corner_scale(const Rect2 &style_rect, const Rect2 &inner_rect, const real_t corner_radius[4], Point2 *inner_scale) {
	real_t border_left = inner_rect.position.x - style_rect.position.x;
	real_t border_top = inner_rect.position.y - style_rect.position.y;
	real_t border_right = style_rect.size.width - inner_rect.size.width - border_left;
	real_t border_bottom = style_rect.size.height - inner_rect.size.height - border_top;

	// Amount of overflow along an edge.
	// Ex. SIDE_LEFT edge is the overflow between top_left and bottom_left corners.
	// MIN(0,) is to ignore underflow, and negating is to make values positive.
	real_t edge_overflow[4] = {
		-MIN(0, inner_rect.size.y - corner_radius[CORNER_TOP_LEFT] - corner_radius[CORNER_BOTTOM_LEFT]),
		-MIN(0, inner_rect.size.x - corner_radius[CORNER_TOP_LEFT] - corner_radius[CORNER_TOP_RIGHT]),
		-MIN(0, inner_rect.size.y - corner_radius[CORNER_TOP_RIGHT] - corner_radius[CORNER_BOTTOM_RIGHT]),
		-MIN(0, inner_rect.size.x - corner_radius[CORNER_BOTTOM_LEFT] - corner_radius[CORNER_BOTTOM_RIGHT])
	};

	// Sums of borders.
	real_t hb_sum = border_left + border_right;
	real_t vb_sum = border_top + border_bottom;

	// Ratio of each side to the sum of itself and opposite side.
	// Since overflow only happens with opposite borders, you only need to get the ratio of each border relative to the sum of involved borders.
	real_t ratios[4] = {
		// Prevent divide by 0 errors.
		hb_sum > 0 ? (border_left / hb_sum) : 0,
		vb_sum > 0 ? (border_top / vb_sum) : 0,
		hb_sum > 0 ? (border_right / hb_sum) : 0,
		vb_sum > 0 ? (border_bottom / vb_sum) : 0
	};

	// Raw amount each corner should shrink.
	Point2 corner_reduction[4] = {
		Point2(edge_overflow[SIDE_TOP] * ratios[SIDE_LEFT], edge_overflow[SIDE_LEFT] * ratios[SIDE_TOP]),
		Point2(edge_overflow[SIDE_TOP] * ratios[SIDE_RIGHT], edge_overflow[SIDE_RIGHT] * ratios[SIDE_TOP]),
		Point2(edge_overflow[SIDE_BOTTOM] * ratios[SIDE_RIGHT], edge_overflow[SIDE_RIGHT] * ratios[SIDE_BOTTOM]),
		Point2(edge_overflow[SIDE_BOTTOM] * ratios[SIDE_LEFT], edge_overflow[SIDE_LEFT] * ratios[SIDE_BOTTOM]),
	};

	// Corner Radii as Point2s.
	Point2 pcr[4] = {
		Point2(corner_radius[0], corner_radius[0]),
		Point2(corner_radius[1], corner_radius[1]),
		Point2(corner_radius[2], corner_radius[2]),
		Point2(corner_radius[3], corner_radius[3]),
	};

	// If corner radii are too small, they won't shrink the full amount.
	// Adjacent corners will have to shrink the leftovers if they can.
	// Minf(0) is to ignore non-leftovers, and negating is to make values positive.
	Point2 leftovers[4] = {
		-((pcr[0] - corner_reduction[0]).minf(0)),
		-((pcr[1] - corner_reduction[1]).minf(0)),
		-((pcr[2] - corner_reduction[2]).minf(0)),
		-((pcr[3] - corner_reduction[3]).minf(0)),
	};

	// New shrunken radii after distributing the leftovers.
	Point2 distributed[4] = {
		((pcr[0] - corner_reduction[0] - leftovers[3] - leftovers[1]).maxf(0)),
		((pcr[1] - corner_reduction[1] - leftovers[0] - leftovers[2]).maxf(0)),
		((pcr[2] - corner_reduction[2] - leftovers[1] - leftovers[3]).maxf(0)),
		((pcr[3] - corner_reduction[3] - leftovers[2] - leftovers[0]).maxf(0)),
	};

	// How much the curve should scale to achieve the shrunken radii.
	for (int i = 0; i < 4; i++) {
		// Unshrinkable is how much is still left over, even after distributing leftovers.
		// Exclude it from the final scale.
		Point2 unshrinkable = (leftovers[(i + 1) % 4] + leftovers[(i + 4 - 1) % 4] - distributed[i]).maxf(0);
		inner_scale[i] = distributed[i] / (pcr[i] - unshrinkable).maxf(FLT_EPSILON);
	}
}

inline void draw_rounded_rectangle(Vector<Vector2> &verts, Vector<int> &indices, Vector<Color> &colors, const Rect2 &style_rect, const real_t corner_radius[4],
		const Rect2 &ring_rect, const Rect2 &inner_rect, const Color &inner_color, const Color &outer_color, const int corner_detail, const Vector2 &skew, bool is_filled = false) {
	int vert_offset = verts.size();
	int adapted_corner_detail = (corner_radius[0] > 0) || (corner_radius[1] > 0) || (corner_radius[2] > 0) || (corner_radius[3] > 0) ? corner_detail : 1;

	bool draw_border = !is_filled;

	real_t ring_corner_radius[4];
	set_inner_corner_radius(style_rect, ring_rect, corner_radius, ring_corner_radius);

	Point2 ring_scale[4];
	set_corner_scale(style_rect, ring_rect, ring_corner_radius, ring_scale);

	// Corner radius center points.
	Vector<Point2> outer_points = {
		ring_rect.position + Vector2(ring_corner_radius[0], ring_corner_radius[0]) * ring_scale[0], //tl
		Point2(ring_rect.position.x + ring_rect.size.x - ring_corner_radius[1] * ring_scale[1].x, ring_rect.position.y + ring_corner_radius[1] * ring_scale[1].y), //tr
		ring_rect.position + ring_rect.size - Vector2(ring_corner_radius[2], ring_corner_radius[2]) * ring_scale[2], //br
		Point2(ring_rect.position.x + ring_corner_radius[3] * ring_scale[3].x, ring_rect.position.y + ring_rect.size.y - ring_corner_radius[3] * ring_scale[3].y) //bl
	};

	real_t inner_corner_radius[4];
	set_inner_corner_radius(style_rect, inner_rect, corner_radius, inner_corner_radius);

	Point2 inner_scale[4];
	set_corner_scale(style_rect, inner_rect, inner_corner_radius, inner_scale);

	Vector<Point2> inner_points = {
		inner_rect.position + Vector2(inner_corner_radius[0], inner_corner_radius[0]) * inner_scale[0], //tl
		Point2(inner_rect.position.x + inner_rect.size.x - inner_corner_radius[1] * inner_scale[1].x, inner_rect.position.y + inner_corner_radius[1] * inner_scale[1].y), //tr
		inner_rect.position + inner_rect.size - Vector2(inner_corner_radius[2], inner_corner_radius[2]) * inner_scale[2], //br
		Point2(inner_rect.position.x + inner_corner_radius[3] * inner_scale[3].x, inner_rect.position.y + inner_rect.size.y - inner_corner_radius[3] * inner_scale[3].y) //bl
	};

	// Calculate the vertices.

	// If the center is filled, we do not draw the border and directly use the inner ring as reference. Because all calls to this
	// method either draw a ring or a filled rounded rectangle, but not both.
	const real_t quarter_arc_rad = Math::PI / 2.0;
	const Point2 style_rect_center = style_rect.get_center();

	const int colors_size = colors.size();
	const int verts_size = verts.size();
	const int new_verts_amount = (adapted_corner_detail + 1) * (draw_border ? 8 : 4);

	colors.resize(colors_size + new_verts_amount);
	verts.resize(verts_size + new_verts_amount);
	Color *colors_ptr = colors.ptrw();
	Vector2 *verts_ptr = verts.ptrw();

	for (int corner_idx = 0; corner_idx < 4; corner_idx++) {
		for (int detail = 0; detail <= adapted_corner_detail; detail++) {
			int idx_ofs = (adapted_corner_detail + 1) * corner_idx + detail;
			if (draw_border) {
				idx_ofs *= 2;
			}

			const real_t pt_angle = (corner_idx + detail / (double)adapted_corner_detail) * quarter_arc_rad + Math::PI;
			const real_t angle_cosine = Math::cos(pt_angle);
			const real_t angle_sine = Math::sin(pt_angle);

			{
				const real_t x = inner_corner_radius[corner_idx] * angle_cosine * inner_scale[corner_idx].x + inner_points[corner_idx].x;
				const real_t y = inner_corner_radius[corner_idx] * angle_sine * inner_scale[corner_idx].y + inner_points[corner_idx].y;
				const float x_skew = -skew.x * (y - style_rect_center.y);
				const float y_skew = -skew.y * (x - style_rect_center.x);
				verts_ptr[verts_size + idx_ofs] = Vector2(x + x_skew, y + y_skew);
				colors_ptr[colors_size + idx_ofs] = inner_color;
			}

			if (draw_border) {
				const real_t x = ring_corner_radius[corner_idx] * angle_cosine * ring_scale[corner_idx].x + outer_points[corner_idx].x;
				const real_t y = ring_corner_radius[corner_idx] * angle_sine * ring_scale[corner_idx].y + outer_points[corner_idx].y;
				const float x_skew = -skew.x * (y - style_rect_center.y);
				const float y_skew = -skew.y * (x - style_rect_center.x);
				verts_ptr[verts_size + idx_ofs + 1] = Vector2(x + x_skew, y + y_skew);
				colors_ptr[colors_size + idx_ofs + 1] = outer_color;
			}
		}
	}

	int ring_vert_count = verts.size() - vert_offset;

	// Fill the indices and the colors for the border.

	if (draw_border) {
		int indices_size = indices.size();
		indices.resize(indices_size + ring_vert_count * 3);
		int *indices_ptr = indices.ptrw();

		for (int i = 0; i < ring_vert_count; i++) {
			int idx_ofs = indices_size + i * 3;
			indices_ptr[idx_ofs] = vert_offset + i % ring_vert_count;
			indices_ptr[idx_ofs + 1] = vert_offset + (i + 2) % ring_vert_count;
			indices_ptr[idx_ofs + 2] = vert_offset + (i + 1) % ring_vert_count;
		}
	}

	if (is_filled) {
		// Compute the triangles pattern to draw the rounded rectangle.
		// Consists of vertical stripes of two triangles each.

		int stripes_count = ring_vert_count / 2 - 1;
		int last_vert_id = ring_vert_count - 1;

		int indices_size = indices.size();
		indices.resize(indices_size + stripes_count * 6);
		int *indices_ptr = indices.ptrw();

		for (int i = 0; i < stripes_count; i++) {
			int idx_ofs = indices_size + i * 6;
			// Polygon 1.
			indices_ptr[idx_ofs] = vert_offset + i;
			indices_ptr[idx_ofs + 1] = vert_offset + last_vert_id - i - 1;
			indices_ptr[idx_ofs + 2] = vert_offset + i + 1;
			// Polygon 2.
			indices_ptr[idx_ofs + 3] = vert_offset + i;
			indices_ptr[idx_ofs + 4] = vert_offset + last_vert_id - i;
			indices_ptr[idx_ofs + 5] = vert_offset + last_vert_id - i - 1;
		}
	}
}

inline void adapt_values(int p_index_a, int p_index_b, real_t *adapted_values, const real_t *p_values, const real_t p_width, const real_t p_max_a, const real_t p_max_b) {
	real_t value_a = p_values[p_index_a];
	real_t value_b = p_values[p_index_b];
	real_t factor = MIN(1.0, p_width / (value_a + value_b));
	adapted_values[p_index_a] = MIN(MIN(value_a * factor, p_max_a), adapted_values[p_index_a]);
	adapted_values[p_index_b] = MIN(MIN(value_b * factor, p_max_b), adapted_values[p_index_b]);
}

Rect2 StyleBoxFlat::get_draw_rect(const Rect2 &p_rect) const {
	Rect2 draw_rect = p_rect.grow_individual(expand_margin[SIDE_LEFT], expand_margin[SIDE_TOP], expand_margin[SIDE_RIGHT], expand_margin[SIDE_BOTTOM]);

	if (shadow_size > 0) {
		Rect2 shadow_rect = draw_rect.grow(shadow_size);
		shadow_rect.position += shadow_offset;
		draw_rect = draw_rect.merge(shadow_rect);
	}

	return draw_rect;
}

void StyleBoxFlat::draw(RID p_canvas_item, const Rect2 &p_rect) const {
	bool draw_border = (border_width[0] > 0) || (border_width[1] > 0) || (border_width[2] > 0) || (border_width[3] > 0);
	bool draw_shadow = (shadow_size > 0);
	if (!draw_border && !draw_center && !draw_shadow) {
		return;
	}

	Rect2 style_rect = p_rect.grow_individual(expand_margin[SIDE_LEFT], expand_margin[SIDE_TOP], expand_margin[SIDE_RIGHT], expand_margin[SIDE_BOTTOM]);
	if (Math::is_zero_approx(style_rect.size.width) || Math::is_zero_approx(style_rect.size.height)) {
		return;
	}

	const bool rounded_corners = (corner_radius[0] > 0) || (corner_radius[1] > 0) || (corner_radius[2] > 0) || (corner_radius[3] > 0);
	// Only enable antialiasing if it is actually needed. This improves performance
	// and maximizes sharpness for non-skewed StyleBoxes with sharp corners.
	const bool aa_on = (rounded_corners || !skew.is_zero_approx()) && anti_aliased;

	const bool blend_on = blend_border && draw_border;

	Color border_color_alpha = Color(border_color.r, border_color.g, border_color.b, 0);
	Color border_color_blend = (draw_center ? bg_color : border_color_alpha);
	Color border_color_inner = blend_on ? border_color_blend : border_color;

	// Adapt borders (prevent weird overlapping/glitchy drawings).
	real_t width = MAX(style_rect.size.width, 0);
	real_t height = MAX(style_rect.size.height, 0);
	real_t adapted_border[4] = { 1000000.0, 1000000.0, 1000000.0, 1000000.0 };
	adapt_values(SIDE_TOP, SIDE_BOTTOM, adapted_border, border_width, height, height, height);
	adapt_values(SIDE_LEFT, SIDE_RIGHT, adapted_border, border_width, width, width, width);

	// Adapt corners (prevent weird overlapping/glitchy drawings).
	real_t adapted_corner[4] = { 1000000.0, 1000000.0, 1000000.0, 1000000.0 };
	adapt_values(CORNER_TOP_RIGHT, CORNER_BOTTOM_RIGHT, adapted_corner, corner_radius, height, height - adapted_border[SIDE_BOTTOM], height - adapted_border[SIDE_TOP]);
	adapt_values(CORNER_TOP_LEFT, CORNER_BOTTOM_LEFT, adapted_corner, corner_radius, height, height - adapted_border[SIDE_BOTTOM], height - adapted_border[SIDE_TOP]);
	adapt_values(CORNER_TOP_LEFT, CORNER_TOP_RIGHT, adapted_corner, corner_radius, width, width - adapted_border[SIDE_RIGHT], width - adapted_border[SIDE_LEFT]);
	adapt_values(CORNER_BOTTOM_LEFT, CORNER_BOTTOM_RIGHT, adapted_corner, corner_radius, width, width - adapted_border[SIDE_RIGHT], width - adapted_border[SIDE_LEFT]);

	Rect2 infill_rect = style_rect.grow_individual(-adapted_border[SIDE_LEFT], -adapted_border[SIDE_TOP], -adapted_border[SIDE_RIGHT], -adapted_border[SIDE_BOTTOM]);

	Rect2 border_style_rect = style_rect;

	real_t aa_size_scaled = 1.0f;
	if (aa_on) {
		real_t scale_factor = 1.0f;
		const SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (tree) {
			const Window *window = tree->get_root();
			const Vector2 stretch_scale = window->get_stretch_transform().get_scale();
			scale_factor = MIN(stretch_scale.x, stretch_scale.y);
		}

		// Adjust AA feather size to account for the 2D scale factor, so that
		// antialiasing doesn't become blurry at viewport resolutions higher
		// than the default when using the `canvas_items` stretch mode
		// (or when using `content_scale_factor` values different than `1.0`).
		aa_size_scaled = aa_size / scale_factor;
	}

	if (aa_on) {
		for (int i = 0; i < 4; i++) {
			if (border_width[i] > 0) {
				border_style_rect = border_style_rect.grow_side((Side)i, -aa_size_scaled);
			}
		}
	}

	Vector<Point2> verts;
	Vector<int> indices;
	Vector<Color> colors;
	Vector<Point2> uvs;

	// Create shadow.
	if (draw_shadow) {
		Rect2 shadow_inner_rect = style_rect;
		shadow_inner_rect.position += shadow_offset;

		Rect2 shadow_rect = style_rect.grow(shadow_size);
		shadow_rect.position += shadow_offset;

		Color shadow_color_transparent = Color(shadow_color.r, shadow_color.g, shadow_color.b, 0);

		draw_rounded_rectangle(verts, indices, colors, shadow_inner_rect, adapted_corner,
				shadow_rect, shadow_inner_rect, shadow_color, shadow_color_transparent, corner_detail, skew);

		if (draw_center) {
			draw_rounded_rectangle(verts, indices, colors, shadow_inner_rect, adapted_corner,
					shadow_inner_rect, shadow_inner_rect, shadow_color, shadow_color, corner_detail, skew, true);
		}
	}

	// Create border (no AA).
	if (draw_border && !aa_on) {
		draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
				border_style_rect, infill_rect, border_color_inner, border_color, corner_detail, skew);
	}

	// Create infill (no AA).
	if (draw_center && (!aa_on || blend_on)) {
		draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
				infill_rect, infill_rect, bg_color, bg_color, corner_detail, skew, true);
	}

	if (aa_on) {
		real_t aa_border_width[4];
		real_t aa_border_width_half[4];
		real_t aa_fill_width[4];
		real_t aa_fill_width_half[4];

		if (draw_border) {
			for (int i = 0; i < 4; i++) {
				if (border_width[i] > 0) {
					aa_border_width[i] = aa_size_scaled;
					aa_border_width_half[i] = aa_size_scaled * 0.5;
					aa_fill_width[i] = 0;
					aa_fill_width_half[i] = 0;
				} else {
					aa_border_width[i] = 0;
					aa_border_width_half[i] = 0;
					aa_fill_width[i] = aa_size_scaled;
					aa_fill_width_half[i] = aa_size_scaled * 0.5;
				}
			}
		} else {
			for (int i = 0; i < 4; i++) {
				aa_border_width[i] = 0;
				aa_border_width_half[i] = 0;
				aa_fill_width[i] = aa_size_scaled;
				aa_fill_width_half[i] = aa_size_scaled * 0.5;
			}
		}

		if (draw_center) {
			// Infill rect, transparent side of antialiasing gradient (base infill rect enlarged by AA size)
			Rect2 infill_rect_aa_transparent = infill_rect.grow_individual(aa_fill_width_half[SIDE_LEFT], aa_fill_width_half[SIDE_TOP],
					aa_fill_width_half[SIDE_RIGHT], aa_fill_width_half[SIDE_BOTTOM]);
			// Infill rect, colored side of antialiasing gradient (base infill rect shrunk by AA size)
			Rect2 infill_rect_aa_colored = infill_rect_aa_transparent.grow_individual(-aa_fill_width[SIDE_LEFT], -aa_fill_width[SIDE_TOP],
					-aa_fill_width[SIDE_RIGHT], -aa_fill_width[SIDE_BOTTOM]);
			if (!blend_on) {
				// Create center fill, not antialiased yet
				draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
						infill_rect_aa_colored, infill_rect_aa_colored, bg_color, bg_color, corner_detail, skew, true);
			}
			if (!blend_on || !draw_border) {
				Color alpha_bg = Color(bg_color.r, bg_color.g, bg_color.b, 0);
				// Add antialiasing on the center fill
				draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
						infill_rect_aa_transparent, infill_rect_aa_colored, bg_color, alpha_bg, corner_detail, skew);
			}
		}

		if (draw_border) {
			// Inner border recct, fully colored side of antialiasing gradient (base inner rect enlarged by AA size)
			Rect2 inner_rect_aa_colored = infill_rect.grow_individual(aa_border_width_half[SIDE_LEFT], aa_border_width_half[SIDE_TOP],
					aa_border_width_half[SIDE_RIGHT], aa_border_width_half[SIDE_BOTTOM]);
			// Inner border rect, transparent side of antialiasing gradient (base inner rect shrunk by AA size)
			Rect2 inner_rect_aa_transparent = inner_rect_aa_colored.grow_individual(-aa_border_width[SIDE_LEFT], -aa_border_width[SIDE_TOP],
					-aa_border_width[SIDE_RIGHT], -aa_border_width[SIDE_BOTTOM]);
			// Outer border rect, transparent side of antialiasing gradient (base outer rect enlarged by AA size)
			Rect2 outer_rect_aa_transparent = style_rect.grow_individual(aa_border_width_half[SIDE_LEFT], aa_border_width_half[SIDE_TOP],
					aa_border_width_half[SIDE_RIGHT], aa_border_width_half[SIDE_BOTTOM]);
			// Outer border rect, colored side of antialiasing gradient (base outer rect shrunk by AA size)
			Rect2 outer_rect_aa_colored = border_style_rect.grow_individual(aa_border_width_half[SIDE_LEFT], aa_border_width_half[SIDE_TOP],
					aa_border_width_half[SIDE_RIGHT], aa_border_width_half[SIDE_BOTTOM]);

			// Create border ring, not antialiased yet
			draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
					outer_rect_aa_colored, ((blend_on) ? infill_rect : inner_rect_aa_colored), border_color_inner, border_color, corner_detail, skew);
			if (!blend_on) {
				// Add antialiasing on the ring inner border
				draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
						inner_rect_aa_colored, inner_rect_aa_transparent, border_color_blend, border_color, corner_detail, skew);
			}
			// Add antialiasing on the ring outer border
			draw_rounded_rectangle(verts, indices, colors, border_style_rect, adapted_corner,
					outer_rect_aa_transparent, outer_rect_aa_colored, border_color, border_color_alpha, corner_detail, skew);
		}
	}

	// Compute UV coordinates.
	Rect2 uv_rect = style_rect.grow(aa_on ? aa_size_scaled : 0);
	uvs.resize(verts.size());
	Point2 *uvs_ptr = uvs.ptrw();
	for (int i = 0; i < verts.size(); i++) {
		uvs_ptr[i].x = (verts[i].x - uv_rect.position.x) / uv_rect.size.width;
		uvs_ptr[i].y = (verts[i].y - uv_rect.position.y) / uv_rect.size.height;
	}

	// Draw stylebox.
	RenderingServer *vs = RenderingServer::get_singleton();
	vs->canvas_item_add_triangle_array(p_canvas_item, indices, verts, colors, uvs);
}

void StyleBoxFlat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_bg_color", "color"), &StyleBoxFlat::set_bg_color);
	ClassDB::bind_method(D_METHOD("get_bg_color"), &StyleBoxFlat::get_bg_color);

	ClassDB::bind_method(D_METHOD("set_border_color", "color"), &StyleBoxFlat::set_border_color);
	ClassDB::bind_method(D_METHOD("get_border_color"), &StyleBoxFlat::get_border_color);

	ClassDB::bind_method(D_METHOD("set_border_width_all", "width"), &StyleBoxFlat::set_border_width_all);
	ClassDB::bind_method(D_METHOD("get_border_width_min"), &StyleBoxFlat::get_border_width_min);

	ClassDB::bind_method(D_METHOD("set_border_width", "margin", "width"), &StyleBoxFlat::set_border_width);
	ClassDB::bind_method(D_METHOD("get_border_width", "margin"), &StyleBoxFlat::get_border_width);

	ClassDB::bind_method(D_METHOD("set_border_blend", "blend"), &StyleBoxFlat::set_border_blend);
	ClassDB::bind_method(D_METHOD("get_border_blend"), &StyleBoxFlat::get_border_blend);

	ClassDB::bind_method(D_METHOD("set_corner_radius_all", "radius"), &StyleBoxFlat::set_corner_radius_all);

	ClassDB::bind_method(D_METHOD("set_corner_radius", "corner", "radius"), &StyleBoxFlat::set_corner_radius);
	ClassDB::bind_method(D_METHOD("get_corner_radius", "corner"), &StyleBoxFlat::get_corner_radius);

	ClassDB::bind_method(D_METHOD("set_expand_margin", "margin", "size"), &StyleBoxFlat::set_expand_margin);
	ClassDB::bind_method(D_METHOD("set_expand_margin_all", "size"), &StyleBoxFlat::set_expand_margin_all);
	ClassDB::bind_method(D_METHOD("get_expand_margin", "margin"), &StyleBoxFlat::get_expand_margin);

	ClassDB::bind_method(D_METHOD("set_draw_center", "draw_center"), &StyleBoxFlat::set_draw_center);
	ClassDB::bind_method(D_METHOD("is_draw_center_enabled"), &StyleBoxFlat::is_draw_center_enabled);

	ClassDB::bind_method(D_METHOD("set_skew", "skew"), &StyleBoxFlat::set_skew);
	ClassDB::bind_method(D_METHOD("get_skew"), &StyleBoxFlat::get_skew);

	ClassDB::bind_method(D_METHOD("set_shadow_color", "color"), &StyleBoxFlat::set_shadow_color);
	ClassDB::bind_method(D_METHOD("get_shadow_color"), &StyleBoxFlat::get_shadow_color);

	ClassDB::bind_method(D_METHOD("set_shadow_size", "size"), &StyleBoxFlat::set_shadow_size);
	ClassDB::bind_method(D_METHOD("get_shadow_size"), &StyleBoxFlat::get_shadow_size);

	ClassDB::bind_method(D_METHOD("set_shadow_offset", "offset"), &StyleBoxFlat::set_shadow_offset);
	ClassDB::bind_method(D_METHOD("get_shadow_offset"), &StyleBoxFlat::get_shadow_offset);

	ClassDB::bind_method(D_METHOD("set_anti_aliased", "anti_aliased"), &StyleBoxFlat::set_anti_aliased);
	ClassDB::bind_method(D_METHOD("is_anti_aliased"), &StyleBoxFlat::is_anti_aliased);

	ClassDB::bind_method(D_METHOD("set_aa_size", "size"), &StyleBoxFlat::set_aa_size);
	ClassDB::bind_method(D_METHOD("get_aa_size"), &StyleBoxFlat::get_aa_size);

	ClassDB::bind_method(D_METHOD("set_corner_detail", "detail"), &StyleBoxFlat::set_corner_detail);
	ClassDB::bind_method(D_METHOD("get_corner_detail"), &StyleBoxFlat::get_corner_detail);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "bg_color"), "set_bg_color", "get_bg_color");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draw_center"), "set_draw_center", "is_draw_center_enabled");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "skew"), "set_skew", "get_skew");

	ADD_GROUP("Border Width", "border_width_");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "border_width_left", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_border_width", "get_border_width", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "border_width_top", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_border_width", "get_border_width", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "border_width_right", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_border_width", "get_border_width", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "border_width_bottom", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_border_width", "get_border_width", SIDE_BOTTOM);

	ADD_GROUP("Border", "border_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "border_color"), "set_border_color", "get_border_color");

	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "border_blend"), "set_border_blend", "get_border_blend");

	ADD_GROUP("Corner Radius", "corner_radius_");
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "corner_radius_top_left", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_corner_radius", "get_corner_radius", CORNER_TOP_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "corner_radius_top_right", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_corner_radius", "get_corner_radius", CORNER_TOP_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "corner_radius_bottom_right", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_corner_radius", "get_corner_radius", CORNER_BOTTOM_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::INT, "corner_radius_bottom_left", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_corner_radius", "get_corner_radius", CORNER_BOTTOM_LEFT);

	ADD_PROPERTY(PropertyInfo(Variant::INT, "corner_detail", PROPERTY_HINT_RANGE, "1,20,1"), "set_corner_detail", "get_corner_detail");

	ADD_GROUP("Expand Margins", "expand_margin_");
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_left", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_LEFT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_top", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_TOP);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_right", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_RIGHT);
	ADD_PROPERTYI(PropertyInfo(Variant::FLOAT, "expand_margin_bottom", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_expand_margin", "get_expand_margin", SIDE_BOTTOM);

	ADD_GROUP("Shadow", "shadow_");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "shadow_color"), "set_shadow_color", "get_shadow_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "shadow_size", PROPERTY_HINT_RANGE, "0,100,1,or_greater,suffix:px"), "set_shadow_size", "get_shadow_size");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "shadow_offset", PROPERTY_HINT_NONE, "suffix:px"), "set_shadow_offset", "get_shadow_offset");

	ADD_GROUP("Anti Aliasing", "anti_aliasing_");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "anti_aliasing"), "set_anti_aliased", "is_anti_aliased");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "anti_aliasing_size", PROPERTY_HINT_RANGE, "0.01,10,0.001,suffix:px"), "set_aa_size", "get_aa_size");
}
