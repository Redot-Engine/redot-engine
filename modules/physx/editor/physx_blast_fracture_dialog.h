/**************************************************************************/
/*  physx_blast_fracture_dialog.h                                         */
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

#include "scene/gui/dialogs.h"

class EditorResourcePicker;
class Label;
class Mesh;
class MeshEditor;
class Node;
class OptionButton;
class PhysXBlastAsset;
class SpinBox;

// Live in-viewport Blast fracture authoring: runs
// PhysXBlastAuthoring::fracture_mesh() against a real Mesh and shows the
// resulting fracture cells (one flat color per leaf chunk) in an embedded
// MeshEditor preview (the same live-rotate mesh preview the inspector uses
// for a Mesh resource) -- Regenerate tries a new random seed, Accept saves a
// PhysXBlastAsset .tres and, when opened for a scene node, swaps that node
// for a PhysXDestructible3D using it.
//
// Deliberately the only piece of UI this feature has -- no separate
// "fracture settings" dock/panel. Authoring a fracture pattern blind (unlike
// authoring collision shapes, which are functional/invisible so blind
// authoring is fine) isn't something users can do without seeing the result
// first, so the dialog always shows a live result rather than taking
// parameters up front.
class PhysXBlastFractureDialog : public ConfirmationDialog {
	GDCLASS(PhysXBlastFractureDialog, ConfirmationDialog);

	enum Mode {
		MODE_NONE,
		MODE_SCENE_NODE,
		MODE_MESH_RESOURCE,
	};

	Mode mode = MODE_NONE;
	// MODE_SCENE_NODE only, not owned by this dialog. Stored as an ObjectID
	// and re-validated on confirm (EDIT-1): the target can be deleted (or the
	// scene closed) while the dialog sits open, and a raw Node* would then be
	// a dangling pointer used by _on_confirmed().
	ObjectID target_node_id;
	String target_mesh_path; // MODE_MESH_RESOURCE only.
	Ref<Mesh> source_mesh;

	MeshEditor *preview = nullptr;
	OptionButton *pattern_option = nullptr;
	SpinBox *site_count_spin = nullptr;
	// Only shown for PATTERN_CUTOUT -- that pattern doesn't generate itself
	// (see PhysXBlastAuthoring's own notes on why), the caller supplies a
	// grayscale bitmap directly.
	EditorResourcePicker *cutout_pattern_picker = nullptr;
	Label *chunk_count_label = nullptr;

	int seed = 1;
	Ref<PhysXBlastAsset> authored_asset;

	void _start(const Ref<Mesh> &p_mesh);
	void _regenerate();
	void _on_confirmed();

protected:
	void _notification(int p_what);

public:
	// Opens the dialog for a MeshInstance3D already in the edited scene:
	// Accept replaces it in-place (Node::replace_by, so children/groups carry
	// over) with a PhysXDestructible3D using the authored asset, undo/redo
	// recorded, and saves the asset resource next to the scene file.
	void open_for_node(Node *p_mesh_instance);

	// Opens the dialog for a Mesh resource selected in the FileSystem dock:
	// Accept just saves a PhysXBlastAsset .tres next to that resource -- no
	// scene, no node, nothing to replace.
	void open_for_mesh_resource(const String &p_mesh_path);

	PhysXBlastFractureDialog();
};

#endif // GODOT_PHYSX_BLAST
