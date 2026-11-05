/**************************************************************************/
/*  physx_blast_fracture_dialog.cpp                                       */
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

//////////////////////////////////////////////////////////////////////////
/*  physx_blast_fracture_dialog.cpp                                     */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_fracture_dialog.h"

#ifdef GODOT_PHYSX_BLAST

#include "../blast/physx_blast_asset.h"
#include "../blast/physx_blast_authoring.h"
#include "../blast/physx_destructible_3d.h"
#include "physx_blast_preview.h"

#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/object.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/scene/3d/mesh_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/option_button.h"
#include "scene/gui/spin_box.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"
#include "scene/scene_string_names.h"

void PhysXBlastFractureDialog::_start(const Ref<Mesh> &p_mesh) {
	source_mesh = p_mesh;
	seed = 1;
	_regenerate();
	popup_centered(Size2(640, 560));
}

void PhysXBlastFractureDialog::_regenerate() {
	seed += 1;
	const int site_count = site_count_spin ? (int)site_count_spin->get_value() : 12;
	const int pattern_id = pattern_option ? pattern_option->get_selected_id() : 0;
	PhysXBlastAuthoring::FracturePattern pattern = PhysXBlastAuthoring::PATTERN_VORONOI;
	if (pattern_id == 1) {
		pattern = PhysXBlastAuthoring::PATTERN_SLICING;
	} else if (pattern_id == 2) {
		pattern = PhysXBlastAuthoring::PATTERN_CUTOUT;
	}
	if (cutout_pattern_picker) {
		cutout_pattern_picker->set_visible(pattern == PhysXBlastAuthoring::PATTERN_CUTOUT);
	}
	if (site_count_spin) {
		site_count_spin->set_visible(pattern != PhysXBlastAuthoring::PATTERN_CUTOUT); // Cutout ignores it entirely
	}

	Ref<Texture2D> cutout_pattern;
	if (pattern == PhysXBlastAuthoring::PATTERN_CUTOUT && cutout_pattern_picker) {
		cutout_pattern = cutout_pattern_picker->get_edited_resource();
	}

	Ref<PhysXBlastAuthoring> authoring;
	authoring.instantiate();
	authored_asset = authoring->fracture_mesh(source_mesh, site_count, seed, pattern, cutout_pattern);

	if (authored_asset.is_null()) {
		if (chunk_count_label) {
			if (pattern == PhysXBlastAuthoring::PATTERN_CUTOUT && cutout_pattern.is_null()) {
				chunk_count_label->set_text(TTR("Cutout needs a pattern texture -- pick one above."));
			} else {
				chunk_count_label->set_text(TTR("Fracture failed -- check the mesh has real geometry."));
			}
		}
		get_ok_button()->set_disabled(true);
		return;
	}

	get_ok_button()->set_disabled(false);
	const int piece_count = MAX(authored_asset->get_chunk_count() - 1, 0);
	if (chunk_count_label) {
		chunk_count_label->set_text(vformat(TTR("%d pieces"), piece_count));
	}

	if (preview) {
		preview->edit(physx_blast_build_preview_mesh(authored_asset));
	}
}

void PhysXBlastFractureDialog::_on_confirmed() {
	if (authored_asset.is_null()) {
		return;
	}

	// Re-validate the (possibly long-dead) target on every use (EDIT-1): the
	// dialog can stay open across node deletion, scene switches, or undo
	// history — a raw Node* captured at open time would dangle by now.
	Node3D *target_node = Object::cast_to<Node3D>(ObjectDB::get_instance(target_node_id));

	String save_dir;
	String base_name;
	if (mode == MODE_SCENE_NODE && target_node) {
		Node *scene_root = EditorNode::get_singleton()->get_edited_scene();
		const String scene_path = scene_root ? scene_root->get_scene_file_path() : String();
		save_dir = scene_path.is_empty() ? "res://" : scene_path.get_base_dir();
		base_name = target_node->get_name();
	} else if (mode == MODE_MESH_RESOURCE) {
		save_dir = target_mesh_path.get_base_dir();
		base_name = target_mesh_path.get_file().get_basename();
	} else {
		if (mode == MODE_SCENE_NODE) {
			// The scene node the dialog was opened for is gone; nothing left
			// to replace — still save the asset so the authoring work isn't
			// lost, then bail out of the node-swap stage.
			ERR_PRINT("PhysXBlastFractureDialog: target node was deleted while the dialog was open; saving the asset only.");
			const Error err = ResourceSaver::save(authored_asset, "res://fractured_blast.tres");
			if (err != OK) {
				ERR_PRINT(vformat("PhysXBlastFractureDialog: failed to save fallback asset (error %d).", (int)err));
			} else {
				authored_asset->set_path("res://fractured_blast.tres", true);
				EditorFileSystem::get_singleton()->update_file("res://fractured_blast.tres");
			}
		}
		return;
	}

	const String asset_path = save_dir.path_join(base_name + "_blast.tres");
	const Error err = ResourceSaver::save(authored_asset, asset_path);
	if (err != OK) {
		ERR_PRINT(vformat("PhysXBlastFractureDialog: failed to save '%s' (error %d).", asset_path, (int)err));
		return;
	}
	authored_asset->set_path(asset_path, true);
	EditorFileSystem::get_singleton()->update_file(asset_path);

	if (mode == MODE_SCENE_NODE && target_node) {
		Node3D *old_node = target_node;
		if (!old_node->get_parent()) {
			return;
		}

		PhysXDestructible3D *destructible = memnew(PhysXDestructible3D);
		destructible->set_name(old_node->get_name());
		destructible->set_transform(old_node->get_transform());
		destructible->set_blast_asset(authored_asset);

		EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
		ur->create_action(TTR("Fracture Mesh with Blast"));
		ur->add_do_method(old_node, "replace_by", destructible, true);
		ur->add_do_reference(destructible);
		ur->add_undo_method(destructible, "replace_by", old_node, true);
		ur->add_undo_reference(old_node);
		ur->commit_action();
	}
}

void PhysXBlastFractureDialog::open_for_node(Node *p_mesh_instance) {
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_mesh_instance);
	if (!mi || mi->get_mesh().is_null()) {
		return;
	}
	mode = MODE_SCENE_NODE;
	target_node_id = p_mesh_instance->get_instance_id();
	target_mesh_path = String();
	_start(mi->get_mesh());
}

void PhysXBlastFractureDialog::open_for_mesh_resource(const String &p_mesh_path) {
	Ref<Mesh> mesh = ResourceLoader::load(p_mesh_path);
	if (mesh.is_null()) {
		return;
	}
	mode = MODE_MESH_RESOURCE;
	target_node_id = ObjectID();
	target_mesh_path = p_mesh_path;
	_start(mesh);
}

PhysXBlastFractureDialog::PhysXBlastFractureDialog() {
	set_title(TTR("Fracture Mesh with Blast"));

	VBoxContainer *root = memnew(VBoxContainer);
	add_child(root);

	preview = memnew(MeshEditor);
	preview->set_custom_minimum_size(Size2(0, 360) * EDSCALE);
	preview->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	root->add_child(preview);

	HBoxContainer *controls = memnew(HBoxContainer);
	root->add_child(controls);

	controls->add_child(memnew(Label(TTR("Pattern:"))));
	pattern_option = memnew(OptionButton);
	pattern_option->add_item(TTR("Voronoi"), 0);
	pattern_option->add_item(TTR("Slicing"), 1);
	pattern_option->add_item(TTR("Cutout"), 2);
	pattern_option->connect(SceneStringName(item_selected), callable_mp(this, &PhysXBlastFractureDialog::_regenerate).unbind(1));
	controls->add_child(pattern_option);

	controls->add_child(memnew(Label(TTR("Chunks:"))));
	site_count_spin = memnew(SpinBox);
	site_count_spin->set_min(4);
	site_count_spin->set_max(200);
	site_count_spin->set_step(1);
	site_count_spin->set_value(12);
	site_count_spin->set_custom_minimum_size(Size2(80, 0) * EDSCALE);
	controls->add_child(site_count_spin);

	Button *regenerate_button = memnew(Button);
	regenerate_button->set_text(TTR("Regenerate"));
	regenerate_button->connect(SceneStringName(pressed), callable_mp(this, &PhysXBlastFractureDialog::_regenerate));
	controls->add_child(regenerate_button);

	chunk_count_label = memnew(Label);
	chunk_count_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	chunk_count_label->set_horizontal_alignment(HORIZONTAL_ALIGNMENT_RIGHT);
	controls->add_child(chunk_count_label);

	// Second row, only relevant (visible) for PATTERN_CUTOUT -- that
	// pattern doesn't generate its own crack shape the way Voronoi/Slicing
	// do (see PhysXBlastAuthoring's own notes on why), the caller supplies
	// a grayscale pattern bitmap directly.
	HBoxContainer *cutout_row = memnew(HBoxContainer);
	root->add_child(cutout_row);
	cutout_row->add_child(memnew(Label(TTR("Cutout Pattern:"))));
	cutout_pattern_picker = memnew(EditorResourcePicker);
	cutout_pattern_picker->set_base_type("Texture2D");
	cutout_pattern_picker->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	cutout_pattern_picker->connect(SNAME("resource_changed"), callable_mp(this, &PhysXBlastFractureDialog::_regenerate).unbind(1));
	cutout_pattern_picker->set_visible(false); // default pattern is Voronoi
	cutout_row->add_child(cutout_pattern_picker);

	set_ok_button_text(TTR("Accept"));
}

void PhysXBlastFractureDialog::_notification(int p_what) {
	// Self-signal connect deferred to ENTER_TREE (not the constructor):
	// this class is never ClassDB-registered (it's editor-internal, not
	// scriptable), so its own GDType isn't initialized yet when the
	// constructor runs -- connecting "confirmed" (declared on the AcceptDialog
	// ancestor) on `this` that early fails to find it. Matches the working
	// pattern editor/gui/create_dialog.cpp's CreateDialog already uses for
	// the exact same reason.
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			connect(SceneStringName(confirmed), callable_mp(this, &PhysXBlastFractureDialog::_on_confirmed));
		} break;
		case NOTIFICATION_EXIT_TREE: {
			disconnect(SceneStringName(confirmed), callable_mp(this, &PhysXBlastFractureDialog::_on_confirmed));
		} break;
	}
}

#endif // GODOT_PHYSX_BLAST
