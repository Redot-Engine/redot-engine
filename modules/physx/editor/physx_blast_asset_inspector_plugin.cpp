/**************************************************************************/
/*  physx_blast_asset_inspector_plugin.cpp                                */
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
/*  physx_blast_asset_inspector_plugin.cpp                              */
//////////////////////////////////////////////////////////////////////////
/*                         This file is part of:                          */
/*                             REDOT ENGINE                               */
/*                        https://redotengine.org                         */
/* Copyright (c) 2024-present Redot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */

#include "physx_blast_asset_inspector_plugin.h"

#ifdef GODOT_PHYSX_BLAST

#include "../blast/physx_blast_asset.h"
#include "physx_blast_preview.h"

#include "editor/scene/3d/mesh_editor_plugin.h"

bool EditorInspectorPluginPhysXBlastAsset::can_handle(Object *p_object) {
	return Object::cast_to<PhysXBlastAsset>(p_object) != nullptr;
}

void EditorInspectorPluginPhysXBlastAsset::parse_begin(Object *p_object) {
	PhysXBlastAsset *asset = Object::cast_to<PhysXBlastAsset>(p_object);
	if (!asset) {
		return;
	}
	Ref<PhysXBlastAsset> a(asset);

	MeshEditor *editor = memnew(MeshEditor);
	editor->edit(physx_blast_build_preview_mesh(a));
	add_custom_control(editor);
}

#endif // GODOT_PHYSX_BLAST
