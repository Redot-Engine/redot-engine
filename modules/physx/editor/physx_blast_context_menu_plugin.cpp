/**************************************************************************/
/*  physx_blast_context_menu_plugin.cpp                                   */
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
/*  physx_blast_context_menu_plugin.cpp                                 */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_context_menu_plugin.h"

#ifdef GODOT_PHYSX_BLAST

#include "physx_blast_fracture_dialog.h"

#include "core/io/resource_loader.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "editor/editor_node.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/main/node.h"
#include "scene/resources/texture.h"

void PhysXBlastFractureMenuPlugin::get_options(const Vector<String> &p_paths) {
	if (p_paths.size() != 1) {
		return;
	}

	if (target_slot == CONTEXT_SLOT_SCENE_TREE) {
		Node *edited_scene = EditorNode::get_singleton()->get_edited_scene();
		if (!edited_scene) {
			return;
		}
		Node *node = edited_scene->get_node_or_null(NodePath(p_paths[0]));
		MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node);
		if (!mi || mi->get_mesh().is_null()) {
			return;
		}
		add_context_menu_item(TTR("Fracture with Blast..."), callable_mp(this, &PhysXBlastFractureMenuPlugin::_on_scene_tree_option), Ref<Texture2D>());
	} else if (target_slot == CONTEXT_SLOT_FILESYSTEM) {
		const String &path = p_paths[0];
		const String type = ResourceLoader::get_resource_type(path);
		if (type.is_empty() || !ClassDB::is_parent_class(type, "Mesh")) {
			return;
		}
		add_context_menu_item(TTR("Fracture with Blast..."), callable_mp(this, &PhysXBlastFractureMenuPlugin::_on_filesystem_option), Ref<Texture2D>());
	}
}

void PhysXBlastFractureMenuPlugin::_on_scene_tree_option(Array p_nodes) {
	if (p_nodes.is_empty()) {
		return;
	}
	Node *node = Object::cast_to<Node>((Object *)p_nodes[0]);
	if (!node || !dialog) {
		return;
	}
	dialog->open_for_node(node);
}

void PhysXBlastFractureMenuPlugin::_on_filesystem_option(PackedStringArray p_paths) {
	if (p_paths.is_empty() || !dialog) {
		return;
	}
	dialog->open_for_mesh_resource(p_paths[0]);
}

PhysXBlastFractureMenuPlugin::PhysXBlastFractureMenuPlugin(ContextMenuSlot p_target_slot, PhysXBlastFractureDialog *p_dialog) :
		target_slot(p_target_slot), dialog(p_dialog) {
}

#endif // GODOT_PHYSX_BLAST
