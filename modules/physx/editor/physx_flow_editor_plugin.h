/**************************************************************************/
/*  physx_flow_editor_plugin.h                                            */
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

#include "editor/plugins/editor_plugin.h"
#include "editor/scene/3d/gizmos/gizmo_3d_helper.h"
#include "editor/scene/3d/node_3d_editor_gizmos.h"

#ifdef GODOT_PHYSX_FLOW

// Viewport gizmo for PhysXFlowEmitter3D: a sphere ring (or box outline) for
// the emitter shape plus an arrow for the emission velocity -- the same
// visual language as the fluid node's gizmo.
class PhysXFlowEmitter3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXFlowEmitter3DGizmoPlugin, EditorNode3DGizmoPlugin);

	Ref<Gizmo3DHelper> helper;

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXFlowEmitter3DGizmoPlugin();
};

// Viewport gizmo for PhysXFlowCollider3D: box outline / sphere rings for the
// collider shape (collision-emitter footprint the fluid will see).
class PhysXFlowCollider3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXFlowCollider3DGizmoPlugin, EditorNode3DGizmoPlugin);

	Ref<Gizmo3DHelper> helper;

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXFlowCollider3DGizmoPlugin();
};

// Viewport gizmo for PhysXFlowSimulation3D: once the simulation has allocated
// sparse blocks, outlines the ACTIVE volume bounds (readback frame) so the
// sparse footprint is visible for authoring/debugging. Draws nothing before
// the first step (a Flow grid has no fixed domain to show).
class PhysXFlowSimulation3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXFlowSimulation3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXFlowSimulation3DGizmoPlugin();
};

// Registers the Flow gizmo plugins (separate from PhysXEditorPlugin so the
// Flow tooling lifecycle follows the flow=yes build flag).
class PhysXFlowEditorPlugin : public EditorPlugin {
	GDCLASS(PhysXFlowEditorPlugin, EditorPlugin);

public:
	PhysXFlowEditorPlugin();
};

#endif // GODOT_PHYSX_FLOW
