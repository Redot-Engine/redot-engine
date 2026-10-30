/**************************************************************************/
/*  editor_node.cpp                                                       */
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

/**
 * @file editor_node.cpp
 *
 * [Add any documentation that applies to the entire file here!]
 */

#include "editor_node.h"

#include "core/config/project_settings.h"
#include "core/extension/gdextension_manager.h"
#include "core/input/input.h"
#include "core/io/config_file.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/os/keyboard.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "core/string/print_string.h"
#include "core/string/translation_server.h"
#include "core/version.h"
#include "editor/editor_scene_manager.h"
#include "editor/editor_string_names.h"
#include "editor/import/resource_importer_sprite_frames.h"
#ifndef DISABLE_DEPRECATED
#include "editor/import/resource_importer_animated_texture.h"
#endif
#include "editor/import/resource_importer_image_frames.h"
#include "editor/inspector/editor_context_menu_plugin.h"
#include "main/main.h"
#include "scene/2d/node_2d.h"
#include "scene/3d/bone_attachment_3d.h"
#include "scene/animation/animation_tree.h"
#include "scene/gui/color_picker.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/menu_bar.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/popup.h"
#include "scene/gui/rich_text_label.h"
#include "scene/gui/split_container.h"
#include "scene/gui/tab_container.h"
#include "scene/main/window.h"
#include "scene/property_utils.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/portable_compressed_texture.h"
#include "scene/theme/theme_db.h"
#include "servers/display/display_server.h"
#include "servers/navigation_2d/navigation_server_2d.h"
#include "servers/navigation_3d/navigation_server_3d.h"
#include "servers/rendering/rendering_server.h"

#include "editor/animation/animation_player_editor_plugin.h"
#include "editor/asset_library/asset_library_editor_plugin.h"
#include "editor/audio/audio_stream_preview.h"
#include "editor/audio/editor_audio_buses.h"
#include "editor/debugger/debugger_editor_plugin.h"
#include "editor/debugger/editor_debugger_node.h"
#include "editor/debugger/script_editor_debugger.h"
#include "editor/doc/editor_help.h"
#include "editor/docks/editor_dock_manager.h"
#include "editor/docks/filesystem_dock.h"
#include "editor/docks/history_dock.h"
#include "editor/docks/import_dock.h"
#include "editor/docks/inspector_dock.h"
#include "editor/docks/node_dock.h"
#include "editor/docks/scene_tree_dock.h"
#include "editor/editor_data.h"
#include "editor/editor_interface.h"
#include "editor/editor_log.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/export/dedicated_server_export_plugin.h"
#include "editor/export/editor_export.h"
#include "editor/export/export_template_manager.h"
#include "editor/export/gdextension_export_plugin.h"
#include "editor/export/project_export.h"
#include "editor/export/project_zip_packer.h"
#include "editor/export/register_exporters.h"
#include "editor/export/shader_baker_export_plugin.h"
#include "editor/file_system/dependency_editor.h"
#include "editor/file_system/editor_paths.h"
#include "editor/file_system/uid_viewer.h"
#include "editor/gui/editor_about.h"
#include "editor/gui/editor_bottom_panel.h"
#include "editor/gui/editor_file_dialog.h"
#include "editor/gui/editor_quick_open_dialog.h"
#include "editor/gui/editor_title_bar.h"
#include "editor/gui/editor_toaster.h"
#include "editor/gui/progress_dialog.h"
#include "editor/gui/window_wrapper.h"
#include "editor/import/3d/editor_import_collada.h"
#include "editor/import/3d/resource_importer_obj.h"
#include "editor/import/3d/resource_importer_scene.h"
#include "editor/import/3d/scene_import_settings.h"
#include "editor/import/audio_stream_import_settings.h"
#include "editor/import/dynamic_font_import_settings.h"
#include "editor/import/fbx_importer_manager.h"
#include "editor/import/resource_importer_bitmask.h"
#include "editor/import/resource_importer_bmfont.h"
#include "editor/import/resource_importer_csv_translation.h"
#include "editor/import/resource_importer_dynamic_font.h"
#include "editor/import/resource_importer_image.h"
#include "editor/import/resource_importer_imagefont.h"
#include "editor/import/resource_importer_layered_texture.h"
#include "editor/import/resource_importer_shader_file.h"
#include "editor/import/resource_importer_svg.h"
#include "editor/import/resource_importer_texture.h"
#include "editor/import/resource_importer_texture_atlas.h"
#include "editor/import/resource_importer_wav.h"
#include "editor/inspector/editor_inspector.h"
#include "editor/inspector/editor_preview_plugins.h"
#include "editor/inspector/editor_properties.h"
#include "editor/inspector/editor_property_name_processor.h"
#include "editor/inspector/editor_resource_picker.h"
#include "editor/inspector/editor_resource_preview.h"
#include "editor/inspector/multi_node_edit.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/plugins/plugin_config_dialog.h"
#include "editor/project_upgrade/project_upgrade_tool.h"
#include "editor/run/editor_run.h"
#include "editor/run/editor_run_bar.h"
#include "editor/run/game_view_plugin.h"
#include "editor/scene/3d/mesh_library_editor_plugin.h"
#include "editor/scene/3d/node_3d_editor_plugin.h"
#include "editor/scene/3d/root_motion_editor_plugin.h"
#include "editor/scene/canvas_item_editor_plugin.h"
#include "editor/scene/editor_scene_tabs.h"
#include "editor/scene/material_editor_plugin.h"
#include "editor/scene/particle_process_material_editor_plugin.h"
#include "editor/script/editor_script.h"
#include "editor/script/script_text_editor.h"
#include "editor/script/text_editor.h"
#include "editor/settings/editor_build_profile.h"
#include "editor/settings/editor_command_palette.h"
#include "editor/settings/editor_feature_profile.h"
#include "editor/settings/editor_layouts_dialog.h"
#include "editor/settings/editor_settings.h"
#include "editor/settings/editor_settings_dialog.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/shader/editor_native_shader_source_visualizer.h"
#include "editor/shader/visual_shader_editor_plugin.h"
#include "editor/themes/editor_scale.h"
#include "editor/themes/editor_theme_manager.h"
#include "editor/translations/editor_translation_parser.h"
#include "editor/translations/packed_scene_translation_parser_plugin.h"
#include "editor/version_control/version_control_editor_plugin.h"

#ifdef VULKAN_ENABLED
#include "editor/shader/shader_baker/shader_baker_export_plugin_platform_vulkan.h"
#endif

#ifdef D3D12_ENABLED
#include "editor/shader/shader_baker/shader_baker_export_plugin_platform_d3d12.h"
#endif

#ifdef METAL_ENABLED
#include "editor/shader/shader_baker/shader_baker_export_plugin_platform_metal.h"
#endif

#include "modules/modules_enabled.gen.h" // For gdscript, mono.

#ifndef PHYSICS_2D_DISABLED
#include "servers/physics_2d/physics_server_2d.h"
#endif // PHYSICS_2D_DISABLED

#ifndef PHYSICS_3D_DISABLED
#include "servers/physics_3d/physics_server_3d.h"
#endif // PHYSICS_3D_DISABLED

#ifdef ANDROID_ENABLED
#include "editor/gui/touch_actions_panel.h"
#endif // ANDROID_ENABLED

#include <cstdlib>

EditorNode *EditorNode::singleton = nullptr;

static const String EDITOR_NODE_CONFIG_SECTION = "EditorNode";

static const String REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE = TTRC("The Android build template is already installed in this project and it won't be overwritten.\nRemove the \"%s\" directory manually before attempting this operation again.");
static const String INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE = TTRC("This will set up your project for gradle Android builds by installing the source template to \"%s\".\nNote that in order to make gradle builds instead of using pre-built APKs, the \"Use Gradle Build\" option should be enabled in the Android export preset.");

void EditorNode::_version_control_menu_option(int p_idx) {
	switch (vcs_actions_menu->get_item_id(p_idx)) {
		case VCS_METADATA: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_metadata_dialog();
		} break;
		case VCS_SETTINGS: {
			VersionControlEditorPlugin::get_singleton()->popup_vcs_set_up_dialog(gui_base);
		} break;
	}
}

void EditorNode::_update_title() {
	const String appname = GLOBAL_GET("application/config/name");
	String title = (appname.is_empty() ? TTR("Unnamed Project") : appname);
	const String edited = get_editor_data().get_edited_scene_root() ? get_editor_data().get_edited_scene_root()->get_scene_file_path() : String();
	if (!edited.is_empty()) {
		// Display the edited scene name before the program name so that it can be seen in the OS task bar.
		title = vformat("%s - %s", edited.get_file(), title);
	}
	if (EditorSceneManager::get_singleton()->unsaved_cache) {
		// Display the "modified" mark before anything else so that it can always be seen in the OS task bar.
		title = vformat("(*) %s", title);
	}
	DisplayServer::get_singleton()->window_set_title(title + String(" - ") + REDOT_VERSION_NAME);
	if (project_title) {
		project_title->set_text(title);
	}
}

void EditorNode::input(const Ref<InputEvent> &p_event) {
	// EditorNode::get_singleton()->set_process_input is set to true in ProgressDialog only when the progress dialog is visible.
	// We need to discard all key events to disable all shortcuts while the progress
	// dialog is displayed, simulating an exclusive popup. Mouse events are
	// captured by a full-screen container in front of the EditorNode in ProgressDialog,
	// allowing interaction with the actual dialog where a Cancel button may be visible.
	Ref<InputEventKey> k = p_event;
	if (k.is_valid()) {
		get_tree()->get_root()->set_input_as_handled();
	}
}

void EditorNode::shortcut_input(const Ref<InputEvent> &p_event) {
	ERR_FAIL_COND(p_event.is_null());

	Ref<InputEventKey> k = p_event;
	if ((k.is_valid() && k->is_pressed() && !k->is_echo()) || Object::cast_to<InputEventShortcut>(*p_event)) {
		bool is_handled = true;
		if (ED_IS_SHORTCUT("editor/filter_files", p_event)) {
			FileSystemDock::get_singleton()->focus_on_filter();
		} else if (ED_IS_SHORTCUT("editor/editor_2d", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_2D);
		} else if (ED_IS_SHORTCUT("editor/editor_3d", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_3D);
		} else if (ED_IS_SHORTCUT("editor/editor_script", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_SCRIPT);
		} else if (ED_IS_SHORTCUT("editor/editor_game", p_event)) {
			editor_main_screen->select(EditorMainScreen::EDITOR_GAME);
		} else if (ED_IS_SHORTCUT("editor/editor_help", p_event)) {
			emit_signal(SNAME("request_help_search"), "");
		} else if (ED_IS_SHORTCUT("editor/editor_assetlib", p_event) && AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->select(EditorMainScreen::EDITOR_ASSETLIB);
		} else if (ED_IS_SHORTCUT("editor/editor_next", p_event)) {
			editor_main_screen->select_next();
		} else if (ED_IS_SHORTCUT("editor/editor_prev", p_event)) {
			editor_main_screen->select_prev();
		} else if (ED_IS_SHORTCUT("editor/command_palette", p_event)) {
			_open_command_palette();
		} else if (ED_IS_SHORTCUT("editor/toggle_last_opened_bottom_panel", p_event)) {
			bottom_panel->toggle_last_opened_bottom_panel();
		} else {
			is_handled = false;
		}

		if (is_handled) {
			get_tree()->get_root()->set_input_as_handled();
		}
	}
}

void EditorNode::_update_vsync_mode() {
	const DisplayServer::VSyncMode window_vsync_mode = DisplayServer::VSyncMode(int(EDITOR_GET("interface/editor/vsync_mode")));
	DisplayServer::get_singleton()->window_set_vsync_mode(window_vsync_mode);
}

void EditorNode::_update_from_settings() {
	if (!is_inside_tree()) {
		return;
	}
	_update_title();

	int current_filter = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_filter");
	if (current_filter != scene_root->get_default_canvas_item_texture_filter()) {
		Viewport::DefaultCanvasItemTextureFilter tf = (Viewport::DefaultCanvasItemTextureFilter)current_filter;
		scene_root->set_default_canvas_item_texture_filter(tf);
	}
	int current_repeat = GLOBAL_GET("rendering/textures/canvas_textures/default_texture_repeat");
	if (current_repeat != scene_root->get_default_canvas_item_texture_repeat()) {
		Viewport::DefaultCanvasItemTextureRepeat tr = (Viewport::DefaultCanvasItemTextureRepeat)current_repeat;
		scene_root->set_default_canvas_item_texture_repeat(tr);
	}
	String current_fallback_locale = GLOBAL_GET("internationalization/locale/fallback");
	if (current_fallback_locale != TranslationServer::get_singleton()->get_fallback_locale()) {
		TranslationServer::get_singleton()->set_fallback_locale(current_fallback_locale);
		scene_root->propagate_notification(Control::NOTIFICATION_LAYOUT_DIRECTION_CHANGED);
	}

	RS::DOFBokehShape dof_shape = RS::DOFBokehShape(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_shape")));
	RS::get_singleton()->camera_attributes_set_dof_blur_bokeh_shape(dof_shape);
	RS::DOFBlurQuality dof_quality = RS::DOFBlurQuality(int(GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_bokeh_quality")));
	bool dof_jitter = GLOBAL_GET("rendering/camera/depth_of_field/depth_of_field_use_jitter");
	RS::get_singleton()->camera_attributes_set_dof_blur_quality(dof_quality, dof_jitter);
	RS::get_singleton()->environment_set_ssao_quality(RS::EnvironmentSSAOQuality(int(GLOBAL_GET("rendering/environment/ssao/quality"))), GLOBAL_GET("rendering/environment/ssao/half_size"), GLOBAL_GET("rendering/environment/ssao/adaptive_target"), GLOBAL_GET("rendering/environment/ssao/blur_passes"), GLOBAL_GET("rendering/environment/ssao/fadeout_from"), GLOBAL_GET("rendering/environment/ssao/fadeout_to"));
	RS::get_singleton()->screen_space_roughness_limiter_set_active(GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/enabled"), GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/amount"), GLOBAL_GET("rendering/anti_aliasing/screen_space_roughness_limiter/limit"));
	bool glow_bicubic = int(GLOBAL_GET("rendering/environment/glow/upscale_mode")) > 0;
	RS::get_singleton()->environment_set_ssil_quality(RS::EnvironmentSSILQuality(int(GLOBAL_GET("rendering/environment/ssil/quality"))), GLOBAL_GET("rendering/environment/ssil/half_size"), GLOBAL_GET("rendering/environment/ssil/adaptive_target"), GLOBAL_GET("rendering/environment/ssil/blur_passes"), GLOBAL_GET("rendering/environment/ssil/fadeout_from"), GLOBAL_GET("rendering/environment/ssil/fadeout_to"));
	RS::get_singleton()->environment_glow_set_use_bicubic_upscale(glow_bicubic);
	RS::EnvironmentSSRRoughnessQuality ssr_roughness_quality = RS::EnvironmentSSRRoughnessQuality(int(GLOBAL_GET("rendering/environment/screen_space_reflection/roughness_quality")));
	RS::get_singleton()->environment_set_ssr_roughness_quality(ssr_roughness_quality);
	RS::SubSurfaceScatteringQuality sss_quality = RS::SubSurfaceScatteringQuality(int(GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_quality")));
	RS::get_singleton()->sub_surface_scattering_set_quality(sss_quality);
	float sss_scale = GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_scale");
	float sss_depth_scale = GLOBAL_GET("rendering/environment/subsurface_scattering/subsurface_scattering_depth_scale");
	RS::get_singleton()->sub_surface_scattering_set_scale(sss_scale, sss_depth_scale);

	uint32_t directional_shadow_size = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/size");
	uint32_t directional_shadow_16_bits = GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/16_bits");
	RS::get_singleton()->directional_shadow_atlas_set_size(directional_shadow_size, directional_shadow_16_bits);

	RS::ShadowQuality shadows_quality = RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/positional_shadow/soft_shadow_filter_quality")));
	RS::get_singleton()->positional_soft_shadow_filter_set_quality(shadows_quality);
	RS::ShadowQuality directional_shadow_quality = RS::ShadowQuality(int(GLOBAL_GET("rendering/lights_and_shadows/directional_shadow/soft_shadow_filter_quality")));
	RS::get_singleton()->directional_soft_shadow_filter_set_quality(directional_shadow_quality);
	float probe_update_speed = GLOBAL_GET("rendering/lightmapping/probe_capture/update_speed");
	RS::get_singleton()->lightmap_set_probe_capture_update_speed(probe_update_speed);
	RS::EnvironmentSDFGIFramesToConverge frames_to_converge = RS::EnvironmentSDFGIFramesToConverge(int(GLOBAL_GET("rendering/global_illumination/sdfgi/frames_to_converge")));
	RS::get_singleton()->environment_set_sdfgi_frames_to_converge(frames_to_converge);
	RS::EnvironmentSDFGIRayCount ray_count = RS::EnvironmentSDFGIRayCount(int(GLOBAL_GET("rendering/global_illumination/sdfgi/probe_ray_count")));
	RS::get_singleton()->environment_set_sdfgi_ray_count(ray_count);
	RS::VoxelGIQuality voxel_gi_quality = RS::VoxelGIQuality(int(GLOBAL_GET("rendering/global_illumination/voxel_gi/quality")));
	RS::get_singleton()->voxel_gi_set_quality(voxel_gi_quality);
	RS::get_singleton()->environment_set_volumetric_fog_volume_size(GLOBAL_GET("rendering/environment/volumetric_fog/volume_size"), GLOBAL_GET("rendering/environment/volumetric_fog/volume_depth"));
	RS::get_singleton()->environment_set_volumetric_fog_filter_active(bool(GLOBAL_GET("rendering/environment/volumetric_fog/use_filter")));
	RS::get_singleton()->canvas_set_shadow_texture_size(GLOBAL_GET("rendering/2d/shadow_atlas/size"));

	bool use_half_res_gi = GLOBAL_GET("rendering/global_illumination/gi/use_half_resolution");
	RS::get_singleton()->gi_set_use_half_resolution(use_half_res_gi);

	bool snap_2d_transforms = GLOBAL_GET("rendering/2d/snap/snap_2d_transforms_to_pixel");
	scene_root->set_snap_2d_transforms_to_pixel(snap_2d_transforms);
	bool snap_2d_vertices = GLOBAL_GET("rendering/2d/snap/snap_2d_vertices_to_pixel");
	scene_root->set_snap_2d_vertices_to_pixel(snap_2d_vertices);

	Viewport::SDFOversize sdf_oversize = Viewport::SDFOversize(int(GLOBAL_GET("rendering/2d/sdf/oversize")));
	scene_root->set_sdf_oversize(sdf_oversize);
	Viewport::SDFScale sdf_scale = Viewport::SDFScale(int(GLOBAL_GET("rendering/2d/sdf/scale")));
	scene_root->set_sdf_scale(sdf_scale);

	Viewport::MSAA msaa = Viewport::MSAA(int(GLOBAL_GET("rendering/anti_aliasing/quality/msaa_2d")));
	scene_root->set_msaa_2d(msaa);

	// 2D doesn't use a dedicated SubViewport like 3D does, so we apply it on the root viewport instead.
	bool use_debanding = GLOBAL_GET("rendering/anti_aliasing/quality/use_debanding");
	scene_root->set_use_debanding(use_debanding);
	get_viewport()->set_use_debanding(use_debanding);

	bool use_hdr_2d = GLOBAL_GET("rendering/viewport/hdr_2d");
	scene_root->set_use_hdr_2d(use_hdr_2d);
	get_viewport()->set_use_hdr_2d(use_hdr_2d);

	float mesh_lod_threshold = GLOBAL_GET("rendering/mesh_lod/lod_change/threshold_pixels");
	scene_root->set_mesh_lod_threshold(mesh_lod_threshold);

	RS::get_singleton()->decals_set_filter(RS::DecalFilter(int(GLOBAL_GET("rendering/textures/decals/filter"))));
	RS::get_singleton()->light_projectors_set_filter(RS::LightProjectorFilter(int(GLOBAL_GET("rendering/textures/light_projectors/filter"))));
	RS::get_singleton()->lightmaps_set_bicubic_filter(GLOBAL_GET("rendering/lightmapping/lightmap_gi/use_bicubic_filter"));

	SceneTree *tree = get_tree();
	tree->set_debug_collisions_color(GLOBAL_GET("debug/shapes/collision/shape_color"));
	tree->set_debug_collision_contact_color(GLOBAL_GET("debug/shapes/collision/contact_color"));

	ResourceImporterTexture::get_singleton()->update_imports();

	EditorCore::get_singleton()->_update_translations();

#ifdef DEBUG_ENABLED
	NavigationServer2D::get_singleton()->set_debug_navigation_edge_connection_color(GLOBAL_GET("debug/shapes/navigation/2d/edge_connection_color"));
	NavigationServer2D::get_singleton()->set_debug_navigation_geometry_edge_color(GLOBAL_GET("debug/shapes/navigation/2d/geometry_edge_color"));
	NavigationServer2D::get_singleton()->set_debug_navigation_geometry_face_color(GLOBAL_GET("debug/shapes/navigation/2d/geometry_face_color"));
	NavigationServer2D::get_singleton()->set_debug_navigation_geometry_edge_disabled_color(GLOBAL_GET("debug/shapes/navigation/2d/geometry_edge_disabled_color"));
	NavigationServer2D::get_singleton()->set_debug_navigation_geometry_face_disabled_color(GLOBAL_GET("debug/shapes/navigation/2d/geometry_face_disabled_color"));
	NavigationServer2D::get_singleton()->set_debug_navigation_enable_edge_connections(GLOBAL_GET("debug/shapes/navigation/2d/enable_edge_connections"));
	NavigationServer2D::get_singleton()->set_debug_navigation_enable_edge_lines(GLOBAL_GET("debug/shapes/navigation/2d/enable_edge_lines"));
	NavigationServer2D::get_singleton()->set_debug_navigation_enable_geometry_face_random_color(GLOBAL_GET("debug/shapes/navigation/2d/enable_geometry_face_random_color"));

	NavigationServer3D::get_singleton()->set_debug_navigation_edge_connection_color(GLOBAL_GET("debug/shapes/navigation/3d/edge_connection_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_edge_color(GLOBAL_GET("debug/shapes/navigation/3d/geometry_edge_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_face_color(GLOBAL_GET("debug/shapes/navigation/3d/geometry_face_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_edge_disabled_color(GLOBAL_GET("debug/shapes/navigation/3d/geometry_edge_disabled_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_geometry_face_disabled_color(GLOBAL_GET("debug/shapes/navigation/3d/geometry_face_disabled_color"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_connections(GLOBAL_GET("debug/shapes/navigation/3d/enable_edge_connections"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_connections_xray(GLOBAL_GET("debug/shapes/navigation/3d/enable_edge_connections_xray"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_lines(GLOBAL_GET("debug/shapes/navigation/3d/enable_edge_lines"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_edge_lines_xray(GLOBAL_GET("debug/shapes/navigation/3d/enable_edge_lines_xray"));
	NavigationServer3D::get_singleton()->set_debug_navigation_enable_geometry_face_random_color(GLOBAL_GET("debug/shapes/navigation/3d/enable_geometry_face_random_color"));
#endif // DEBUG_ENABLED
}

void EditorNode::_update_theme(bool p_skip_creation) {
	Ref<Theme> theme;
	if (!p_skip_creation) {
		theme = EditorThemeManager::generate_theme(EditorCore::get_singleton()->get_editor_theme());
		EditorCore::get_singleton()->set_editor_theme(theme);
		DisplayServer::set_early_window_clear_color_override(true, theme->get_color(SNAME("background"), EditorStringName(Editor)));
	} else {
		theme = EditorCore::get_singleton()->get_editor_theme();
	}

	Vector<Ref<Theme>> editor_themes;
	editor_themes.push_back(theme);
	editor_themes.push_back(ThemeDB::get_singleton()->get_default_theme());

	ThemeContext *node_tc = ThemeDB::get_singleton()->get_theme_context(this);
	if (node_tc) {
		node_tc->set_themes(editor_themes);
	} else {
		ThemeDB::get_singleton()->create_theme_context(this, editor_themes);
	}

	Window *window = get_window();
	if (window) {
		ThemeContext *window_tc = ThemeDB::get_singleton()->get_theme_context(window);
		if (window_tc) {
			window_tc->set_themes(editor_themes);
		} else {
			ThemeDB::get_singleton()->create_theme_context(window, editor_themes);
		}
	}

	if (CanvasItemEditor::get_singleton()->get_theme_preview() == CanvasItemEditor::THEME_PREVIEW_EDITOR) {
		EditorCore::get_singleton()->update_preview_themes(CanvasItemEditor::THEME_PREVIEW_EDITOR);
	}

	// Update styles.
	{
		bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
		bool dark_mode = DisplayServer::get_singleton()->is_dark_mode_supported() && DisplayServer::get_singleton()->is_dark_mode();

		gui_base->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("Background"), EditorStringName(EditorStyles)));
		main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, theme->get_constant(SNAME("window_border_margin"), EditorStringName(Editor)));
		main_vbox->add_theme_constant_override("separation", theme->get_constant(SNAME("top_bar_separation"), EditorStringName(Editor)));

		if (main_menu_button != nullptr) {
			main_menu_button->set_button_icon(theme->get_icon(SNAME("TripleBar"), EditorStringName(EditorIcons)));
		}

		editor_main_screen->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("Content"), EditorStringName(EditorStyles)));
		bottom_panel->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("BottomPanel"), EditorStringName(EditorStyles)));
		distraction_free->set_button_icon(theme->get_icon(SNAME("DistractionFree"), EditorStringName(EditorIcons)));
		distraction_free->add_theme_style_override(SceneStringName(pressed), theme->get_stylebox(CoreStringName(normal), "FlatMenuButton"));

		help_menu->set_item_icon(help_menu->get_item_index(HELP_SEARCH), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("HelpSearch"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_COPY_SYSTEM_INFO), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("ActionCopy"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_ABOUT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Godot"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_SUPPORT_REDOT_DEVELOPMENT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Heart"), global_menu, dark_mode));

		if (EditorDebuggerNode::get_singleton()->is_visible()) {
			bottom_panel->add_theme_style_override(SceneStringName(panel), theme->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles)));
		}

		_update_renderer_color();
	}

	editor_dock_manager->update_tab_styles();
	editor_dock_manager->update_docks_menu();
	editor_dock_manager->set_tab_icon_max_width(theme->get_constant(SNAME("class_icon_size"), EditorStringName(Editor)));
}

bool EditorNode::_is_project_data_missing() {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	const String project_data_dir = EditorPaths::get_singleton()->get_project_data_dir();
	if (!da->dir_exists(project_data_dir)) {
		return true;
	}

	String project_data_gdignore_file_path = project_data_dir.path_join(".gdignore");
	if (!FileAccess::exists(project_data_gdignore_file_path)) {
		Ref<FileAccess> f = FileAccess::open(project_data_gdignore_file_path, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_line("");
		} else {
			ERR_PRINT("Failed to create file " + project_data_gdignore_file_path.quote() + ".");
		}
	}

	String uid_cache = ResourceUID::get_singleton()->get_cache_file();
	if (!da->file_exists(uid_cache)) {
		Error err = ResourceUID::get_singleton()->save_to_cache();
		if (err != OK) {
			ERR_PRINT("Failed to create file " + uid_cache.quote() + ".");
		}
	}

	const String dirs[] = {
		EditorPaths::get_singleton()->get_project_settings_dir(),
		ProjectSettings::get_singleton()->get_imported_files_path()
	};
	for (const String &dir : dirs) {
		if (!da->dir_exists(dir)) {
			return true;
		}
	}
	return false;
}

void EditorNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_TRANSLATION_CHANGED: {
			_update_title();
			callable_mp(this, &EditorNode::_titlebar_resized).call_deferred();
		} break;

		case NOTIFICATION_POSTINITIALIZE: {
			EditorHelp::generate_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			EditorHelpHighlighter::create_singleton();
#endif
		} break;

		case NOTIFICATION_PROCESS: {
			if (get_editor_data().is_scene_changed(-1)) {
				scene_tabs->update_scene_tabs();
			}

			// Update the animation frame of the update spinner.
			uint64_t frame = Engine::get_singleton()->get_frames_drawn();
			uint64_t tick = OS::get_singleton()->get_ticks_msec();

			if (frame != update_spinner_step_frame && (tick - update_spinner_step_msec) > (1000 / 8)) {
				update_spinner_step++;
				if (update_spinner_step >= 8) {
					update_spinner_step = 0;
				}

				update_spinner_step_msec = tick;
				update_spinner_step_frame = frame + 1;

				// Update the icon itself only when the spinner is visible.
				if (_should_display_update_spinner()) {
					update_spinner->set_button_icon(EditorCore::get_singleton()->get_editor_theme()->get_icon("Progress" + itos(update_spinner_step + 1), EditorStringName(EditorIcons)));
				}
			}

			get_editor_selection()->update();
			// editor_selection->update();

			ResourceImporterTexture::get_singleton()->update_imports();

			if (requested_first_scan) {
				requested_first_scan = false;

				OS::get_singleton()->benchmark_begin_measure("Editor", "First Scan");

				EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorNode::_execute_upgrades), CONNECT_ONE_SHOT);
				EditorFileSystem::get_singleton()->scan();
			}

			if (EditorCore::get_singleton()->take_settings_overrides_changed()) {
				EditorSettings::get_singleton()->notify_changes();
				EditorSettings::get_singleton()->emit_signal(SNAME("settings_changed"));
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			get_tree()->set_disable_node_threading(true); // No node threading while running editor.

			Engine::get_singleton()->set_editor_hint(true);

			Window *window = get_window();
			if (window) {
				// Handle macOS fullscreen and extend-to-title changes.
				window->connect("titlebar_changed", callable_mp(this, &EditorNode::_titlebar_resized));
			}

			// Theme has already been created in the constructor, so we can skip that step.
			_update_theme(true);

			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));
			get_tree()->get_root()->set_as_audio_listener_3d(false);
			get_tree()->get_root()->set_as_audio_listener_2d(false);
			get_tree()->get_root()->set_snap_2d_transforms_to_pixel(false);
			get_tree()->get_root()->set_snap_2d_vertices_to_pixel(false);
			get_tree()->set_auto_accept_quit(false);
#ifdef ANDROID_ENABLED
			get_tree()->set_quit_on_go_back(false);
#endif
			get_tree()->get_root()->connect("files_dropped", callable_mp(this, &EditorNode::_dropped_files));

			EditorCore::get_singleton()->get_command_palette()->register_shortcuts_as_command();

			_begin_first_scan();

			last_dark_mode_state = DisplayServer::get_singleton()->is_dark_mode();
			last_system_accent_color = DisplayServer::get_singleton()->get_accent_color();
			last_system_base_color = DisplayServer::get_singleton()->get_base_color();
			DisplayServer::get_singleton()->set_system_theme_change_callback(callable_mp(this, &EditorNode::_check_system_theme_changed));

			get_viewport()->connect("size_changed", callable_mp(this, &EditorNode::_viewport_resized));

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case NOTIFICATION_EXIT_TREE: {
			// Remove print handler before children are destroyed to prevent
			// EditorLog from receiving messages during shutdown.
			EditorCore::get_singleton()->remove_print_handler_early();
			EditorPluginManager::get_singleton()->get_active_plugins().clear();

			if (progress_dialog) {
				progress_dialog->queue_free();
			}
			if (load_error_dialog) {
				load_error_dialog->queue_free();
			}
			if (execute_output_dialog) {
				execute_output_dialog->queue_free();
			}
			if (warning) {
				warning->queue_free();
			}
			if (accept) {
				accept->queue_free();
			}
			if (save_accept) {
				save_accept->queue_free();
			}
			EditorHelp::save_script_doc_cache();
			get_editor_data().save_editor_external_data();
			EditorCore::get_singleton()->log->deinit(); // Do not get messages anymore.
			get_editor_data().clear_edited_scenes();
			get_editor_data().save_editor_external_data();
			get_editor_data().clear_edited_scenes();
			get_viewport()->disconnect("size_changed", callable_mp(this, &EditorNode::_viewport_resized));
		} break;

		case NOTIFICATION_READY: {
			RenderingServer::get_singleton()->viewport_set_disable_2d(get_scene_root()->get_viewport_rid(), true);
			RenderingServer::get_singleton()->viewport_set_environment_mode(get_viewport()->get_viewport_rid(), RenderingServer::VIEWPORT_ENVIRONMENT_DISABLED);
			DisplayServer::get_singleton()->screen_set_keep_on(EDITOR_GET("interface/editor/keep_screen_on"));

			feature_profile_manager->notify_changed();

			// Save the project after opening to mark it as last modified, except in headless mode.
			if (DisplayServer::get_singleton()->window_can_draw()) {
				ProjectSettings::get_singleton()->save();
			}

			_titlebar_resized();

			// Set up a theme context for the 2D preview viewport using the stored preview theme.
			CanvasItemEditor::ThemePreviewMode theme_preview_mode = (CanvasItemEditor::ThemePreviewMode)(int)EditorSettings::get_singleton()->get_project_metadata("2d_editor", "theme_preview", CanvasItemEditor::THEME_PREVIEW_PROJECT);
			EditorCore::get_singleton()->update_preview_themes(theme_preview_mode);

			if (Engine::get_singleton()->is_recovery_mode_hint()) {
				EditorToaster::get_singleton()->popup_str(TTR("Recovery Mode is enabled. Editor functionality has been restricted."), EditorToaster::SEVERITY_WARNING);
			}

			/* DO NOT LOAD SCENES HERE, WAIT FOR FILE SCANNING AND REIMPORT TO COMPLETE */
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_IN: {
			// Restore the original FPS cap after focusing back on the editor.
			OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/low_processor_mode_sleep_usec")));

			if (_is_project_data_missing()) {
				project_data_missing->popup_centered();
			} else {
				EditorFileSystem::get_singleton()->scan_changes();
			}
			_scan_external_changes();

			GDExtensionManager *gdextension_manager = GDExtensionManager::get_singleton();
			callable_mp(gdextension_manager, &GDExtensionManager::reload_extensions).call_deferred();
		} break;

		case NOTIFICATION_APPLICATION_FOCUS_OUT: {
			// Save on focus loss before applying the FPS limit to avoid slowing down the saving process.
			if (EDITOR_GET("interface/editor/save_on_focus_loss")) {
				EditorSceneManager::get_singleton()->_save_scene_silently();
			}

			// Set a low FPS cap to decrease CPU/GPU usage while the editor is unfocused.
			if (unfocused_low_processor_usage_mode_enabled) {
				OS::get_singleton()->set_low_processor_usage_mode_sleep_usec(int(EDITOR_GET("interface/editor/unfocused_low_processor_mode_sleep_usec")));
			}
		} break;

		case NOTIFICATION_WM_ABOUT: {
			show_about();
		} break;

		case NOTIFICATION_WM_CLOSE_REQUEST: {
			_menu_option_confirm(SCENE_QUIT, false);
		} break;

		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("filesystem/file_dialog")) {
				FileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
				EditorFileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
				EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EDITOR_GET("filesystem/file_dialog/display_mode").operator int());
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/tablet_driver")) {
				String tablet_driver = GLOBAL_GET("input_devices/pen_tablet/driver");
				int tablet_driver_idx = EDITOR_GET("interface/editor/tablet_driver");
				if (tablet_driver_idx != -1) {
					tablet_driver = DisplayServer::get_singleton()->tablet_get_driver_name(tablet_driver_idx);
				}
				if (tablet_driver.is_empty()) {
					tablet_driver = DisplayServer::get_singleton()->tablet_get_driver_name(0);
				}
				DisplayServer::get_singleton()->tablet_set_current_driver(tablet_driver);
				print_verbose("Using \"" + DisplayServer::get_singleton()->tablet_get_current_driver() + "\" pen tablet driver...");
			}

			if (EDITOR_GET("interface/editor/import_resources_when_unfocused")) {
				scan_changes_timer->start();
			} else {
				scan_changes_timer->stop();
			}

			follow_system_theme = EDITOR_GET("interface/theme/follow_system_theme");
			use_system_accent_color = EDITOR_GET("interface/theme/use_system_accent_color");

			if (EditorThemeManager::is_generated_theme_outdated()) {
				EditorCore::get_singleton()->get_class_icon_cache().clear();
				_update_theme();
				EditorCore::get_singleton()->_build_icon_type_cache();
				recent_scenes->reset_size();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor/dock_tab_style")) {
				editor_dock_manager->update_tab_styles();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/scene_tabs")) {
				scene_tabs->update_scene_tabs();
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("docks/filesystem")) {
				if (EditorCore::get_singleton()->update_file_extension_caches()) {
					EditorFileSystem::get_singleton()->scan();
				}
			}

			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/editor")) {
				_update_update_spinner();
				_update_vsync_mode();
				_update_main_menu_type();
				DisplayServer::get_singleton()->screen_set_keep_on(EDITOR_GET("interface/editor/keep_screen_on"));
			}

#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("text_editor/theme/highlighting")) {
				EditorHelpHighlighter::get_singleton()->reset_cache();
			}
#endif
#ifdef ANDROID_ENABLED
			if (EditorSettings::get_singleton()->check_changed_settings_in_group("interface/touchscreen/touch_actions_panel")) {
				_touch_actions_panel_mode_changed();
			}
#endif
		} break;
	}
}

void EditorNode::_update_update_spinner() {
	update_spinner->set_visible(!RenderingServer::get_singleton()->canvas_item_get_debug_redraw() && _should_display_update_spinner());

	const bool update_continuously = EDITOR_GET("interface/editor/update_continuously");
	PopupMenu *update_popup = update_spinner->get_popup();
	update_popup->set_item_checked(update_popup->get_item_index(SPINNER_UPDATE_CONTINUOUSLY), update_continuously);
	update_popup->set_item_checked(update_popup->get_item_index(SPINNER_UPDATE_WHEN_CHANGED), !update_continuously);

	if (update_continuously) {
		update_spinner->set_tooltip_text(TTRC("Spins when the editor window redraws.\nUpdate Continuously is enabled, which can increase power usage. Click to disable it."));

		// Use a different color for the update spinner when Update Continuously is enabled,
		// as this feature should only be enabled for troubleshooting purposes.
		// Make the icon modulate color overbright because icons are not completely white on a dark theme.
		// On a light theme, icons are dark, so we need to modulate them with an even brighter color.
		const bool dark_theme = EditorThemeManager::is_dark_theme();
		update_spinner->set_self_modulate(EditorCore::get_singleton()->get_editor_theme()->get_color(SNAME("error_color"), EditorStringName(Editor)) * (dark_theme ? Color(1.1, 1.1, 1.1) : Color(4.25, 4.25, 4.25)));
	} else {
		update_spinner->set_tooltip_text(TTRC("Spins when the editor window redraws."));
		update_spinner->set_self_modulate(Color(1, 1, 1));
	}

	OS::get_singleton()->set_low_processor_usage_mode(!update_continuously);
}

void EditorNode::_execute_upgrades() {
	if (run_project_upgrade_tool) {
		run_project_upgrade_tool = false;
		// Execute another scan to reimport the modified files.
		project_upgrade_tool->connect(project_upgrade_tool->UPGRADE_FINISHED, callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan), CONNECT_ONE_SHOT);
		project_upgrade_tool->finish_upgrade();
	}
}

void EditorNode::_resources_changed(const Vector<String> &p_resources) {
	List<Ref<Resource>> changed;

	int rc = p_resources.size();
	for (int i = 0; i < rc; i++) {
		Ref<Resource> res = ResourceCache::get_ref(p_resources.get(i));
		if (res.is_null()) {
			continue;
		}

		if (!res->editor_can_reload_from_file()) {
			continue;
		}
		if (!res->get_path().is_resource_file() && !res->get_path().is_absolute_path()) {
			continue;
		}
		if (!FileAccess::exists(res->get_path())) {
			continue;
		}

		if (!res->get_import_path().is_empty()) {
			// This is an imported resource, will be reloaded if reimported via the _resources_reimported() callback.
			continue;
		}

		changed.push_back(res);
	}

	if (changed.size()) {
		for (Ref<Resource> &res : changed) {
			res->reload_from_file();
		}
	}
}

void EditorNode::_fs_changed() {
	EditorCore *core = EditorCore::get_singleton();
	for (FileDialog *E : core->get_file_dialogs()) {
		E->invalidate();
	}

	for (EditorFileDialog *E : core->get_editor_file_dialogs()) {
		E->invalidate();
	}

	EditorSceneManager::get_singleton()->_mark_unsaved_scenes();

	/// @todo FIXME: Move this to a cleaner location, it's hacky to do this in _fs_changed.
	String export_error;
	Error err = OK;
	// It's important to wait for the first scan to finish; otherwise, scripts or resources might not be imported.
	if (!export_defer.preset.is_empty() && !EditorFileSystem::get_singleton()->is_scanning()) {
		String preset_name = export_defer.preset;
		// Ensures export_project does not loop infinitely, because notifications may
		// come during the export.
		export_defer.preset = "";
		Ref<EditorExportPreset> export_preset;
		for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); ++i) {
			export_preset = EditorExport::get_singleton()->get_export_preset(i);
			if (export_preset->get_name() == preset_name) {
				break;
			}
			export_preset.unref();
		}

		if (export_preset.is_null()) {
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			if (da->file_exists("res://export_presets.cfg")) {
				err = FAILED;
				export_error = vformat(
						"Invalid export preset name: %s.\nThe following presets were detected in this project's `export_presets.cfg`:\n\n",
						preset_name);
				for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); ++i) {
					// Write the preset name between double quotes since it needs to be written between quotes on the command line if it contains spaces.
					export_error += vformat("        \"%s\"\n", EditorExport::get_singleton()->get_export_preset(i)->get_name());
				}
			} else {
				err = FAILED;
				export_error = "This project doesn't have an `export_presets.cfg` file at its root.\nCreate an export preset from the \"Project > Export\" dialog and try again.";
			}
		} else {
			Ref<EditorExportPlatform> platform = export_preset->get_platform();
			const String export_path = export_defer.path.is_empty() ? export_preset->get_export_path() : export_defer.path;
			if (export_path.is_empty()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a default export path, and none was specified.", preset_name);
			} else if (platform.is_null()) {
				err = FAILED;
				export_error = vformat("Export preset \"%s\" doesn't have a matching platform.", preset_name);
			} else {
				export_preset->update_value_overrides();
				if (export_defer.pack_only) { // Only export .pck or .zip data pack.
					if (export_path.ends_with(".zip")) {
						if (export_defer.patch) {
							err = platform->export_zip_patch(export_preset, export_defer.debug, export_path, export_defer.patches);
						} else {
							err = platform->export_zip(export_preset, export_defer.debug, export_path);
						}
					} else if (export_path.ends_with(".pck")) {
						if (export_defer.patch) {
							err = platform->export_pack_patch(export_preset, export_defer.debug, export_path, export_defer.patches);
						} else {
							err = platform->export_pack(export_preset, export_defer.debug, export_path);
						}
					} else {
						ERR_PRINT(vformat("Export path \"%s\" doesn't end with a supported extension.", export_path));
						err = FAILED;
					}
				} else { // Normal project export.
					String config_error;
					bool missing_templates;
					if (export_defer.android_build_template) {
						export_template_manager->install_android_template(export_preset);
					}
					if (!platform->can_export(export_preset, config_error, missing_templates, export_defer.debug)) {
						ERR_PRINT(vformat("Cannot export project with preset \"%s\" due to configuration errors:\n%s", preset_name, config_error));
						err = missing_templates ? ERR_FILE_NOT_FOUND : ERR_UNCONFIGURED;
					} else {
						platform->clear_messages();
						err = platform->export_project(export_preset, export_defer.debug, export_path);
					}
				}
				if (err != OK) {
					export_error = vformat("Project export for preset \"%s\" failed.", preset_name);
				} else if (platform->get_worst_message_type() >= EditorExportPlatform::EXPORT_MESSAGE_WARNING) {
					export_error = vformat("Project export for preset \"%s\" completed with warnings.", preset_name);
				}
			}
		}

		if (err != OK) {
			ERR_PRINT(export_error);
			_exit_editor(EXIT_FAILURE);
			return;
		}
		if (!export_error.is_empty()) {
			WARN_PRINT(export_error);
		}
		_exit_editor(EXIT_SUCCESS);
	}
}

void EditorNode::_sources_changed(bool p_exist) {
	if (waiting_for_first_scan) {
		waiting_for_first_scan = false;

		OS::get_singleton()->benchmark_end_measure("Editor", "First Scan");

		// Reload the global shader variables, but this time
		// loading textures, as they are now properly imported.
		RenderingServer::get_singleton()->global_shader_parameters_load_settings(true);

		_load_editor_layout_to_config();

		if (!EditorSceneManager::get_singleton()->defer_load_scene.is_empty()) {
			OS::get_singleton()->benchmark_begin_measure("Editor", "Load Scene");

			EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->defer_load_scene);
			EditorSceneManager::get_singleton()->defer_load_scene = "";

			OS::get_singleton()->benchmark_end_measure("Editor", "Load Scene");
			OS::get_singleton()->benchmark_dump();
		}

		// Start preview thread now that it's safe.
		if (!singleton->cmdline_mode) {
			EditorResourcePreview::get_singleton()->start();
		}
		get_tree()->create_timer(1.0f)->connect("timeout", callable_mp(this, &EditorNode::_remove_lock_file));
	}
}

void EditorNode::_remove_lock_file() {
	OS::get_singleton()->remove_lock_file();
}

void EditorNode::_scan_external_changes() {
	disk_changed_list->clear();
	TreeItem *r = disk_changed_list->create_item();
	disk_changed_list->set_hide_root(true);
	bool need_reload = false;

	disk_changed_scenes.clear();
	disk_changed_project = false;

	// Check if any edited scene has changed.
	for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);

		const String scene_path = get_editor_data().get_scene_path(i);

		if (scene_path == "" || !da->file_exists(scene_path)) {
			continue;
		}

		uint64_t last_date = get_editor_data().get_scene_modified_time(i);
		uint64_t date = FileAccess::get_modified_time(scene_path);

		if (date > last_date) {
			TreeItem *ti = disk_changed_list->create_item(r);
			ti->set_text(0, scene_path.get_file());
			need_reload = true;
			disk_changed_scenes.push_back(scene_path);
		}
	}

	String project_settings_path = ProjectSettings::get_singleton()->get_resource_path().path_join("project.godot");
	if (FileAccess::get_modified_time(project_settings_path) > ProjectSettings::get_singleton()->get_last_saved_time()) {
		TreeItem *ti = disk_changed_list->create_item(r);
		ti->set_text(0, "project.godot");
		need_reload = true;
		disk_changed_project = true;
	}

	if (need_reload) {
		callable_mp((Window *)disk_changed, &Window::popup_centered_ratio).call_deferred(0.3);
	}
}

void EditorNode::_resave_externally_modified_scenes(String p_str) {
	for (const String &scene_path : disk_changed_scenes) {
		EditorSceneManager::get_singleton()->_save_scene(scene_path);
	}

	if (disk_changed_project) {
		ProjectSettings::get_singleton()->save();
	}

	disk_changed->hide();
}

void EditorNode::_reload_modified_scenes() {
	int current_idx = get_editor_data().get_edited_scene();

	for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
		if (get_editor_data().get_scene_path(i) == "") {
			continue;
		}

		uint64_t last_date = get_editor_data().get_scene_modified_time(i);
		uint64_t date = FileAccess::get_modified_time(get_editor_data().get_scene_path(i));

		if (date > last_date) {
			String filename = get_editor_data().get_scene_path(i);
			get_editor_data().set_edited_scene(i);
			EditorSceneManager::get_singleton()->_remove_edited_scene(false);

			Error err = EditorSceneManager::get_singleton()->load_scene(filename, false, false, false, true);
			if (err != OK) {
				ERR_PRINT(vformat("Failed to load scene: %s", filename));
			}
			get_editor_data().move_edited_scene_to_index(i);
		}
	}

	EditorSceneManager::get_singleton()->_set_current_scene(current_idx);
	scene_tabs->update_scene_tabs();
	disk_changed->hide();
}

void EditorNode::_reload_project_settings() {
	ProjectSettings::get_singleton()->setup(ProjectSettings::get_singleton()->get_resource_path(), String(), true, true);
}

void EditorNode::_viewport_resized() {
	Window *w = get_window();
	if (w) {
		was_window_windowed_last = w->get_mode() == Window::MODE_WINDOWED;
	}
}

void EditorNode::_titlebar_resized() {
	DisplayServer::get_singleton()->window_set_window_buttons_offset(Vector2i(title_bar->get_global_position().y + title_bar->get_size().y / 2, title_bar->get_global_position().y + title_bar->get_size().y / 2), DisplayServer::MAIN_WINDOW_ID);
	const Vector3i &margin = DisplayServer::get_singleton()->window_get_safe_title_margins(DisplayServer::MAIN_WINDOW_ID);
	if (left_menu_spacer) {
		int w = (gui_base->is_layout_rtl()) ? margin.y : margin.x;
		left_menu_spacer->set_custom_minimum_size(Size2(w, 0));
	}
	if (right_menu_spacer) {
		int w = (gui_base->is_layout_rtl()) ? margin.x : margin.y;
		right_menu_spacer->set_custom_minimum_size(Size2(w, 0));
	}
	if (title_bar) {
		title_bar->set_custom_minimum_size(Size2(0, margin.z - title_bar->get_global_position().y));
	}
}

void EditorNode::_node_renamed() {
	if (InspectorDock::get_inspector_singleton()) {
		InspectorDock::get_inspector_singleton()->update_tree();
	}
}

void EditorNode::_open_command_palette() {
	EditorCore::get_singleton()->get_command_palette()->open_popup();
}

void EditorNode::_menu_option(int p_option) {
	_menu_option_confirm(p_option, false);
}

void EditorNode::_menu_confirm_current() {
	_menu_option_confirm(current_menu_option, true);
}

void EditorNode::trigger_menu_option(int p_option, bool p_confirmed) {
	_menu_option_confirm(p_option, p_confirmed);
}

void EditorNode::_dialog_display_save_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_FILE_CANT_WRITE: {
				show_accept(TTR("Can't open file for writing:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(TTR("Requested file format unknown:") + " " + p_file.get_extension(), TTR("OK"));
			} break;
			default: {
				show_accept(TTR("Error while saving."), TTR("OK"));
			} break;
		}
	}
}

void EditorNode::_dialog_display_load_error(String p_file, Error p_error) {
	if (p_error) {
		switch (p_error) {
			case ERR_CANT_OPEN: {
				show_accept(vformat(TTR("Can't open file '%s'. The file could have been moved or deleted."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_PARSE_ERROR: {
				show_accept(vformat(TTR("Error while parsing file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_CORRUPT: {
				show_accept(vformat(TTR("Scene file '%s' appears to be invalid/corrupt."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_NOT_FOUND: {
				show_accept(vformat(TTR("Missing file '%s' or one of its dependencies."), p_file.get_file()), TTR("OK"));
			} break;
			case ERR_FILE_UNRECOGNIZED: {
				show_accept(vformat(TTR("File '%s' is saved in a format that is newer than the formats supported by this version of Redot, so it can't be opened."), p_file.get_file()), TTR("OK"));
			} break;
			default: {
				show_accept(vformat(TTR("Error while loading file '%s'."), p_file.get_file()), TTR("OK"));
			} break;
		}
	}
}

void EditorNode::_find_node_types(Node *p_node, int &count_2d, int &count_3d) {
	if (p_node->is_class("Viewport") || (p_node != get_editor_data().get_edited_scene_root() && p_node->get_owner() != get_editor_data().get_edited_scene_root())) {
		return;
	}

	if (p_node->is_class("CanvasItem")) {
		count_2d++;
	} else if (p_node->is_class("Node3D")) {
		count_3d++;
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_find_node_types(p_node->get_child(i), count_2d, count_3d);
	}
}

void EditorNode::_save_scene_with_preview(String p_file, int p_idx) {
	save_scene_progress = memnew(EditorProgress("save", TTR("Saving Scene"), 4));

	if (get_editor_data().get_edited_scene_root() != nullptr) {
		save_scene_progress->step(TTR("Analyzing"), 0);

		int c2d = 0;
		int c3d = 0;

		_find_node_types(get_editor_data().get_edited_scene_root(), c2d, c3d);

		save_scene_progress->step(TTR("Creating Thumbnail"), 1);
		// Current view?

		Ref<Image> img;
		// If neither 3D or 2D nodes are present, make a 1x1 black texture.
		// We cannot fallback on the 2D editor, because it may not have been used yet,
		// which would result in an invalid texture.
		if (c3d == 0 && c2d == 0) {
			img.instantiate();
			img->initialize_data(1, 1, false, Image::FORMAT_RGB8);
		} else if (c3d < c2d) {
			Ref<ViewportTexture> viewport_texture = scene_root->get_texture();
			if (viewport_texture->get_width() > 0 && viewport_texture->get_height() > 0) {
				img = viewport_texture->get_image();
			}
		} else {
			// The 3D editor may be disabled as a feature, but scenes can still be opened.
			// This check prevents the preview from regenerating in case those scenes are then saved.
			// The preview will be generated if no feature profile is set (as the 3D editor is enabled by default).
			Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
			if (profile.is_null() || !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D)) {
				img = Node3DEditor::get_singleton()->get_editor_viewport(0)->get_viewport_node()->get_texture()->get_image();
			}
		}

		if (img.is_valid() && img->get_width() > 0 && img->get_height() > 0) {
			img = img->duplicate();

			save_scene_progress->step(TTR("Creating Thumbnail"), 3);

			int preview_size = EDITOR_GET("filesystem/file_dialog/thumbnail_size");
			preview_size *= EDSCALE;

			// Consider a square region.
			int vp_size = MIN(img->get_width(), img->get_height());
			int x = (img->get_width() - vp_size) / 2;
			int y = (img->get_height() - vp_size) / 2;

			if (vp_size < preview_size) {
				// Just square it.
				img->crop_from_point(x, y, vp_size, vp_size);
			} else {
				int ratio = vp_size / preview_size;
				int size = preview_size * MAX(1, ratio / 2);

				x = (img->get_width() - size) / 2;
				y = (img->get_height() - size) / 2;

				img->crop_from_point(x, y, size, size);
				img->resize(preview_size, preview_size, Image::INTERPOLATE_LANCZOS);
			}
			img->convert(Image::FORMAT_RGB8);

			// Save thumbnail directly, as thumbnailer may not update due to actual scene not changing md5.
			String temp_path = EditorPaths::get_singleton()->get_cache_dir();
			String cache_base = ProjectSettings::get_singleton()->globalize_path(p_file).md5_text();
			cache_base = temp_path.path_join("resthumb-" + cache_base);

			// Does not have it, try to load a cached thumbnail.
			post_process_preview(img);
			img->save_png(cache_base + ".png");
		}
	}

	save_scene_progress->step(TTR("Saving Scene"), 4);
	EditorSceneManager::get_singleton()->_save_scene(p_file, p_idx);

	if (!singleton->cmdline_mode) {
		EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
	}

	_close_save_scene_progress();
}

void EditorNode::_close_save_scene_progress() {
	memdelete_notnull(save_scene_progress);
	save_scene_progress = nullptr;
}

void EditorNode::restart_editor(bool p_goto_project_manager) {
	_menu_option_confirm(p_goto_project_manager ? PROJECT_QUIT_TO_PROJECT_MANAGER : PROJECT_RELOAD_CURRENT_PROJECT, false);
}

void EditorNode::_dialog_action(String p_file) {
	switch (current_menu_option) {
		case SCENE_NEW_INHERITED_SCENE: {
			Node *scene = get_editor_data().get_edited_scene_root();
			// If the previous scene is rootless, just close it in favor of the new one.
			if (!scene) {
				_menu_option_confirm(SCENE_CLOSE, true);
			}

			EditorSceneManager::get_singleton()->load_scene(p_file, false, true);
		} break;
		case SCENE_OPEN_SCENE: {
			EditorSceneManager::get_singleton()->load_scene(p_file);
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();
			/// @todo Would be nice to show the project manager opened with the highlighted field.

			project_run_bar->play_main_scene((bool)pick_main_scene->get_meta("from_native", false));
		} break;
		case SCENE_CLOSE:
		case SCENE_TAB_CLOSE:
		case SCENE_SAVE_SCENE:
		case SCENE_MULTI_SAVE_AS_SCENE:
		case SCENE_SAVE_AS_SCENE: {
			int scene_idx = (current_menu_option == SCENE_SAVE_SCENE ||
									current_menu_option == SCENE_SAVE_AS_SCENE ||
									current_menu_option == SCENE_MULTI_SAVE_AS_SCENE)
					? -1
					: EditorSceneManager::get_singleton()->tab_closing_idx;

			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				bool same_open_scene = false;
				for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
					if (get_editor_data().get_scene_path(i) == p_file && i != scene_idx) {
						same_open_scene = true;
					}
				}

				if (same_open_scene) {
					show_warning(TTR("Can't overwrite scene that is still open!"));
					return;
				}

				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file, scene_idx);
				_add_to_recent_scenes(p_file);
				save_editor_layout_delayed();

				if (scene_idx != -1) {
					EditorSceneManager::get_singleton()->_discard_changes();
				} else {
					// Update the path of the edited scene to ensure later do/undo action history matches.
					get_editor_data().set_scene_path(get_editor_data().get_edited_scene(), p_file);
				}
			}

			if (current_menu_option == SCENE_MULTI_SAVE_AS_SCENE) {
				EditorSceneManager::get_singleton()->_proceed_save_asing_scene_tabs();
			}

		} break;

		case SAVE_AND_RUN: {
			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file);
				project_run_bar->play_custom_scene(p_file);
			}
		} break;

		case SAVE_AND_RUN_MAIN_SCENE: {
			ProjectSettings::get_singleton()->set("application/run/main_scene", ResourceUID::path_to_uid(p_file));
			ProjectSettings::get_singleton()->save();

			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				EditorCore::get_singleton()->save_default_environment();
				_save_scene_with_preview(p_file);
				project_run_bar->play_main_scene((bool)pick_main_scene->get_meta("from_native", false));
			}
		} break;

		case FILE_EXPORT_MESH_LIBRARY: {
			const Dictionary &fd_options = file_export_lib->get_selected_options();
			bool merge_with_existing_library = fd_options.get(TTR("Merge With Existing"), true);
			bool apply_mesh_instance_transforms = fd_options.get(TTR("Apply MeshInstance Transforms"), false);

			Ref<MeshLibrary> ml;
			if (merge_with_existing_library && FileAccess::exists(p_file)) {
				ml = ResourceLoader::load(p_file, "MeshLibrary");

				if (ml.is_null()) {
					show_accept(TTR("Can't load MeshLibrary for merging!"), TTR("OK"));
					return;
				}
			}

			if (ml.is_null()) {
				ml.instantiate();
			}

			MeshLibraryEditor::update_library_file(get_editor_data().get_edited_scene_root(), ml, merge_with_existing_library, apply_mesh_instance_transforms);

			Error err = ResourceSaver::save(ml, p_file);
			if (err) {
				show_accept(TTR("Error saving MeshLibrary!"), TTR("OK"));
				return;
			} else if (ResourceCache::has(p_file)) {
				// Make sure MeshLibrary is updated in the editor.
				ResourceLoader::load(p_file)->reload_from_file();
			}

		} break;

		case PROJECT_PACK_AS_ZIP: {
			ProjectZIPPacker::pack_project_zip(p_file);
			{
				Ref<FileAccess> f = FileAccess::open(p_file, FileAccess::READ);
				ERR_FAIL_COND_MSG(f.is_null(), vformat("Unable to create ZIP file at: %s. Check for write permissions and whether you have enough disk space left.", p_file));
			}

		} break;

		case RESOURCE_SAVE:
		case RESOURCE_SAVE_AS: {
			ERR_FAIL_COND(EditorCore::get_singleton()->get_saving_resource().is_null());
			EditorCore::get_singleton()->save_resource_in_path(EditorCore::get_singleton()->get_saving_resource(), p_file);

			EditorCore::get_singleton()->set_saving_resource(Ref<Resource>());
			ObjectID current_id = get_editor_selection_history()->get_current();
			Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;
			ERR_FAIL_NULL(current_obj);
			current_obj->notify_property_list_changed();
		} break;
		case LAYOUT_SAVE: {
			if (p_file.is_empty()) {
				return;
			}

			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err == ERR_FILE_CANT_OPEN || err == ERR_FILE_NOT_FOUND) {
				config.instantiate();
			} else if (err != OK) {
				show_warning(TTR("An error occurred while trying to save the editor layout.\nMake sure the editor's user data path is writable."));
				return;
			}

			editor_dock_manager->save_docks_to_config(config, p_file);

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Default editor layout overridden.\nTo restore the Default layout to its base settings, use the Delete Layout option and delete the Default layout."));
			}

		} break;
		case LAYOUT_DELETE: {
			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());

			if (err != OK || !config->has_section(p_file)) {
				show_warning(TTR("Layout name not found!"));
				return;
			}

			// Erase key values.
			Vector<String> keys = config->get_section_keys(p_file);
			for (const String &key : keys) {
				config->set_value(p_file, key, Variant());
			}

			config->save(EditorSettings::get_singleton()->get_editor_layouts_config());

			layout_dialog->hide();
			_update_layouts_menu();

			if (p_file == "Default") {
				show_warning(TTR("Restored the Default layout to its base settings."));
			}

		} break;
		default: {
			// Save scene?
			if (file->get_file_mode() == EditorFileDialog::FILE_MODE_SAVE_FILE) {
				_save_scene_with_preview(p_file);
			}

		} break;
	}
}

bool EditorNode::_is_class_editor_disabled_by_feature_profile(const StringName &p_class) {
	Ref<EditorFeatureProfile> profile = EditorFeatureProfileManager::get_singleton()->get_current_profile();
	if (profile.is_null()) {
		return false;
	}

	StringName class_name = p_class;

	while (class_name != StringName()) {
		if (profile->is_class_disabled(class_name)) {
			return true;
		}
		if (profile->is_class_editor_disabled(class_name)) {
			return true;
		}
		class_name = ClassDB::get_parent_class(class_name);
	}

	return false;
}

static bool overrides_external_editor(Object *p_object) {
	Script *script = Object::cast_to<Script>(p_object);

	if (!script) {
		return false;
	}

	return script->get_language()->overrides_external_editor();
}

void EditorNode::_add_to_history(const Object *p_object, const String &p_property, bool p_inspector_only) {
	ObjectID id = p_object->get_instance_id();
	ObjectID history_id = get_editor_selection_history()->get_current();
	if (id != history_id) {
		const MultiNodeEdit *multi_node_edit = Object::cast_to<const MultiNodeEdit>(p_object);
		const MultiNodeEdit *history_multi_node_edit = ObjectDB::get_instance<MultiNodeEdit>(history_id);
		if (multi_node_edit && history_multi_node_edit && multi_node_edit->is_same_selection(history_multi_node_edit)) {
			return;
		}
		if (p_inspector_only) {
			get_editor_selection_history()->add_object(id, String(), true);
		} else if (p_property.is_empty()) {
			get_editor_selection_history()->add_object(id);
		} else {
			get_editor_selection_history()->add_object(id, p_property);
		}
	}
}

void EditorNode::_edit_current(bool p_skip_foreign, bool p_skip_inspector_update) {
	ObjectID current_id = get_editor_selection_history()->get_current();
	Object *current_obj = current_id.is_valid() ? ObjectDB::get_instance(current_id) : nullptr;

	Ref<Resource> res = Object::cast_to<Resource>(current_obj);
	if (p_skip_foreign && res.is_valid()) {
		const int current_tab = scene_tabs->get_current_tab();
		if (res->get_path().contains("::") && res->get_path().get_slice("::", 0) != get_editor_data().get_scene_path(current_tab)) {
			// Trying to edit resource that belongs to another scene; abort.
			current_obj = nullptr;
		}
	}

	bool inspector_only = get_editor_selection_history()->is_current_inspector_only();
	EditorSceneManager::get_singleton()->current = current_obj;

	if (!current_obj) {
		SceneTreeDock::get_singleton()->set_selected(nullptr);
		InspectorDock::get_inspector_singleton()->edit(nullptr);
		NodeDock::get_singleton()->set_node(nullptr);
		InspectorDock::get_singleton()->update(nullptr);
		EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
		EditorSceneManager::get_singleton()->hide_unused_editors();
		return;
	}

	// Update the use folding setting and state.
	bool disable_folding = bool(EDITOR_GET("interface/inspector/disable_folding")) || current_obj->is_class("EditorDebuggerRemoteObjects");
	if (InspectorDock::get_inspector_singleton()->is_using_folding() == disable_folding) {
		InspectorDock::get_inspector_singleton()->set_use_folding(!disable_folding, false);
	}

	bool is_resource = current_obj->is_class("Resource");
	bool is_node = current_obj->is_class("Node");
	bool stay_in_script_editor_on_node_selected = bool(EDITOR_GET("text_editor/behavior/navigation/stay_in_script_editor_on_node_selected"));
	bool skip_main_plugin = false;

	String editable_info; // None by default.
	bool info_is_warning = false;

	if (current_obj->has_method("_is_read_only")) {
		if (current_obj->call("_is_read_only")) {
			editable_info = TTR("This object is marked as read-only, so it's not editable.");
		}
	}

	if (is_resource) {
		Resource *current_res = Object::cast_to<Resource>(current_obj);
		ERR_FAIL_NULL(current_res);

		if (!p_skip_inspector_update) {
			InspectorDock::get_inspector_singleton()->edit(current_res);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			NodeDock::get_singleton()->set_node(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
			EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
			ImportDock::get_singleton()->set_edit_path(current_res->get_path());
		}

		int subr_idx = current_res->get_path().find("::");
		if (subr_idx != -1) {
			String base_path = current_res->get_path().substr(0, subr_idx);
			if (FileAccess::exists(base_path + ".import")) {
				if (!base_path.is_resource_file()) {
					if (EditorSceneManager::get_singleton()->get_edited_scene() && EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path() == base_path) {
						info_is_warning = true;
					}
				}
				editable_info = TTR("This resource belongs to a scene that was imported, so it's not editable.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
			} else if ((!EditorSceneManager::get_singleton()->get_edited_scene() ||
							   EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path() != base_path) &&
					ResourceLoader::get_resource_type(base_path) == "PackedScene") {
				editable_info = TTR("This resource belongs to a scene that was instantiated or inherited.\nChanges to it must be made inside the original scene.");
			}
		} else if (current_res->get_path().is_resource_file()) {
			if (FileAccess::exists(current_res->get_path() + ".import")) {
				editable_info = TTR("This resource was imported, so it's not editable. Change its settings in the import panel and then re-import.");
			}
		}
	} else if (is_node) {
		Node *current_node = Object::cast_to<Node>(current_obj);
		ERR_FAIL_NULL(current_node);

		InspectorDock::get_inspector_singleton()->edit(current_node);
		if (current_node->is_inside_tree()) {
			NodeDock::get_singleton()->set_node(current_node);
			SceneTreeDock::get_singleton()->set_selected(current_node);
			SceneTreeDock::get_singleton()->set_selection({ current_node });
			InspectorDock::get_singleton()->update(current_node);
			if (!inspector_only && !skip_main_plugin) {
				if (!ScriptEditor::get_singleton()->is_editor_floating() && ScriptEditor::get_singleton()->is_visible_in_tree()) {
					skip_main_plugin = stay_in_script_editor_on_node_selected;
				} else {
					skip_main_plugin = !editor_main_screen->can_auto_switch_screens();
				}
			}
		} else {
			NodeDock::get_singleton()->set_node(nullptr);
			SceneTreeDock::get_singleton()->set_selected(nullptr);
			InspectorDock::get_singleton()->update(nullptr);
		}
		EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();

		if (EditorSceneManager::get_singleton()->get_edited_scene() && !EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path().is_empty()) {
			String source_scene = EditorSceneManager::get_singleton()->get_edited_scene()->get_scene_file_path();
			if (FileAccess::exists(source_scene + ".import")) {
				editable_info = TTR("This scene was imported, so changes to it won't be kept.\nInstantiating or inheriting it will allow you to make changes to it.\nPlease read the documentation relevant to importing scenes to better understand this workflow.");
				info_is_warning = true;
			}
		}
	} else {
		Node *selected_node = nullptr;

		Vector<Node *> multi_nodes;
		if (current_obj->is_class("MultiNodeEdit")) {
			Node *scene = EditorSceneManager::get_singleton()->get_edited_scene();
			if (scene) {
				MultiNodeEdit *multi_node_edit = Object::cast_to<MultiNodeEdit>(current_obj);
				int node_count = multi_node_edit->get_node_count();
				if (node_count > 0) {
					for (int node_index = 0; node_index < node_count; ++node_index) {
						Node *node = scene->get_node(multi_node_edit->get_node(node_index));
						if (node) {
							multi_nodes.push_back(node);
						}
					}
					if (!multi_nodes.is_empty()) {
						// Pick the top-most node.
						multi_nodes.sort_custom<Node::Comparator>();
						selected_node = multi_nodes[0];
					}
				}
			}
		}

		if (!current_obj->is_class("EditorDebuggerRemoteObjects")) {
			EditorDebuggerNode::get_singleton()->clear_remote_tree_selection();
		}

		InspectorDock::get_inspector_singleton()->edit(current_obj);
		NodeDock::get_singleton()->set_node(nullptr);
		SceneTreeDock::get_singleton()->set_selected(selected_node);
		SceneTreeDock::get_singleton()->set_selection(multi_nodes);
		InspectorDock::get_singleton()->update(nullptr);
	}

	InspectorDock::get_singleton()->set_info(
			info_is_warning ? TTR("Changes may be lost!") : TTR("This object is read-only."),
			editable_info,
			info_is_warning);

	Object *editor_owner = (is_node || current_obj->is_class("MultiNodeEdit")) ? (Object *)SceneTreeDock::get_singleton() : is_resource ? (Object *)InspectorDock::get_inspector_singleton()
																																		: (Object *)this;

	// Take care of the main editor plugin.

	if (!inspector_only) {
		EditorPlugin *main_plugin = get_editor_data().get_handling_main_editor(current_obj);

		int plugin_index = editor_main_screen->get_plugin_index(main_plugin);
		if (main_plugin && plugin_index >= 0 && !editor_main_screen->is_button_enabled(plugin_index)) {
			main_plugin = nullptr;
		}
		EditorPlugin *editor_plugin_screen = editor_main_screen->get_selected_plugin();

		ObjectID editor_owner_id = editor_owner->get_instance_id();
		if (main_plugin && !skip_main_plugin) {
			// Special case if use of external editor is true.
			Resource *current_res = Object::cast_to<Resource>(current_obj);
			if (main_plugin->get_plugin_name() == "Script" && current_res && !current_res->is_built_in() && (bool(EDITOR_GET("text_editor/external/use_external_editor")) || overrides_external_editor(current_obj))) {
				if (!EditorSceneManager::get_singleton()->changing_scene) {
					main_plugin->edit(current_obj);
				}
			} else if (Object::cast_to<Script>(current_obj)) {
				editor_main_screen->select(plugin_index);
				main_plugin->edit(current_obj);
			} else if (main_plugin != editor_plugin_screen) {
				// Unedit previous plugin.
				editor_plugin_screen->edit(nullptr);
				EditorPluginManager::get_singleton()->get_active_plugins()[editor_owner_id].erase(editor_plugin_screen);
				// Update screen main_plugin.
				editor_main_screen->select(plugin_index);
				main_plugin->edit(current_obj);
			} else {
				editor_plugin_screen->edit(current_obj);
			}
			EditorPluginManager::get_singleton()->set_main_screen_editing(true);
		} else if (!main_plugin && editor_plugin_screen && EditorPluginManager::get_singleton()->is_main_screen_editing_flag()) {
			editor_plugin_screen->edit(nullptr);
			EditorPluginManager::get_singleton()->set_main_screen_editing(false);
		}

		EditorSceneManager::get_singleton()->edit_item(current_obj, editor_owner);
	}

	InspectorDock::get_singleton()->update(current_obj);
}

void EditorNode::_android_build_source_selected(const String &p_file) {
	export_template_manager->install_android_template_from_file(p_file, android_export_preset);
}

void EditorNode::_android_export_preset_selected(int p_index) {
	if (p_index >= 0) {
		android_export_preset = EditorExport::get_singleton()->get_export_preset(choose_android_export_profile->get_item_id(p_index));
	} else {
		android_export_preset.unref();
	}
	install_android_build_template_message->set_text(vformat(TTR(INSTALL_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
}

void EditorNode::_android_install_build_template() {
	gradle_build_manage_templates->hide();
	file_android_build_source->popup_centered_ratio();
}

void EditorNode::_android_explore_build_templates() {
	OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(export_template_manager->get_android_build_directory(android_export_preset).get_base_dir()), true);
}

void EditorNode::_menu_option_confirm(int p_option, bool p_confirmed) {
	if (!p_confirmed) { /// @todo FIXME: this may be a hack.
		current_menu_option = (MenuOptions)p_option;
	}

	switch (p_option) {
		case SCENE_NEW_SCENE: {
			EditorSceneManager::get_singleton()->new_scene();

		} break;
		case SCENE_NEW_INHERITED_SCENE:
		case SCENE_OPEN_SCENE: {
			file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = get_editor_data().get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_scene_file_path());
			};
			file->set_title(p_option == SCENE_OPEN_SCENE ? TTR("Open Scene") : TTR("Open Base Scene"));
			file->popup_file_dialog();

		} break;
		case SCENE_QUICK_OPEN: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "Resource" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case SCENE_QUICK_OPEN_SCENE: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "PackedScene" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case SCENE_QUICK_OPEN_SCRIPT: {
			EditorCore::get_singleton()->get_quick_open_dialog()->popup_dialog({ "Script" }, callable_mp(this, &EditorNode::_quick_opened));
		} break;
		case SCENE_OPEN_PREV: {
			if (!EditorSceneManager::get_singleton()->prev_closed_scenes.is_empty()) {
				EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->prev_closed_scenes.back()->get());
			}
		} break;
		case EditorSceneTabs::SCENE_CLOSE_OTHERS: {
			EditorSceneManager::get_singleton()->tab_closing_menu_option = -1;
			for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
				if (i == get_editor_data().get_edited_scene()) {
					continue;
				}
				EditorSceneManager::get_singleton()->tabs_to_close.push_back(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case EditorSceneTabs::SCENE_CLOSE_RIGHT: {
			EditorSceneManager::get_singleton()->tab_closing_menu_option = -1;
			for (int i = get_editor_data().get_edited_scene() + 1; i < get_editor_data().get_edited_scene_count(); i++) {
				EditorSceneManager::get_singleton()->tabs_to_close.push_back(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case EditorSceneTabs::SCENE_CLOSE_ALL: {
			EditorSceneManager::get_singleton()->tab_closing_menu_option = -1;
			for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
				EditorSceneManager::get_singleton()->tabs_to_close.push_back(get_editor_data().get_scene_path(i));
			}
			EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
		} break;
		case SCENE_CLOSE: {
			EditorSceneManager::get_singleton()->_scene_tab_closed(get_editor_data().get_edited_scene());
		} break;
		case SCENE_TAB_CLOSE:
		case SCENE_SAVE_SCENE: {
			int scene_idx = (p_option == SCENE_SAVE_SCENE) ? -1 : EditorSceneManager::get_singleton()->tab_closing_idx;
			Node *scene = get_editor_data().get_edited_scene_root(scene_idx);
			if (scene && !scene->get_scene_file_path().is_empty()) {
				if (DirAccess::exists(scene->get_scene_file_path().get_base_dir())) {
					if (scene_idx != get_editor_data().get_edited_scene()) {
						_save_scene_with_preview(scene->get_scene_file_path(), scene_idx);
					} else {
						_save_scene_with_preview(scene->get_scene_file_path());
					}

					if (scene_idx != -1) {
						EditorSceneManager::get_singleton()->_discard_changes();
					}
					save_editor_layout_delayed();
				} else {
					show_save_accept(vformat(TTR("%s no longer exists! Please specify a new save location."), scene->get_scene_file_path().get_base_dir()), TTR("OK"));
				}
				break;
			}
			[[fallthrough]];
		}
		case SCENE_MULTI_SAVE_AS_SCENE:
		case SCENE_SAVE_AS_SCENE: {
			int scene_idx = (p_option == SCENE_SAVE_SCENE ||
									p_option == SCENE_SAVE_AS_SCENE ||
									p_option == SCENE_MULTI_SAVE_AS_SCENE)
					? -1
					: EditorSceneManager::get_singleton()->tab_closing_idx;

			Node *scene = get_editor_data().get_edited_scene_root(scene_idx);

			if (!scene) {
				if (p_option == SCENE_SAVE_SCENE) {
					// Pressing Ctrl + S saves the current script if a scene is currently open, but it won't if the scene has no root node.
					// Work around this by explicitly saving the script in this case (similar to pressing Ctrl + Alt + S).
					ScriptEditor::get_singleton()->save_current_script();
				}

				const int saved = EditorSceneManager::get_singleton()->_save_external_resources(true);
				if (saved > 0) {
					EditorToaster::get_singleton()->popup_str(vformat(TTR("The current scene has no root node, but %d modified external resource(s) and/or plugin data were saved anyway."), saved), EditorToaster::SEVERITY_INFO);
				} else if (p_option == SCENE_SAVE_AS_SCENE) {
					// Don't show this dialog when pressing Ctrl + S to avoid interfering with script saving.
					show_accept(
							TTR("A root node is required to save the scene. You can add a root node using the Scene tree dock."),
							TTR("OK"));
				}

				break;
			}

			file->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);

			List<String> extensions;
			Ref<PackedScene> sd = memnew(PackedScene);
			ResourceSaver::get_recognized_extensions(sd, &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			if (!scene->get_scene_file_path().is_empty()) {
				String path = scene->get_scene_file_path();
				String root_name = EditorCore::adjust_scene_name_casing(scene->get_name());
				String ext = path.get_extension().to_lower();
				path = path.get_base_dir().path_join(root_name + "." + ext);

				file->set_current_path(path);
				if (extensions.size()) {
					if (extensions.find(ext) == nullptr) {
						file->set_current_path(path.replacen("." + ext, "." + extensions.front()->get()));
					}
				}
			} else if (extensions.size()) {
				String root_name = scene->get_name();
				root_name = EditorCore::adjust_scene_name_casing(root_name);
				file->set_current_path(root_name + "." + extensions.front()->get().to_lower());
			}
			file->set_title(TTR("Save Scene As..."));
			file->popup_file_dialog();

		} break;

		case SCENE_SAVE_ALL_SCENES: {
			EditorSceneManager::get_singleton()->_save_all_scenes();
		} break;

		case EditorSceneTabs::SCENE_RUN: {
			project_run_bar->play_current_scene();
		} break;

		case PROJECT_EXPORT: {
			EditorCore::get_singleton()->get_project_export()->popup_export();
		} break;

		case PROJECT_PACK_AS_ZIP: {
			String resource_path = ProjectSettings::get_singleton()->get_resource_path();
			const String base_path = resource_path.substr(0, resource_path.rfind_char('/')) + "/";

			file_pack_zip->set_current_path(base_path);
			file_pack_zip->set_current_file(ProjectZIPPacker::get_project_zip_safe_name());
			file_pack_zip->popup_file_dialog();
		} break;

		case SCENE_UNDO: {
			EditorLog *undo_log = EditorCore::get_singleton()->get_log();
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				undo_log->add_message(TTR("Can't undo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
				String action = undo_redo->get_current_action_name();
				int id = undo_redo->get_current_action_history_id();
				if (!undo_redo->undo()) {
					undo_log->add_message(TTR("Nothing to undo."), EditorLog::MSG_TYPE_EDITOR);
				} else if (!action.is_empty()) {
					switch (id) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							undo_log->add_message(vformat(TTR("Global Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							undo_log->add_message(vformat(TTR("Remote Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							undo_log->add_message(vformat(TTR("Scene Undo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
			EditorSceneManager::get_singleton()->_update_unsaved_cache();
		} break;
		case SCENE_REDO: {
			EditorUndoRedoManager *undo_redo = EditorUndoRedoManager::get_singleton();
			EditorLog *redo_log = EditorCore::get_singleton()->get_log();
			if ((int)Input::get_singleton()->get_mouse_button_mask() & 0x7) {
				redo_log->add_message(TTR("Can't redo while mouse buttons are pressed."), EditorLog::MSG_TYPE_EDITOR);
			} else {
				if (!undo_redo->redo()) {
					redo_log->add_message(TTR("Nothing to redo."), EditorLog::MSG_TYPE_EDITOR);
				} else {
					String action = undo_redo->get_current_action_name();
					if (action.is_empty()) {
						break;
					}

					switch (undo_redo->get_current_action_history_id()) {
						case EditorUndoRedoManager::GLOBAL_HISTORY:
							redo_log->add_message(vformat(TTR("Global Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						case EditorUndoRedoManager::REMOTE_HISTORY:
							redo_log->add_message(vformat(TTR("Remote Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
							break;
						default:
							redo_log->add_message(vformat(TTR("Scene Redo: %s"), action), EditorLog::MSG_TYPE_EDITOR);
					}
				}
			}
			EditorSceneManager::get_singleton()->_update_unsaved_cache();
		} break;

		case SCENE_RELOAD_SAVED_SCENE: {
			Node *scene = EditorSceneManager::get_singleton()->get_edited_scene();

			if (!scene) {
				break;
			}

			String scene_filename = scene->get_scene_file_path();
			String unsaved_message;

			if (scene_filename.is_empty()) {
				show_warning(TTR("Can't reload a scene that was never saved."));
				break;
			}

			if (EditorSceneManager::get_singleton()->unsaved_cache) {
				if (!p_confirmed) {
					confirmation->set_ok_button_text(TTRC("Save & Reload"));
					unsaved_message = EditorSceneManager::get_singleton()->_get_unsaved_scene_dialog_text(scene_filename, EditorCore::get_singleton()->get_started_timestamp());
					confirmation->set_text(unsaved_message + "\n\n" + TTR("Save before reloading the scene?"));
					confirmation->popup_centered();
					confirmation_button->show();
					confirmation_button->grab_focus();
					break;
				} else {
					_save_scene_with_preview(scene_filename);
				}
			}

			EditorSceneManager::get_singleton()->_discard_changes();
		} break;

		case EditorSceneTabs::SCENE_SHOW_IN_FILESYSTEM: {
			String path = get_editor_data().get_scene_path(get_editor_data().get_edited_scene());
			if (!path.is_empty()) {
				FileSystemDock::get_singleton()->navigate_to_path(path);
			}
		} break;

		case PROJECT_OPEN_SETTINGS: {
			EditorCore::get_singleton()->get_project_settings_editor()->popup_project_settings();
		} break;

		case PROJECT_FIND_IN_FILES: {
			ScriptEditor::get_singleton()->open_find_in_files_dialog("");
		} break;

		case PROJECT_INSTALL_ANDROID_SOURCE: {
			if (p_confirmed) {
				if (export_template_manager->is_android_template_installed(android_export_preset)) {
					remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
					remove_android_build_template->popup_centered();
				} else if (!export_template_manager->can_install_android_template(android_export_preset)) {
					gradle_build_manage_templates->popup_centered();
				} else {
					export_template_manager->install_android_template(android_export_preset);
				}
			} else {
				bool has_custom_gradle_build = false;
				choose_android_export_profile->clear();
				for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
					Ref<EditorExportPreset> export_preset = EditorExport::get_singleton()->get_export_preset(i);
					if (export_preset->get_platform()->get_class_name() == "EditorExportPlatformAndroid" && (bool)export_preset->get("gradle_build/use_gradle_build")) {
						choose_android_export_profile->add_item(export_preset->get_name(), i);
						String gradle_build_directory = export_preset->get("gradle_build/gradle_build_directory");
						String android_source_template = export_preset->get("gradle_build/android_source_template");
						if (!android_source_template.is_empty() || (gradle_build_directory != "" && gradle_build_directory != "res://android")) {
							has_custom_gradle_build = true;
						}
					}
				}
				_android_export_preset_selected(choose_android_export_profile->get_item_count() >= 1 ? 0 : -1);

				if (choose_android_export_profile->get_item_count() > 1 && has_custom_gradle_build) {
					// If there's multiple options and at least one of them uses a custom gradle build then prompt the user to choose.
					choose_android_export_profile->show();
					install_android_build_template->popup_centered();
				} else {
					choose_android_export_profile->hide();

					if (export_template_manager->is_android_template_installed(android_export_preset)) {
						remove_android_build_template->set_text(vformat(TTR(REMOVE_ANDROID_BUILD_TEMPLATE_MESSAGE), export_template_manager->get_android_build_directory(android_export_preset)));
						remove_android_build_template->popup_centered();
					} else if (export_template_manager->can_install_android_template(android_export_preset)) {
						install_android_build_template->popup_centered();
					} else {
						gradle_build_manage_templates->popup_centered();
					}
				}
			}
		} break;
		case PROJECT_OPEN_USER_DATA_FOLDER: {
			// Ensure_user_data_dir() to prevent the edge case: "Open User Data Folder" won't work after the project was renamed in ProjectSettingsEditor unless the project is saved.
			OS::get_singleton()->ensure_user_data_dir();
			OS::get_singleton()->shell_show_in_file_manager(OS::get_singleton()->get_user_data_dir(), true);
		} break;
		case SCENE_QUIT:
		case PROJECT_QUIT_TO_PROJECT_MANAGER:
		case PROJECT_RELOAD_CURRENT_PROJECT: {
			if (p_confirmed && plugin_to_save) {
				plugin_to_save->save_external_data();
				p_confirmed = false;
			}

			if (p_confirmed && stop_project_confirmation && project_run_bar->is_playing()) {
				project_run_bar->stop_playing();
				stop_project_confirmation = false;
				p_confirmed = false;
			}

			if (!p_confirmed) {
				if (!stop_project_confirmation && project_run_bar->is_playing()) {
					if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
						confirmation->set_text(TTR("Stop running project before reloading the current project?"));
						confirmation->set_ok_button_text(TTR("Stop & Reload"));
					} else {
						confirmation->set_text(TTR("Stop running project before exiting the editor?"));
						confirmation->set_ok_button_text(TTR("Stop & Quit"));
					}
					confirmation->reset_size();
					confirmation->popup_centered();
					confirmation_button->hide();
					stop_project_confirmation = true;
					break;
				}

				bool save_each = EDITOR_GET("interface/editor/save_each_scene_on_quit");
				if (EditorSceneManager::get_singleton()->_next_unsaved_scene(!save_each) == -1) {
					if (EditorUndoRedoManager::get_singleton()->is_history_unsaved(EditorUndoRedoManager::GLOBAL_HISTORY)) {
						if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
							save_confirmation->set_ok_button_text(TTR("Save & Reload"));
							save_confirmation->set_text(TTR("Save modified resources before reloading?"));
						} else {
							save_confirmation->set_ok_button_text(TTR("Save & Quit"));
							save_confirmation->set_text(TTR("Save modified resources before closing?"));
						}
						save_confirmation->reset_size();
						save_confirmation->popup_centered();
						break;
					}

					plugin_to_save = nullptr;
					for (int i = 0; i < get_editor_data().get_editor_plugin_count(); i++) {
						const String unsaved_status = get_editor_data().get_editor_plugin(i)->get_unsaved_status();
						if (!unsaved_status.is_empty()) {
							if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
								save_confirmation->set_ok_button_text(TTR("Save & Reload"));
								save_confirmation->set_text(unsaved_status);
							} else {
								save_confirmation->set_ok_button_text(TTR("Save & Quit"));
								save_confirmation->set_text(unsaved_status);
							}
							save_confirmation->reset_size();
							save_confirmation->popup_centered();
							plugin_to_save = get_editor_data().get_editor_plugin(i);
							break;
						}
					}

					if (plugin_to_save) {
						break;
					}

					EditorSceneManager::get_singleton()->_discard_changes();
					break;
				}

				if (save_each) {
					EditorSceneManager::get_singleton()->tab_closing_menu_option = current_menu_option;
					for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
						EditorSceneManager::get_singleton()->tabs_to_close.push_back(get_editor_data().get_scene_path(i));
					}
					EditorSceneManager::get_singleton()->_proceed_closing_scene_tabs();
				} else {
					String unsaved_scenes;
					int i = EditorSceneManager::get_singleton()->_next_unsaved_scene(true, 0);
					while (i != -1) {
						unsaved_scenes += "\n            " + get_editor_data().get_edited_scene_root(i)->get_scene_file_path();
						i = EditorSceneManager::get_singleton()->_next_unsaved_scene(true, ++i);
					}
					if (p_option == PROJECT_RELOAD_CURRENT_PROJECT) {
						save_confirmation->set_ok_button_text(TTR("Save & Reload"));
						save_confirmation->set_text(TTR("Save changes to the following scene(s) before reloading?") + unsaved_scenes);
					} else {
						save_confirmation->set_ok_button_text(TTR("Save & Quit"));
						save_confirmation->set_text((p_option == SCENE_QUIT ? TTR("Save changes to the following scene(s) before quitting?") : TTR("Save changes to the following scene(s) before opening Project Manager?")) + unsaved_scenes);
					}
					save_confirmation->reset_size();
					save_confirmation->popup_centered();
				}

				DisplayServer::get_singleton()->window_request_attention();
				break;
			}
			EditorSceneManager::get_singleton()->_save_external_resources();
			EditorSceneManager::get_singleton()->_discard_changes();
		} break;
		case SPINNER_UPDATE_CONTINUOUSLY: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", true);
			_update_update_spinner();
			show_accept(TTR("This option is deprecated. Situations where refresh must be forced are now considered a bug. Please report."), TTR("OK"));
		} break;
		case SPINNER_UPDATE_WHEN_CHANGED: {
			EditorSettings::get_singleton()->set("interface/editor/update_continuously", false);
			_update_update_spinner();
		} break;
		case SPINNER_UPDATE_SPINNER_HIDE: {
			EditorSettings::get_singleton()->set("interface/editor/show_update_spinner", 2); // Disabled
			_update_update_spinner();
		} break;
		case EDITOR_OPEN_SETTINGS: {
			EditorCore::get_singleton()->get_editor_settings_dialog()->popup_edit_settings();
		} break;
		case EDITOR_OPEN_DATA_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_data_dir(), true);
		} break;
		case EDITOR_OPEN_CONFIG_FOLDER: {
			OS::get_singleton()->shell_show_in_file_manager(EditorPaths::get_singleton()->get_config_dir(), true);
		} break;
		case EDITOR_MANAGE_EXPORT_TEMPLATES: {
			export_template_manager->popup_manager();
		} break;
		case EDITOR_CONFIGURE_FBX_IMPORTER: {
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
			EditorCore::get_singleton()->get_fbx_importer_manager()->show_dialog();
#endif
		} break;
		case EDITOR_MANAGE_FEATURE_PROFILES: {
			feature_profile_manager->popup_centered_clamped(Size2(900, 800) * EDSCALE, 0.8);
		} break;
		case EDITOR_TOGGLE_FULLSCREEN: {
			DisplayServer::WindowMode mode = DisplayServer::get_singleton()->window_get_mode();
			if (mode == DisplayServer::WINDOW_MODE_FULLSCREEN || mode == DisplayServer::WINDOW_MODE_EXCLUSIVE_FULLSCREEN) {
				DisplayServer::get_singleton()->window_set_mode(prev_mode);
			} else {
				prev_mode = mode;
				DisplayServer::get_singleton()->window_set_mode(DisplayServer::WINDOW_MODE_FULLSCREEN);
			}
		} break;
		case EDITOR_TAKE_SCREENSHOT: {
			screenshot_timer->start();
		} break;
		case SETTINGS_PICK_MAIN_SCENE: {
			file->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
			List<String> extensions;
			ResourceLoader::get_recognized_extensions_for_type("PackedScene", &extensions);
			file->clear_filters();
			for (const String &extension : extensions) {
				file->add_filter("*." + extension, extension.to_upper());
			}

			Node *scene = get_editor_data().get_edited_scene_root();
			if (scene) {
				file->set_current_path(scene->get_scene_file_path());
			}
			file->set_title(TTR("Pick a Main Scene"));
			file->popup_file_dialog();

		} break;
		case HELP_SEARCH: {
			emit_signal(SNAME("request_help_search"), "");
		} break;
		case EDITOR_COMMAND_PALETTE: {
			EditorCore::get_singleton()->get_command_palette()->open_popup();
		} break;
		case HELP_DOCS: {
			OS::get_singleton()->shell_open(REDOT_VERSION_DOCS_URL "/");
		} break;
		case HELP_FORUM: {
			OS::get_singleton()->shell_open("https://forum.redotengine.org/");
		} break;
		case HELP_REPORT_A_BUG: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/redot-engine/issues");
		} break;
		case HELP_COPY_SYSTEM_INFO: {
			String info = EditorCore::get_singleton()->_get_system_info();
			DisplayServer::get_singleton()->clipboard_set(info);
		} break;
		case HELP_SUGGEST_A_FEATURE: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/redot-proposals#readme");
		} break;
		case HELP_SEND_DOCS_FEEDBACK: {
			OS::get_singleton()->shell_open("https://github.com/Redot-Engine/Redot-Documentation/issues");
		} break;
		case HELP_COMMUNITY: {
			OS::get_singleton()->shell_open("https://redotengine.org/community");
		} break;
		case HELP_ABOUT: {
			about->popup_centered(Size2(780, 500) * EDSCALE);
		} break;
		case HELP_SUPPORT_REDOT_DEVELOPMENT: {
			OS::get_singleton()->shell_open("https://redotfoundation.org/support");
		} break;
	}
}

void EditorNode::_request_screenshot() {
	_screenshot();
}

void EditorNode::_screenshot(bool p_use_utc) {
	String name = "editor_screenshot_" + Time::get_singleton()->get_datetime_string_from_system(p_use_utc).remove_char(':') + ".png";
	String path = String("user://") + name;

	if (!EditorRun::request_screenshot(callable_mp(this, &EditorNode::_save_screenshot_with_embedded_process).bind(path))) {
		_save_screenshot(path);
	}
}

void EditorNode::_save_screenshot_with_embedded_process(int64_t p_w, int64_t p_h, const String &p_emb_path, const Rect2i &p_rect, const String &p_path) {
	Control *main_screen_control = editor_main_screen->get_control();
	ERR_FAIL_NULL_MSG(main_screen_control, "Cannot get the editor main screen control.");
	Viewport *viewport = main_screen_control->get_viewport();
	ERR_FAIL_NULL_MSG(viewport, "Cannot get a viewport from the editor main screen.");
	Ref<ViewportTexture> texture = viewport->get_texture();
	ERR_FAIL_COND_MSG(texture.is_null(), "Cannot get a viewport texture from the editor main screen.");
	Ref<Image> img = texture->get_image();
	ERR_FAIL_COND_MSG(img.is_null(), "Cannot get an image from a viewport texture of the editor main screen.");
	img->convert(Image::FORMAT_RGBA8);
	ERR_FAIL_COND(p_emb_path.is_empty());
	Ref<Image> overlay = Image::load_from_file(p_emb_path);
	DirAccess::remove_absolute(p_emb_path);
	ERR_FAIL_COND_MSG(overlay.is_null(), "Cannot get an image from a embedded process.");
	overlay->convert(Image::FORMAT_RGBA8);
	overlay->resize(p_rect.size.x, p_rect.size.y);
	img->blend_rect(overlay, Rect2i(0, 0, p_w, p_h), p_rect.position);
	Error error = img->save_png(p_path);
	ERR_FAIL_COND_MSG(error != OK, "Cannot save screenshot to file '" + p_path + "'.");

	if (EDITOR_GET("interface/editor/automatically_open_screenshots")) {
		OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(p_path), true);
	}
}

void EditorNode::_save_screenshot(const String &p_path) {
	Control *main_screen_control = editor_main_screen->get_control();
	ERR_FAIL_NULL_MSG(main_screen_control, "Cannot get the editor main screen control.");
	Viewport *viewport = main_screen_control->get_viewport();
	ERR_FAIL_NULL_MSG(viewport, "Cannot get a viewport from the editor main screen.");
	Ref<ViewportTexture> texture = viewport->get_texture();
	ERR_FAIL_COND_MSG(texture.is_null(), "Cannot get a viewport texture from the editor main screen.");
	Ref<Image> img = texture->get_image();
	ERR_FAIL_COND_MSG(img.is_null(), "Cannot get an image from a viewport texture of the editor main screen.");
	Error error = img->save_png(p_path);
	ERR_FAIL_COND_MSG(error != OK, "Cannot save screenshot to file '" + p_path + "'.");

	if (EDITOR_GET("interface/editor/automatically_open_screenshots")) {
		OS::get_singleton()->shell_show_in_file_manager(ProjectSettings::get_singleton()->globalize_path(p_path), true);
	}
}

void EditorNode::_check_system_theme_changed() {
	DisplayServer *display_server = DisplayServer::get_singleton();

	bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
	bool system_theme_changed = false;

	if (follow_system_theme) {
		if (display_server->get_base_color() != last_system_base_color) {
			system_theme_changed = true;
			last_system_base_color = display_server->get_base_color();
		}

		if (display_server->is_dark_mode_supported() && display_server->is_dark_mode() != last_dark_mode_state) {
			system_theme_changed = true;
			last_dark_mode_state = display_server->is_dark_mode();
		}
	}

	if (use_system_accent_color) {
		if (display_server->get_accent_color() != last_system_accent_color) {
			system_theme_changed = true;
			last_system_accent_color = display_server->get_accent_color();
		}
	}

	if (system_theme_changed) {
		_update_theme();
	} else if (global_menu && display_server->is_dark_mode_supported() && display_server->is_dark_mode() != last_dark_mode_state) {
		last_dark_mode_state = display_server->is_dark_mode();

		// Update system menus.
		bool dark_mode = DisplayServer::get_singleton()->is_dark_mode();

		help_menu->set_item_icon(help_menu->get_item_index(HELP_SEARCH), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("HelpSearch"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_COPY_SYSTEM_INFO), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("ActionCopy"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_ABOUT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Godot"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_SUPPORT_REDOT_DEVELOPMENT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Heart"), global_menu, dark_mode));
		editor_dock_manager->update_docks_menu();
	}
}

void EditorNode::_tool_menu_option(int p_idx) {
	switch (tool_menu->get_item_id(p_idx)) {
		case TOOLS_ORPHAN_RESOURCES: {
			orphan_resources->show();
		} break;
		case TOOLS_BUILD_PROFILE_MANAGER: {
			build_profile_manager->popup_centered_clamped(Size2(700, 800) * EDSCALE, 0.8);
		} break;
		case TOOLS_PROJECT_UPGRADE: {
			project_upgrade_tool->popup_dialog();
		} break;
		case TOOLS_UID_VIEWER: {
			uid_viewer->_open_tool();
		} break;
		case TOOLS_CUSTOM: {
			if (tool_menu->get_item_submenu(p_idx) == "") {
				Callable callback = tool_menu->get_item_metadata(p_idx);
				Callable::CallError ce;
				Variant result;
				callback.callp(nullptr, 0, result, ce);

				if (ce.error != Callable::CallError::CALL_OK) {
					String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
					ERR_PRINT("Error calling function from tool menu: " + err);
				}
			} // Else it's a submenu so don't do anything.
		} break;
	}
}

void EditorNode::_export_as_menu_option(int p_idx) {
	if (p_idx == 0) { // MeshLibrary
		current_menu_option = FILE_EXPORT_MESH_LIBRARY;

		if (!get_editor_data().get_edited_scene_root()) {
			show_accept(TTR("This operation can't be done without a scene."), TTR("OK"));
			return;
		}

		List<String> extensions;
		Ref<MeshLibrary> ml(memnew(MeshLibrary));
		ResourceSaver::get_recognized_extensions(ml, &extensions);
		file_export_lib->clear_filters();
		for (const String &E : extensions) {
			file_export_lib->add_filter("*." + E);
		}

		file_export_lib->set_title(TTR("Export Mesh Library"));
		file_export_lib->popup_file_dialog();
	} else { // Custom menu options added by plugins
		if (export_as_menu->get_item_submenu(p_idx).is_empty()) { // If not a submenu
			Callable callback = export_as_menu->get_item_metadata(p_idx);
			Callable::CallError ce;
			Variant result;
			callback.callp(nullptr, 0, result, ce);

			if (ce.error != Callable::CallError::CALL_OK) {
				String err = Variant::get_callable_error_text(callback, nullptr, 0, ce);
				ERR_PRINT("Error calling function from export_as menu: " + err);
			}
		}
	}
}

void EditorNode::_exit_editor(int p_exit_code) {
	exiting = true;
	waiting_for_first_scan = false;
	EditorCore::get_singleton()->get_resource_preview()->stop(); // Stop early to avoid crashes.
	_save_editor_layout_to_config();

	// Dim the editor window while it's quitting to make it clearer that it's busy.
	dim_editor(true);

	// Unload addons before quitting to allow cleanup.
	EditorPluginManager::get_singleton()->unload_editor_addons();

	get_tree()->quit(p_exit_code);
}

void EditorNode::_update_file_menu_opened() {
	bool has_unsaved = false;
	for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
		if (EditorSceneManager::get_singleton()->_is_scene_unsaved(i)) {
			has_unsaved = true;
			break;
		}
	}
	if (has_unsaved) {
		file_menu->set_item_disabled(file_menu->get_item_index(SCENE_SAVE_ALL_SCENES), false);
		file_menu->set_item_tooltip(file_menu->get_item_index(SCENE_SAVE_ALL_SCENES), String());
	} else {
		file_menu->set_item_disabled(file_menu->get_item_index(SCENE_SAVE_ALL_SCENES), true);
		file_menu->set_item_tooltip(file_menu->get_item_index(SCENE_SAVE_ALL_SCENES), TTR("All scenes are already saved."));
	}
	EditorSceneManager::get_singleton()->_update_undo_redo_allowed();
}

void EditorNode::_palette_quick_open_dialog() {
	EditorQuickOpenDialog *color_palette = EditorCore::get_singleton()->get_quick_open_color_palette();
	color_palette->popup_dialog({ "ColorPalette" }, EditorCore::get_singleton()->get_palette_file_selected_callback());
	color_palette->set_title(TTRC("Quick Open Color Palette..."));
}

void EditorNode::edit_foreign_resource(const Ref<Resource> &p_resource) {
	EditorSceneManager::get_singleton()->load_scene(p_resource->get_path().get_slice("::", 0));
	callable_mp(InspectorDock::get_singleton(), &InspectorDock::edit_resource).call_deferred(p_resource);
}

void EditorNode::_inherit_request(String p_file) {
	current_menu_option = SCENE_NEW_INHERITED_SCENE;
	_dialog_action(p_file);
}

void EditorNode::_instantiate_request(const Vector<String> &p_files) {
	EditorSceneManager::get_singleton()->request_instantiate_scenes(p_files);
}

void EditorNode::_close_messages() {
	old_split_ofs = center_split->get_split_offset();
	center_split->set_split_offset(0);
}

void EditorNode::_show_messages() {
	center_split->set_split_offset(old_split_ofs);
}

void EditorNode::_add_to_recent_scenes(const String &p_scene) {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	if (rc.has(p_scene)) {
		rc.erase(p_scene);
	}
	rc.push_front(p_scene);
	if (rc.size() > 10) {
		rc.resize(10);
	}

	EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
	_update_recent_scenes();
}

void EditorNode::_open_recent_scene(int p_idx) {
	if (p_idx == recent_scenes->get_item_count() - 1) {
		EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", Array());
		callable_mp(this, &EditorNode::_update_recent_scenes).call_deferred();
	} else {
		Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
		ERR_FAIL_INDEX(p_idx, rc.size());

		if (EditorSceneManager::get_singleton()->load_scene(rc[p_idx]) != OK) {
			rc.remove_at(p_idx);
			EditorSettings::get_singleton()->set_project_metadata("recent_files", "scenes", rc);
			_update_recent_scenes();
		}
	}
}

void EditorNode::_update_recent_scenes() {
	Array rc = EditorSettings::get_singleton()->get_project_metadata("recent_files", "scenes", Array());
	recent_scenes->clear();

	if (rc.size() == 0) {
		recent_scenes->add_item(TTRC("No Recent Scenes"), -1);
		recent_scenes->set_item_disabled(-1, true);
	} else {
		String path;
		for (int i = 0; i < rc.size(); i++) {
			path = rc[i];
			recent_scenes->add_item(path.replace("res://", ""), i);
		}

		recent_scenes->add_separator();
		recent_scenes->add_shortcut(ED_SHORTCUT("editor/clear_recent", TTRC("Clear Recent Scenes")));
	}
	recent_scenes->set_item_auto_translate_mode(-1, AUTO_TRANSLATE_MODE_ALWAYS);
	recent_scenes->reset_size();
}

void EditorNode::_quick_opened(const String &p_file_path) {
	EditorCore::get_singleton()->load_scene_or_resource(p_file_path);
}

void EditorNode::_project_run_started() {
	if (bool(EDITOR_GET("run/output/always_clear_output_on_play"))) {
		EditorCore::get_singleton()->log->clear();
	}

	int action_on_play = EDITOR_GET("run/bottom_panel/action_on_play");
	if (action_on_play == ACTION_ON_PLAY_OPEN_OUTPUT) {
		bottom_panel->make_item_visible(EditorCore::get_singleton()->log);
	} else if (action_on_play == ACTION_ON_PLAY_OPEN_DEBUGGER) {
		bottom_panel->make_item_visible(EditorDebuggerNode::get_singleton());
	}
}

void EditorNode::_project_run_stopped() {
	int action_on_stop = EDITOR_GET("run/bottom_panel/action_on_stop");
	if (action_on_stop == ACTION_ON_STOP_CLOSE_BUTTOM_PANEL) {
		bottom_panel->hide_bottom_panel();
	}
}

void EditorNode::notify_all_debug_sessions_exited() {
	project_run_bar->stop_playing();
}

void EditorNode::add_io_error(const String &p_error) {
	DEV_ASSERT(Thread::get_caller_id() == Thread::get_main_id());
	singleton->load_errors->add_image(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Error"), EditorStringName(EditorIcons)));
	singleton->load_errors->add_text(p_error + "\n");
	// When a progress dialog is displayed, we will wait for it ot close before displaying
	// the io errors to prevent the io popup to set it's parent to the progress dialog.
	if (singleton->progress_dialog->is_visible()) {
		singleton->load_errors_queued_to_display = true;
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
	}
}

void EditorNode::add_io_warning(const String &p_warning) {
	DEV_ASSERT(Thread::get_caller_id() == Thread::get_main_id());
	singleton->load_errors->add_image(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Warning"), EditorStringName(EditorIcons)));
	singleton->load_errors->add_text(p_warning + "\n");
	// When a progress dialog is displayed, we will wait for it ot close before displaying
	// the io errors to prevent the io popup to set it's parent to the progress dialog.
	if (singleton->progress_dialog->is_visible()) {
		singleton->load_errors_queued_to_display = true;
	} else {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
	}
}

bool EditorNode::close_scene() {
	int tab_index = get_editor_data().get_edited_scene();
	if (tab_index == 0 && EditorSceneManager::get_singleton()->get_edited_scene() == nullptr && get_editor_data().get_scene_path(tab_index).is_empty()) {
		return false;
	}

	EditorSceneManager::get_singleton()->tab_closing_idx = tab_index;
	current_menu_option = SCENE_CLOSE;
	EditorSceneManager::get_singleton()->_discard_changes();
	EditorSceneManager::get_singleton()->changing_scene = false;
	return true;
}

OS::ProcessID EditorNode::has_child_process(OS::ProcessID p_pid) const {
	return project_run_bar->has_child_process(p_pid);
}

void EditorNode::stop_child_process(OS::ProcessID p_pid) {
	project_run_bar->stop_child_process(p_pid);
}

void EditorNode::_pick_main_scene_custom_action(const String &p_custom_action_name) {
	if (p_custom_action_name == "select_current") {
		Node *scene = get_editor_data().get_edited_scene_root();

		if (!scene) {
			show_accept(TTR("There is no defined scene to run."), TTR("OK"));
			return;
		}

		pick_main_scene->hide();

		if (!FileAccess::exists(scene->get_scene_file_path())) {
			current_menu_option = SAVE_AND_RUN_MAIN_SCENE;
			_menu_option_confirm(SCENE_SAVE_AS_SCENE, true);
			file->set_title(TTR("Save scene before running..."));
		} else {
			current_menu_option = SETTINGS_PICK_MAIN_SCENE;
			_dialog_action(scene->get_scene_file_path());
		}
	}
}

void EditorNode::_progress_dialog_visibility_changed() {
	if (load_errors_queued_to_display && !progress_dialog->is_visible()) {
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(singleton->load_error_dialog, 0.5);
		load_errors_queued_to_display = false;
	}
}

void EditorNode::_load_error_dialog_visibility_changed() {
	if (!load_error_dialog->is_visible()) {
		load_errors->clear();
	}
}

bool EditorNode::_should_display_update_spinner() const {
#ifdef DEV_ENABLED
	const bool in_dev = true;
#else
	const bool in_dev = false;
#endif
	const int show_update_spinner_setting = EDITOR_GET("interface/editor/show_update_spinner");
	return (show_update_spinner_setting == 0 && in_dev) || show_update_spinner_setting == 1;
}

void EditorNode::_begin_first_scan() {
	if (!waiting_for_first_scan) {
		return;
	}
	requested_first_scan = true;
}

Error EditorNode::export_preset(const String &p_preset, const String &p_path, bool p_debug, bool p_pack_only, bool p_android_build_template, bool p_patch, const Vector<String> &p_patches) {
	export_defer.preset = p_preset;
	export_defer.path = p_path;
	export_defer.debug = p_debug;
	export_defer.pack_only = p_pack_only;
	export_defer.android_build_template = p_android_build_template;
	export_defer.patch = p_patch;
	export_defer.patches = p_patches;
	cmdline_mode = true;
	return OK;
}

void EditorNode::show_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	if (accept) {
		_close_save_scene_progress();
		accept->set_ok_button_text(p_title);
		accept->set_text(p_text);
		accept->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(accept, Size2i(), 0.0);
	}
}

void EditorNode::show_save_accept(const String &p_text, const String &p_title) {
	current_menu_option = -1;
	if (save_accept) {
		_close_save_scene_progress();
		save_accept->set_ok_button_text(p_title);
		save_accept->set_text(p_text);
		save_accept->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(save_accept, Size2i(), 0.0);
	}
}

void EditorNode::show_warning(const String &p_text, const String &p_title) {
	if (warning) {
		_close_save_scene_progress();
		warning->set_text(p_text);
		warning->set_title(p_title);
		warning->reset_size();
		EditorInterface::get_singleton()->popup_dialog_centered_clamped(warning, Size2i(), 0.0);
	} else {
		WARN_PRINT(p_title + " " + p_text);
	}
}

void EditorNode::_copy_warning(const String &p_str) {
	DisplayServer::get_singleton()->clipboard_set(warning->get_text());
}

void EditorNode::_save_editor_layout_to_config() {
	if (!load_editor_layout_done) {
		return;
	}
	Ref<ConfigFile> config;
	config.instantiate();
	// Load and amend existing config if it exists.
	config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));

	editor_dock_manager->save_docks_to_config(config, "docks");
	_save_open_scenes_to_config(config);
	_save_central_editor_layout_to_config(config);
	_save_window_settings_to_config(config, "EditorWindow");
	get_editor_data().get_plugin_window_layout(config);

	config->save(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
}

void EditorNode::_save_open_scenes_to_config(Ref<ConfigFile> p_layout) {
	PackedStringArray scenes;
	for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
		String path = get_editor_data().get_scene_path(i);
		if (path.is_empty()) {
			continue;
		}
		scenes.push_back(path);
	}
	p_layout->set_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes", scenes);

	String currently_edited_scene_path = get_editor_data().get_scene_path(get_editor_data().get_edited_scene());
	p_layout->set_value(EDITOR_NODE_CONFIG_SECTION, "current_scene", currently_edited_scene_path);
}

void EditorNode::save_editor_layout_delayed() {
	editor_layout_save_delay_timer->start();
}

void EditorNode::_load_editor_layout_to_config() {
	EditorProgress ep("loading_editor_layout", TTR("Loading editor"), 5);
	ep.step(TTR("Loading editor layout..."), 0, true);
	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorPaths::get_singleton()->get_project_settings_dir().path_join("editor_layout.cfg"));
	if (err != OK) { // No config.
		// If config is not found, expand the res:// folder and favorites by default.
		TreeItem *root = FileSystemDock::get_singleton()->get_tree_control()->get_item_with_metadata("res://", 0);
		if (root) {
			root->set_collapsed(false);
		}

		TreeItem *favorites = FileSystemDock::get_singleton()->get_tree_control()->get_item_with_metadata("Favorites", 0);
		if (favorites) {
			favorites->set_collapsed(false);
		}

		if (overridden_default_layout >= 0) {
			_layout_menu_option(overridden_default_layout);
		}
	} else {
		ep.step(TTR("Loading docks..."), 1, true);
		editor_dock_manager->load_docks_from_config(config, "docks", true);

		ep.step(TTR("Reopening scenes..."), 2, true);
		_load_open_scenes_from_config(config);

		ep.step(TTR("Loading central editor layout..."), 3, true);
		_load_central_editor_layout_from_config(config);

		ep.step(TTR("Loading plugin window layout..."), 4, true);
		get_editor_data().set_plugin_window_layout(config);

		ep.step(TTR("Editor layout ready."), 5, true);
	}
	load_editor_layout_done = true;
}

void EditorNode::_save_central_editor_layout_to_config(Ref<ConfigFile> p_config_file) {
	// Bottom panel.

	int center_split_offset = center_split->get_split_offset();
	p_config_file->set_value(EDITOR_NODE_CONFIG_SECTION, "center_split_offset", center_split_offset);

	bottom_panel->save_layout_to_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);

	// Debugger tab.

	int selected_default_debugger_tab_idx = EditorDebuggerNode::get_singleton()->get_default_debugger()->get_current_debugger_tab();
	p_config_file->set_value(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx", selected_default_debugger_tab_idx);

	// Main editor (plugin).

	editor_main_screen->save_layout_to_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);
}

void EditorNode::_load_central_editor_layout_from_config(Ref<ConfigFile> p_config_file) {
	// Bottom panel.

	bottom_panel->load_layout_from_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);

	if (p_config_file->has_section_key(EDITOR_NODE_CONFIG_SECTION, "center_split_offset")) {
		int center_split_offset = p_config_file->get_value(EDITOR_NODE_CONFIG_SECTION, "center_split_offset");
		center_split->set_split_offset(center_split_offset);
	}

	// Debugger tab.

	if (p_config_file->has_section_key(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx")) {
		int selected_default_debugger_tab_idx = p_config_file->get_value(EDITOR_NODE_CONFIG_SECTION, "selected_default_debugger_tab_idx");
		EditorDebuggerNode::get_singleton()->get_default_debugger()->switch_to_debugger(selected_default_debugger_tab_idx);
	}

	// Main editor (plugin).

	editor_main_screen->load_layout_from_config(p_config_file, EDITOR_NODE_CONFIG_SECTION);
}

void EditorNode::_save_window_settings_to_config(Ref<ConfigFile> p_layout, const String &p_section) {
	Window *w = get_window();
	if (w) {
		p_layout->set_value(p_section, "screen", w->get_current_screen());

		Window::Mode mode = w->get_mode();
		switch (mode) {
			case Window::MODE_WINDOWED:
				p_layout->set_value(p_section, "mode", "windowed");
				p_layout->set_value(p_section, "size", w->get_size());
				break;
			case Window::MODE_FULLSCREEN:
			case Window::MODE_EXCLUSIVE_FULLSCREEN:
				p_layout->set_value(p_section, "mode", "fullscreen");
				break;
			case Window::MODE_MINIMIZED:
				if (was_window_windowed_last) {
					p_layout->set_value(p_section, "mode", "windowed");
					p_layout->set_value(p_section, "size", w->get_size());
				} else {
					p_layout->set_value(p_section, "mode", "maximized");
				}
				break;
			default:
				p_layout->set_value(p_section, "mode", "maximized");
				break;
		}

		p_layout->set_value(p_section, "position", w->get_position());
	}
}

void EditorNode::_load_open_scenes_from_config(Ref<ConfigFile> p_layout) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return;
	}

	if (!bool(EDITOR_GET("interface/scene_tabs/restore_scenes_on_load"))) {
		return;
	}

	if (!p_layout->has_section(EDITOR_NODE_CONFIG_SECTION) ||
			!p_layout->has_section_key(EDITOR_NODE_CONFIG_SECTION, "open_scenes")) {
		return;
	}

	EditorSceneManager::get_singleton()->restoring_scenes = true;

	PackedStringArray scenes = p_layout->get_value(EDITOR_NODE_CONFIG_SECTION, "open_scenes");
	for (int i = 0; i < scenes.size(); i++) {
		if (FileAccess::exists(scenes[i])) {
			EditorSceneManager::get_singleton()->load_scene(scenes[i]);
		}
	}

	if (p_layout->has_section_key(EDITOR_NODE_CONFIG_SECTION, "current_scene")) {
		String current_scene = p_layout->get_value(EDITOR_NODE_CONFIG_SECTION, "current_scene");
		for (int i = 0; i < get_editor_data().get_edited_scene_count(); i++) {
			if (get_editor_data().get_scene_path(i) == current_scene) {
				EditorSceneManager::get_singleton()->_set_current_scene(i);
				break;
			}
		}
	}

	save_editor_layout_delayed();

	EditorSceneManager::get_singleton()->restoring_scenes = false;
}

bool EditorNode::ensure_main_scene(bool p_from_native) {
	pick_main_scene->set_meta("from_native", p_from_native); // Whether from play button or native run.
	String main_scene = GLOBAL_GET("application/run/main_scene");

	if (main_scene.is_empty()) {
		current_menu_option = -1;
		pick_main_scene->set_text(TTR("No main scene has ever been defined. Select one?\nYou can change it later in \"Project Settings\" under the 'application' category."));
		pick_main_scene->popup_centered();

		if (get_editor_data().get_edited_scene_root()) {
			select_current_scene_button->set_disabled(false);
			select_current_scene_button->grab_focus();
		} else {
			select_current_scene_button->set_disabled(true);
		}

		return false;
	}

	if (!FileAccess::exists(main_scene)) {
		current_menu_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' does not exist. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered();
		return false;
	}

	if (ResourceLoader::get_resource_type(main_scene) != "PackedScene") {
		current_menu_option = -1;
		pick_main_scene->set_text(vformat(TTR("Selected scene '%s' is not a scene file. Select a valid one?\nYou can change it later in \"Project Settings\" under the 'application' category."), main_scene));
		pick_main_scene->popup_centered();
		return false;
	}

	return true;
}

bool EditorNode::validate_custom_directory() {
	bool use_custom_dir = GLOBAL_GET("application/config/use_custom_user_dir");

	if (use_custom_dir) {
		String data_dir = OS::get_singleton()->get_user_data_dir();
		Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_USERDATA);
		if (dir->change_dir(data_dir) != OK) {
			dir->make_dir_recursive(data_dir);
			if (dir->change_dir(data_dir) != OK) {
				open_project_settings->set_text(vformat(TTR("User data dir '%s' is not valid. Change to a valid one?"), data_dir));
				open_project_settings->popup_centered();
				return false;
			}
		}
	}

	return true;
}

void EditorNode::_immediate_dialog_confirmed() {
	immediate_dialog_confirmed = true;
}

bool EditorNode::immediate_confirmation_dialog(const String &p_text, const String &p_ok_text, const String &p_cancel_text, uint32_t p_wrap_width) {
	ConfirmationDialog *cd = memnew(ConfirmationDialog);
	cd->set_text(p_text);
	cd->set_ok_button_text(p_ok_text);
	cd->set_cancel_button_text(p_cancel_text);
	if (p_wrap_width > 0) {
		cd->set_autowrap(true);
		cd->get_label()->set_custom_minimum_size(Size2(p_wrap_width, 0) * EDSCALE);
	}

	cd->connect(SceneStringName(confirmed), callable_mp(singleton, &EditorNode::_immediate_dialog_confirmed));
	singleton->gui_base->add_child(cd);

	cd->popup_centered();

	while (true) {
		DisplayServer::get_singleton()->process_events();
		Main::iteration();
		if (singleton->immediate_dialog_confirmed || !cd->is_visible()) {
			break;
		}
	}

	memdelete(cd);
	return singleton->immediate_dialog_confirmed;
}

bool EditorNode::is_cmdline_mode() {
	ERR_FAIL_NULL_V(singleton, false);
	return singleton->cmdline_mode;
}


void EditorNode::_update_layouts_menu() {
	editor_layouts->clear();
	overridden_default_layout = -1;

	editor_layouts->reset_size();
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/save", TTRC("Save Layout...")), LAYOUT_SAVE);
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/delete", TTRC("Delete Layout...")), LAYOUT_DELETE);
	editor_layouts->add_separator();
	editor_layouts->add_shortcut(ED_SHORTCUT("layout/default", TTRC("Default")), LAYOUT_DEFAULT);

	Ref<ConfigFile> config;
	config.instantiate();
	Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
	if (err != OK) {
		return; // No config.
	}

	Vector<String> layouts = config->get_sections();

	for (const String &layout : layouts) {
		if (layout == TTR("Default")) {
			editor_layouts->remove_item(editor_layouts->get_item_index(LAYOUT_DEFAULT));
			overridden_default_layout = editor_layouts->get_item_count();
		}

		editor_layouts->add_item(layout);
		editor_layouts->set_item_auto_translate_mode(-1, AUTO_TRANSLATE_MODE_DISABLED);
	}
}

void EditorNode::_layout_menu_option(int p_id) {
	switch (p_id) {
		case LAYOUT_SAVE: {
			current_menu_option = p_id;
			layout_dialog->set_title(TTR("Save Layout"));
			layout_dialog->set_ok_button_text(TTR("Save"));
			layout_dialog->set_name_line_enabled(true);
			layout_dialog->popup_centered();
		} break;
		case LAYOUT_DELETE: {
			current_menu_option = p_id;
			layout_dialog->set_title(TTR("Delete Layout"));
			layout_dialog->set_ok_button_text(TTR("Delete"));
			layout_dialog->set_name_line_enabled(false);
			layout_dialog->popup_centered();
		} break;
		case LAYOUT_DEFAULT: {
			editor_dock_manager->load_docks_from_config(default_layout, "docks");
			_save_editor_layout_to_config();
		} break;
		default: {
			Ref<ConfigFile> config;
			config.instantiate();
			Error err = config->load(EditorSettings::get_singleton()->get_editor_layouts_config());
			if (err != OK) {
				return; // No config.
			}

			editor_dock_manager->load_docks_from_config(config, editor_layouts->get_item_text(p_id));
			_save_editor_layout_to_config();
		}
	}
}

bool EditorNode::_is_closing_editor() const {
	return EditorSceneManager::get_singleton()->tab_closing_menu_option == SCENE_QUIT ||
			EditorSceneManager::get_singleton()->tab_closing_menu_option == PROJECT_QUIT_TO_PROJECT_MANAGER ||
			EditorSceneManager::get_singleton()->tab_closing_menu_option == PROJECT_RELOAD_CURRENT_PROJECT;
}

void EditorNode::_restart_editor(bool p_goto_project_manager) {
	exiting = true;

	if (project_run_bar->is_playing()) {
		project_run_bar->stop_playing();
	}

	String to_reopen;
	if (!p_goto_project_manager && get_tree()->get_edited_scene_root()) {
		to_reopen = get_tree()->get_edited_scene_root()->get_scene_file_path();
	}

	_exit_editor(EXIT_SUCCESS);

	List<String> args;
	for (const String &a : Main::get_forwardable_cli_arguments(Main::CLI_SCOPE_TOOL)) {
		args.push_back(a);
	}

	if (p_goto_project_manager) {
		args.push_back("--project-manager");

		// Setup working directory.
		const String exec_dir = OS::get_singleton()->get_executable_path().get_base_dir();
		if (!exec_dir.is_empty()) {
			args.push_back("--path");
			args.push_back(exec_dir);
		}
	} else {
		args.push_back("--path");
		args.push_back(ProjectSettings::get_singleton()->get_resource_path());

		args.push_back("-e");
	}

	if (!to_reopen.is_empty()) {
		args.push_back(to_reopen);
	}

	OS::get_singleton()->set_restart_on_exit(true, args);
}

void EditorNode::_cancel_confirmation() {
	stop_project_confirmation = false;
}

void EditorNode::_prepare_save_confirmation_popup() {
	if (save_confirmation->get_window() != get_last_exclusive_window()) {
		save_confirmation->reparent(get_last_exclusive_window());
	}
}

void EditorNode::_toggle_distraction_free_mode() {
	if (EDITOR_GET("interface/editor/separate_distraction_mode")) {
		int screen = editor_main_screen->get_selected_index();

		if (screen == EditorMainScreen::EDITOR_SCRIPT) {
			script_distraction_free = !script_distraction_free;
			set_distraction_free_mode(script_distraction_free);
		} else {
			scene_distraction_free = !scene_distraction_free;
			set_distraction_free_mode(scene_distraction_free);
		}
	} else {
		set_distraction_free_mode(distraction_free->is_pressed());
	}
}

void EditorNode::update_distraction_free_mode() {
	if (!EDITOR_GET("interface/editor/separate_distraction_mode")) {
		return;
	}
	int screen = editor_main_screen->get_selected_index();
	if (screen == EditorMainScreen::EDITOR_SCRIPT) {
		set_distraction_free_mode(script_distraction_free);
	} else {
		set_distraction_free_mode(scene_distraction_free);
	}
}

void EditorNode::set_distraction_free_mode(bool p_enter) {
	distraction_free->set_pressed(p_enter);

	if (p_enter) {
		if (editor_dock_manager->are_docks_visible()) {
			editor_dock_manager->set_docks_visible(false);
		}
	} else {
		editor_dock_manager->set_docks_visible(true);
	}
}

bool EditorNode::is_distraction_free_mode_enabled() const {
	return distraction_free->is_pressed();
}

Dictionary EditorNode::drag_resource(const Ref<Resource> &p_res, Control *p_from) {
	Control *drag_control = memnew(Control);
	TextureRect *drag_preview = memnew(TextureRect);
	Label *label = memnew(Label);
	label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

	Ref<Texture2D> preview;

	{
		/// @todo Make proper previews
		Ref<Texture2D> texture = EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("FileBigThumb"), EditorStringName(EditorIcons));
		if (texture.is_valid()) {
			Ref<Image> img = texture->get_image();
			img = img->duplicate();
			img->resize(48, 48); // meh
			preview = ImageTexture::create_from_image(img);
		}
	}

	drag_preview->set_texture(preview);
	drag_control->add_child(drag_preview);
	if (p_res->get_path().is_resource_file()) {
		label->set_text(p_res->get_path().get_file());
	} else if (!p_res->get_name().is_empty()) {
		label->set_text(p_res->get_name());
	} else {
		label->set_text(p_res->get_class());
	}

	drag_control->add_child(label);

	p_from->set_drag_preview(drag_control); // Wait until it enters scene.

	label->set_position(Point2((preview->get_width() - label->get_minimum_size().width) / 2, preview->get_height()));

	Dictionary drag_data;
	drag_data["type"] = "resource";
	drag_data["resource"] = p_res;
	drag_data["from"] = p_from;

	return drag_data;
}

Dictionary EditorNode::drag_files_and_dirs(const Vector<String> &p_paths, Control *p_from) {
	bool has_folder = false;
	bool has_file = false;
	for (int i = 0; i < p_paths.size(); i++) {
		bool is_folder = p_paths[i].ends_with("/");
		has_folder |= is_folder;
		has_file |= !is_folder;
	}

	int max_rows = 6;
	int num_rows = p_paths.size() > max_rows ? max_rows - 1 : p_paths.size(); // Don't waste a row to say "1 more file" - list it instead.
	VBoxContainer *vbox = memnew(VBoxContainer);
	for (int i = 0; i < num_rows; i++) {
		HBoxContainer *hbox = memnew(HBoxContainer);
		TextureRect *icon = memnew(TextureRect);
		Label *label = memnew(Label);
		label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		label->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);

		if (p_paths[i].ends_with("/")) {
			label->set_text(p_paths[i].substr(0, p_paths[i].length() - 1).get_file());
			icon->set_texture(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Folder"), EditorStringName(EditorIcons)));
		} else {
			label->set_text(p_paths[i].get_file());
			icon->set_texture(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("File"), EditorStringName(EditorIcons)));
		}
		icon->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
		icon->set_size(Size2(16, 16));
		hbox->add_child(icon);
		hbox->add_child(label);
		vbox->add_child(hbox);
	}

	if (p_paths.size() > num_rows) {
		Label *label = memnew(Label);
		label->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		if (has_file && has_folder) {
			label->set_text(vformat(TTR("%d more files or folders"), p_paths.size() - num_rows));
		} else if (has_folder) {
			label->set_text(vformat(TTR("%d more folders"), p_paths.size() - num_rows));
		} else {
			label->set_text(vformat(TTR("%d more files"), p_paths.size() - num_rows));
		}
		vbox->add_child(label);
	}
	p_from->set_drag_preview(vbox); // Wait until it enters scene.

	Dictionary drag_data;
	drag_data["type"] = has_folder ? "files_and_dirs" : "files";
	drag_data["files"] = p_paths;
	drag_data["from"] = p_from;
	return drag_data;
}

void EditorNode::add_tool_menu_item(const String &p_name, const Callable &p_callback) {
	int idx = tool_menu->get_item_count();
	tool_menu->add_item(p_name, TOOLS_CUSTOM);
	tool_menu->set_item_metadata(idx, p_callback);
}

void EditorNode::add_tool_submenu_item(const String &p_name, PopupMenu *p_submenu) {
	ERR_FAIL_NULL(p_submenu);
	ERR_FAIL_COND(p_submenu->get_parent() != nullptr);
	tool_menu->add_submenu_node_item(p_name, p_submenu, TOOLS_CUSTOM);
}

void EditorNode::remove_tool_menu_item(const String &p_name) {
	for (int i = 0; i < tool_menu->get_item_count(); i++) {
		if (tool_menu->get_item_id(i) != TOOLS_CUSTOM) {
			continue;
		}

		if (tool_menu->get_item_text(i) == p_name) {
			if (tool_menu->get_item_submenu(i) != "") {
				Node *n = tool_menu->get_node(tool_menu->get_item_submenu(i));
				tool_menu->remove_child(n);
				memdelete(n);
			}
			tool_menu->remove_item(i);
			tool_menu->reset_size();
			return;
		}
	}
}

PopupMenu *EditorNode::get_export_as_menu() {
	return export_as_menu;
}

void EditorNode::_dropped_files(const Vector<String> &p_files) {
	String to_path = FileSystemDock::get_singleton()->get_folder_path_at_mouse_position();
	if (to_path.is_empty()) {
		to_path = FileSystemDock::get_singleton()->get_current_directory();
	}
	to_path = ProjectSettings::get_singleton()->globalize_path(to_path);

	_add_dropped_files_recursive(p_files, to_path);

	EditorFileSystem::get_singleton()->scan_changes();
}

void EditorNode::_add_dropped_files_recursive(const Vector<String> &p_files, String to_path) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND(dir.is_null());

	for (int i = 0; i < p_files.size(); i++) {
		const String &from = p_files[i];
		String to = to_path.path_join(from.get_file());

		if (dir->dir_exists(from)) {
			Vector<String> sub_files;

			Ref<DirAccess> sub_dir = DirAccess::open(from);
			ERR_FAIL_COND(sub_dir.is_null());

			sub_dir->list_dir_begin();

			String next_file = sub_dir->get_next();
			while (!next_file.is_empty()) {
				if (next_file == "." || next_file == "..") {
					next_file = sub_dir->get_next();
					continue;
				}

				sub_files.push_back(from.path_join(next_file));
				next_file = sub_dir->get_next();
			}

			if (!sub_files.is_empty()) {
				dir->make_dir(to);
				_add_dropped_files_recursive(sub_files, to);
			}

			continue;
		}

		dir->copy(from, to);
	}
}

void EditorNode::_inherit_imported(const String &p_action) {
	open_imported->hide();
	EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->get_open_import_request(), true, true);
}

void EditorNode::_open_imported() {
	EditorSceneManager::get_singleton()->load_scene(EditorSceneManager::get_singleton()->get_open_import_request(), true, false, true);
}

void EditorNode::dim_editor(bool p_dimming) {
	dimmed = p_dimming;
	gui_base->set_modulate(p_dimming ? Color(0.5, 0.5, 0.5) : Color(1, 1, 1));
}

bool EditorNode::is_editor_dimmed() const {
	return dimmed;
}

void EditorNode::open_export_template_manager() {
	export_template_manager->popup_manager();
}

void EditorNode::_update_renderer_color() {
	String rendering_method = renderer->get_selected_metadata();

	if (rendering_method == "forward_plus") {
		renderer->add_theme_color_override(SceneStringName(font_color), EditorCore::get_singleton()->get_editor_theme()->get_color(SNAME("forward_plus_color"), EditorStringName(Editor)));
	} else if (rendering_method == "mobile") {
		renderer->add_theme_color_override(SceneStringName(font_color), EditorCore::get_singleton()->get_editor_theme()->get_color(SNAME("mobile_color"), EditorStringName(Editor)));
	} else if (rendering_method == "gl_compatibility") {
		renderer->add_theme_color_override(SceneStringName(font_color), EditorCore::get_singleton()->get_editor_theme()->get_color(SNAME("gl_compatibility_color"), EditorStringName(Editor)));
	}
}

void EditorNode::_renderer_selected(int p_which) {
	String rendering_method = renderer->get_item_metadata(p_which);

	String current_renderer = GLOBAL_GET("rendering/renderer/rendering_method");

	if (rendering_method == current_renderer) {
		return;
	}

	renderer_request = rendering_method;
	video_restart_dialog->set_text(
			vformat(TTR("Changing the renderer requires restarting the editor.\n\nChoosing Save & Restart will change the rendering method to:\n- Desktop platforms: %s\n- Mobile platforms: %s\n- Web platform: gl_compatibility"),
					renderer_request, renderer_request.replace("forward_plus", "mobile")));
	video_restart_dialog->popup_centered();
	renderer->select(renderer_current);
	_update_renderer_color();
}

void EditorNode::_add_renderer_entry(const String &p_renderer_name, bool p_mark_overridden) {
	/// @todo Make this a dictionary or something instead of if block bile.
	String item_text;
	if (p_renderer_name == "forward_plus") {
		item_text = TTR("Forward+");
	}
	if (p_renderer_name == "mobile") {
		item_text = TTR("Mobile");
	}
	if (p_renderer_name == "gl_compatibility") {
		item_text = TTR("Compatibility");
	}
	if (p_mark_overridden) {
		// TRANSLATORS: The placeholder is the rendering method that has overridden the default one.
		item_text = vformat(TTR("%s (Overridden)"), item_text);
	}
	renderer->add_item(item_text);
}

void EditorNode::_set_renderer_name_save_and_restart() {
	ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method", renderer_request);
	if (renderer_request == "mobile" || renderer_request == "gl_compatibility") {
		// Also change the mobile override if changing to a compatible rendering method.
		// This prevents visual discrepancies between desktop and mobile platforms.
		ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method.mobile", renderer_request);
	} else if (renderer_request == "forward_plus") {
		// Use the equivalent mobile rendering method. This prevents the rendering method from staying
		// on its old choice if moving from `gl_compatibility` to `forward_plus`.
		ProjectSettings::get_singleton()->set("rendering/renderer/rendering_method.mobile", "mobile");
	}

	ProjectSettings::get_singleton()->save();

	EditorSceneManager::get_singleton()->save_all_open_scenes();
	restart_editor();
}

void EditorNode::_feature_profile_changed() {
	Ref<EditorFeatureProfile> profile = feature_profile_manager->get_current_profile();
	if (profile.is_valid()) {
		editor_dock_manager->set_dock_enabled(NodeDock::get_singleton(), !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_NODE_DOCK));
		// The Import dock is useless without the FileSystem dock. Ensure the configuration is valid.
		bool fs_dock_disabled = profile->is_feature_disabled(EditorFeatureProfile::FEATURE_FILESYSTEM_DOCK);
		editor_dock_manager->set_dock_enabled(FileSystemDock::get_singleton(), !fs_dock_disabled);
		editor_dock_manager->set_dock_enabled(ImportDock::get_singleton(), !fs_dock_disabled && !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_IMPORT_DOCK));
		editor_dock_manager->set_dock_enabled(history_dock, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_HISTORY_DOCK));

		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_3D, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_3D));
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_SCRIPT, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_SCRIPT));
		if (!Engine::get_singleton()->is_recovery_mode_hint()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_GAME, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_GAME));
		}
		if (AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_ASSETLIB, !profile->is_feature_disabled(EditorFeatureProfile::FEATURE_ASSET_LIB));
		}
	} else {
		editor_dock_manager->set_dock_enabled(ImportDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(NodeDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(FileSystemDock::get_singleton(), true);
		editor_dock_manager->set_dock_enabled(history_dock, true);
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_3D, true);
		editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_SCRIPT, true);
		if (!Engine::get_singleton()->is_recovery_mode_hint()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_GAME, true);
		}
		if (AssetLibraryEditorPlugin::is_available()) {
			editor_main_screen->set_button_enabled(EditorMainScreen::EDITOR_ASSETLIB, true);
		}
	}
}

void EditorNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("push_item", "object", "property", "inspector_only"), &EditorSceneManager::push_item, DEFVAL(""), DEFVAL(false));

	ClassDB::bind_method("stop_child_process", &EditorNode::stop_child_process);

	// NOTE: "request_help_search" and "script_add_function_request" are declared on EditorShell.
	ADD_SIGNAL(MethodInfo("resource_saved", PropertyInfo(Variant::OBJECT, "obj")));
	ADD_SIGNAL(MethodInfo("preview_locale_changed"));
}

static void _execute_thread(void *p_ud) {
	EditorNode::ExecuteThreadArgs *eta = (EditorNode::ExecuteThreadArgs *)p_ud;
	Error err = OS::get_singleton()->execute(eta->path, eta->args, &eta->output, &eta->exitcode, true, &eta->execute_output_mutex);
	print_verbose("Thread exit status: " + itos(eta->exitcode));
	if (err != OK) {
		eta->exitcode = err;
	}

	eta->done.set();
}

int EditorNode::execute_and_show_output(const String &p_title, const String &p_path, const List<String> &p_arguments, bool p_close_on_ok, bool p_close_on_errors, String *r_output) {
	if (execute_output_dialog) {
		execute_output_dialog->set_title(p_title);
		execute_output_dialog->get_ok_button()->set_disabled(true);
		execute_outputs->clear();
		execute_outputs->set_scroll_follow(true);
		EditorInterface::get_singleton()->popup_dialog_centered_ratio(execute_output_dialog);
	}

	ExecuteThreadArgs eta;
	eta.path = p_path;
	eta.args = p_arguments;
	eta.exitcode = 255;

	int prev_len = 0;

	eta.execute_output_thread.start(_execute_thread, &eta);

	while (!eta.done.is_set()) {
		{
			MutexLock lock(eta.execute_output_mutex);
			if (prev_len != eta.output.length()) {
				String to_add = eta.output.substr(prev_len);
				prev_len = eta.output.length();
				execute_outputs->add_text(to_add);
				DisplayServer::get_singleton()->process_events(); // Get rid of pending events.
				Main::iteration();
			}
		}
		OS::get_singleton()->delay_usec(1000);
	}

	eta.execute_output_thread.wait_to_finish();
	execute_outputs->add_text("\nExit Code: " + itos(eta.exitcode));

	if (execute_output_dialog) {
		if (p_close_on_errors && eta.exitcode != 0) {
			execute_output_dialog->hide();
		}
		if (p_close_on_ok && eta.exitcode == 0) {
			execute_output_dialog->hide();
		}

		execute_output_dialog->get_ok_button()->set_disabled(false);
	}

	if (r_output) {
		*r_output = eta.output;
	}
	return eta.exitcode;
}

void EditorNode::set_unfocused_low_processor_usage_mode_enabled(bool p_enabled) {
	unfocused_low_processor_usage_mode_enabled = p_enabled;
}

void EditorNode::_update_main_menu_type() {
	bool use_menu_button = EDITOR_GET("interface/editor/collapse_main_menu");
	bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);

	bool already_using_button = main_menu_button != nullptr;
	bool already_using_bar = main_menu_bar != nullptr;
	if ((use_menu_button && already_using_button) || (!use_menu_button && already_using_bar)) {
		return; // Already correctly configured.
	}

	if (use_menu_button && !global_menu) {
		main_menu_button = memnew(MenuButton);
		main_menu_button->set_text(TTRC("Main Menu"));
		main_menu_button->set_theme_type_variation("MainScreenButton");
		main_menu_button->set_focus_mode(Control::FOCUS_NONE);
		if (is_inside_tree()) {
			main_menu_button->set_button_icon(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("TripleBar"), EditorStringName(EditorIcons)));
		}
		main_menu_button->set_switch_on_hover(true);

		if (main_menu_bar != nullptr) {
			Vector<PopupMenu *> menus_to_move;
			for (int i = 0; i < main_menu_bar->get_child_count(); i++) {
				PopupMenu *menu = Object::cast_to<PopupMenu>(main_menu_bar->get_child(i));
				if (menu != nullptr) {
					menus_to_move.push_back(menu);
				}
			}
			for (PopupMenu *menu : menus_to_move) {
				main_menu_bar->remove_child(menu);
				main_menu_button->get_popup()->add_submenu_node_item(menu->get_name(), menu);
			}
		}

#ifdef ANDROID_ENABLED
		// Align main menu icon visually with TouchActionsPanel buttons.
		main_menu_button->get_popup()->add_theme_constant_override("v_separation", 16 * EDSCALE);
		menu_btn_spacer = memnew(Control);
		menu_btn_spacer->set_custom_minimum_size(Vector2(8, 0) * EDSCALE);
		title_bar->add_child(menu_btn_spacer);
		title_bar->move_child(menu_btn_spacer, left_menu_spacer ? left_menu_spacer->get_index() + 1 : 0);
#endif
		title_bar->add_child(main_menu_button);
		if (menu_btn_spacer == nullptr) {
			title_bar->move_child(main_menu_button, left_menu_spacer ? left_menu_spacer->get_index() + 1 : 0);
		} else {
			title_bar->move_child(main_menu_button, menu_btn_spacer->get_index() + 1);
		}
		memdelete_notnull(main_menu_bar);
		main_menu_bar = nullptr;

		if (project_run_bar != nullptr) {
			// Adjust spacers to center 2D / 3D / Script buttons.
			int max_w = MAX(project_run_bar->get_minimum_size().x + right_menu_hb->get_minimum_size().x, main_menu_button->get_minimum_size().x);
			left_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - main_menu_button->get_minimum_size().x), 0));
			right_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - project_run_bar->get_minimum_size().x - right_menu_hb->get_minimum_size().x), 0));
		}
	} else {
		main_menu_bar = memnew(MenuBar);
		main_menu_bar->set_mouse_filter(Control::MOUSE_FILTER_STOP);
		main_menu_bar->set_v_size_flags(Control::SIZE_SHRINK_CENTER);
		main_menu_bar->set_theme_type_variation("MainMenuBar");
		main_menu_bar->set_start_index(0); // Main menu, add to the start of global menu.
		main_menu_bar->set_prefer_global_menu(global_menu);
		main_menu_bar->set_switch_on_hover(true);

		if (main_menu_button != nullptr) {
			Vector<PopupMenu *> menus_to_move;
			for (int i = 0; i < main_menu_button->get_item_count(); i++) {
				PopupMenu *menu = main_menu_button->get_popup()->get_item_submenu_node(i);
				if (menu != nullptr) {
					menus_to_move.push_back(menu);
				}
			}
			for (PopupMenu *menu : menus_to_move) {
				menu->get_parent()->remove_child(menu);
				main_menu_bar->add_child(menu);
			}
		}

		title_bar->add_child(main_menu_bar);
		title_bar->move_child(main_menu_bar, left_menu_spacer ? left_menu_spacer->get_index() + 1 : 0);

		memdelete_notnull(menu_btn_spacer);
		memdelete_notnull(main_menu_button);
		menu_btn_spacer = nullptr;
		main_menu_button = nullptr;

		if (project_run_bar != nullptr) {
			// Adjust spacers to center 2D / 3D / Script buttons.
			int max_w = MAX(project_run_bar->get_minimum_size().x + right_menu_hb->get_minimum_size().x, main_menu_bar->get_minimum_size().x);
			left_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - main_menu_bar->get_minimum_size().x), 0));
			right_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - project_run_bar->get_minimum_size().x - right_menu_hb->get_minimum_size().x), 0));
		}
	}
}

void EditorNode::_add_to_main_menu(const String &p_name, PopupMenu *p_menu) {
	p_menu->set_name(p_name);
	if (main_menu_button != nullptr) {
		main_menu_button->get_popup()->add_submenu_node_item(p_name, p_menu);
	} else {
		main_menu_bar->add_child(p_menu);
	}
}

#ifdef ANDROID_ENABLED
void EditorNode::_touch_actions_panel_mode_changed() {
	int panel_mode = EDITOR_GET("interface/touchscreen/touch_actions_panel");
	switch (panel_mode) {
		case 1:
			if (touch_actions_panel != nullptr) {
				touch_actions_panel->queue_free();
			}
			touch_actions_panel = memnew(TouchActionsPanel);
			main_hbox->call_deferred("add_child", touch_actions_panel);
			break;
		case 2:
			if (touch_actions_panel != nullptr) {
				touch_actions_panel->queue_free();
			}
			touch_actions_panel = memnew(TouchActionsPanel);
			call_deferred("add_child", touch_actions_panel);
			break;
		case 0:
			if (touch_actions_panel != nullptr) {
				touch_actions_panel->queue_free();
				touch_actions_panel = nullptr;
			}
			break;
	}
}
#endif

#ifdef MACOS_ENABLED
extern "C" GameViewPluginBase *get_game_view_plugin();
#else
GameViewPluginBase *get_game_view_plugin() {
	return memnew(GameViewPlugin);
}
#endif

void EditorNode::open_setting_override(const String &p_property) {
	EditorCore::get_singleton()->get_editor_settings_dialog()->hide();
	EditorCore::get_singleton()->get_project_settings_editor()->popup_for_override(p_property);
}

EditorNode::EditorNode() {
	DEV_ASSERT(!singleton);
	singleton = this;

	ERR_FAIL_NULL(EditorCore::get_singleton());

	// Register this shell with the core; this also wires the shell's
	// back-pointers to the core, scene manager, and plugin manager.
	EditorCore::get_singleton()->set_shell(this);

	// Detecting headless mode, that means the editor is running in command line.
	if (!DisplayServer::get_singleton()->window_can_draw()) {
		cmdline_mode = true;
	}

	ED_SHORTCUT("editor/lock_selected_nodes", TTRC("Lock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::L);
	ED_SHORTCUT("editor/unlock_selected_nodes", TTRC("Unlock Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::L);
	ED_SHORTCUT("editor/group_selected_nodes", TTRC("Group Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | Key::G);
	ED_SHORTCUT("editor/ungroup_selected_nodes", TTRC("Ungroup Selected Node(s)"), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::G);

	FileAccess::set_backup_save(EDITOR_GET("filesystem/on_save/safe_save_on_backup_then_rename"));

	_update_vsync_mode();

	// Warm up the project upgrade tool as early as possible.
	project_upgrade_tool = memnew(ProjectUpgradeTool);
	run_project_upgrade_tool = EditorSettings::get_singleton()->get_project_metadata(project_upgrade_tool->META_PROJECT_UPGRADE_TOOL, project_upgrade_tool->META_RUN_ON_RESTART, false);
	if (run_project_upgrade_tool) {
		project_upgrade_tool->begin_upgrade();
	}

	{
		bool agile_input_event_flushing = EDITOR_GET("input/buffering/agile_event_flushing");
		bool use_accumulated_input = EDITOR_GET("input/buffering/use_accumulated_input");

		Input::get_singleton()->set_agile_input_event_flushing(agile_input_event_flushing);
		Input::get_singleton()->set_use_accumulated_input(use_accumulated_input);
	}

	{
		int display_scale = EDITOR_GET("interface/editor/display_scale");

		switch (display_scale) {
			case 0:
				// Try applying a suitable display scale automatically.
				EditorScale::set_scale(EditorSettings::get_auto_display_scale());
				break;
			case 1:
				EditorScale::set_scale(0.75);
				break;
			case 2:
				EditorScale::set_scale(1.0);
				break;
			case 3:
				EditorScale::set_scale(1.25);
				break;
			case 4:
				EditorScale::set_scale(1.5);
				break;
			case 5:
				EditorScale::set_scale(1.75);
				break;
			case 6:
				EditorScale::set_scale(2.0);
				break;
			default:
				EditorScale::set_scale(EDITOR_GET("interface/editor/custom_display_scale"));
				break;
		}
	}

	// Define a minimum window size to prevent UI elements from overlapping or being cut off.
	Window *w = Object::cast_to<Window>(SceneTree::get_singleton()->get_root());
	if (w) {
		const Size2 minimum_size = Size2(1280, 600) * EDSCALE;
		w->set_min_size(minimum_size); // Calling it this early doesn't sync the property with DS.
		DisplayServer::get_singleton()->window_set_min_size(minimum_size);
	}

	EditorFileDialog::set_default_show_hidden_files(EDITOR_GET("filesystem/file_dialog/show_hidden_files"));
	EditorFileDialog::set_default_display_mode((EditorFileDialog::DisplayMode)EDITOR_GET("filesystem/file_dialog/display_mode").operator int());

	int swap_cancel_ok = EDITOR_GET("interface/editor/accept_dialog_cancel_ok_buttons");
	if (swap_cancel_ok != 0) { // 0 is auto, set in register_scene based on DisplayServer.
		// Swap on means OK first.
		AcceptDialog::set_swap_cancel_ok(swap_cancel_ok == 2);
	}

	int ed_root_dir = EDITOR_GET("interface/editor/ui_layout_direction");
	Control::set_root_layout_direction(ed_root_dir);
	Window::set_root_layout_direction(ed_root_dir);

	ResourceLoader::set_abort_on_missing_resources(false);

	{
		// Register importers at the beginning, so dialogs are created with the right extensions.
		Ref<ResourceImporterTexture> import_texture = memnew(ResourceImporterTexture(true));
		ResourceFormatImporter::get_singleton()->add_importer(import_texture);

		Ref<ResourceImporterLayeredTexture> import_cubemap;
		import_cubemap.instantiate();
		import_cubemap->set_mode(ResourceImporterLayeredTexture::MODE_CUBEMAP);
		ResourceFormatImporter::get_singleton()->add_importer(import_cubemap);

		Ref<ResourceImporterLayeredTexture> import_array;
		import_array.instantiate();
		import_array->set_mode(ResourceImporterLayeredTexture::MODE_2D_ARRAY);
		ResourceFormatImporter::get_singleton()->add_importer(import_array);

		Ref<ResourceImporterLayeredTexture> import_cubemap_array;
		import_cubemap_array.instantiate();
		import_cubemap_array->set_mode(ResourceImporterLayeredTexture::MODE_CUBEMAP_ARRAY);
		ResourceFormatImporter::get_singleton()->add_importer(import_cubemap_array);

		Ref<ResourceImporterLayeredTexture> import_3d = memnew(ResourceImporterLayeredTexture(true));
		import_3d->set_mode(ResourceImporterLayeredTexture::MODE_3D);
		ResourceFormatImporter::get_singleton()->add_importer(import_3d);

		Ref<ResourceImporterImage> import_image;
		import_image.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_image);

		Ref<ResourceImporterSVG> import_svg;
		import_svg.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_svg);

		Ref<ResourceImporterTextureAtlas> import_texture_atlas;
		import_texture_atlas.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_texture_atlas);

		Ref<ResourceImporterSpriteFrames> import_sprite_frames;
		import_sprite_frames.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_sprite_frames);

#ifndef DISABLE_DEPRECATED
		Ref<ResourceImporterAnimatedTexture> import_animated_texture;
		import_animated_texture.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_animated_texture);
#endif

		Ref<ResourceImporterImageFrames> import_image_frames;
		import_image_frames.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_image_frames);

		Ref<ResourceImporterDynamicFont> import_font_data_dynamic;
		import_font_data_dynamic.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_dynamic);

		Ref<ResourceImporterBMFont> import_font_data_bmfont;
		import_font_data_bmfont.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_bmfont);

		Ref<ResourceImporterImageFont> import_font_data_image;
		import_font_data_image.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_font_data_image);

		Ref<ResourceImporterCSVTranslation> import_csv_translation;
		import_csv_translation.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_csv_translation);

		Ref<ResourceImporterWAV> import_wav;
		import_wav.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_wav);

		Ref<ResourceImporterOBJ> import_obj;
		import_obj.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_obj);

		Ref<ResourceImporterShaderFile> import_shader_file;
		import_shader_file.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_shader_file);

		Ref<ResourceImporterScene> import_scene = memnew(ResourceImporterScene("PackedScene", true));
		ResourceFormatImporter::get_singleton()->add_importer(import_scene);

		Ref<ResourceImporterScene> import_animation = memnew(ResourceImporterScene("AnimationLibrary", true));
		ResourceFormatImporter::get_singleton()->add_importer(import_animation);

		{
			Ref<EditorSceneFormatImporterCollada> import_collada;
			import_collada.instantiate();
			ResourceImporterScene::add_scene_importer(import_collada);

			Ref<EditorOBJImporter> import_obj2;
			import_obj2.instantiate();
			ResourceImporterScene::add_scene_importer(import_obj2);

			Ref<EditorSceneFormatImporterESCN> import_escn;
			import_escn.instantiate();
			ResourceImporterScene::add_scene_importer(import_escn);
		}

		Ref<ResourceImporterBitMap> import_bitmap;
		import_bitmap.instantiate();
		ResourceFormatImporter::get_singleton()->add_importer(import_bitmap);
	}

	{
		Ref<EditorInspectorDefaultPlugin> eidp;
		eidp.instantiate();
		EditorInspector::add_inspector_plugin(eidp);

		Ref<EditorInspectorRootMotionPlugin> rmp;
		rmp.instantiate();
		EditorInspector::add_inspector_plugin(rmp);

		Ref<EditorInspectorVisualShaderModePlugin> smp;
		smp.instantiate();
		EditorInspector::add_inspector_plugin(smp);

		Ref<EditorInspectorParticleProcessMaterialPlugin> ppm;
		ppm.instantiate();
		EditorInspector::add_inspector_plugin(ppm);
	}

	EditorContextMenuPluginManager::create();

	register_exporters();

	ED_SHORTCUT("canvas_item_editor/pan_view", TTRC("Pan View"), Key::SPACE);

	progress_dialog = memnew(ProgressDialog);
	add_child(progress_dialog);
	progress_dialog->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorNode::_progress_dialog_visibility_changed));

	gui_base = memnew(Panel);
	add_child(gui_base);

	// Take up all screen.
	gui_base->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	gui_base->set_anchor(SIDE_RIGHT, Control::ANCHOR_END);
	gui_base->set_anchor(SIDE_BOTTOM, Control::ANCHOR_END);
	gui_base->set_end(Point2(0, 0));

	main_vbox = memnew(VBoxContainer);

#ifdef ANDROID_ENABLED
	base_vbox = memnew(VBoxContainer);
	base_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, EditorCore::get_singleton()->get_editor_theme()->get_constant(SNAME("window_border_margin"),

	title_bar = memnew(EditorTitleBar);
	base_vbox->add_child(title_bar);

	main_hbox = memnew(HBoxContainer);
	main_hbox->add_child(main_vbox);
	main_vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	main_hbox->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	base_vbox->add_child(main_hbox);

	_touch_actions_panel_mode_changed();

	gui_base->add_child(base_vbox);
#else
	gui_base->add_child(main_vbox);

	title_bar = memnew(EditorTitleBar);
	main_vbox->add_child(title_bar);
#endif

	left_l_hsplit = memnew(DockSplitContainer);
	left_l_hsplit->set_name("DockHSplitLeftL");
	main_vbox->add_child(left_l_hsplit);

	left_l_hsplit->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	left_l_vsplit = memnew(DockSplitContainer);
	left_l_vsplit->set_name("DockVSplitLeftL");
	left_l_vsplit->set_vertical(true);
	left_l_hsplit->add_child(left_l_vsplit);

	TabContainer *dock_slot[EditorDockManager::DOCK_SLOT_MAX];
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL]->set_name("DockSlotLeftUL");
	left_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UL]);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL]->set_name("DockSlotLeftBL");
	left_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BL]);

	left_r_hsplit = memnew(DockSplitContainer);
	left_r_hsplit->set_name("DockHSplitLeftR");
	left_l_hsplit->add_child(left_r_hsplit);
	left_r_vsplit = memnew(DockSplitContainer);
	left_r_vsplit->set_name("DockVSplitLeftR");
	left_r_vsplit->set_vertical(true);
	left_r_hsplit->add_child(left_r_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR]->set_name("DockSlotLeftUR");
	left_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_UR]);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR]->set_name("DockSlotLeftBR");
	left_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_LEFT_BR]);

	main_hsplit = memnew(DockSplitContainer);
	main_hsplit->set_name("DockHSplitMain");
	left_r_hsplit->add_child(main_hsplit);
	VBoxContainer *center_vb = memnew(VBoxContainer);
	main_hsplit->add_child(center_vb);

	center_vb->set_h_size_flags(Control::SIZE_EXPAND_FILL);

	center_split = memnew(DockSplitContainer);
	center_split->set_name("DockVSplitCenter");
	center_split->set_vertical(true);
	center_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	center_split->set_collapsed(true);
	center_vb->add_child(center_split);

	right_hsplit = memnew(DockSplitContainer);
	right_hsplit->set_name("DockHSplitRight");
	main_hsplit->add_child(right_hsplit);

	right_l_vsplit = memnew(DockSplitContainer);
	right_l_vsplit->set_name("DockVSplitRightL");
	right_l_vsplit->set_vertical(true);
	right_hsplit->add_child(right_l_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL]->set_name("DockSlotRightUL");
	right_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UL]);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL]->set_name("DockSlotRightBL");
	right_l_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BL]);

	right_r_vsplit = memnew(DockSplitContainer);
	right_r_vsplit->set_name("DockVSplitRightR");
	right_r_vsplit->set_vertical(true);
	right_hsplit->add_child(right_r_vsplit);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR]->set_name("DockSlotRightUR");
	right_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_UR]);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR] = memnew(TabContainer);
	dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR]->set_name("DockSlotRightBR");
	right_r_vsplit->add_child(dock_slot[EditorDockManager::DOCK_SLOT_RIGHT_BR]);

	editor_dock_manager = memnew(EditorDockManager);

	// Save the splits for easier access.
	editor_dock_manager->add_vsplit(left_l_vsplit);
	editor_dock_manager->add_vsplit(left_r_vsplit);
	editor_dock_manager->add_vsplit(right_l_vsplit);
	editor_dock_manager->add_vsplit(right_r_vsplit);

	editor_dock_manager->add_hsplit(left_l_hsplit);
	editor_dock_manager->add_hsplit(left_r_hsplit);
	editor_dock_manager->add_hsplit(main_hsplit);
	editor_dock_manager->add_hsplit(right_hsplit);

	for (int i = 0; i < EditorDockManager::DOCK_SLOT_MAX; i++) {
		editor_dock_manager->register_dock_slot((EditorDockManager::DockSlot)i, dock_slot[i]);
	}

	editor_layout_save_delay_timer = memnew(Timer);
	add_child(editor_layout_save_delay_timer);
	editor_layout_save_delay_timer->set_wait_time(0.5);
	editor_layout_save_delay_timer->set_one_shot(true);
	editor_layout_save_delay_timer->connect("timeout", callable_mp(this, &EditorNode::_save_editor_layout_to_config));

	scan_changes_timer = memnew(Timer);
	scan_changes_timer->set_wait_time(0.5);
	scan_changes_timer->set_autostart(EDITOR_GET("interface/editor/import_resources_when_unfocused"));
	scan_changes_timer->connect("timeout", callable_mp(EditorFileSystem::get_singleton(), &EditorFileSystem::scan_changes));
	add_child(scan_changes_timer);

	top_split = memnew(VSplitContainer);
	center_split->add_child(top_split);
	top_split->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	top_split->set_collapsed(true);

	VBoxContainer *srt = memnew(VBoxContainer);
	srt->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	srt->add_theme_constant_override("separation", 0);
	top_split->add_child(srt);

	scene_tabs = memnew(EditorSceneTabs);
	srt->add_child(scene_tabs);
	scene_tabs->connect("tab_changed", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_set_current_scene));
	scene_tabs->connect("tab_closed", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_scene_tab_closed));

	distraction_free = memnew(Button);
	distraction_free->set_theme_type_variation("FlatMenuButton");
	ED_SHORTCUT_AND_COMMAND("editor/distraction_free_mode", TTRC("Distraction Free Mode"), KeyModifierMask::CTRL | KeyModifierMask::SHIFT | Key::F11);
	ED_SHORTCUT_OVERRIDE("editor/distraction_free_mode", "macos", KeyModifierMask::META | KeyModifierMask::SHIFT | Key::D);
	ED_SHORTCUT_AND_COMMAND("editor/toggle_last_opened_bottom_panel", TTRC("Toggle Last Opened Bottom Panel"), KeyModifierMask::CMD_OR_CTRL | Key::J);
	distraction_free->set_shortcut(ED_GET_SHORTCUT("editor/distraction_free_mode"));
	distraction_free->set_tooltip_text(TTRC("Toggle distraction-free mode."));
	distraction_free->set_toggle_mode(true);
	scene_tabs->add_extra_button(distraction_free);
	distraction_free->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_toggle_distraction_free_mode));

	editor_main_screen = memnew(EditorMainScreen);
	editor_main_screen->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	editor_main_screen->set_draw_behind_parent(true);
	srt->add_child(editor_main_screen);
	editor_main_screen->set_v_size_flags(Control::SIZE_EXPAND_FILL);

	scene_root = memnew(SubViewport);
	scene_root->set_auto_translate_mode(AUTO_TRANSLATE_MODE_ALWAYS);
	scene_root->set_translation_domain(StringName());
	scene_root->set_embedding_subwindows(true);
	scene_root->set_disable_3d(true);
	scene_root->set_disable_input(true);
	scene_root->set_as_audio_listener_2d(true);

	bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
	bool dark_mode = DisplayServer::get_singleton()->is_dark_mode_supported() && DisplayServer::get_singleton()->is_dark_mode();
	bool can_expand = bool(EDITOR_GET("interface/editor/expand_to_title")) && DisplayServer::get_singleton()->has_feature(DisplayServer::FEATURE_EXTEND_TO_TITLE);

	if (can_expand) {
		// Add spacer to avoid other controls under window minimize/maximize/close buttons (left side).
		left_menu_spacer = memnew(Control);
		left_menu_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		title_bar->add_child(left_menu_spacer);
	}

	_update_main_menu_type();

	file_menu = memnew(PopupMenu);
	_add_to_main_menu(TTRC("Scene"), file_menu);

	accept = memnew(AcceptDialog);
	accept->set_autowrap(true);
	accept->set_min_size(Vector2i(600, 0));
	accept->set_unparent_when_invisible(true);

	save_accept = memnew(AcceptDialog);
	save_accept->set_unparent_when_invisible(true);
	save_accept->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind((int)MenuOptions::SCENE_SAVE_AS_SCENE));

	EditorCore *core = EditorCore::get_singleton();
	core->initialize_dialogs();

	core->get_project_export()->reparent(gui_base);

	dependency_error = memnew(DependencyErrorDialog);
	gui_base->add_child(dependency_error);

	dependency_fixer = memnew(DependencyEditor);
	gui_base->add_child(dependency_fixer);

	core->get_editor_settings_dialog()->reparent(gui_base);
	core->get_project_settings_editor()->reparent(gui_base);

	scene_import_settings = memnew(SceneImportSettingsDialog);
	gui_base->add_child(scene_import_settings);

	audio_stream_import_settings = memnew(AudioStreamImportSettingsDialog);
	gui_base->add_child(audio_stream_import_settings);

	fontdata_import_settings = memnew(DynamicFontImportSettingsDialog);
	gui_base->add_child(fontdata_import_settings);

	export_template_manager = memnew(ExportTemplateManager);
	gui_base->add_child(export_template_manager);

	feature_profile_manager = memnew(EditorFeatureProfileManager);
	gui_base->add_child(feature_profile_manager);

	build_profile_manager = memnew(EditorBuildProfileManager);
	gui_base->add_child(build_profile_manager);

	uid_viewer = memnew(UIDViewer);
	gui_base->add_child(uid_viewer);

	about = memnew(EditorAbout);
	gui_base->add_child(about);
	feature_profile_manager->connect("current_feature_profile_changed", callable_mp(this, &EditorNode::_feature_profile_changed));

	warning = memnew(AcceptDialog);
	warning->set_unparent_when_invisible(true);
	warning->add_button(TTR("Copy Text"), true, "copy");
	warning->connect("custom_action", callable_mp(this, &EditorNode::_copy_warning));

	ED_SHORTCUT("editor/next_tab", TTRC("Next Scene Tab"), KeyModifierMask::CTRL + Key::TAB);
	ED_SHORTCUT("editor/prev_tab", TTRC("Previous Scene Tab"), KeyModifierMask::CTRL + KeyModifierMask::SHIFT + Key::TAB);
	ED_SHORTCUT("editor/filter_files", TTRC("Focus FileSystem Filter"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::ALT + Key::P);

	// The command palette is owned by the core; the shell parents it.
	core->get_command_palette()->set_title(TTR("Command Palette"));
	gui_base->add_child(core->get_command_palette());

	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_scene", TTRC("New Scene"), KeyModifierMask::CMD_OR_CTRL + Key::N), SCENE_NEW_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/new_inherited_scene", TTRC("New Inherited Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::N), SCENE_NEW_INHERITED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/open_scene", TTRC("Open Scene..."), KeyModifierMask::CMD_OR_CTRL + Key::O), SCENE_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reopen_closed_scene", TTRC("Reopen Closed Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::T), SCENE_OPEN_PREV);

	recent_scenes = memnew(PopupMenu);
	recent_scenes->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	file_menu->add_submenu_node_item(TTRC("Open Recent"), recent_scenes, SCENE_OPEN_RECENT);
	recent_scenes->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_open_recent_scene));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene", TTRC("Save Scene"), KeyModifierMask::CMD_OR_CTRL + Key::S), SCENE_SAVE_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_scene_as", TTRC("Save Scene As..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::S), SCENE_SAVE_AS_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/save_all_scenes", TTRC("Save All Scenes"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::S), SCENE_SAVE_ALL_SCENES);

	file_menu->add_separator();

	file_menu->add_shortcut(ED_SHORTCUT_ARRAY_AND_COMMAND("editor/quick_open", TTRC("Quick Open..."), { int32_t(KeyModifierMask::SHIFT + KeyModifierMask::ALT + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) }), SCENE_QUICK_OPEN);
	ED_SHORTCUT_OVERRIDE_ARRAY("editor/quick_open", "macos", { int32_t(KeyModifierMask::META + KeyModifierMask::CTRL + Key::O), int32_t(KeyModifierMask::CMD_OR_CTRL + Key::P) });
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_scene", TTRC("Quick Open Scene..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::O), SCENE_QUICK_OPEN_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/quick_open_script", TTRC("Quick Open Script..."), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::ALT + Key::O), SCENE_QUICK_OPEN_SCRIPT);

	file_menu->add_separator();
	export_as_menu = memnew(PopupMenu);
	file_menu->add_submenu_node_item(TTRC("Export As..."), export_as_menu);
	export_as_menu->add_shortcut(ED_SHORTCUT("editor/export_as_mesh_library", TTRC("MeshLibrary...")), FILE_EXPORT_MESH_LIBRARY);
	export_as_menu->connect("index_pressed", callable_mp(this, &EditorNode::_export_as_menu_option));

	file_menu->add_separator();
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_undo"), SCENE_UNDO, false, true);
	file_menu->add_shortcut(ED_GET_SHORTCUT("ui_redo"), SCENE_REDO, false, true);

	file_menu->add_separator();
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/reload_saved_scene", TTRC("Reload Saved Scene")), SCENE_RELOAD_SAVED_SCENE);
	file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/close_scene", TTRC("Close Scene"), KeyModifierMask::CMD_OR_CTRL + KeyModifierMask::SHIFT + Key::W), SCENE_CLOSE);
	ED_SHORTCUT_OVERRIDE("editor/close_scene", "macos", KeyModifierMask::CMD_OR_CTRL + Key::W);

	if (!global_menu || !OS::get_singleton()->has_feature("macos")) {
		// On macOS  "Quit" and "About" options are in the "app" menu.
		file_menu->add_separator();
		file_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/file_quit", TTRC("Quit"), KeyModifierMask::CMD_OR_CTRL + Key::Q), SCENE_QUIT, true);
	}

	ED_SHORTCUT_AND_COMMAND("editor/editor_settings", TTRC("Editor Settings..."));
	ED_SHORTCUT_OVERRIDE("editor/editor_settings", "macos", KeyModifierMask::META + Key::COMMA);
#ifdef MACOS_ENABLED
	if (global_menu && NativeMenu::get_singleton()->has_system_menu(NativeMenu::APPLICATION_MENU_ID)) {
		apple_menu = memnew(PopupMenu);
		apple_menu->set_system_menu(NativeMenu::APPLICATION_MENU_ID);
		main_menu_bar->add_child(apple_menu);

		apple_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), EDITOR_OPEN_SETTINGS);
		apple_menu->add_separator();
		apple_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	}
#endif

	project_menu = memnew(PopupMenu);
	_add_to_main_menu(TTRC("Project"), project_menu);

	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/project_settings", TTRC("Project Settings..."), Key::NONE, TTRC("Project Settings")), PROJECT_OPEN_SETTINGS);
	project_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	ED_SHORTCUT_AND_COMMAND("editor/find_in_files", TTRC("Find in Files..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::F);
	project_menu->add_shortcut(ED_GET_SHORTCUT("editor/find_in_files"), PROJECT_FIND_IN_FILES);

	project_menu->add_separator();
	project_menu->add_item(TTRC("Version Control"), PROJECT_VERSION_CONTROL);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/export", TTRC("Export..."), Key::NONE, TTRC("Export")), PROJECT_EXPORT);
	project_menu->add_item(TTRC("Pack Project as ZIP..."), PROJECT_PACK_AS_ZIP);
#ifndef ANDROID_ENABLED
	project_menu->add_item(TTRC("Install Android Build Template..."), PROJECT_INSTALL_ANDROID_SOURCE);
	project_menu->add_item(TTRC("Open User Data Folder"), PROJECT_OPEN_USER_DATA_FOLDER);
#endif

	project_menu->add_separator();

	tool_menu = memnew(PopupMenu);
	tool_menu->connect("index_pressed", callable_mp(this, &EditorNode::_tool_menu_option));
	project_menu->add_submenu_node_item(TTRC("Tools"), tool_menu);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/orphan_resource_explorer", TTRC("Orphan Resource Explorer...")), TOOLS_ORPHAN_RESOURCES);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/engine_compilation_configuration_editor", TTRC("Engine Compilation Configuration Editor...")), TOOLS_BUILD_PROFILE_MANAGER);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/upgrade_project", TTRC("Upgrade Project Files...")), TOOLS_PROJECT_UPGRADE);
	tool_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/uid_viewer", TTRC("UID Viewer...")), TOOLS_UID_VIEWER);

	project_menu->add_separator();
	project_menu->add_shortcut(ED_SHORTCUT("editor/reload_current_project", TTRC("Reload Current Project")), PROJECT_RELOAD_CURRENT_PROJECT);
	ED_SHORTCUT_AND_COMMAND("editor/quit_to_project_list", TTRC("Quit to Project List"), KeyModifierMask::CTRL + KeyModifierMask::SHIFT + Key::Q);
	ED_SHORTCUT_OVERRIDE("editor/quit_to_project_list", "macos", KeyModifierMask::META + KeyModifierMask::CTRL + KeyModifierMask::ALT + Key::Q);
	project_menu->add_shortcut(ED_GET_SHORTCUT("editor/quit_to_project_list"), PROJECT_QUIT_TO_PROJECT_MANAGER, true);

	// Spacer to center 2D / 3D / Script buttons.
	left_spacer = memnew(HBoxContainer);
	left_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	left_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(left_spacer);

	if (can_expand && global_menu) {
		project_title = memnew(Label);
		project_title->add_theme_font_override(SceneStringName(font), EditorCore::get_singleton()->get_editor_theme()->get_font(SNAME("bold"), EditorStringName(EditorFonts)));
		project_title->add_theme_font_size_override(SceneStringName(font_size), EditorCore::get_singleton()->get_editor_theme()->get_font_size(SNAME("bold_size"), EditorStringName(EditorFonts)));
		project_title->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
		project_title->set_vertical_alignment(VERTICAL_ALIGNMENT_CENTER);
		project_title->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		project_title->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		left_spacer->add_child(project_title);
	}

	HBoxContainer *main_editor_button_hb = memnew(HBoxContainer);
	main_editor_button_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	main_editor_button_hb->set_name("EditorMainScreenButtons");
	editor_main_screen->set_button_container(main_editor_button_hb);
	title_bar->add_child(main_editor_button_hb);
	title_bar->set_center_control(main_editor_button_hb);

	// Options are added and handled by DebuggerEditorPlugin.
	debug_menu = memnew(PopupMenu);
	_add_to_main_menu(TTRC("Debug"), debug_menu);

	settings_menu = memnew(PopupMenu);
	_add_to_main_menu(TTRC("Editor"), settings_menu);

#ifdef MACOS_ENABLED
	if (!global_menu) {
		settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), EDITOR_OPEN_SETTINGS);
	}
#else
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/editor_settings"), EDITOR_OPEN_SETTINGS);
#endif
	settings_menu->add_shortcut(ED_SHORTCUT("editor/command_palette", TTRC("Command Palette..."), KeyModifierMask::CMD_OR_CTRL | KeyModifierMask::SHIFT | Key::P), EDITOR_COMMAND_PALETTE);
	settings_menu->add_separator();

	settings_menu->add_submenu_node_item(TTRC("Editor Docks"), editor_dock_manager->get_docks_menu());

	editor_layouts = memnew(PopupMenu);
	settings_menu->add_submenu_node_item(TTRC("Editor Layout"), editor_layouts);
	editor_layouts->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_layout_menu_option));
	settings_menu->add_separator();

	ED_SHORTCUT_AND_COMMAND("editor/take_screenshot", TTRC("Take Screenshot"), KeyModifierMask::CTRL | Key::F12);
	ED_SHORTCUT_OVERRIDE("editor/take_screenshot", "macos", KeyModifierMask::META | Key::F12);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/take_screenshot"), EDITOR_TAKE_SCREENSHOT);

	settings_menu->set_item_tooltip(-1, TTRC("Screenshots are stored in the user data folder (\"user://\")."));

	ED_SHORTCUT_AND_COMMAND("editor/fullscreen_mode", TTRC("Toggle Fullscreen"), KeyModifierMask::SHIFT | Key::F11);
	ED_SHORTCUT_OVERRIDE("editor/fullscreen_mode", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::F);
	settings_menu->add_shortcut(ED_GET_SHORTCUT("editor/fullscreen_mode"), EDITOR_TOGGLE_FULLSCREEN);
	settings_menu->add_separator();

#ifndef ANDROID_ENABLED
	if (OS::get_singleton()->get_data_path() == OS::get_singleton()->get_config_path()) {
		// Configuration and data folders are located in the same place (Windows/macOS).
		settings_menu->add_item(TTRC("Open Editor Data/Settings Folder"), EDITOR_OPEN_DATA_FOLDER);
	} else {
		// Separate configuration and data folders (Linux).
		settings_menu->add_item(TTRC("Open Editor Data Folder"), EDITOR_OPEN_DATA_FOLDER);
		settings_menu->add_item(TTRC("Open Editor Settings Folder"), EDITOR_OPEN_CONFIG_FOLDER);
	}
	settings_menu->add_separator();
#endif

	settings_menu->add_item(TTRC("Manage Editor Features..."), EDITOR_MANAGE_FEATURE_PROFILES);
	settings_menu->add_item(TTRC("Manage Export Templates..."), EDITOR_MANAGE_EXPORT_TEMPLATES);
#if !defined(ANDROID_ENABLED) && !defined(WEB_ENABLED)
	settings_menu->add_item(TTRC("Configure FBX Importer..."), EDITOR_CONFIGURE_FBX_IMPORTER);
#endif

	help_menu = memnew(PopupMenu);
	if (global_menu && NativeMenu::get_singleton()->has_system_menu(NativeMenu::HELP_MENU_ID)) {
		help_menu->set_system_menu(NativeMenu::HELP_MENU_ID);
	}
	_add_to_main_menu(TTRC("Help"), help_menu);

	help_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	ED_SHORTCUT_AND_COMMAND("editor/editor_help", TTRC("Search Help..."), Key::F1);
	ED_SHORTCUT_OVERRIDE("editor/editor_help", "macos", KeyModifierMask::ALT | Key::SPACE);
	help_menu->add_icon_shortcut(EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("HelpSearch"), global_menu, dark_mode), ED_GET_SHORTCUT("editor/editor_help"), HELP_SEARCH);
	help_menu->add_separator();
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/online_docs", TTRC("Online Documentation")), HELP_DOCS);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/forum", TTRC("Forum")), HELP_FORUM);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/community", TTRC("Community")), HELP_COMMUNITY);
	help_menu->add_separator();
	help_menu->add_icon_shortcut(EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("ActionCopy"), global_menu, dark_mode), ED_SHORTCUT_AND_COMMAND("editor/copy_system_info", TTRC("Copy System Info")), HELP_COPY_SYSTEM_INFO);
	help_menu->set_item_tooltip(-1, TTR("Copies the system info as a single-line text into the clipboard."));
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/report_a_bug", TTRC("Report a Bug")), HELP_REPORT_A_BUG);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/suggest_a_feature", TTRC("Suggest a Feature")), HELP_SUGGEST_A_FEATURE);
	help_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/send_docs_feedback", TTRC("Send Docs Feedback")), HELP_SEND_DOCS_FEEDBACK);
	help_menu->add_separator();
	if (!global_menu || !OS::get_singleton()->has_feature("macos")) {
		// On macOS  "Quit" and "About" options are in the "app" menu.
		help_menu->add_icon_shortcut(EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Godot"), global_menu, dark_mode), ED_SHORTCUT_AND_COMMAND("editor/about", TTRC("About Engine...")), HELP_ABOUT);
	}

	help_menu->add_icon_shortcut(EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Heart"), global_menu, dark_mode), ED_SHORTCUT_AND_COMMAND("editor/support_development", TTRC("Support Engine Development")), HELP_SUPPORT_REDOT_DEVELOPMENT);

	// Spacer to center 2D / 3D / Script buttons.
	right_spacer = memnew(Control);
	right_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
	right_spacer->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	title_bar->add_child(right_spacer);

	project_run_bar = memnew(EditorRunBar);
	project_run_bar->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(project_run_bar);
	project_run_bar->connect("play_pressed", callable_mp(this, &EditorNode::_project_run_started));
	project_run_bar->connect("stop_pressed", callable_mp(this, &EditorNode::_project_run_stopped));

	right_menu_hb = memnew(HBoxContainer);
	right_menu_hb->set_mouse_filter(Control::MOUSE_FILTER_STOP);
	title_bar->add_child(right_menu_hb);

	renderer = memnew(OptionButton);
	renderer->set_visible(true);
	renderer->set_flat(true);
	renderer->set_theme_type_variation("TopBarOptionButton");
	renderer->set_fit_to_longest_item(false);
	renderer->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	renderer->set_auto_translate_mode(AUTO_TRANSLATE_MODE_DISABLED);
	renderer->set_tooltip_auto_translate_mode(AUTO_TRANSLATE_MODE_ALWAYS);
	renderer->set_tooltip_text(TTRC("Choose a rendering method.\n\nNotes:\n- On mobile platforms, the Mobile rendering method is used if Forward+ is selected here.\n- On the web platform, the Compatibility rendering method is always used."));
	renderer->set_accessibility_name(TTRC("Rendering Method"));

	right_menu_hb->add_child(renderer);

	if (can_expand) {
		// Add spacer to avoid other controls under the window minimize/maximize/close buttons (right side).
		right_menu_spacer = memnew(Control);
		right_menu_spacer->set_mouse_filter(Control::MOUSE_FILTER_PASS);
		title_bar->add_child(right_menu_spacer);
	}

	String current_renderer_ps = GLOBAL_GET("rendering/renderer/rendering_method");
	current_renderer_ps = current_renderer_ps.to_lower();
	String current_renderer_os = OS::get_singleton()->get_current_rendering_method().to_lower();

	// Add the renderers name to the UI.
	if (current_renderer_ps == current_renderer_os) {
		renderer->connect(SceneStringName(item_selected), callable_mp(this, &EditorNode::_renderer_selected));
		// As we are doing string comparisons, keep in standard case to prevent problems with capitals
		// "vulkan" in particular uses lowercase "v" in the code, and uppercase in the UI.
		PackedStringArray renderers = ProjectSettings::get_singleton()->get_custom_property_info().get(StringName("rendering/renderer/rendering_method")).hint_string.split(",", false);
		for (int i = 0; i < renderers.size(); i++) {
			String rendering_method = renderers[i];
			if (rendering_method == "dummy") {
				continue;
			}
			_add_renderer_entry(rendering_method, false);
			renderer->set_item_metadata(i, rendering_method);
			// Lowercase for standard comparison.
			rendering_method = rendering_method.to_lower();
			if (current_renderer_ps == rendering_method) {
				renderer->select(i);
				renderer_current = i;
			}
		}
	} else {
		// It's an CLI-overridden rendering method.
		_add_renderer_entry(current_renderer_os, true);
		renderer->set_item_metadata(0, current_renderer_os);
		renderer->select(0);
		renderer_current = 0;
	}
	_update_renderer_color();

	video_restart_dialog = memnew(ConfirmationDialog);
	video_restart_dialog->set_ok_button_text(TTR("Save & Restart"));
	video_restart_dialog->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_set_renderer_name_save_and_restart));
	gui_base->add_child(video_restart_dialog);

	progress_hb = memnew(BackgroundProgress);
	EditorProgressTracker::get_singleton()->set_progress_ui(progress_dialog, progress_hb);
	EditorProgressTracker::get_singleton()->set_cmdline_mode(cmdline_mode);

	layout_dialog = memnew(EditorLayoutsDialog);
	gui_base->add_child(layout_dialog);
	layout_dialog->set_hide_on_ok(false);
	layout_dialog->set_size(Size2(225, 270) * EDSCALE);
	layout_dialog->connect("name_confirmed", callable_mp(this, &EditorNode::_dialog_action));

	update_spinner = memnew(MenuButton);
	right_menu_hb->add_child(update_spinner);
    update_spinner->set_button_icon(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Progress1"), EditorStringName(EditorIcons)));
	update_spinner->get_popup()->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	update_spinner->set_accessibility_name(TTRC("Update Mode"));
	PopupMenu *p = update_spinner->get_popup();
	p->add_radio_check_item(TTRC("Update Continuously"), SPINNER_UPDATE_CONTINUOUSLY);
	p->add_radio_check_item(TTRC("Update When Changed"), SPINNER_UPDATE_WHEN_CHANGED);
	p->add_separator();
	p->add_item(TTRC("Hide Update Spinner"), SPINNER_UPDATE_SPINNER_HIDE);
	_update_update_spinner();

	// Instantiate and place editor docks.

	memnew(SceneTreeDock(scene_root, core->get_editor_selection(), get_editor_data()));
	memnew(FileSystemDock);
	memnew(InspectorDock(get_editor_data()));
	memnew(ImportDock);
	memnew(NodeDock);

	FileSystemDock *filesystem_dock = FileSystemDock::get_singleton();
	filesystem_dock->connect("inherit", callable_mp(this, &EditorNode::_inherit_request));
	filesystem_dock->connect("instantiate", callable_mp(this, &EditorNode::_instantiate_request));
	filesystem_dock->connect("display_mode_changed", callable_mp(this, &EditorNode::_save_editor_layout_to_config));
	core->get_project_settings_editor()->connect_filesystem_dock_signals(filesystem_dock);

	history_dock = memnew(HistoryDock);

	// Scene: Top left.
	editor_dock_manager->add_dock(SceneTreeDock::get_singleton(), TTRC("Scene"), EditorDockManager::DOCK_SLOT_LEFT_UR, ED_SHORTCUT_AND_COMMAND("docks/open_scene", TTRC("Open Scene Dock")), "PackedScene");

	// Import: Top left, behind Scene.
	editor_dock_manager->add_dock(ImportDock::get_singleton(), TTRC("Import"), EditorDockManager::DOCK_SLOT_LEFT_UR, ED_SHORTCUT_AND_COMMAND("docks/open_import", TTRC("Open Import Dock")), "FileAccess");

	// FileSystem: Bottom left.
	editor_dock_manager->add_dock(FileSystemDock::get_singleton(), TTRC("FileSystem"), EditorDockManager::DOCK_SLOT_LEFT_BR, ED_SHORTCUT_AND_COMMAND("docks/open_filesystem", TTRC("Open FileSystem Dock"), KeyModifierMask::ALT | Key::F), "Folder");

	// Inspector: Full height right.
	editor_dock_manager->add_dock(InspectorDock::get_singleton(), TTRC("Inspector"), EditorDockManager::DOCK_SLOT_RIGHT_UL, ED_SHORTCUT_AND_COMMAND("docks/open_inspector", TTRC("Open Inspector Dock")), "AnimationTrackList");

	// Node: Full height right, behind Inspector.
	editor_dock_manager->add_dock(NodeDock::get_singleton(), TTRC("Node"), EditorDockManager::DOCK_SLOT_RIGHT_UL, ED_SHORTCUT_AND_COMMAND("docks/open_node", TTRC("Open Node Dock")), "Object");

	// History: Full height right, behind Node.
	editor_dock_manager->add_dock(history_dock, TTRC("History"), EditorDockManager::DOCK_SLOT_RIGHT_UL, ED_SHORTCUT_AND_COMMAND("docks/open_history", TTRC("Open History Dock")), "History");

	// Add some offsets to left_r and main hsplits to make LEFT_R and RIGHT_L docks wider than minsize.
	left_r_hsplit->set_split_offset(270 * EDSCALE);
	main_hsplit->set_split_offset(-270 * EDSCALE);

	// Define corresponding default layout.

	const String docks_section = "docks";
	default_layout.instantiate();
	// Dock numbers are based on DockSlot enum value + 1.
	default_layout->set_value(docks_section, "dock_3", "Scene,Import");
	default_layout->set_value(docks_section, "dock_4", "FileSystem");
	default_layout->set_value(docks_section, "dock_5", "Inspector,Node,History");

	// There are 4 vsplits and 4 hsplits.
	for (int i = 0; i < editor_dock_manager->get_vsplit_count(); i++) {
		default_layout->set_value(docks_section, "dock_split_" + itos(i + 1), 0);
	}
	default_layout->set_value(docks_section, "dock_hsplit_1", 0);
	default_layout->set_value(docks_section, "dock_hsplit_2", 270);
	default_layout->set_value(docks_section, "dock_hsplit_3", -270);
	default_layout->set_value(docks_section, "dock_hsplit_4", 0);

	_update_layouts_menu();

	// Bottom panels.

	bottom_panel = memnew(EditorBottomPanel);
	center_split->add_child(bottom_panel);
	center_split->set_dragger_visibility(SplitContainer::DRAGGER_HIDDEN);

	// The log is owned by the core; the shell parents it into the bottom panel.
	EditorLog *output_log = core->get_log();
	Button *output_button = bottom_panel->add_item(TTRC("Output"), output_log, ED_SHORTCUT_AND_COMMAND("bottom_panels/toggle_output_bottom_panel", TTRC("Toggle Output Bottom Panel"), KeyModifierMask::ALT | Key::O));
	output_log->set_tool_button(output_button);

	core->get_native_shader_source_visualizer()->reparent(gui_base);

	orphan_resources = memnew(OrphanResourcesDialog);
	gui_base->add_child(orphan_resources);

	confirmation = memnew(ConfirmationDialog);
	confirmation_button = confirmation->add_button(TTRC("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	gui_base->add_child(confirmation);
	confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	confirmation->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));
	confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	confirmation->connect("canceled", callable_mp(this, &EditorNode::_cancel_confirmation));

	save_confirmation = memnew(ConfirmationDialog);
	save_confirmation->add_button(TTRC("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	gui_base->add_child(save_confirmation);
	save_confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	save_confirmation->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));
	save_confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	save_confirmation->connect("canceled", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_cancel_close_scene_tab));
	save_confirmation->connect("about_to_popup", callable_mp(this, &EditorNode::_prepare_save_confirmation_popup));

	gradle_build_manage_templates = memnew(ConfirmationDialog);
	gradle_build_manage_templates->set_text(TTR("Android build template is missing, please install relevant templates."));
	gradle_build_manage_templates->set_ok_button_text(TTR("Manage Templates"));
	gradle_build_manage_templates->add_button(TTR("Install from file"))->connect(SceneStringName(pressed), callable_mp(this, &EditorNode::_android_install_build_template));
	gradle_build_manage_templates->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(EDITOR_MANAGE_EXPORT_TEMPLATES));
	gui_base->add_child(gradle_build_manage_templates);

	file_android_build_source = memnew(EditorFileDialog);
	file_android_build_source->set_title(TTR("Select Android sources file"));
	file_android_build_source->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_android_build_source->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_android_build_source->add_filter("*.zip");
	file_android_build_source->connect("file_selected", callable_mp(this, &EditorNode::_android_build_source_selected));
	gui_base->add_child(file_android_build_source);

	{
		VBoxContainer *vbox = memnew(VBoxContainer);
		install_android_build_template_message = memnew(Label);
		install_android_build_template_message->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		install_android_build_template_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		install_android_build_template_message->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
		vbox->add_child(install_android_build_template_message);

		choose_android_export_profile = memnew(OptionButton);
		choose_android_export_profile->connect(SceneStringName(item_selected), callable_mp(this, &EditorNode::_android_export_preset_selected));
		vbox->add_child(choose_android_export_profile);

		install_android_build_template = memnew(ConfirmationDialog);
		install_android_build_template->set_ok_button_text(TTR("Install"));
		install_android_build_template->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_confirm_current));
		install_android_build_template->add_child(vbox);
		install_android_build_template->set_min_size(Vector2(500.0 * EDSCALE, 0));
		gui_base->add_child(install_android_build_template);
	}

	remove_android_build_template = memnew(ConfirmationDialog);
	remove_android_build_template->set_ok_button_text(TTR("Show in File Manager"));
	remove_android_build_template->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_android_explore_build_templates));
	gui_base->add_child(remove_android_build_template);

	file_templates = memnew(EditorFileDialog);
	file_templates->set_title(TTR("Import Templates From ZIP File"));

	gui_base->add_child(file_templates);
	file_templates->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_templates->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_templates->clear_filters();
	file_templates->add_filter("*.tpz", TTR("Template Package"));

	file = memnew(EditorFileDialog);
	gui_base->add_child(file);
	file->set_current_dir("res://");
	file->set_transient_to_focused(true);

	file_export_lib = memnew(EditorFileDialog);
	file_export_lib->set_title(TTR("Export Library"));
	file_export_lib->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_export_lib->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));
	file_export_lib->add_option(TTR("Merge With Existing"), Vector<String>(), true);
	file_export_lib->add_option(TTR("Apply MeshInstance Transforms"), Vector<String>(), false);
	gui_base->add_child(file_export_lib);

	file_script = memnew(EditorFileDialog);
	file_script->set_title(TTR("Open & Run a Script"));
	file_script->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_script->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	List<String> sexts;
	ResourceLoader::get_recognized_extensions_for_type("Script", &sexts);
	for (const String &E : sexts) {
		file_script->add_filter("*." + E);
	}
	gui_base->add_child(file_script);
	file_script->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));

	file_pack_zip = memnew(EditorFileDialog);
	file_pack_zip->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));
	file_pack_zip->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_pack_zip->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_pack_zip->add_filter("*.zip", "ZIP Archive");
	file_pack_zip->set_title(TTR("Pack Project as ZIP..."));
	gui_base->add_child(file_pack_zip);

	file_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));
	file_menu->connect("about_to_popup", callable_mp(this, &EditorNode::_update_file_menu_opened));

	settings_menu->connect(SceneStringName(id_pressed), callable_mp(this, &EditorNode::_menu_option));

	file->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));
	file_templates->connect("file_selected", callable_mp(this, &EditorNode::_dialog_action));

	audio_preview_gen = memnew(AudioStreamPreviewGenerator);
	add_child(audio_preview_gen);

	add_editor_plugin(memnew(DebuggerEditorPlugin(debug_menu)));

	disk_changed = memnew(ConfirmationDialog);
	{
		disk_changed->set_title(TTR("Files have been modified outside Godot"));

		VBoxContainer *vbc = memnew(VBoxContainer);
		disk_changed->add_child(vbc);

		Label *dl = memnew(Label);
		dl->set_text(TTR("The following files are newer on disk:"));
		vbc->add_child(dl);

		disk_changed_list = memnew(Tree);
		disk_changed_list->set_accessibility_name(TTRC("The following files are newer on disk:"));
		vbc->add_child(disk_changed_list);
		disk_changed_list->set_v_size_flags(Control::SIZE_EXPAND_FILL);

		Label *what_action_label = memnew(Label);
		what_action_label->set_text(TTR("What action should be taken?"));
		vbc->add_child(what_action_label);

		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_reload_modified_scenes));
		disk_changed->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_reload_project_settings));
		disk_changed->set_ok_button_text(TTR("Reload from disk"));

		disk_changed->add_button(TTR("Ignore external changes"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "resave");
		disk_changed->connect("custom_action", callable_mp(this, &EditorNode::_resave_externally_modified_scenes));
	}

	gui_base->add_child(disk_changed);

	project_data_missing = memnew(ConfirmationDialog);
	project_data_missing->set_text(TTRC("Project data folder (.godot) is missing. Please restart editor."));
	project_data_missing->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::restart_editor).bind(false));
	project_data_missing->set_ok_button_text(TTRC("Restart"));

	gui_base->add_child(project_data_missing);

	add_editor_plugin(memnew(AnimationPlayerEditorPlugin));
	add_editor_plugin(memnew(AnimationTrackKeyEditEditorPlugin));
	add_editor_plugin(memnew(AnimationMarkerKeyEditEditorPlugin));
	add_editor_plugin(memnew(CanvasItemEditorPlugin));
	add_editor_plugin(memnew(Node3DEditorPlugin));
	add_editor_plugin(memnew(ScriptEditorPlugin));

	if (!Engine::get_singleton()->is_recovery_mode_hint()) {
		add_editor_plugin(get_game_view_plugin());
	}

	EditorAudioBuses *audio_bus_editor = EditorAudioBuses::register_editor();

	ScriptTextEditor::register_editor(); // Register one for text scripts.
	TextEditor::register_editor();

	if (AssetLibraryEditorPlugin::is_available()) {
		add_editor_plugin(memnew(AssetLibraryEditorPlugin));
	} else {
		print_verbose("Asset Library not available (due to using Web editor, or SSL support disabled).");
	}

	// More visually meaningful to have this later.
	bottom_panel->move_item_to_end(AnimationPlayerEditor::get_singleton());

	add_editor_plugin(VersionControlEditorPlugin::get_singleton());

	vcs_actions_menu = VersionControlEditorPlugin::get_singleton()->get_version_control_actions_panel();
	vcs_actions_menu->connect("index_pressed", callable_mp(this, &EditorNode::_version_control_menu_option));
	vcs_actions_menu->add_item(TTRC("Create/Override Version Control Metadata..."), VCS_METADATA);
	vcs_actions_menu->add_item(TTRC("Version Control Settings..."), VCS_SETTINGS);
	project_menu->set_item_submenu_node(project_menu->get_item_index(PROJECT_VERSION_CONTROL), vcs_actions_menu);

	add_editor_plugin(memnew(AudioBusesEditorPlugin(audio_bus_editor)));

	for (int i = 0; i < EditorPlugins::get_plugin_count(); i++) {
		add_editor_plugin(EditorPlugins::create(i));
	}

	for (const StringName &extension_class_name : GDExtensionEditorPlugins::get_extension_classes()) {
		add_extension_editor_plugin(extension_class_name);
	}
	GDExtensionEditorPlugins::editor_node_add_plugin = &EditorPluginManager::_gdextension_add_plugin;
	GDExtensionEditorPlugins::editor_node_remove_plugin = &EditorPluginManager::_gdextension_remove_plugin;

	core->get_resource_preview()->add_preview_generator(Ref<EditorTexturePreviewPlugin>(memnew(EditorTexturePreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorImagePreviewPlugin>(memnew(EditorImagePreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorPackedScenePreviewPlugin>(memnew(EditorPackedScenePreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorMaterialPreviewPlugin>(memnew(EditorMaterialPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorScriptPreviewPlugin>(memnew(EditorScriptPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorAudioStreamPreviewPlugin>(memnew(EditorAudioStreamPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorMeshPreviewPlugin>(memnew(EditorMeshPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorBitmapPreviewPlugin>(memnew(EditorBitmapPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorFontPreviewPlugin>(memnew(EditorFontPreviewPlugin)));
	core->get_resource_preview()->add_preview_generator(Ref<EditorGradientPreviewPlugin>(memnew(EditorGradientPreviewPlugin)));

	update_spinner_step_msec = OS::get_singleton()->get_ticks_msec();
	update_spinner_step_frame = Engine::get_singleton()->get_frames_drawn();

	Ref<GDExtensionExportPlugin> gdextension_export_plugin;
	gdextension_export_plugin.instantiate();

	EditorExport::get_singleton()->add_export_plugin(gdextension_export_plugin);

	Ref<DedicatedServerExportPlugin> dedicated_server_export_plugin;
	dedicated_server_export_plugin.instantiate();

	EditorExport::get_singleton()->add_export_plugin(dedicated_server_export_plugin);

	Ref<ShaderBakerExportPlugin> shader_baker_export_plugin;
	shader_baker_export_plugin.instantiate();

#ifdef VULKAN_ENABLED
	Ref<ShaderBakerExportPluginPlatformVulkan> shader_baker_export_plugin_platform_vulkan;
	shader_baker_export_plugin_platform_vulkan.instantiate();
	shader_baker_export_plugin->add_platform(shader_baker_export_plugin_platform_vulkan);
#endif

#ifdef D3D12_ENABLED
	Ref<ShaderBakerExportPluginPlatformD3D12> shader_baker_export_plugin_platform_d3d12;
	shader_baker_export_plugin_platform_d3d12.instantiate();
	shader_baker_export_plugin->add_platform(shader_baker_export_plugin_platform_d3d12);
#endif

#ifdef METAL_ENABLED
	Ref<ShaderBakerExportPluginPlatformMetal> shader_baker_export_plugin_platform_metal;
	shader_baker_export_plugin_platform_metal.instantiate();
	shader_baker_export_plugin->add_platform(shader_baker_export_plugin_platform_metal);
#endif

	EditorExport::get_singleton()->add_export_plugin(shader_baker_export_plugin);

	Ref<PackedSceneEditorTranslationParserPlugin> packed_scene_translation_parser_plugin;
	packed_scene_translation_parser_plugin.instantiate();
	EditorTranslationParser::get_singleton()->add_parser(packed_scene_translation_parser_plugin, EditorTranslationParser::STANDARD);

	_edit_current();
	EditorSceneManager::get_singleton()->current = nullptr;

	set_process(true);

	open_imported = memnew(ConfirmationDialog);
	open_imported->set_ok_button_text(TTR("Open Anyway"));
	new_inherited_button = open_imported->add_button(TTR("New Inherited"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "inherit");
	open_imported->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_open_imported));
	open_imported->connect("custom_action", callable_mp(this, &EditorNode::_inherit_imported));
	gui_base->add_child(open_imported);

	// The quick-open dialogs are owned by the core; the shell parents them.
	gui_base->add_child(core->get_quick_open_dialog());
	gui_base->add_child(core->get_quick_open_color_palette());

	_update_recent_scenes();

	set_process_shortcut_input(true);

	load_errors = memnew(RichTextLabel);
	load_error_dialog = memnew(AcceptDialog);
	load_error_dialog->set_unparent_when_invisible(true);
	load_error_dialog->add_child(load_errors);
	load_error_dialog->set_title(TTR("Load Errors"));
	load_error_dialog->connect(SceneStringName(visibility_changed), callable_mp(this, &EditorNode::_load_error_dialog_visibility_changed));

	execute_outputs = memnew(RichTextLabel);
	execute_outputs->set_selection_enabled(true);
	execute_outputs->set_context_menu_enabled(true);
	execute_output_dialog = memnew(AcceptDialog);
	execute_output_dialog->set_unparent_when_invisible(true);
	execute_output_dialog->add_child(execute_outputs);
	execute_output_dialog->set_title("");

	EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp(this, &EditorNode::_sources_changed));
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorNode::_fs_changed));
	EditorFileSystem::get_singleton()->connect("resources_reload", callable_mp(this, &EditorNode::_resources_changed));

	pick_main_scene = memnew(ConfirmationDialog);
	gui_base->add_child(pick_main_scene);
	pick_main_scene->set_ok_button_text(TTR("Select"));
	pick_main_scene->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(SETTINGS_PICK_MAIN_SCENE));
	select_current_scene_button = pick_main_scene->add_button(TTR("Select Current"), true, "select_current");
	pick_main_scene->connect("custom_action", callable_mp(this, &EditorNode::_pick_main_scene_custom_action));

	open_project_settings = memnew(ConfirmationDialog);
	gui_base->add_child(open_project_settings);
	open_project_settings->set_ok_button_text(TTRC("Open Project Settings"));
	open_project_settings->connect(SceneStringName(confirmed), callable_mp(this, &EditorNode::_menu_option).bind(PROJECT_OPEN_SETTINGS));

	for (int i = 0; i < EditorPluginManager::_init_callbacks.size(); i++) {
		EditorPluginManager::_init_callbacks[i]();
	}

	get_editor_data().add_edited_scene(-1);
	get_editor_data().set_edited_scene(0);
	scene_tabs->update_scene_tabs();

	ImportDock::get_singleton()->initialize_import_options();

	// Use the Ctrl modifier so F2 can be used to rename nodes in the scene tree dock.
	ED_SHORTCUT_AND_COMMAND("editor/editor_2d", TTRC("Open 2D Editor"), KeyModifierMask::CTRL | Key::F1);
	ED_SHORTCUT_AND_COMMAND("editor/editor_3d", TTRC("Open 3D Editor"), KeyModifierMask::CTRL | Key::F2);
	ED_SHORTCUT_AND_COMMAND("editor/editor_script", TTRC("Open Script Editor"), KeyModifierMask::CTRL | Key::F3);
	ED_SHORTCUT_AND_COMMAND("editor/editor_game", TTRC("Open Game View"), KeyModifierMask::CTRL | Key::F4);
	ED_SHORTCUT_AND_COMMAND("editor/editor_assetlib", TTRC("Open Asset Library"), KeyModifierMask::CTRL | Key::F5);

	ED_SHORTCUT_OVERRIDE("editor/editor_2d", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_1);
	ED_SHORTCUT_OVERRIDE("editor/editor_3d", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_2);
	ED_SHORTCUT_OVERRIDE("editor/editor_script", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_3);
	ED_SHORTCUT_OVERRIDE("editor/editor_game", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_4);
	ED_SHORTCUT_OVERRIDE("editor/editor_assetlib", "macos", KeyModifierMask::META | KeyModifierMask::CTRL | Key::KEY_5);

	ED_SHORTCUT_AND_COMMAND("editor/editor_next", TTRC("Open the next Editor"));
	ED_SHORTCUT_AND_COMMAND("editor/editor_prev", TTRC("Open the previous Editor"));

	// Apply setting presets in case the editor_settings file is missing values.
	EditorSettingsDialog::update_navigation_preset();

	screenshot_timer = memnew(Timer);
	screenshot_timer->set_one_shot(true);
	screenshot_timer->set_wait_time(settings_menu->get_submenu_popup_delay() + 0.1f);
	screenshot_timer->connect("timeout", callable_mp(this, &EditorNode::_request_screenshot));
	add_child(screenshot_timer);
	screenshot_timer->set_owner(get_owner());

	// Adjust spacers to center 2D / 3D / Script buttons.
	if (main_menu_button != nullptr) {
		int max_w = MAX(project_run_bar->get_minimum_size().x + right_menu_hb->get_minimum_size().x, main_menu_button->get_minimum_size().x);
		left_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - main_menu_button->get_minimum_size().x), 0));
		right_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - project_run_bar->get_minimum_size().x - right_menu_hb->get_minimum_size().x), 0));
	} else {
		int max_w = MAX(project_run_bar->get_minimum_size().x + right_menu_hb->get_minimum_size().x, main_menu_bar->get_minimum_size().x);
		left_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - main_menu_bar->get_minimum_size().x), 0));
		right_spacer->set_custom_minimum_size(Size2(MAX(0, max_w - project_run_bar->get_minimum_size().x - right_menu_hb->get_minimum_size().x), 0));
	}
	// Extend menu bar to window title.
	if (can_expand) {
		DisplayServer::get_singleton()->process_events();
		DisplayServer::get_singleton()->window_set_flag(DisplayServer::WINDOW_FLAG_EXTEND_TO_TITLE, true, DisplayServer::MAIN_WINDOW_ID);
		title_bar->set_can_move_window(true);
	}

	{
		const String exec = OS::get_singleton()->get_executable_path();
		const String old_exec = EditorSettings::get_singleton()->get_project_metadata("editor_metadata", "executable_path", "");
		// Save editor executable path for third-party tools.
		if (exec != old_exec) {
			EditorSettings::get_singleton()->set_project_metadata("editor_metadata", "executable_path", exec);
		}
	}

	follow_system_theme = EDITOR_GET("interface/theme/follow_system_theme");
	use_system_accent_color = EDITOR_GET("interface/theme/use_system_accent_color");
	system_theme_timer = memnew(Timer);
	system_theme_timer->set_wait_time(1.0);
	system_theme_timer->connect("timeout", callable_mp(this, &EditorNode::_check_system_theme_changed));
	add_child(system_theme_timer);
	system_theme_timer->set_owner(get_owner());
	system_theme_timer->set_autostart(true);
}

void EditorNode::add_control_to_dock(int p_slot, Control *p_control, const Ref<Shortcut> &p_shortcut) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_NULL(editor_dock_manager);
	editor_dock_manager->add_dock(p_control, String(), EditorDockManager::DockSlot(p_slot), p_shortcut);
}

void EditorNode::remove_control_from_docks(Control *p_control) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_NULL(editor_dock_manager);
	editor_dock_manager->remove_dock(p_control);
}

void EditorNode::set_dock_tab_icon(Control *p_control, const Ref<Texture2D> &p_icon) {
	ERR_FAIL_NULL(p_control);
	ERR_FAIL_NULL(editor_dock_manager);
	editor_dock_manager->set_dock_tab_icon(p_control, p_icon);
}

EditorNode::~EditorNode() {
	EditorInspector::cleanup_plugins();
	EditorTranslationParser::get_singleton()->clean_parsers();
	ResourceImporterScene::clean_up_importer_plugins();
	EditorContextMenuPluginManager::cleanup();

	EditorHelp::cleanup_doc();
#if defined(MODULE_GDSCRIPT_ENABLED) || defined(MODULE_MONO_ENABLED)
	EditorHelpHighlighter::free_singleton();
#endif
	memdelete(progress_hb);
	memdelete(project_upgrade_tool);
	memdelete(editor_dock_manager);

	GDExtensionEditorPlugins::editor_node_add_plugin = nullptr;
	GDExtensionEditorPlugins::editor_node_remove_plugin = nullptr;

	singleton = nullptr;
}
