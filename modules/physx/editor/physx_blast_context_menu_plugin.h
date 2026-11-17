/**************************************************************************/
/*  physx_blast_context_menu_plugin.h                                     */
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

#pragma once

#ifdef GODOT_PHYSX_BLAST

#include "editor/inspector/editor_context_menu_plugin.h"

class PhysXBlastFractureDialog;

// Adds a "Fracture with Blast..." item to two right-click menus -- a
// MeshInstance3D node in the Scene tree dock (CONTEXT_SLOT_SCENE_TREE) and a
// Mesh resource file in the FileSystem dock (CONTEXT_SLOT_FILESYSTEM) -- both
// opening the same PhysXBlastFractureDialog instance. One plugin instance
// handles exactly one slot (EditorContextMenuPluginManager::add_plugin sets
// the slot per-Ref), so PhysXEditorPlugin constructs two of these, one per
// slot, both pointed at the same dialog.
class PhysXBlastFractureMenuPlugin : public EditorContextMenuPlugin {
	GDCLASS(PhysXBlastFractureMenuPlugin, EditorContextMenuPlugin);

	ContextMenuSlot target_slot;
	PhysXBlastFractureDialog *dialog = nullptr; // not owned, outlives this plugin

	void _on_scene_tree_option(Array p_nodes);
	void _on_filesystem_option(PackedStringArray p_paths);

public:
	void get_options(const Vector<String> &p_paths) override;

	PhysXBlastFractureMenuPlugin(ContextMenuSlot p_target_slot, PhysXBlastFractureDialog *p_dialog);
};

#endif // GODOT_PHYSX_BLAST
