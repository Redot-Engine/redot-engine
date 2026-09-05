/**************************************************************************/
/*  menu.cpp                                                              */
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

#include "menu.h"

#include "menu/baker.h"
#include "menu/channel_packer.h"
#include "menu/directory_setup.h"

#include "../worldscape_3d.h"
#include "worldscape_3d_editor.h"

WorldScape3DMenu::WorldScape3DMenu(WorldScape3DEditorPlugin *plugin) :
		_plugin{ plugin }, _baker{ memnew(Baker(plugin)) }, _dir_setup{ memnew(DirectorySetup(plugin)) }, _packer_dialog(memnew(ChannelPackerDialog)) {
	set_flat(false);
	set_theme_type_variation("FlatMenuButton");
	set_text(TTR("Terrain"));
	set_switch_on_hover(true);
	set_visible(false);

	PopupMenu *p = get_popup();
	p->add_item("Directory Setup...", MENU_DIRECTORY_SETUP);
	p->add_item("Pack Textures...", MENU_PACK_TEXTURES); // TODO channel packer
	p->add_separator("", MENU_SEPARATOR);
	p->add_item("Bake ArrayMesh...", MENU_BAKE_ARRAY_MESH);
	p->add_item("Bake Occluder3D...", MENU_BAKE_OCCLUDER);
	p->add_separator("", MENU_SEPARATOR2);
	p->add_item("Set up Navigation...", MENU_SET_UP_NAVIGATION);
	p->add_item("Bake NavMesh...", MENU_BAKE_NAV_MESH);

	add_child(_baker);
	add_child(_packer_dialog);
	add_child(_dir_setup);

	p->connect("id_pressed", callable_mp(this, &WorldScape3DMenu::on_menu_entry));
}

WorldScape3DMenu::~WorldScape3DMenu() {
	_baker->queue_free();
	_packer_dialog->queue_free();
	_dir_setup->queue_free();
}

void WorldScape3DMenu::pressed() {
	on_menu_about_to_popup();
	MenuButton::pressed();
}

void WorldScape3DMenu::on_menu_entry(const int id) const {
	switch (id) {
		case MENU_DIRECTORY_SETUP:
			_dir_setup->directory_setup_popup();
			break;
		case MENU_PACK_TEXTURES:
			_packer_dialog->popup_centered();
			break;
		case MENU_BAKE_ARRAY_MESH:
			_baker->bake_mesh_popup();
			break;
		case MENU_BAKE_OCCLUDER:
			_baker->bake_occluder_popup();
			break;
		case MENU_SET_UP_NAVIGATION:
			_baker->set_up_navigation_popup();
			break;
		case MENU_BAKE_NAV_MESH:
			_baker->bake_nav_mesh();
			break;
		default:
			break; // TODO
	}
}

void WorldScape3DMenu::on_menu_about_to_popup() const {
	const auto terrain = _plugin->get_terrain();

	PopupMenu *p = get_popup();
	p->set_item_disabled(MENU_DIRECTORY_SETUP, !terrain);
	p->set_item_disabled(MENU_PACK_TEXTURES, !terrain);
	p->set_item_disabled(MENU_BAKE_ARRAY_MESH, !terrain);
	p->set_item_disabled(MENU_BAKE_OCCLUDER, !terrain);

	if (terrain) {
		const auto nav_regions = _baker->find_terrain_nav_regions(terrain);
		p->set_item_disabled(MENU_BAKE_NAV_MESH, nav_regions.is_empty());
		p->set_item_disabled(MENU_SET_UP_NAVIGATION, !nav_regions.is_empty());
	} else if (_plugin->get_nav_region()) {
		const auto terrains = _baker->find_nav_region_terrains(_plugin->get_nav_region());
		p->set_item_disabled(MENU_BAKE_NAV_MESH, terrains.is_empty());
		p->set_item_disabled(MENU_SET_UP_NAVIGATION, true);
	} else {
		p->set_item_disabled(MENU_BAKE_NAV_MESH, true);
		p->set_item_disabled(MENU_SET_UP_NAVIGATION, true);
	}
}
