/**************************************************************************/
/*  editor_plugin_manager.cpp                                             */
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

#include "editor_plugin_manager.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "editor/editor_core.h"
#include "editor/editor_main_screen.h"
#include "editor/editor_node.h"
#include "editor/editor_scene_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/plugins/editor_plugin_list.h"
#include "editor/plugins/editor_resource_conversion_plugin.h"
#include "editor/scene/material_editor_plugin.h"
#include "editor/scene/particle_process_material_editor_plugin.h"
#include "editor/settings/project_settings_editor.h"
#include "editor/shader/visual_shader_editor_plugin.h"
#include "scene/main/node.h"

EditorPluginManager *EditorPluginManager::singleton = nullptr;

EditorBuildCallback EditorPluginManager::build_callbacks[EditorPluginManager::MAX_BUILD_CALLBACKS];
int EditorPluginManager::build_callback_count = 0;

EditorPluginInitializeCallback EditorPluginManager::plugin_init_callbacks[EditorPluginManager::MAX_INIT_CALLBACKS];
int EditorPluginManager::plugin_init_callback_count = 0;

Vector<EditorNodeInitCallback> EditorPluginManager::_init_callbacks;

EditorPluginManager::EditorPluginManager() {
	DEV_ASSERT(!singleton);
	singleton = this;
	editor_plugins_over = memnew(EditorPluginList);
	editor_plugins_force_over = memnew(EditorPluginList);
	editor_plugins_force_input_forwarding = memnew(EditorPluginList);

	// Register the built-in resource-conversion plugins.
	{
		Ref<StandardMaterial3DConversionPlugin> spatial_mat_convert;
		spatial_mat_convert.instantiate();
		resource_conversion_plugins.push_back(spatial_mat_convert);

		Ref<ORMMaterial3DConversionPlugin> orm_mat_convert;
		orm_mat_convert.instantiate();
		resource_conversion_plugins.push_back(orm_mat_convert);

		Ref<CanvasItemMaterialConversionPlugin> canvas_item_mat_convert;
		canvas_item_mat_convert.instantiate();
		resource_conversion_plugins.push_back(canvas_item_mat_convert);

		Ref<ParticleProcessMaterialConversionPlugin> particles_mat_convert;
		particles_mat_convert.instantiate();
		resource_conversion_plugins.push_back(particles_mat_convert);

		Ref<ProceduralSkyMaterialConversionPlugin> procedural_sky_mat_convert;
		procedural_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(procedural_sky_mat_convert);

		Ref<PanoramaSkyMaterialConversionPlugin> panorama_sky_mat_convert;
		panorama_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(panorama_sky_mat_convert);

		Ref<PhysicalSkyMaterialConversionPlugin> physical_sky_mat_convert;
		physical_sky_mat_convert.instantiate();
		resource_conversion_plugins.push_back(physical_sky_mat_convert);

		Ref<FogMaterialConversionPlugin> fog_mat_convert;
		fog_mat_convert.instantiate();
		resource_conversion_plugins.push_back(fog_mat_convert);

		Ref<VisualShaderConversionPlugin> vshader_convert;
		vshader_convert.instantiate();
		resource_conversion_plugins.push_back(vshader_convert);
	}
}

EditorPluginManager::~EditorPluginManager() {
	memdelete(editor_plugins_over);
	memdelete(editor_plugins_force_over);
	memdelete(editor_plugins_force_input_forwarding);
	singleton = nullptr;
}

void EditorPluginManager::init_plugins() {
	_initializing_plugins = true;
	Vector<String> addons;
	if (ProjectSettings::get_singleton()->has_setting("editor_plugins/enabled")) {
		addons = GLOBAL_GET("editor_plugins/enabled");
	}

	for (const String &addon : addons) {
		set_addon_plugin_enabled(addon, true);
	}
	_initializing_plugins = false;

	if (!pending_addons.is_empty()) {
		EditorFileSystem::get_singleton()->connect("script_classes_updated", callable_mp(this, &EditorPluginManager::_enable_pending_addons), CONNECT_ONE_SHOT);
	}
}

void EditorPluginManager::_on_plugin_ready(Object *p_script, const String &p_activate_name) {
	Ref<Script> scr = Object::cast_to<Script>(p_script);
	if (scr.is_null()) {
		return;
	}
	ERR_FAIL_NULL(core);
	core->get_project_settings_editor()->update_plugins();
	core->get_project_settings_editor()->hide();
	core->get_scene_manager()->push_item(scr.operator->());
	if (p_activate_name.length()) {
		set_addon_plugin_enabled(p_activate_name, true);
	}
}

void EditorPluginManager::_remove_plugin_from_enabled(const String &p_name) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	PackedStringArray enabled_plugins = ps->get("editor_plugins/enabled");
	for (int i = 0; i < enabled_plugins.size(); ++i) {
		if (enabled_plugins.get(i) == p_name) {
			enabled_plugins.remove_at(i);
			break;
		}
	}
	ps->set("editor_plugins/enabled", enabled_plugins);
}

void EditorPluginManager::_plugin_over_edit(EditorPlugin *p_plugin, Object *p_object) {
	if (p_object) {
		editor_plugins_over->add_plugin(p_plugin);
		p_plugin->edit(p_object);
		p_plugin->make_visible(true);
	} else {
		editor_plugins_over->remove_plugin(p_plugin);
		p_plugin->edit(nullptr);
		p_plugin->make_visible(false);
	}
}

void EditorPluginManager::_plugin_over_self_own(EditorPlugin *p_plugin) {
	active_plugins[p_plugin->get_instance_id()].insert(p_plugin);
}

void EditorPluginManager::add_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	ERR_FAIL_NULL(core);
	if (p_editor->has_main_screen() && shell) {
		shell->get_editor_main_screen()->add_main_plugin(p_editor);
	}
	core->get_editor_data().add_editor_plugin(p_editor);
	EditorNode::get_singleton()->add_child(p_editor);
	if (p_config_changed) {
		p_editor->enable_plugin();
	}
}

void EditorPluginManager::remove_editor_plugin(EditorPlugin *p_editor, bool p_config_changed) {
	ERR_FAIL_NULL(core);
	if (p_editor->has_main_screen() && shell) {
		shell->get_editor_main_screen()->remove_main_plugin(p_editor);
	}
	p_editor->make_visible(false);
	p_editor->clear();
	if (p_config_changed) {
		p_editor->disable_plugin();
	}
	editor_plugins_over->remove_plugin(p_editor);
	editor_plugins_force_over->remove_plugin(p_editor);
	editor_plugins_force_input_forwarding->remove_plugin(p_editor);
	EditorNode::get_singleton()->remove_child(p_editor);
	core->get_editor_data().remove_editor_plugin(p_editor);

	for (KeyValue<ObjectID, HashSet<EditorPlugin *>> &kv : active_plugins) {
		kv.value.erase(p_editor);
	}
}

void EditorPluginManager::_gdextension_add_plugin(const StringName &p_class_name) {
	ERR_FAIL_NULL(singleton);
	singleton->add_extension_editor_plugin(p_class_name);
}

void EditorPluginManager::_gdextension_remove_plugin(const StringName &p_class_name) {
	ERR_FAIL_NULL(singleton);
	singleton->remove_extension_editor_plugin(p_class_name);
}

void EditorPluginManager::add_extension_editor_plugin(const StringName &p_class_name) {
	ERR_FAIL_NULL(core);
	ERR_FAIL_COND_MSG(!ClassDB::class_exists(p_class_name), vformat("No such editor plugin registered: %s", p_class_name));
	ERR_FAIL_COND_MSG(!ClassDB::is_parent_class(p_class_name, SNAME("EditorPlugin")), vformat("Class is not an editor plugin: %s", p_class_name));
	ERR_FAIL_COND_MSG(core->get_editor_data().has_extension_editor_plugin(p_class_name), vformat("Editor plugin already added for class: %s", p_class_name));

	EditorPlugin *plugin = Object::cast_to<EditorPlugin>(ClassDB::_instantiate_allow_unexposed(p_class_name));
	core->get_editor_data().add_extension_editor_plugin(p_class_name, plugin);
	add_editor_plugin(plugin);
}

void EditorPluginManager::remove_extension_editor_plugin(const StringName &p_class_name) {
	// If we're exiting, the editor plugins will get cleaned up anyway, so don't do anything.
	if (!core || (shell && shell->is_exiting())) {
		return;
	}

	ERR_FAIL_NULL(core);
	ERR_FAIL_COND_MSG(!core->get_editor_data().has_extension_editor_plugin(p_class_name), vformat("No editor plugin added for class: %s", p_class_name));

	EditorPlugin *plugin = core->get_editor_data().get_extension_editor_plugin(p_class_name);
	remove_editor_plugin(plugin);
	memdelete(plugin);
	core->get_editor_data().remove_extension_editor_plugin(p_class_name);
}

void EditorPluginManager::unload_editor_addons() {
	auto &addons = addon_name_to_plugin;
	for (const KeyValue<String, EditorPlugin *> &E : addons) {
		print_verbose(vformat("Unloading addon: %s", E.key));
		remove_editor_plugin(E.value, false);
		memdelete(E.value);
	}

	addons.clear();
}

void EditorPluginManager::_update_addon_config() {
	if (_initializing_plugins) {
		return;
	}

	Vector<String> enabled_addons;

	for (const KeyValue<String, EditorPlugin *> &E : addon_name_to_plugin) {
		enabled_addons.push_back(E.key);
	}

	if (enabled_addons.is_empty()) {
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", Variant());
	} else {
		enabled_addons.sort();
		ProjectSettings::get_singleton()->set("editor_plugins/enabled", enabled_addons);
	}

	if (core) {
		core->get_project_settings_editor()->queue_save();
	}
}

void EditorPluginManager::set_addon_plugin_enabled(const String &p_addon, bool p_enabled, bool p_config_changed) {
	String addon_path = p_addon;

	if (!addon_path.begins_with("res://")) {
		addon_path = "res://addons/" + addon_path + "/plugin.cfg";
	}

	ERR_FAIL_COND(p_enabled && addon_name_to_plugin.has(addon_path));
	ERR_FAIL_COND(!p_enabled && !addon_name_to_plugin.has(addon_path));

	if (!p_enabled) {
		EditorPlugin *addon = addon_name_to_plugin[addon_path];
		remove_editor_plugin(addon, p_config_changed);
		memdelete(addon);
		addon_name_to_plugin.erase(addon_path);
		_update_addon_config();
		return;
	}

	Ref<ConfigFile> cf;
	cf.instantiate();
	if (!DirAccess::exists(addon_path.get_base_dir())) {
		_remove_plugin_from_enabled(addon_path);
		WARN_PRINT("Addon '" + addon_path + "' failed to load. No directory found. Removing from enabled plugins.");
		return;
	}
	Error err = cf->load(addon_path);
	if (err != OK) {
		if (shell) {
			shell->show_warning(vformat(TTR("Unable to enable addon plugin at: '%s' parsing of config failed."), addon_path));
		}
		return;
	}

	String plugin_version;
	if (cf->has_section_key("plugin", "version")) {
		plugin_version = cf->get_value("plugin", "version");
	}

	if (!cf->has_section_key("plugin", "script")) {
		if (shell) {
			shell->show_warning(vformat(TTR("Unable to find script field for addon plugin at: '%s'."), addon_path));
		}
		return;
	}

	String script_path = cf->get_value("plugin", "script");
	Ref<Script> scr; // We need to save it for creating "ep" below.

	// Only try to load the script if it has a name. Else, the plugin has no init script.
	if (script_path.length() > 0) {
		script_path = addon_path.get_base_dir().path_join(script_path);
		// We should not use the cached version on startup to prevent a script reload
		// if it is already loaded and potentially running from autoloads. See GH-100750.
		scr = ResourceLoader::load(script_path, "Script", EditorFileSystem::get_singleton()->doing_first_scan() ? ResourceFormatLoader::CACHE_MODE_REUSE : ResourceFormatLoader::CACHE_MODE_IGNORE);

		if (scr.is_null()) {
			if (shell) {
				shell->show_warning(vformat(TTR("Unable to load addon script from path: '%s'."), script_path));
			}
			return;
		}

		// Errors in the script cause the base_type to be an empty StringName.
		if (scr->get_instance_base_type() == StringName()) {
			if (_initializing_plugins) {
				// However, if it happens during initialization, waiting for file scan might help.
				pending_addons.push_back(p_addon);
				return;
			}

			if (shell) {
				shell->show_warning(vformat(TTR("Unable to load addon script from path: '%s'. This might be due to a code error in that script.\nDisabling the addon at '%s' to prevent further errors."), script_path, addon_path));
			}
			_remove_plugin_from_enabled(addon_path);
			return;
		}

		// Plugin init scripts must inherit from EditorPlugin and be tools.
		if (!ClassDB::is_parent_class(scr->get_instance_base_type(), "EditorPlugin")) {
			if (shell) {
				shell->show_warning(vformat(TTR("Unable to load addon script from path: '%s'. Base type is not 'EditorPlugin'."), script_path));
			}
			return;
		}

		if (!scr->is_tool()) {
			if (shell) {
				shell->show_warning(vformat(TTR("Unable to load addon script from path: '%s'. Script is not in tool mode."), script_path));
			}
			return;
		}
	}

	EditorPlugin *ep = memnew(EditorPlugin);
	ep->set_script(scr);
	ep->set_plugin_version(plugin_version);
	addon_name_to_plugin[addon_path] = ep;
	add_editor_plugin(ep, p_config_changed);

	_update_addon_config();
}

bool EditorPluginManager::is_addon_plugin_enabled(const String &p_addon) const {
	if (p_addon.begins_with("res://")) {
		return addon_name_to_plugin.has(p_addon);
	}

	return addon_name_to_plugin.has("res://addons/" + p_addon + "/plugin.cfg");
}

void EditorPluginManager::_enable_pending_addons() {
	for (uint32_t i = 0; i < pending_addons.size(); i++) {
		set_addon_plugin_enabled(pending_addons[i], true);
	}
	pending_addons.clear();
}

void EditorPluginManager::add_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	resource_conversion_plugins.push_back(p_plugin);
}

void EditorPluginManager::remove_resource_conversion_plugin(const Ref<EditorResourceConversionPlugin> &p_plugin) {
	resource_conversion_plugins.erase(p_plugin);
}

Vector<Ref<EditorResourceConversionPlugin>> EditorPluginManager::find_resource_conversion_plugin_for_resource(const Ref<Resource> &p_for_resource) {
	if (p_for_resource.is_null()) {
		return Vector<Ref<EditorResourceConversionPlugin>>();
	}

	Vector<Ref<EditorResourceConversionPlugin>> ret;
	for (Ref<EditorResourceConversionPlugin> resource_conversion_plugin : resource_conversion_plugins) {
		if (resource_conversion_plugin.is_valid() && resource_conversion_plugin->handles(p_for_resource)) {
			ret.push_back(resource_conversion_plugin);
		}
	}

	return ret;
}

Vector<Ref<EditorResourceConversionPlugin>> EditorPluginManager::find_resource_conversion_plugin_for_type_name(const String &p_type) {
	Vector<Ref<EditorResourceConversionPlugin>> ret;

	if (ClassDB::class_exists(p_type) && ClassDB::can_instantiate(p_type)) {
		Ref<Resource> temp = Object::cast_to<Resource>(ClassDB::instantiate(p_type));
		if (temp.is_valid()) {
			for (Ref<EditorResourceConversionPlugin> resource_conversion_plugin : resource_conversion_plugins) {
				if (resource_conversion_plugin.is_valid() && resource_conversion_plugin->handles(temp)) {
					ret.push_back(resource_conversion_plugin);
				}
			}
		}
	}

	return ret;
}

void EditorPluginManager::add_plugin_init_callback(EditorPluginInitializeCallback p_callback) {
	ERR_FAIL_COND(plugin_init_callback_count == MAX_INIT_CALLBACKS);

	plugin_init_callbacks[plugin_init_callback_count++] = p_callback;
}

void EditorPluginManager::add_build_callback(EditorBuildCallback p_callback) {
	ERR_FAIL_COND(build_callback_count == MAX_BUILD_CALLBACKS);

	build_callbacks[build_callback_count++] = p_callback;
}

bool EditorPluginManager::call_build() {
	bool builds_successful = true;

	for (int i = 0; i < build_callback_count && builds_successful; i++) {
		if (!build_callbacks[i]()) {
			ERR_PRINT("A Redot Engine build callback failed.");
			builds_successful = false;
		}
	}

	if (builds_successful && core && !core->get_editor_data().call_build()) {
		ERR_PRINT("An EditorPlugin build callback failed.");
		builds_successful = false;
	}

	return builds_successful;
}

void EditorPluginManager::cleanup() {
	_init_callbacks.clear();
}

void EditorPluginManager::_load_editor_plugin_states_from_config(const Ref<ConfigFile> &p_config_file) {
	ERR_FAIL_NULL(core);
	Node *scene = core->get_editor_data().get_edited_scene_root();

	if (!scene) {
		return;
	}

	Vector<String> esl = p_config_file->get_section_keys("editor_states");

	Dictionary md;
	for (const String &E : esl) {
		Variant st = p_config_file->get_value("editor_states", E);
		if (st.get_type() != Variant::NIL) {
			md[E] = st;
		}
	}

	core->get_editor_data().set_editor_plugin_states(md);
}
