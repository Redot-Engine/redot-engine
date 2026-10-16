/**************************************************************************/
/*  physx_editor_plugin.h                                                 */
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

#ifdef GODOT_PHYSX_BLAST
class PhysXBlastFractureDialog;
#endif

// Viewport gizmo for PhysXParticleFluid3D: a wireframe box for the spawn region
// (with drag handles), a ring for the emission radius and an arrow for the
// emission velocity.
class PhysXParticleFluid3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXParticleFluid3DGizmoPlugin, EditorNode3DGizmoPlugin);

	Ref<Gizmo3DHelper> helper;

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	String get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const override;
	Variant get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const override;
	void begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) override;
	void set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) override;
	void commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) override;

	PhysXParticleFluid3DGizmoPlugin();
};

// Viewport gizmo for PhysXCloth3D: the rest grid outline with drag handles for
// its size, dots on the pinned vertices, and an arrow for the wind direction.
class PhysXCloth3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXCloth3DGizmoPlugin, EditorNode3DGizmoPlugin);

	Ref<Gizmo3DHelper> helper;

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	String get_handle_name(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const override;
	Variant get_handle_value(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) const override;
	void begin_handle_action(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary) override;
	void set_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, Camera3D *p_camera, const Point2 &p_point) override;
	void commit_handle(const EditorNode3DGizmo *p_gizmo, int p_id, bool p_secondary, const Variant &p_restore, bool p_cancel) override;

	PhysXCloth3DGizmoPlugin();
};

// Viewport gizmo for PhysXGas3D: a wireframe box for domain_size, centered on
// the node -- otherwise the domain is entirely invisible until Play. Read-only
// (no drag handles yet; resize via the inspector's domain_size property).
class PhysXGas3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXGas3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXGas3DGizmoPlugin();
};

// Viewport gizmo for PhysXGasEmitter3D: a wireframe sphere or box matching
// [member shape], plus an arrow for the injected velocity direction -- read
// only, same scope as PhysXGas3DGizmoPlugin.
class PhysXGasEmitter3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXGasEmitter3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXGasEmitter3DGizmoPlugin();
};

// Viewport gizmo for PhysXVehicleWheel3D: a wireframe disc for the wheel
// radius at the node's own (authored rest) position, plus a forward arrow
// from the ground-contact point -- lets a scene author judge wheel size and
// placement directly in the editor.
class PhysXVehicleWheel3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXVehicleWheel3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXVehicleWheel3DGizmoPlugin();
};

#ifdef GODOT_PHYSX_BLAST
// Viewport gizmo for PhysXDestructible3D: registers chunk 0's real
// per-triangle collision geometry for viewport click-to-select (the same
// mechanism MeshInstance3DGizmoPlugin uses) and draws the dashed selection
// box outline manually -- the node deliberately isn't a VisualInstance3D
// (see its class doc comment), so the editor won't draw one on its own.
class PhysXDestructible3DGizmoPlugin : public EditorNode3DGizmoPlugin {
	GDCLASS(PhysXDestructible3DGizmoPlugin, EditorNode3DGizmoPlugin);

public:
	bool has_gizmo(Node3D *p_spatial) override;
	String get_gizmo_name() const override;
	int get_priority() const override;
	bool is_selectable_when_hidden() const override;
	void redraw(EditorNode3DGizmo *p_gizmo) override;

	PhysXDestructible3DGizmoPlugin();
};
#endif

class PhysXEditorPlugin : public EditorPlugin {
	GDCLASS(PhysXEditorPlugin, EditorPlugin);

#ifdef GODOT_PHYSX_BLAST
	// Owned as a child of the editor's GUI base; both context-menu plugins
	// target this one shared dialog instance.
	PhysXBlastFractureDialog *blast_fracture_dialog = nullptr;
#endif

public:
	PhysXEditorPlugin();
};
