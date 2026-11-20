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
	EditorShell::_update_theme(p_skip_creation);
}

void EditorNode::_apply_theme_styles(const Ref<Theme> &p_theme) {
	// Update styles.
	{
		bool global_menu = !bool(EDITOR_GET("interface/editor/use_embedded_menu")) && NativeMenu::get_singleton()->has_feature(NativeMenu::FEATURE_GLOBAL_MENU);
		bool dark_mode = DisplayServer::get_singleton()->is_dark_mode_supported() && DisplayServer::get_singleton()->is_dark_mode();

		gui_base->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("Background"), EditorStringName(EditorStyles)));
		main_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, p_theme->get_constant(SNAME("window_border_margin"), EditorStringName(Editor)));
		main_vbox->add_theme_constant_override("separation", p_theme->get_constant(SNAME("top_bar_separation"), EditorStringName(Editor)));

		if (main_menu_button != nullptr) {
			main_menu_button->set_button_icon(p_theme->get_icon(SNAME("TripleBar"), EditorStringName(EditorIcons)));
		}

		editor_main_screen->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("Content"), EditorStringName(EditorStyles)));
		bottom_panel->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("BottomPanel"), EditorStringName(EditorStyles)));
		distraction_free->set_button_icon(p_theme->get_icon(SNAME("DistractionFree"), EditorStringName(EditorIcons)));
		distraction_free->add_theme_style_override(SceneStringName(pressed), p_theme->get_stylebox(CoreStringName(normal), "FlatMenuButton"));

		help_menu->set_item_icon(help_menu->get_item_index(HELP_SEARCH), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("HelpSearch"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_COPY_SYSTEM_INFO), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("ActionCopy"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_ABOUT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Godot"), global_menu, dark_mode));
		help_menu->set_item_icon(help_menu->get_item_index(HELP_SUPPORT_REDOT_DEVELOPMENT), EditorCore::get_singleton()->_get_editor_theme_native_menu_icon(SNAME("Heart"), global_menu, dark_mode));

		if (EditorDebuggerNode::get_singleton()->is_visible()) {
			bottom_panel->add_theme_style_override(SceneStringName(panel), p_theme->get_stylebox(SNAME("BottomPanelDebuggerOverride"), EditorStringName(EditorStyles)));
		}

		_update_renderer_color();
	}

	editor_dock_manager->update_tab_styles();
	editor_dock_manager->update_docks_menu();
	editor_dock_manager->set_tab_icon_max_width(p_theme->get_constant(SNAME("class_icon_size"), EditorStringName(Editor)));
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

void EditorNode::_fs_changed() {
	EditorShell::_fs_changed();

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

void EditorNode::_update_title_label(const String &p_title) {
	if (project_title) {
		project_title->set_text(p_title);
	}
}

void EditorNode::_open_command_palette() {
	EditorCore::get_singleton()->get_command_palette()->open_popup();
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

	if (!cmdline_mode) {
		EditorResourcePreview::get_singleton()->check_for_invalidation(p_file);
	}

	_close_save_scene_progress();
}

void EditorNode::_close_save_scene_progress() {
	memdelete_notnull(save_scene_progress);
	save_scene_progress = nullptr;
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

void EditorNode::_palette_quick_open_dialog() {
	EditorQuickOpenDialog *color_palette = EditorCore::get_singleton()->get_quick_open_color_palette();
	color_palette->popup_dialog({ "ColorPalette" }, EditorCore::get_singleton()->get_palette_file_selected_callback());
	color_palette->set_title(TTRC("Quick Open Color Palette..."));
}

void EditorNode::_close_messages() {
	old_split_ofs = center_split->get_split_offset();
	center_split->set_split_offset(0);
}

void EditorNode::_show_messages() {
	center_split->set_split_offset(old_split_ofs);
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

void EditorNode::_load_editor_layout_on_first_scan() {
	_load_editor_layout_to_config();
}

void EditorNode::_save_editor_layout_on_exit() {
	_save_editor_layout_to_config();
}

void EditorNode::_save_layout(const String &p_file) {
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
}

void EditorNode::_delete_layout(const String &p_file) {
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
}

void EditorNode::_apply_default_layout() {
	editor_dock_manager->load_docks_from_config(default_layout, "docks");
	_save_editor_layout_to_config();
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

	EditorSceneManager::get_singleton()->set_restoring_scenes(true);

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

	EditorSceneManager::get_singleton()->set_restoring_scenes(false);
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
			_apply_default_layout();
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

	ClassDB::bind_method("stop_child_process", &EditorShell::stop_child_process);

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

EditorNode::EditorNode() {
	DEV_ASSERT(!singleton);
	singleton = this;

	ERR_FAIL_NULL(EditorCore::get_singleton());

	// Register this shell with the core; this also wires the shell's
	// back-pointers to the core, scene manager, and plugin manager.
	EditorCore::get_singleton()->set_shell(this);
	ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &EditorNode::_update_from_settings));

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
	base_vbox->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT, Control::PRESET_MODE_MINSIZE, EditorCore::get_singleton()->get_editor_theme()->get_constant(SNAME("window_border_margin"), EditorStringName(Editor)));

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
	save_accept->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_option).bind((int)MenuOptions::SCENE_SAVE_AS_SCENE));

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
	recent_scenes->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_open_recent_scene));

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
		apple_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));
	}
#endif

	project_menu = memnew(PopupMenu);
	_add_to_main_menu(TTRC("Project"), project_menu);

	project_menu->add_shortcut(ED_SHORTCUT_AND_COMMAND("editor/project_settings", TTRC("Project Settings..."), Key::NONE, TTRC("Project Settings")), PROJECT_OPEN_SETTINGS);
	project_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

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
	tool_menu->connect("index_pressed", callable_mp((EditorShell *)this, &EditorShell::_tool_menu_option));
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

	help_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

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
	project_run_bar->connect("play_pressed", callable_mp((EditorShell *)this, &EditorShell::_project_run_started));
	project_run_bar->connect("stop_pressed", callable_mp((EditorShell *)this, &EditorShell::_project_run_stopped));

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
	layout_dialog->connect("name_confirmed", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));

	update_spinner = memnew(MenuButton);
	right_menu_hb->add_child(update_spinner);
	update_spinner->set_button_icon(EditorCore::get_singleton()->get_editor_theme()->get_icon(SNAME("Progress1"), EditorStringName(EditorIcons)));
	update_spinner->get_popup()->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));
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
	filesystem_dock->connect("inherit", callable_mp((EditorShell *)this, &EditorShell::_inherit_request));
	filesystem_dock->connect("instantiate", callable_mp((EditorShell *)this, &EditorShell::_instantiate_request));
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
	confirmation->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_confirm_current));
	confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	confirmation->connect("canceled", callable_mp((EditorShell *)this, &EditorShell::_cancel_confirmation));

	save_confirmation = memnew(ConfirmationDialog);
	save_confirmation->add_button(TTRC("Don't Save"), DisplayServer::get_singleton()->get_swap_cancel_ok(), "discard");
	gui_base->add_child(save_confirmation);
	save_confirmation->set_min_size(Vector2(450.0 * EDSCALE, 0));
	save_confirmation->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_confirm_current));
	save_confirmation->connect("custom_action", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_discard_changes));
	save_confirmation->connect("canceled", callable_mp(EditorSceneManager::get_singleton(), &EditorSceneManager::_cancel_close_scene_tab));
	save_confirmation->connect("about_to_popup", callable_mp((EditorShell *)this, &EditorShell::_prepare_save_confirmation_popup));

	gradle_build_manage_templates = memnew(ConfirmationDialog);
	gradle_build_manage_templates->set_text(TTR("Android build template is missing, please install relevant templates."));
	gradle_build_manage_templates->set_ok_button_text(TTR("Manage Templates"));
	gradle_build_manage_templates->add_button(TTR("Install from file"))->connect(SceneStringName(pressed), callable_mp((EditorShell *)this, &EditorShell::_android_install_build_template));
	gradle_build_manage_templates->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_option).bind(EDITOR_MANAGE_EXPORT_TEMPLATES));
	gui_base->add_child(gradle_build_manage_templates);

	file_android_build_source = memnew(EditorFileDialog);
	file_android_build_source->set_title(TTR("Select Android sources file"));
	file_android_build_source->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_android_build_source->set_file_mode(EditorFileDialog::FILE_MODE_OPEN_FILE);
	file_android_build_source->add_filter("*.zip");
	file_android_build_source->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_android_build_source_selected));
	gui_base->add_child(file_android_build_source);

	{
		VBoxContainer *vbox = memnew(VBoxContainer);
		install_android_build_template_message = memnew(Label);
		install_android_build_template_message->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
		install_android_build_template_message->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		install_android_build_template_message->set_custom_minimum_size(Size2(300 * EDSCALE, 1));
		vbox->add_child(install_android_build_template_message);

		choose_android_export_profile = memnew(OptionButton);
		choose_android_export_profile->connect(SceneStringName(item_selected), callable_mp((EditorShell *)this, &EditorShell::_android_export_preset_selected));
		vbox->add_child(choose_android_export_profile);

		install_android_build_template = memnew(ConfirmationDialog);
		install_android_build_template->set_ok_button_text(TTR("Install"));
		install_android_build_template->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_confirm_current));
		install_android_build_template->add_child(vbox);
		install_android_build_template->set_min_size(Vector2(500.0 * EDSCALE, 0));
		gui_base->add_child(install_android_build_template);
	}

	remove_android_build_template = memnew(ConfirmationDialog);
	remove_android_build_template->set_ok_button_text(TTR("Show in File Manager"));
	remove_android_build_template->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_android_explore_build_templates));
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
	file_export_lib->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));
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
	file_script->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));

	file_pack_zip = memnew(EditorFileDialog);
	file_pack_zip->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));
	file_pack_zip->set_file_mode(EditorFileDialog::FILE_MODE_SAVE_FILE);
	file_pack_zip->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	file_pack_zip->add_filter("*.zip", "ZIP Archive");
	file_pack_zip->set_title(TTR("Pack Project as ZIP..."));
	gui_base->add_child(file_pack_zip);

	file_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));
	file_menu->connect("about_to_popup", callable_mp((EditorShell *)this, &EditorShell::_update_file_menu_opened));

	settings_menu->connect(SceneStringName(id_pressed), callable_mp((EditorShell *)this, &EditorShell::_menu_option));

	file->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));
	file_templates->connect("file_selected", callable_mp((EditorShell *)this, &EditorShell::_dialog_action));

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
	project_data_missing->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::restart_editor).bind(false));
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
	EditorSceneManager::get_singleton()->set_current_object(nullptr);

	set_process(true);

	open_imported = memnew(ConfirmationDialog);
	open_imported->set_ok_button_text(TTR("Open Anyway"));
	new_inherited_button = open_imported->add_button(TTR("New Inherited"), !DisplayServer::get_singleton()->get_swap_cancel_ok(), "inherit");
	open_imported->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_open_imported));
	open_imported->connect("custom_action", callable_mp((EditorShell *)this, &EditorShell::_inherit_imported));
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

	EditorFileSystem::get_singleton()->connect("sources_changed", callable_mp((EditorShell *)this, &EditorShell::_sources_changed));
	EditorFileSystem::get_singleton()->connect("filesystem_changed", callable_mp(this, &EditorNode::_fs_changed));
	EditorFileSystem::get_singleton()->connect("resources_reload", callable_mp((EditorShell *)this, &EditorShell::_resources_changed));

	pick_main_scene = memnew(ConfirmationDialog);
	gui_base->add_child(pick_main_scene);
	pick_main_scene->set_ok_button_text(TTR("Select"));
	pick_main_scene->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_option).bind(SETTINGS_PICK_MAIN_SCENE));
	select_current_scene_button = pick_main_scene->add_button(TTR("Select Current"), true, "select_current");
	pick_main_scene->connect("custom_action", callable_mp((EditorShell *)this, &EditorShell::_pick_main_scene_custom_action));

	open_project_settings = memnew(ConfirmationDialog);
	gui_base->add_child(open_project_settings);
	open_project_settings->set_ok_button_text(TTRC("Open Project Settings"));
	open_project_settings->connect(SceneStringName(confirmed), callable_mp((EditorShell *)this, &EditorShell::_menu_option).bind(PROJECT_OPEN_SETTINGS));

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
