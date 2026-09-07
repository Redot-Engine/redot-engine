/**************************************************************************/
/*  test_editor_cleanup.h                                                 */
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

#ifdef TOOLS_ENABLED

#include "modules/worldscape_3d/editor/menu/baker.h"
#include "modules/worldscape_3d/editor/ui/worldscape_3d_asset_dock.h"
#include "modules/worldscape_3d/editor/ui/worldscape_3d_tools.h"
#include "scene/resources/image_texture.h"
#include "tests/test_macros.h"

namespace TestWorldScape3DEditor {

TEST_CASE("[SceneTree][WorldScape3D] Brush material and shader are released") {
	WorldScape3DToolSettings *settings = memnew(WorldScape3DToolSettings(nullptr));
	Ref<ShaderMaterial> material = settings->get_brush_preview_material();
	const ObjectID material_id = material->get_instance_id();
	const ObjectID shader_id = material->get_shader()->get_instance_id();
	material.unref();
	CHECK(ObjectDB::get_instance(material_id) != nullptr);
	memdelete(settings);
	CHECK(ObjectDB::get_instance(material_id) == nullptr);
	CHECK(ObjectDB::get_instance(shader_id) == nullptr);
}

TEST_CASE("[SceneTree][WorldScape3D] Asset entry controls are released synchronously") {
	for (WorldScape3DAssets::AssetType type : { WorldScape3DAssets::TYPE_TEXTURE, WorldScape3DAssets::TYPE_MESH }) {
		const int before = ObjectDB::get_object_count();
		ListEntry *entry = memnew(ListEntry(type));
		CHECK(ObjectDB::get_object_count() > before);
		memdelete(entry);
		CHECK(ObjectDB::get_object_count() == before);
	}
}

TEST_CASE("[SceneTree][WorldScape3D] Detached baker dialogs are released synchronously") {
	// Warm the shared font cache used when the dialog's text is first shaped.
	Baker *warmup = memnew(Baker(nullptr));
	memdelete(warmup);
	const int before = ObjectDB::get_object_count();
	Baker *baker = memnew(Baker(nullptr));
	CHECK(ObjectDB::get_object_count() > before);
	memdelete(baker);
	CHECK(ObjectDB::get_object_count() == before);
}

TEST_CASE("[SceneTree][WorldScape3D] Populated asset entries release their resources") {
	for (bool enter_tree : { false, true }) {
		for (WorldScape3DAssets::AssetType type : { WorldScape3DAssets::TYPE_TEXTURE, WorldScape3DAssets::TYPE_MESH }) {
			ListEntry *entry = memnew(ListEntry(type));
			Ref<WorldScape3DAssetResource> asset;
			ObjectID texture_id;
			if (type == WorldScape3DAssets::TYPE_TEXTURE) {
				Ref<WorldScape3DTextureAsset> texture_asset;
				texture_asset.instantiate();
				Ref<ImageTexture> texture = ImageTexture::create_from_image(Image::create_empty(4, 4, true, Image::FORMAT_RGBA8));
				texture_id = texture->get_instance_id();
				texture_asset->set_albedo_texture(texture);
				asset = texture_asset;
			} else {
				asset = memnew(WorldScape3DMeshAsset);
			}
			const ObjectID asset_id = asset->get_instance_id();
			entry->set_edited_resource(asset);
			asset.unref();
			if (enter_tree) {
				SceneTree::get_singleton()->get_root()->add_child(entry);
			}
			CHECK(ObjectDB::get_instance(asset_id) != nullptr);
			if (texture_id.is_valid()) {
				CHECK(ObjectDB::get_instance(texture_id) != nullptr);
			}
			memdelete(entry);
			CHECK(ObjectDB::get_instance(asset_id) == nullptr);
			if (texture_id.is_valid()) {
				CHECK(ObjectDB::get_instance(texture_id) == nullptr);
			}
		}
	}
}

} // namespace TestWorldScape3DEditor

#endif // TOOLS_ENABLED
