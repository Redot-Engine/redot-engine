/**************************************************************************/
/*  baker.h                                                               */
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

#pragma once

#include "scene/gui/dialogs.h"
#include "scene/gui/spin_box.h"
#include "scene/main/node.h"

class NavigationRegion3D;
class WorldScape3D;
class WorldScape3DEditorPlugin;

class BakerLODDialog final : public ConfirmationDialog {
	GDCLASS(BakerLODDialog, ConfirmationDialog);

	int _lod = 0;
	String _description;

	MarginContainer *_margin = nullptr;
	VBoxContainer *_vbox = nullptr;
	HBoxContainer *_hbox = nullptr;
	Label *_label = nullptr;
	SpinBox *_lodbox = nullptr;
	Label *_description_label = nullptr;

	void setup_layout();

	void init();

	void on_about_to_popup();
	void on_visibility_changed();
	void on_lod_box_value_changed(real_t value);

public:
	~BakerLODDialog() override;

	void set_description(const String &description) { _description = description; }

	int get_lod() const { return _lod; }

	void _notification(int what);
};

class Baker : public Node {
	GDCLASS(Baker, Node);

	WorldScape3DEditorPlugin *_plugin = nullptr;
	Callable _bake_method;
	BakerLODDialog *_bake_lod_dlg = nullptr;
	ConfirmationDialog *_confirm_dlg = nullptr;

	void popup(const String &descr, const Callable &method);

	void bake_mesh();
	void bake_occluder();
	void bake_nav_region_nav_mesh(NavigationRegion3D *nav_region);

	void set_up_navigation();

	void on_confirm();

public:
	explicit Baker(WorldScape3DEditorPlugin *plugin);
	~Baker() override;

	Vector<WorldScape3D *> find_nav_region_terrains(NavigationRegion3D *nav_region) const;
	Vector<NavigationRegion3D *> find_terrain_nav_regions(WorldScape3D *terrain) const;

	void do_set_up_navigation(NavigationRegion3D *nav_region, WorldScape3D *terrain);
	void undo_set_up_navigation(NavigationRegion3D *nav_region, WorldScape3D *terrain);

	void bake_mesh_popup();
	void bake_occluder_popup();
	void set_up_navigation_popup();
	void bake_nav_mesh();

	void set_bake_method(const Callable &method) { _bake_method = method; }

	static void _bind_methods();
};
