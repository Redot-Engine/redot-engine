/**************************************************************************/
/*  mcp_tools.cpp                                                         */
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

#include "mcp_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/class_db.h"
#include "core/version.h"
#include "mcp_bridge.h"
#include "mcp_server.h"
#include "scene/resources/packed_scene.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript_parser.h"
#endif

#include "core/doc_data.h"
#include <functional>

// ============================================================================
// Helpers
// ============================================================================

Variant MCPTools::_json_to_variant(const Variant &p_json, Variant::Type p_type) {
	if (p_json.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_json;
		if (d.has("x") && d.has("y")) {
			if (d.has("z")) {
				return Vector3(d["x"], d["y"], d["z"]);
			}
			return Vector2(d["x"], d["y"]);
		}
		if (d.has("r") && d.has("g") && d.has("b")) {
			return Color(d["r"], d["g"], d["b"], d.get("a", 1.0));
		}
	}
	return p_json;
}

Error MCPTools::_ensure_callback_exists(const String &p_script_path, const String &p_callback_name) {
	String path = normalize_path(p_script_path);
	Error err;
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	String content = f->get_as_text();
	f.unref();

	if (content.contains("func " + p_callback_name)) {
		return OK;
	}

	// Append callback
	f = FileAccess::open(path, FileAccess::WRITE, &err);
	if (err != OK) {
		return err;
	}

	if (!content.ends_with("\n")) {
		content += "\n";
	}
	content += "\nfunc " + p_callback_name + "():\n\tpass # Added by MCP\n";
	f->store_string(content);
	return OK;
}

MCPTools::MCPTools() {
}

MCPTools::~MCPTools() {
}

void MCPTools::_bind_methods() {
}

// ============================================================================
// Path Utilities
// ============================================================================

String MCPTools::normalize_path(const String &p_path) {
	if (p_path.begins_with("res://")) {
		return p_path;
	}
	String project_path = ProjectSettings::get_singleton()->get_resource_path();
	if (p_path.begins_with(project_path)) {
		return "res://" + p_path.substr(project_path.length()).lstrip("/");
	}
	return "res://" + p_path.lstrip("/");
}

bool MCPTools::validate_path(const String &p_path) {
	String normalized = normalize_path(p_path);
	if (normalized.contains("..") || !normalized.begins_with("res://")) {
		return false;
	}
	return true;
}

String MCPTools::get_absolute_path(const String &p_path) {
	return ProjectSettings::get_singleton()->globalize_path(normalize_path(p_path));
}

// ============================================================================
// Tool Definitions
// ============================================================================

Array MCPTools::get_tool_definitions() {
	Array tools;

	// scene_action
	{
		Dictionary props;
		props["action"] = MCPSchemaBuilder::make_string_property("Action: 'add', 'remove', 'instance', 'set_prop', 'connect', 'get_node', 'reparent', 'create'");
		props["scene_path"] = MCPSchemaBuilder::make_string_property("Path to scene file");
		props["node_path"] = MCPSchemaBuilder::make_string_property("Target node path ('.' for root)");
		props["node_type"] = MCPSchemaBuilder::make_string_property("Type for 'add' or 'create'");
		props["node_name"] = MCPSchemaBuilder::make_string_property("Name for node");
		props["property"] = MCPSchemaBuilder::make_string_property("Property name");
		props["value"] = MCPSchemaBuilder::make_object_property("Value (supports numbers, strings, and objects like {x:0, y:0})");
		props["signal"] = MCPSchemaBuilder::make_string_property("Signal name");
		props["target_node"] = MCPSchemaBuilder::make_string_property("Target node path for connect/reparent");
		props["method"] = MCPSchemaBuilder::make_string_property("Method name for connect");
		props["instance_path"] = MCPSchemaBuilder::make_string_property("Path to scene to instance");

		Array required;
		required.push_back("action");
		required.push_back("scene_path");

		Dictionary tool;
		tool["name"] = "scene_action";
		tool["description"] = "Perform actions within a scene file (add nodes, set properties, wire signals). IMPORTANT: Always use this tool for .tscn files instead of direct text editing to maintain project integrity.";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// resource_action
	{
		Dictionary props;
		props["action"] = MCPSchemaBuilder::make_string_property("Action: 'create', 'modify', 'inspect', 'duplicate', 'inspect_asset'");
		props["path"] = MCPSchemaBuilder::make_string_property("Path to resource (.tres) or asset");
		props["type"] = MCPSchemaBuilder::make_string_property("Type for 'create'");
		props["property"] = MCPSchemaBuilder::make_string_property("Property name");
		props["value"] = MCPSchemaBuilder::make_object_property("Value");
		props["new_path"] = MCPSchemaBuilder::make_string_property("Path for 'duplicate'");

		Array required;
		required.push_back("action");
		required.push_back("path");

		Dictionary tool;
		tool["name"] = "resource_action";
		tool["description"] = "Manage Redot resource files (.tres) and asset imports";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// code_intel
	{
		Dictionary props;
		props["action"] = MCPSchemaBuilder::make_string_property("Action: 'get_symbols', 'search', 'validate', 'get_docs'");
		props["path"] = MCPSchemaBuilder::make_string_property("Path to script (.gd)");
		props["query"] = MCPSchemaBuilder::make_string_property("Class name or search query");

		Array required;
		required.push_back("action");

		Dictionary tool;
		tool["name"] = "code_intel";
		tool["description"] = "Script analysis and engine documentation lookup";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// project_config
	{
		Dictionary props;
		props["action"] = MCPSchemaBuilder::make_string_property("Action: 'get_info', 'set_setting', 'add_input', 'add_autoload', 'run', 'stop', 'output', 'list_files', 'read_file_res', 'create_file_res', 'open_editor'");
		props["setting"] = MCPSchemaBuilder::make_string_property("Setting key or Autoload/Input name");
		props["value"] = MCPSchemaBuilder::make_object_property("Value for setting");
		props["path"] = MCPSchemaBuilder::make_string_property("File/Directory path (res://)");
		props["content"] = MCPSchemaBuilder::make_string_property("Content for 'create_file_res'");

		Array required;
		required.push_back("action");

		Dictionary tool;
		tool["name"] = "project_config";
		tool["description"] = "Global project settings and Redot-specific I/O. Note: For editing existing GDScript files, use native text editing tools for precision.";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	// game_control
	{
		Dictionary props;
		props["action"] = MCPSchemaBuilder::make_string_property("Action: 'capture', 'click', 'type', 'trigger_action', 'inspect_live', 'wait'");
		props["scale"] = MCPSchemaBuilder::make_object_property("Scale for screenshot (0.1 to 1.0)");
		props["node_path"] = MCPSchemaBuilder::make_string_property("Node path for click/inspect");
		props["text"] = MCPSchemaBuilder::make_string_property("Text for 'type'");
		props["action_name"] = MCPSchemaBuilder::make_string_property("Action name for 'trigger_action' (e.g. ui_cancel)");
		props["x"] = MCPSchemaBuilder::make_object_property("X coord for click");
		props["y"] = MCPSchemaBuilder::make_object_property("Y coord for click");
		props["seconds"] = MCPSchemaBuilder::make_object_property("Wait duration");
		props["recursive"] = MCPSchemaBuilder::make_boolean_property("Recursive tree dump (for inspect_live)");
		props["depth"] = MCPSchemaBuilder::make_object_property("Max depth for recursive dump");

		Array required;
		required.push_back("action");

		Dictionary tool;
		tool["name"] = "game_control";
		tool["description"] = "Interact with the running game process (screenshots, input, live tree)";
		tool["inputSchema"] = MCPSchemaBuilder::make_object_schema(props, required);
		tools.push_back(tool);
	}

	return tools;
}

// ============================================================================
// Tool Execution
// ============================================================================

MCPTools::ToolResult MCPTools::execute_tool(const String &p_name, const Dictionary &p_arguments) {
	if (p_name == "scene_action") {
		return tool_scene_action(p_arguments);
	}
	if (p_name == "resource_action") {
		return tool_resource_action(p_arguments);
	}
	if (p_name == "code_intel") {
		return tool_code_intel(p_arguments);
	}
	if (p_name == "project_config") {
		return tool_project_config(p_arguments);
	}
	if (p_name == "game_control") {
		return tool_game_control(p_arguments);
	}

	ToolResult result;
	result.set_error("Unknown tool: " + p_name);
	return result;
}

// ============================================================================
// Implementation
// ============================================================================

MCPTools::ToolResult MCPTools::tool_scene_action(const Dictionary &p_args) {
	ToolResult result;
	String action = p_args.get("action", "");
	String scene_path = p_args.get("scene_path", "");

	if (action.is_empty() || scene_path.is_empty()) {
		result.set_error("Missing action or scene_path");
		return result;
	}

	if (!validate_path(scene_path)) {
		result.set_error("Invalid scene_path");
		return result;
	}

	String normalized_scene = normalize_path(scene_path);

	if (action == "create") {
		String root_type = p_args.get("node_type", "Node2D");
		String root_name = p_args.get("node_name", "");
		if (!ClassDB::class_exists(root_type)) {
			result.set_error("Unknown node type: " + root_type);
			return result;
		}
		Object *obj = ClassDB::instantiate(root_type);
		Node *root_node = Object::cast_to<Node>(obj);
		if (!root_node) {
			if (obj) {
				memdelete(obj);
			}
			result.set_error("Failed to create root node");
			return result;
		}
		root_node->set_name(root_name.is_empty() ? scene_path.get_file().get_basename() : root_name);
		Ref<PackedScene> new_scene;
		new_scene.instantiate();
		new_scene->pack(root_node);
		Error err = ResourceSaver::save(new_scene, normalized_scene);
		memdelete(root_node);
		if (err != OK) {
			result.set_error("Failed to save scene: " + itos(err));
		} else {
			result.add_text("Scene created: " + normalized_scene);
		}
		return result;
	}

	Ref<PackedScene> scene = ResourceLoader::load(normalized_scene, "PackedScene");
	if (scene.is_null()) {
		result.set_error("Failed to load scene: " + normalized_scene);
		return result;
	}

	Node *root = scene->instantiate();
	if (!root) {
		result.set_error("Failed to instantiate scene");
		return result;
	}

	bool should_save = false;

	if (action == "get_node") {
		String node_path = p_args.get("node_path", ".");
		Node *target = (node_path == "." || node_path.is_empty()) ? root : root->get_node_or_null(node_path);
		if (!target) {
			result.set_error("Node not found");
		} else {
			Dictionary info;
			info["name"] = target->get_name();
			info["type"] = target->get_class();
			info["path"] = node_path;
			Ref<Resource> script_res = target->get_script();
			if (script_res.is_valid()) {
				info["script"] = script_res->get_path();
			}
			Array children;
			for (int i = 0; i < target->get_child_count(); i++) {
				children.push_back(target->get_child(i)->get_name());
			}
			info["children"] = children;
			Dictionary props;
			List<PropertyInfo> plist;
			target->get_property_list(&plist);
			for (const PropertyInfo &p : plist) {
				if (p.usage & PROPERTY_USAGE_EDITOR) {
					props[p.name] = target->get(p.name);
				}
			}
			info["properties"] = props;
			result.add_text(JSON::stringify(info, "  "));
		}
	} else if (action == "add") {
		String parent_path = p_args.get("node_path", ".");
		String node_type = p_args.get("node_type", "");
		String node_name = p_args.get("node_name", "");
		Node *parent = (parent_path == "." || parent_path.is_empty()) ? root : root->get_node_or_null(parent_path);
		if (!parent) {
			result.set_error("Parent not found");
		} else {
			Object *obj = ClassDB::instantiate(node_type);
			Node *new_node = Object::cast_to<Node>(obj);
			if (!new_node) {
				if (obj) {
					memdelete(obj);
				}
				result.set_error("Invalid type");
			} else {
				if (!node_name.is_empty()) {
					new_node->set_name(node_name);
				}
				parent->add_child(new_node);
				new_node->set_owner(root);
				should_save = true;
				result.add_text("Added node '" + new_node->get_name() + "' to '" + parent_path + "'");
			}
		}
	} else if (action == "remove") {
		String node_path = p_args.get("node_path", "");
		Node *target = root->get_node_or_null(node_path);
		if (!target || target == root) {
			result.set_error("Cannot remove root");
		} else {
			target->get_parent()->remove_child(target);
			memdelete(target);
			should_save = true;
			result.add_text("Removed node: " + node_path);
		}
	} else if (action == "instance") {
		String parent_path = p_args.get("node_path", ".");
		String instance_path = p_args.get("instance_path", "");
		Node *parent = (parent_path == "." || parent_path.is_empty()) ? root : root->get_node_or_null(parent_path);
		Ref<PackedScene> sub = ResourceLoader::load(normalize_path(instance_path), "PackedScene");
		if (!parent || sub.is_null()) {
			result.set_error("Parent or instance scene not found");
		} else {
			Node *instance = sub->instantiate();
			if (p_args.has("node_name")) {
				instance->set_name(p_args["node_name"]);
			}
			parent->add_child(instance);
			instance->set_owner(root);
			should_save = true;
			result.add_text("Instanced '" + instance_path + "'");
		}
	} else if (action == "set_prop") {
		String node_path = p_args.get("node_path", ".");
		String property = p_args.get("property", "");
		Variant value = _json_to_variant(p_args.get("value", Variant()));
		Node *target = (node_path == "." || node_path.is_empty()) ? root : root->get_node_or_null(node_path);
		if (!target) {
			result.set_error("Node not found");
		} else {
			target->set(property, value);
			should_save = true;
			result.add_text("Set property '" + property + "'");
		}
	} else if (action == "connect") {
		String node_path = p_args.get("node_path", ".");
		String sig = p_args.get("signal", "");
		String target_path = p_args.get("target_node", ".");
		String method = p_args.get("method", "");
		Node *source = (node_path == "." || node_path.is_empty()) ? root : root->get_node_or_null(node_path);
		Node *target = (target_path == "." || target_path.is_empty()) ? root : root->get_node_or_null(target_path);
		if (!source || !target) {
			result.set_error("Node not found");
		} else {
			Ref<Resource> script_res = target->get_script();
			if (script_res.is_valid()) {
				_ensure_callback_exists(script_res->get_path(), method);
			}
			source->connect(sig, Callable(target, method));
			should_save = true;
			result.add_text("Connected signal");
		}
	} else if (action == "reparent") {
		String node_path = p_args.get("node_path", "");
		String target_path = p_args.get("target_node", ".");
		Node *node = root->get_node_or_null(node_path);
		Node *new_parent = (target_path == "." || target_path.is_empty()) ? root : root->get_node_or_null(target_path);
		if (!node || !new_parent || node == root) {
			result.set_error("Node not found or root");
		} else {
			node->get_parent()->remove_child(node);
			new_parent->add_child(node);
			node->set_owner(root);
			should_save = true;
			result.add_text("Reparented");
		}
	}

	if (should_save) {
		Ref<PackedScene> new_scene;
		new_scene.instantiate();
		if (new_scene.is_valid()) {
			new_scene->pack(root);
			Error err = ResourceSaver::save(new_scene, normalized_scene);
			if (err != OK) {
				result.set_error("Failed to save scene: " + itos(err));
			}
		} else {
			result.set_error("Failed to instantiate PackedScene for saving");
		}
	}
	memdelete(root);
	return result;
}

MCPTools::ToolResult MCPTools::tool_resource_action(const Dictionary &p_args) {
	ToolResult result;
	String action = p_args.get("action", "");
	String path = p_args.get("path", "");
	if (action.is_empty() || path.is_empty()) {
		result.set_error("Missing action or path");
		return result;
	}
	String normalized = normalize_path(path);

	if (action == "inspect_asset") {
		String import_path = normalized + ".import";
		if (!FileAccess::exists(import_path)) {
			result.set_error("Asset is not imported or not found: " + path);
			return result;
		}
		Error err;
		Ref<FileAccess> f = FileAccess::open(import_path, FileAccess::READ, &err);
		if (err != OK) {
			result.set_error("Failed to open .import file");
			return result;
		}
		result.add_text(f->get_as_text());
		return result;
	}

	if (action == "create") {
		String type = p_args.get("type", "Resource");
		Object *obj = ClassDB::instantiate(type);
		Ref<Resource> res = Object::cast_to<Resource>(obj);
		if (res.is_null()) {
			if (obj) {
				memdelete(obj);
			}
			result.set_error("Invalid resource type");
		} else {
			Error err = ResourceSaver::save(res, normalized);
			if (err != OK) {
				result.set_error("Failed to save resource: " + itos(err));
			} else {
				result.add_text("Created resource at " + normalized);
			}
		}
		return result;
	}

	Ref<Resource> res = ResourceLoader::load(normalized);
	if (res.is_null()) {
		result.set_error("Failed to load resource");
		return result;
	}

	if (action == "inspect") {
		Dictionary props;
		List<PropertyInfo> plist;
		res->get_property_list(&plist);
		for (const PropertyInfo &p : plist) {
			if (p.usage & PROPERTY_USAGE_EDITOR) {
				props[p.name] = res->get(p.name);
			}
		}
		result.add_text(JSON::stringify(props, "  "));
	} else if (action == "modify") {
		String prop = p_args.get("property", "");
		Variant val = _json_to_variant(p_args.get("value", Variant()));
		res->set(prop, val);
		Error err = ResourceSaver::save(res, normalized);
		if (err != OK) {
			result.set_error("Failed to save modified resource: " + itos(err));
		} else {
			result.add_text("Modified resource");
		}
	} else if (action == "duplicate") {
		String np = p_args.get("new_path", "");
		if (np.is_empty()) {
			result.set_error("Missing new_path");
		} else {
			Ref<Resource> copy = res->duplicate();
			Error err = ResourceSaver::save(copy, normalize_path(np));
			if (err != OK) {
				result.set_error("Failed to save duplicated resource: " + itos(err));
			} else {
				result.add_text("Duplicated resource");
			}
		}
	}
	return result;
}

MCPTools::ToolResult MCPTools::tool_code_intel(const Dictionary &p_args) {
	ToolResult result;
	String action = p_args.get("action", "");
	if (action == "get_docs") {
		String query = p_args.get("query", "");
		if (ClassDB::class_exists(query)) {
			Dictionary info;
			info["class"] = query;
			info["inherits"] = ClassDB::get_parent_class(query);
			Array props;
			List<PropertyInfo> plist;
			ClassDB::get_property_list(query, &plist);
			for (const PropertyInfo &p : plist) {
				props.push_back(p.name + " (" + Variant::get_type_name(p.type) + ")");
			}
			info["properties"] = props;
			Array signals;
			List<MethodInfo> slist;
			ClassDB::get_signal_list(query, &slist);
			for (const MethodInfo &s : slist) {
				signals.push_back(s.name);
			}
			info["signals"] = signals;
			Array methods;
			List<MethodInfo> mlist;
			ClassDB::get_method_list(query, &mlist);
			for (const MethodInfo &m : mlist) {
				if (m.name.begins_with("_")) {
					continue;
				}
				String sig = m.name + "(";
				for (int i = 0; i < m.arguments.size(); i++) {
					if (i > 0) {
						sig += ", ";
					}
					sig += m.arguments[i].name;
				}
				sig += ")";
				methods.push_back(sig);
			}
			info["methods"] = methods;
			result.add_text(JSON::stringify(info, "  "));
		} else {
			result.set_error("Class not found");
		}
		return result;
	}
	String path = p_args.get("path", "");
	if (path.is_empty()) {
		result.set_error("Missing path");
		return result;
	}
	String normalized = normalize_path(path);
#ifdef MODULE_GDSCRIPT_ENABLED
	if (action == "validate" || action == "get_symbols") {
		Error err;
		Ref<FileAccess> f = FileAccess::open(normalized, FileAccess::READ, &err);
		if (err != OK) {
			result.set_error("Failed to open script");
			return result;
		}
		String source = f->get_as_text();
		GDScriptParser parser;
		Error parse_err = parser.parse(source, normalized, false);
		if (parse_err != OK) {
			String el = "Validation failed:\n";
			for (const GDScriptParser::ParserError &e : parser.get_errors()) {
				el += "Line " + itos(e.line) + ": " + e.message + "\n";
			}
			result.set_error(el);
		} else {
			if (action == "validate") {
				result.add_text("Valid");
			} else {
				Dictionary symbols;
				const GDScriptParser::ClassNode *head = parser.get_tree();
				if (head) {
					Array functions, variables, signals;
					for (int i = 0; i < head->members.size(); i++) {
						const GDScriptParser::ClassNode::Member &m = head->members[i];
						if (m.type == GDScriptParser::ClassNode::Member::FUNCTION) {
							functions.push_back(m.get_name());
						} else if (m.type == GDScriptParser::ClassNode::Member::VARIABLE) {
							variables.push_back(m.get_name());
						} else if (m.type == GDScriptParser::ClassNode::Member::SIGNAL) {
							signals.push_back(m.get_name());
						}
					}
					symbols["functions"] = functions;
					symbols["variables"] = variables;
					symbols["signals"] = signals;
				}
				result.add_text(JSON::stringify(symbols, "  "));
			}
		}
		return result;
	}
#else
	result.set_error("GDScript module disabled");
#endif
	return result;
}

MCPTools::ToolResult MCPTools::tool_project_config(const Dictionary &p_args) {
	ToolResult result;
	String action = p_args.get("action", "");
	if (action == "get_info") {
		Dictionary info;
		info["name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "Unnamed");
		info["main_scene"] = ProjectSettings::get_singleton()->get_setting("application/run/main_scene", "");
		Dictionary v;
		v["major"] = VERSION_MAJOR;
		v["minor"] = VERSION_MINOR;
		info["version"] = v;
		result.add_text(JSON::stringify(info, "  "));
	} else if (action == "run") {
		if (MCPServer::get_singleton()->is_game_running()) {
			MCPServer::get_singleton()->stop_game_process();
		}
		if (MCPBridge::get_singleton()->get_port() == 0) {
			Error err = MCPBridge::get_singleton()->start_server();
			if (err != OK) {
				result.set_error("Failed to start bridge server: " + itos(err));
				return result;
			}
		}
		int b_port = MCPBridge::get_singleton()->get_port();
		if (b_port == 0) {
			result.set_error("Bridge port is 0 after start");
			return result;
		}
		String lf = normalize_path("res://.redot/mcp_game.log");
		if (!validate_path(lf)) {
			result.set_error("Invalid log path: " + lf);
			return result;
		}
		List<String> args;
		args.push_back("--path");
		args.push_back(ProjectSettings::get_singleton()->get_resource_path());
		args.push_back("--log-file");
		args.push_back(ProjectSettings::get_singleton()->globalize_path(lf));
		args.push_back("--no-header");
		args.push_back("--mcp-bridge-port");
		args.push_back(itos(b_port));
		Error err = MCPServer::get_singleton()->start_game_process(args, ProjectSettings::get_singleton()->globalize_path(lf));
		if (err != OK) {
			result.set_error("Failed to run project: " + itos(err));
		} else {
			result.add_text("Started with bridge port " + itos(b_port));
		}
	} else if (action == "stop") {
		if (MCPServer::get_singleton()->is_game_running()) {
			Error err = MCPServer::get_singleton()->stop_game_process();
			if (err == OK) {
				result.add_text("Stopped");
			} else {
				result.set_error("Failed to stop process: " + itos(err));
			}
		} else {
			result.set_error("Not running");
		}
	} else if (action == "output") {
		String glp = MCPServer::get_singleton()->get_game_log_path();
		if (glp.is_empty()) {
			result.set_error("No log path available");
		} else {
			Ref<FileAccess> f = FileAccess::open(glp, FileAccess::READ);
			if (f.is_valid()) {
				result.add_text(f->get_as_text());
			} else {
				result.set_error("Log file not found: " + glp);
			}
		}
	} else if (action == "read_file_res") {
		String p = p_args.get("path", "");
		if (!validate_path(p)) {
			result.set_error("Invalid path: " + p);
			return result;
		}
		Ref<FileAccess> f = FileAccess::open(normalize_path(p), FileAccess::READ);
		if (f.is_valid()) {
			result.add_text(f->get_as_text());
		} else {
			result.set_error("Failed to read: " + p);
		}
	} else if (action == "create_file_res") {
		String p = p_args.get("path", "");
		if (!validate_path(p)) {
			result.set_error("Invalid path: " + p);
			return result;
		}
		String norm_p = normalize_path(p);
		if (norm_p.ends_with(".gd") && FileAccess::exists(norm_p)) {
			result.set_error("Error: File '" + norm_p + "' already exists. To modify GDScripts, you must use your native 'edit' tool instead of this MCP tool.");
			return result;
		}
		String dir_path = norm_p.get_base_dir();
		Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
		if (!d->dir_exists(dir_path)) {
			d->make_dir_recursive(dir_path);
		}
		Ref<FileAccess> f = FileAccess::open(norm_p, FileAccess::WRITE);
		if (f.is_valid()) {
			f->store_string(p_args.get("content", ""));
			result.add_text("Wrote to " + norm_p);
		} else {
			result.set_error("Failed to write: " + norm_p);
		}
	} else if (action == "list_files") {
		String p = p_args.get("path", "res://");
		if (!validate_path(p)) {
			result.set_error("Invalid path: " + p);
			return result;
		}
		Ref<DirAccess> d = DirAccess::open(normalize_path(p));
		if (d.is_valid()) {
			Array e;
			d->list_dir_begin();
			String n = d->get_next();
			while (!n.is_empty()) {
				if (n != "." && n != "..") {
					Dictionary di;
					di["name"] = n;
					di["is_dir"] = d->current_is_dir();
					e.push_back(di);
				}
				n = d->get_next();
			}
			result.add_text(JSON::stringify(e));
		} else {
			result.set_error("Failed to list: " + p);
		}
	} else if (action == "set_setting") {
		ProjectSettings::get_singleton()->set_setting(p_args.get("setting", ""), p_args.get("value", Variant()));
		ProjectSettings::get_singleton()->save();
		result.add_text("Saved setting");
	} else if (action == "open_editor") {
		List<String> args;
		args.push_back("--editor");
		args.push_back("--path");
		args.push_back(ProjectSettings::get_singleton()->get_resource_path());
		OS::get_singleton()->create_process(OS::get_singleton()->get_executable_path(), args);
		result.add_text("Opening editor");
	}
	return result;
}

MCPTools::ToolResult MCPTools::tool_game_control(const Dictionary &p_args) {
	String action = p_args.get("action", "");
	ToolResult result;
	if (action == "wait") {
		float secs = p_args.get("seconds", 1.0);
		OS::get_singleton()->delay_usec(secs * 1000000);
		result.add_text("Waited " + String::num(secs) + " seconds on server.");
		return result;
	}
	if (!MCPBridge::get_singleton()->is_client_connected()) {
		result.set_error("Game process not connected to bridge yet. Try action='wait' first.");
		return result;
	}
	Dictionary resp = MCPBridge::get_singleton()->send_command(action, p_args);
	if (resp.has("error")) {
		result.set_error(resp["error"]);
	} else if (resp.has("image_base64")) {
		Dictionary content;
		content["type"] = "image";
		content["data"] = resp["image_base64"];
		content["mimeType"] = "image/png";
		result.content.push_back(content);
	} else {
		result.add_text(JSON::stringify(resp, "  "));
	}
	return result;
}
