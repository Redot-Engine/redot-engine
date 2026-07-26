/**************************************************************************/
/*  menu.h                                                                */
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

#include "scene/gui/menu_button.h"

class WorldScape3DEditorPlugin;

class Baker;
class DirectorySetup;
class ChannelPackerDialog;

class WorldScape3DMenu final : public MenuButton {
	GDCLASS(WorldScape3DMenu, MenuButton);

	WorldScape3DEditorPlugin *_plugin = nullptr;
	Baker *_baker = nullptr;
	DirectorySetup *_dir_setup = nullptr;
	ChannelPackerDialog *_packer_dialog = nullptr;

public:
	enum MenuOption {
		MENU_DIRECTORY_SETUP,
		MENU_PACK_TEXTURES,
		MENU_SEPARATOR,
		MENU_BAKE_ARRAY_MESH,
		MENU_BAKE_OCCLUDER,
		MENU_SEPARATOR2,
		MENU_SET_UP_NAVIGATION,
		MENU_BAKE_NAV_MESH,
	};

	explicit WorldScape3DMenu(WorldScape3DEditorPlugin *plugin);
	~WorldScape3DMenu() override;

	void pressed() override;

private:
	void on_menu_entry(int id) const;
	void on_menu_about_to_popup() const;
};
