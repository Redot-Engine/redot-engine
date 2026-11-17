/**************************************************************************/
/*  physx_cloth_paint_plugin.h                                            */
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

class Button;
class HBoxContainer;
class MeshInstance3D;
class PhysXSkinnedCloth3D;
class SpinBox;
class StandardMaterial3D;

// Paints PhysXSkinnedCloth3D::max_distances in the 3D viewport, like cloth
// painting in other engines: select the node, toggle "Paint Cloth", and the
// source mesh shows a heat map of each vertex's max distance (blue pinned,
// red free). LMB paints toward the brush value, Shift+LMB toward 0 (pinned),
// Ctrl+LMB smooths. Fill sets every vertex; Ramp starts from the height
// ramp (pin_height / max_distance). Each stroke is one undo step.
//
// Ported from the reference module's editor toolset; the
// node-side data path (max_distances / effective / height ramp) was already
// in place.
class PhysXClothPaintPlugin : public EditorPlugin {
	GDCLASS(PhysXClothPaintPlugin, EditorPlugin);

	PhysXSkinnedCloth3D *cloth = nullptr;

	HBoxContainer *toolbar = nullptr;
	Button *paint_button = nullptr;
	SpinBox *value_spin = nullptr;
	SpinBox *radius_spin = nullptr;
	SpinBox *strength_spin = nullptr;

	MeshInstance3D *overlay = nullptr;
	ObjectID hidden_source; // source mesh hidden (render instance only, not the property) while painting
	Ref<StandardMaterial3D> overlay_material;

	// Source surface in world space (skinned to the skeleton's current pose)
	// and the values being painted.
	PackedVector3Array world_verts;
	PackedVector3Array world_normals;
	PackedInt32Array indices;
	PackedFloat32Array values;
	PackedFloat32Array stroke_before;
	bool stroking = false;

	void _paint_toggled(bool p_on);
	void _fill();
	void _ramp();
	bool _prepare();
	void _clear_overlay();
	void _update_overlay();
	float _scale_max() const;
	void _commit(const String &p_action, const PackedFloat32Array &p_before);
	bool _raycast(Camera3D *p_camera, const Vector2 &p_screen, Vector3 &r_hit) const;
	void _apply_brush(const Vector3 &p_center, int p_mode);
	void _refresh_overlay();

protected:
	static void _bind_methods();

public:
	virtual String get_plugin_name() const override { return "PhysXClothPaint"; }
	virtual bool has_main_screen() const override { return false; }
	virtual void edit(Object *p_object) override;
	virtual bool handles(Object *p_object) const override;
	virtual void make_visible(bool p_visible) override;
	virtual EditorPlugin::AfterGUIInput forward_3d_gui_input(Camera3D *p_camera, const Ref<InputEvent> &p_event) override;

	PhysXClothPaintPlugin();
};
