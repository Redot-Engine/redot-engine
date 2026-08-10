/**************************************************************************/
/*  baker.cpp                                                             */
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

// Terrain3D Godot plugin: Copyright © 2025 Cory Petkovsek, Roope Palmroos, and Contributors.
#include "baker.h"

#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/gui/margin_container.h"
#include "scene/resources/3d/navigation_mesh_source_geometry_data_3d.h"
#include "scene/resources/mesh.h"
#include "servers/navigation_server_3d.h"

#include "core/core_bind.h"

#include "modules/worldscape_3d/editor/worldscape_3d_editor.h"
#include "modules/worldscape_3d/worldscape_3d.h"

#include <cassert>

namespace {
const String BAKE_MESH_DESCRIPTION = R"(This will create a child MeshInstance3D. LOD4+ is recommended. LOD0 is slow and dense with vertices every 1 unit. It is not an optimal mesh.)";
const String BAKE_OCCLUDER_DESCRIPTION = R"(This will create a child OccluderInstance3D. LOD4+ is recommended and will take 5+ seconds per region to generate. LOD0 is unnecessarily dense and slow.)";
const String SET_UP_NAVIGATION_DESCRIPTION = R"(This operation will:

- Create a NavigationRegion3D node,
- Assign it a blank NavigationMesh resource,
- Move the WorldScape3D node to be a child of the new node,
- And bake the nav mesh.

Once setup is complete, you can modify the settings on your nav mesh, and rebake
without having to run through the setup again.

If preferred, this setup can be canceled and the steps performed manually. For
the best results, adjust the settings on the NavigationMesh resource to match
the settings of your navigation agents and collisions.)";

using Polygon = Vector<int32_t>;

Vector<Vector3> postprocess_nav_mesh_round_vertices(NavigationMesh *nav_mesh) {
	assert(nav_mesh != nullptr);
	assert(nav_mesh->get_cell_size() > 0.f);
	assert(nav_mesh->get_cell_height() > 0.f);

	const Vector3 cell_size{ nav_mesh->get_cell_size(), nav_mesh->get_cell_height(), nav_mesh->get_cell_size() };

	// Round a little harder to avoid rounding errors with non-power-of-two cell_size/cell_height
	// causing the navigation map to put two non-matching edges in the same cell:
	const auto round_factor = cell_size * 1.001f;

	Vector<Vector3> vertices = nav_mesh->get_vertices();
	for (auto &vertex : vertices) {
		vertex = (vertex / round_factor).floor() * round_factor;
	}
	return vertices;
}

Vector<Polygon> postprocess_nav_mesh_remove_empty_polygons(NavigationMesh *nav_mesh, const Vector<Vector3> &vertices) {
	Vector<Polygon> polygons{};
	for (int i = 0; i < nav_mesh->get_polygon_count(); i++) {
		Polygon old_polygon = nav_mesh->get_polygon(i);
		Polygon new_polygon{};

		// Remove duplicate vertices (introduced by rounding) from the polygon:
		Vector<Vector3> polygon_vertices{};
		for (auto index : old_polygon) {
			auto vertex = vertices[index];
			if (polygon_vertices.has(vertex)) {
				continue;
			}
			polygon_vertices.push_back(vertex);
			new_polygon.push_back(index);
		}
		// If we removed some vertices, we might be able to remove the polygon too:
		if (new_polygon.size() <= 2) {
			continue;
		}
		polygons.push_back(new_polygon);
	}
	return polygons;
}

void postprocess_nav_mesh_remove_overlapping_polygons(
		NavigationMesh *nav_mesh,
		const Vector<Vector3> &vertices,
		Vector<Polygon> &polygons) {
	// Occasionally, a baked nav mesh comes out with overlapping polygons:
	// https://github.com/godotengine/godot/issues/85548#issuecomment-1839341071
	// Until the bug is fixed in the engine, this function attempts to detect and remove overlapping
	// polygons.

	// This function has to make a choice of which polygon to remove when an overlap is detected,
	// because in this case the nav mesh is ambiguous. To do this it uses a heuristic:
	// (1) an 'overlap' is defined as an edge that is shared by 3 or more polygons.
	// (2) a 'bad polygon' is defined as a polygon that contains 2 or more 'overlaps'.
	// The function removes the 'bad polygons', which in practice seems to be enough to remove all
	// overlaps without creating holes in the nav mesh.

	const Vector3 cell_size{ nav_mesh->get_cell_size(), nav_mesh->get_cell_height(), nav_mesh->get_cell_size() };

	// `edges` is going to map edges (vertex pairs) to arrays of polygons that contain that edge.
	Dictionary edges{};

	for (int polygon_index = 0; polygon_index < polygons.size(); polygon_index++) {
		const auto &polygon = polygons[polygon_index];
		for (int j = 0; j < polygon.size(); j++) {
			Vector3 vertex = vertices[polygon[j]];
			Vector3 next_vertex = vertices[polygon[(j + 1) % polygon.size()]];

			// edge_key is a key we can use in the edges dictionary that uniquely identifies the
			// edge. We use cell coordinates here (Vector3i) because with a non-power-of-two
			// cell_size, rounding errors can cause Vector3 vertices to not be equal.
			// Array.sort IS defined for vector types - see the Godot docs. It's necessary here
			// because polygons that share an edge can have their vertices in a different order.
			Array edge_key{ Vector3i{ vertex / cell_size }, Vector3i{ next_vertex / cell_size } };
			edge_key.sort();

			if (!edges.has(edge_key)) {
				edges[edge_key] = Polygon{};
			}
			Polygon edge = edges[edge_key];
			edge.push_back(polygon_index);
			edges[edge_key] = edge;
		}
	}
	Dictionary overlap_count{};
	for (auto values : edges.values()) {
		Polygon connections = values;
		if (connections.size() <= 2) {
			continue;
		}
		for (int i = 0; i < connections.size(); ++i) {
			overlap_count[connections[i]] = static_cast<int>(overlap_count.get(connections[i], 0)) + 1;
		}
	}
	Vector<int> bad_polygons{};
	for (int poly_index : overlap_count.keys()) {
		if (static_cast<int>(overlap_count[poly_index]) >= 2) {
			bad_polygons.push_back(poly_index);
		}
	}
	bad_polygons.sort();
	for (int i = bad_polygons.size() - 1; i >= 0; --i) {
		polygons.remove_at(bad_polygons[i]);
	}
}

void postprocess_nav_mesh(NavigationMesh *nav_mesh) {
	// Post-process the nav mesh to work around Godot issue #85548

	// Round all the vertices in the nav_mesh to the nearest cell_size/cell_height so that it doesn't
	// contain any edges shorter than cell_size/cell_height (one cause of #85548).
	auto vertices = postprocess_nav_mesh_round_vertices(nav_mesh);

	// Rounding vertices can collapse some edges to 0 length. We remove these edges, and any polygons
	// that have been reduced to 0 area.
	auto polygons = postprocess_nav_mesh_remove_empty_polygons(nav_mesh, vertices);

	// Another cause of #85548 is baking producing overlapping polygons. We remove these.
	postprocess_nav_mesh_remove_overlapping_polygons(nav_mesh, vertices, polygons);

	nav_mesh->clear_polygons();
	nav_mesh->set_vertices(vertices);
	for (auto &polygon : polygons) {
		nav_mesh->add_polygon(polygon);
	}
}
} //namespace

void BakerLODDialog::setup_layout() {
	set_title("Bake terrain mesh");
	set_position(Point2i{ 0, 36 });
	set_size(Size2i{ 400, 155 });

	_margin = memnew(MarginContainer);
	_margin->set_name("MarginContainer");
	_margin->set_offset(Side::SIDE_LEFT, 8.f);
	_margin->set_offset(SIDE_TOP, 8.f);
	_margin->set_offset(SIDE_RIGHT, 392.f);
	_margin->set_offset(SIDE_BOTTOM, 106.f);
	_margin->add_theme_constant_override("theme_override_constants/margin_left", 10);
	_margin->add_theme_constant_override("theme_override_constants/margin_top", 10);
	_margin->add_theme_constant_override("theme_override_constants/margin_right", 10);
	_margin->add_theme_constant_override("theme_override_constants/margin_bottom", 10);
	add_child(_margin);

	_vbox = memnew(VBoxContainer);
	_vbox->set_name("VBoxContainer");
	_margin->add_child(_vbox);

	_hbox = memnew(HBoxContainer);
	_hbox->set_name("HBoxContainer");
	_hbox->add_theme_constant_override("theme_override_constants/separation", 20);
	_vbox->add_child(_hbox);

	_label = memnew(Label);
	_label->set_name("Label");
	_label->set_text("LOD:");
	_hbox->add_child(_label);

	_lodbox = memnew(SpinBox);
	_lodbox->set_name("LodBox");
	_lodbox->set_unique_name_in_owner(true);
	_lodbox->set_h_size_flags(Control::SIZE_FILL | Control::SIZE_EXPAND);
	_lodbox->set_max(8.f);
	_lodbox->set_value(4.f);
	_hbox->add_child(_lodbox);

	_description_label = memnew(Label);
	_description_label->set_name("DescriptionLabel");
	_description_label->set_unique_name_in_owner(true);
	_description_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD);
	_vbox->add_child(_description_label);

	set_visible(true);
}

void BakerLODDialog::init() {
	set_unparent_when_invisible(true);
	setup_layout();
	if (!is_connected("about_to_popup", callable_mp(this, &BakerLODDialog::on_about_to_popup))) {
		connect("about_to_popup", callable_mp(this, &BakerLODDialog::on_about_to_popup));
	}
	if (!is_connected("visibility_changed", callable_mp(this, &BakerLODDialog::on_visibility_changed))) {
		connect("visibility_changed", callable_mp(this, &BakerLODDialog::on_visibility_changed));
	}
	if (_lodbox && !_lodbox->is_connected("value_changed", callable_mp(this, &BakerLODDialog::on_lod_box_value_changed))) {
		_lodbox->connect("value_changed", callable_mp(this, &BakerLODDialog::on_lod_box_value_changed));
	}
}

void BakerLODDialog::on_about_to_popup() {
	_lod = static_cast<int>(_lodbox->get_value());
}

void BakerLODDialog::on_visibility_changed() {
	// Change text on the autowrap label only when the popup is visible.
	// Works around Godot issue #47005:
	// https://github.com/godotengine/godot/issues/47005
	if (is_visible()) {
		_description_label->set_text(_description);
	}
}

void BakerLODDialog::on_lod_box_value_changed(real_t) {
	_lod = static_cast<int>(_lodbox->get_value());
}

BakerLODDialog::~BakerLODDialog() {
	_description_label->queue_free();
	_lodbox->queue_free();
	_label->queue_free();
	_hbox->queue_free();
	_vbox->queue_free();
	_margin->queue_free();
}

void BakerLODDialog::_notification(int what) {
	if (what == NOTIFICATION_POSTINITIALIZE) {
		init();
	}
}

void Baker::bake_mesh_popup() {
	popup(BAKE_MESH_DESCRIPTION, callable_mp(this, &Baker::Baker::bake_mesh));
}

void Baker::bake_occluder_popup() {
	popup(BAKE_OCCLUDER_DESCRIPTION, callable_mp(this, &Baker::Baker::bake_occluder));
}

void Baker::set_up_navigation_popup() {
	if (auto terrain = _plugin->get_terrain(); terrain) {
		_bake_method = callable_mp(this, &Baker::set_up_navigation);
		_confirm_dlg->set_text(SET_UP_NAVIGATION_DESCRIPTION);
		EditorInterface::get_singleton()->popup_dialog_centered(_confirm_dlg);
	}
}

void Baker::popup(const String &descr, const Callable &method) {
	if (auto terrain = _plugin->get_terrain(); terrain) {
		_bake_method = method;
		_bake_lod_dlg->set_description(descr);
		EditorInterface::get_singleton()->popup_dialog_centered(_bake_lod_dlg);
	}
}

void Baker::bake_mesh() {
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		return;
	}

	auto tdata = terrain->get_data();
	if (!tdata || tdata->get_region_count() == 0) {
		print_error("Terrain has no active regions to bake");
		return;
	}

	auto mesh = terrain->bake_mesh(_bake_lod_dlg->get_lod(), WorldScape3DData::HEIGHT_FILTER_NEAREST);
	if (mesh.is_null()) {
		print_error("Failed to bake mesh from terrain");
		return;
	}

	auto undo = _plugin->get_undo_redo();
	undo->create_action("WorldScape3D Bake ArrayMesh");

	auto mesh_instance = cast_to<MeshInstance3D>(terrain->get_node_or_null(NodePath{ "MeshInstance3D" }));
	if (!mesh_instance) {
		mesh_instance = memnew(MeshInstance3D);
		mesh_instance->set_name("MeshInstance3D");
		mesh_instance->set_skeleton_path(NodePath{});
		mesh_instance->set_mesh(mesh);

		undo->add_do_method(terrain, SNAME("add_child"), mesh_instance, true);
		undo->add_undo_method(terrain, SNAME("remove_child"), mesh_instance);
		undo->add_do_property(mesh_instance, SNAME("owner"), EditorInterface::get_singleton()->get_edited_scene_root());
		undo->add_do_reference(mesh_instance);
	} else {
		undo->add_do_property(mesh_instance, SNAME("mesh"), mesh);
		undo->add_undo_property(mesh_instance, SNAME("mesh"), mesh_instance->get_mesh());

		if (mesh.is_valid() && !mesh->get_path().is_empty()) {
			const auto path = mesh->get_path();
			undo->add_do_method(mesh.ptr(), SNAME("take_over_path"), path);
			undo->add_undo_method(mesh_instance->get_mesh().ptr(), SNAME("take_over_path"), path);
			const auto saver = CoreBind::ResourceSaver::get_singleton();
			undo->add_do_method(saver, SNAME("save"), mesh);
			undo->add_undo_method(saver, SNAME("save"), mesh_instance->get_mesh());
		}
	}
	undo->commit_action();
}

void Baker::bake_occluder() {
	auto terrain = _plugin->get_terrain();
	if (!terrain) {
		return;
	}

	auto tdata = terrain->get_data();
	if (!tdata || tdata->get_region_count() == 0) {
		print_error("Terrain has no active regions to bake");
		return;
	}

	auto mesh = terrain->bake_mesh(_bake_lod_dlg->get_lod(), WorldScape3DData::HEIGHT_FILTER_NEAREST);
	if (mesh.is_null()) {
		print_error("Failed to bake mesh from terrain");
		return;
	}
	assert(mesh->get_surface_count() == 1);

	auto occluder = memnew(ArrayOccluder3D);

	auto undo = _plugin->get_undo_redo();
	undo->create_action("WorldScape3D Bake Occluder3D");

	auto arrays = mesh->surface_get_arrays(0);
	assert(arrays.size() > Mesh::ARRAY_INDEX);
	assert(!arrays[Mesh::ARRAY_INDEX].is_null());
	occluder->set_arrays(arrays[Mesh::ARRAY_VERTEX], arrays[Mesh::ARRAY_INDEX]);

	auto occluder_instance = cast_to<OccluderInstance3D>(terrain->get_node_or_null(NodePath{ "OccluderInstance3D" }));
	if (!occluder_instance) {
		occluder_instance = memnew(OccluderInstance3D);
		occluder_instance->set_name("OccluderInstance3D");
		occluder_instance->set_occluder(occluder);

		undo->add_do_method(terrain, SNAME("add_child"), occluder_instance, true);
		undo->add_undo_method(terrain, SNAME("remove_child"), occluder_instance);
		undo->add_do_property(occluder_instance, SNAME("owner"), EditorInterface::get_singleton()->get_edited_scene_root());
		undo->add_do_reference(occluder_instance);
	} else {
		undo->add_do_property(occluder_instance, SNAME("occluder"), occluder);
		undo->add_undo_property(occluder_instance, SNAME("occluder"), occluder_instance->get_occluder());

		if (!occluder_instance->get_occluder()->get_path().is_empty()) {
			const auto path = occluder_instance->get_occluder()->get_path();
			undo->add_do_method(occluder, SNAME("take_over_path"), path);
			undo->add_undo_method(occluder_instance->get_occluder().ptr(), SNAME("take_over_path"), path);
			const auto saver = CoreBind::ResourceSaver::get_singleton();
			undo->add_do_method(saver, SNAME("save"), occluder);
			undo->add_undo_method(saver, SNAME("save"), occluder_instance->get_occluder());
		}
	}

	undo->commit_action();
}

void Baker::bake_nav_region_nav_mesh(NavigationRegion3D *nav_region) {
	auto nav_mesh = nav_region->get_navigation_mesh();
	assert(nav_mesh.is_valid());

	auto nav_server = NavigationServer3D::get_singleton();

	Ref source_geometry_data = memnew(NavigationMeshSourceGeometryData3D);
	nav_server->parse_source_geometry_data(nav_mesh, source_geometry_data, nav_region);

	for (auto &terrain : find_nav_region_terrains(nav_region)) {
		AABB aabb = nav_mesh->get_filter_baking_aabb();
		aabb.position += nav_mesh->get_filter_baking_aabb_offset();
		aabb = nav_region->get_global_transform().xform(aabb);
		auto faces = terrain->generate_nav_mesh_source_geometry(aabb);
		if (!faces.is_empty()) {
			source_geometry_data->add_faces(faces, Transform3D{});
		}
	}
	nav_server->bake_from_source_geometry_data(nav_mesh, source_geometry_data);

	postprocess_nav_mesh(nav_mesh.ptr());

	// Assign null first to force the debug display to actually update:
	nav_region->set_navigation_mesh(nullptr);
	nav_region->set_navigation_mesh(nav_mesh);
	// Trigger save to disk if it is saved as an external file
	if (!nav_mesh->get_path().is_empty()) {
		const auto saver = CoreBind::ResourceSaver::get_singleton();
		saver->save(nav_mesh, nav_mesh->get_path(), CoreBind::ResourceSaver::FLAG_COMPRESS);
	}
	// Let other editor plugins and tool scripts know the nav mesh was just baked:
	nav_region->emit_signal("bake_finished");
}

void Baker::bake_nav_mesh() {
	if (_plugin->get_nav_region()) {
		// A NavigationRegion3D is selected. We only need to bake that one navmesh.
		bake_nav_region_nav_mesh(_plugin->get_nav_region());
		print_line("WorldScape3DNavigation: Finished baking 1 NavigationMesh.");
		return;
	}

	auto terrain = _plugin->get_terrain();
	if (terrain) {
		if (terrain->get_data()->get_region_count() == 0) {
			print_error("WorldScape3D has no active regions to bake");
			return;
		}

		// A WorldScape3D is selected. There are potentially multiple navmeshes to bake and we need to
		// find them all. (The multiple navmesh use-case is likely on very large scenes with lots of
		// geometry. Each navmesh in this case would define its own, non-overlapping, baking AABB, to
		// cut down on the amount of geometry to bake. In a large open-world RPG, for instance, there
		// could be a navmesh for each town.)
		auto nav_regions = find_terrain_nav_regions(terrain);
		for (auto &nav_region : nav_regions) {
			bake_nav_region_nav_mesh(nav_region);
		}
		printf("WorldScape3DNavigation: Finished baking %ld NavigationMesh(es).", nav_regions.size());
	}
}

void Baker::do_set_up_navigation(NavigationRegion3D *nav_region, WorldScape3D *terrain) {
	auto parent = terrain->get_parent();
	auto index = terrain->get_index();
	auto owner = terrain->get_owner();

	parent->add_child(nav_region, true);
	terrain->reparent(nav_region);
	parent->move_child(nav_region, index);

	nav_region->set_owner(owner);
	terrain->set_owner(owner);
}

void Baker::undo_set_up_navigation(NavigationRegion3D *nav_region, WorldScape3D *terrain) {
	assert(terrain->get_parent() == nav_region);

	auto parent = nav_region->get_parent();
	auto index = nav_region->get_index();
	auto owner = nav_region->get_owner();

	terrain->reparent(parent);
	parent->remove_child(nav_region);
	parent->move_child(terrain, index);

	terrain->set_owner(owner);
}

void Baker::set_up_navigation() {
	auto terrain = _plugin->get_terrain();
	assert(terrain);
	if (terrain == EditorInterface::get_singleton()->get_edited_scene_root()) {
		print_error("WorldScape3D Navigation setup not possible if WorldScape3D node is scene root");
		return;
	}
	if (terrain->get_data()->get_region_count() == 0) {
		print_error("WorldScape3D has no active regions");
		return;
	}

	auto undo_redo = _plugin->get_undo_redo();
	undo_redo->create_action("WorldScape3D Set up Navigation");

	auto nav_region = memnew(NavigationRegion3D);
	nav_region->set_name("NavigationRegion3D");
	nav_region->set_navigation_mesh(memnew(NavigationMesh));

	undo_redo->add_do_method(this, SNAME("do_set_up_navigation"), nav_region, terrain);
	undo_redo->add_undo_method(this, SNAME("undo_set_up_navigation"), nav_region, terrain);
	undo_redo->add_do_reference(nav_region);

	undo_redo->commit_action();

	EditorInterface::get_singleton()->inspect_object(nav_region);
	assert(nav_region == _plugin->get_nav_region());

	bake_nav_mesh();
}

void Baker::on_confirm() {
	_bake_method.call();
}

Baker::Baker(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin } {
	_bake_lod_dlg = memnew(BakerLODDialog);
	_bake_lod_dlg->hide();
	_bake_lod_dlg->connect("confirmed", callable_mp(this, &Baker::on_confirm));
	_bake_lod_dlg->set_unparent_when_invisible(true);

	_confirm_dlg = memnew(ConfirmationDialog);
	_confirm_dlg->hide();
	_confirm_dlg->connect("confirmed", callable_mp(this, &Baker::on_confirm));
	_confirm_dlg->set_unparent_when_invisible(true);
}

Baker::~Baker() {
	_confirm_dlg->queue_free();
	_bake_lod_dlg->queue_free();
}

Vector<WorldScape3D *> Baker::find_nav_region_terrains(NavigationRegion3D *nav_region) const {
	Vector<WorldScape3D *> result;
	if (!nav_region) {
		return result;
	}
	Ref<NavigationMesh> nav_mesh = nav_region->get_navigation_mesh();
	if (nav_mesh.is_null()) {
		return result;
	}

	NavigationMesh::SourceGeometryMode source_mode = nav_mesh->get_source_geometry_mode();
	if (source_mode == NavigationMesh::SOURCE_GEOMETRY_ROOT_NODE_CHILDREN) {
		auto terrains = nav_region->find_children("", "WorldScape3D");
		for (const auto &node : terrains) {
			auto terrain = cast_to<WorldScape3D>(node);
			if (terrain) {
				result.append(terrain);
			}
		}
		return result;
	}
	List<Node *> group_nodes;
	nav_region->get_tree()->get_nodes_in_group(nav_mesh->get_source_group_name(), &group_nodes);
	for (auto const &node : group_nodes) {
		if (auto terrain = cast_to<WorldScape3D>(node)) {
			result.append(terrain);
		}
		if (source_mode == NavigationMesh::SOURCE_GEOMETRY_GROUPS_WITH_CHILDREN) {
			auto children = nav_region->find_children("", "WorldScape3D");
			for (const auto &child : children) {
				if (auto terrain = cast_to<WorldScape3D>(child)) {
					result.append((terrain));
				}
			}
		}
	}
	return result;
}

Vector<NavigationRegion3D *> Baker::find_terrain_nav_regions(WorldScape3D *terrain) const {
	Vector<NavigationRegion3D *> result;
	auto root_node = EditorInterface::get_singleton()->get_edited_scene_root();
	if (!root_node) {
		return result;
	}
	auto children = root_node->find_children("", "NavigationRegion3D");
	for (const auto &child : children) {
		if (auto nav_r = cast_to<NavigationRegion3D>(child); nav_r) {
			if (find_nav_region_terrains(nav_r).has(terrain)) {
				result.append(nav_r);
			}
		}
	}
	return result;
}

void Baker::_bind_methods() {
	ClassDB::bind_method(D_METHOD("do_set_up_navigation", "nav_region", "terrain"), &Baker::do_set_up_navigation);
	ClassDB::bind_method(D_METHOD("undo_set_up_navigation", "nav_region", "terrain"), &Baker::undo_set_up_navigation);
}
