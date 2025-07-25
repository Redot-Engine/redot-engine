/**************************************************************************/
/*  collision_shape_3d_gizmo_plugin.cpp                                   */
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

#include "collision_shape_3d_gizmo_plugin.h"

#include "core/math/convex_hull.h"
#include "core/math/geometry_3d.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/scene/3d/gizmos/gizmo_3d_helper.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/convex_polygon_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/height_map_shape_3d.h"
#include "scene/resources/3d/separation_ray_shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/3d/world_boundary_shape_3d.h"

CollisionShape3DGizmoPlugin::CollisionShape3DGizmoPlugin() {
	helper.instantiate();

	create_collision_material("shape_material", 2.0);
	create_collision_material("shape_material_arraymesh", 0.0625);

	create_collision_material("shape_material_disabled", 0.0625);
	create_collision_material("shape_material_arraymesh_disabled", 0.015625);

	create_handle_material("handles");
}

void CollisionShape3DGizmoPlugin::create_collision_material(const String &p_name, float p_alpha) {
	Vector<Ref<StandardMaterial3D>> mats;

	const Color collision_color(1.0, 1.0, 1.0, p_alpha);

	for (int i = 0; i < 4; i++) {
		bool instantiated = i < 2;

		Ref<StandardMaterial3D> material = memnew(StandardMaterial3D);

		Color color = collision_color;
		color.a *= instantiated ? 0.25 : 1.0;

		material->set_albedo(color);
		material->set_shading_mode(StandardMaterial3D::SHADING_MODE_UNSHADED);
		material->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
		material->set_render_priority(StandardMaterial3D::RENDER_PRIORITY_MIN + 1);
		material->set_cull_mode(StandardMaterial3D::CULL_BACK);
		material->set_flag(StandardMaterial3D::FLAG_DISABLE_FOG, true);
		material->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
		material->set_flag(StandardMaterial3D::FLAG_SRGB_VERTEX_COLOR, true);

		mats.push_back(material);
	}

	materials[p_name] = mats;
}

bool CollisionShape3DGizmoPlugin::has_gizmo(Node3D *p_spatial) {
	return Object::cast_to<CollisionShape3D>(p_spatial) != nullptr;
}

String CollisionShape3DGizmoPlugin::get_gizmo_name() const {
	return "CollisionShape3D";
}

int CollisionShape3DGizmoPlugin::get_priority() const {
	return -1;
}

String CollisionShape3DGizmoPlugin::get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	const CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_gizmo->get_node_3d());

	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		return "";
	}

	if (Object::cast_to<SphereShape3D>(*s)) {
		return "Radius";
	}

	if (Object::cast_to<BoxShape3D>(*s)) {
		return helper->box_get_handle_name(p_id);
	}

	if (Object::cast_to<CapsuleShape3D>(*s)) {
		return helper->capsule_get_handle_name(p_id);
	}

	if (Object::cast_to<CylinderShape3D>(*s)) {
		return helper->cylinder_get_handle_name(p_id);
	}

	if (Object::cast_to<SeparationRayShape3D>(*s)) {
		return "Length";
	}

	return "";
}

Variant CollisionShape3DGizmoPlugin::get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const {
	CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_gizmo->get_node_3d());

	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		return Variant();
	}

	if (Object::cast_to<SphereShape3D>(*s)) {
		Ref<SphereShape3D> ss = s;
		return ss->get_radius();
	}

	if (Object::cast_to<BoxShape3D>(*s)) {
		Ref<BoxShape3D> bs = s;
		return bs->get_size();
	}

	if (Object::cast_to<CapsuleShape3D>(*s)) {
		Ref<CapsuleShape3D> cs2 = s;
		return Vector2(cs2->get_radius(), cs2->get_height());
	}

	if (Object::cast_to<CylinderShape3D>(*s)) {
		Ref<CylinderShape3D> cs2 = s;
		return Vector2(cs2->get_radius(), cs2->get_height());
	}

	if (Object::cast_to<SeparationRayShape3D>(*s)) {
		Ref<SeparationRayShape3D> cs2 = s;
		return cs2->get_length();
	}

	return Variant();
}

void CollisionShape3DGizmoPlugin::begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) {
	helper->initialize_handle_action(get_handle_value(p_gizmo, p_id, p_secondary), p_gizmo->get_node_3d()->get_global_transform());
}

void CollisionShape3DGizmoPlugin::set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) {
	CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_gizmo->get_node_3d());

	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		return;
	}

	Vector3 sg[2];
	helper->get_segment(p_camera, p_point, sg);

	if (Object::cast_to<SphereShape3D>(*s)) {
		Ref<SphereShape3D> ss = s;
		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(), Vector3(4096, 0, 0), sg[0], sg[1], ra, rb);
		float d = ra.x;
		if (Node3DEditor::get_singleton()->is_snap_enabled()) {
			d = Math::snapped(d, Node3DEditor::get_singleton()->get_translate_snap());
		}

		if (d < 0.001) {
			d = 0.001;
		}

		ss->set_radius(d);
	}

	if (Object::cast_to<SeparationRayShape3D>(*s)) {
		Ref<SeparationRayShape3D> rs = s;
		Vector3 ra, rb;
		Geometry3D::get_closest_points_between_segments(Vector3(), Vector3(0, 0, 4096), sg[0], sg[1], ra, rb);
		float d = ra.z;
		if (Node3DEditor::get_singleton()->is_snap_enabled()) {
			d = Math::snapped(d, Node3DEditor::get_singleton()->get_translate_snap());
		}

		if (d < 0.001) {
			d = 0.001;
		}

		rs->set_length(d);
	}

	if (Object::cast_to<BoxShape3D>(*s)) {
		Ref<BoxShape3D> bs = s;
		Vector3 size = bs->get_size();
		Vector3 position;
		helper->box_set_handle(sg, p_id, size, position);
		bs->set_size(size);
		cs->set_global_position(position);
	}

	if (Object::cast_to<CapsuleShape3D>(*s)) {
		Ref<CapsuleShape3D> cs2 = s;

		real_t height = cs2->get_height();
		real_t radius = cs2->get_radius();
		Vector3 position;
		helper->capsule_set_handle(sg, p_id, height, radius, position);
		cs2->set_height(height);
		cs2->set_radius(radius);
		cs->set_global_position(position);
	}

	if (Object::cast_to<CylinderShape3D>(*s)) {
		Ref<CylinderShape3D> cs2 = s;

		real_t height = cs2->get_height();
		real_t radius = cs2->get_radius();
		Vector3 position;
		helper->cylinder_set_handle(sg, p_id, height, radius, position);
		cs2->set_height(height);
		cs2->set_radius(radius);
		cs->set_global_position(position);
	}
}

void CollisionShape3DGizmoPlugin::commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) {
	CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_gizmo->get_node_3d());

	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		return;
	}

	if (Object::cast_to<SphereShape3D>(*s)) {
		Ref<SphereShape3D> ss = s;
		if (p_cancel) {
			ss->set_radius(p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Sphere Shape Radius"));
		ur->add_do_method(ss.ptr(), "set_radius", ss->get_radius());
		ur->add_undo_method(ss.ptr(), "set_radius", p_restore);
		ur->commit_action();
	}

	if (Object::cast_to<BoxShape3D>(*s)) {
		helper->box_commit_handle(TTR("Change Box Shape Size"), p_cancel, cs, s.ptr());
	}

	if (Object::cast_to<CapsuleShape3D>(*s)) {
		Ref<CapsuleShape3D> ss = s;
		helper->cylinder_commit_handle(p_id, TTR("Change Capsule Shape Radius"), TTR("Change Capsule Shape Height"), p_cancel, cs, *ss, *ss);
	}

	if (Object::cast_to<CylinderShape3D>(*s)) {
		Ref<CylinderShape3D> ss = s;
		helper->cylinder_commit_handle(p_id, TTR("Change Cylinder Shape Radius"), TTR("Change Cylinder Shape Height"), p_cancel, cs, *ss, *ss);
	}

	if (Object::cast_to<SeparationRayShape3D>(*s)) {
		Ref<SeparationRayShape3D> ss = s;
		if (p_cancel) {
			ss->set_length(p_restore);
			return;
		}

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Change Separation Ray Shape Length"));
		ur->add_do_method(ss.ptr(), "set_length", ss->get_length());
		ur->add_undo_method(ss.ptr(), "set_length", p_restore);
		ur->commit_action();
	}
}

void CollisionShape3DGizmoPlugin::redraw(EditorNode3DGizmo *p_gizmo) {
	CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_gizmo->get_node_3d());

	p_gizmo->clear();

	Ref<Shape3D> s = cs->get_shape();
	if (s.is_null()) {
		return;
	}

	const Ref<StandardMaterial3D> material =
			get_material(!cs->is_disabled() ? "shape_material" : "shape_material_disabled", p_gizmo);
	const Ref<StandardMaterial3D> material_arraymesh =
			get_material(!cs->is_disabled() ? "shape_material_arraymesh" : "shape_material_arraymesh_disabled", p_gizmo);
	const Ref<Material> handles_material = get_material("handles");

	const Color collision_color = cs->is_disabled() ? Color(1.0, 1.0, 1.0, 0.75) : cs->get_debug_color();

	if (cs->get_debug_fill_enabled()) {
		Ref<ArrayMesh> array_mesh = s->get_debug_arraymesh_faces(collision_color);
		if (array_mesh.is_valid() && array_mesh->get_surface_count() > 0) {
			p_gizmo->add_mesh(array_mesh, material_arraymesh);
		}
	}

	if (Object::cast_to<SphereShape3D>(*s)) {
		Ref<SphereShape3D> sp = s;
		float radius = sp->get_radius();

#define PUSH_QUARTER(from_x, from_y, to_x, to_y, y)      \
	points_ptrw[index++] = Vector3(from_x, y, from_y);   \
	points_ptrw[index++] = Vector3(to_x, y, to_y);       \
	points_ptrw[index++] = Vector3(from_x, y, -from_y);  \
	points_ptrw[index++] = Vector3(to_x, y, -to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, from_y);  \
	points_ptrw[index++] = Vector3(-to_x, y, to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, -from_y); \
	points_ptrw[index++] = Vector3(-to_x, y, -to_y);

#define PUSH_QUARTER_XY(from_x, from_y, to_x, to_y, y)       \
	points_ptrw[index++] = Vector3(from_x, -from_y - y, 0);  \
	points_ptrw[index++] = Vector3(to_x, -to_y - y, 0);      \
	points_ptrw[index++] = Vector3(from_x, from_y + y, 0);   \
	points_ptrw[index++] = Vector3(to_x, to_y + y, 0);       \
	points_ptrw[index++] = Vector3(-from_x, -from_y - y, 0); \
	points_ptrw[index++] = Vector3(-to_x, -to_y - y, 0);     \
	points_ptrw[index++] = Vector3(-from_x, from_y + y, 0);  \
	points_ptrw[index++] = Vector3(-to_x, to_y + y, 0);

#define PUSH_QUARTER_YZ(from_x, from_y, to_x, to_y, y)       \
	points_ptrw[index++] = Vector3(0, -from_y - y, from_x);  \
	points_ptrw[index++] = Vector3(0, -to_y - y, to_x);      \
	points_ptrw[index++] = Vector3(0, from_y + y, from_x);   \
	points_ptrw[index++] = Vector3(0, to_y + y, to_x);       \
	points_ptrw[index++] = Vector3(0, -from_y - y, -from_x); \
	points_ptrw[index++] = Vector3(0, -to_y - y, -to_x);     \
	points_ptrw[index++] = Vector3(0, from_y + y, -from_x);  \
	points_ptrw[index++] = Vector3(0, to_y + y, -to_x);

		// Number of points in an octant. So there will be 8 * points_in_octant * 2 points in total for one circle.
		// This Corresponds to the smoothness of the circle.
		const uint32_t points_in_octant = 16;
		const real_t inc = (Math::PI / (4 * points_in_octant));
		const real_t radius_squared = radius * radius;
		real_t r = 0;

		Vector<Vector3> points;
		uint32_t index = 0;
		// 3 full circles.
		points.resize(3 * 8 * points_in_octant * 2);
		Vector3 *points_ptrw = points.ptrw();

		float previous_x = radius;
		float previous_y = 0.f;

		for (uint32_t i = 0; i < points_in_octant; ++i) {
			r += inc;
			real_t x = Math::cos(r) * radius;
			real_t y = Math::sqrt(radius_squared - (x * x));

			PUSH_QUARTER(previous_x, previous_y, x, y, 0);
			PUSH_QUARTER(previous_y, previous_x, y, x, 0);

			PUSH_QUARTER_XY(previous_x, previous_y, x, y, 0);
			PUSH_QUARTER_XY(previous_y, previous_x, y, x, 0);

			PUSH_QUARTER_YZ(previous_x, previous_y, x, y, 0);
			PUSH_QUARTER_YZ(previous_y, previous_x, y, x, 0)

			previous_x = x;
			previous_y = y;
		}
#undef PUSH_QUARTER
#undef PUSH_QUARTER_XY
#undef PUSH_QUARTER_YZ

		p_gizmo->add_lines(points, material, false, collision_color);
		p_gizmo->add_collision_segments(points);
		Vector<Vector3> handles;
		handles.push_back(Vector3(radius, 0, 0));
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<BoxShape3D>(*s)) {
		Ref<BoxShape3D> bs = s;
		Vector<Vector3> lines;
		AABB aabb;
		aabb.position = -bs->get_size() / 2;
		aabb.size = bs->get_size();

		for (int i = 0; i < 12; i++) {
			Vector3 a, b;
			aabb.get_edge(i, a, b);
			lines.push_back(a);
			lines.push_back(b);
		}

		const Vector<Vector3> handles = helper->box_get_handles(bs->get_size());

		p_gizmo->add_lines(lines, material, false, collision_color);
		p_gizmo->add_collision_segments(lines);
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<CapsuleShape3D>(*s)) {
		Ref<CapsuleShape3D> cs2 = s;
		float radius = cs2->get_radius();
		float height = cs2->get_height();

		// Number of points in an octant. So there will be 8 * points_in_octant points in total.
		// This corresponds to the smoothness of the circle.
		const uint32_t points_in_octant = 16;
		const real_t octant_angle = Math::PI / 4;
		const real_t inc = (Math::PI / (4 * points_in_octant));
		const real_t radius_squared = radius * radius;
		real_t r = 0;

		Vector<Vector3> points;
		// 4 vertical lines and 4 full circles.
		points.resize(4 * 2 + 4 * 8 * points_in_octant * 2);
		Vector3 *points_ptrw = points.ptrw();

		uint32_t index = 0;
		float y_value = height * 0.5 - radius;

		// Vertical Lines.
		points_ptrw[index++] = Vector3(0.f, y_value, radius);
		points_ptrw[index++] = Vector3(0.f, -y_value, radius);
		points_ptrw[index++] = Vector3(0.f, y_value, -radius);
		points_ptrw[index++] = Vector3(0.f, -y_value, -radius);
		points_ptrw[index++] = Vector3(radius, y_value, 0.f);
		points_ptrw[index++] = Vector3(radius, -y_value, 0.f);
		points_ptrw[index++] = Vector3(-radius, y_value, 0.f);
		points_ptrw[index++] = Vector3(-radius, -y_value, 0.f);

#define PUSH_QUARTER(from_x, from_y, to_x, to_y, y)      \
	points_ptrw[index++] = Vector3(from_x, y, from_y);   \
	points_ptrw[index++] = Vector3(to_x, y, to_y);       \
	points_ptrw[index++] = Vector3(from_x, y, -from_y);  \
	points_ptrw[index++] = Vector3(to_x, y, -to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, from_y);  \
	points_ptrw[index++] = Vector3(-to_x, y, to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, -from_y); \
	points_ptrw[index++] = Vector3(-to_x, y, -to_y);

#define PUSH_QUARTER_XY(from_x, from_y, to_x, to_y, y)       \
	points_ptrw[index++] = Vector3(from_x, -from_y - y, 0);  \
	points_ptrw[index++] = Vector3(to_x, -to_y - y, 0);      \
	points_ptrw[index++] = Vector3(from_x, from_y + y, 0);   \
	points_ptrw[index++] = Vector3(to_x, to_y + y, 0);       \
	points_ptrw[index++] = Vector3(-from_x, -from_y - y, 0); \
	points_ptrw[index++] = Vector3(-to_x, -to_y - y, 0);     \
	points_ptrw[index++] = Vector3(-from_x, from_y + y, 0);  \
	points_ptrw[index++] = Vector3(-to_x, to_y + y, 0);

#define PUSH_QUARTER_YZ(from_x, from_y, to_x, to_y, y)       \
	points_ptrw[index++] = Vector3(0, -from_y - y, from_x);  \
	points_ptrw[index++] = Vector3(0, -to_y - y, to_x);      \
	points_ptrw[index++] = Vector3(0, from_y + y, from_x);   \
	points_ptrw[index++] = Vector3(0, to_y + y, to_x);       \
	points_ptrw[index++] = Vector3(0, -from_y - y, -from_x); \
	points_ptrw[index++] = Vector3(0, -to_y - y, -to_x);     \
	points_ptrw[index++] = Vector3(0, from_y + y, -from_x);  \
	points_ptrw[index++] = Vector3(0, to_y + y, -to_x);

		float previous_x = radius;
		float previous_y = 0.f;

		for (uint32_t i = 0; i < points_in_octant; ++i) {
			r += inc;
			real_t x = Math::cos((i == points_in_octant - 1) ? octant_angle : r) * radius;
			real_t y = Math::sqrt(radius_squared - (x * x));

			// High circle ring.
			PUSH_QUARTER(previous_x, previous_y, x, y, y_value);
			PUSH_QUARTER(previous_y, previous_x, y, x, y_value);

			// Low circle ring.
			PUSH_QUARTER(previous_x, previous_y, x, y, -y_value);
			PUSH_QUARTER(previous_y, previous_x, y, x, -y_value);

			// Up and Low circle in X-Y plane.
			PUSH_QUARTER_XY(previous_x, previous_y, x, y, y_value);
			PUSH_QUARTER_XY(previous_y, previous_x, y, x, y_value);

			// Up and Low circle in Y-Z plane.
			PUSH_QUARTER_YZ(previous_x, previous_y, x, y, y_value);
			PUSH_QUARTER_YZ(previous_y, previous_x, y, x, y_value)

			previous_x = x;
			previous_y = y;
		}

#undef PUSH_QUARTER
#undef PUSH_QUARTER_XY
#undef PUSH_QUARTER_YZ

		p_gizmo->add_lines(points, material, false, collision_color);
		p_gizmo->add_collision_segments(points);

		Vector<Vector3> handles = helper->capsule_get_handles(cs2->get_height(), cs2->get_radius());
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<CylinderShape3D>(*s)) {
		Ref<CylinderShape3D> cs2 = s;
		float radius = cs2->get_radius();
		float height = cs2->get_height();

#define PUSH_QUARTER(from_x, from_y, to_x, to_y, y)      \
	points_ptrw[index++] = Vector3(from_x, y, from_y);   \
	points_ptrw[index++] = Vector3(to_x, y, to_y);       \
	points_ptrw[index++] = Vector3(from_x, y, -from_y);  \
	points_ptrw[index++] = Vector3(to_x, y, -to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, from_y);  \
	points_ptrw[index++] = Vector3(-to_x, y, to_y);      \
	points_ptrw[index++] = Vector3(-from_x, y, -from_y); \
	points_ptrw[index++] = Vector3(-to_x, y, -to_y);

		// Number of points in an octant. So there will be 8 * points_in_octant * 2 points in total for one circle.
		// This corresponds to the smoothness of the circle.
		const uint32_t points_in_octant = 16;
		const real_t inc = (Math::PI / (4 * points_in_octant));
		const real_t radius_squared = radius * radius;
		real_t r = 0;

		Vector<Vector3> points;
		uint32_t index = 0;
		// 4 vertical lines and 2 full circles.
		points.resize(4 * 2 + 2 * 8 * points_in_octant * 2);
		Vector3 *points_ptrw = points.ptrw();
		float y_value = height * 0.5;

		// Vertical lines.
		points_ptrw[index++] = Vector3(0.f, y_value, radius);
		points_ptrw[index++] = Vector3(0.f, -y_value, radius);
		points_ptrw[index++] = Vector3(0.f, y_value, -radius);
		points_ptrw[index++] = Vector3(0.f, -y_value, -radius);
		points_ptrw[index++] = Vector3(radius, y_value, 0.f);
		points_ptrw[index++] = Vector3(radius, -y_value, 0.f);
		points_ptrw[index++] = Vector3(-radius, y_value, 0.f);
		points_ptrw[index++] = Vector3(-radius, -y_value, 0.f);

		float previous_x = radius;
		float previous_y = 0.f;

		for (uint32_t i = 0; i < points_in_octant; ++i) {
			r += inc;
			real_t x = Math::cos(r) * radius;
			real_t y = Math::sqrt(radius_squared - (x * x));

			// High circle ring.
			PUSH_QUARTER(previous_x, previous_y, x, y, y_value);
			PUSH_QUARTER(previous_y, previous_x, y, x, y_value);

			// Low circle ring.
			PUSH_QUARTER(previous_x, previous_y, x, y, -y_value);
			PUSH_QUARTER(previous_y, previous_x, y, x, -y_value);

			previous_x = x;
			previous_y = y;
		}
#undef PUSH_QUARTER

		p_gizmo->add_lines(points, material, false, collision_color);
		p_gizmo->add_collision_segments(points);

		Vector<Vector3> handles = helper->cylinder_get_handles(cs2->get_height(), cs2->get_radius());
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<WorldBoundaryShape3D>(*s)) {
		Ref<WorldBoundaryShape3D> wbs = s;
		const Plane &p = wbs->get_plane();

		Vector3 n1 = p.get_any_perpendicular_normal();
		Vector3 n2 = p.normal.cross(n1).normalized();

		Vector3 pface[4] = {
			p.normal * p.d + n1 * 10.0 + n2 * 10.0,
			p.normal * p.d + n1 * 10.0 + n2 * -10.0,
			p.normal * p.d + n1 * -10.0 + n2 * -10.0,
			p.normal * p.d + n1 * -10.0 + n2 * 10.0,
		};

		Vector<Vector3> points = {
			pface[0],
			pface[1],
			pface[1],
			pface[2],
			pface[2],
			pface[3],
			pface[3],
			pface[0],
			p.normal * p.d,
			p.normal * p.d + p.normal * 3
		};

		p_gizmo->add_lines(points, material, false, collision_color);
		p_gizmo->add_collision_segments(points);
	}

	if (Object::cast_to<ConvexPolygonShape3D>(*s)) {
		Vector<Vector3> points = Object::cast_to<ConvexPolygonShape3D>(*s)->get_points();

		if (points.size() > 1) { // Need at least 2 points for a line.
			Vector<Vector3> varr = Variant(points);
			Geometry3D::MeshData md;
			Error err = ConvexHullComputer::convex_hull(varr, md);
			if (err == OK) {
				Vector<Vector3> lines;
				lines.resize(md.edges.size() * 2);
				for (uint32_t i = 0; i < md.edges.size(); i++) {
					lines.write[i * 2 + 0] = md.vertices[md.edges[i].vertex_a];
					lines.write[i * 2 + 1] = md.vertices[md.edges[i].vertex_b];
				}
				p_gizmo->add_lines(lines, material, false, collision_color);
				p_gizmo->add_collision_segments(lines);
			}
		}
	}

	if (Object::cast_to<ConcavePolygonShape3D>(*s)) {
		Ref<ConcavePolygonShape3D> cs2 = s;
		Ref<ArrayMesh> mesh = cs2->get_debug_mesh();
		p_gizmo->add_lines(cs2->get_debug_mesh_lines(), material, false, collision_color);
		p_gizmo->add_collision_segments(cs2->get_debug_mesh_lines());
	}

	if (Object::cast_to<SeparationRayShape3D>(*s)) {
		Ref<SeparationRayShape3D> rs = s;

		Vector<Vector3> points = {
			Vector3(),
			Vector3(0, 0, rs->get_length())
		};
		p_gizmo->add_lines(points, material, false, collision_color);
		p_gizmo->add_collision_segments(points);
		Vector<Vector3> handles;
		handles.push_back(Vector3(0, 0, rs->get_length()));
		p_gizmo->add_handles(handles, handles_material);
	}

	if (Object::cast_to<HeightMapShape3D>(*s)) {
		Ref<HeightMapShape3D> hms = s;

		Vector<Vector3> lines = hms->get_debug_mesh_lines();
		p_gizmo->add_lines(lines, material, false, collision_color);
	}
}
